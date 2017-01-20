/* Copyright (c) 2013 The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/android_pmem.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <mach/clk.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <mach/msm_iomap-8x60.h>
#include <mach/audio_dma_msm8k.h>
#include <sound/dai.h>
#include "ipq-pcm.h"
#include "ipq-lpaif.h"
#include "ipq806x.h"

struct ipq_lpaif_dai_baseinfo dai_info;
EXPORT_SYMBOL_GPL(dai_info);

struct dai_drv *dai[MAX_LPAIF_CHANNELS];
static spinlock_t dai_lock;
struct clk *lpaif_pcm_bit_clk;
EXPORT_SYMBOL_GPL(lpaif_pcm_bit_clk);

int ipq_pcm_int_enable(uint8_t dma_ch)
{
	uint32_t intr_val;
	uint32_t status_val;

	if (dma_ch >= MAX_LPAIF_CHANNELS)
		return -EINVAL;

	/* clear status before enabling interrupt */
	status_val = readl(dai_info.base + LPAIF_IRQ_CLEAR(0));
	status_val = status_val | (1 << (dma_ch * 3));
	writel(status_val, dai_info.base + LPAIF_IRQ_CLEAR(0));

	intr_val = readl(dai_info.base + LPAIF_IRQ_EN(0));
	intr_val = intr_val | (1 << (dma_ch * 3));
	writel(intr_val, dai_info.base + LPAIF_IRQ_EN(0));

	return 0;
}
EXPORT_SYMBOL_GPL(ipq_pcm_int_enable);

int ipq_pcm_int_disable(uint8_t dma_ch)
{
	uint32_t intr_val;
	if (dma_ch >= MAX_LPAIF_CHANNELS)
		return -EINVAL;
	intr_val = readl(dai_info.base + LPAIF_IRQ_EN(0));
	intr_val = intr_val & ~(1 << (dma_ch * 3));
	writel(intr_val, dai_info.base + LPAIF_IRQ_EN(0));
	return 0;
}
EXPORT_SYMBOL_GPL(ipq_pcm_int_disable);

void ipq_cfg_pcm_aux_mode(uint8_t mode)
{
	uint32_t cfg;
	cfg = readl(dai_info.base + LPA_IF_PCM_0);
	if (mode)
		cfg |= LPA_IF_PCM_AUX_MODE;
	else
		cfg &= ~(LPA_IF_PCM_AUX_MODE);

	writel(cfg, dai_info.base + LPA_IF_PCM_0);
}
EXPORT_SYMBOL_GPL(ipq_cfg_pcm_aux_mode);

void ipq_cfg_pcm_sync_src(uint8_t src)
{
	uint32_t cfg;
	cfg = readl(dai_info.base + LPA_IF_PCM_0);
	if (src)
		cfg |= LPA_IF_PCM_SYNC_SRC_INT;
	else
		cfg = cfg & ~(LPA_IF_PCM_SYNC_SRC_INT);

	writel(cfg, dai_info.base + LPA_IF_PCM_0);
}
EXPORT_SYMBOL_GPL(ipq_cfg_pcm_sync_src);

void ipq_cfg_pcm_slot(uint8_t slot, uint8_t dir)
{
	uint32_t cfg;
	cfg = readl(dai_info.base + LPA_IF_PCM_0);

	if (dir)
		cfg = (cfg & ~(LPA_IF_RPCM_SLOT_MASK)) |
			(LPA_IF_PCM_RPCM_SLOT(slot));
	else
		cfg = (cfg & ~(LPA_IF_TPCM_SLOT_MASK))  |
			LPA_IF_PCM_TPCM_SLOT(slot);

	writel(cfg, dai_info.base + LPA_IF_PCM_0);
}
EXPORT_SYMBOL_GPL(ipq_cfg_pcm_slot);

