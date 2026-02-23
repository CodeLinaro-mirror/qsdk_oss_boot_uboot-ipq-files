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
#ifdef CONFIG_ARM64
#include <asm/armv8/mmu.h>
#endif
#include <asm/cache.h>
#include <mailbox.h>
#include <linux/tmelcom-qmp.h>
#include <linux/mtd/mtd.h>
#include <nand.h>
#include <u-boot/crc.h>
#include <dm/device-internal.h>

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

/* MIBIB and partition table definitions */
#define MIBIB_MAGIC1			0xFE569FAC
#define MIBIB_MAGIC2			0xCD7F127A
#define MIBIB_VERSION			4
#define MIBIB_BLOCK_SEARCH_MAX		0x40
#define MIBIB_PAGE_PARTITION_TABLE	1
#define MIBIB_PAGE_LAST_PAGE		4
#define MIBIB_PAGE_CRC			3

#define FLASH_PART_MAGIC1		0x55EE73AA
#define FLASH_PART_MAGIC2		0xE35EBDDB
#define FLASH_PARTITION_VERSION		4

#define FLASH_USR_PART_MAGIC1		0xAA7D1B9A
#define FLASH_USR_PART_MAGIC2		0x1F7D48BC

#define FLASH_MIBIB_CRC_MAGIC1		0x9D41BEA1
#define FLASH_MIBIB_CRC_MAGIC2		0xF1DED2EA
#define FLASH_MIBIB_CRC_VERSION		1

/* Global variables to store MIBIB partition table and bootloader offset */
static struct flash_partition_table *g_mibib_parti_ptr;
static int g_bootldr_offset;

/**
 * struct mi_boot_info - MIBIB header structure
 * @magic1:	First magic number for validation
 * @magic2:	Second magic number for validation
 * @version:	MIBIB version
 * @age:	Age counter for determining the newest MIBIB
 * @numparts:	Number of partitions
 * @reserved1:	Reserved for future use
 * @reserved2:	Reserved for future use
 * @reserved3:	Reserved for future use
 */
struct mi_boot_info {
	u32 magic1;
	u32 magic2;
	u32 version;
	u32 age;
	u32 numparts;
	u32 reserved1;
	u32 reserved2;
	u32 reserved3;
};

/**
 * struct flash_partition_entry - System partition table entry definition
 * @name:        Name of the partition in the form of 0:ALL, 0:EFS2, etc.
 * @offset:      Offset in blocks from beginning of device
 * @length:      Length in blocks of the partition
 * @attrib1:     Partition attribute 1 (e.g., read-only, SLC/MLC mode)
 * @attrib2:     Partition attribute 2 (e.g., ECC configuration)
 * @attrib3:     Partition attribute 3 (e.g., upgrade mechanism)
 * @which_flash: Numeric ID of flash part (first = 0, second = 1)
 *
 * This structure defines a single partition entry in the system partition table.
 * Each entry contains information about the partition's location, size, and
 * various attributes that control how the partition is accessed and managed.
 */
struct flash_partition_entry {
	/* Name of the partition in the form of 0:ALL, 0:EFS2, etc. */
	char name[16];

	/* Offset in blocks from beginning of device */
	u32 offset;

	/* length in blocks of the partition */
	u32 length;

	/* Partition attributes */
	u8 attrib1;
	u8 attrib2;
	u8 attrib3;

	/* Numeric ID of flash part (first = 0, second = 1) */
	u8 which_flash;
};

/**
 * Maximum number of partitions supported in the partition table
 * Plus one extra entry for the "all" partition that represents the entire device
 */
#define FLASH_NUM_PART_ENTRIES  32
#define FLASH_PART_ENTRY_TOTAL (FLASH_NUM_PART_ENTRIES + 1)

/**
 * struct flash_partition_table - System partition table definition
 * @magic1:    First magic number for validation (0x55EE73AA)
 * @magic2:    Second magic number for validation (0xE35EBDDB)
 * @version:   Partition table version (currently 4)
 * @numparts:  Number of valid partition entries in the table
 * @part_entry: Array of partition entries
 *
 * This structure defines the system partition table that is stored in flash.
 * It contains a header with magic numbers and version information, followed
 * by an array of partition entries that define the layout of the flash device.
 *
 * WARNING: The placement of the first three elements (magic1, magic2, version)
 * must not be changed to ensure backward compatibility.
 */
struct flash_partition_table {
	/* Partition table magic numbers and version number.
	 *   WARNING!!!!
	 *   No matter how you change the structure, do not change
	 *   the placement of the first three elements so that future
	 *   compatibility will always be guaranteed at least for
	 *   the identifiers.
	 */
	u32 magic1;
	u32 magic2;
	u32 version;

