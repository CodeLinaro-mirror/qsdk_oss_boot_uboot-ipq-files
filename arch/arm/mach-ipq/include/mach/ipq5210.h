/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __IPQ5210_H__
#define __IPQ5210_H__

#define TCSR_BOOT_MISC_REG			((u32 *)0x195C100)

#define ROOT_FS_PART_NAME			"rootfs"
#define ROOT_FS_ATL_PART_NAME			"rootfs_1"

#define TME_OEM_ATE_FUSE_START			0x000A00E0
#define TME_OEM_ATE_FUSE_CNT			0x1
#define TME_OEM_ATE_FUSE_READ_SIZE		0x8

#define TME_OEM_MRC_HASH_FUSE_START		0x000A00F8
#define TME_OEM_MRC_HASH_FUSE_CNT		0x7
#define TME_OEM_MRC_HASH_FUSE_READ_SIZE		0x8

#define TME_AUTH_EN_MASK			0x82
#define TME_OEM_ID_MSK				0xFFFF0000
#define TME_PRODUCT_ID_MSK			0x0000FFFF

#define OEM_SEC_BOOT_ENABLE			BIT(7)

#define QFPROM_CORR_TME_OEM_ATE_ROW0_LSB	0xA40E0
#define QFPROM_CORR_TME_OEM_ATE_ROW1_LSB	0xA40E8

#define ROOTFS_AUTH_FUSE			0xA0058

struct fuse_payload {
	u32 fuse_addr;
	u32 lsb_val;
	u32 msb_val;
};

#define CFG_CPU_CONTEXT_DUMP_SIZE		0x180F0
#define TME_CTXT_SIZE				(128 * 1024)
#define CPU_CNTXT_HDR_SIZE			4624

#if defined(CONFIG_IPQ_MINIDUMP_VERSION_V2)
#define TLV_BUF_OFFSET				(489 * 1024) - TME_CTXT_SIZE
#define CFG_TLV_DUMP_SIZE			(23 * 1024)
#else
#define TLV_BUF_OFFSET                          (500 * 1024) - TME_CTXT_SIZE
#define CFG_TLV_DUMP_SIZE			(12 * 1024)
#endif /* CONFIG_IPQ_MINIDUMP_VERSION_V2 */
#endif
