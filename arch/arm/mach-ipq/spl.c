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
#include <spl.h>
#include <elf.h>
#include <mach/ipq.h>
#include <mach/smem_info.h>
#include <asm/io.h>
#include <asm/sections.h>
#include <smem.h>

/*******************************************************************************
 * Globals constant & typedef
 ******************************************************************************/
#define IPQ_SPL_BOOTCFG_REG_ADDR	(0xA602C)
#define IPQ_SPL_BOOTCFG_DEV_MASK	GENMASK(3, 1)
#define IPQ_SPL_BOOTCFG_DEV_SHFT	(0x1)

#define IPQ_SPL_IMG_CNT_MAX		(32)
#define IPQ_SPL_ELF_HASH_SEG_SZ		(10 * SZ_1K)
#define IPQ_SPL_ELF_PHDR_CNT_MAX	(32)
#define IPQ_SPL_FLASH_RD_PRT_LMT	(false)

#define IPQ_SPL_ELF_METADATA_SZ		(sizeof(Elf64_Ehdr) +\
					(sizeof(Elf64_Phdr) *\
					IPQ_SPL_ELF_PHDR_CNT_MAX) +\
					IPQ_SPL_ELF_HASH_SEG_SZ)

#define IPQ_SPL_BDEV_MAX_SZ		(SZ_4K)

#define MI_PBT_HASH_SEGMENT		(0x2)
#define MI_PBT_FLAG_SEGMENT_TYPE_SHIFT	(0x18)
#define MI_PBT_FLAG_SEGMENT_TYPE_MASK	(0x7000000)

#define IPQ_SPL_IS_HASH_SEG(x)		(MI_PBT_HASH_SEGMENT ==\
					((x & MI_PBT_FLAG_SEGMENT_TYPE_MASK) >>\
					MI_PBT_FLAG_SEGMENT_TYPE_SHIFT))

#define MAGIC_KEY			("QCLIB_CB")
#define MAX_ENTRIES			(0xF)
#define IF_TABLE_VERSION		(0x1)
#define QCCONFIG			("qc_config")

/*******************************************************************************
 * Structure enum and static
 ******************************************************************************/
static uint8_t elf_metadata_buf[IPQ_SPL_ELF_METADATA_SZ] __aligned(SZ_4K);

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

enum {
	IPQ_SPL_ELF_IMG = 0x0,
	IPQ_SPL_BIN_IMG,
	IPQ_SPL_IMG_MAX
};

struct ipq_spl_fl_ctx {
	void *data;
	uint8_t type;
};

struct ipq_spl_elf_ctx {
	uint8_t class;
	uint8_t auth;
	void *ehdr;
	void *phdr;
	void *hash;
	uint32_t hash_sz;
	uint32_t metadata_sz;
	struct ipq_spl_fl_ctx *fl_ctx;
};

/** QCLIB INTERFACE */
/* meta data for blobsattribute bits can be added as required by the blob */
struct interface_table_entry {
	char entry_name[24];		// "dcb_settings", "ddr_training_data", etc..
	uint64_t address;		//address of the data blob
	uint32_t size;			// size of the data blob
	uint32_t attributes;		//bits [0]-save to storage
};

/* Interface table header*/
struct interface_table {
	char magic_key[8];		// QCLIB_CB
	uint32_t version;		// interface table version
	uint32_t num_entries;		// number of valid entries
	uint32_t max_entries;		// max allowable entries
	uint32_t global_attributes;	// flag for SDI path, force ddr training etc..
	uint32_t reserved1;
	uint32_t reserved2;
	struct interface_table_entry if_table_entries[MAX_ENTRIES];
};

struct ipq_spl_img_ctx {
	uint8_t img_type;
	char *img_name;
	char *prt_name;
	uint8_t *load_addr;
	uint64_t img_sz;
	uint64_t img_off;
	uint8_t load;
	uint8_t auth;
	uint8_t exec;
	uint8_t optional;
	int (*prepare)(void *ctx);
	int (*fixup)(void *ctx);
	int (*jump)(void *ctx);
};

struct ipq_spl_ctx {
	struct ipq_spl_img_ctx *img_tbl;
	struct interface_table if_tbl;
	struct ipq_spl_elf_ctx elf_ctx;
	struct ipq_spl_fl_ctx fl_ctx;
	struct spl_image_info *spl_image;
};

static int ipq_spl_loader(struct spl_image_info *spl_image,
				struct spl_boot_device *bootdev);

/* Image load Method */
#if CONFIG_IPQ_MMC
SPL_LOAD_IMAGE_METHOD("MMC", 0, SMEM_BOOT_MMC_FLASH, ipq_spl_loader);
#endif
#if CONFIG_IPQ_SPI_NOR
SPL_LOAD_IMAGE_METHOD("NOR GPT", 0, SMEM_BOOT_NORGPT_FLASH, ipq_spl_loader);
#endif
#if CONFIG_IPQ_NAND
SPL_LOAD_IMAGE_METHOD("SPI NAND", 0, SMEM_BOOT_QSPI_NAND_FLASH, ipq_spl_loader);
#endif

