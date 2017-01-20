/* * Copyright (c) 2013 The Linux Foundation. All rights reserved.* */

#ifndef   _IPQ806X_BOARD_PARAM_H_
#define   _IPQ806X_BOARD_PARAM_H_

#include <asm/arch-ipq806x/iomap.h>
#include "ipq806x_cdp.h"
#include "../board/qcom/common/athrs17_phy.h"
#include <asm/arch-ipq806x/gpio.h>
#include <asm/arch-ipq806x/nss/msm_ipq806x_gmac.h>
#include <phy.h>
#include <config.h>

gpio_func_data_t gmac0_gpio[] = {
	{
		.gpio = 0,
		.func = 1,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_NO_PULL,
		.drvstr = GPIO_8MA,
		.enable = GPIO_ENABLE
	},
	{
		.gpio = 1,
		.func = 1,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_NO_PULL,
		.drvstr = GPIO_8MA,
		.enable = GPIO_DISABLE
	},
	{
		.gpio = 2,
		.func = 0,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_NO_PULL,
		.drvstr = GPIO_8MA,
		.enable = GPIO_ENABLE
	},
	{
		.gpio = 66,
		.func = 0,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_NO_PULL,
		.drvstr = GPIO_16MA,
		.enable = GPIO_ENABLE
	},
};

gpio_func_data_t gmac1_gpio[] = {
	{
		.gpio = 0,
		.func = 1,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_NO_PULL,
		.drvstr = GPIO_8MA,
		.enable = GPIO_ENABLE
	},
	{
		.gpio = 1,
		.func = 1,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_NO_PULL,
		.drvstr = GPIO_8MA,
		.enable = GPIO_DISABLE
	},
	{
		.gpio = 51,
		.func = 2,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_NO_PULL,
		.drvstr = GPIO_8MA,
		.enable = GPIO_ENABLE
	},
	{
		.gpio = 52,
		.func = 2,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_NO_PULL,
		.drvstr = GPIO_8MA,
		.enable = GPIO_ENABLE
	},
	{
		.gpio = 59,
		.func = 2,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_NO_PULL,
		.drvstr = GPIO_8MA,
		.enable = GPIO_ENABLE
	},
	{
		.gpio = 60,
		.func = 2,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_NO_PULL,
		.drvstr = GPIO_8MA,
		.enable = GPIO_ENABLE
	},
	{
		.gpio = 61,
		.func = 2,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_NO_PULL,
		.drvstr = GPIO_8MA,
		.enable = GPIO_ENABLE
	},
	{
		.gpio = 62,
		.func = 2,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_NO_PULL,
		.drvstr = GPIO_8MA,
		.enable = GPIO_ENABLE
	},
	{
		.gpio = 27,
		.func = 2,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_NO_PULL,
		.drvstr = GPIO_8MA,
		.enable = GPIO_DISABLE
	},
	{
		.gpio = 28,
		.func = 2,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_NO_PULL,
		.drvstr = GPIO_8MA,
		.enable = GPIO_DISABLE
	},
	{
		.gpio = 29,
		.func = 2,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_NO_PULL,
		.drvstr = GPIO_8MA,
		.enable = GPIO_DISABLE
	},
	{
		.gpio = 30,
		.func = 2,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_NO_PULL,
		.drvstr = GPIO_8MA,
		.enable = GPIO_DISABLE
	},
	{
		.gpio = 31,
		.func = 2,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_NO_PULL,
		.drvstr = GPIO_8MA,
		.enable = GPIO_DISABLE
	},
	{
		.gpio = 32,
		.func = 2,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_NO_PULL,
		.drvstr = GPIO_8MA,
		.enable = GPIO_DISABLE
	},
};

#define gmac_board_cfg(_b, _sec, _p, _p0, _p1, _mp, _pn, ...)	\
{									\
	.base			= NSS_GMAC##_b##_BASE,			\
	.unit			= _b,					\
	.is_macsec		= _sec,					\
	.phy			= PHY_INTERFACE_MODE_##_p,		\
	.phy_addr		= { .count = _pn, { __VA_ARGS__ } },	\
	.mac_pwr0		= _p0,					\
	.mac_pwr1		= _p1,					\
	.mac_conn_to_phy	= _mp					\
}

#define gmac_board_cfg_invalid()	{ .unit = -1, }

