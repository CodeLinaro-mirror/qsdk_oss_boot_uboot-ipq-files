/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __IPQ9574_H__
#define __IPQ9574_H__

#define TCSR_BOOT_MISC_REG			((u32 *)0x193D100)

/*
 * Rootfs authentication fuse
 */
#define ROOTFS_AUTH_FUSE			0xA6044

struct fuse_payload {
	u32 fuse_addr;
	u32 val;
};

#define TME_OEM_ATE_FUSE_START			0x000A00C0
#define TME_OEM_ATE_FUSE_CNT			0x2
#define TME_OEM_ATE_FUSE_READ_SIZE		0x4

#define TME_OEM_MRC_HASH_FUSE_START		0x000A00D8
#define TME_OEM_MRC_HASH_FUSE_CNT		0xE
#define TME_OEM_MRC_HASH_FUSE_READ_SIZE		0x4

#define KERNEL_START_ADDR			CFG_SYS_SDRAM_BASE
#define BOOT_PARAMS_ADDR			(CFG_SYS_SDRAM_BASE + 0x100)
#define FDT_HIGH				0x48500000

#define PHY_ANEG_TIMEOUT			100

#define ROOT_FS_PART_NAME			"rootfs"
#define ROOT_FS_ATL_PART_NAME			"rootfs_1"

#endif
