// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2015-2017, 2020 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
/*
 * (C) Copyright 2010
 * Texas Instruments, <www.ti.com>
 *
 * Aneesh V <aneesh@ti.com>
 */
/*
 * Copyright (C) 2018 MediaTek Inc.
 * Author: Ryder Lee <ryder.lee@mediatek.com>
 */

// SPDX-License-Identifier: BSD-2-Clause
/*
   Copyright (c) 2001 William L. Pitts
*/

// SPDX-License-Identifier: BSD-3-Clause
/*
 * Reference to the ARM TF Project,
 * plat/arm/common/arm_bl2_setup.c
 * Portions copyright (c) 2013-2016, ARM Limited and Contributors. All rights
 * reserved.
 * Copyright (C) 2016 Rockchip Electronic Co.,Ltd
 * Written by Kever Yang <kever.yang@rock-chips.com>
 * Copyright (C) 2017 Theobroma Systems Design und Consulting GmbH
 */

#include <clk.h>
#include <hang.h>
#include <cpu_func.h>
#include <init.h>
#include <image.h>
#include <spl.h>
#include <spl_load.h>
#include <mach/ipq.h>
#include <mach/smem_info.h>
#include <asm/io.h>
#include <asm/sections.h>
#include <smem.h>
#include <atf_common.h>
#include <linux/err.h>

/*******************************************************************************
 * Globals constant & typedef
 ******************************************************************************/
#define IPQ_SPL_BOOTCFG_REG_ADDR	0xA602C
#define IPQ_SPL_BOOTCFG_DEV_MASK	GENMASK(3, 1)
#define IPQ_SPL_BOOTCFG_DEV_SHFT	0x1

#define IPQ_SPL_TCSR_REG_ADDR		0x195C100
#define IPQ_SPL_DLOAD_MASK		GENMASK(4, 4)
#define IPQ_SPL_DLOAD_SHFT		0x4

#define IPQ_SPL_IS_DLOAD_BIT_SET	((readl(IPQ_SPL_TCSR_REG_ADDR) & \
					IPQ_SPL_DLOAD_MASK) >> \
					IPQ_SPL_DLOAD_SHFT)

#define IPQ_SPL_FIT_IMG_PARTITION	"0:BOOTLDR"
#define IPQ_SPL_FLASH_RD_PRT_LMT	true

#define IPQ_SPL_BDEV_MAX_SZ		SZ_4K

#define MAGIC_KEY			"QCLIB_CB"
#define MAX_ENTRIES			0xF
#define IF_TABLE_VERSION		0x1
#define QCCONFIG			"qc_config"
#define QCSDI				"qcsdi"

/*******************************************************************************
 * Structure enum and static
 ******************************************************************************/
enum {
	IPQ_SPL_RAM_FLASHLESS = 0xFD,
	IPQ_SPL_FLASHTYPE_MAX = 0xFF
};

enum {
	IPQ_SPL_BOOTCFG_DEV_NOR_GPT	= 0x0,
	IPQ_SPL_BOOTCFG_DEV_MMC		= 0x1,
	IPQ_SPL_BOOTCFG_DEV_SPI_NAND	= 0x2,
	IPQ_SPL_BOOTCFG_DEV_USB		= 0x3,
	IPQ_SPL_BOOTCFG_DEV_NOR_MIBIB	= 0x4,
	IPQ_SPL_BOOTCFG_DEV_MAX
};

/**
 * struct ipq_spl_fl_ops - Structure for flash operations
 * @open:	Function pointer to open a flash partition.
 * @close:	Function pointer to close a flash partition.
 * @read:	Function pointer to read from a flash partition.
 */
struct ipq_spl_fl_ops {
	ulong (*open)(struct spl_load_info *load, char *prt_name);
	ulong (*close)(struct spl_load_info *load);
	ulong (*read)(struct spl_load_info *load, ulong sector,
		      ulong count, void *buf);
};

/**
 * struct ipq_spl_fl_ctx - SPL flash context
 * @load:	SPL load info structure.
 * @ops:	Pointer to flash operations structure.
 * @type:	Type of flash device.
 */
struct ipq_spl_fl_ctx {
	struct spl_load_info load;
	struct ipq_spl_fl_ops *ops;
	u8 type;
};

/**
 * struct interface_table_entry - Meta data for blobs in QCLIB interface
 * @entry_name:	Name of the data blob (e.g., "dcb_settings").
 * @address:	Address of the data blob.
 * @size:	Size of the data blob.
 * @attributes:	Attributes for the blob (e.g., save to storage).
 */
struct interface_table_entry {
	char entry_name[24];
	u64 address;
	u32 size;
	u32 attributes;
};

/**
 * struct interface_table - QCLIB Interface table header
 * @magic_key:		Magic key for validation ("QCLIB_CB").
 * @version:		Interface table version.
 * @num_entries:	Number of valid entries.
 * @max_entries:	Maximum allowable entries.
 * @global_attributes:	Flags for global attributes (e.g., SDI path).
 * @reserved1:		Reserved for future use.
 * @reserved2:		Reserved for future use.
 * @if_table_entries:	Array of interface table entries.
 */
struct interface_table {
	char magic_key[8];
	u32 version;
	u32 num_entries;
	u32 max_entries;
	u32 global_attributes;
	u32 reserved1;
	u32 reserved2;
	struct interface_table_entry if_table_entries[MAX_ENTRIES];
};

/**
 * struct ipq_spl_img_ctx - SPL image context
 * @img_name:	Name of the image.
 * @prt_name:	Partition name where the image resides.
 * @load_addr:	Load address of the image.
 * @img_sz:	Size of the image.
 * @img_off:	Offset of the image within the partition.
 * @load:	Flag indicating if the image should be loaded.
 * @auth:	Authentication flag.
 * @optional:	Flag indicating if the image loading is optional.
 * @fit_node:	Node ID for FIT image component.
 * @fixup:	Function pointer for image-specific fixup operations.
 */
struct ipq_spl_img_ctx {
	char *img_name;
	char *prt_name;
	u64 load_addr;
	u64 img_sz;
	u64 img_off;
	u8 load;
	u8 auth;
	u8 optional;
	int fit_node;
	int (*fixup)(void *ctx);
};

/**
 * struct ipq_spl_ctx - Global SPL context structure
 * @img_tbl:	Pointer to the current image table entry.
 * @if_tbl:	QCLIB interface table.
 * @elf_ctx:	ELF context structure (if ELF loading is enabled).
 * @fl_ctx:	Flash context structure.
 * @spl_image:	SPL image info structure.
 * @bootdev:	SPL boot device structure.
 * @fit:	Pointer to the loaded FIT image blob.
 * @bl31_entry:	Entry point for BL31 (TF-A).
 * @bl32_entry:	Entry point for BL32 (OP-TEE).
 * @bl33_entry:	Entry point for BL33 (U-Boot/kernel).
 */
struct ipq_spl_ctx {
	struct ipq_spl_img_ctx *img_tbl;
	struct interface_table if_tbl;
	struct ipq_spl_fl_ctx fl_ctx;
	struct spl_image_info *spl_image;
	struct spl_boot_device *bootdev;
	void *fit;
};

#define U_BOOT_IPQ_SPL_CTX(__name) \
ll_entry_declare(struct ipq_spl_ctx, __name, ipq_spl_ctx)

#define U_BOOT_GET_IPQ_SPL_CTX(__name) \
llsym(struct ipq_spl_ctx, __name, ipq_spl_ctx)

