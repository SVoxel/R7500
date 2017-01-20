
/* * Copyright (c) 2012 - 2013 The Linux Foundation. All rights reserved.* */

#include <common.h>
#include <linux/mtd/ipq_nand.h>
#include <asm/arch-ipq806x/gpio.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/arch-ipq806x/clock.h>
#include <asm/arch-ipq806x/ebi2.h>
#include <asm/arch-ipq806x/smem.h>
#include <asm/errno.h>
#include "ipq806x_board_param.h"
#include "ipq806x_cdp.h"
#include <asm/arch-ipq806x/nss/msm_ipq806x_gmac.h>
#include <asm/arch-ipq806x/timer.h>
#include <nand.h>
#include <phy.h>
#include "../common/athrs17_phy.h"
#include <dni_common.h>

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_SYS_NMRP
extern int NmrpState;
extern ulong NmrpAliveTimerStart;
extern ulong NmrpAliveTimerBase;
extern int NmrpAliveTimerTimeout;
#endif


/* Watchdog bite time set to default reset value */
#define RESET_WDT_BITE_TIME 0x31F3

/* Watchdog bark time value is ketp larger than the watchdog timeout
 * of 0x31F3, effectively disabling the watchdog bark interrupt
 */
#define RESET_WDT_BARK_TIME (5 * RESET_WDT_BITE_TIME)

/*
 * If SMEM is not found, we provide a value, that will prevent the
 * environment from being written to random location in the flash.
 *
 * NAND: In the case of NAND, we do this by setting ENV_RANGE to
 * zero. If ENV_RANGE < ENV_SIZE, then environment is not written.
 *
 * SPI Flash: In the case of SPI Flash, we do this by setting the
 * flash_index to -1.
 */

loff_t board_env_offset;
loff_t board_env_range;
extern int nand_env_device;

/*
 * Don't have this as a '.bss' variable. The '.bss' and '.rel.dyn'
 * sections seem to overlap.
 *
 * $ arm-none-linux-gnueabi-objdump -h u-boot
 * . . .
 *  8 .rel.dyn      00004ba8  40630b0c  40630b0c  00038b0c  2**2
 *                  CONTENTS, ALLOC, LOAD, READONLY, DATA
 *  9 .bss          0000559c  40630b0c  40630b0c  00000000  2**3
 *                  ALLOC
 * . . .
 *
 * board_early_init_f() initializes this variable, resulting in one
 * of the relocation entries present in '.rel.dyn' section getting
 * corrupted. Hence, when relocate_code()'s 'fixrel' executes, it
 * patches a wrong address, which incorrectly modifies some global
 * variable resulting in a crash.
 *
 * Moral of the story: Global variables that are written before
 * relocate_code() gets executed cannot be in '.bss'
 */
board_ipq806x_params_t *gboard_param = (board_ipq806x_params_t *)0xbadb0ad;
extern int ipq_gmac_eth_initialize(const char *ethaddr);
uchar ipq_def_enetaddr[6] = {0x00, 0x03, 0x7F, 0xBA, 0xDB, 0xAD};

/*******************************************************
Function description: Board specific initialization.
I/P : None
O/P : integer, 0 - no error.

********************************************************/
static board_ipq806x_params_t *get_board_param(unsigned int machid)
{
	unsigned int index = 0;

	for (index = 0; index < NUM_IPQ806X_BOARDS; index++) {
		if (machid == board_params[index].machid)
			return &board_params[index];
	}
	BUG_ON(index == NUM_IPQ806X_BOARDS);
	printf("cdp: Invalid machine id 0x%x\n", machid);
	for (;;);
}

