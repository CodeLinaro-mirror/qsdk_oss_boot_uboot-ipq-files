/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023-2025, Qualcomm Innovation Center,Inc.All rights reserved.
 */

#ifndef _IPQ9574_H_
#define _IPQ9574_H_

#ifndef __ASSEMBLY__
#include <linux/types.h>
extern uint32_t g_board_machid;
#endif

#define CONFIG_HAS_CUSTOM_SYS_INIT_SP_ADDR
#define CONFIG_CUSTOM_SYS_INIT_SP_ADDR		\
		(CONFIG_TEXT_BASE - CONFIG_SYS_MALLOC_LEN -\
			CONFIG_ENV_SIZE - GENERATED_GBL_DATA_SIZE)

#define CONFIG_MACH_TYPE		(g_board_machid)
#define CFG_SYS_SDRAM_BASE		0x40000000
#define KERNEL_START_ADDR		CFG_SYS_SDRAM_BASE
#define CFG_ROOTFS_LOAD_ADDR		(CFG_SYS_SDRAM_BASE + (32 << 20))
#define FDT_HIGH			0x48500000
#endif /* _IPQ9574_H_ */