/*
 * Declare IPQ SPL default context
 */
U_BOOT_IPQ_SPL_CTX(ipq_default_ctx);

static int ipq_spl_loader_post_ddr(struct spl_image_info *spl_image,
				   struct spl_boot_device *bootdev);

/**
 * Forward declarations for RAM operations
 */
static ulong ipq_spl_ram_open(struct spl_load_info *load, char *prt_name);
static ulong ipq_spl_ram_close(struct spl_load_info *load);
static ulong ipq_spl_ram_read(struct spl_load_info *load, ulong sector,
			      ulong count, void *buf);

/**
 * ipq_spl_ram_ops - Flash operations structure for RAM-less mode
 */
struct ipq_spl_fl_ops ipq_spl_ram_ops = {
	.open = ipq_spl_ram_open,
	.close = ipq_spl_ram_close,
	.read = ipq_spl_ram_read,
};

#if CONFIG_IPQ_MMC
/**
 * Forward declarations for MMC operations
 */
static ulong ipq_spl_mmc_open(struct spl_load_info *load, char *prt_name);
static ulong ipq_spl_mmc_close(struct spl_load_info *load);
static ulong ipq_spl_mmc_read(struct spl_load_info *load, ulong sector,
			      ulong count, void *buf);

/**
 * ipq_spl_mmc_ops - Flash operations structure for MMC
 */
struct ipq_spl_fl_ops ipq_spl_mmc_ops = {
	.open = ipq_spl_mmc_open,
	.close = ipq_spl_mmc_close,
	.read = ipq_spl_mmc_read,
};

SPL_LOAD_IMAGE_METHOD("MMC", 0, SMEM_BOOT_MMC_FLASH,
		      ipq_spl_loader_post_ddr);
#endif

#if CONFIG_IPQ_SPI_NOR
/**
 * Forward declarations for NOR operations
 */
static ulong ipq_spl_nor_open(struct spl_load_info *load, char *prt_name);
static ulong ipq_spl_nor_close(struct spl_load_info *load);
static ulong ipq_spl_nor_read(struct spl_load_info *load, ulong sector,
			      ulong count, void *buf);

/**
 * ipq_spl_nor_ops - Flash operations structure for SPI NOR
 */
struct ipq_spl_fl_ops ipq_spl_nor_ops = {
	.open = ipq_spl_nor_open,
	.close = ipq_spl_nor_close,
	.read = ipq_spl_nor_read,
};

SPL_LOAD_IMAGE_METHOD("NOR GPT", 0, SMEM_BOOT_NORGPT_FLASH,
		      ipq_spl_loader_post_ddr);
#endif

#if CONFIG_IPQ_NAND
/**
 * Forward declarations for NAND operations
 */
static ulong ipq_spl_nand_open(struct spl_load_info *load, char *prt_name);
static ulong ipq_spl_nand_close(struct spl_load_info *load);
static ulong ipq_spl_nand_read(struct spl_load_info *load, ulong sector,
			       ulong count, void *buf);

/**
 * ipq_spl_nand_ops - Flash operations structure for NAND
 */
struct ipq_spl_fl_ops ipq_spl_nand_ops = {
	.open = ipq_spl_nand_open,
	.close = ipq_spl_nand_close,
	.read = ipq_spl_nand_read,
};

SPL_LOAD_IMAGE_METHOD("SPI NAND", 0, SMEM_BOOT_QSPI_NAND_FLASH,
		      ipq_spl_loader_post_ddr);
#endif

/**
 * ipq_spl_jump_img_entry_t - Type definition for image entry point functions.
 * @arg1:	First argument passed to the entry point.
 * @arg2:	Second argument passed to the entry point.
 */
typedef void (*ipq_spl_jump_img_entry_t)(void *arg1, void *arg2);

/* Forward declarations for image fixup functions */
static int ipq_spl_xcfg_fixup(void *ctx);
static int ipq_spl_qclib_fixup(void *ctx);
static int ipq_spl_tfa_fixup(void *ctx);
static int ipq_spl_optee_fixup(void *ctx);
static int ipq_spl_uboot_fixup(void *ctx);

/**
 * img_tbl_fit - Image loader table for FIT images.
 *
 * This table defines the FIT images to be loaded and their associated fixup
 * functions.
 */
struct ipq_spl_img_ctx img_tbl_fit[] = {
	{
		.img_name = "qcconfig-meta",
		.auth = true,
		.fixup = ipq_spl_xcfg_fixup,
	}, {
		.img_name = "qclib-meta",
		.auth = true,
		.fixup = ipq_spl_qclib_fixup,
	}, {
		.img_name = "tfa_bl31-meta",
		.auth = true,
		.fixup = ipq_spl_tfa_fixup,
	}, {
		.img_name = "optee-meta",
		.auth = true,
		.fixup = ipq_spl_optee_fixup,
	}, {
		.img_name = "uboot-meta",
		.auth = true,
		.fixup = ipq_spl_uboot_fixup,
	},
};

/*******************************************************************************
 * Function definition
 ******************************************************************************/

/**
 * lowlevel_init() - Early low-level initialization.
 *
 * This function performs very early hardware initialization,
 * specifically disabling the MMU, which is a common requirement for
 * bare-metal bootloaders before memory management is fully set up.
 */
void lowlevel_init(void)
{
	unsigned long sctlr;

	/*
	 * Early disable the MMU
	 */
	sctlr = get_sctlr();
	set_sctlr(sctlr & ~(CR_M));
}

/**
 * ipq_spl_error_handler() - Centralized SPL error handler.
 * @arg:	Generic argument (unused).
 *
 * This function is invoked upon critical errors during the SPL boot process.
 * It currently prints an error message and halts the system.
 * TODO: Implement more robust error handling (e.g., logging, recovery attempts).
 */
void ipq_spl_error_handler(void *arg)
{
	pr_err("Entered the SPL Error Handler\n");
	/*
	 * TODO: Implement SPL error handler
	 */
	hang();
}

#if defined(CFG_EMUL_FREQUENCY_DIVIDER)
/**
 * ipq_spl_setup_arch_cntfreq() - Set up architecture counter frequency.
 *
 * This function is used in emulation environments to divide the system
 * counter frequency for compatibility.
 */
void ipq_spl_setup_arch_cntfreq(void)
{
	unsigned long freq;

	/*
	 * Emulation - Divide the counter frequency
	 */
	if ((CONFIG_COUNTER_FREQUENCY > 0) &&
		(CFG_EMUL_FREQUENCY_DIVIDER > 0)) {
		freq = CONFIG_COUNTER_FREQUENCY / CFG_EMUL_FREQUENCY_DIVIDER;
		asm volatile("msr cntfrq_el0, %0" : : "r" (freq) : "memory");
	}
}
#endif

#if CONFIG_IS_ENABLED(SYS_MALLOC_F)
/**
 * ipq_spl_malloc_init_f() - Initialize malloc for SPL.
 *
 * This function initializes the malloc subsystem using the memory region
 */
void ipq_spl_malloc_init_f(void)
{
	/*
	 * Set up by crt0.S
	 */
	assert(gd->malloc_base);
	gd->malloc_limit = CONFIG_VAL(SYS_MALLOC_F_LEN);
	gd->malloc_ptr = 0;

	mem_malloc_init(gd->malloc_base, gd->malloc_limit);
	gd->flags |= GD_FLG_FULL_MALLOC_INIT;
}
#endif

