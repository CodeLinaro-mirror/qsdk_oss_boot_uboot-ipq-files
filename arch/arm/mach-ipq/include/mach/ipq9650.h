/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __IPQ9650_H__
#define __IPQ9650_H__

#define TCSR_BOOT_MISC_REG			((u32 *)0x195C100)

#define ROOT_FS_PART_NAME			"rootfs"
#define ROOT_FS_ATL_PART_NAME			"rootfs_1"

#define TME_OEM_ATE_FUSE_START			0x000A40E8
#define TME_OEM_ATE_FUSE_CNT			0x1
#define TME_OEM_ATE_FUSE_READ_SIZE		0x8

#define TME_OEM_MRC_HASH_FUSE_START		0x000A4100
#define TME_OEM_MRC_HASH_FUSE_CNT		0x7
#define TME_OEM_MRC_HASH_FUSE_READ_SIZE		0x8

#define TME_AUTH_EN_MASK			0x82
#define TME_OEM_ID_MSK				0xFFFF0000
#define TME_PRODUCT_ID_MSK			0x0000FFFF

struct fuse_payload {
	u32 fuse_addr;
	u32 lsb_val;
	u32 msb_val;
};

#endif
