/* Copyright (c) 2013 Qualcomm Atheros, Inc.
 *
 * Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/memory_alloc.h>
#include <asm/dma.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <sound/pcm.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <mach/clk.h>
#include <linux/irq.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <asm/io.h>
#include "ipq-lpaif.h"
#include "ipq806x.h"
#include "ipq-pcm.h"
#include <mach/audio_dma_msm8k.h>

static atomic_t data_avail;
static atomic_t data_at;
extern struct ipq_lpaif_dai_baseinfo dai_info;
static struct voice_dma_buffer *rx_dma_buffer;
static struct voice_dma_buffer *tx_dma_buffer;
static spinlock_t pcm_lock;
static DECLARE_WAIT_QUEUE_HEAD(pcm_q);
static struct pcm_context context;
static struct dai_dma_params tx_dma_params;
static struct dai_dma_params rx_dma_params;
extern void pcm_test_init(void);
extern void pcm_test_exit(void);
static int dsp_flag;
static int dsp_realtime_flag;

static irqreturn_t pcm_irq_handler(int intrsrc, void *data)
{
	void *dma_addr;
	uint32_t offset;
	uint32_t dma_at;
	/*
	 * Since Rx and Tx DMA starts at the same time after
	 * PCM is taken out of reset, both would complete at
	 * the same time, so no point in waiting for 2 interrupts.
	 * If one finishes its implicit that other too finishes.
	 * This handler assumes only Tx or Rx DMA interrupt is enabled.
	 *
	 * Read DMA current buffer address and figure out of buffer A
	 * finished or B finished.
	 */

	dma_addr = (void *)readl(dai_info.base +
				LPAIF_DMA_CURR_ADDR(PCM0_DMA_WR_CH));
	offset = dma_addr - rx_dma_buffer->addr;
	dma_at = offset / VOICE_PERIOD_SIZE;

	if (dma_at > (NUM_BUFFERS - 1)) {
		/* some error, get out */
		return IRQ_HANDLED;
	}
	atomic_set(&data_at, (dma_at + 1) % NUM_BUFFERS);
	atomic_set(&data_avail, 1);
	wake_up_interruptible(&pcm_q);

	/* Set the real-time miss flag if dsp_flag isn’t cleared by dsp */
	if (dsp_flag)
		dsp_realtime_flag++;

	dsp_flag = 1;

        return IRQ_HANDLED;
}

void ipq_pcm_clear_flag(void)
{
	dsp_flag = 0;
}
EXPORT_SYMBOL(ipq_pcm_clear_flag);

int ipq_pcm_check_flag(void)
{
	int retvalue;

	retvalue = dsp_realtime_flag;
	if (retvalue) {
		dsp_realtime_flag = 0;
		return retvalue;
	}
	return 0;
}
EXPORT_SYMBOL(ipq_pcm_check_flag);