/**
 * ipq_spl_flash_init() - Initialize the flash device.
 * @fl_ctx:	Pointer to the SPL flash context.
 *
 * This function performs early initialization specific to the detected
 * flash device type (e.g., MMC).
 * Return: 0 on success, or a negative error code on failure.
 */
int ipq_spl_flash_init(struct ipq_spl_fl_ctx *fl_ctx)
{
	int ret;

	if (!fl_ctx) {
		pr_err("Invalid flash context\n");
		return -EINVAL;
	}

	switch (fl_ctx->type) {
#if CONFIG_IPQ_MMC
	case SMEM_BOOT_MMC_FLASH:
		/*
		 * Initialize MMC
		 */
		struct mmc *mmc;

		/*
		 * Initialize to default device
		 */
		int mmc_dev = 0;

		ret = mmc_init_device(mmc_dev);
		if (ret) {
			pr_err("mmc_init_device() failed (ret=%d)\n", ret);
			return ret;
		}

		mmc = find_mmc_device(mmc_dev);
		if (!mmc) {
			pr_err("find_mmc_device() failed\n");
			return -EIO;
		}

		ret = mmc_init(mmc);
		if (ret) {
			pr_err("mmc_init() failed (ret=%d)\n", ret);
			return ret;
		}
		break;
#endif
#if CONFIG_IPQ_SPI_NOR
	case SMEM_BOOT_NORGPT_FLASH:
		/*
		 * Initialize SPI NOR - Placeholder for future implementation
		 */
		pr_debug("SPI NOR initialization placeholder\n");
		break;
#endif
#if CONFIG_IPQ_NAND
	case SMEM_BOOT_QSPI_NAND_FLASH:
		/*
		 * Initialize NAND - Placeholder for future implementation
		 */
		pr_debug("NAND initialization placeholder\n");
		break;
#endif
	case IPQ_SPL_RAM_FLASHLESS:
		pr_debug("RAM-less flash type selected\n");
		break;
	default:
		pr_err("Unsupported flash type %d\n", fl_ctx->type);
		return -EINVAL;
	}

	return 0;
}

/**
 * ipq_spl_flash_get_ops() - Get flash operations for the current flash context.
 * @fl_ctx:	Pointer to the SPL flash context.
 *
 * This function assigns the appropriate flash operations (open, close, read)
 * based on the flash device type specified in the context.
 * Return: 0 on success, or a negative error code on failure.
 */
int ipq_spl_flash_get_ops(struct ipq_spl_fl_ctx *fl_ctx)
{
	if (!fl_ctx) {
		pr_err("Invalid flash context\n");
		return -EINVAL;
	}

	switch (fl_ctx->type) {
#if CONFIG_IPQ_MMC
	case SMEM_BOOT_MMC_FLASH:
		fl_ctx->ops = &ipq_spl_mmc_ops;
		fl_ctx->load.read = ipq_spl_mmc_ops.read;
		fl_ctx->load.bl_len = 1;
		break;
#endif
#if CONFIG_IPQ_SPI_NOR
	case SMEM_BOOT_NORGPT_FLASH:
		fl_ctx->ops = &ipq_spl_nor_ops;
		fl_ctx->load.read = ipq_spl_nor_ops.read;
		fl_ctx->load.bl_len = 1;
		break;
#endif
#if CONFIG_IPQ_NAND
	case SMEM_BOOT_QSPI_NAND_FLASH:
		fl_ctx->ops = &ipq_spl_nand_ops;
		fl_ctx->load.read = ipq_spl_nand_ops.read;
		fl_ctx->load.bl_len = 1;
		break;
#endif
	case IPQ_SPL_RAM_FLASHLESS:
		fl_ctx->ops = &ipq_spl_ram_ops;
		fl_ctx->load.read = ipq_spl_ram_ops.read;
		fl_ctx->load.bl_len = 1;
		break;
	default:
		pr_err("Unsupported flash type %d\n", fl_ctx->type);
		return -EINVAL;
	}

	return 0;
}

/**
 * ipq_spl_verify_partition_limit() - Verify read offset against partition end.
 * @rd_offset:	Current read offset.
 * @prt_end:	End boundary of the partition.
 *
 * This function checks if the read operation would go beyond the allocated
 * partition limits, based on IPQ_SPL_FLASH_RD_PRT_LMT define.
 * Return: 0 on success, or -EOVERFLOW if limit is exceeded.
 */
static int ipq_spl_verify_partition_limit(int rd_offset, int prt_end)
{
#if IPQ_SPL_FLASH_RD_PRT_LMT
	if (prt_end < rd_offset) {
		pr_err("Read offset %d exceeds partition end %d\n",
			rd_offset, prt_end);
		return -EOVERFLOW;
	}
#endif

	return 0;
}

/**
 * ipq_spl_ram_close() - Close RAM-based flash operations.
 * @load:	Pointer to the SPL load info structure.
 *
 * This is a no-op for RAM-based operations.
 * Return: Always 0.
 */
static ulong ipq_spl_ram_close(struct spl_load_info *load)
{
	return 0;
}

/**
 * ipq_spl_ram_open() - Open RAM-based flash operations.
 * @load:	Pointer to the SPL load info structure.
 * @part_name:	Name of the partition (unused for RAM).
 *
 * This is a no-op for RAM-based operations.
 * Return: Always 0.
 */
static ulong ipq_spl_ram_open(struct spl_load_info *load, char *part_name)
{
	return 0;
}

/**
 * ipq_spl_ram_read() - Read data from RAM-based source.
 * @load:	Pointer to the SPL load info structure.
 * @sector:	Starting sector/offset for the read.
 * @count:	Number of bytes to read.
 * @buf:	Buffer to store the read data.
 *
 * This function copies data directly from memory, simulating a read
 * from a flash device in RAM-less mode.
 * Return: Number of bytes read on success, or zero on failure.
 */
static ulong ipq_spl_ram_read(struct spl_load_info *load, ulong sector,
			      ulong count, void *buf)
{
	struct ipq_spl_ctx *ctx = U_BOOT_GET_IPQ_SPL_CTX(ipq_default_ctx);

	if (!buf) {
		pr_err("Read buffer is NULL\n");
		return 0;
	}

	/*
	 * Check if the image offset is shifted from the partition start
	 */
	if (ctx) {
		if (ctx->img_tbl)
			sector = ctx->img_tbl->img_off + sector;
	}

	memcpy(buf, (void *)sector, count);

	return count;
}

#if CONFIG_IPQ_MMC
/**
 * ipq_spl_mmc_close() - Close MMC flash operations.
 * @load:	Pointer to the SPL load info structure.
 *
 * This function frees resources allocated during MMC open.
 * Return: 0 on success, or a negative error code on failure.
 */
static ulong ipq_spl_mmc_close(struct spl_load_info *load)
{
	struct blkpart_info *bpart_info;

	if (!load) {
		pr_err("Invalid SPL load info\n");
		return -EINVAL;
	}

	if (!load->priv) {
		pr_debug("Media already closed\n");
		/*
		 * media already closed
		 */
		return 0;
	}

	bpart_info = (struct blkpart_info *)load->priv;

	/*
	 * Free the disk info
	 */
	if (bpart_info->info)
		free(bpart_info->info);

	/*
	 * Free the blkpart info
	 */
	free(bpart_info);

	/*
	 * Initialize the media handle
	 */
	load->priv = NULL;

	return 0;
}

