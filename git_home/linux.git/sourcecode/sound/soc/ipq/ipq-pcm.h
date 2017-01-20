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

#ifndef _IPQ_PCM_H
#define _IPQ_PCM_H

/* 8 bit 2 channel configuration */
#define CHANNEL_BIT_WIDTH       16
#define NUM_PCM_SLOTS           8
#define CHANNEL_SAMPLING_RATE   8000
#define CHANNEL_BYTE_WIDTH      CHANNEL_BIT_WIDTH / 8
#define SAMPLES_PER_TXN         80
#define NUM_BUFFERS             2
#define VOICE_BUFF_SIZE         (CHANNEL_BYTE_WIDTH *			\
					SAMPLES_PER_TXN * NUM_BUFFERS)
#define VOICE_PERIOD_SIZE       (VOICE_BUFF_SIZE / NUM_BUFFERS)

struct pcm_context {
	uint32_t rd_pending;
	uint8_t needs_deinit;
};

struct voice_dma_buffer {
	dma_addr_t addr;
	size_t size;
	unsigned char *area;
};

enum ipq_stream_channels {
	IPQ_CHANNEL_MONO = 1,
	IPQ_CHANNELS_STEREO = 2,
	IPQ_CHANNELS_4 = 4,
	IPQ_CHANNELS_6 = 6,
	IPQ_CHANNELS_8 = 8,
};

enum ipq_pcm_bits_in_frame {
	IPQ_PCM_BITS_IN_FRAME_8 = 8,
	IPQ_PCM_BITS_IN_FRAME_16 = 16,
	IPQ_PCM_BITS_IN_FRAME_32 = 32,
	IPQ_PCM_BITS_IN_FRAME_64 = 64,
	IPQ_PCM_BITS_IN_FRAME_128 = 128,
	IPQ_PCM_BITS_IN_FRAME_256 = 256,
};

enum ipq_pcm_bit_width {
	IPQ_PCM_BIT_WIDTH_8 = 8,
	IPQ_PCM_BIT_WIDTH_16 = 16,
};

struct ipq_pcm_stream_t {
	uint8_t pcm_prepare_start;
	uint32_t period_index;
	struct snd_pcm_substream *substream;
	uint8_t pcm_fwd_flag;
	uint32_t bit_width;
	uint32_t compr_mode;
	uint32_t blocks_per_period;
	uint32_t blocks_done_per_period;
	uint32_t spdif_frame_size;
};

struct ipq_lpa_if_clk_t {
	uint8_t is_bit_clk_enabled;
	uint8_t is_osr_clk_enabled;
};

struct ipq_dml_t {
	uint8_t	dml_dma_started;
	dma_addr_t dml_start_addr;
	dma_addr_t dml_src_addr;
	dma_addr_t dml_dst_addr;
	ssize_t dml_transfer_size;
	uint32_t lpm_period_size;
	uint32_t lpm_periods;
};

struct ipq_lpa_if_t {
	uint8_t lpa_if_dma_start;
	uint8_t dma_ch;
};

struct ipq_lpass_runtime_data_t {
	struct ipq_pcm_stream_t pcm_stream_info;
	struct ipq_dml_t dml_info;
	struct ipq_lpa_if_t lpaif_info;
	struct ipq_lpa_if_clk_t lpaif_clk;
};

enum dma_intf_wr_ch {
	MIN_DMA_WR_CH = 5,
	PCM0_DMA_WR_CH = 5,
	PCM1_DMA_WR_CH,
	MAX_DMA_WR_CH = 8,
};

enum dma_intf_rd_ch {
	MIN_DMA_RD_CH = 0,
	MI2S_DMA_RD_CH = 0,
	PCM0_DMA_RD_CH = 1,
	PCM1_DMA_RD_CH,
	MAX_DMA_RD_CH = 4,
};

extern struct clk *lpaif_pcm_bit_clk;
#endif /*_IPQ_PCM_H */