int board_init()
{
	int ret;
	uint32_t start_blocks;
	uint32_t size_blocks;
	loff_t board_env_size;
	ipq_smem_flash_info_t *sfi = &ipq_smem_flash_info;

	/*
	 * after relocation gboard_param is reset to NULL
	 * initialize again
	 */
	gd->bd->bi_boot_params = IPQ_BOOT_PARAMS_ADDR;
	gd->bd->bi_arch_number = smem_get_board_machtype();
	gboard_param = get_board_param(gd->bd->bi_arch_number);

	/*
	 * Should be inited, before env_relocate() is called,
	 * since env. offset is obtained from SMEM.
	 */
	ret = smem_ptable_init();
	if (ret < 0) {
		printf("cdp: SMEM init failed\n");
		return ret;
	}

	ret = smem_get_boot_flash(&sfi->flash_type,
				  &sfi->flash_index,
				  &sfi->flash_chip_select,
				  &sfi->flash_block_size);
	if (ret < 0) {
		printf("cdp: get boot flash failed\n");
		return ret;
	}

	if (sfi->flash_type == SMEM_BOOT_NAND_FLASH) {
		nand_env_device = CONFIG_IPQ_NAND_NAND_INFO_IDX;
	} else if (sfi->flash_type == SMEM_BOOT_SPI_FLASH) {
		nand_env_device = CONFIG_IPQ_SPI_NAND_INFO_IDX;
	} else {
		printf("BUG: unsupported flash type : %d\n", sfi->flash_type);
		BUG();
	}

	ret = smem_getpart("0:APPSBLENV", &start_blocks, &size_blocks);
	if (ret < 0) {
		printf("cdp: get environment part failed\n");
		return ret;
	}

	board_env_offset = ((loff_t) sfi->flash_block_size) * start_blocks;
	board_env_size = ((loff_t) sfi->flash_block_size) * size_blocks;
	board_env_range = CONFIG_ENV_SIZE;
	BUG_ON(board_env_size < CONFIG_ENV_SIZE);

	return 0;
}

void enable_caches(void)
{
	icache_enable();
	/* When dcache is enabled it causes the tftp timeout CR is raised CR.No: 513868.
         * disabing dcache now to make tftp to work */
#if (CONFIG_IPQ_CACHE_ENABLE == 1)
	dcache_enable();
#endif
}


/*******************************************************
Function description: DRAM initialization.
I/P : None
O/P : integer, 0 - no error.

********************************************************/

int dram_init(void)
{
	struct smem_ram_ptable rtable;
	int i;
	int mx = ARRAY_SIZE(rtable.parts);

	if (smem_ram_ptable_init(&rtable) > 0) {
		gd->ram_size = 0;
		for (i = 0; i < mx; i++) {
			if (rtable.parts[i].category == RAM_PARTITION_SDRAM
			 && rtable.parts[i].type == RAM_PARTITION_SYS_MEMORY) {
				gd->ram_size += rtable.parts[i].size;
			}
		}
		gboard_param->ddr_size = gd->ram_size;
	} else {
		gd->ram_size = gboard_param->ddr_size;
	}
	return 0;
}

/*******************************************************
Function description: initi Dram Bank size
I/P : None
O/P : integer, 0 - no error.

********************************************************/


void dram_init_banksize(void)
{
	gd->bd->bi_dram[0].start = IPQ_KERNEL_START_ADDR;
	gd->bd->bi_dram[0].size = gboard_param->ddr_size - GENERATED_IPQ_RESERVE_SIZE;

}

/**********************************************************
Function description: Display board information on console.
I/P : None
O/P : integer, 0 - no error.

**********************************************************/

#ifdef CONFIG_DISPLAY_BOARDINFO
int checkboard(void)
{
	printf("U-boot 2012.07 dni1 V2.2 for DNI HW ID: 29764743 NOR flash 0MB NAND flash 128MB RAM 256MB 1st Radio 3x3 2nd Radio 4x4\n");
	return 0;
}
#endif /* CONFIG_DISPLAY_BOARDINFO */

void reset_cpu(ulong addr)
{
	printf("\nResetting with watch dog!\n");

	writel(0, APCS_WDT0_EN);
	writel(1, APCS_WDT0_RST);
	writel(RESET_WDT_BARK_TIME, APCS_WDT0_BARK_TIME);
	writel(RESET_WDT_BITE_TIME, APCS_WDT0_BITE_TIME);
	writel(1, APCS_WDT0_EN);
	writel(1, APCS_WDT0_CPU0_WDOG_EXPIRED_ENABLE);

	for(;;);
}