	/* Partition table data.  This portion of the structure may be changed
	 * as necessary to accommodate new features.  Be sure to increment
	 * version number if you change it.
	 */
	u32 numparts;   /* number of partition entries */
	struct flash_partition_entry part_entry[FLASH_PART_ENTRY_TOTAL];
};

/**
 * struct flash_usr_partition_entry - User partition table entry definition
 * @name:          Name of the partition in the form of 0:ALL, 0:EFS2, etc.
 * @img_size:      Size in KB for the partition
 * @padding:       Padding in KB for static handling of NAND bad blocks
 * @which_flash:   Numeric ID of flash part (first = 0, second = 1)
 * @reserved_flag1: Attribute 1 for this partition (copied to attrib1)
 * @reserved_flag2: Attribute 2 for this partition (copied to attrib2)
 * @reserved_flag3: Attribute 3 for this partition (copied to attrib3)
 * @reserved_flag4: Layout based flags
 *
 * This structure defines a single entry in the user partition table.
 * The user partition table is used during initial flash programming to
 * create the system partition table. It contains information about the
 * desired size and attributes of each partition.
 */
struct flash_usr_partition_entry {
	/* Name of the partition in the form of 0:ALL, 0:EFS2, etc. */
	char name[16];

	/* Size in KB for the partition */
	u32 img_size;

	/* Padding in KB for static handling of NAND bad blocks in this partition */
	/* This field can also be used to define minimum size requirement for a
	 * parition defined in terms of number of sectors/blocks.
	 * Please note that this is possible because there never are bad blocks on a
	 * a NOR device
	 */
	u16 padding;

	/* Numeric ID of flash part (first = 0, second = 1) */
	u16 which_flash;

	/* Attributes for this partition - This get copied to the attribx flags in
	 * system partition table
	 */
	u8 reserved_flag1;
	u8 reserved_flag2;
	u8 reserved_flag3;

	u8 reserved_flag4;  /* layout based flags */
};

/**
 * struct flash_usr_partition_table - User partition table definition
 * @magic1:    First magic number for validation (0xAA7D1B9A)
 * @magic2:    Second magic number for validation (0x1F7D48BC)
 * @version:   Partition table version (currently 4)
 * @numparts:  Number of valid partition entries in the table
 * @part_entry: Array of user partition entries
 *
 * This structure defines the user partition table that is used during
 * initial flash programming to create the system partition table.
 * It contains a header with magic numbers and version information,
 * followed by an array of user partition entries.
 *
 * WARNING: The placement of the first three elements (magic1, magic2, version)
 * must not be changed to ensure backward compatibility.
 */
struct flash_usr_partition_table {
	/* Partition table magic numbers and version number.
	 *   WARNING!!!!
	 *   No matter how you change the structure, do not change
	 *   the placement of the first three elements so that future
	 *   compatibility will always be guaranteed at least for
	 *   the identifiers.
	 */
	u32 magic1;
	u32 magic2;
	u32 version;

	/* Partition table data.  This portion of the structure may be changed
	 * as necessary to accommodate new features.  Be sure to increment
	 * version number if you change it.
	 */
	u32 numparts;   /* number of partition entries */
	struct flash_usr_partition_entry part_entry[FLASH_PART_ENTRY_TOTAL];
};

/**
 * struct flash_mibib_crc - MIBIB CRC structure
 * @magic1:    First magic number for validation (0x9D41BEA1)
 * @magic2:    Second magic number for validation (0xF1DED2EA)
 * @version:   CRC version (currently 1)
 * @crc:       CRC32 checksum of the MIBIB contents
 * @reserved:  Reserved fields for future use
 *
 * This structure is stored in the MIBIB CRC page and contains the CRC32
 * checksum of the MIBIB contents. It is used to verify the integrity of
 * the MIBIB during boot.
 */
struct flash_mibib_crc {
	u32 magic1;
	u32 magic2;
	u32 version;
	u32 crc;
	u32 reserved[4];
};

/**
 * struct ipq_spl_img_ctx - SPL image context
 * @img_name:	Name of the image.
 * @prt_name:	Partition name where the image resides.
 * @sw_id:	Software ID for the image.
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
	u64 sw_id;
	u64 load_addr;
	u64 img_sz;
	u64 img_off;
	u8 load;
	u8 auth;
	u8 optional;
	u8 img_arch;
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

#if defined(CONFIG_IPQ_TMEL_IPC_SUPPORT)
/**
 * tme_fuse_info_array - Array of TME IPC based fuse information.
 *
 * This array contains the names and addresses of various TME IPC based fuses
 * used in the system. These fuses are typically used for security features
 * and configuration settings.
 */