void ipq_pcm_init(void)
{
	uint32_t ret;

	uint32_t bit_width = CHANNEL_BIT_WIDTH;
	uint32_t freq = CHANNEL_SAMPLING_RATE;
	uint32_t slots = NUM_PCM_SLOTS;

	dsp_flag = 0;
	dsp_realtime_flag = 0;
	atomic_set(&data_avail, 0);
	atomic_set(&data_at, 1);
	clk_reset(lpaif_pcm_bit_clk, LPAIF_PCM_ASSERT);

	/* Put the PCM instance to Reset */
	ipq_cfg_pcm_reset(1);

	/* Sync source internal */
	ipq_cfg_pcm_sync_src(1);

	/* Select long FSYNC */
	ipq_cfg_pcm_aux_mode(1);

	/* Configuring Slot0 for Tx and Rx */
	ipq_cfg_pcm_slot(0, 0);
	ipq_cfg_pcm_slot(0, 1);

	/* Configure bit width */
	ipq_cfg_pcm_width(bit_width, 0);
	ipq_cfg_pcm_width(bit_width, 1);

	/* Configure the number of slots */
	ipq_cfg_pcm_rate(bit_width * slots);

	/*
	 * Configure the DMA , Intr is enabled only for
	 * for write channel. (host Rx)
	*/

	tx_dma_params.src_start = tx_dma_buffer->addr;
	tx_dma_params.buffer = (u8 *)tx_dma_buffer->area;
	tx_dma_params.buffer_size = VOICE_BUFF_SIZE;
	tx_dma_params.period_size = VOICE_PERIOD_SIZE;
	ret = ipq_lpaif_cfg_dma(PCM0_DMA_RD_CH, &tx_dma_params,
					CHANNEL_BIT_WIDTH, 0 /*disable */);
	if (ret) {
		goto error_cfg_rd_ch;
		return ;
	}

	rx_dma_params.src_start = rx_dma_buffer->addr;
	rx_dma_params.buffer = (u8 *)rx_dma_buffer->area;
	rx_dma_params.buffer_size = VOICE_BUFF_SIZE;
	rx_dma_params.period_size = VOICE_PERIOD_SIZE;
	ret = ipq_lpaif_cfg_dma(PCM0_DMA_WR_CH, &rx_dma_params,
					CHANNEL_BIT_WIDTH, 0 /*disable */);

	ipq_lpaif_register_dma_irq_handler(PCM0_DMA_WR_CH,
						pcm_irq_handler, NULL);
	if (ret) {
		goto error_cfg_wr_ch;
		return ;
	}

	ipq_pcm_int_enable(PCM0_DMA_WR_CH);

	/* Start PCLK */
	ret = clk_set_rate(lpaif_pcm_bit_clk, (freq * bit_width * slots));
	if (IS_ERR_VALUE(ret)) {
		pr_err("%s : clk_set_rate failed for pcm bit clock\n", __func__);
		goto error_clk;
		return;
	}

	ret = clk_prepare_enable(lpaif_pcm_bit_clk);
	if (IS_ERR_VALUE(ret)) {
		pr_err("%s : clk_prepare_enable failed for pcm bit clock\n", __func__);
		goto error_clk;
	}

	/* take PCM out of reset to start it */
	ipq_cfg_pcm_reset(0);

	context.needs_deinit = 1;
	return;

error_clk:
	ipq_lpaif_disable_dma(PCM0_DMA_WR_CH);
error_cfg_wr_ch:
error_cfg_rd_ch:
	ipq_lpaif_unregister_dma_irq_handler(PCM0_DMA_WR_CH);

}
EXPORT_SYMBOL(ipq_pcm_init);

void ipq_pcm_deinit(void)
{
	context.needs_deinit = 0;
	clk_reset(lpaif_pcm_bit_clk, LPAIF_PCM_ASSERT);
	clk_disable_unprepare(lpaif_pcm_bit_clk);
	ipq_pcm_int_disable(PCM0_DMA_WR_CH);
	ipq_lpaif_unregister_dma_irq_handler(PCM0_DMA_WR_CH);
	ipq_lpaif_unregister_dma_irq_handler(PCM0_DMA_RD_CH);
	ipq_lpaif_disable_dma(PCM0_DMA_WR_CH);
	ipq_lpaif_disable_dma(PCM0_DMA_RD_CH);
	memset(&context, 0, sizeof(struct voice_dma_buffer));
}
EXPORT_SYMBOL(ipq_pcm_deinit);

int ipq_pcm_data(char **rx_buf, char **tx_buf)
{
	unsigned long flag;
	uint32_t offset;

        wait_event_interruptible(pcm_q, atomic_read(&data_avail) != 0);
	atomic_set(&data_avail, 0);
	offset = VOICE_PERIOD_SIZE * atomic_read(&data_at);
	spin_lock_irqsave(&pcm_lock, flag);
	*rx_buf = rx_dma_buffer->area + offset;
	*tx_buf = tx_dma_buffer->area + offset;
	spin_unlock_irqrestore(&pcm_lock, flag);

	return VOICE_PERIOD_SIZE;
}
EXPORT_SYMBOL(ipq_pcm_data);

void ipq_pcm_done(void)
{
	atomic_set(&data_avail, 0);
}
EXPORT_SYMBOL(ipq_pcm_done);