static int ipq_spl_xcfg_fixup(void *ctx);
static int ipq_spl_qclib_jump(void *ctx);
static int ipq_spl_bl31_jump(void *ctx);
static int ipq_spl_bl33_prepare(void *ctx);
typedef void (*ipq_spl_jump_img_entry_t)(void *arg);

/* Image loader Table */
struct ipq_spl_img_ctx img_tbl[] = { {
		.img_type = IPQ_SPL_ELF_IMG,
		.img_name = "XBLCONFIG",
		.prt_name = "0:XBLCONFIG",
		.load = true,
		.auth = true,
		.fixup = ipq_spl_xcfg_fixup,
	}, {
		.img_type = IPQ_SPL_ELF_IMG,
		.img_name = "QCLIB",
		.prt_name = "0:XBL_1",
		.load = true,
		.auth = true,
		.exec = true,
		.jump = ipq_spl_qclib_jump,
	}, {
		.img_type = IPQ_SPL_ELF_IMG,
		.img_name = "APPSBL",
		.prt_name = "0:APPSBL",
		.load = true,
		.auth = true,
		.prepare = ipq_spl_bl33_prepare,
	}, {
		.img_type = IPQ_SPL_ELF_IMG,
		.img_name = "OPTEE",
		.prt_name = "0:QSEE_1",
		.load = true,
		.auth = true,
		.optional = true,
	}, {
		.img_type = IPQ_SPL_ELF_IMG,
		.img_name = "TFA",
		.prt_name = "0:QSEE",
		.load = true,
		.auth = true,
		.exec = true,
		.jump = ipq_spl_bl31_jump,
	},
};

/*******************************************************************************
 * Function definition
 ******************************************************************************/
void lowlevel_init(void)
{
	/* Early disable the MMU */
	unsigned long sctlr;

	sctlr = get_sctlr();
	set_sctlr(sctlr & ~(CR_M));
}

#if defined(CONFIG_ARM64) && defined(CFG_EMUL_FREQUENCY_DIVIDER)
void ipq_spl_setup_arch_cntfreq(void)
{
	/* Emulation - Divide the counter frequency */
	unsigned long freq = CONFIG_COUNTER_FREQUENCY /
				CFG_EMUL_FREQUENCY_DIVIDER;
	asm volatile("msr cntfrq_el0, %0" : : "r" (freq) : "memory");
}
#endif

int ipq_spl_flash_init(struct ipq_spl_fl_ctx *fl_ctx)
{
	int ret;

	/* Verify media handle */
	if (IS_ERR_OR_NULL(fl_ctx)) {
		printf("%s: failed to get fl_ctx\n", __func__);
		return -ENODEV;
	}

	switch (fl_ctx->type) {
#if CONFIG_IPQ_MMC
	case SMEM_BOOT_MMC_FLASH:
		/* Initialize MMC */
		struct mmc *mmc;
		int mmc_dev;

		ret = mmc_init_device(mmc_dev);
		if (ret) {
			printf("mmc_init_device() failed\n");
			return ret;
		}

		mmc = find_mmc_device(mmc_dev);
		if (!mmc) {
			printf("find_mmc_device() failed\n");
			return -EIO;
		}

		ret = mmc_init(mmc);
		if (ret) {
			printf("mmc_init() failed\n");
			return ret;
		}
		break;
#endif
	case IPQ_SPL_RAM_FLASHLESS:
		break;
	/* TODO: Need to add all flash supports */
	default:
		printf("%s: Flash type Unsupported %d\n",
			__func__, fl_ctx->type);
		return -EINVAL;
	}

	return 0;
}

int ipq_spl_flash_close(struct ipq_spl_fl_ctx *fl_ctx)
{
	/* Verify media handle */
	if (IS_ERR_OR_NULL(fl_ctx)) {
		printf("%s: failed to get fl_ctx\n", __func__);
		return -ENODEV;
	}

	/* Media aldready closed */
	if (IS_ERR_OR_NULL(fl_ctx->data))
		return 0;

	/* Close the flash media */
	switch (fl_ctx->type) {
#if CONFIG_IPQ_MMC
	case SMEM_BOOT_MMC_FLASH:
		/* Free the disk info */
		if (((struct blkpart_info *)fl_ctx->data)->info)
			free(((struct blkpart_info *)fl_ctx->data)->info);

		/* Free the blkpart info */
		if (fl_ctx->data)
			free(fl_ctx->data);

		/* Initialalize the media handle */
		fl_ctx->data = NULL;
		break;
#endif
	case IPQ_SPL_RAM_FLASHLESS:
		fl_ctx->data = NULL;
		break;
	/* TODO: Need to add all flash supports */
	default:
		printf("%s: Flash type Unsupported %d\n",
			__func__, fl_ctx->type);
		return -EINVAL;
	}

	return 0;
}

