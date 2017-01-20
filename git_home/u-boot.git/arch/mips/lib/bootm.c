/*
 * (C) Copyright 2003
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <common.h>
#include <command.h>
#include <image.h>
#include <u-boot/zlib.h>
#include <asm/byteorder.h>
#include <asm/addrspace.h>

#if defined(CONFIG_AR7240) && defined(CONFIG_WASP)
#include <ar7240_soc.h>
#endif
DECLARE_GLOBAL_DATA_PTR;

#define	LINUX_MAX_ENVS		256
#define	LINUX_MAX_ARGS		256

static int linux_argc;
static char **linux_argv;

static char **linux_env;
static char *linux_env_p;
static int linux_env_idx;

static void linux_params_init(ulong start, char *commandline);
static void linux_env_set(char *env_name, char *env_val);

#ifdef CONFIG_WASP_SUPPORT
void wasp_set_cca(void)
{
	/* set cache coherency attribute */
	asm(	"mfc0	$t0,	$16\n"		/* CP0_CONFIG == 16 */
		"li	$t1,	~7\n"
		"and	$t0,	$t0,	$t1\n"
		"ori	$t0,	3\n"		/* CONF_CM_CACHABLE_NONCOHERENT */
		"mtc0	$t0,	$16\n"		/* CP0_CONFIG == 16 */
		"nop\n": : );
}
#endif

int do_bootm_linux(int flag, int argc, char * const argv[],
			bootm_headers_t *images)
{
#if defined(CONFIG_AR7240)
	int flash_size_mbytes;
	void (*theKernel) (int, char **, char **, int);
#else
	void (*theKernel) (int, char **, char **, int *);
#endif
	char *commandline = getenv("bootargs");
	char env_buf[12];
	char *cp;

	if ((flag != 0) && (flag != BOOTM_STATE_OS_GO))
		return 1;

	/* find kernel entry point */
#if defined(CONFIG_AR7240)
	theKernel =
		(void (*)(int, char **, char **, int)) images->ep;
#else
	theKernel = (void (*)(int, char **, char **, int *))images->ep;
#endif

	bootstage_mark(BOOTSTAGE_ID_RUN_OS);

#ifdef DEBUG
	printf("## Transferring control to Linux (at address %08lx) ...\n",
		(ulong) theKernel);
#endif

	linux_params_init(UNCACHED_SDRAM(gd->bd->bi_boot_params), commandline);

#ifdef CONFIG_MEMSIZE_IN_BYTES
	sprintf(env_buf, "%lu", (ulong)gd->ram_size);
	debug("## Giving linux memsize in bytes, %lu\n", (ulong)gd->ram_size);
#else
	sprintf(env_buf, "%lu", (ulong)(gd->ram_size >> 20));
	debug("## Giving linux memsize in MB, %lu\n",
		(ulong)(gd->ram_size >> 20));
#endif /* CONFIG_MEMSIZE_IN_BYTES */

	linux_env_set("memsize", env_buf);

	sprintf(env_buf, "0x%08X", (uint) UNCACHED_SDRAM(images->rd_start));
	linux_env_set("initrd_start", env_buf);

	sprintf(env_buf, "0x%X", (uint) (images->rd_end - images->rd_start));
	linux_env_set("initrd_size", env_buf);

	sprintf(env_buf, "0x%08X", (uint) (gd->bd->bi_flashstart));
	linux_env_set("flash_start", env_buf);

	sprintf(env_buf, "0x%X", (uint) (gd->bd->bi_flashsize));
	linux_env_set("flash_size", env_buf);

	cp = getenv("ethaddr");
	if (cp)
		linux_env_set("ethaddr", cp);

	cp = getenv("eth1addr");
	if (cp)
		linux_env_set("eth1addr", cp);

	/* we assume that the kernel is in place */
	printf("\nStarting kernel ...\n\n");

#ifdef CONFIG_WASP_SUPPORT
	wasp_set_cca();
#endif

#if defined(CONFIG_AR7240)
	/* Pass the flash size as expected by current Linux kernel for AR7240*/
	flash_size_mbytes = gd->bd->bi_flashsize/(1024 * 1024);
	theKernel(linux_argc, linux_argv, linux_env, flash_size_mbytes);
#else
	theKernel(linux_argc, linux_argv, linux_env, 0);
#endif

	/* does not return */
	return 1;
}

static void linux_params_init(ulong start, char *line)
{
	char *next, *quote, *argp;
	char memstr[32];

	linux_argc = 1;
	linux_argv = (char **) start;
	linux_argv[0] = 0;
	argp = (char *) (linux_argv + LINUX_MAX_ARGS);

	next = line;

	if (strstr(line, "mem=")) {
		memstr[0] = 0;
	} else {
		memstr[0] = 1;
	}

	while (line && *line && linux_argc < LINUX_MAX_ARGS) {
		quote = strchr(line, '"');
		next = strchr(line, ' ');

		while (next && quote && quote < next) {
			/* we found a left quote before the next blank
			 * now we have to find the matching right quote
			 */
			next = strchr(quote + 1, '"');
			if (next) {
				quote = strchr(next + 1, '"');
				next = strchr(next + 1, ' ');
			}
		}

		if (!next)
			next = line + strlen(line);

		linux_argv[linux_argc] = argp;
		memcpy(argp, line, next - line);
		argp[next - line] = 0;
#if defined(CONFIG_AR7240) && (defined(CONFIG_WNR2200) || defined(CONFIG_WNR2000V3) || defined(CONFIG_AP121) || defined(CONFIG_WNR1000V4) || defined(CONFIG_HW29763847P16P64) || defined(CONFIG_WASP))
#define REVSTR	"REVISIONID"
#define PYTHON	"python"
#define VIRIAN	"virian"
		if (strcmp(argp, REVSTR) == 0) {
			if (is_ar7241() || is_ar7242()) {
				strcpy(argp, VIRIAN);
			} else {
				strcpy(argp, PYTHON);
			}
		}
#endif

		argp += next - line + 1;
		linux_argc++;

		if (*next)
			next++;

		line = next;
	}

#if defined(CONFIG_AR7240)
	/* Add mem size to command line */
	if (memstr[0]) {
		sprintf(memstr, "mem=%luM", gd->ram_size >> 20);
		memcpy (argp, memstr, strlen(memstr)+1);
		linux_argv[linux_argc] = argp;
		linux_argc++;
		argp += strlen(memstr) + 1;
	}
#endif

	linux_env = (char **) (((ulong) argp + 15) & ~15);
	linux_env[0] = 0;
	linux_env_p = (char *) (linux_env + LINUX_MAX_ENVS);
	linux_env_idx = 0;
}

static void linux_env_set(char *env_name, char *env_val)
{
	if (linux_env_idx < LINUX_MAX_ENVS - 1) {
		linux_env[linux_env_idx] = linux_env_p;

		strcpy(linux_env_p, env_name);
		linux_env_p += strlen(env_name);

		strcpy(linux_env_p, "=");
		linux_env_p += 1;

		strcpy(linux_env_p, env_val);
		linux_env_p += strlen(env_val);

		linux_env_p++;
		linux_env[++linux_env_idx] = 0;
	}
}
