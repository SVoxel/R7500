/*
 * (C) Copyright 2003
 * Wolfgang Denk, DENX Software Engineering, <wd@denx.de>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <command.h>
#include <asm/inca-ip.h>
#include <asm/mipsregs.h>

#if defined(CONFIG_AR7240)
#include <asm/addrspace.h>
#include <ar7240_soc.h>
#endif

int do_reset(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
#if defined(CONFIG_INCA_IP)
	*INCA_IP_WDT_RST_REQ = 0x3f;
#elif defined(CONFIG_PURPLE) || defined(CONFIG_TB0229)
	void (*f)(void) = (void *) 0xbfc00000;

	f();
#elif defined(CONFIG_AR7240)
	fprintf(stdout, "\nResetting...\n");
	for (;;) {
#ifdef CONFIG_WASP
		if (ar7240_reg_rd(AR7240_REV_ID) & 0xf) {
			ar7240_reg_wr(AR7240_RESET,
				(AR7240_RESET_FULL_CHIP | AR7240_RESET_DDR));
		} else {
			/*
			 * WAR for full chip reset spi vs. boot-rom selection
			 * bug in wasp 1.0
			 */
			ar7240_reg_wr (AR7240_GPIO_OE,
				ar7240_reg_rd(AR7240_GPIO_OE) & (~(1 << 17)));
		}
#else
		ar7240_reg_wr(AR7240_RESET,
			(AR7240_RESET_FULL_CHIP | AR7240_RESET_DDR));
#endif
	}
#endif
	fprintf(stderr, "*** reset failed ***\n");
	return 0;
}

void flush_cache(ulong start_addr, ulong size)
{
	u32 end, a;
    int i;

    a = start_addr & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
    size = (size + CONFIG_SYS_CACHELINE_SIZE - 1) & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
    end = a + size;

    dcache_flush_range(a, end);
}

void write_one_tlb(int index, u32 pagemask, u32 hi, u32 low0, u32 low1)
{
	write_c0_entrylo0(low0);
	write_c0_pagemask(pagemask);
	write_c0_entrylo1(low1);
	write_c0_entryhi(hi);
	write_c0_index(index);
	tlb_write_indexed();
}
