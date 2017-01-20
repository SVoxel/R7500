#include <common.h>
#include <command.h>
#include <asm/mipsregs.h>
#include <asm/addrspace.h>
#include <config.h>
#include <version.h>
#include "ar7240_soc.h"
#include <netdev.h>

#ifdef CONFIG_SYS_NMRP
extern int NmrpState;
extern ulong NmrpAliveTimerStart;
extern ulong NmrpAliveTimerBase;
extern int NmrpAliveTimerTimeout;
#endif

extern int wasp_ddr_initial_config(uint32_t refresh);
extern int ar7240_ddr_find_size(void);

extern flash_info_t flash_info[];

void
wasp_usb_initial_config(void)
{
#define unset(a)	(~(a))

	if ((ar7240_reg_rd(WASP_BOOTSTRAP_REG) & WASP_REF_CLK_25) == 0) {
		ar7240_reg_wr_nf(AR934X_SWITCH_CLOCK_SPARE,
			ar7240_reg_rd(AR934X_SWITCH_CLOCK_SPARE) |
			SWITCH_CLOCK_SPARE_USB_REFCLK_FREQ_SEL_SET(2));
	} else {
		ar7240_reg_wr_nf(AR934X_SWITCH_CLOCK_SPARE,
			ar7240_reg_rd(AR934X_SWITCH_CLOCK_SPARE) |
			SWITCH_CLOCK_SPARE_USB_REFCLK_FREQ_SEL_SET(5));
	}

	udelay(1000);
	ar7240_reg_wr(AR7240_RESET,
		ar7240_reg_rd(AR7240_RESET) |
		RST_RESET_USB_PHY_SUSPEND_OVERRIDE_SET(1));
	udelay(1000);
	ar7240_reg_wr(AR7240_RESET,
		ar7240_reg_rd(AR7240_RESET) &
		unset(RST_RESET_USB_PHY_RESET_SET(1)));
	udelay(1000);
	ar7240_reg_wr(AR7240_RESET,
		ar7240_reg_rd(AR7240_RESET) &
		unset(RST_RESET_USB_PHY_ARESET_SET(1)));
	udelay(1000);
	ar7240_reg_wr(AR7240_RESET,
		ar7240_reg_rd(AR7240_RESET) &
		unset(RST_RESET_USB_HOST_RESET_SET(1)));
	udelay(1000);
	if ((ar7240_reg_rd(AR7240_REV_ID) & 0xf) == 0) {
		/* Only for WASP 1.0 */
		ar7240_reg_wr(0xb8116c84 ,
			ar7240_reg_rd(0xb8116c84) & unset(1<<20));
	}
}

void wasp_gpio_config(void)
{
#if 0
	/* Disable clock obs */
	ar7240_reg_wr (AR7240_GPIO_FUNC, (ar7240_reg_rd(AR7240_GPIO_FUNC) & 0xffe7e0ff));
	/* Enable eth Switch LEDs */
#ifdef CONFIG_K31
	ar7240_reg_wr (AR7240_GPIO_FUNC, (ar7240_reg_rd(AR7240_GPIO_FUNC) | 0xd8));
#else
	ar7240_reg_wr (AR7240_GPIO_FUNC, (ar7240_reg_rd(AR7240_GPIO_FUNC) | 0xfa));
#endif
#endif
	ar7240_reg_wr (AR7240_GPIO_OUT_FUNCTION4,
        (ar7240_reg_rd(AR7240_GPIO_OUT_FUNCTION4) & 0xffff0000) | 0x2f2e0000);
}

int
wasp_mem_config(void)
{
	unsigned int type, reg32;

	type = wasp_ddr_initial_config(CONFIG_SYS_DDR_REFRESH_VAL);

	/* Take WMAC out of reset */
	reg32 = ar7240_reg_rd(AR7240_RESET);
	reg32 = reg32 &  ~AR7240_RESET_WMAC;
	ar7240_reg_wr_nf(AR7240_RESET, reg32);

	/* Switching regulator settings */
	ar7240_reg_wr_nf(0x18116c40, 0x633c8176); /* AR_PHY_PMU1 */
	if (type == 2) {
		// ddr1
		ar7240_reg_wr_nf(0x18116c44, 0x10000000); /* AR_PHY_PMU2 */
	} else {
		// ddr2 & sdram
		ar7240_reg_wr_nf(0x18116c44, 0x10380000); /* AR_PHY_PMU2 */
	}

	wasp_usb_initial_config();

	wasp_gpio_config();

	reg32 = ar7240_ddr_find_size();

	return reg32;
}

phys_size_t initdram(int board_type)
{
	return (wasp_mem_config());
}

int	checkboard(void)
{
	printf("WNDR3700v3 (ar7240) U-boot dni25 V0.1\n");
	return 0;
}

int board_eth_init(bd_t *bis)
{
	return pci_eth_init(bis);
}

/*ledstat 0:on; 1:off*/
void board_power_led(int ledstat)
{
}

/*ledstat 0:on; 1:off*/
void board_test_led(int ledstat)
{
}

void board_reset_default_LedSet(void)
{
}

/*return value 0: not pressed, 1: pressed*/
int board_reset_button_is_press()
{
    return 0;
}

extern int flash_sect_erase (ulong, ulong);

/*erase the config sector of flash*/
void board_reset_default()
{
}

