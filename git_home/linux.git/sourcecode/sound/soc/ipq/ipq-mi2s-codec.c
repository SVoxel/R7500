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
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <sound/control.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include "ipq-mi2s-codec.h"

static void ipq_mi2s_ctrl_enable_spi(void);
static struct kobject *mi2s_ctrl_object;

static unsigned short ipq_mi2s_ctrl_enable;

static ssize_t enable_show (
		struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf
		) {

	return sprintf(buf,"%hd", ipq_mi2s_ctrl_enable);
}

static ssize_t enable_store (
		struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf,
		size_t count) {

	sscanf(buf, "%hd", &ipq_mi2s_ctrl_enable);
	ipq_mi2s_ctrl_enable_spi();
	return count;
}

static struct kobj_attribute enable_attribute =
		__ATTR(ipq_mi2s_ctrl_enable, 0644, enable_show, enable_store);

DEFINE_INTEGER_ATTR(ipq_mi2s_ctrl_enable);

static struct attribute *attributes[] = {
	&enable_attribute.attr,
	NULL
};

static struct attribute_group attr_group = {
	.attrs = attributes
};

static struct spi_device *ipq_mi2s_ctrl_spi;
static uint8_t ipq_mi2s_volume = IPQ_MI2S_CTRL_VOL_MAX;

static struct snd_soc_dai_driver mi2s_codec_dai = {
		.name = "ipq-mi2s-codec-dai",
		.playback = {
			.stream_name = "lpass-mi2s-playback",
			.channels_min = 2,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16 |
				SNDRV_PCM_FMTBIT_S24 |
				SNDRV_PCM_FMTBIT_S32,
		},
};

static int mi2s_info(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = IPQ_MI2S_CTRL_VOL_CHANNELS;
	uinfo->value.integer.min = IPQ_MI2S_CTRL_VOL_MIN;
	uinfo->value.integer.max = IPQ_MI2S_CTRL_VOL_MAX;
	return 0;
}

static int mi2s_get_volume(struct snd_kcontrol *kctrl,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = ipq_mi2s_volume;
	return 0;
}

static int mi2s_put_volume(struct snd_kcontrol *kctrl,
		      struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.integer.value[0] < IPQ_MI2S_CTRL_VOL_MIN ||
		ucontrol->value.integer.value[0] > IPQ_MI2S_CTRL_VOL_MAX)
		return -EINVAL;
	ipq_mi2s_volume = ucontrol->value.integer.value[0];
	ipq_mi2s_ctrl_write(IPQ_MI2S_CTRL_REG_VOLUME, ipq_mi2s_volume);
	return 0;
}

static const struct snd_kcontrol_new mi2s_vol_ctrl  = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "MI2S",
	.index = 0,
	.info = mi2s_info,
	.get = mi2s_get_volume,
	.put = mi2s_put_volume,
};

static const struct snd_soc_codec_driver mi2s_codec = {
	.controls = &mi2s_vol_ctrl,
	.num_controls = 1,
};

void ipq_mi2s_ctrl_reset_codec(void)
{
	ipq_mi2s_ctrl_write(IPQ_MI2S_CTRL_REG_IFC_CTRL,
		IPQ_MI2S_CTRL_REG_IFC_CTRL_I2S_JUSTIFIED_24_BIT);
	ipq_mi2s_volume = IPQ_MI2S_CTRL_VOL_MAX;
}
EXPORT_SYMBOL_GPL(ipq_mi2s_ctrl_reset_codec);

int ipq_mi2s_ctrl_write(uint8_t addr, uint8_t data)
{
	int rc;
	uint8_t tx_buff[IPQ_MI2S_CTRL_BUFF_SIZE];
	struct spi_transfer xfer;
	struct spi_message msg;

	if (ipq_mi2s_ctrl_spi == NULL) {
		return 1;
	}

	memset((void *)&xfer, 0, sizeof(xfer));

	xfer.tx_buf = tx_buff;
	tx_buff[0] = addr;
	tx_buff[1] = data;
	xfer.len = IPQ_MI2S_CTRL_BUFF_SIZE;
	xfer.bits_per_word = IPQ_MI2S_CTRL_BITS_PER_WORD;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);

	rc = spi_sync(ipq_mi2s_ctrl_spi, &msg);
	return rc;
}

static void ipq_mi2s_ctrl_sysfs_init(void)
{
	int ret;
	mi2s_ctrl_object = kobject_create_and_add("mi2sctrl", kernel_kobj);
	if (!mi2s_ctrl_object) {
		printk("%s : Error allocating mi2sctrl\n",__func__);
		return;
	}

	ret = sysfs_create_group(mi2s_ctrl_object, &attr_group);

	if (ret) {
		printk("%s : Error creating mi2sctrl\n",__func__);
		kobject_put(mi2s_ctrl_object);
		mi2s_ctrl_object = NULL;
	}
}

static void ipq_mi2s_ctrl_sysfs_deinit(void)
{
	if (mi2s_ctrl_object) {
		kobject_put(mi2s_ctrl_object);
		mi2s_ctrl_object = NULL;
	}
}

static int __devinit ipq_mi2s_ctrl_probe(struct spi_device *spi)
{
	ipq_mi2s_ctrl_spi = spi;
	return 0;
}

static int __devexit ipq_mi2s_ctrl_remove(struct spi_device *spi)
{
	ipq_mi2s_ctrl_spi = NULL;
	return 0;
}

struct spi_driver ipq_mi2s_ctrl_driver = {
	.driver = {
		.name = "ipq_pcm_spi",
		.owner = THIS_MODULE,
	},
	.probe = ipq_mi2s_ctrl_probe,
	.remove = __devexit_p(ipq_mi2s_ctrl_remove),
};

static void ipq_mi2s_ctrl_enable_spi(void)
{
	if (ipq_mi2s_ctrl_spi)
		return; /* SPI is already enabled */

	if (ipq_mi2s_ctrl_enable) {
		spi_register_driver((void *)&ipq_mi2s_ctrl_driver);
		ipq_mi2s_ctrl_reset_codec();
	}
}

static int mi2s_codec_probe(struct platform_device *pdev)
{
	ipq_mi2s_ctrl_sysfs_init();
	return snd_soc_register_codec(&pdev->dev,
		&mi2s_codec, &mi2s_codec_dai, 1);
}

static int mi2s_codec_remove(struct platform_device *pdev)
{
	if (ipq_mi2s_ctrl_spi)
		spi_unregister_driver((void *)&ipq_mi2s_ctrl_driver);
	ipq_mi2s_ctrl_sysfs_deinit();
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

struct platform_driver mi2s_codec_driver = {
	.probe	= mi2s_codec_probe,
	.remove = mi2s_codec_remove,
	.driver = {
		.name = "ipq-mi2s-codec",
		.owner = THIS_MODULE,
	},
};