int ipq_spl_flash_open(struct ipq_spl_fl_ctx *fl_ctx, char *prt_name)
{
	int ret;
	struct disk_partition *disk_info;
	struct blkpart_info *bpart_info;
	struct blk_desc *bdev;

	/* Verify media handle */
	if (IS_ERR_OR_NULL(fl_ctx)) {
		printf("%s: failed to get fl_ctx\n", __func__);
		return -ENODEV;
	}

	/* Close the flash media */
	switch (fl_ctx->type) {
#if CONFIG_IPQ_MMC
	case SMEM_BOOT_MMC_FLASH:
		if (!IS_ERR_OR_NULL(fl_ctx->data))
			ipq_spl_flash_close(fl_ctx);

		/* Allocate MMC blk info struct */
		disk_info = calloc(1, sizeof(struct disk_partition));
		if (IS_ERR_OR_NULL(disk_info)) {
			printf("%s: Can't allocate disk_info\n", __func__);
			return -ENODEV;
		}

		bpart_info = calloc(1, sizeof(struct blkpart_info));
		if (IS_ERR_OR_NULL(bpart_info)) {
			printf("%s: Can't allocate bpart_info\n", __func__);
			return -ENODEV;
		}

		/* Populate MMC blk info */
		bpart_info->name = prt_name;
		bpart_info->info = disk_info;
		bpart_info->flash_type = fl_ctx->type;
		bpart_info->devnum = 0;
		bpart_info->verbose = true;

		/* Assign the mmc block pointer to the media handle */
		fl_ctx->data = bpart_info;

		/* Get MMC device by Class */
		bdev = blk_get_devnum_by_uclass_id(UCLASS_MMC,
							bpart_info->devnum);
		if (IS_ERR_OR_NULL(bdev)) {
			printf("No such device\n");
			return -ENODEV;
		}
#ifdef CONFIG_EFI_PARTITION
		if (bdev->part_type == PART_TYPE_UNKNOWN)
			bdev->part_type = PART_TYPE_EFI;
#endif
		/* Get MMC partition info using the partition name */
		bpart_info->desc = bdev;
		ret = part_get_info_by_name(bdev,
						bpart_info->name,
						bpart_info->info);
		if (ret < 0) {
			if (bpart_info->verbose)
				printf(" %s Partition not found, ret %d !!!\n",
					bpart_info->name, ret);
			return -ENODEV;
		}
	break;
#endif
	case IPQ_SPL_RAM_FLASHLESS:
		fl_ctx->data = NULL;
	break;
	/* TODO: Need to add all flash supports */
	default:
		printf("%s: Flash type Unsupported %d\n",
			__func__, fl_ctx->type);
		return -EINVAL;
	}

	return 0;
}

