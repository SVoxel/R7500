/*
 * Copyright (c) 2013 The Linux Foundation. All rights reserved.
 */

#include <common.h>
#include <command.h>
#include <image.h>
#include <nand.h>
#include <errno.h>
#include <asm/arch-ipq806x/smem.h>

#define img_addr	((void *)CONFIG_SYS_LOAD_ADDR)
static int debug = 0;
static ipq_smem_flash_info_t *sfi = &ipq_smem_flash_info;
int ipq_fs_on_nand, rootfs_part_avail = 1;
extern board_ipq806x_params_t *gboard_param;

#ifdef CONFIG_IPQ_LOAD_NSS_FW
/**
 * check if the image and its header is valid and move it to
 * load address as specified in the header
 */
static int load_nss_img(const char *runcmd, char *args, int argslen,
						int nsscore)
{
	char cmd[128];
	int ret;

	if (debug)
		printf(runcmd);

	if ((ret = run_command(runcmd, 0)) != CMD_RET_SUCCESS) {
		return ret;
	}

	sprintf(cmd, "bootm start 0x%x; bootm loados", CONFIG_SYS_LOAD_ADDR);

	if (debug)
		printf(cmd);

	if ((ret = run_command(cmd, 0)) != CMD_RET_SUCCESS) {
		return ret;
	}

	if (args) {
		snprintf(args, argslen, "qca-nss-drv.load%d=0x%x,"
				"qca-nss-drv.entry%d=0x%x,"
				"qca-nss-drv.string%d=\"%.*s\"",
				nsscore, image_get_load(img_addr),
				nsscore, image_get_ep(img_addr),
				nsscore, IH_NMLEN, image_get_name(img_addr));
	}

	return ret;
}

#endif /* CONFIG_IPQ_LOAD_NSS_FW */

/*
 * Set the root device and bootargs for mounting root filesystem.
 */
static int set_fs_bootargs(int *fs_on_nand)
{
	char *bootargs;

#define nand_rootfs	"ubi.mtd=" IPQ_ROOT_FS_PART_NAME " root=mtd:ubi_rootfs"
#define nor_rootfs	"root=mtd:" IPQ_ROOT_FS_PART_NAME

	if (sfi->flash_type == SMEM_BOOT_SPI_FLASH) {

		if (sfi->rootfs.offset == 0xBAD0FF5E) {
			rootfs_part_avail = 0;
			/*
			 * While booting out of SPI-NOR, not having a
			 * 'rootfs' entry in the partition table implies
			 * that the Root FS is available in the NAND flash
			 */
			bootargs = nand_rootfs;
			sfi->rootfs.offset = 0;
			sfi->rootfs.size = IPQ_NAND_ROOTFS_SIZE;
			*fs_on_nand = 1;
		} else {
			bootargs = nor_rootfs;
			*fs_on_nand = 0;
		}
	} else if (sfi->flash_type == SMEM_BOOT_NAND_FLASH) {
		bootargs = nand_rootfs;
		*fs_on_nand = 1;
	} else {
		printf("bootipq: unsupported boot flash type\n");
		return -EINVAL;
	}

	if (getenv("fsbootargs") == NULL)
		setenv("fsbootargs", bootargs);

	return run_command("setenv bootargs ${bootargs} ${fsbootargs}", 0);
}

/**
 * Inovke the dump routine and in case of failure, do not stop unless the user
 * requested to stop
 */