/**
 * ipq_spl_mmc_open() - Open an MMC partition for reading.
 * @load:	Pointer to the SPL load info structure.
 * @prt_name:	Name of the partition to open.
 *
 * This function initializes MMC and retrieves partition information.
 * It also handles previous open media by closing it first.
 * Return: 0 on success, or a negative error code on failure.
 */
static ulong ipq_spl_mmc_open(struct spl_load_info *load, char *prt_name)
{
	int ret;
	struct disk_partition *disk_info = NULL;
	struct blkpart_info *bpart_info = NULL;
	struct blk_desc *bdev;
	int part_num;

	if (!load) {
		pr_err("Invalid SPL load info\n");
		return -EINVAL;
	}

	if (!prt_name) {
		pr_err("Partition name is NULL\n");
		return -EINVAL;
	}

	/*
	 * Close any previously opened media
	 */
	if (load->priv) {
		ret = ipq_spl_mmc_close(load);
		if (ret)
			return ret;
	}

	disk_info = calloc(1, sizeof(struct disk_partition));
	if (!disk_info) {
		pr_err("Failed to allocate disk_info\n");
		ret = -ENOMEM;
		goto fail;
	}

	bpart_info = calloc(1, sizeof(struct blkpart_info));
	if (!bpart_info) {
		pr_err("Failed to allocate bpart_info\n");
		ret = -ENOMEM;
		goto fail;
	}

	/*
	 * Populate MMC blk info
	 */
	bpart_info->name = prt_name;
	bpart_info->info = disk_info;
	bpart_info->devnum = 0;
	bpart_info->verbose = true;

	/*
	 * Assign the mmc block pointer to the media handle
	 */
	load->priv = bpart_info;

	/*
	 * Get MMC device by Class
	 */
	bdev = blk_get_devnum_by_uclass_id(UCLASS_MMC, bpart_info->devnum);
	if (!bdev) {
		pr_err("No such MMC device\n");
		ret = -ENODEV;
		goto fail;
	}

#ifdef CONFIG_EFI_PARTITION
	if (bdev->part_type == PART_TYPE_UNKNOWN)
		bdev->part_type = PART_TYPE_EFI;
#endif
	/*
	 * Get MMC partition info using the partition name
	 */
	bpart_info->desc = bdev;
	part_num = part_get_info_by_name(bdev,
					 bpart_info->name,
					 bpart_info->info);
	if (part_num < 0) {
		if (bpart_info->verbose)
			pr_err("Partition '%s' not found (ret=%d)\n",
				bpart_info->name, part_num);
		ret = -ENODEV;
		goto fail;
	}

	return 0;

fail:
	if (disk_info)
		free(disk_info);
	if (bpart_info)
		free(bpart_info);
	/*
	 * Ensure load->priv is NULL on error
	 */
	load->priv = NULL;

	return ret;
}

/**
 * ipq_spl_mmc_read() - Read data from an MMC partition.
 * @load:	Pointer to the SPL load info structure.
 * @sector:	Starting sector/offset for the read.
 * @count:	Number of bytes to read.
 * @buf:	Buffer to store the read data.
 *
 * This function handles block-aligned and partial reads from an MMC device.
 * Return: Number of bytes read on success, or zero on failure.
 */
static ulong ipq_spl_mmc_read(struct spl_load_info *load, ulong sector,
			      ulong count, void *buf)
{
	int ret;
	/*
	 * Common buffer to read one block
	 */
	u8 rd_blk_buf[IPQ_SPL_BDEV_MAX_SZ];
	ulong rd_count = count;

	/*
	 * Variables to handle flash read in block devices
	 */
	u32 start_blk;
	u32 blk_cnt;
	u32 end_blk;
	u32 byte_offset;
	u32 byte_cnt;
	struct blkpart_info *bpart_info;
	struct ipq_spl_ctx *ctx = U_BOOT_GET_IPQ_SPL_CTX(ipq_default_ctx);

	if (!buf) {
		pr_err("Read buffer is NULL\n");
		return 0;
	}

	if (!load) {
		pr_err("Invalid SPL load info\n");
		return 0;
	}

	if (!load->priv) {
		pr_err("Invalid private data in load context\n");
		return 0;
	}

	bpart_info = (struct blkpart_info *)load->priv;

	/*
	 * Check the image offset from the partition start
	 */
	if (ctx) {
		if (ctx->img_tbl)
			sector = ctx->img_tbl->img_off + sector;
	}

	/*
	 * Populate block read variables
	 */
	start_blk = bpart_info->info->start;
	start_blk += sector / bpart_info->info->blksz;

	byte_offset = sector % bpart_info->info->blksz;
	end_blk = bpart_info->info->start + bpart_info->info->size;

	/*
	 * Handle partial read for the first block
	 */
	if (byte_offset > 0) {
		blk_cnt = 1;

		ret = ipq_spl_verify_partition_limit((int)(start_blk + blk_cnt),
						     (int)end_blk);
		if (ret) {
			pr_err("Partition limit check failed (ret=%d)\n", ret);
			return 0;
		}

		memset(rd_blk_buf, 0, bpart_info->info->blksz);

		ret = blk_dread(bpart_info->desc,
					start_blk,
					blk_cnt,
					rd_blk_buf);
		if (ret != blk_cnt) {
			pr_err("MMC block read error for first block\n");
			goto fail;
		}

		byte_cnt = bpart_info->info->blksz - byte_offset;
		/*
		 * Get the partial bytes in given size
		 */
		byte_cnt = min_t(u64, byte_cnt, rd_count);

		memcpy(buf, &rd_blk_buf[byte_offset], byte_cnt);

		/*
		 * Update the offsets after read
		 */
		start_blk += blk_cnt;
		buf = (u8 *)buf + byte_cnt;
		rd_count -= byte_cnt;
	}

	/*
	 * Handle block read for full blocks in the remaining size
	 */
	blk_cnt = rd_count / bpart_info->info->blksz;
	if (blk_cnt > 0) {
		ret = ipq_spl_verify_partition_limit((int)(start_blk + blk_cnt),
						     (int)end_blk);
		if (ret) {
			pr_err("Partition limit check failed (ret=%d)\n", ret);
			return 0;
		}

		ret = blk_dread(bpart_info->desc,
					start_blk,
					blk_cnt,
					buf);
		if (ret != blk_cnt) {
			pr_err("MMC block read error for full blocks\n");
			goto fail;
		}

		/*
		 * Update the offsets after read
		 */
		start_blk += blk_cnt;
		buf = (u8 *)buf + (blk_cnt * bpart_info->info->blksz);
		rd_count -= blk_cnt * bpart_info->info->blksz;
	}

	/*
	 * Handle partial read for the last block in the remaining size
	 */
	byte_cnt = rd_count;
	if (byte_cnt > 0) {
		blk_cnt = 1;

		ret = ipq_spl_verify_partition_limit((int)(start_blk + blk_cnt),
						     (int)end_blk);
		if (ret) {
			pr_err("Partition limit check failed (ret=%d)\n", ret);
			return 0;
		}

		memset(rd_blk_buf, 0, bpart_info->info->blksz);

		ret = blk_dread(bpart_info->desc,
					start_blk,
					blk_cnt,
					rd_blk_buf);
		if (ret != blk_cnt) {
			pr_err("MMC block read error for full blocks\n");
			goto fail;
		}

		/*
		 * copy partial read bytes
		 */
		memcpy(buf, rd_blk_buf, byte_cnt);

		/*
		 * Update the offsets after read
		 */
		start_blk += blk_cnt;
		buf = (u8 *)buf + byte_cnt;
		rd_count -= byte_cnt;
	}
	return count;

fail:

	return ret;
}
#endif /* CONFIG_IPQ_MMC */

