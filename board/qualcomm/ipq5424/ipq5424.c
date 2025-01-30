// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2023-2025, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <mach/ipq.h>
#include <asm/io.h>
#include <asm/io.h>
#include <string.h>
#include <stdlib.h>
#include <fdtdec.h>
#include <config.h>

#include "ipq5424.h"


#define IM_SLEEP_CLK				0x1834020

void ipq_board_early_init_f(void)
{
	/*
	 * Enable the IM_SLEEP clock
	 * This clk require for Block control reset
	 */
	writel((readl(IM_SLEEP_CLK) | BIT(0)), IM_SLEEP_CLK);
}

#ifdef CONFIG_DTB_RESELECT
struct machid_dts_map machid_dts[] = {
	{ MACH_TYPE_IPQ5424_RDP464, "ipq5424-rdp464"},
	{ MACH_TYPE_IPQ5424_RDP464_C2, "ipq5424-rdp464-c2"},
	{ MACH_TYPE_IPQ5424_RDP466, "ipq5424-rdp466"},
	{ MACH_TYPE_IPQ5424_RDP466_C2, "ipq5424-rdp466-c2"},
	{ MACH_TYPE_IPQ5424_RDP466_C3, "ipq5424-rdp466"},
	{ MACH_TYPE_IPQ5424_RDP485, "ipq5424-rdp485"},
	{ MACH_TYPE_IPQ5424_RDP485_C2, "ipq5424-rdp485-c2"},
	{ MACH_TYPE_IPQ5424_RDP485_C3, "ipq5424-rdp485"},
	{ MACH_TYPE_IPQ5424_RDP487, "ipq5424-rdp487"},
	{ MACH_TYPE_IPQ5424_RDP466_RFFE, "ipq5424-rdp466"},
	{ MACH_TYPE_IPQ5424_RDP496, "ipq5424-rdp485"},
	{ MACH_TYPE_IPQ5424_RDP485_RFFE, "ipq5424-rdp485"},
	{ MACH_TYPE_IPQ5424_RDP485_RFFE_C2, "ipq5424-rdp485-c2"},
	{ MACH_TYPE_IPQ5424_DB_MR01_1, "ipq5424-db-mr01.1"},
};

struct multidtb_config ipq5424_dtb_info = {
	.list = machid_dts,
	.ncount = ARRAY_SIZE(machid_dts),
};

struct multidtb_config *g_board_dtb_info = &ipq5424_dtb_info;

void ipq_update_board_name(int machid, struct multidtb_config *dtb)
{
	switch(machid) {
	case MACH_TYPE_IPQ5424_RDP466_C3:
		strlcpy(dtb->dts_name, "ipq5424-rdp466-c3",
			BOARD_DTS_MAX_NAMELEN);
	break;
	case MACH_TYPE_IPQ5424_RDP485_C3:
		strlcpy(dtb->dts_name, "ipq5424-rdp485-c3",
			BOARD_DTS_MAX_NAMELEN);
	break;
	case MACH_TYPE_IPQ5424_RDP466_RFFE:
		strlcpy(dtb->dts_name, "ipq5424-rdp466-rffe",
			BOARD_DTS_MAX_NAMELEN);
	break;
	case MACH_TYPE_IPQ5424_RDP496:
		strlcpy(dtb->dts_name, "ipq5424-rdp496",
			BOARD_DTS_MAX_NAMELEN);
	break;
	case MACH_TYPE_IPQ5424_RDP485_RFFE:
		strlcpy(dtb->dts_name, "ipq5424-rdp485-rffe",
			BOARD_DTS_MAX_NAMELEN);
	break;
	case MACH_TYPE_IPQ5424_RDP485_RFFE_C2:
		strlcpy(dtb->dts_name, "ipq5424-rdp485-rffe-c2",
			BOARD_DTS_MAX_NAMELEN);
	break;
	default:
		strlcpy(dtb->dts_name, dtb->dts_base, BOARD_DTS_MAX_NAMELEN);
	}

}
#endif /* CONFIG_DTB_RESELECT */