static void configure_nand_gpio(void)
{
	/* EBI2 CS, CLE, ALE, WE, OE */
	gpio_tlmm_config(34, 1, 0, GPIO_NO_PULL, GPIO_10MA, GPIO_DISABLE);
	gpio_tlmm_config(35, 1, 0, GPIO_NO_PULL, GPIO_10MA, GPIO_DISABLE);
	gpio_tlmm_config(36, 1, 0, GPIO_NO_PULL, GPIO_10MA, GPIO_DISABLE);
	gpio_tlmm_config(37, 1, 0, GPIO_NO_PULL, GPIO_10MA, GPIO_DISABLE);
	gpio_tlmm_config(38, 1, 0, GPIO_NO_PULL, GPIO_10MA, GPIO_DISABLE);

	/* EBI2 BUSY */
	gpio_tlmm_config(39, 1, 0, GPIO_PULL_UP, GPIO_10MA, GPIO_DISABLE);

	/* EBI2 D7 - D0 */
	gpio_tlmm_config(40, 1, 0, GPIO_KEEPER, GPIO_10MA, GPIO_DISABLE);
	gpio_tlmm_config(41, 1, 0, GPIO_KEEPER, GPIO_10MA, GPIO_DISABLE);
	gpio_tlmm_config(42, 1, 0, GPIO_KEEPER, GPIO_10MA, GPIO_DISABLE);
	gpio_tlmm_config(43, 1, 0, GPIO_KEEPER, GPIO_10MA, GPIO_DISABLE);
	gpio_tlmm_config(44, 1, 0, GPIO_KEEPER, GPIO_10MA, GPIO_DISABLE);
	gpio_tlmm_config(45, 1, 0, GPIO_KEEPER, GPIO_10MA, GPIO_DISABLE);
	gpio_tlmm_config(46, 1, 0, GPIO_KEEPER, GPIO_10MA, GPIO_DISABLE);
	gpio_tlmm_config(47, 1, 0, GPIO_KEEPER, GPIO_10MA, GPIO_DISABLE);
}

void board_nand_init(void)
{
	struct ebi2cr_regs *ebi2_regs;
#if defined(CONFIG_IPQ_SPI)
	extern int ipq_spi_init(void);
#endif

	if (gboard_param->flashdesc != NOR_MMC) {

		ebi2_regs = (struct ebi2cr_regs *) EBI2CR_BASE;

		nand_clock_config();
		configure_nand_gpio();

		/* NAND Flash is connected to CS0 */
		clrsetbits_le32(&ebi2_regs->chip_select_cfg0, CS0_CFG_MASK,
				CS0_CFG_SERIAL_FLASH_DEVICE);

		ipq_nand_init(IPQ_NAND_LAYOUT_LINUX);
	}
#if defined(CONFIG_IPQ_SPI)
	ipq_spi_init();
#endif
}

void ipq_get_part_details(void)
{
	int ret, i;
	uint32_t start;		/* block number */
	uint32_t size;		/* no. of blocks */

	ipq_smem_flash_info_t *smem = &ipq_smem_flash_info;

	struct { char *name; ipq_part_entry_t *part; } entries[] = {
		{ "0:HLOS", &smem->hlos },
#ifdef CONFIG_IPQ_LOAD_NSS_FW
		{ "0:NSS0", &smem->nss[0] },
		{ "0:NSS1", &smem->nss[1] },
#endif
		{ "rootfs", &smem->rootfs },
	};

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		ret = smem_getpart(entries[i].name, &start, &size);
		if (ret < 0) {
			ipq_part_entry_t *part = entries[i].part;
			printf("cdp: get part failed for %s\n", entries[i].name);
			part->offset = 0xBAD0FF5E;
			part->size = 0xBAD0FF5E;
		} else {
			ipq_set_part_entry(smem, entries[i].part, start, size);
		}
	}

	return;
}

/*
 * Get the kernel partition details from SMEM and populate the,
 * environment with sufficient information for the boot command to
 * load and execute the kernel.
 */
int board_late_init(void)
{
	unsigned int machid;

	ipq_get_part_details();

        /* get machine type from SMEM and set in env */
	machid = gd->bd->bi_arch_number;
	if (machid != 0) {
		setenv_addr("machid", (void *)machid);
		gd->bd->bi_arch_number = machid;
	}

	return 0;
}