#if CONFIG_IPQ_SPI_NOR
/**
 * ipq_spl_nor_close() - Close SPI NOR flash operations.
 * @load:	Pointer to the SPL load info structure.
 *
 * This is a placeholder for SPI NOR close functionality.
 * Return: Always 0.
 */
static ulong ipq_spl_nor_close(struct spl_load_info *load)
{
	pr_debug("SPI NOR close placeholder\n");

	return 0;
}

/**
 * ipq_spl_nor_open() - Open SPI NOR flash operations.
 * @load:	Pointer to the SPL load info structure.
 * @part_name:	Name of the partition (unused for placeholder).
 *
 * This is a placeholder for SPI NOR open functionality.
 * Return: Always 0.
 */
static ulong ipq_spl_nor_open(struct spl_load_info *load, char *part_name)
{
	pr_debug("SPI NOR open placeholder for partition %s\n", part_name);

	return 0;
}

/**
 * ipq_spl_nor_read() - Read data from SPI NOR flash.
 * @load:	Pointer to the SPL load info structure.
 * @sector:	Starting sector/offset for the read.
 * @count:	Number of bytes to read.
 * @buf:	Buffer to store the read data.
 *
 * This is a placeholder for SPI NOR read functionality.
 * Return: Number of bytes requested (simulated success for placeholder).
 */
static ulong ipq_spl_nor_read(struct spl_load_info *load, ulong sector,
			      ulong count, void *buf)
{
	pr_debug("SPI NOR read placeholder (sector=%lu, count=%lu)\n",
		 sector, count);
	/*
	 * Simulate a successful read of 'count' bytes for placeholder
	 */
	return count;
}
#endif /* CONFIG_IPQ_SPI_NOR */

#if CONFIG_IPQ_NAND
/**
 * ipq_spl_nand_close() - Close NAND flash operations.
 * @load:	Pointer to the SPL load info structure.
 *
 * This is a placeholder for NAND close functionality.
 * Return: Always 0.
 */
static ulong ipq_spl_nand_close(struct spl_load_info *load)
{
	pr_debug("NAND close placeholder\n");

	return 0;
}

/**
 * ipq_spl_nand_open() - Open NAND flash operations.
 * @load:	Pointer to the SPL load info structure.
 * @part_name:	Name of the partition (unused for placeholder).
 *
 * This is a placeholder for NAND open functionality.
 * Return: Always 0.
 */
static ulong ipq_spl_nand_open(struct spl_load_info *load, char *part_name)
{
	pr_debug("NAND open placeholder for partition %s\n", part_name);

	return 0;
}

/**
 * ipq_spl_nand_read() - Read data from NAND flash.
 * @load:	Pointer to the SPL load info structure.
 * @sector:	Starting sector/offset for the read.
 * @count:	Number of bytes to read.
 * @buf:	Buffer to store the read data.
 *
 * This is a placeholder for NAND read functionality.
 * Return: Number of bytes requested (simulated success for placeholder).
 */
static ulong ipq_spl_nand_read(struct spl_load_info *load, ulong sector,
			       ulong count, void *buf)
{
	pr_debug("NAND read placeholder (sector=%lu, count=%lu)\n",
		 sector, count);
	/*
	 * Simulate a successful read of 'count' bytes for placeholder
	 */
	return count;
}
#endif /* CONFIG_IPQ_NAND */

/**
 * ipq_spl_get_fit_img_entry_point() - Get entry point from FIT image node.
 * @fit:	Pointer to the FIT image blob.
 * @node:	Node ID within the FIT image.
 * @entry_point:Pointer to store the retrieved entry point.
 *
 * This function attempts to retrieve the entry point from a FIT image node.
 * If not explicitly defined, it falls back to the load address.
 * Return: 0 on success, or a negative error code on failure.
 */
static int ipq_spl_get_fit_img_entry_point(void *fit,
						int node,
						u64 *entry_point)
{
	int ret;

	if (!fit) {
		pr_err("FIT image blob is NULL\n");
		return -EINVAL;
	}
	if (node <= 0) {
		pr_err("Invalid FIT node ID %d\n", node);
		return -EINVAL;
	}
	if (!entry_point) {
		pr_err("Entry point pointer is NULL\n");
		return -EINVAL;
	}

	ret = fit_image_get_entry(fit, node, (ulong *)entry_point);
	if (ret) {
		pr_debug("No entry point for node %d, trying load address\n",
			 node);
		ret = fit_image_get_load(fit, node, (ulong *)entry_point);
		if (ret)
			pr_err("No load address for node %d (ret=%d)\n",
				node, ret);
	}

	return ret;
}

/**
 * ipq_spl_populate_smem() - Populate shared memory (SMEM) information.
 * @ctx:	Pointer to the global SPL context.
 *
 * This function initializes and populates various SMEM items with boot-related
 * information, such as flash type, try-mode status, and ATF enable status.
 * TODO: Populate the SMEM MIBIB Info.
 * Return: 0 on success, or a negative error code on failure.
 */
static int ipq_spl_populate_smem(void *ctx)
{
	int ret;
	struct ipq_spl_ctx *pctx = ctx;
	struct udevice *smem;
	size_t size;
	u32 *fltype;
	u32 *trymode;
	u32 *atf_en;

	if (!pctx) {
		pr_err("Invalid SPL context\n");
		return -EINVAL;
	}

	/*
	 * Populate SMEM
	 */
	ipq_uboot_fdt_fixup((void *)gd->fdt_blob, UBOOT_FIXUP_SMEM);
	ret = uclass_get_device(UCLASS_SMEM, 0, &smem);
	if (ret) {
		pr_err("Failed to find SMEM node (ret=%d)\n", ret);
		return ret;
	}

	/*
	 * Populate Flash Type
	 */
	size = sizeof(u32);
	ret = smem_alloc(smem, -1, SMEM_BOOT_FLASH_TYPE, size);
	if (ret) {
		pr_err("Failed to alloc item: SMEM_BOOT_FLASH_TYPE (ret=%d)\n",
			ret);
		return ret;
	}

	fltype = (u32 *)smem_get(smem, -1, SMEM_BOOT_FLASH_TYPE, &size);
	if (!fltype) {
		pr_err("Failed to get item: SMEM_BOOT_FLASH_TYPE\n");
		return -ENOENT;
	}
	*fltype = pctx->fl_ctx.type;

	/*
	 * Populate Trymode info
	 */
	size = sizeof(u32);
	ret = smem_alloc(smem, -1, SMEM_TRY_MODE_INPROGRESS, size);
	if (ret) {
		pr_err(
		"Failed to alloc item: SMEM_TRY_MODE_INPROGRESS (ret=%d)\n",
		ret);
		return ret;
	}

	trymode = (u32 *)smem_get(smem, -1, SMEM_TRY_MODE_INPROGRESS, &size);
	if (!trymode) {
		pr_err("Failed to get item: SMEM_TRY_MODE_INPROGRESS\n");
		return -ENOENT;
	}
	*trymode = false;

	/*
	 * Populate ATF info
	 */
	size = sizeof(u32);
	ret = smem_alloc(smem, -1, SMEM_ATF_ENABLE, size);
	if (ret) {
		pr_err("Failed to alloc item: SMEM_ATF_ENABLE (ret=%d)\n",
			ret);
		return ret;
	}

	atf_en = (u32 *)smem_get(smem, -1, SMEM_ATF_ENABLE, &size);
	if (!atf_en) {
		pr_err("Failed to get item: SMEM_ATF_ENABLE\n");
		return -ENOENT;
	}
	*atf_en = true;

	/*
	 * TODO: Populate the SMEM MIBIB Info
	 */
	return 0;
}