int ipq_spl_flash_read(struct ipq_spl_fl_ctx *fl_ctx,
			void *addr,
			uint64_t offset,
			uint64_t size)
{
	/* Common buffer to read one block */
	uint8_t  rd_blk_buf[IPQ_SPL_BDEV_MAX_SZ];

	/* Variables to handle flash read in block devices */
	uint32_t start_blk;
	uint32_t blk_cnt;
	uint32_t end_blk;

	uint32_t byte_offset;
	uint32_t byte_cnt;

	/* Verify media handle */
	if (IS_ERR_OR_NULL(fl_ctx)) {
		printf("%s: failed to get fl_ctx\n", __func__);
		return -ENODEV;
	}

	switch (fl_ctx->type) {
#if CONFIG_IPQ_MMC
	case SMEM_BOOT_MMC_FLASH:
		if (IS_ERR_OR_NULL(fl_ctx->data)) {
			printf("%s: failed to get fl_ctx->data\n", __func__);
			return -ENODEV;
		}

		struct blkpart_info *bpart_info =
			(struct blkpart_info *)fl_ctx->data;

		/* Populate block read variables */
		start_blk = bpart_info->info->start;
		start_blk += offset / bpart_info->info->blksz;

		byte_offset = offset % bpart_info->info->blksz;
		end_blk = bpart_info->info->start +
				bpart_info->info->size;

		/* Handle partial read for the first block */
		if (byte_offset > 0) {
			/* Read 1 block and
			 * copy the remaining bytes to the memory
			 */
			blk_cnt = 1;
#if IPQ_SPL_FLASH_RD_PRT_LMT
			/* Validate block present within the partition */
			if (end_blk < (start_blk + blk_cnt)) {
				printf("%s: Partition limit exceeded\n",
					__func__);
				return -EOVERFLOW;
			}
#endif
			memset((void *)rd_blk_buf,
				0,
				bpart_info->info->blksz);

			if (blk_cnt != blk_dread(bpart_info->desc,
						start_blk,
						blk_cnt,
						rd_blk_buf)) {
				printf("mmc block read error\n");
				return -EIO;
			}

			/* Get the partial number of bytes present in the block
			 * from the offset
			 */
			byte_cnt = bpart_info->info->blksz - byte_offset;

			/* Get the partial number of bytes in the given size */
			byte_cnt = min_t(uint64_t, byte_cnt, size);

			/* copy partial read bytes to the memory */
			memcpy(addr,
				(void *)&rd_blk_buf[byte_offset],
				byte_cnt);

			/* Update the offsets after read */
			byte_offset = 0;
			start_blk += blk_cnt;
			addr = (uint8_t *)addr + byte_cnt;
			size -= byte_cnt;
		}

		/* Handle block read for blocks
		 * in the remaining size
		 */
		blk_cnt = size / bpart_info->info->blksz;
		if (blk_cnt > 0) {
#if IPQ_SPL_FLASH_RD_PRT_LMT
			/* Validate block present within the partition */
			if (end_blk < (start_blk + blk_cnt)) {
				printf("%s: Partition limit exceeded\n",
					__func__);
				return -EOVERFLOW;
			}
#endif
			if (blk_cnt != blk_dread(bpart_info->desc,
						start_blk,
						blk_cnt,
						addr)) {
				printf("mmc block read error\n");
				return -EIO;
			}

			/* Update the offsets after read */
			start_blk += blk_cnt;
			addr = (uint8_t *)addr +
				(blk_cnt * bpart_info->info->blksz);
			size -= blk_cnt * bpart_info->info->blksz;
		}

		/* Handle partial read for the last block
		 * in the remaining size
		 */
		byte_cnt = size;
		if (byte_cnt > 0) {
			/* Read 1 block and
			 * copy the remaining bytes to the memory
			 */
			blk_cnt = 1;
#if IPQ_SPL_FLASH_RD_PRT_LMT
			/* Validate block present within the partition */
			if (end_blk < (start_blk + blk_cnt)) {
				printf("%s: Partition limit exceeded\n",
					__func__);
				return -EOVERFLOW;
			}
#endif
			memset((void *)rd_blk_buf,
				0,
				bpart_info->info->blksz);

			if (blk_cnt != blk_dread(bpart_info->desc,
						start_blk,
						blk_cnt,
						rd_blk_buf)) {
				printf("mmc block read error\n");
				return -EIO;
			}

			/* copy partial read bytes to the memory */
			memcpy(addr,
				(void *)rd_blk_buf,
				byte_cnt);

				/* Update the offsets after read */
				start_blk += blk_cnt;
				addr = (uint8_t *)addr + byte_cnt;
				size -= byte_cnt;
				byte_cnt = 0;
		}
		break;
#endif
	case IPQ_SPL_RAM_FLASHLESS:
		memcpy(addr, (void *)(offset), size);
		break;
	/* TODO: Need to add all flash supports */
	default:
		printf("%s: Flash type Unsupported %d\n",
			__func__, fl_ctx->type);
		return -EINVAL;
	}

	return 0;
}

int ipq_spl_elf_init(struct ipq_spl_elf_ctx *elf_ctx)
{
	/* Verify params */
	if (IS_ERR_OR_NULL(elf_ctx)) {
		printf("%s: failed to get elf_ctx\n", __func__);
		return -ENODEV;
	}

	/* init the ELF ctx */
	elf_ctx->class = ELFCLASSNONE;
	elf_ctx->auth = false;
	elf_ctx->ehdr = NULL;
	elf_ctx->phdr = NULL;
	elf_ctx->hash = NULL;
	elf_ctx->hash_sz = 0;
	elf_ctx->metadata_sz = 0;
	elf_ctx->fl_ctx = NULL;

	/* Zero init the ELF metadata buffer
	 * used for Authentication
	 */
	memset((void *)elf_metadata_buf,
		0x0,
		IPQ_SPL_ELF_METADATA_SZ);

	return 0;
}

int ipq_spl_load_ehdr(struct ipq_spl_elf_ctx *elf_ctx)
{
	int ret;
	Elf64_Ehdr *ehdr;

	/* Verify params */
	if (IS_ERR_OR_NULL(elf_ctx)) {
		printf("%s: failed to get elf_ctx\n", __func__);
		return -ENODEV;
	}

	/* Verify media handle */
	if (IS_ERR_OR_NULL(elf_ctx->fl_ctx)) {
		printf("%s: failed to get fl_ctx\n", __func__);
		return -ENODEV;
	}

	/* Read ELF header into the buffer */
	elf_ctx->ehdr = (void *)elf_metadata_buf;
	ret = ipq_spl_flash_read(elf_ctx->fl_ctx,
				elf_ctx->ehdr,
				0,
				sizeof(Elf64_Ehdr));
	if (ret)
		return ret;

	/* Verify ELF header */
	ehdr = (Elf64_Ehdr *)elf_ctx->ehdr;
	if (!IS_ELF(*ehdr)) {
		printf("%s: ELF Image Invalid\n", __func__);
		return -EINVAL;
	}

	/* Parse elf class */
	elf_ctx->class = ehdr->e_ident[EI_CLASS];

	return 0;
}