/*
 * This function is called in the very beginning.
 * Retreive the machtype info from SMEM and map the board specific
 * parameters. Shared memory region at Dram address 0x40400000
 * contains the machine id/ board type data polulated by SBL.
 */
int board_early_init_f(void)
{
	gboard_param = get_board_param(smem_get_board_machtype());

	return 0;
}

/*
 * Gets the ethernet address from the ART partition table and return the value
 */
int get_eth_mac_address(uchar *enetaddr, uint no_of_macs)
{
	s32 ret;
	u32 start_blocks;
	u32 size_blocks;
	u32 length = (6 * no_of_macs);
	u32 flash_type;
	loff_t art_offset;
	uchar buf;

	if (ipq_smem_flash_info.flash_type == SMEM_BOOT_SPI_FLASH)
		flash_type = CONFIG_IPQ_SPI_NAND_INFO_IDX;
	else if (ipq_smem_flash_info.flash_type == SMEM_BOOT_NAND_FLASH)
		flash_type = CONFIG_IPQ_NAND_NAND_INFO_IDX;
	else {
		printf("Unknown flash type\n");
		return -EINVAL;
	}

	ret = smem_getpart("0:ART", &start_blocks, &size_blocks);
	if (ret < 0) {
		printf("No ART partition found\n");
		return ret;
	}

	/*
	 * ART partition 0th position (6 * 4) 24 bytes will contain the
	 * 4 MAC Address. First 0-5 bytes for GMAC0, Second 6-11 bytes
	 * for GMAC1, 12-17 bytes for GMAC2 and 18-23 bytes for GMAC3
	 */
	art_offset = ((loff_t) ipq_smem_flash_info.flash_block_size * start_blocks);

	ret = nand_read_skip_bad(&nand_info[flash_type], art_offset, &length, enetaddr);
	if (ret < 0)
		printf("ART partition read failed..\n");
	buf = enetaddr[0];
	enetaddr[0] = enetaddr[12];
	enetaddr[12] = buf;
	buf = enetaddr[1];
	enetaddr[1] = enetaddr[13];
	enetaddr[13] = buf;
	buf = enetaddr[2];
	enetaddr[2] = enetaddr[14];
	enetaddr[14] = buf;
	buf = enetaddr[3];
	enetaddr[3] = enetaddr[15];
	enetaddr[15] = buf;
	buf = enetaddr[4];
	enetaddr[4] = enetaddr[16];
	enetaddr[16] = buf;
	buf = enetaddr[5];
	enetaddr[5] = enetaddr[17];
	enetaddr[17] = buf;
	return ret;
}

void ipq_configure_gpio(gpio_func_data_t *gpio, uint count)
{
	int i;

	for (i = 0; i < count; i++) {
		gpio_tlmm_config(gpio->gpio, gpio->func, gpio->dir,
				gpio->pull, gpio->drvstr, gpio->enable);
		gpio++;
	}
}

int board_eth_init(bd_t *bis)
{
	int status;

	ipq_gmac_common_init(gboard_param->gmac_cfg);

	ipq_configure_gpio(gboard_param->gmac_gpio,
			gboard_param->gmac_gpio_count);

	status = ipq_gmac_init(gboard_param->gmac_cfg);
	return status;
}

/*
 * Routine to get GPIO INPUT pin or set GPIO OUTPUT pin.
 *
 * Modified from gmac_handle_gpio() of drivers/net/nss/mii_gpio.c
 *
 * @param is_get     1: get INPUT pin, 0: set OUTPUT pin
 * @param pin        GPIO pin to get/set
 * @param write_val  value to write in set operation
 * @return           pin value in get; 0 in set
 */
#define GPIO_IN   0
#define GPIO_OUT  1
#define GPIO_SET  0
#define GPIO_GET  1
static inline uint32_t ipq8064_handle_gpio(uint32_t is_get, uint32_t pin,
                                           uint32_t write_val)
{
	uint32_t addr = GPIO_IN_OUT_ADDR(pin);
	uint32_t val = readl(addr);

	if (is_get)
		return val & (1<<GPIO_IN);

	val &= (~(1 << GPIO_OUT));
	if (write_val)
		val |= (1 << GPIO_OUT);
	writel(val, addr);

	return 0;
}

