/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _IPQ5424_H_
#define _IPQ5424_H_

#ifndef __ASSEMBLY__
#include <linux/types.h>
#include <linux/sizes.h>
extern uint32_t g_board_machid;
#endif

#define CONFIG_HAS_CUSTOM_SYS_INIT_SP_ADDR
#define CONFIG_CUSTOM_SYS_INIT_SP_ADDR		\
		(CONFIG_TEXT_BASE - CONFIG_SYS_MALLOC_LEN -\
			CONFIG_ENV_SIZE - GENERATED_GBL_DATA_SIZE)

#define CONFIG_MACH_TYPE                        (g_board_machid)

#define CFG_SYS_SDRAM_BASE0_ADDR		0x80000000
#define CFG_SYS_SDRAM_BASE0_SIZE		0x80000000
#if (CONFIG_NR_DRAM_BANKS > 1)
#define CFG_SYS_SDRAM_BASE1_ADDR		0x800000000
#define CFG_SYS_SDRAM_BASE1_SIZE		0x180000000
#endif

#define CFG_SYS_SDRAM_BASE			CFG_SYS_SDRAM_BASE0_ADDR
#define CFG_ROOTFS_LOAD_ADDR			(CFG_SYS_SDRAM_BASE + (32 << 20))

#define IPQ5424_UBOOT_END_ADDRESS		CONFIG_TEXT_BASE + \
							CONFIG_TEXT_SIZE
#ifndef CONFIG_ETH_LOW_MEM
#define NONCACHED_MEM_REGION_ADDR		((IPQ5424_UBOOT_END_ADDRESS + \
						SZ_1M - 1) & ~(SZ_1M - 1))
#define NONCACHED_MEM_REGION_SIZE		SZ_1M

#endif /* ifnot defined CONFIG_ETH_LOW_MEM */

#endif /* _IPQ5424_H_ */