void ipq_cfg_pcm_rate(uint32_t rate)
{
	uint32_t cfg;
	cfg = readl(dai_info.base + LPA_IF_PCM_0);
	/* Clear the rate field */
	cfg = cfg & ~(LPA_IF_PCM_RATE_MASK);
	switch (rate) {
		case IPQ_PCM_BITS_IN_FRAME_8:
			cfg |= LPA_IF_PCM_CTL_8_BITS;
			break;
		case IPQ_PCM_BITS_IN_FRAME_16:
			cfg |= LPA_IF_PCM_CTL_16_BITS;
			break;
		case IPQ_PCM_BITS_IN_FRAME_32:
			cfg |= LPA_IF_PCM_CTL_32_BITS;
			break;
		case IPQ_PCM_BITS_IN_FRAME_64:
			cfg |= LPA_IF_PCM_CTL_64_BITS;
			break;
		case IPQ_PCM_BITS_IN_FRAME_128:
			cfg |= LPA_IF_PCM_CTL_128_BITS;
			break;
		case IPQ_PCM_BITS_IN_FRAME_256:
			cfg |= LPA_IF_PCM_CTL_256_BITS;
			break;
		default:
			break;
	}
	writel(cfg, dai_info.base + LPA_IF_PCM_0);
}
EXPORT_SYMBOL_GPL(ipq_cfg_pcm_rate);

void ipq_cfg_pcm_width(uint8_t bit_width, uint8_t dir)
{
	uint32_t cfg;
	cfg = readl(dai_info.base + LPA_IF_PCM_0);

	switch (bit_width) {
		case IPQ_PCM_BIT_WIDTH_8:
			if (dir)
				cfg = cfg & ~(LPA_IF_PCM_TPCM_WIDTH);
			else
				cfg = cfg & ~(LPA_IF_PCM_RPCM_WIDTH);
			break;
		case IPQ_PCM_BIT_WIDTH_16:
			if (dir)
				cfg |= LPA_IF_PCM_TPCM_WIDTH;
			else
				cfg |= LPA_IF_PCM_RPCM_WIDTH;
			break;
		default:
			break;
	}
	writel(cfg, dai_info.base + LPA_IF_PCM_0);
}
EXPORT_SYMBOL_GPL(ipq_cfg_pcm_width);

void ipq_pcm_start(void)
{
	clk_reset(lpaif_pcm_bit_clk, LPAIF_PCM_DEASSERT);
}
EXPORT_SYMBOL_GPL(ipq_pcm_start);

void ipq_pcm_stop(void)
{
	clk_reset(lpaif_pcm_bit_clk, LPAIF_PCM_ASSERT);
}
EXPORT_SYMBOL_GPL(ipq_pcm_stop);

void ipq_cfg_mi2s_disable(uint32_t off)
{
	writel(0, dai_info.base + LPAIF_MI2S_CTL_OFFSET(off));
}
EXPORT_SYMBOL_GPL(ipq_cfg_mi2s_disable);

void ipq_cfg_i2s_spkr(uint8_t enable, uint32_t mode, uint32_t off)
{
	uint32_t cfg;
	cfg = readl(dai_info.base + LPAIF_MI2S_CTL_OFFSET(off));

	writel(0, dai_info.base + LPAIF_MI2S_CTL_OFFSET(off));

	if (enable)
		cfg |= LPA_IF_SPK_EN;
	else
		cfg = cfg & (~(LPA_IF_SPK_EN));

	cfg |= (mode << LPA_IF_SPK_MODE);
	cfg = cfg & ~LPA_IF_WS;

	writel(cfg, dai_info.base + LPAIF_MI2S_CTL_OFFSET(off));
}
EXPORT_SYMBOL_GPL(ipq_cfg_i2s_spkr);