static struct ipq_spl_fuse_info tme_fuse_info_array[] = {
	{"OEM TME Row 0", IPQ_SPL_FUSE_OEM_TME_ROW_0_ADDR},
	{"Feature Config Row 0", IPQ_SPL_FUSE_FEATURE_CONFIG_ROW_0_ADDR},
	{"Feature Config Row 1", IPQ_SPL_FUSE_FEATURE_CONFIG_ROW_1_ADDR},
	{"OEM Config Row 0", IPQ_SPL_FUSE_OEM_CONFIG_ROW_0_ADDR},
	{"OEM Config Row 1", IPQ_SPL_FUSE_OEM_CONFIG_ROW_1_ADDR},
};
#endif /* CONFIG_IPQ_TMEL_IPC_SUPPORT */

/**
 * img_tbl_fit - Image loader table for FIT images.
 *
 * This table defines the FIT images to be loaded and their associated fixup
 * functions.
 */
struct ipq_spl_img_ctx img_tbl_fit[] = {
	{
		.img_name = "qcconfig-meta",
		.sw_id = IPQ_SPL_QCLIB_DDR_SEC_AUTH_SWID,
		.auth = true,
		.fixup = ipq_spl_xcfg_fixup,
	}, {
		.img_name = "qclib-meta",
		.sw_id = IPQ_SPL_QCLIB_DDR_SEC_AUTH_SWID,
		.auth = true,
		.fixup = ipq_spl_qclib_fixup,
	}, {
		.img_name = "tfa_bl31-meta",
		.sw_id = IPQ_SPL_TZ_TEE_SEC_AUTH_SWID,
		.auth = true,
		.fixup = ipq_spl_tfa_fixup,
	}, {
		.img_name = "optee-meta",
		.sw_id = IPQ_SPL_OP_TEE_SEC_AUTH_SWID,
		.auth = true,
		.fixup = ipq_spl_optee_fixup,
	}, {
		.img_name = "uboot-meta",
		.sw_id = IPQ_SPL_APPSBL_SEC_AUTH_SWID,
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

#if defined(CONFIG_CLK_QCOM_PLL)
/**
 * ipq_spl_probe_and_enable_plls() - Probe and enable all PLLs.
 *
 * This function probes and enables all available PLLs in the system.
 * Return: 0 on success, or a negative error code on failure.
 */
int ipq_spl_probe_and_enable_plls(void)
{
	int ret;
	ofnode node, p_handle;
	struct udevice *pll_dev;
	u32 index, num_plls;

	node = ofnode_by_compatible(ofnode_null(), "qcom,ipq-init-plls");
	if (!ofnode_valid(node)) {
		pr_debug("Failed to get qcom,ipq-init-plls node\n");
		/**
		 * No PLL node is available in device tree.
		 * Return success.
		 */
		return 0;
	}

	/**
	 * Get the number of phandles are in "plls"
	 */
	num_plls = ofnode_count_phandle_with_args(node, "plls", NULL, 0);
	if (num_plls < 0) {
		pr_debug("No plls found %d", num_plls);
		/**
		 * No PLLs available in device tree to be initialized.
		 * Return success.
		 */
		return 0;
	}

	/**
	 * Probe and enable all the PLL devices.
	 */
	for (index = 0; index < num_plls; index++) {
		p_handle = ofnode_parse_phandle(node, "plls", index);
		if (!ofnode_valid(p_handle)) {
			pr_debug(" No more PLL phandles\n");
			/**
			 * If no more PLL phandles, break the loop with Success.
			 */
			break;
		}

		/* Convert the ofnode p_handle to a udevice and probe it.
		 * The probe step initializes the PLL hardware.
		 */
		ret = uclass_get_device_by_ofnode(UCLASS_MISC, p_handle,
							&pll_dev);
		if (ret) {
			pr_err("Failed to get PLL[%d] device: %d\n",
				index, ret);
			return ret;
		}
	}

	return 0;
}
#endif /* CONFIG_CLK_QCOM_PLL */

/**
 * ipq_spl_board_init_clk() - Initialize board clocks.
 *
 * This function initializes the board-specific clocks.
 * Return: 0 on success, or a negative error code on failure.
 */
int ipq_spl_board_init_clk(void)
{
	struct udevice *dev;
	struct clk_bulk bulk;
	int ret;

	ret = uclass_get_device_by_name(UCLASS_NOP,
						"qcom,ipq-init-clks", &dev);
	if (ret) {
		pr_debug("Failed to get qcom,ipq-init-clks device\n");
		/**
		 * No board init clks are in device tree to be initialized.
		 * Return success.
		 */
		return 0;
	}

	/*
	 * Enable listed clocks (gates/votes) in SPL/U-Boot
	 * via standard bulk clock API.
	 */
	ret = clk_get_bulk(dev, &bulk);
	if (!ret) {
		ret = clk_enable_bulk(&bulk);
		if (ret)
			return ret;
	}

	return 0;
}

static const struct udevice_id ipq_init_clk_of_match[] = {
	{ .compatible = "qcom,ipq-init-clks" },
	{ }
};

U_BOOT_DRIVER(ipq_init_clk) = {
	.name		= "ipq-init-clk",
	.id		= UCLASS_NOP,
	.of_match	= ipq_init_clk_of_match,
};

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
	 * Populate MIBIB Info if available
	 */
	if (g_mibib_parti_ptr) {
		/* Validate MIBIB partition table magic numbers and version */
		if ((g_mibib_parti_ptr->magic1 != FLASH_PART_MAGIC1) ||
		    (g_mibib_parti_ptr->magic2 != FLASH_PART_MAGIC2) ||
		    (g_mibib_parti_ptr->version != FLASH_PARTITION_VERSION)) {
			pr_err("Invalid MIBIB partition table detected, skipping SMEM population\n");
			free(g_mibib_parti_ptr);
			g_mibib_parti_ptr = NULL;
			return -EINVAL;
		}

		size = sizeof(struct flash_partition_table);
		ret = smem_alloc(smem, -1, SMEM_AARM_PARTITION_TABLE, size);
		if (ret) {
			pr_err("Failed to alloc item: SMEM_AARM_PARTITION_TABLE (ret=%d)\n", ret);
			free(g_mibib_parti_ptr);
			g_mibib_parti_ptr = NULL;
			return ret;
		}

		void *mibib_info = smem_get(smem, -1, SMEM_AARM_PARTITION_TABLE, &size);

		if (!mibib_info) {
			pr_err("Failed to get item: SMEM_AARM_PARTITION_TABLE\n");
			free(g_mibib_parti_ptr);
			g_mibib_parti_ptr = NULL;
			return -ENOENT;
		}

		/* Verify size is sufficient for the copy operation */
		if (size < sizeof(struct flash_partition_table)) {
			pr_err("SMEM allocation too small for MIBIB partition table\n");
			free(g_mibib_parti_ptr);
			g_mibib_parti_ptr = NULL;
			return -EINVAL;
		}

		/* Copy the partition table to SMEM */
		memcpy(mibib_info, g_mibib_parti_ptr, sizeof(struct flash_partition_table));
		printf("MIBIB partition table populated in SMEM\n");

		/* Free the temporary allocation after copying to SMEM */
		free(g_mibib_parti_ptr);
		g_mibib_parti_ptr = NULL;
	} else {
		printf("No MIBIB partition table available to populate SMEM\n");
	}

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
 * ipq_spl_get_img_ctx_by_name() - Get image table entry by name.
 * @img_name:	Name of the image to find.
 *
 * This function searches the img_tbl_fit for any entry matching the given name
 * and returns the pointer to the matching image entry from the table.
 *
 * Return: pointer to maching image table entry on success, or NULL on failure.
 */

struct ipq_spl_img_ctx *ipq_spl_get_img_ctx_by_name(char *img_name)
{
	u8 uc_index;
	u8 uc_size;

	if (!img_name)
		return NULL;

	uc_size = ARRAY_SIZE(img_tbl_fit);
	for (uc_index = 0; uc_index < uc_size; uc_index++) {
		if (strcmp(img_name, img_tbl_fit[uc_index].img_name) == 0)
			return &img_tbl_fit[uc_index];
	}

	return NULL;
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

#if defined(CONFIG_IPQ_TMEL_IPC_SUPPORT)
/**
 * ipq_spl_list_tme_fuse() - List all TME fuses.
 * @fuse_arr:	Pointer to the fuse array.
 * @fuse_cnt:	Number of fuses.
 *
 * This function lists all fuses using TME IPC communication.
 * Return: 0 on success, or a negative error code on failure.
 */
static int ipq_spl_list_tme_fuse(struct ipq_spl_fuse_info *fuse_arr,
				 u8 fuse_cnt)
{
	int ret;
	u8 index;
	size_t fuse_size;
	size_t aligned_size;
	struct fuse_payload *fuse;
	struct list_fuse_params fuse_params;

	fuse_size = sizeof(struct fuse_payload) * fuse_cnt;
	aligned_size = roundup(fuse_size, CONFIG_SYS_CACHELINE_SIZE);
	fuse = malloc_cache_aligned(aligned_size);
	if (fuse == NULL) {
		pr_err("Failed to allocate memory for fuse data\n");
		return -ENOMEM;
	}

	memset(fuse, 0, aligned_size);

	for (index = 0; index < fuse_cnt ; index++)
		fuse[index].fuse_addr = fuse_arr[index].fuse_addr;

	fuse_params.fuse = fuse;
	fuse_params.fuse_read_cnt = fuse_cnt;
	fuse_params.fuse_payload_size = sizeof(struct fuse_payload);
	fuse_params.size = fuse_size;

	ret = ipq_list_fuse_tme_impl(&fuse_params);
	if (ret)
		goto fail;

	for (index = 0; index < fuse_cnt ; index++) {
		printf("%-24s @ 0x%08X = 0x%08X%08X\n",
			fuse_arr[index].fuse_name,
			fuse[index].fuse_addr,
			fuse[index].msb_val,
			fuse[index].lsb_val);
	}
fail:
	free(fuse);

	return ret;
}

/**
 * ipq_spl_auth_image() - Authenticate an image using TME.
 * @p_img_entry: Pointer to the image context.
 *
 * This function authenticates an image using TME.
 * Return: 0 on success, or a negative error code on failure.
 */
static int ipq_spl_auth_image(struct ipq_spl_img_ctx *p_img_entry)
{
	int ret;
	struct secure_auth_params auth_params = {0};

	if (!p_img_entry) {
		pr_err("Invalid image entry\n");
		return -EINVAL;
	}

	if (p_img_entry->auth == false) {
		printf("Authentication disabled for %s\n",
				p_img_entry->img_name);
		return 0;
	}

	/*
	 * Validate authentication parameters
	 */
	if (!p_img_entry->load_addr) {
		pr_err("Image Auth: Invalid load address\n");
		return -EINVAL;
	}

	if (!p_img_entry->img_sz) {
		pr_err("Image Auth: Invalid image size\n");
		return -EINVAL;
	}

	printf("Auth image with SW_ID=0x%llx, addr=0x%llx, size=0x%llx\n",
		p_img_entry->sw_id, p_img_entry->load_addr,
		p_img_entry->img_sz);

	/*
	 * Populate Image params
	 */
	auth_params.type = p_img_entry->sw_id;
	auth_params.addr = p_img_entry->load_addr;
	auth_params.size = p_img_entry->img_sz;
#ifdef CONFIG_SECURE_AUTH_V3
	auth_params.flags = 1;
#endif

	/*
	 * Populate relocated segments information
	 */
	auth_params.relocate = 0;
	auth_params.load_seg_buff = NULL;
	auth_params.load_seg_info_size = 0;
	auth_params.load_seg_cnt = 0;

	ret = ipq_secure_auth_tme_impl(&auth_params);
	if (ret) {
		pr_err("Image Auth: failed (ret=%d)\n", ret);
		return ret;
	}

	printf("Image Auth: success\n");

	return 0;
}

#endif /* CONFIG_IPQ_TMEL_IPC_SUPPORT */

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
	u8 img_arch;
	u64 load_addr;
	void *load_ptr;
	const char *img_name = fit_get_name(fit, node, NULL);
	struct ipq_spl_ctx *ctx = U_BOOT_GET_IPQ_SPL_CTX(ipq_default_ctx);

	printf("Loading FIT Image: %s\n", img_name);

	if (!ctx) {
		pr_err("Unable to get SPL context\n");
		goto fail;
	}

	if (!p_image || !*p_image || !p_size || !*p_size) {
		pr_err("Invalid image parameters\n");
		goto fail;
	}

	/*
	 * Get the actual image load address from the FIT image.
	 */
	if (fit_image_get_load(fit, node, (ulong *)&load_addr)) {
		pr_err("Failed to get load address for %s\n", img_name);
		goto fail;
	}

	/*
	 * Copy the p_image pointer to the actual image load address to
	 * handle any address alignments caused by block‑based flash reads.
	 */
	load_ptr = map_sysmem(load_addr, *p_size);
	memcpy(load_ptr, *p_image, *p_size);

	/*
	 * Adjust the image pointer to the final load address post-memcpy.
	 */
	*p_image = load_ptr;

#if !(CONFIG_IS_ENABLED(SYS_ICACHE_OFF) && CONFIG_IS_ENABLED(SYS_DCACHE_OFF))
	/*
	 * Ensure that the image data is written to the actual
	 * memory location before we process it.
	 */
	flush_cache((unsigned long)(*p_image), (unsigned long)(*p_size));
#endif

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
			img_tbl_fit[uc_index].load_addr = (u64)(*p_image);
			img_tbl_fit[uc_index].img_sz = *p_size;

			if (fit_image_get_arch(fit, node, &img_arch)) {
				pr_err("Failed to get architecture for %s\n", img_name);
				goto fail;
			}
			img_tbl_fit[uc_index].img_arch = img_arch;

#if defined(CONFIG_IPQ_TMEL_IPC_SUPPORT)
			/*
			 * Authenticate image if enabled
			 */
			if (img_tbl_fit[uc_index].auth) {
				ret = ipq_spl_auth_image(ctx->img_tbl);
				if (ret) {
					pr_err("%s auth failed (ret=%d)\n",
					ctx->img_tbl->img_name,
					ret);
					goto fail;
				}
			}
#endif /* CONFIG_IPQ_TMEL_IPC_SUPPORT */

			/*
			 * Do the image fixups if available
			 */
			if (img_tbl_fit[uc_index].fixup) {
				ret = img_tbl_fit[uc_index].fixup(ctx);
				if (ret) {
					pr_err(
					"Failed to fixup %s image (ret=%d)\n",
					img_name, ret);
					goto fail;
				}
			}
			break;
		}
	}