#define ipq8064_led_on(_gpio_pin)	\
		ipq8064_handle_gpio(GPIO_SET, _gpio_pin, 1)
#define ipq8064_led_off(_gpio_pin)	\
		ipq8064_handle_gpio(GPIO_SET, _gpio_pin, 0)
#define ipq8064_is_button_pressed(_gpio_pin)	\
		!ipq8064_handle_gpio(GPIO_GET, _gpio_pin, 0)

void ipq8064_leds_on(gpio_func_data_t *led_gpio, uint count)
{
	int i;

	for (i = 0; i < count; i++) {
		ipq8064_led_on(led_gpio->gpio);
		led_gpio++;
	}
}

void ipq8064_leds_off(gpio_func_data_t *led_gpio, uint count)
{
	int i;

	for (i = 0; i < count; i++) {
		ipq8064_led_off(led_gpio->gpio);
		led_gpio++;
	}
}

#define ipq8064_all_leds_on()  do {				\
	ipq8064_leds_on(white_led_gpio, NUM_WHITE_LEDS);	\
	ipq8064_leds_on(amber_led_gpio, NUM_AMBER_LEDS);	\
} while (0)

#define ipq8064_all_leds_off()  do {				\
	ipq8064_leds_off(white_led_gpio, NUM_WHITE_LEDS);	\
	ipq8064_leds_off(amber_led_gpio, NUM_AMBER_LEDS);	\
} while (0)

#if defined(CONFIG_MISC_INIT_R)
int misc_init_r(void)
{
	char board_model_id[BOARD_MODEL_ID_LENGTH + 1];

	ipq_configure_gpio(white_led_gpio, NUM_WHITE_LEDS);
	ipq_configure_gpio(amber_led_gpio, NUM_AMBER_LEDS);
	ipq_configure_gpio(button_gpio, NUM_BUTTONS);

	ipq8064_all_leds_off();
	ipq8064_led_on(TEST_LED_GPIO);

	/*
	 * Make sure switch is fully reset. Otherwise, Ethernet may be unable
	 * to be brought up. This pin should be high-active.
	 */
	gpio_tlmm_config(63, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA, 1);
	ipq8064_handle_gpio(GPIO_SET, 63, 1);
	mdelay(10);
	ipq8064_handle_gpio(GPIO_SET, 63, 0);

	memset(board_model_id, 0, sizeof(board_model_id));
	get_board_data(BOARD_MODEL_ID_OFFSET, BOARD_MODEL_ID_LENGTH, (u8 *)board_model_id);
	setenv("modelid", board_model_id);
	return 0;
}
#endif

/*ledstat 0:on; 1:off*/
void board_power_led(int ledstat)
{
	if (ledstat == 1) {
		ipq8064_led_off(POWER_LED_GPIO);
	} else {
		ipq8064_led_off(TEST_LED_GPIO);
		ipq8064_led_on(POWER_LED_GPIO);
	}
}

/*ledstat 0:on; 1:off*/
void board_test_led(int ledstat)
{
	if (ledstat == 1) {
		ipq8064_led_off(TEST_LED_GPIO);
	} else {
		ipq8064_led_off(POWER_LED_GPIO);
		ipq8064_led_on(TEST_LED_GPIO);
	}
}

void board_reset_default_LedSet(void)
{
	static int DiagnosLedCount = 1;
	if ((DiagnosLedCount++ % 2) == 1) {
		/*power on test led 0.25s */
		board_test_led(0);
		NetSetTimeout((CONFIG_SYS_HZ* 1) / 4, board_reset_default_LedSet);
	} else {
		/*power off test led 0.75s */
		board_test_led(1);
		NetSetTimeout((CONFIG_SYS_HZ * 3) / 4, board_reset_default_LedSet);
	}
}

/*return value 0: not pressed, 1: pressed*/
int board_reset_button_is_press(void)
{
	return (int)ipq8064_is_button_pressed(RESET_BUTTON_GPIO);
}

/**
 * ubi_prepare:
 *
 * Prepare for "ubi" commands.
 *
 * If "ubi" is not attached to a mtd partition, issue "mtdparts default" and
 * "ubi parts NETGEAR_MTDPART_NAME" so that "ubi read" and "ubi write" can
 * work as expected.
 *
 * @return: success = 0, failed = 1
 */