static int voice_allocate_dma_buffer(struct device *dev,
		struct voice_dma_buffer *dma_buffer)
{
	dma_buffer->area =  dma_alloc_coherent(dev, VOICE_BUFF_SIZE,
			&dma_buffer->addr, GFP_KERNEL);
	dma_buffer->size = VOICE_BUFF_SIZE;

	if (!dma_buffer->area) {
		return -ENOMEM;
	}
	return 0;
}

static int voice_free_dma_buffer(struct device *dev,
		struct voice_dma_buffer *dma_buff)
{
	if (!dma_buff->area)
		return 0;

	dma_free_coherent(dev, dma_buff->size + 4,
			dma_buff->area, dma_buff->addr);
	dma_buff->area = NULL;
	return 0;
}

static __devinit int ipq_pcm_driver_probe(struct platform_device *pdev)
{
	int ret;

	if (!pdev->dev.coherent_dma_mask)
		pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);

	/* Allocate memory for rx  and tx instance */
	rx_dma_buffer = kzalloc(sizeof(struct voice_dma_buffer), GFP_KERNEL);
	if (!rx_dma_buffer) {
		dev_err(&pdev->dev, "%s: %d:Error in allocating rx_dma_mem\n",
				__func__, __LINE__);
		ret = -ENOMEM;
		goto error;
	}

	tx_dma_buffer = kzalloc(sizeof(struct voice_dma_buffer), GFP_KERNEL);
	if (!tx_dma_buffer) {
		dev_err(&pdev->dev, "%s: %d:Error in allocating tx_dma_mem\n",
				__func__, __LINE__);
		ret = -ENOMEM;
		goto error_mem;
	}

	ret = voice_allocate_dma_buffer(&pdev->dev, rx_dma_buffer);
	if (ret) {
		dev_err(&pdev->dev, "%s: %d:Error in allocating tx_ring\n",
				__func__, __LINE__);
		ret = -ENOMEM;
		goto error_mem1;
	}

	ret = voice_allocate_dma_buffer(&pdev->dev, tx_dma_buffer);
	if (ret) {
		dev_err(&pdev->dev, "%s: %d:Error in allocating tx_ring\n",
				__func__, __LINE__);
		ret = -ENOMEM;
		goto error_mem2;
	}

	spin_lock_init(&pcm_lock);
	return 0;

error_mem2:
	voice_free_dma_buffer(&pdev->dev, rx_dma_buffer);
error_mem1:
	kfree(tx_dma_buffer);
error_mem:
	kfree(rx_dma_buffer);
error:
	return ret;
}

static __devexit int ipq_pcm_driver_remove(struct platform_device *pdev)
{
	voice_free_dma_buffer(&pdev->dev, tx_dma_buffer);
	voice_free_dma_buffer(&pdev->dev, rx_dma_buffer);
	if (tx_dma_buffer)
		kfree(tx_dma_buffer);
	if (rx_dma_buffer)
		kfree(rx_dma_buffer);
	return 0;
}

static struct platform_driver ipq_pcm_driver = {
	.driver = {
		.name   = "ipq-pcm-raw",
		.owner  = THIS_MODULE,
	},
	.probe  = ipq_pcm_driver_probe,
	.remove = ipq_pcm_driver_remove,
};

static int __init ipq_lpass_d2_pcm_init(void)
{
	int ret;
	ret = platform_driver_register(&ipq_pcm_driver);
	if (ret) {
		pr_err("%s: Failed to register VoIP platform driver\n",
				__func__);
	}

	pcm_test_init();
	return ret;
}

static void __exit ipq_lpass_d2_pcm_exit(void)
{
	pcm_test_exit();
	if (context.needs_deinit)
		ipq_pcm_deinit();

	platform_driver_unregister(&ipq_pcm_driver);
}

module_init(ipq_lpass_d2_pcm_init);
module_exit(ipq_lpass_d2_pcm_exit);

MODULE_DESCRIPTION("IPQ RAW PCM VoIP Platform Driver");
MODULE_LICENSE("GPL v2");