int ipq_spl_load_phdr(struct ipq_spl_elf_ctx *elf_ctx)
{
	int ret;
	uint64_t phdr_offset;
	uint64_t phdrs_size;

	/* Verify params */
	if (IS_ERR_OR_NULL(elf_ctx)) {
		printf("%s: failed to get elf_ctx\n", __func__);
		return -ENODEV;
	}

	/* Verify media handle */
	if (IS_ERR_OR_NULL(elf_ctx->fl_ctx)) {
		printf("%s: failed to get fl_ctx\n", __func__);
		return -ENODEV;
	}

	switch (elf_ctx->class) {
	case ELFCLASS64:
		phdr_offset =
			((Elf64_Ehdr *)elf_ctx->ehdr)->e_phoff;
		phdrs_size =
			((Elf64_Ehdr *)elf_ctx->ehdr)->e_phnum *
			((Elf64_Ehdr *)elf_ctx->ehdr)->e_phentsize;
	break;
	case ELFCLASS32:
		phdr_offset =
			((Elf32_Ehdr *)elf_ctx->ehdr)->e_phoff;
		phdrs_size =
			((Elf32_Ehdr *)elf_ctx->ehdr)->e_phnum *
			((Elf32_Ehdr *)elf_ctx->ehdr)->e_phentsize;
	break;
	default:
		printf("%s: ELF class Invalid %d\n",
			__func__, elf_ctx->class);
		return -EINVAL;
	}

	/* Read ELF Program Header into the buffer */
	elf_ctx->phdr = (void *)(&elf_metadata_buf[phdr_offset]);

	ret = ipq_spl_flash_read(elf_ctx->fl_ctx,
				elf_ctx->phdr,
				phdr_offset,
				phdrs_size);
	if (ret)
		return ret;

	return 0;
}

int ipq_spl_load_seg(struct ipq_spl_fl_ctx *fl_ctx,
			uint8_t class,
			void *phdr)
{
	int ret;
	uint32_t seg_ptype;
	uint64_t seg_flags;
	uint64_t seg_paddr;
	uint64_t seg_offset;
	uint64_t seg_filesz;
	uint64_t seg_memsz;

	/* Verify params */
	if (IS_ERR_OR_NULL(phdr)) {
		printf("%s: failed to get phdr\n", __func__);
		return -ENODEV;
	}

	/* Verify media handle */
	if (IS_ERR_OR_NULL(fl_ctx)) {
		printf("%s: failed to get fl_ctx\n", __func__);
		return -ENODEV;
	}

	switch (class) {
	case ELFCLASS64:
		seg_ptype = ((Elf64_Phdr *)phdr)->p_type;
		seg_flags = ((Elf64_Phdr *)phdr)->p_flags;
		seg_offset = ((Elf64_Phdr *)phdr)->p_offset;
		seg_filesz = ((Elf64_Phdr *)phdr)->p_filesz;
		seg_memsz = ((Elf64_Phdr *)phdr)->p_memsz;
		seg_paddr = ((Elf64_Phdr *)phdr)->p_paddr;
		break;
	case ELFCLASS32:
		seg_ptype = ((Elf32_Phdr *)phdr)->p_type;
		seg_flags = ((Elf32_Phdr *)phdr)->p_flags;
		seg_offset = ((Elf32_Phdr *)phdr)->p_offset;
		seg_filesz = ((Elf32_Phdr *)phdr)->p_filesz;
		seg_memsz = ((Elf32_Phdr *)phdr)->p_memsz;
		seg_paddr = ((Elf32_Phdr *)phdr)->p_paddr;
		break;
	default:
		printf("%s: ELF class Invalid %d\n",
			__func__, class);
		return -EINVAL;
	}

	if ((seg_ptype != PT_LOAD) &&
		(false == IPQ_SPL_IS_HASH_SEG(seg_flags))) {
		/* Not a Loadable or Not an hash segmnet
		 * Skip the segment
		 */
		return 0;
	} else if (!seg_paddr) {
		/* Skip the segment */
		return 0;
	} else if (seg_filesz > seg_memsz) {
		/* Skip the segment */
		return 0;
	} else if (seg_filesz > 0) {
		/* Load the segment */
		ret = ipq_spl_flash_read(fl_ctx,
					(void *)(seg_paddr),
					seg_offset,
					seg_filesz);
		if (ret)
			return ret;
	}

	/* Zero init the remaining memory region */
	if (seg_memsz > seg_filesz) {
		memset((void *)(seg_paddr + seg_filesz),
			0,
			seg_memsz - seg_filesz);
	}

	return 0;
}