static int ubi_prepare(void)
{
	if (run_command("mtdparts default", 0) == CMD_RET_FAILURE) {
		return 1;
	}
	/* FIXME: use a better method to check whether UBI device is
	 *        attached. */
	if (run_command("ubi part", 0) == CMD_RET_FAILURE &&
	    run_command("ubi part netgear", 0) == CMD_RET_FAILURE) {
		return 1;
	}
	return 0;
}

/*erase the config sector of flash*/
void board_reset_default(void)
{
    char cmd[128];
    int ret = 0;

    printf("Restore to factory default\n");
    ubi_prepare();

    /* 0 write size means erasing whole volume */
    snprintf(cmd, 128, "ubi write 0x41000000 "
                       CONFIG_SYS_FLASH_CONFIG_PARTITION_NAME " 0");
    ret = run_command(cmd, 0);
    printf("ubi erase %s\n", ret == CMD_RET_SUCCESS ? "OK" : "ERROR");

#ifdef CONFIG_SYS_NMRP
    if(NmrpState != 0)
        return;
#endif
    printf("Rebooting...\n");
    do_reset(NULL,0,0,NULL);
}

void board_upgrade_string_table(unsigned char *load_addr, int table_number, unsigned int file_size)
{
    unsigned char *string_table_addr, *addr2;
    unsigned long offset;
    unsigned int table_length;
    unsigned char high_bit, low_bit;
    unsigned long passed;
    int offset_num;
    uchar *src_addr;
    ulong target_addr;
    size_t read_size;
    char cmd[128];
    int ret = 0;

    /* Read whole string table partition from Flash to                  */
    /* RAM(load_addr + CONFIG_SYS_FLASH_SECTOR_SIZE)                    */
    string_table_addr = load_addr + CONFIG_SYS_FLASH_SECTOR_SIZE;
    memset(string_table_addr, 0, CONFIG_SYS_STRING_TABLE_TOTAL_LEN);
    read_size = CONFIG_SYS_STRING_TABLE_TOTAL_LEN;
    ubi_prepare();
    snprintf(cmd, 128, "ubi read 0x%lx %s 0x%zx",
             string_table_addr, CONFIG_SYS_STRING_TABLE_PARTITION_NAME,
	     read_size);
    ret = run_command(cmd, 0);
    printf(" %zu bytes read: %s\n", read_size,
	       ret ? "ERROR" : "OK");

    /* Save string table checksum to (100K - 1) */
    memcpy(load_addr + CONFIG_SYS_STRING_TABLE_LEN - 1, load_addr + file_size- 1, 1);
    /* Remove checksum from string table file's tail */
    memset(load_addr + file_size - 1, 0, 1);

    table_length = file_size - 1;
    printf("string table length is %d\n", table_length);

    /* Save (string table length / 1024)to 100K-3 */
    high_bit = table_length / 1024;
    addr2 = load_addr + CONFIG_SYS_STRING_TABLE_LEN - 3;
    memcpy(addr2, &high_bit, sizeof(high_bit));

    /* Save (string table length % 1024)to 100K-2 */
    low_bit = table_length % 1024;
    addr2 = load_addr + CONFIG_SYS_STRING_TABLE_LEN - 2;
    memcpy(addr2, &low_bit, sizeof(low_bit));

    /* Copy processed string table from load_addr to RAM */
    offset = (table_number - 1) * CONFIG_SYS_STRING_TABLE_LEN;
    memcpy(string_table_addr + offset, load_addr, CONFIG_SYS_STRING_TABLE_LEN);

    /**
     * Write back string tables from RAM to Flash; replace update_date().
     *
     * FIXME: Alive timer should be checked once a block is erased and written
     */
    snprintf(cmd, 128, "ubi write 0x%lx %s 0x%zx", string_table_addr,
             CONFIG_SYS_STRING_TABLE_PARTITION_NAME,
             CONFIG_SYS_STRING_TABLE_TOTAL_LEN);
    run_command(cmd, 0);
    CheckNmrpAliveTimerExpire(1);
    return;
}