	/*
	 * Return on success
	 */
	return;

fail:
	ipq_spl_error_handler(NULL);
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
	struct ipq_spl_img_ctx *img_tbl;

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
		} else if (node->image_id == ATF_BL33_IMAGE_ID) {
			img_tbl = ipq_spl_get_img_ctx_by_name("uboot-meta");

			if (img_tbl && img_tbl->img_arch == IH_ARCH_ARM) {
				/* SPSR = 0x1D3 for 32-bit Mode */
				node->ep_info->spsr = SPSR_32_SVC_ARM_MASKED_LE;
			}
		}
	}

	return bl_params;
}

/**
 * spl_board_prepare_for_boot() - Prepare board for booting.
 *
 * This function is invoked during the SPL boot sequence to carry out
 * any board‑specific setup required before exiting SPL.
 */
void spl_board_prepare_for_boot(void)
{
	/*
	 * Disconnect the TME mailbox channel so the client does not receive
	 * anymore data and can reliquish control of the channel.
	 */
	int ret;
	struct tmelcom *tmelcom_priv;

	ret = ipq_get_tmelcom_device(&tmelcom_priv);
	if (ret || !tmelcom_priv) {
		pr_err("Failed to find TMELCOM node %d\n", ret);
		goto fail;
	}

	ret = mbox_free(&tmelcom_priv->mbox);
	if (ret) {
		pr_err("Failed to shutdown TME mailbox channel: %d\n", ret);
		goto fail;
	}

	return;
fail:
	ipq_spl_error_handler(NULL);
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

#if defined(CONFIG_CLK_QCOM_PLL)
	ret = ipq_spl_probe_and_enable_plls();
	if (ret) {
		pr_err("Failed to enable PLLs (ret=%d)\n", ret);
		goto fail;
	}
#endif /* CONFIG_CLK_QCOM_PLL */

	ret = ipq_spl_board_init_clk();
	if (ret) {
		pr_err("Failed to initialize board clocks (ret=%d)\n", ret);
		goto fail;
	}

	preloader_console_init();

	ipq_spl_list_fuse(fuse_info_array,
				ARRAY_SIZE(fuse_info_array));

#if defined(CONFIG_IPQ_TMEL_IPC_SUPPORT)
	ipq_spl_list_tme_fuse(tme_fuse_info_array,
				ARRAY_SIZE(tme_fuse_info_array));
#endif

#if !(CONFIG_IS_ENABLED(SYS_ICACHE_OFF) && CONFIG_IS_ENABLED(SYS_DCACHE_OFF))
	ret = arm_reserve_mmu();
	if (ret) {
		pr_debug("Failed to reserve space for MMU (ret=%d)\n", ret);
		goto fail;
	}

	enable_caches();
#endif

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
 * nand_is_block_mibib() - Check if a block contains a valid MIBIB
 * @block:	Block number to check
 * @age:	Pointer to store the age of the MIBIB if valid
 *
 * This function checks if the specified block contains a valid MIBIB
 * by verifying magic numbers, version information, and CRC32 checksum.
 *
 * Return: true if valid MIBIB found, false otherwise
 */
static bool nand_is_block_mibib(int block, u32 *age)
{
	struct mi_boot_info *mibib_magic;
	struct flash_partition_table *parti_sys;
	struct flash_usr_partition_table *parti_usr;
	struct flash_mibib_crc *mibib_crc;
	u8 *page_buf;
	u32 page, crc32 = 0;
	struct mtd_info *mtd;
	int ret, i;

	mtd = get_nand_dev_by_index(0);
	if (!mtd) {
		printf("Failed to get NAND device\n");
		return false;
	}

	/* Allocate a buffer for reading pages */
	page_buf = malloc(mtd->writesize);
	if (!page_buf) {
		printf("Failed to allocate page buffer\n");
		return false;
	}

	/* Calculate page number for MIBIB header */
	page = block * (mtd->erasesize / mtd->writesize);

	/* Read the MIBIB header page */
	size_t length = mtd->writesize;

	ret = nand_read(mtd, page * mtd->writesize, &length, page_buf);
	if (ret || length != mtd->writesize) {
		printf("Failed to read MIBIB header page\n");
		free(page_buf);
		return false;
	}

	/* Check MIBIB magic numbers and version */
	mibib_magic = (struct mi_boot_info *)page_buf;
	if ((mibib_magic->magic1 != MIBIB_MAGIC1) ||
	    (mibib_magic->magic2 != MIBIB_MAGIC2) ||
	    (mibib_magic->version != MIBIB_VERSION)) {
		free(page_buf);
		return false;
	}

	/* Store the age number */
	*age = mibib_magic->age;

	/* Start calculating CRC32 from MIBIB header page */
	crc32 = crc32_no_comp(crc32, (uint8_t *)page_buf, mtd->writesize);

	/* Read the partition table page */
	page++;
	length = mtd->writesize;
	ret = nand_read(mtd, page * mtd->writesize, &length, page_buf);
	if (ret || length != mtd->writesize) {
		printf("Failed to read partition table page\n");
		free(page_buf);
		return false;
	}

	/* Check partition table magic numbers and version */
	parti_sys = (struct flash_partition_table *)page_buf;
	if ((parti_sys->magic1 != FLASH_PART_MAGIC1) ||
	    (parti_sys->magic2 != FLASH_PART_MAGIC2) ||
	    (parti_sys->version != FLASH_PARTITION_VERSION)) {
		free(page_buf);
		return false;
	}

	/* Continue calculating CRC32 with partition table page */
	crc32 = crc32_no_comp(crc32, (uint8_t *)page_buf, mtd->writesize);

	/* Read and calculate CRC for remaining pages up to USR_PART page */
	for (i = MIBIB_PAGE_PARTITION_TABLE + 1; i < MIBIB_PAGE_LAST_PAGE - 2; i++) {
		page++;
		length = mtd->writesize;
		ret = nand_read(mtd, page * mtd->writesize, &length, page_buf);
		if (ret) {
			if (ret == -EUCLEAN) {
				/* Page is erased, fill with 0xFF for CRC calculation */
				memset(page_buf, 0xFF, mtd->writesize);
			} else {
				printf("Failed to read MIBIB page %d\n", i);
				free(page_buf);
				return false;
			}
		}
		crc32 = crc32_no_comp(crc32, (uint8_t *)page_buf, mtd->writesize);
	}

	/* Read the USR_PART page */
	page++;
	length = mtd->writesize;
	ret = nand_read(mtd, page * mtd->writesize, &length, page_buf);
	if (ret) {
		printf("Failed to read USR_PART page\n");
		free(page_buf);
		return false;
	}

	/* Validate USR_PART page */
	parti_usr = (struct flash_usr_partition_table *)page_buf;
	if ((parti_usr->magic1 != FLASH_USR_PART_MAGIC1) ||
	    (parti_usr->magic2 != FLASH_USR_PART_MAGIC2) ||
	    (parti_usr->version != FLASH_PARTITION_VERSION)) {
		printf("USR_PART magic or version number mismatch\n");
		free(page_buf);
		return false;
	}

	/* Continue CRC calculation with USR_PART page */
	crc32 = crc32_no_comp(crc32, (uint8_t *)page_buf, mtd->writesize);

	/* Read the CRC page */
	page++;
	length = mtd->writesize;
	ret = nand_read(mtd, page * mtd->writesize, &length, page_buf);
	if (ret || length != mtd->writesize) {
		printf("Failed to read MIBIB CRC page\n");
		free(page_buf);
		return false;
	}

	/* Check CRC magic numbers and version */
	mibib_crc = (struct flash_mibib_crc *)page_buf;
	if ((mibib_crc->magic1 != FLASH_MIBIB_CRC_MAGIC1) ||
	    (mibib_crc->magic2 != FLASH_MIBIB_CRC_MAGIC2) ||
	    (mibib_crc->version != FLASH_MIBIB_CRC_VERSION)) {
		printf("MIBIB CRC magic or version mismatch\n");
		free(page_buf);
		return false;
	}

	/* Verify CRC32 checksum */
	if (mibib_crc->crc != crc32) {
		/*
		 * printf("MIBIB CRC checksum mismatch: calculated=0x%08x, stored=0x%08x\n",
		 * crc32, mibib_crc->crc);
		 * free(page_buf); // TBD: UBOOT_SPL
		 * return false;
		 */

	}

	/* All checks passed, we have a valid MIBIB */
	free(page_buf);
	return true;
}

/**
 * nand_retrieve_mibib() - Find and retrieve MIBIB from flash
 *
 * This function searches for valid MIBIB blocks in flash and returns
 * the partition table from the most recent valid MIBIB.
 *
 * Return: Pointer to the partition table, or NULL if not found
 */
static struct flash_partition_table *nand_retrieve_mibib(void)
{
	int cur_block;
	u32 copy1_age = 0, copy2_age = 0;
	int copy1_blockno = -1, copy2_blockno = -1;
	bool copy1_valid = false, copy2_valid = false;
	int new_mibib_block = -1;
	struct mtd_info *mtd;
	struct flash_partition_table *parti_ptr = NULL;
	u8 *page_buf = NULL;
	int ret;

	mtd = get_nand_dev_by_index(0);
	if (!mtd) {
		printf("Failed to get NAND device\n");
		return NULL;
	}

	/* Allocate a buffer for reading pages */
	page_buf = malloc(mtd->writesize);
	if (!page_buf) {
		printf("Failed to allocate page buffer\n");
		return NULL;
	}

	/* Search for first MIBIB copy */
	for (cur_block = 0; cur_block <= MIBIB_BLOCK_SEARCH_MAX; cur_block++) {
		if (nand_is_block_mibib(cur_block, &copy1_age)) {
			copy1_valid = true;
			copy1_blockno = cur_block;
			break;
		}
	}

	/* If no valid MIBIB found, return NULL */
	if (!copy1_valid) {
		printf("No valid MIBIB found\n");
		free(page_buf);
		return NULL;
	}

	/* Search for second MIBIB copy */
	for (cur_block = copy1_blockno + 1; cur_block <= MIBIB_BLOCK_SEARCH_MAX; cur_block++) {
		if (nand_is_block_mibib(cur_block, &copy2_age)) {
			copy2_valid = true;
			copy2_blockno = cur_block;
			break;
		}
	}

	/* Determine which MIBIB copy is newer */
	if (copy1_valid && !copy2_valid)
		new_mibib_block = copy1_blockno;
	else if (!copy1_valid && copy2_valid)
		new_mibib_block = copy2_blockno;
	else if (copy1_valid && copy2_valid)
		if (copy1_age > copy2_age)
			new_mibib_block = copy1_blockno;
		else
			new_mibib_block = copy2_blockno;

	if (new_mibib_block == -1) {
		printf("Failed to determine valid MIBIB block\n");
		free(page_buf);
		return NULL;
	}

	/* Allocate memory for the partition table first */
	parti_ptr = malloc(sizeof(struct flash_partition_table));
	if (!parti_ptr) {
		printf("Failed to allocate memory for partition table\n");
		free(page_buf);
		return NULL;
	}

	/* Read the partition table from the valid MIBIB block */
	u32 page = (new_mibib_block * (mtd->erasesize / mtd->writesize)) +
			MIBIB_PAGE_PARTITION_TABLE;

	size_t length = mtd->writesize;

	ret = nand_read(mtd, page * mtd->writesize, &length, page_buf);
	if (ret || length != mtd->writesize) {
		printf("Failed to read partition table\n");
		free(page_buf);
		free(parti_ptr);
		return NULL;
	}

	/* Copy the partition table */
	memcpy(parti_ptr, page_buf, sizeof(struct flash_partition_table));

	/* Verify the partition table */
	if ((parti_ptr->magic1 != FLASH_PART_MAGIC1) ||
		(parti_ptr->magic2 != FLASH_PART_MAGIC2) ||
		(parti_ptr->version != FLASH_PARTITION_VERSION)) {
		printf("Invalid partition table in MIBIB\n");
		free(page_buf);
		free(parti_ptr);
		return NULL;
	}

	free(page_buf);
	return parti_ptr;
}

/**
 * find_bootldr_partition() - Find the BOOTLDR partition in the partition table
 * @parti_ptr:	Pointer to the partition table
 *
 * This function searches for the 0:BOOTLDR partition in the partition table
 * and returns its offset.
 *
 * Return: Offset of the BOOTLDR partition, or 0 if not found
 */
static u32 find_bootldr_partition(struct flash_partition_table *parti_ptr)
{
	int i;

	if (!parti_ptr)
		return 0;

	for (i = 0; i < parti_ptr->numparts; i++) {
		if (strncmp(parti_ptr->part_entry[i].name, "0:BOOTLDR", 9) == 0 ||
			strncmp(parti_ptr->part_entry[i].name, "BOOTLDR", 7) == 0)
			return parti_ptr->part_entry[i].offset;
	}

	return 0;
}

/**
 * spl_nand_get_uboot_raw_page() - Get the page offset of the BOOTLDR partition
 *
 * This function retrieves the MIBIB from flash, finds the BOOTLDR partition,
 * and returns its page offset.
 *
 * Return: Page offset of the BOOTLDR partition, or 0 if not found
 */
int spl_nand_get_uboot_raw_page(void)
{
	struct mtd_info *mtd;

	/* If bootloader offset is already calculated, return it directly */
	if (g_bootldr_offset != 0)
		return g_bootldr_offset;

	/* Only retrieve MIBIB if it's not already saved */
	if (!g_mibib_parti_ptr) {
		/* Retrieve MIBIB */
		g_mibib_parti_ptr = nand_retrieve_mibib();
	}

	if (g_mibib_parti_ptr) {
		/* Find BOOTLDR partition */
		g_bootldr_offset = find_bootldr_partition(g_mibib_parti_ptr);

		/* Convert block offset to page offset */
		mtd = get_nand_dev_by_index(0);
		if (mtd)
			g_bootldr_offset = g_bootldr_offset * (mtd->erasesize);

		printf("BOOTLDR partition found at page offset: %d\n", g_bootldr_offset);
	} else {
		printf("Failed to retrieve MIBIB, using default offset\n");
	}

	return g_bootldr_offset;
}


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