/* Board specific parameter Array */
board_ipq806x_params_t board_params[] = {
	/*
	 * Replicate DB149 details for RUMI until, the board no.s are
	 * properly sorted out
	 */
	{
		.machid = MACH_TYPE_IPQ806X_RUMI3,
		.ddr_size = (256 << 20),
		.uart_gsbi = GSBI_1,
		.uart_gsbi_base = UART_GSBI1_BASE,
		.uart_dm_base = UART1_DM_BASE,
		.mnd_value = { 48, 125, 63 },
		.flashdesc = NAND_NOR,
		.flash_param = {
			.mode =	NOR_SPI_MODE_0,
			.bus_number = GSBI_BUS_5,
			.chip_select = SPI_CS_0,
			.vendor = SPI_NOR_FLASH_VENDOR_SPANSION,
		},
		.dbg_uart_gpio = {
			{
				.gpio = 51,
				.func = 1,
				.dir = GPIO_OUTPUT,
				.pull = GPIO_NO_PULL,
				.drvstr = GPIO_12MA,
				.enable = GPIO_DISABLE
			},
			{
				.gpio = 52,
				.func = 1,
				.dir = GPIO_INPUT,
				.pull = GPIO_NO_PULL,
				.drvstr = GPIO_12MA,
				.enable = GPIO_DISABLE
			},
		},
		.clk_dummy = 1,
	},
	{
		.machid = MACH_TYPE_IPQ806X_DB149,
		.ddr_size = (256 << 20),
		.uart_gsbi = GSBI_2,
		.uart_gsbi_base = UART_GSBI2_BASE,
		.uart_dm_base = UART2_DM_BASE,
		.mnd_value = { 12, 625, 313 },
		.gmac_gpio_count = ARRAY_SIZE(gmac0_gpio),
		.gmac_gpio = gmac0_gpio,
		.gmac_cfg = {
			gmac_board_cfg(0, 0, RGMII, 0, 0, 0,
					1, 4),
			gmac_board_cfg(1, 1, SGMII, 0, 0, 0,
					4, 0, 1, 2, 3),
			gmac_board_cfg(2, 1, SGMII, 0, 0, 1,
					1, 6),
			gmac_board_cfg(3, 1, SGMII, 0, 0, 1,
					1, 7),
		},
		.flashdesc = NAND_NOR,
		.flash_param = {
			.mode =	NOR_SPI_MODE_0,
			.bus_number = GSBI_BUS_5,
			.chip_select = SPI_CS_0,
			.vendor = SPI_NOR_FLASH_VENDOR_SPANSION,
		},
		.dbg_uart_gpio = {
			{
				.gpio = 22,
				.func = 1,
				.dir = GPIO_OUTPUT,
				.pull = GPIO_NO_PULL,
				.drvstr = GPIO_12MA,
				.enable = GPIO_DISABLE
			},
			{
				.gpio = 23,
				.func = 1,
				.dir = GPIO_INPUT,
				.pull = GPIO_NO_PULL,
				.drvstr = GPIO_12MA,
				.enable = GPIO_DISABLE
			},
		}
	},
	{
		.machid = MACH_TYPE_IPQ806X_DB149_1XX,
		.ddr_size = (256 << 20),
		.uart_gsbi = GSBI_2,
		.uart_gsbi_base = UART_GSBI2_BASE,
		.uart_dm_base = UART2_DM_BASE,
		.mnd_value = { 12, 625, 313 },
		.gmac_gpio_count = ARRAY_SIZE(gmac0_gpio),
		.gmac_gpio = gmac0_gpio,
		.gmac_cfg = {
			gmac_board_cfg(0, 0, RGMII, 0, 0, 0,
					1, 4),
			gmac_board_cfg(1, 1, SGMII, 0, 0, 0,
					4, 0, 1, 2, 3),
			gmac_board_cfg(2, 1, SGMII, 0, 0, 1,
					1, 6),
			gmac_board_cfg(3, 1, SGMII, 0, 0, 1,
					1, 7),
		},
		.flashdesc = NOR_MMC,
		.flash_param = {
			.mode =	NOR_SPI_MODE_0,
			.bus_number = GSBI_BUS_5,
			.chip_select = SPI_CS_0,
			.vendor = SPI_NOR_FLASH_VENDOR_SPANSION,
		},
		.dbg_uart_gpio = {
			{
				.gpio = 22,
				.func = 1,
				.dir = GPIO_OUTPUT,
				.pull = GPIO_NO_PULL,
				.drvstr = GPIO_12MA,
				.enable = GPIO_DISABLE
			},
			{
				.gpio = 23,
				.func = 1,
				.dir = GPIO_INPUT,
				.pull = GPIO_NO_PULL,
				.drvstr = GPIO_12MA,
				.enable = GPIO_DISABLE
			},
		}
	},
	{
		.machid = MACH_TYPE_IPQ806X_TB726,
		.ddr_size = (256 << 20),
		.uart_gsbi = GSBI_2,
		.uart_gsbi_base = UART_GSBI2_BASE,
		.uart_dm_base = UART2_DM_BASE,
		.mnd_value = { 12, 625, 313 },
		.gmac_gpio_count = ARRAY_SIZE(gmac1_gpio),
		.gmac_gpio = gmac1_gpio,

		/* This GMAC config table is not valid as of now.
		 * To accomodate this config, TB726 board needs
		 * hardware rework.Moreover this setting is not
		 * been validated in TB726 board */
		.gmac_cfg = {
			gmac_board_cfg(0, 0, RGMII, 0, 0, 0,
					1, 4),
			gmac_board_cfg(1, 1, SGMII, 0, 0, 0,
					4, 0, 1, 2, 3),
			gmac_board_cfg(2, 1, SGMII, 0, 0, 1,
					1, 6),
			gmac_board_cfg(3, 1, SGMII, 0, 0, 1,
					1, 7),
		},
		.flashdesc = NAND_NOR,
		.flash_param = {
			.mode =	NOR_SPI_MODE_0,
			.bus_number = GSBI_BUS_5,
			.chip_select = SPI_CS_0,
			.vendor = SPI_NOR_FLASH_VENDOR_SPANSION,
		},
		.dbg_uart_gpio = {
			{
				.gpio = 22,
				.func = 1,
				.dir = GPIO_OUTPUT,
				.pull = GPIO_NO_PULL,
				.drvstr = GPIO_12MA,
				.enable = GPIO_DISABLE
			},
			{
				.gpio = 23,
				.func = 1,
				.dir = GPIO_INPUT,
				.pull = GPIO_NO_PULL,
				.drvstr = GPIO_12MA,
				.enable = GPIO_DISABLE
			},
		}

	},
	{
		.machid = MACH_TYPE_IPQ806X_DB147,
		.ddr_size = (512 << 20),
		.uart_gsbi = GSBI_2,
		.uart_gsbi_base = UART_GSBI2_BASE,
		.uart_dm_base = UART2_DM_BASE,
		.mnd_value = { 12, 625, 313 },
		.gmac_gpio_count = ARRAY_SIZE(gmac1_gpio),
		.gmac_gpio = gmac1_gpio,
		.gmac_cfg = {
			gmac_board_cfg(1, 1, RGMII, S17_RGMII0_1_8V,
					S17_RGMII1_1_8V, 0, 1, 4),
			gmac_board_cfg(2, 1, SGMII, S17_RGMII0_1_8V,
					S17_RGMII1_1_8V, 0, 4, 0, 1, 2, 3),
			gmac_board_cfg_invalid(),
			gmac_board_cfg_invalid(),
		},
		.flashdesc = NAND_NOR,
		.flash_param = {
			.mode =	NOR_SPI_MODE_0,
			.bus_number = GSBI_BUS_5,
			.chip_select = SPI_CS_0,
			.vendor = SPI_NOR_FLASH_VENDOR_SPANSION,
		},
		.dbg_uart_gpio = {
			{
				.gpio = 22,
				.func = 1,
				.dir = GPIO_OUTPUT,
				.pull = GPIO_NO_PULL,
				.drvstr = GPIO_12MA,
				.enable = GPIO_DISABLE
			},
			{
				.gpio = 23,
				.func = 1,
				.dir = GPIO_INPUT,
				.pull = GPIO_NO_PULL,
				.drvstr = GPIO_12MA,
				.enable = GPIO_DISABLE
			},
		}

	},
	{
		.machid = MACH_TYPE_IPQ806X_AP148,
		.ddr_size = (256 << 20),
		.uart_gsbi = GSBI_4,
		.uart_gsbi_base = UART_GSBI4_BASE,
		.uart_dm_base = UART4_DM_BASE,
		.mnd_value = { 12, 625, 313 },
		.gmac_gpio_count = ARRAY_SIZE(gmac1_gpio),
		.gmac_gpio = gmac1_gpio,
		.gmac_cfg = {
			gmac_board_cfg(1, 1, RGMII, S17_RGMII0_1_8V,
					S17_RGMII1_1_8V, 0, 1, 4),
			gmac_board_cfg(2, 1, SGMII, S17_RGMII0_1_8V,
					S17_RGMII1_1_8V, 0, 4, 0, 1, 2, 3),
			gmac_board_cfg_invalid(),
			gmac_board_cfg_invalid(),
		},
		.flashdesc = NAND_NOR,
		.flash_param = {
			.mode =	NOR_SPI_MODE_0,
			.bus_number = GSBI_BUS_5,
			.chip_select = SPI_CS_0,
			.vendor = SPI_NOR_FLASH_VENDOR_SPANSION,
		},
		.dbg_uart_gpio = {
			{
				.gpio = 10,
				.func = 1,
				.dir = GPIO_OUTPUT,
				.pull = GPIO_NO_PULL,
				.drvstr = GPIO_12MA,
				.enable = GPIO_DISABLE
			},
			{
				.gpio = 11,
				.func = 1,
				.dir = GPIO_INPUT,
				.pull = GPIO_NO_PULL,
				.drvstr = GPIO_12MA,
				.enable = GPIO_DISABLE
			},
		}

	},
	{
		.machid = MACH_TYPE_IPQ806X_AP145,
		.ddr_size = (256 << 20),
		.uart_gsbi = GSBI_4,
		.uart_gsbi_base = UART_GSBI4_BASE,
		.uart_dm_base = UART4_DM_BASE,
		.mnd_value = { 12, 625, 313 },
		.gmac_gpio_count = ARRAY_SIZE(gmac1_gpio),
		.gmac_gpio = gmac1_gpio,
		.gmac_cfg = {
			gmac_board_cfg(1, 1, RGMII, S17_RGMII0_1_8V,
					S17_RGMII1_1_8V, 0, 1, 4),
			gmac_board_cfg(2, 1, SGMII, S17_RGMII0_1_8V,
					S17_RGMII1_1_8V, 0, 4, 0, 1, 2, 3),
			gmac_board_cfg_invalid(),
			gmac_board_cfg_invalid(),
		},
		.flashdesc = NAND_NOR,
		.flash_param = {
			.mode =	NOR_SPI_MODE_0,
			.bus_number = GSBI_BUS_5,
			.chip_select = SPI_CS_0,
			.vendor = SPI_NOR_FLASH_VENDOR_SPANSION,
		},
		.dbg_uart_gpio = {
			{
				.gpio = 10,
				.func = 1,
				.dir = GPIO_OUTPUT,
				.pull = GPIO_NO_PULL,
				.drvstr = GPIO_12MA,
				.enable = GPIO_DISABLE
			},
			{
				.gpio = 11,
				.func = 1,
				.dir = GPIO_INPUT,
				.pull = GPIO_NO_PULL,
				.drvstr = GPIO_12MA,
				.enable = GPIO_DISABLE
			},
		}

	},
};