int ipq_spl_load_img_segments(struct ipq_spl_elf_ctx *elf_ctx)
{
	int ret;
	uint8_t seg_count;
	uint8_t seg_index;
	void *phdr;
	uint64_t phdr_size;
	uint32_t ptype;

	/* Verify params */
	if (IS_ERR_OR_NULL(elf_ctx)) {
		printf("%s: failed to get elf_ctx\n", __func__);
		return -ENODEV;
	}

	/* Verify media handle */
	if (IS_ERR_OR_NULL(elf_ctx->fl_ctx)) {
		printf("%s: failed to get fl_ctx\n", __func__);
		return -ENODEV;
	}

	switch (elf_ctx->class) {
	case ELFCLASS64:
		seg_count = ((Elf64_Ehdr *)elf_ctx->ehdr)->e_phnum;
		phdr_size = ((Elf64_Ehdr *)elf_ctx->ehdr)->e_phentsize;
		break;
	case ELFCLASS32:
		seg_count = ((Elf32_Ehdr *)elf_ctx->ehdr)->e_phnum;
		phdr_size = ((Elf32_Ehdr *)elf_ctx->ehdr)->e_phentsize;
		break;
	default:
		printf("%s: ELF class Invalid %d\n",
			__func__, elf_ctx->class);
		return -EINVAL;
	}

	/* Load the ELF segments */
	for (seg_index = 0; seg_index < seg_count; seg_index++) {
		phdr = (uint8_t *)elf_ctx->phdr + (seg_index * phdr_size);

		ptype = (elf_ctx->class == ELFCLASS64) ?
				((Elf64_Phdr *)phdr)->p_type :
				((Elf32_Phdr *)phdr)->p_type;

		if (ptype == PT_LOAD) {
			/* Read the ELF segment */
			ret = ipq_spl_load_seg(elf_ctx->fl_ctx,
						elf_ctx->class,
						phdr);
			if (ret)
				break;
		}
	}

	return 0;
}

static int ipq_spl_xcfg_fixup(void *ctx)
{
	struct ipq_spl_ctx *pctx = ctx;

	/* Verify params */
	if (IS_ERR_OR_NULL(pctx)) {
		printf("%s: failed to get ctx\n", __func__);
		return -ENODEV;
	}

	/* Zero init the QCLIB structure */
	memset((void *)&pctx->if_tbl,
		0,
		sizeof(struct interface_table));

	/* Populate qclib structure */
	memcpy((void *)pctx->if_tbl.magic_key,
		MAGIC_KEY,
		strlen(MAGIC_KEY));

	pctx->if_tbl.version = IF_TABLE_VERSION;
	pctx->if_tbl.num_entries = 0x1;
	pctx->if_tbl.max_entries = MAX_ENTRIES;
	pctx->if_tbl.if_table_entries[0].attributes = 0;

	memcpy((void *)pctx->if_tbl.if_table_entries[0].entry_name,
		QCCONFIG,
		strlen(QCCONFIG));

	/* Populate xblconfig entry point to the if_table */
	switch (pctx->elf_ctx.class) {
	case ELFCLASS64:
		pctx->if_tbl.if_table_entries[0].address =
			((Elf64_Ehdr *)pctx->elf_ctx.ehdr)->e_entry;
		break;
	case ELFCLASS32:
		pctx->if_tbl.if_table_entries[0].address =
			((Elf32_Ehdr *)pctx->elf_ctx.ehdr)->e_entry;
		break;
	default:
		printf("%s: ELF class Invalid %d\n",
			__func__, pctx->elf_ctx.class);
		return -EINVAL;
	}

	return 0;
}

static int ipq_spl_qclib_jump(void *ctx)
{
	struct ipq_spl_ctx *pctx = ctx;
	ipq_spl_jump_img_entry_t qclib_entry;
	uint64_t entry_point;

	/* Verify params */
	if (IS_ERR_OR_NULL(pctx)) {
		printf("%s: failed to get ctx\n", __func__);
		return -ENODEV;
	}

	/* parse entry point */
	switch (pctx->elf_ctx.class) {
	case ELFCLASS64:
		entry_point =
			((Elf64_Ehdr *)pctx->elf_ctx.ehdr)->e_entry;
		break;
	case ELFCLASS32:
		entry_point =
			((Elf32_Ehdr *)pctx->elf_ctx.ehdr)->e_entry;
		break;
	default:
		printf("%s: ELF class Invalid %d\n",
			__func__, pctx->elf_ctx.class);
		return -EINVAL;
	}

	/* Set qclib entry point */
	qclib_entry = (ipq_spl_jump_img_entry_t)(entry_point);

	/* Jump to Qclib */
	printf("Jump to Image: %s\n", pctx->img_tbl->img_name);
	qclib_entry(&pctx->if_tbl);

	return 0;
}

static int ipq_spl_bl31_jump(void *ctx)
{
	struct ipq_spl_ctx *pctx = ctx;
	ipq_spl_jump_img_entry_t ipq_spl_bl31_entry;

	/* Verify params */
	if (IS_ERR_OR_NULL(pctx)) {
		printf("%s: failed to get ctx\n", __func__);
		return -ENODEV;
	}

	/* Verify Image info */
	if (IS_ERR_OR_NULL(pctx->spl_image)) {
		printf("%s: failed to get spl_image\n", __func__);
		return -ENODEV;
	}

	/* parse entry point */
	switch (pctx->elf_ctx.class) {
	case ELFCLASS64:
		pctx->spl_image->entry_point =
			((Elf64_Ehdr *)pctx->elf_ctx.ehdr)->e_entry;
		break;
	case ELFCLASS32:
		pctx->spl_image->entry_point =
			((Elf32_Ehdr *)pctx->elf_ctx.ehdr)->e_entry;
		break;
	default:
		printf("%s: ELF class Invalid %d\n",
			__func__, pctx->elf_ctx.class);
		return -EINVAL;
	}

	/* Set atf entry point */
	ipq_spl_bl31_entry =
		(ipq_spl_jump_img_entry_t)(pctx->spl_image->entry_point);

	/* Jump to atf */
	printf("Jump to Image: %s\n", pctx->img_tbl->img_name);
	pctx->spl_image->arg = (void *)((uint64_t)(0x80058000));
	ipq_spl_bl31_entry(pctx->spl_image->arg);

	return 0;
}