#if defined(CONFIG_SYS_SINGLE_FIRMWARE)
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

    /* Read whole string table partition from Flash to RAM(load_addr+128k) */
    string_table_addr = load_addr + 0x20000;
    memset(string_table_addr, 0, CONFIG_SYS_STRING_TABLE_TOTAL_LEN);
    memcpy(string_table_addr, CONFIG_SYS_STRING_TABLE_ADDR_BEGIN, CONFIG_SYS_STRING_TABLE_TOTAL_LEN);

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

    /* Write back string tables from RAM to Flash */
    printf("erase %x to %x\n", CONFIG_SYS_STRING_TABLE_ADDR_BEGIN, CONFIG_SYS_STRING_TABLE_ADDR_END);
    for (offset_num = 0; offset_num < (CONFIG_SYS_STRING_TABLE_TOTAL_LEN / CONFIG_SYS_FLASH_SECTOR_SIZE); offset_num++) {
        /* erase 64K */
        flash_sect_erase (CONFIG_SYS_STRING_TABLE_ADDR_BEGIN + offset_num * CONFIG_SYS_FLASH_SECTOR_SIZE,\
                          CONFIG_SYS_STRING_TABLE_ADDR_BEGIN + ((offset_num+1) * CONFIG_SYS_FLASH_SECTOR_SIZE) - 1);

        /* Check if Alive-timer expires? */
        passed = get_timer(NmrpAliveTimerStart);
        if ((passed / CONFIG_SYS_HZ) + NmrpAliveTimerBase > NmrpAliveTimerTimeout) {
            printf("Active-timer expires\n");
            NmrpSend();
            NmrpAliveTimerBase = NmrpAliveTimerTimeout / 4;
            NmrpAliveTimerStart = get_timer(0);
        } else {
            printf("Alive-timer %u\n",(passed / CONFIG_SYS_HZ) + NmrpAliveTimerBase);
            /* If passed 1/4 NmrpAliveTimerTimeout,
             * add 1/4 NmrpAliveTimerTimeout to NmrpAliveTimerBase.
             * This is for avoiding "passed" overflow.
             */
            if ((passed) / CONFIG_SYS_HZ >= (NmrpAliveTimerTimeout / 4)) {
                NmrpAliveTimerBase += NmrpAliveTimerTimeout / 4;
                NmrpAliveTimerStart = get_timer(0);
            }
        }
    }

    printf ("Copy all string tables to Flash...\n");
    for (offset_num = 0; offset_num < (CONFIG_SYS_STRING_TABLE_TOTAL_LEN / CONFIG_SYS_FLASH_SECTOR_SIZE); offset_num++) {
        src_addr = string_table_addr + offset_num * CONFIG_SYS_FLASH_SECTOR_SIZE;
        target_addr = CONFIG_SYS_STRING_TABLE_ADDR_BEGIN + offset_num * CONFIG_SYS_FLASH_SECTOR_SIZE;
        flash_write( src_addr, target_addr, CONFIG_SYS_FLASH_SECTOR_SIZE);

	/* Check if Alive-timer expires? */
	passed = get_timer(NmrpAliveTimerStart);
        if ((passed / CONFIG_SYS_HZ) + NmrpAliveTimerBase > NmrpAliveTimerTimeout) {
            printf("Active-timer expires\n");
            NmrpSend();
            NmrpAliveTimerBase = NmrpAliveTimerTimeout / 4;
            NmrpAliveTimerStart = get_timer(0);
        } else {
            printf("Alive-timer %u\n",(passed / CONFIG_SYS_HZ) + NmrpAliveTimerBase);
            /* If passed 1/4 NmrpAliveTimerTimeout,
             * add 1/4 NmrpAliveTimerTimeout to NmrpAliveTimerBase.
             * This is for avoiding "passed" overflow.
             */
            if ((passed) / CONFIG_SYS_HZ >= (NmrpAliveTimerTimeout / 4)) {
                NmrpAliveTimerBase += NmrpAliveTimerTimeout / 4;
                NmrpAliveTimerStart = get_timer(0);
            }
        }
    }
    return;
}
#endif

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
    memcpy(board_hw_id, (void *)(BOARDCAL + BOARD_HW_ID_OFFSET),
           BOARD_HW_ID_LENGTH);
    printf("HW ID on board: %s\n", board_hw_id);

    /*get hardward id from image */
    memset(image_hw_id, 0, sizeof(image_hw_id));
    board_get_image_info(fw_image_addr, "hd_id", (char*)image_hw_id);
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
    memcpy(board_model_id, (void *)(BOARDCAL + BOARD_MODEL_ID_OFFSET),
           BOARD_MODEL_ID_LENGTH);
    printf("MODEL ID on board: %s\n", board_model_id);

    /*get hardward id from image */
    memset(image_model_id, 0, sizeof(image_model_id));
    board_get_image_info(fw_image_addr, "device", (char*)image_model_id);
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
    char sectorBuff[CONFIG_SYS_FLASH_SECTOR_SIZE];

    /* Read boarddata */
    memcpy(sectorBuff, (void *)BOARDCAL, CONFIG_SYS_FLASH_SECTOR_SIZE);

    /*get hardward id from board */
    memset(board_model_id, 0, sizeof(board_model_id));
    memcpy(board_model_id, (void *)(BOARDCAL + BOARD_MODEL_ID_OFFSET),
           BOARD_MODEL_ID_LENGTH);
    printf("Original board MODEL ID: %s\n", board_model_id);

    /*get hardward id from image */
    memset(image_model_id, 0, sizeof(image_model_id));
    board_get_image_info(fw_image_addr, "device", (char*)image_model_id);
    printf("New MODEL ID from image: %s\n", image_model_id);

    printf("Updating MODEL ID\n");
    memcpy(sectorBuff + BOARD_MODEL_ID_OFFSET, image_model_id,
	    BOARD_MODEL_ID_LENGTH);
    flash_erase(flash_info, CAL_SECTOR, CAL_SECTOR);
    write_buff(flash_info, sectorBuff, BOARDCAL, CONFIG_SYS_FLASH_SECTOR_SIZE);

    printf("done\n\n");
}
#endif	/* BOARD_ID */
