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
#include <linux/mtd/spi-nor.h>
#include <spl.h>
#include <spl_load.h>
#include <mach/ipq.h>
#include <spi_flash.h>
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
 * struct ipq_spl_fuse_info - Fuse information structure
 * @fuse_name:	Name of the fuse.
 * @fuse_addr:	Address of the fuse register.
 */
struct ipq_spl_fuse_info {
	char fuse_name[24];
	u32 fuse_addr;
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

/**
 * ipq_spl_jump_img_entry_t - Type definition for image entry point functions.
 * @arg1:	First argument passed to the entry point.
 * @arg2:	Second argument passed to the entry point.
 */
typedef void (*ipq_spl_jump_img_entry_t)(void *arg1, void *arg2);

/*
 * Forward declarations for image fixup functions
 */
static int ipq_spl_xcfg_fixup(void *ctx);
static int ipq_spl_qclib_fixup(void *ctx);
static int ipq_spl_tfa_fixup(void *ctx);
static int ipq_spl_optee_fixup(void *ctx);
static int ipq_spl_uboot_fixup(void *ctx);

/**
 * fuse_info_array - Array of fuse information.
 *
 * This array contains the names and addresses of various fuses used in the
 * system. These fuses are typically used for configuration.
 */
static struct ipq_spl_fuse_info fuse_info_array[] = {
	{"Boot Config", IPQ_SPL_FUSE_BOOT_CFG_ADDR},
	{"JTAG ID", IPQ_SPL_FUSE_JTAG_ID_ADDR},
	{"OEM ID", IPQ_SPL_FUSE_OEM_ID_ADDR},
	{"TME-L LCS", IPQ_SPL_FUSE_TME_L_LCS_ADDR},
	{"Serial Number", IPQ_SPL_FUSE_SERIAL_NUM_ADDR},
	{"Product Id", IPQ_SPL_FUSE_PRODUCT_ID_ADDR},
	{"Reset Debug", IPQ_SPL_GCC_RESET_DEBUG_ADDR},
	{"Reset Status", IPQ_SPL_GCC_RESET_STATUS_ADDR},
	{"FSM Status", IPQ_SPL_GCC_FSM_STATUS_ADDR},
	{"GPR0", IPQ_SPL_DDR_GPR0_ADDR},
};

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
 * ipq_spl_list_fuse() - List all fuses.
 * @fuse_arr:	Pointer to the fuse array.
 * @fuse_cnt:	Number of fuses.
 *
 * This function lists all fuses.
 * Return: 0 on success, or a negative error code on failure.
 */
int ipq_spl_list_fuse(struct ipq_spl_fuse_info *fuse_arr, size_t fuse_cnt)
{
	size_t index;

	if (!fuse_arr) {
		pr_err("Invalid fuse array pointer\n");
		return -EINVAL;
	}

	for (index = 0; index < fuse_cnt ; index++) {
		if (fuse_arr[index].fuse_addr == 0) {
			pr_warn("invalid fuse address at index %zu\n", index);
			continue;
		}

		printf("%-24s @ 0x%08X = 0x%08X\n",
			fuse_arr[index].fuse_name,
			fuse_arr[index].fuse_addr,
			readl((uintptr_t)fuse_arr[index].fuse_addr));
	}

	return 0;
}

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

	switch (spl_boot_device()) {
#if CONFIG_IPQ_SPI_NOR
	case BOOT_DEVICE_SPI:
		*fltype = SMEM_BOOT_NORGPT_FLASH;
		break;
#endif
#if CONFIG_IPQ_MMC
	case BOOT_DEVICE_MMC1:
		*fltype = SMEM_BOOT_MMC_FLASH;
		break;
#endif
#if CONFIG_IPQ_NAND
	case BOOT_DEVICE_NAND:
		*fltype = SMEM_BOOT_QSPI_NAND_FLASH;
		break;
#endif
	default:
		pr_err("Invalid boot device for SMEM: %d\n",
			spl_boot_device());
		return -EINVAL;
	}

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
	return (void *)(CONFIG_SPL_LOAD_FIT_ADDRESS);
}

/**
 * board_spl_fit_buffer_addr() - Get the address of the FIT image buffer.
 * @fit_size:	Size of the FIT image.
 * @sectors:	Number of sectors.
 * @bl_len:	Block length.
 *
 * This function returns the address where the FIT image will be loaded.
 * It uses the SPL load buffer address.
 * Return: Address of the FIT image buffer.
 */