static int ipq_spl_bl33_prepare(void *ctx)
{
	int ret;
	struct ipq_spl_ctx *pctx = ctx;
	struct udevice *smem;
	size_t size;
	uint32_t *fltype;
	uint32_t *trymode;
	uint32_t *atf_en;

	/* Verify params */
	if (IS_ERR_OR_NULL(pctx)) {
		printf("%s: failed to get ctx\n", __func__);
		return -ENODEV;
	}

	ipq_uboot_fdt_fixup((void *)gd->fdt_blob, UBOOT_FIXUP_SMEM);
	ret = uclass_get_device(UCLASS_SMEM, 0, &smem);
	if (ret) {
		printf("Failed to find SMEM node %d\n", ret);
		return ret;
	}

	/* Populate Flash Type */
	size = sizeof(uint32_t);
	ret = smem_alloc(smem,
			-1,
			SMEM_BOOT_FLASH_TYPE,
			size);
	if (ret) {
		printf("Failed to alloc SMEM item: SMEM_BOOT_FLASH_TYPE\n");
		return ret;
	}

	fltype = (uint32_t *)smem_get(smem,
					-1,
					SMEM_BOOT_FLASH_TYPE,
					&size);
	if (fltype == NULL) {
		printf("Failed to get SMEM item: SMEM_BOOT_FLASH_TYPE\n");
		return -ENODEV;
	}
	*fltype = pctx->fl_ctx.type;

	/* Populate Trymode info */
	size = sizeof(uint32_t);
	ret = smem_alloc(smem,
			-1,
			SMEM_TRY_MODE_INPROGRESS,
			size);
	if (ret) {
		printf("Failed to alloc SMEM item: SMEM_TRY_MODE_INPROGRESS\n");
		return ret;
	}

	trymode = (uint32_t *)smem_get(smem,
					-1,
					SMEM_TRY_MODE_INPROGRESS,
					&size);
	if (trymode == NULL) {
		printf("Failed to get SMEM item: SMEM_TRY_MODE_INPROGRESS\n");
		return -ENODEV;
	}
	*trymode = false;

	/* Populate atf info */
	size = sizeof(uint32_t);
	ret = smem_alloc(smem,
			-1,
			SMEM_ATF_ENABLE,
			size);
	if (ret) {
		printf("Failed to alloc SMEM item: SMEM_ATF_ENABLE\n");
		return ret;
	}

	atf_en = (uint32_t *)smem_get(smem,
					-1,
					SMEM_ATF_ENABLE,
					&size);
	if (atf_en == NULL) {
		printf("Failed to get SMEM item: SMEM_ATF_ENABLE\n");
		return -ENODEV;
	}
	*atf_en = true;

	/* TODO: Populate the SMEM MIBIB Info */
	return 0;
}

int ipq_spl_load_image(struct ipq_spl_ctx *pctx,
			struct ipq_spl_img_ctx *p_img_entry)
{
	int ret;

	/* Verify params */
	if (IS_ERR_OR_NULL(pctx)) {
		printf("%s: failed to get ctx\n", __func__);
		return -ENODEV;
	}

	if (IS_ERR_OR_NULL(p_img_entry)) {
		printf("%s: failed to get p_img_entry\n", __func__);
		return -ENODEV;
	}

	/* Open Image Partition */
	printf("Loading Image: %s\n", p_img_entry->img_name);
	ret = ipq_spl_flash_open(&pctx->fl_ctx,
				p_img_entry->prt_name);
	if (ret)
		return ret;

	/* Load Image */
	switch (p_img_entry->img_type) {
	case IPQ_SPL_ELF_IMG:
		/* Initialize the ELF handle */
		ret = ipq_spl_elf_init(&pctx->elf_ctx);
		if (ret)
			return ret;

		/* Populate elf handle data */
		pctx->elf_ctx.fl_ctx  = &pctx->fl_ctx;

		/* Load ELF header */
		ret = ipq_spl_load_ehdr(&pctx->elf_ctx);
		if (ret)
			return ret;

		/* Load ELF Program headers */
		ret = ipq_spl_load_phdr(&pctx->elf_ctx);
		if (ret)
			return ret;

		/* Load the image segments */
		ret = ipq_spl_load_img_segments(&pctx->elf_ctx);
		if (ret)
			return ret;
		break;
	case IPQ_SPL_BIN_IMG:
		ret = ipq_spl_flash_read(&pctx->fl_ctx,
					p_img_entry->load_addr,
					p_img_entry->img_off,
					p_img_entry->img_sz);
		if (ret)
			return ret;
		break;
	default:
		printf("%s, Unsupported Image type %d\n",
			__func__, p_img_entry->img_type);
		return -EINVAL;
	}