/**
 * ipq_spl_get_iftbl_entry_by_name() - Get an interface table entry by name.
 * @if_tbl:	Pointer to the QCLIB interface table.
 * @name:	Name of the entry to find.
 * @entry:	Pointer to a buffer where the found entry will be copied.
 *
 * This function searches the provided interface table for an entry
 * matching the given name and copies it to the output buffer if found.
 *
 * Return: 0 on success, or a negative error code on failure.
 */
static int ipq_spl_get_iftbl_entry_by_name(struct interface_table *if_tbl,
					   char *name,
					   struct interface_table_entry *entry)
{
	u8 uc_index;

	if (!if_tbl) {
		pr_err("Invalid interface table\n");
		return -EINVAL;
	}
	if (!name) {
		pr_err("Invalid name\n");
		return -EINVAL;
	}
	if (!entry) {
		pr_err("Invalid entry pointer\n");
		return -EINVAL;
	}

	for (uc_index = 0; uc_index < MAX_ENTRIES; uc_index++) {
		/*
		 * Find the entry with the matching name
		 */
		if (strcmp(if_tbl->if_table_entries[uc_index].entry_name,
			   name) == 0) {
			memcpy(entry,
			       &if_tbl->if_table_entries[uc_index],
			       sizeof(struct interface_table_entry));
			return 0;
		}
	}
	pr_err("Interface table entry '%s' not found\n", name);

	return -ENOENT;
}

/**
 * ipq_spl_xcfg_fixup() - Perform fixups for qcconfig-meta image.
 * @ctx:	Pointer to the global SPL context.
 *
 * This function initializes and populates the QCLIB interface table with
 * information about the qcconfig-meta image's entry point.
 * Return: 0 on success, or a negative error code on failure.
 */
static int ipq_spl_xcfg_fixup(void *ctx)
{
	int ret;
	int entry_idx;
	struct ipq_spl_ctx *pctx = ctx;

	if (!pctx) {
		pr_err("Invalid SPL context\n");
		return -EINVAL;
	}

	if (!pctx->img_tbl) {
		pr_err("Image table is NULL\n");
		return -EINVAL;
	}

	if (!pctx->fit) {
		pr_err("FIT image not loaded\n");
		return -EINVAL;
	}

	/*
	 * Initialize the interface table
	 */
	memset(&pctx->if_tbl, 0, sizeof(struct interface_table));
	memcpy(pctx->if_tbl.magic_key, MAGIC_KEY, strlen(MAGIC_KEY));

	pctx->if_tbl.version = IF_TABLE_VERSION;
	pctx->if_tbl.num_entries = 0;
	pctx->if_tbl.max_entries = MAX_ENTRIES;

	/*
	 * Add QCCONFIG entry to the interface table
	 */
	entry_idx = 0;
	memcpy(pctx->if_tbl.if_table_entries[entry_idx].entry_name,
		QCCONFIG,
		strlen(QCCONFIG));

	ret = ipq_spl_get_fit_img_entry_point(pctx->fit,
			pctx->img_tbl->fit_node,
			&pctx->if_tbl.if_table_entries[entry_idx].address);
	if (ret) {
		pr_err("Failed to get qcconfig-meta entry point (ret=%d)\n",
			ret);
		return ret;
	}
	pctx->if_tbl.if_table_entries[entry_idx].attributes = 0;
	pctx->if_tbl.num_entries = entry_idx + 1;

	/*
	 * Add QCSDI entry to the interface table
	 */
	entry_idx++;
	memcpy(pctx->if_tbl.if_table_entries[entry_idx].entry_name,
		QCSDI,
		strlen(QCSDI));

	pctx->if_tbl.if_table_entries[entry_idx].address = 0;
	pctx->if_tbl.if_table_entries[entry_idx].attributes = 0;
	pctx->if_tbl.num_entries = entry_idx + 1;

	return 0;
}

/**
 * ipq_spl_qclib_fixup() - Perform fixups for qclib-meta image and jump to it.
 * @ctx:	Pointer to the global SPL context.
 *
 * This function gets the entry point for the qclib-meta image and
 * performs a direct jump to it with the populated QCLIB interface table.
 * Return: 0 on success, or a negative error code on failure.
 */
static int ipq_spl_qclib_fixup(void *ctx)
{
	int ret;
	struct ipq_spl_ctx *pctx = ctx;
	ipq_spl_jump_img_entry_t qclib_entry;
	u64 entry_point;

	if (!pctx) {
		pr_err("Invalid SPL context\n");
		return -EINVAL;
	}

	if (!pctx->img_tbl) {
		pr_err("Image table is NULL\n");
		return -EINVAL;
	}

	if (!pctx->fit) {
		pr_err("FIT image not loaded\n");
		return -EINVAL;
	}

	ret = ipq_spl_get_fit_img_entry_point(pctx->fit,
				pctx->img_tbl->fit_node,
				&entry_point);
	if (ret) {
		pr_err("Failed to get qclib-meta entry point (ret=%d)\n", ret);
		return ret;
	}

	qclib_entry = (ipq_spl_jump_img_entry_t)entry_point;

	pr_info("Jumping to Image: %s at 0x%llx\n", pctx->img_tbl->img_name,
		entry_point);
	qclib_entry(&pctx->if_tbl, NULL);

	return 0;
}

/**
 * ipq_spl_tfa_fixup() - Perform fixups for tfa_bl31-meta image.
 * @ctx:	Pointer to the global SPL context.
 *
 * This function gets the entry point for the ARM TF-A BL31 image and
 * populates shared memory (SMEM) with relevant boot information.
 * Return: 0 on success, or a negative error code on failure.
 */
static int ipq_spl_tfa_fixup(void *ctx)
{
	int ret;
	struct ipq_spl_ctx *pctx = ctx;

	if (!pctx) {
		pr_err("Invalid SPL context\n");
		return -EINVAL;
	}

	if (!pctx->img_tbl) {
		pr_err("Image table is NULL\n");
		return -EINVAL;
	}

	if (!pctx->fit) {
		pr_err("FIT image not loaded\n");
		return -EINVAL;
	}

	/*
	 * Populate SMEM in coldboot (Dload bit not set)
	 */
	if (!IPQ_SPL_IS_DLOAD_BIT_SET) {
		printf("Populating SMEM\n");
		ret = ipq_spl_populate_smem(pctx);
		if (ret) {
			pr_err("Failed to populate SMEM (ret=%d)\n", ret);
			return ret;
		}
	}

	return 0;
}

/**
 * ipq_spl_optee_fixup() - Perform fixups for optee-meta image.
 * @ctx:	Pointer to the global SPL context.
 *
 * This function gets the entry point for the OP-TEE (BL32) image.
 * Return: 0 on success, or a negative error code on failure.
 */
