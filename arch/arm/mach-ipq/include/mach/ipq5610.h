/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __IPQ5610_H__
#define __IPQ5610_H__

#define TCSR_BOOT_MISC_REG			((u32 *)0x195C100)

#define ROOT_FS_PART_NAME			"rootfs"
#define ROOT_FS_ATL_PART_NAME			"rootfs_1"

#define QFPROM_CORR_TME_OEM_ATE_ROW0_LSB	0xA40E8
#define QFPROM_CORR_TME_OEM_ATE_ROW1_LSB	0xA40F0

#endif

struct fuse_payload {
	u32 fuse_addr;
	u32 lsb_val;
	u32 msb_val;
};