#define NUM_IPQ806X_BOARDS	ARRAY_SIZE(board_params)

#define LED_CURRENT  GPIO_4MA
gpio_func_data_t white_led_gpio[] = {
	{
		.gpio = POWER_LED_GPIO,
		.func = 0,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_PULL_UP,
		.drvstr = LED_CURRENT,
		.enable = 1
	},
	{
		.gpio = WIFI_ON_OFF_LED_GPIO,
		.func = 0,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_PULL_UP,
		.drvstr = LED_CURRENT,
		.enable = 1
	},
	{
		.gpio = INTERNET_LED_GPIO,
		.func = 0,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_PULL_UP,
		.drvstr = LED_CURRENT,
		.enable = 1
	},
	{
		.gpio = WPS_LED_GPIO,
		.func = 0,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_PULL_UP,
		.drvstr = LED_CURRENT,
		.enable = 1
	},
	{
		.gpio = SATA_LED_GPIO,
		.func = 0,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_PULL_UP,
		.drvstr = LED_CURRENT,
		.enable = 1
	},
	{
		.gpio = USB1_LED_GPIO,
		.func = 0,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_PULL_UP,
		.drvstr = LED_CURRENT,
		.enable = 1
	},
	{
		.gpio = USB3_LED_GPIO,
		.func = 0,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_PULL_UP,
		.drvstr = LED_CURRENT,
		.enable = 1
	},
	{
		.gpio = WIFI_5G_LED_GPIO,
		.func = 0,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_PULL_UP,
		.drvstr = LED_CURRENT,
		.enable = 1
	},
};