#if defined(NETGEAR_BOARD_ID_SUPPORT)
/*
 * item_name_want could be "device" to get Model Id, "version" to get Version
 * or "hd_id" to get Hardware ID.
 */
void board_get_image_info(ulong fw_image_addr, char *item_name_want, char *buf)
{
    char image_header[HEADER_LEN];
    char item_name[HEADER_LEN];
    char *item_value;
    char *parsing_string;

    memset(image_header, 0, HEADER_LEN);
    memcpy(image_header, fw_image_addr, HEADER_LEN);

    parsing_string = strtok(image_header, "\n");
    while (parsing_string != NULL) {
       memset(item_name, 0, sizeof(item_name));
       strncpy(item_name, parsing_string, (int)(strchr(parsing_string, ':') - parsing_string));

       if (strcmp(item_name, item_name_want) == 0) {
           item_value = strstr(parsing_string, ":") + 1;

           memcpy(buf, item_value, strlen(item_value));
       }

       parsing_string = strtok(NULL, "\n");
    }
}

int board_match_image_hw_id (ulong fw_image_addr)
{
    char board_hw_id[BOARD_HW_ID_LENGTH + 1];
    char image_hw_id[BOARD_HW_ID_LENGTH + 1];

    /*get hardward id from board */
    memset(board_hw_id, 0, sizeof(board_hw_id));
    get_board_data(BOARD_HW_ID_OFFSET, BOARD_HW_ID_LENGTH, (u8 *)board_hw_id);
    printf("HW ID on board: %s\n", board_hw_id);

    /*get hardward id from image */
    memset(image_hw_id, 0, sizeof(image_hw_id));
    board_get_image_info(fw_image_addr, "hd_id", image_hw_id);
    printf("HW ID on image: %s\n", image_hw_id);

    if (memcmp(board_hw_id, image_hw_id, BOARD_HW_ID_LENGTH) != 0) {
            printf("Firmware Image HW ID do not match Board HW ID\n");
            return 0;
    }
    printf("Firmware Image HW ID matched Board HW ID\n\n");
    return 1;
}

int board_match_image_model_id (ulong fw_image_addr)
{
    char board_model_id[BOARD_MODEL_ID_LENGTH + 1];
    char image_model_id[BOARD_MODEL_ID_LENGTH + 1];

    /*get hardward id from board */
    memset(board_model_id, 0, sizeof(board_model_id));
    get_board_data(BOARD_MODEL_ID_OFFSET, BOARD_MODEL_ID_LENGTH, (u8 *)board_model_id);
    printf("MODEL ID on board: %s\n", board_model_id);

    /*get hardward id from image */
    memset(image_model_id, 0, sizeof(image_model_id));
    board_get_image_info(fw_image_addr, "device", image_model_id);
    printf("MODEL ID on image: %s\n", image_model_id);

    if (memcmp(board_model_id, image_model_id, BOARD_MODEL_ID_LENGTH) != 0) {
            printf("Firmware Image MODEL ID do not match Board model ID\n");
            return 0;
    }
    printf("Firmware Image MODEL ID matched Board model ID\n\n");
    return 1;
}

void board_update_image_model_id (ulong fw_image_addr)
{
    char board_model_id[BOARD_MODEL_ID_LENGTH + 1];
    char image_model_id[BOARD_MODEL_ID_LENGTH + 1];

    /*get model id from board */
    memset(board_model_id, 0, sizeof(board_model_id));
    get_board_data(BOARD_MODEL_ID_OFFSET, BOARD_MODEL_ID_LENGTH, board_model_id);
    printf("Original board MODEL ID: %s\n", board_model_id);

    /*get model id from image */
    memset(image_model_id, 0, sizeof(image_model_id));
    board_get_image_info(fw_image_addr, "device", image_model_id);
    printf("New MODEL ID from image: %s\n", image_model_id);

    printf("Updating MODEL ID\n");
	set_board_data(BOARD_MODEL_ID_OFFSET, BOARD_MODEL_ID_LENGTH, image_model_id);

    printf("done\n\n");
}
#if defined(OPEN_SOURCE_ROUTER_SUPPORT) && defined(OPEN_SOURCE_ROUTER_ID)
int board_model_id_match_open_source_id(void)
{
	char *s;

	s = getenv("modelid");
	if (!s) goto empty_out;

	return strcmp(s, OPEN_SOURCE_ROUTER_ID) == 0;

empty_out:
	return 0;
}