static int ipq_spl_optee_fixup(void *ctx)
{
	pr_debug("OP-TEE fixup skipped\n");
	return 0;
}

/**
 * ipq_spl_uboot_fixup() - Perform fixups for uboot-meta image.
 * @ctx:	Pointer to the global SPL context.
 *
 * This function gets the entry point for the U-Boot (BL33) image.
 * Return: 0 on success, or a negative error code on failure.
 */
static int ipq_spl_uboot_fixup(void *ctx)
{
	pr_debug("U-Boot fixup skipped\n");
	return 0;
}

/**
 * spl_get_load_buffer() - Allocate a cache-aligned buffer for image loading.
 * @offset:	Offset (unused, typically 0 for SPL).
 * @size:	Size of the buffer to allocate.
 *
 * This function is part of the U-Boot SPL framework to provide a buffer
 * for image loading, ensuring it's cache-aligned.
 * Return: Pointer to the allocated buffer, or NULL on failure.
 */
struct legacy_img_hdr *spl_get_load_buffer(ssize_t offset, size_t size)
{
	return (struct legacy_img_hdr *)malloc_cache_aligned(size);
}

#if defined(CONFIG_SPL_FIT_IMAGE_POST_PROCESS)
/**
 * board_fit_image_post_process() - Post-processing callback for FIT images.
 * @fit:	Pointer to the FIT image blob.
 * @node:	Node ID of the current image component within the FIT.
 * @p_image:	Pointer to the image data.
 * @p_size:	Pointer to the size of the image.
 *
 * This function is called by the SPL framework after an image component
 * within a FIT is loaded. It populates the global SPL context and
 * performs image-specific fixups.
 */
void board_fit_image_post_process(const void *fit, int node, void **p_image,
					size_t *p_size)
{
	int ret;
	u8 uc_index;
	u8 uc_size;
	const char *img_name = fit_get_name(fit, node, NULL);
	struct ipq_spl_ctx *ctx = U_BOOT_GET_IPQ_SPL_CTX(ipq_default_ctx);

	printf("Loading FIT Image: %s\n", img_name);

	if (!ctx) {
		pr_err("Unable to get SPL context\n");
		return;
	}

	/*
	 * Traverse through the SPL image table
	 */
	uc_size = ARRAY_SIZE(img_tbl_fit);
	for (uc_index = 0; uc_index < uc_size; uc_index++) {
		if (strcmp(img_name, img_tbl_fit[uc_index].img_name) == 0) {
			/*
			 * Populate the context
			 */
			ctx->img_tbl = &img_tbl_fit[uc_index];
			ctx->fit = (void *)fit;
			img_tbl_fit[uc_index].fit_node = node;

			/*
			 * Do the image fixups if available
			 */
			if (img_tbl_fit[uc_index].fixup) {
				ret = img_tbl_fit[uc_index].fixup(ctx);
				if (ret) {
					pr_err(
					"Failed to fixup %s image (ret=%d)\n",
					img_name, ret);
					ipq_spl_error_handler(NULL);
				}
			}
			break;
		}
	}
}
#endif /* CONFIG_SPL_FIT_IMAGE_POST_PROCESS */

/**
 * bl2_plat_get_bl31_params_v2() - Retrieve and fixup BL31 parameters.
 * @bl32_entry:	Entry point for BL32 (OP-TEE).
 * @bl33_entry:	Entry point for BL33 (U-Boot/kernel).
 * @fdt_addr:	Address of the Device Tree Blob (FDT).
 *
 * This function retrieves the default BL31 parameters and then performs
 * platform-specific fixups, such as populating ATF BL31's arg0 with
 * the address of the QCSDI interface table entry if available.
 *
 * Return: Pointer to the populated BL31 parameters structure.
 */
struct bl_params *bl2_plat_get_bl31_params_v2(uintptr_t bl32_entry,
					     uintptr_t bl33_entry,
					     uintptr_t fdt_addr)
{
	struct bl_params *bl_params;
	struct bl_params_node *node;
	struct interface_table_entry if_tbl_entry;
	int ret;
	struct ipq_spl_ctx *ctx = U_BOOT_GET_IPQ_SPL_CTX(ipq_default_ctx);

	/*
	 * Populate the bl31 params with default values.
	 */
	bl_params = bl2_plat_get_bl31_params_v2_default(bl32_entry,
							 bl33_entry,
							 fdt_addr);

	/*
	 * Fixup the bl31 params based on platform requirements.
	 */
	for_each_bl_params_node(bl_params, node) {
		if (node->image_id == ATF_BL31_IMAGE_ID) {
			if (!ctx) {
				pr_err("Unable to get SPL context\n");
				break;
			}

			/*
			 * Attempt to get the QCSDI entry from the global
			 * interface table.
			 */
			ret = ipq_spl_get_iftbl_entry_by_name(
						&ctx->if_tbl,
						QCSDI,
						&if_tbl_entry);
			if (ret) {
				/*
				 * Log the error but continue, as QCSDI might
				 * not be critical or could be handled later.
				 */
				pr_err("Unable to get QCSDI entry (ret=%d)\n",
					ret);
				break;
			}

			/*
			 * If found, populate arg0 with the QCSDI address.
			 */
			node->ep_info->args.arg0 = if_tbl_entry.address;
		}
	}

	return bl_params;
}

/**
 * ipq_spl_load_fit_image() - Load a FIT image from the boot device.
 * @ctx:	Pointer to the global SPL context.
 *
 * This function opens the configured FIT image partition, loads the FIT
 * image using the SPL framework, and then closes the partition.
 * Return: 0 on success, or a negative error code on failure.
 */
int ipq_spl_load_fit_image(void *ctx)
{
	int ret;
	int close_res;
	struct ipq_spl_ctx *pctx = ctx;
	struct spl_load_info *load;
	struct ipq_spl_fl_ops *fl_ops;

	if (!pctx) {
		pr_err("Invalid SPL context\n");
		return -EINVAL;
	}

	load = &pctx->fl_ctx.load;
	fl_ops = pctx->fl_ctx.ops;

	if (!fl_ops) {
		pr_err("Flash operations not set\n");
		ret = -EINVAL;
		goto end_func;
	}

	if (!fl_ops->open) {
		pr_err("Flash open operation is NULL\n");
		ret = -EINVAL;
		goto end_func;
	}
	ret = fl_ops->open(load, IPQ_SPL_FIT_IMG_PARTITION);
	if (ret) {
		pr_err("Failed to open FIT image partition %s (ret=%d)\n",
			IPQ_SPL_FIT_IMG_PARTITION, ret);
		goto end_func;
	}

	ret = spl_load(pctx->spl_image, pctx->bootdev, load, 0, 0);
	if (ret) {
		pr_err("Failed to load FIT img from partition %s (ret=0x%x)\n",
			IPQ_SPL_FIT_IMG_PARTITION, ret);
		goto close_media;
	}

close_media:
	if (!fl_ops->close) {
		pr_err("Flash close operation is NULL\n");
		if (!ret)
			/*
			 * If previous operations were successful,
			 * set this error
			 */
			ret = -EINVAL;
	} else {
		close_res = fl_ops->close(load);
		if (close_res && !ret)
			ret = close_res;
	}

end_func:
	return ret;
}