gpio_func_data_t amber_led_gpio[] = {
	{
		.gpio = TEST_LED_GPIO,
		.func = 0,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_PULL_UP,
		.drvstr = LED_CURRENT,
		.enable = 1
	},
	{
		.gpio = WAN_LED_GPIO,
		.func = 0,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_PULL_UP,
		.drvstr = LED_CURRENT,
		.enable = 1
	},
};

gpio_func_data_t button_gpio[] = {
	{
		.gpio = WIFI_ON_OFF_BUTTON_GPIO,
		.func = 0,
		.dir = GPIO_INPUT,
		.pull = GPIO_PULL_UP,
		.drvstr = 0,
		.enable = 0
	},
	{
		.gpio = RESET_BUTTON_GPIO,
		.func = 0,
		.dir = GPIO_INPUT,
		.pull = GPIO_PULL_UP,
		.drvstr = 0,
		.enable = 0
	},
	{
		.gpio = WPS_BUTTON_GPIO,
		.func = 0,
		.dir = GPIO_INPUT,
		.pull = GPIO_PULL_UP,
		.drvstr = 0,
		.enable = 0
	},
};
#define NUM_WHITE_LEDS  ARRAY_SIZE(white_led_gpio)
#define NUM_AMBER_LEDS  ARRAY_SIZE(amber_led_gpio)
#define NUM_BUTTONS  ARRAY_SIZE(button_gpio)
#endif