void *board_spl_fit_buffer_addr(ulong fit_size, int sectors, int bl_len)
{
	void *buffer = spl_get_load_buffer(0, sectors * bl_len);

	if (!buffer) {
		pr_err("Failed to get FIT load buffer\n");
		ipq_spl_error_handler(NULL);
	}

	return buffer;
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
	struct spl_image_info *spl_image;
	struct spl_boot_device *boot_dev;
	int ret = -ENODEV;
	struct spl_image_loader *drv =
		ll_entry_start(struct spl_image_loader, spl_image_loader);
	const int n_ents =
		ll_entry_count(struct spl_image_loader, spl_image_loader);

	spl_image = calloc(1, sizeof(struct spl_image_info));
	if (!spl_image) {
		pr_err("Failed to allocate spl_image\n");
		ret = -ENOMEM;
		return ret;
	}

	boot_dev = calloc(1, sizeof(struct spl_boot_device));
	if (!boot_dev) {
		pr_err("Failed to allocate bootdev\n");
		free(spl_image);
		ret = -ENOMEM;
		return ret;
	}

	boot_dev->boot_device = boot_device;

	struct spl_image_loader *loader;
	int bootdev = boot_device;

	if (CONFIG_IS_ENABLED(SHOW_ERRORS))
		ret = -ENXIO;
	for (loader = drv; loader != drv + n_ents; loader++) {
		if (bootdev != loader->boot_device)
			continue;
		if (!CONFIG_IS_ENABLED(SILENT_CONSOLE)) {
			if (loader)
				printf("Trying to boot from %s\n",
					spl_loader_name(loader));
			else if (CONFIG_IS_ENABLED(SHOW_ERRORS)) {
				printf(PHASE_PROMPT
					"Unsupported Boot Device %d\n",
					bootdev);
			} else {
				puts(PHASE_PROMPT
					"Unsupported Boot Device!\n");
			}
		}
		if (loader) {
			ret = loader->load_image(spl_image, boot_dev);
			if (!ret) {
				spl_image->boot_device = bootdev;
				ret = 0;
				break;
			}
			printf("Error: %d\n", ret);
		}
	}

	free(spl_image);
	free(boot_dev);

	return ret;
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

	ipq_spl_list_fuse(fuse_info_array,
				ARRAY_SIZE(fuse_info_array));

	ret = ipq_spl_loader_pre_ddr(spl_boot_device());
	if (ret) {
		pr_debug("ipq_spl_loader_pre_ddr() failed (ret=%d)\n", ret);
		goto fail;
	}

	board_init_r(NULL, 0);

fail:
	if (ret)
		ipq_spl_error_handler(NULL);
}
#endif /* !CONFIG_SPL_FRAMEWORK_BOARD_INIT_F */


/**
 * spl_find_partition_info() - Find partition information by name
 * @uclass_id: Device class ID (UCLASS_MMC, UCLASS_SPI, etc.)
 * @device_num: Device number within the class
 * @part_name: Name of the partition to find
 * @info: Pointer to store partition information
 *
 * This function provides common partition lookup logic that can be shared
 * between different boot device types (MMC, SPI, etc.).
 * Return: Partition number on success, negative error code on failure
 */
static int spl_find_partition_info(enum uclass_id uclass_id, int device_num,
				   const char *part_name,
				   struct disk_partition *info)
{
	int ret;
	struct blk_desc *desc;

	if (!part_name || !info) {
		printf("Invalid parameters for partition lookup\n");
		return -EINVAL;
	}

	/*
	 *Get block device descriptor
	 */
	desc = blk_get_devnum_by_uclass_id(uclass_id, device_num);
	if (!desc) {
		printf("Block device not found for class %d, device %d\n",
				uclass_id, device_num);
		return -ENODEV;
	}

	/*
	 * Initialize partition table if needed
	 */
	if (desc->part_type == PART_TYPE_UNKNOWN) {
		printf("Initializing partition table\n");
		/*
		 * Prefer EFI/GPT to avoid memory-intensive part_init()
		 */
		desc->part_type = PART_TYPE_EFI;
	}

	/*
	 * Find partition by name
	 */
	ret = part_get_info_by_name(desc, part_name, info);
	if (ret < 0) {
		printf("Partition '%s' not found\n", part_name);
		return -ENOENT;
	}

	printf("Found partition '%s' at partition number %d\n", part_name, ret);
	return ret;
}

#if CONFIG_IPQ_MMC
/**
 * spl_mmc_boot_mode() - Determine the boot mode for MMC
 * @mmc:	Pointer to the MMC device
 * @boot_device:	Boot device ID
 *
 * This function determines the boot mode for MMC devices.
 * It returns MMCSD_MODE_RAW to indicate that raw partition access
 * should be used rather than filesystem access.
 * Return: MMCSD_MODE_RAW to use raw partition access
 */
u32 spl_mmc_boot_mode(struct mmc *mmc, const u32 boot_device)
{
	return MMCSD_MODE_RAW;
}

