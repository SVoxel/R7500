/* * Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.* */
/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/string.h>
#include "ipq-pcm.h"

/*
 * This is an external loopback test module for PCM interface.
 * For this to work, the Rx and Tx should be shorted in the
 * SLIC header.
 * This test module exposes a sysfs interface to start\stop the
 * tests. This module sends a sequence of numbers starting from
 * 0 to 255 and wraps around to 0 and continues the sequence.
 * The received data is then compared for the sequence. Any errors
 * found are reported immediately. When test stop, a final statistics
 * of how much passed and failed is displayed.
 */

extern void ipq_pcm_init(void);
extern void ipq_pcm_deinit(void);
extern uint32_t ipq_pcm_data(char **rx_buf, char **tx_buf);
extern void ipq_pcm_done();
static void pcm_start_test(void);

#define LOOPBACK_SKIP_COUNT		10
#define LOOPBACK_FAIL_THRESHOLD		20
#define DEFINE_INTEGER_ATTR(name)					\
static unsigned short name;						\
									\
static ssize_t name##_show (						\
		struct kobject *kobj,					\
		struct kobj_attribute *attr,				\
		char *buf						\
		) {							\
									\
	return sprintf(buf,"%hd", name);				\
}									\
									\
static ssize_t name##_store (						\
		struct kobject *kobj,   				\
		struct kobj_attribute *attr, 				\
		const char *buf,              				\
		size_t count) {						\
									\
	sscanf(buf, "%hd", &name);					\
	pcm_start_test();						\
	return count;							\
}									\
									\
static struct kobj_attribute 						\
		name##_attribute =					\
			__ATTR(name,0644,name##_show,name##_store);	\

struct pcm_lb_test_ctx {
	uint32_t failed;
	uint32_t passed;
	unsigned char *last_rx_buff;
	int running;
	unsigned char tx_data;
	unsigned char expected_rx_seq;
	int read_count;
	struct task_struct *task;
};

static struct pcm_lb_test_ctx ctx;
DEFINE_INTEGER_ATTR(start)

static struct attribute *attributes[] = {
	&start_attribute.attr,
	NULL
};

static struct attribute_group attr_group = {
	.attrs = attributes
};

static struct kobject *pcm_lb_test_object;

void pcm_lb_sysfs_init(void)
{
	int ret;
	pcm_lb_test_object = kobject_create_and_add("pcmlb", kernel_kobj);
	if (!pcm_lb_test_object) {
		printk("%s : Error allocating pcmlb\n",__func__);
		return;
	}

	ret = sysfs_create_group(pcm_lb_test_object, &attr_group);

	if (ret) {
		printk("%s : Error creating pcmlb\n",__func__);
		kobject_put(pcm_lb_test_object);
		pcm_lb_test_object = NULL;
	}
}

void pcm_lb_sysfs_deinit(void)
{
	if (pcm_lb_test_object) {
		kobject_put(pcm_lb_test_object);
		pcm_lb_test_object = NULL;
	}
}

void pcm_read_write(void)
{
	char *rx_buff;
	char *tx_buff;
	int i;

	ipq_pcm_data(&rx_buff, &tx_buff);
	ctx.last_rx_buff = rx_buff;

	/* get current Tx buffer and write the pattern
	 * We will write 1, 2, 3, ..., 255, 1, 2, 3...
	 */
	for (i =  0; i < (VOICE_PERIOD_SIZE); i++) {
		tx_buff[i] = ctx.tx_data;
		ctx.tx_data ++;
	}
	ipq_pcm_done();
}

void pcm_init(void)
{
	ipq_pcm_init();
}

void pcm_deinit(void)
{
	memset((void *)&ctx, 0, sizeof(ctx));
	start = 0;
	ipq_pcm_deinit();
}

void process_read(void)
{
	uint32_t index;
	static uint32_t continuous_failures = 0;
	char data;

	ctx.read_count ++;
	if (ctx.read_count  <= LOOPBACK_SKIP_COUNT) {
		/*
		 * As soon as do pcm init, the DMA would start. So the initial
		 * few rw till the 1st Rx is called will be 0's, so we skip
		 * 10 reads so that our loopback settles down.
		 * Note: our 1st loopback Tx is only after an RX is called.
		 */
		return;
	} else if (ctx.read_count == (LOOPBACK_SKIP_COUNT + 1)) {
		/*
		 * our loopback should have settled, so start looking for the
		 * sequence from here.
		 */
		ctx.expected_rx_seq = ctx.last_rx_buff[0];
	}

	/* get out if test stopped */
	if (start == 0)
		return;

	data = ctx.expected_rx_seq;
	for (index = 0; index < VOICE_PERIOD_SIZE; index++) {
		if (ctx.last_rx_buff[index] != (data)) {
			printk("Rx (%d) Failed at index %d: "
				"Expected : %d, Received : %d\n",
				ctx.read_count, index, data, ctx.last_rx_buff[index]);
			break;
		}
		data ++;
	}

	if (index == VOICE_PERIOD_SIZE) {
		ctx.passed++;
		continuous_failures = 0;
	} else {
		ctx.failed++;
		continuous_failures ++;
	}

	ctx.expected_rx_seq += VOICE_PERIOD_SIZE;

	/* Abort if there are more failures */
	if (continuous_failures >= LOOPBACK_FAIL_THRESHOLD) {
		printk("Aborting loopback test as there are %d continuous "
				"failures\n", continuous_failures);
		continuous_failures = 0;
		ctx.running = 0; /* stops test thread (current) */
	}
}

int pcm_test_rw(void *data)
{
	struct sched_param param;
	int ret;
	printk("%s : Test thread started\n", __func__);
	ctx.running = 1;

	/*
	 * set test thread priority as 90, this is to align with what
	 * D2 VOIP stack does.
	 */

	param.sched_priority = 90;
	ret = sched_setscheduler(ctx.task, SCHED_FIFO, &param);
	if (ret)
		printk("%s : Error setting priority, error: %d\n", __func__, ret);

	pcm_init();

	while (ctx.running) {
		pcm_read_write();
		process_read();
	}
	printk("%s : Test Thread stopped\n", __func__);
	printk("\nPassed : %d, Failed : %d\n", ctx.passed, ctx.failed);
	pcm_deinit();
	return 0;
}

static void pcm_start_test(void)
{
	printk("%s : %d\n",__func__, start);
	if (start) {
		if (ctx.running) {
			printk("%s : Test already running\n", __func__);
		} else {
			ctx.task = kthread_create(&pcm_test_rw, NULL, "PCMTest");
			if(ctx.task)
				wake_up_process(ctx.task);
		}
	} else {
		if (ctx.running) {
			printk("%s : Stopping test\n", __func__);
			ctx.running = 0;
			/* wait sufficient time for test thread to finish */
			mdelay(2000);
		} else
			printk("%s : Test already stopped\n", __func__);
	}
}

int pcm_test_init(void)
{
	pcm_lb_sysfs_init();
	return 0;
}

void pcm_test_exit(void)
{
	pcm_lb_sysfs_deinit();
	if (ctx.running) {
		ctx.running = 0;
		/* wait sufficient time for test thread to finish */
		mdelay(2000);
	}
}