static int inline do_dumpipq_data()
{
	uint64_t etime;

	if (run_command("dumpipq_data", 0) != CMD_RET_SUCCESS) {
		printf("\nAuto crashdump saving failed!"
			"\nPress any key within 10s to take control of U-Boot");

		etime = get_timer_masked() + (10 * CONFIG_SYS_HZ);
		while (get_timer_masked() < etime) {
			if (tstc())
				break;
		}

		if (get_timer_masked() < etime)
			return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

/**
 * Load the NSS images and Kernel image and transfer control to kernel
 */
static int do_bootipq(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	char bootargs[IH_NMLEN+32];
	char runcmd[256];
	int nandid = 0, ret;

#ifdef CONFIG_IPQ_APPSBL_DLOAD
	unsigned long * dmagic1 = (unsigned long *) 0x2A03F000;
	unsigned long * dmagic2 = (unsigned long *) 0x2A03F004;
#endif

	if (argc == 2 && strncmp(argv[1], "debug", 5) == 0)
		debug = 1;

#ifdef CONFIG_IPQ_APPSBL_DLOAD
	/* check if we are in download mode */
	if (*dmagic1 == 0xE47B337D && *dmagic2 == 0x0501CAB0) {
		/* clear the magic and run the dump command */
		*dmagic1 = 0;
		*dmagic2 = 0;
		uint64_t etime = get_timer_masked() + (10 * CONFIG_SYS_HZ);

		printf("\nCrashdump magic found."
			"\nHit any key within 10s to stop dump activity...");
		while (!tstc()) {       /* while no incoming data */
			if (get_timer_masked() >= etime) {
				if (do_dumpipq_data() == CMD_RET_FAILURE)
					return CMD_RET_FAILURE;
				break;
			}
		}

		/* reset the system, some images might not be loaded
		 * when crashmagic is found
		 */
		run_command("reset", 0);
	}
#endif

	if ((ret = set_fs_bootargs(&ipq_fs_on_nand)))
		return ret;

	/* check the smem info to see which flash used for booting */
	if (sfi->flash_type == SMEM_BOOT_SPI_FLASH) {
		nandid = 1;
		if (debug) {
			printf("Using nand device 1\n");
		}
		run_command("nand device 1", 0);
	} else if (sfi->flash_type == SMEM_BOOT_NAND_FLASH) {
		if (debug) {
			printf("Using nand device 0\n");
		}
	} else {
		printf("Unsupported BOOT flash type\n");
		return -1;
	}

#ifdef CONFIG_IPQ_LOAD_NSS_FW
	/* check the smem info to see whether the partition size is valid.
	 * refer board/qcom/ipq806x_cdp/ipq806x_cdp.c:ipq_get_part_details
	 * for more details
	 */
	if (sfi->nss[0].size != 0xBAD0FF5E) {
		sprintf(runcmd, "nand read 0x%x 0x%llx 0x%llx",
				CONFIG_SYS_LOAD_ADDR,
				sfi->nss[0].offset, sfi->nss[0].size);

		if (load_nss_img(runcmd, bootargs, sizeof(bootargs), 0)
				!= CMD_RET_SUCCESS)
			return CMD_RET_FAILURE;

		if (getenv("nssbootargs0") == NULL)
			setenv("nssbootargs0", bootargs);

		run_command("setenv bootargs ${bootargs} ${nssbootargs0}", 0);
	}

	if (sfi->nss[1].size != 0xBAD0FF5E) {
		sprintf(runcmd, "nand read 0x%x 0x%llx 0x%llx",
				CONFIG_SYS_LOAD_ADDR,
				sfi->nss[1].offset, sfi->nss[1].size);

		if (load_nss_img(runcmd, bootargs, sizeof(bootargs), 1)
				!= CMD_RET_SUCCESS)
			return CMD_RET_FAILURE;

		if (getenv("nssbootargs1") == NULL)
			setenv("nssbootargs1", bootargs);

		run_command("setenv bootargs ${bootargs} ${nssbootargs1}", 0);
	}
#endif /* CONFIG_IPQ_LOAD_NSS_FW */

	if (debug) {
		run_command("printenv bootargs", 0);
		printf("Booting from flash\n");
	}

	if (ipq_fs_on_nand) {
		/*
		 * The kernel will be available inside a UBI volume
		 */
		snprintf(runcmd, sizeof(runcmd),
			"set mtdids nand0=nand0 && "
			"set mtdparts mtdparts=nand0:0x%llx@0x%llx(fs),${msmparts} && "
			"ubi part fs && "
			"ubi read 0x%x kernel && "
			"bootm 0x%x\n", sfi->rootfs.size, sfi->rootfs.offset,
				CONFIG_SYS_LOAD_ADDR, CONFIG_SYS_LOAD_ADDR);
	} else {
		/*
		 * Kernel is in a separate partition
		 */
		snprintf(runcmd, sizeof(runcmd),
			/* NOR is treated as psuedo NAND */
			"set mtdids nand1=nand1 && "
			"set mtdparts mtdparts=nand1:${msmparts} && "
			"set autostart yes;"
			"nboot 0x%x %d 0x%llx", CONFIG_SYS_LOAD_ADDR,
				nandid, sfi->hlos.offset);
	}
	if (debug)
		printf(runcmd);

	if (run_command(runcmd, 0) != CMD_RET_SUCCESS)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(bootipq, 2, 0, do_bootipq,
	   "bootipq from flash device",
	   "bootipq [debug] - Load image(s) and boots the kernel\n");

/**
 * Load the NSS images and Kernel image and transfer control to kernel
 *
 * This function is preserved for R7500 so that kernel in raw NAND flash
 * can be boot through method of "bootipq" in QSDK LN.1.0.1.
 */
static int do_bootipq2(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	char bootargs[IH_NMLEN+32];
	char runcmd[128];
	int nandid = 0;
	int fake_fs_on_nand;

#ifdef CONFIG_IPQ_APPSBL_DLOAD
	unsigned long * dmagic1 = (unsigned long *) 0x2A03F000;
	unsigned long * dmagic2 = (unsigned long *) 0x2A03F004;
#endif

	if (argc == 2 && strncmp(argv[1], "debug", 5) == 0)
		debug = 1;

#ifdef CONFIG_IPQ_APPSBL_DLOAD
	/* check if we are in download mode */
	if (*dmagic1 == 0xE47B337D && *dmagic2 == 0x0501CAB0) {
		/* clear the magic and run the dump command */
		*dmagic1 = 0;
		*dmagic2 = 0;
		uint64_t etime = get_timer_masked() + (10 * CONFIG_SYS_HZ);

		printf("\nCrashdump magic found."
			"\nHit any key within 10s to stop dump activity...");
		while (!tstc()) {       /* while no incoming data */
			if (get_timer_masked() >= etime) {
				if (do_dumpipq_data() == CMD_RET_FAILURE)
					return CMD_RET_FAILURE;
				break;
			}
		}

		/* reset the system, some images might not be loaded
		 * when crashmagic is found
		 */
		run_command("reset", 0);
	}
#endif

	set_fs_bootargs(&fake_fs_on_nand);

	/* check the smem info to see which flash used for booting */
	if (sfi->flash_type == SMEM_BOOT_SPI_FLASH) {
		nandid = 1;
		if (debug) {
			printf("Using nand device 1\n");
		}
		run_command("nand device 1", 0);
	} else if (sfi->flash_type == SMEM_BOOT_NAND_FLASH) {
		if (debug) {
			printf("Using nand device 0\n");
		}
	} else {
		printf("Unsupported BOOT flash type\n");
		return -1;
	}

#ifdef CONFIG_IPQ_LOAD_NSS_FW
	/* check the smem info to see whether the partition size is valid.
	 * refer board/qcom/ipq806x_cdp/ipq806x_cdp.c:ipq_get_part_details
	 * for more details
	 */
	if (sfi->nss[0].size != 0xBAD0FF5E) {
		sprintf(runcmd, "nand read 0x%x 0x%llx 0x%llx",
				CONFIG_SYS_LOAD_ADDR,
				sfi->nss[0].offset, sfi->nss[0].size);

		if (load_nss_img(runcmd, bootargs, sizeof(bootargs), 0)
				!= CMD_RET_SUCCESS)
			return CMD_RET_FAILURE;

		if (getenv("nssbootargs0") == NULL)
			setenv("nssbootargs0", bootargs);

		run_command("setenv bootargs ${bootargs} ${nssbootargs0}", 0);
	}

	if (sfi->nss[1].size != 0xBAD0FF5E) {
		sprintf(runcmd, "nand read 0x%x 0x%llx 0x%llx",
				CONFIG_SYS_LOAD_ADDR,
				sfi->nss[1].offset, sfi->nss[1].size);

		if (load_nss_img(runcmd, bootargs, sizeof(bootargs), 1)
				!= CMD_RET_SUCCESS)
			return CMD_RET_FAILURE;

		if (getenv("nssbootargs1") == NULL)
			setenv("nssbootargs1", bootargs);

		run_command("setenv bootargs ${bootargs} ${nssbootargs1}", 0);
	}
#endif /* CONFIG_IPQ_LOAD_NSS_FW */

	if (debug) {
		run_command("printenv bootargs", 0);
		printf("Booting from flash\n");
	}

#if defined(CONFIG_HW29764743P0P128P256P3X3P4X4) || \
    defined(CONFIG_HW29764841P0P128P256P3X3P4X4)
	snprintf(runcmd, sizeof(runcmd), "set autostart yes;"
			"nboot 0x%x %d 0x%llx", CONFIG_SYS_LOAD_ADDR, nandid,
			0x1340000);
#else
	snprintf(runcmd, sizeof(runcmd), "set autostart yes;"
			"nboot 0x%x %d 0x%llx", CONFIG_SYS_LOAD_ADDR, nandid,
			sfi->hlos.offset);
#endif
	if (debug)
		printf(runcmd);

	if (run_command(runcmd, 0) != CMD_RET_SUCCESS)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(bootipq2, 2, 0, do_bootipq2,
	   "bootipq from flash device",
	   "bootipq2 [debug] - Load image(s) and boots the kernel\n");