int ipq_cfg_mi2s_hwparams_bit_width(uint32_t bit_width, uint32_t off)
{
	int ret = 0;
	uint32_t cfg;

	cfg = readl(dai_info.base + LPAIF_MI2S_CTL_OFFSET(off));
	cfg &= ~(LPA_IF_BIT_MASK);

	switch (bit_width) {
	case SNDRV_PCM_FORMAT_S16:
		cfg |= LPA_IF_BIT_RATE16;
		break;
	case SNDRV_PCM_FORMAT_S24:
		cfg |= LPA_IF_BIT_RATE24;
		break;
	case SNDRV_PCM_FORMAT_S32:
		cfg |= LPA_IF_BIT_RATE32;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if (!ret)
		writel(cfg, dai_info.base + LPAIF_MI2S_CTL_OFFSET(off));

	return ret;
}
EXPORT_SYMBOL_GPL(ipq_cfg_mi2s_hwparams_bit_width);

int ipq_cfg_mi2s_hwparams_channels(uint32_t channels, uint32_t off,
						uint32_t bit_width)
{
	int ret = 0;
	uint32_t cfg;

	cfg = readl(dai_info.base + LPAIF_MI2S_CTL_OFFSET(off));
	cfg &= ~(LPA_IF_SPK_MODE_MASK);

	switch (channels) {
	case IPQ_CHANNELS_STEREO:
		cfg |= LPA_IF_SPK_MODE_SD0;
		break;
	case IPQ_CHANNELS_4:
		cfg |= LPA_IF_SPK_MODE_QUAD01;
		break;
	case IPQ_CHANNELS_6:
		cfg |= LPA_IF_SPK_MODE_6CH;
		break;
	case IPQ_CHANNELS_8:
		cfg |= LPA_IF_SPK_MODE_8CH;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if (!ret)
		writel(cfg, dai_info.base + LPAIF_MI2S_CTL_OFFSET(off));

	return ret;
}
EXPORT_SYMBOL_GPL(ipq_cfg_mi2s_hwparams_channels);

int ipq_lpaif_dai_config_dma(uint32_t dma_ch)
{
	if (dma_ch >= MAX_LPAIF_CHANNELS)
		return -EINVAL;

	writel(dai[dma_ch]->buffer_phys,
			dai_info.base + LPAIF_DMA_BASE(dma_ch));
	writel(((dai[dma_ch]->buffer_len >> 2) - 1),
			dai_info.base + LPAIF_DMA_BUFF_LEN(dma_ch));
	writel(((dai[dma_ch]->period_len >> 2) - 1),
			dai_info.base + LPAIF_DMA_PER_LEN(dma_ch));
	mb();

	return 0;
}
EXPORT_SYMBOL_GPL(ipq_lpaif_dai_config_dma);

void ipq_lpaif_disable_dma(uint32_t dma_ch)
{
	unsigned long flag;

	spin_lock_irqsave(&dai_lock, flag);
	writel(0x0, dai_info.base + LPAIF_DMA_CTL(dma_ch));
	spin_unlock_irqrestore(&dai_lock, flag);
}
EXPORT_SYMBOL_GPL(ipq_lpaif_disable_dma);

static void ipq_lpaif_dai_disable_codec(uint32_t dma_ch, int codec)
{
	uint32_t intr_val;
	unsigned long flag;

	spin_lock_irqsave(&dai_lock, flag);

	intr_val = readl(dai_info.base + LPAIF_MI2S_CTL_OFFSET(codec));

	if (codec == DAI_SPKR)
		intr_val = intr_val & ~(LPA_IF_SPK_EN);
	else if (codec == DAI_MIC)
		intr_val = intr_val & ~(LPA_IF_MIC_EN);

	writel(intr_val, dai_info.base + LPAIF_MI2S_CTL_OFFSET(codec));
	writel(0x0, dai_info.base + LPAIF_DMA_CTL(dma_ch));

	spin_unlock_irqrestore(&dai_lock, flag);
}

int ipq_lpaif_dai_open(uint32_t dma_ch)
{
	if (!dai_info.base) {
		pr_debug("%s: %d: failed as no DAI device\n",
					__func__, __LINE__);
		return -ENODEV;
	}

	if (dma_ch >= MAX_LPAIF_CHANNELS) {
		pr_debug("%s over max channesl %d\n", __func__, dma_ch);
		return -ENODEV;
	}
	return 0;
}

void ipq_lpaif_dai_close(uint32_t dma_ch)
{
	if ((dma_ch >= MIN_DMA_RD_CH) && (dma_ch < MIN_DMA_WR_CH))
		ipq_lpaif_dai_disable_codec(dma_ch, DAI_SPKR);
	else
		ipq_lpaif_dai_disable_codec(dma_ch, DAI_MIC);
}

void ipq_lpaif_dai_set_master_mode(uint32_t dma_ch, int mode)
{
	if (dma_ch < MAX_LPAIF_CHANNELS)
		dai[dma_ch]->master_mode = mode;
	else
		pr_err("%s: %d:invalid dma channel\n", __func__, __LINE__);
}

static int ipq_cfg_lpaif_dma_ch(uint32_t lpaif_dma_ch, uint32_t channels,
							uint32_t bit_width)
{
	int ret = 0;
	uint32_t cfg = readl(dai_info.base + LPAIF_DMA_CTL(lpaif_dma_ch));

	if (lpaif_dma_ch >= MAX_LPAIF_CHANNELS)
		return -EINVAL;

	if ((lpaif_dma_ch == PCM0_DMA_WR_CH) ||
		(lpaif_dma_ch == PCM0_DMA_RD_CH)) {
		cfg &= LPA_IF_DMACTL_AUDIO_INTF_MASK;
		cfg |= LPA_IF_DMACTL_AUDIO_INTF_PCM;
	} else if (lpaif_dma_ch == MI2S_DMA_RD_CH) {
		cfg |= LPA_IF_DMACTL_AUDIO_INTF_MI2S;
		cfg &= ~(LPA_IF_DMACTL_WPSCNT_MASK);

		if ((__BIT_16 == bit_width) &&
			(IPQ_CHANNELS_STEREO == channels))
			cfg |= LPA_IF_DMACTL_WPSCNT_MONO;
		else if (((__BIT_16 == bit_width) &&
			(IPQ_CHANNELS_4 == channels)) ||
			(((__BIT_24 == bit_width) || (__BIT_32 == bit_width))
					&& (IPQ_CHANNELS_STEREO == channels)))
			cfg |= LPA_IF_DMACTL_WPSCNT_STEREO;
		else if ((__BIT_16 == bit_width) &&
			(IPQ_CHANNELS_6 == channels))
			cfg |= LPA_IF_DMACTL_WPSCNT_3CH;
		else if (((__BIT_16 == bit_width) &&
			(IPQ_CHANNELS_8 == channels)) ||
			(((__BIT_32 == bit_width) || (__BIT_24 == bit_width)) &&
			(IPQ_CHANNELS_4 == channels)))
			cfg |= LPA_IF_DMACTL_WPSCNT_4CH;
		else if (((__BIT_24 == bit_width) || (__BIT_32 == bit_width)) &&
			(IPQ_CHANNELS_6 == channels))
			cfg |= LPA_IF_DMACTL_WPSCNT_6CH;
		else if	(((__BIT_24 == bit_width) || (__BIT_32 == bit_width)) &&
			(IPQ_CHANNELS_8 == channels))
			cfg |= LPA_IF_DMACTL_WPSCNT_8CH;
		else
			ret = -EINVAL;


	}

	if (!ret)
		writel(cfg, dai_info.base + LPAIF_DMA_CTL(lpaif_dma_ch));

	return ret;
}

int ipq_lpaif_cfg_dma(uint32_t dma_ch, struct dai_dma_params *params,
		uint32_t bit_width, bool enable_intr)
{
	int ret;
	uint32_t cfg;

	dai[dma_ch]->buffer = params->buffer;
	dai[dma_ch]->buffer_phys = params->src_start;
	dai[dma_ch]->channels = params->channels;
	dai[dma_ch]->buffer_len = params->buffer_size;
	dai[dma_ch]->period_len = params->period_size;

	ret = ipq_lpaif_dai_config_dma(dma_ch);
	if (ret)
		return ret;

	if (enable_intr) {
		ipq_pcm_int_enable(dma_ch);
	}

	ret = ipq_cfg_lpaif_dma_ch(dma_ch, params->channels, bit_width);
	if (ret) {
		ipq_pcm_int_disable(dma_ch);
		return ret;
	}

	cfg = readl(dai_info.base + LPAIF_DMA_CTL(dma_ch));
	cfg |= (LPA_IF_DMACTL_FIFO_WM_8 |
			LPA_IF_DMACTL_BURST_EN);
	writel(cfg, dai_info.base + LPAIF_DMA_CTL(dma_ch));

	cfg = readl(dai_info.base + LPAIF_DMA_CTL(dma_ch));
	cfg |= LPA_IF_DMACTL_ENABLE;
	writel(cfg, dai_info.base + LPAIF_DMA_CTL(dma_ch));

	return 0;
}
EXPORT_SYMBOL_GPL(ipq_lpaif_cfg_dma);

int ipq_lpaif_dai_stop(uint32_t dma_ch)
{
	writel(0x0, dai_info.base + LPAIF_MI2S_CTL_OFFSET(LPA_IF_MI2S));
	writel(0x0, dai_info.base + LPAIF_IRQ_EN(0));
	writel(~0x0, dai_info.base + LPAIF_IRQ_CLEAR(0));
	writel(0x0, dai_info.base + LPAIF_DMA_CTL(dma_ch));
	return 0;
}
EXPORT_SYMBOL_GPL(ipq_lpaif_dai_stop);

int ipq_lpaif_pcm_stop(uint32_t dma_ch)
{
	writel(0x0, dai_info.base + LPAIF_IRQ_EN(0));
	writel(~0x0, dai_info.base + LPAIF_IRQ_CLEAR(0));
	writel(0x0, dai_info.base + LPAIF_DMA_CTL(dma_ch));
	return 0;
}
EXPORT_SYMBOL_GPL(ipq_lpaif_pcm_stop);

uint8_t ipq_lpaif_dma_stop(uint8_t dma_ch)
{
	uint32_t cfg;
	cfg = readl(dai_info.base + LPAIF_DMA_CTL(dma_ch));
	cfg &= ~(LPA_IF_DMACTL_ENABLE);
	writel(cfg, dai_info.base + LPAIF_DMA_CTL(dma_ch));
	return 0;
}
EXPORT_SYMBOL_GPL(ipq_lpaif_dma_stop);

uint8_t ipq_lpaif_dma_start(uint8_t dma_ch)
{
	uint32_t cfg;
	cfg = readl(dai_info.base + LPAIF_DMA_CTL(dma_ch));
	cfg |= LPA_IF_DMACTL_ENABLE;
	writel(cfg, dai_info.base + LPAIF_DMA_CTL(dma_ch));
	return 0;
}
EXPORT_SYMBOL_GPL(ipq_lpaif_dma_start);

void ipq_lpaif_register_dma_irq_handler(int dma_ch,
	irqreturn_t (*callback) (int intrsrc, void *private_data),
	void *private_data)
{
	dai[dma_ch]->callback = callback;
	dai[dma_ch]->private_data = private_data;
}
EXPORT_SYMBOL_GPL(ipq_lpaif_register_dma_irq_handler);

void ipq_lpaif_unregister_dma_irq_handler(int dma_ch)
{
	dai[dma_ch]->callback = NULL;
	dai[dma_ch]->private_data = NULL;
}
EXPORT_SYMBOL_GPL(ipq_lpaif_unregister_dma_irq_handler);

/*
 * Logic to find the dma channel from interrupt.
 * In total we have 9 channels, each channel records the transcation
 * status. Either one of ths 3 status will be recorded per transcation
 * (PER_CH,UNDER_RUN,OVER_RUN)
 */
static int dai_find_dma_channel(uint32_t intrsrc)
{
	uint32_t dma_channel = 0;

	while (dma_channel <= MAX_LPAIF_CHANNELS) {
		if (intrsrc & (0x1 << (dma_channel * 3)))
			break;

		dma_channel++;
	}

	return dma_channel;
}

/*
 * ISR for handling LPA_IF interrupts.
 */
static irqreturn_t dai_irq_handler(int irq, void *data)
{
	unsigned long flag;
	uint32_t intrsrc;
	uint32_t dma_ch;
	irqreturn_t ret = IRQ_NONE;

	spin_lock_irqsave(&dai_lock, flag);
	intrsrc = readl(dai_info.base + LPAIF_IRQ_STAT(0));
	writel(intrsrc, dai_info.base + LPAIF_IRQ_CLEAR(0));
	mb();
	while (intrsrc) {
		dma_ch = dai_find_dma_channel(intrsrc);

		if (dai[dma_ch]->callback) {

			ret = dai[dma_ch]->callback(intrsrc,
				dai[dma_ch]->private_data);
		}
		intrsrc &= ~(0x1 << (dma_ch * 3));
	}
	spin_unlock_irqrestore(&dai_lock, flag);
	return ret;
}

static void ipq_lpaif_dai_ch_free(void)
{
	int i;
	for (i = 0; i < MAX_LPAIF_CHANNELS; i++) {
		if (dai[i])
			kfree(dai[i]);
	}
}

static struct resource *lpa_irq;

static int __devinit ipq_lpaif_dai_probe(struct platform_device *pdev)
{
	uint8_t i;
	uint32_t rc;
	struct resource *lpa_res;

	lpa_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ipq-dai");
	if (!lpa_res) {
		dev_err(&pdev->dev, "%s: %d:error getting resource\n",
				__func__, __LINE__);
		return -ENODEV;
	}
	dai_info.base = ioremap(lpa_res->start,
			(lpa_res->end - lpa_res->start));
	if (!dai_info.base) {
		dev_err(&pdev->dev, "%s: %d:error remapping resource\n",
				__func__, __LINE__);
		return -ENOMEM;
	}

	lpa_irq = platform_get_resource_byname(
			pdev, IORESOURCE_IRQ, "ipq-dai-irq");
	if (!lpa_irq) {
		dev_err(&pdev->dev, "%s: %d: failed get irq res\n",
				__func__, __LINE__);
		rc = -ENODEV;
		goto error;
	}

	rc = request_irq(lpa_irq->start, dai_irq_handler,
			IRQF_TRIGGER_RISING, "ipq-lpaif-intr", NULL);

	if (rc < 0) {
		dev_err(&pdev->dev, "%s: %d:irq resource request failed\n",
				__func__, __LINE__);
		goto error;
	}

	lpaif_pcm_bit_clk = clk_get(&pdev->dev, "pcm_bit_clk");
	if (!lpaif_pcm_bit_clk) {
		dev_err(&pdev->dev, "%s: %d: cannot get PCM bit clock \n",
				__func__, __LINE__);
		goto error_irq;
	}

	/*
	 * Allocating memory for all the LPA_IF DMA channels
	 */
	for (i = 0; i < MAX_LPAIF_CHANNELS; i++) {
		dai[i] = kzalloc(sizeof(struct dai_drv), GFP_KERNEL);
		if (!dai[i]) {
			dev_err(&pdev->dev, "%s: %d:ch allocation failed\n",
					__func__, __LINE__);
			rc = -ENOMEM;
			goto error_mem;
		}
	}
	spin_lock_init(&dai_lock);
	return 0;
error_mem:
	clk_put(lpaif_pcm_bit_clk);
error_irq:
	free_irq(lpa_irq->start, NULL);
	ipq_lpaif_dai_ch_free();
error:
	iounmap(dai_info.base);
	return rc;
}

static int ipq_lpaif_dai_remove(struct platform_device *pdev)
{
	free_irq(lpa_irq->start, NULL);
	iounmap(dai_info.base);
	ipq_lpaif_dai_ch_free();
	return 0;
}

static struct platform_driver dai_driver = {
	.probe = ipq_lpaif_dai_probe,
	.remove = ipq_lpaif_dai_remove,
	.driver = {
		.name = "ipq-lpaif",
		.owner = THIS_MODULE
		},
};

static int __init dai_init(void)
{
	int ret;
	ret = platform_driver_register(&dai_driver);
	if (ret)
		pr_err("%s: %d:registration failed\n", __func__, __LINE__);

	return ret;
}

static void __exit dai_exit(void)
{
	platform_driver_unregister(&dai_driver);
}

module_init(dai_init);
module_exit(dai_exit);

MODULE_DESCRIPTION("IPQ LPA_IF Driver");
MODULE_LICENSE("GPL v2");