	/* close the media */
	ret = ipq_spl_flash_close(&pctx->fl_ctx);
	if (ret)
		return ret;

	return 0;
}

int ipq_spl_load(void *ctx)
{
	int ret;
	struct ipq_spl_ctx *pctx = ctx;
	struct ipq_spl_img_ctx *p_img_entry;

	/* Verify params */
	if (IS_ERR_OR_NULL(pctx)) {
		printf("%s: failed to get ctx\n", __func__);
		return -ENODEV;
	}

	p_img_entry = pctx->img_tbl;
	if (IS_ERR_OR_NULL(p_img_entry)) {
		printf("%s: failed to get p_img_entry\n", __func__);
		return -ENODEV;
	}

	if (p_img_entry->load == false)
		return 0;

	/* prepare to load image */
	if (p_img_entry->prepare != NULL) {
		ret = p_img_entry->prepare(pctx);
		if (ret)
			return ret;
	}

	/* load the image */
	ret = ipq_spl_load_image(pctx, p_img_entry);
	if (ret) {
		if (p_img_entry->optional == true) {
			/* Clear the error for an optional image */
			ret = 0;
			printf("Failed to Load optional Image: %s\n",
				p_img_entry->img_name);
		} else {
			printf("Failed to Load Image: %s, Error 0x%x\n",
				p_img_entry->img_name,
				ret);
		}
		ipq_spl_flash_close(&pctx->fl_ctx);
	}

	/* perform image fixup */
	if (p_img_entry->fixup != NULL) {
		ret = p_img_entry->fixup(pctx);
		if (ret)
			return ret;
	}

	/* Do Jump function */
	if ((p_img_entry->jump != NULL) &&
		(true == p_img_entry->exec)) {
		ret = p_img_entry->jump(pctx);
		if (ret)
			return ret;
	}

	return 0;
}

static int ipq_spl_loader(struct spl_image_info *spl_image,
				struct spl_boot_device *bootdev)
{
	int ret;
	uint8_t uc_index;
	uint8_t uc_size;
	struct ipq_spl_ctx ctx;

	/* Zero init the ipq spl loader context */
	memset((void *)&ctx, 0, sizeof(struct ipq_spl_ctx));

	/* Populate context */
	ctx.fl_ctx.type = bootdev->boot_device;
	ctx.spl_image = spl_image;

	/* Initialize the flash */
	ret = ipq_spl_flash_init(&ctx.fl_ctx);
	if (ret)
		return ret;

	/* Load images */
	uc_size = ARRAY_SIZE(img_tbl);
	for (uc_index = 0; uc_index < uc_size; uc_index++) {
		/* Load the image */
		ctx.img_tbl = &img_tbl[uc_index];
		ret = ipq_spl_load(&ctx);
		if (ret)
			return ret;
	}

	return 0;
}

#if !defined(CONFIG_SPL_FRAMEWORK_BOARD_INIT_F)
void board_init_f(ulong dummy)
{
	int ret;

	/* Clear BSS */
	memset(__bss_start, 0, __bss_end - __bss_start);

#if defined(CONFIG_ARM64) && defined(CFG_EMUL_FREQUENCY_DIVIDER)
	ipq_spl_setup_arch_cntfreq();
#endif
	/* Referred from U-boot Proper code (target specific):
	 * board/qualcomm/<ipqxxxx/ipqxxxx.c
	 */
	ipq_spl_board_early_init_f();

	ret = spl_early_init();
	if (ret) {
		printf("spl_early_init() failed: %d\n", ret);
		hang();
	}

	preloader_console_init();
}
#endif

u32 spl_boot_device(void)
{
	/* Parse Bootdevice */
	uint8_t boot_device =
	(readl(IPQ_SPL_BOOTCFG_REG_ADDR) & IPQ_SPL_BOOTCFG_DEV_MASK) >>
	IPQ_SPL_BOOTCFG_DEV_SHFT;

	/* Map the boot device */
	switch (boot_device) {
#if CONFIG_IPQ_SPI_NOR
	case IPQ_SPL_BOOTCFG_DEV_NOR_GPT:
		boot_device = SMEM_BOOT_NORGPT_FLASH;
		break;
#endif
#if CONFIG_IPQ_MMC
	case IPQ_SPL_BOOTCFG_DEV_MMC:
		boot_device = SMEM_BOOT_MMC_FLASH;
		break;
#endif
#if CONFIG_IPQ_NAND
	case IPQ_SPL_BOOTCFG_DEV_SPI_NAND:
		boot_device = SMEM_BOOT_QSPI_NAND_FLASH;
		break;
#endif
	default:
		printf("Invalid Boot device %d\n", boot_device);
		return -EINVAL;
	}

	return boot_device;
}