int board_image_reserved_length(void)
{
	return board_model_id_match_open_source_id() * CONFIG_RESERVED_LEN;
}
#endif
#endif	/* BOARD_ID */

#define qca8337_lan_all_leds_off()  do {		\
	uint32_t reg_val;				\
							\
	reg_val = athrs17_reg_read(S17_LED_CTRL0_REG);	\
	reg_val &= ~(0x3 << 14);			\
	athrs17_reg_write(S17_LED_CTRL0_REG, reg_val);	\
							\
	reg_val = athrs17_reg_read(S17_LED_CTRL1_REG);	\
	reg_val &= ~(0x3 << 14);			\
	athrs17_reg_write(S17_LED_CTRL1_REG, reg_val);	\
							\
	reg_val = athrs17_reg_read(S17_LED_CTRL3_REG);	\
	reg_val &= ~(0x7ffff << 8);			\
	athrs17_reg_write(S17_LED_CTRL3_REG, reg_val);	\
} while (0)

#define qca8337_lan_1g_leds_on()  do {			\
	uint32_t reg_val;				\
							\
	reg_val = athrs17_reg_read(S17_LED_CTRL0_REG);	\
	reg_val &= ~(0x1 << 14);			\
	reg_val |= (0x1 << 15);				\
	athrs17_reg_write(S17_LED_CTRL0_REG, reg_val);	\
							\
	reg_val = athrs17_reg_read(S17_LED_CTRL3_REG);	\
	reg_val &= ~(0x1041 << 8);			\
	reg_val |= (0x2082 << 8);			\
	athrs17_reg_write(S17_LED_CTRL3_REG, reg_val);	\
} while (0)

#define qca8337_lan_100m_leds_on()  do {		\
	uint32_t reg_val;				\
							\
	reg_val = athrs17_reg_read(S17_LED_CTRL1_REG);	\
	reg_val &= ~(0x1 << 14);			\
	reg_val |= (0x1 << 15);				\
	athrs17_reg_write(S17_LED_CTRL1_REG, reg_val);	\
							\
	reg_val = athrs17_reg_read(S17_LED_CTRL3_REG);	\
	reg_val &= ~(0x1041 << 10);			\
	reg_val |= (0x2082 << 10);			\
	athrs17_reg_write(S17_LED_CTRL3_REG, reg_val);	\
} while (0)

int do_ledctl(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int val;

	if (argc < 2) {
		val = 0;
	} else {
		val = simple_strtol(argv[1], NULL, 16);
	}

	ipq8064_all_leds_off();
	qca8337_lan_all_leds_off();

	if(val == 1) {
		ipq8064_leds_on(white_led_gpio, NUM_WHITE_LEDS);
		qca8337_lan_1g_leds_on();
	} else if(val == 2) {
		ipq8064_leds_on(amber_led_gpio, NUM_AMBER_LEDS);
		qca8337_lan_100m_leds_on();
	}
	return 0;
}

U_BOOT_CMD(
	ledctl, 2, 0, do_ledctl,
	"Test LEDs",
	"- Test LEDs\n"
	"\nledctl 1: turn on all white LEDs\n"
	"\nledctl 2: turn on all amber LEDs\n"
	"\nledctl  : turn off all LEDs\n"
);

int do_button_test(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	puts(ipq8064_is_button_pressed(RESET_BUTTON_GPIO) ?
		"reset button: pressed \n" :
		"reset button: NOT pressed \n");

	puts(ipq8064_is_button_pressed(WPS_BUTTON_GPIO) ?
		"  WPS button: pressed \n" :
		"  WPS button: NOT pressed \n");

	puts(ipq8064_is_button_pressed(WIFI_ON_OFF_BUTTON_GPIO) ?
		" WLAN button: pressed \n" :
		" WLAN button: NOT pressed \n");

	return 0;
}

U_BOOT_CMD(
	button_test, 1, 1, do_button_test,
	"Test buttons",
	"- Test buttons\n"
	"Press and hold button to be tested before issuing this command.\n"
);
