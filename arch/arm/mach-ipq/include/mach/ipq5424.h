/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __IPQ5424_H__
#define __IPQ5424_H__

#define TCSR_BOOT_MISC_REG			((u32 *)0x195C100)
#define QFPROM_CORR_TME_OEM_ATE_ROW0_LSB	0xA40E0
#define QFPROM_CORR_TME_OEM_ATE_ROW1_LSB	0xA40E8

/*
 * Rootfs authentication fuse
 */
#define ROOTFS_AUTH_FUSE			0xA0058

#define OEM_SEC_BOOT_ENABLE			BIT(7)

/*
 * TCSR Registers
 */
#define TCSR_TZ_WONCE0				0x195C000
#define TCSR_TZ_WONCE1				0x195C004

#define ENABLE_EDL_MODE				BIT(0)

#define EDL_RECOVERY_MODE			0x2
#define UBOOT_RECOVERY_MODE			0x1

/* Crashdump minimal configs */
#define CFG_CPU_CONTEXT_DUMP_SIZE		0x180F0
#define TME_CTXT_SIZE				(128 * 1024)
#define CPU_CNTXT_HDR_SIZE			4624
#define TLV_BUF_OFFSET				(500 * 1024) - TME_CTXT_SIZE \
							- CPU_CNTXT_HDR_SIZE
#define CFG_TLV_DUMP_SIZE			(12 * 1024)

#define TME_OEM_ATE_FUSE_START			0x000A00E0
#define TME_OEM_ATE_FUSE_CNT			0x1
#define TME_OEM_ATE_FUSE_READ_SIZE		0x8

#define TME_OEM_MRC_HASH_FUSE_START		0x000A00F8
#define TME_OEM_MRC_HASH_FUSE_CNT		0x7
#define TME_OEM_MRC_HASH_FUSE_READ_SIZE		0x8

#define TME_AUTH_EN_MASK			0x82
#define TME_OEM_ID_MSK				0xFFFF0000
#define TME_PRODUCT_ID_MSK			0x0000FFFF

#define KERNEL_START_ADDR			CFG_SYS_SDRAM_BASE
#define BOOT_PARAMS_ADDR			(KERNEL_START_ADDR + 0x100)
#define FDT_HIGH				0x88500000

#define PHY_ANEG_TIMEOUT			100

#define ROOT_FS_PART_NAME			"rootfs"
#define ROOT_FS_ATL_PART_NAME			"rootfs_1"

#define TME_AUTH_EN_MASK			0x82
#define TME_OEM_ID_MSK				0xFFFF0000
#define TME_PRODUCT_ID_MSK			0x0000FFFF

#define CRASH_DUMP_ADDR_IMEM			0x8600658

struct fuse_payload {
	u32 fuse_addr;
	u32 lsb_val;
	u32 msb_val;
};

#endif /* _IPQ5424_H_ */