/**
 * spl_mmc_boot_partition() - Determine which partition to boot from
 * @boot_device:	Boot device ID
 *
 * This function determines which partition to boot from for MMC devices.
 * It attempts to find the partition specified by IPQ_SPL_FIT_IMG_PARTITION
 * using the common partition lookup function. If not found, it falls back
 * to the default partition defined by CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_PARTITION.
 * Return: Partition number to boot from, or default partition on error
 */
int spl_mmc_boot_partition(const u32 boot_device)
{
	int ret;
	struct disk_partition info;

	/*
	 * Use common partition lookup function
	 */
	ret = spl_find_partition_info(UCLASS_MMC, 0, IPQ_SPL_FIT_IMG_PARTITION, &info);
	if (ret < 0) {
		printf("Using default MMC partition %d\n",
				CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_PARTITION);
		return CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_PARTITION;
	}

	return ret;
}

/**
 * spl_mmc_get_uboot_raw_sector() - Find the raw sector offset
 * @mmc:	Pointer to the MMC device
 * @raw_sect:	Sector
 *
 * This function returns the offset of the image from the starting of the partition.
 *
 * Return: 0 if the image is at the starting of the partition without any offset.
 */
unsigned long spl_mmc_get_uboot_raw_sector(struct mmc *mmc,
					unsigned long raw_sect)
{
	return 0;
}
#endif /*CONFIG_IPQ_MMC*/

#if CONFIG_IPQ_SPI_NOR
/**
 * spl_spi_find_partition_offset() - Find the offset of a partition by name
 * @flash: Pointer to the SPI flash device
 * @part_name: Name of the partition to find
 * @offset: Pointer to store the found offset
 *
 * This function searches for a partition with the given name in the GPT
 * partition table of the SPI flash device and returns its offset.
 * Uses the common partition lookup function for consistency.
 *
 * Return: 0 on success, negative error code on failure
 */
static int spl_spi_find_partition_offset(struct spi_flash *flash,
					const char *part_name,
					unsigned int *offset)
{
	int ret;
	struct disk_partition info;

	if (!flash || !offset) {
		printf("Invalid SPI flash pointer or offset\n");
		return -EINVAL;
	}

	/*
	 * Use common partition lookup function
	 */
	ret = spl_find_partition_info(UCLASS_SPI, CONFIG_SF_DEFAULT_BUS,
				      part_name, &info);
	if (ret < 0)
		return ret;

	/*
	 * Calculate the offset in bytes
	 */
	*offset = info.start * info.blksz;
	printf("Found partition '%s' at offset 0x%x\n", part_name, *offset);

	return 0;
}

/**
 * spl_spi_get_uboot_offs() - Get the offset of U-Boot in SPI flash
 * @flash: Pointer to the SPI flash device
 *
 * This function finds the partition specified by IPQ_SPL_FIT_IMG_PARTITION
 * in the GPT partition table and returns its offset. If the partition is not found,
 * it falls back to the default offset defined by CONFIG_SYS_SPI_U_BOOT_OFFS.
 *
 * Return: The offset of U-Boot in SPI flash
 */
unsigned int spl_spi_get_uboot_offs(struct spi_flash *flash)
{
	unsigned int offset;
	const char *part_name = IPQ_SPL_FIT_IMG_PARTITION;
	int ret;

	/*
	 * Try to find the partition by name
	 */
	ret = spl_spi_find_partition_offset(flash, part_name, &offset);

	if (ret != 0) {
		/*
		 * Partition not found, hang
		 */
		hang();
	} else {
		/*
		 * Partition found, return its offset
		 */
		return offset;
	}
}

/**
 * spl_spi_boot_bus() - Get the SPI bus number to use
 *
 * This function returns the SPI bus number to use for SPI flash operations.
 * Return: The SPI bus number
 */
u32 spl_spi_boot_bus(void)
{
	/*
	 * Return the SPI bus number to use
	 */
	return CONFIG_SF_DEFAULT_BUS;
}

/**
 * spl_spi_boot_cs() - Get the SPI chip select to use
 *
 * This function returns the SPI chip select to use for SPI flash operations.
 * Return: The SPI chip select
 */
u32 spl_spi_boot_cs(void)
{
	/*
	 * Return the SPI chip select to use
	 */
	return CONFIG_SF_DEFAULT_CS;
}
#endif /*CONFIG_IPQ_SPI_NOR*/

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
		boot_device_smem = BOOT_DEVICE_SPI;
		printf("Selected boot device: SPI-NOR GPT\n");
		break;
#endif
#if CONFIG_IPQ_MMC
	case IPQ_SPL_BOOTCFG_DEV_MMC:
		boot_device_smem = BOOT_DEVICE_MMC1;
		printf("Selected boot device: MMC\n");
		break;
#endif
#if CONFIG_IPQ_NAND
	case IPQ_SPL_BOOTCFG_DEV_SPI_NAND:
		boot_device_smem = BOOT_DEVICE_NAND;
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