/**
 * ipq_spl_loader_pre_ddr() - SPL loader for pre-DDR stage.
 * @boot_device:Type of boot device.
 *
 * This function initializes the global SPL context, allocates necessary
 * structures, initializes flash, gets flash operations, and loads
 * images required before DDR initialization (currently only FIT).
 * Resources allocated are freed before returning.
 * Return: 0 on success, or a negative error code on failure.
 */
static int ipq_spl_loader_pre_ddr(u8 boot_device)
{
	int ret;
	u8 uc_size;
	struct ipq_spl_ctx *ctx = U_BOOT_GET_IPQ_SPL_CTX(ipq_default_ctx);

	if (!ctx) {
		pr_err("Unable to get SPL context\n");
		return -EINVAL;
	}

	memset(ctx, 0, sizeof(struct ipq_spl_ctx));

	ctx->spl_image = calloc(1, sizeof(struct spl_image_info));
	if (!ctx->spl_image) {
		pr_err("Failed to allocate spl_image\n");
		ret = -ENOMEM;
		goto fail_alloc_spl_image;
	}

	ctx->bootdev = calloc(1, sizeof(struct spl_boot_device));
	if (!ctx->bootdev) {
		pr_err("Failed to allocate bootdev\n");
		ret = -ENOMEM;
		goto fail_alloc_bootdev;
	}

	/*
	 * Populate context
	 */
	ctx->bootdev->boot_device = boot_device;
	ctx->fl_ctx.type = boot_device;

	ret = ipq_spl_flash_init(&ctx->fl_ctx);
	if (ret) {
		pr_err("Failed to initialize flash (ret=%d)\n", ret);
		goto fail_flash_init;
	}

	ret = ipq_spl_flash_get_ops(&ctx->fl_ctx);
	if (ret) {
		pr_err("Failed to get flash ops (ret=%d)\n", ret);
		goto fail_flash_init;
	}

	/*
	 * Load images from FIT image table
	 */
	uc_size = ARRAY_SIZE(img_tbl_fit);
	if (uc_size) {
		ctx->img_tbl = NULL;
		ret = ipq_spl_load_fit_image(ctx);
		if (ret) {
			pr_err("Failed to load FIT image (ret=%d)\n", ret);
			goto fail_load_fit;
		}
	}

	/*
	 * Success path
	 */
	ret = 0;

fail_load_fit:
fail_flash_init:
	if (ctx->bootdev)
		free(ctx->bootdev);
fail_alloc_bootdev:
	if (ctx->spl_image)
		free(ctx->spl_image);
fail_alloc_spl_image:
	return ret;
}

/**
 * ipq_spl_loader_post_ddr() - SPL loader for post-DDR stage.
 * @spl_image:	Pointer to the SPL image info structure.
 * @bootdev:	Pointer to the SPL boot device structure.
 *
 * This function is registered as a callback for the SPL framework once
 * DDR is initialized. It populates the global SPL context and loads
 * images required after DDR is ready (currently only FIT).
 * Return: 0 on success, or a negative error code on failure.
 */
static int ipq_spl_loader_post_ddr(struct spl_image_info *spl_image,
				   struct spl_boot_device *bootdev)
{
	int ret;
	u8 uc_size;
	struct ipq_spl_ctx *ctx = U_BOOT_GET_IPQ_SPL_CTX(ipq_default_ctx);

	if (!spl_image) {
		pr_err("Invalid SPL image info\n");
		return -EINVAL;
	}
	if (!bootdev) {
		pr_err("Invalid boot device info\n");
		return -EINVAL;
	}

	/*
	 * Populate context
	 */
	ctx->fl_ctx.type = bootdev->boot_device;
	ctx->spl_image = spl_image;
	ctx->bootdev = bootdev;

	ret = ipq_spl_flash_get_ops(&ctx->fl_ctx);
	if (ret) {
		pr_err("Failed to get flash ops (ret=%d)\n", ret);
		return ret;
	}

	/*
	 * Load images from FIT image table
	 */
	uc_size = ARRAY_SIZE(img_tbl_fit);
	if (uc_size) {
		ctx->img_tbl = NULL;
		ret = ipq_spl_load_fit_image(ctx);
		if (ret) {
			pr_err("Failed to load FIT image (ret=%d)\n", ret);
			return ret;
		}
	}

	return 0;
}

#if !defined(CONFIG_SPL_FRAMEWORK_BOARD_INIT_F)
/**
 * board_init_f() - Main entry point for SPL.
 * @dummy:	Dummy argument (unused).
 *
 * This is the primary function called by U-Boot's SPL framework. It performs
 * essential system setup, initializes memory management (if enabled),
 * initializes the console, and then initiates the pre-DDR boot stage.
 * On failure, it calls the error handler.
 */
void board_init_f(ulong dummy)
{
	int ret;

	/*
	 * Clear BSS
	 */
	memset(__bss_start, 0, __bss_end - __bss_start);

#if defined(CFG_EMUL_FREQUENCY_DIVIDER)
	ipq_spl_setup_arch_cntfreq();
#endif
	/*
	 * Referred from U-boot Proper code (target specific):
	 * board/qualcomm/<ipqxxxx/ipqxxxx.c
	 */
	ipq_spl_board_early_init_f();

#if CONFIG_IS_ENABLED(SYS_MALLOC_F)
	ipq_spl_malloc_init_f();
#endif

	ret = spl_early_init();
	if (ret) {
		pr_debug("spl_early_init() failed (ret=%d)\n", ret);
		goto fail;
	}

	preloader_console_init();

	ret = ipq_spl_loader_pre_ddr(spl_boot_device());
	if (ret) {
		pr_debug("ipq_spl_loader_pre_ddr() failed (ret=%d)\n", ret);
		goto fail;
	}

fail:
	if (ret)
		ipq_spl_error_handler(NULL);
}
#endif /* !CONFIG_SPL_FRAMEWORK_BOARD_INIT_F */

/**
 * spl_boot_device() - Determine the boot device.
 *
 * This function reads a hardware register to identify the current boot
 * device and maps it to the corresponding SMEM boot flash type.
 * Return: The mapped boot device type, or -EINVAL if the device is invalid.
 */
u32 spl_boot_device(void)
{
	/*
	 * Parse Bootdevice
	 */
	u8 boot_device_cfg =
	(readl(IPQ_SPL_BOOTCFG_REG_ADDR) & IPQ_SPL_BOOTCFG_DEV_MASK) >>
	IPQ_SPL_BOOTCFG_DEV_SHFT;
	u32 boot_device_smem;

	/*
	 * Map the boot device
	 */
	switch (boot_device_cfg) {
#if CONFIG_IPQ_SPI_NOR
	case IPQ_SPL_BOOTCFG_DEV_NOR_GPT:
		boot_device_smem = SMEM_BOOT_NORGPT_FLASH;
		printf("Selected boot device: SPI-NOR GPT\n");
		break;
#endif
#if CONFIG_IPQ_MMC
	case IPQ_SPL_BOOTCFG_DEV_MMC:
		boot_device_smem = SMEM_BOOT_MMC_FLASH;
		printf("Selected boot device: MMC\n");
		break;
#endif
#if CONFIG_IPQ_NAND
	case IPQ_SPL_BOOTCFG_DEV_SPI_NAND:
		boot_device_smem = SMEM_BOOT_QSPI_NAND_FLASH;
		printf("Selected boot device: SPI-NAND\n");
		break;
#endif
	default:
		pr_err("Invalid boot device configured: %d\n",
			boot_device_cfg);
		return -EINVAL;
	}

	return boot_device_smem;
}
