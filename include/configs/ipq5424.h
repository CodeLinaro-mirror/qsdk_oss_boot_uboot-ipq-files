// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023-2025, Qualcomm Innovation Center,Inc.All rights reserved.
 */

#ifndef _IPQ5424_H_
#define _IPQ5424_H_

#ifndef __ASSEMBLY__
#include <linux/types.h>
extern uint32_t g_board_machid;
#endif

#define CONFIG_HAS_CUSTOM_SYS_INIT_SP_ADDR
#define CONFIG_CUSTOM_SYS_INIT_SP_ADDR		\
		(CONFIG_TEXT_BASE - CONFIG_SYS_MALLOC_LEN -\
			CONFIG_ENV_SIZE - GENERATED_GBL_DATA_SIZE)

#define CONFIG_MACH_TYPE                        (g_board_machid)
#define CFG_SYS_SDRAM_BASE			0x80000000

#endif /* _IPQ5424_H_ */
