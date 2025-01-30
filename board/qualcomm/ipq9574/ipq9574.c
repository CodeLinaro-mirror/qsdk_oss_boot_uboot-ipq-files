// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2023-2025, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <asm/io.h>
#include <string.h>
#include <stdlib.h>
#include <mach/ipq.h>
#include <fdtdec.h>
#include <config.h>

#include "ipq9574.h"

#ifdef CONFIG_DTB_RESELECT
struct machid_dts_map machid_dts[] = {
	{ MACH_TYPE_IPQ9574_RDP417, "ipq9574-rdp417"},
	{ MACH_TYPE_IPQ9574_RDP418, "ipq9574-rdp418"},
	{ MACH_TYPE_IPQ9574_RDP418_EMMC, "ipq9574-rdp418"},
	{ MACH_TYPE_IPQ9574_RDP437, "ipq9574-rdp437"},
	{ MACH_TYPE_IPQ9574_RDP433, "ipq9574-rdp433"},
	{ MACH_TYPE_IPQ9574_RDP433_SFP, "ipq9574-rdp433-sfp"},
	{ MACH_TYPE_IPQ9574_RDP449, "ipq9574-rdp449" },
	{ MACH_TYPE_IPQ9574_RDP433_MHT_PHY, "ipq9574-rdp433-mht-phy"},
	{ MACH_TYPE_IPQ9574_RDP453, "ipq9574-rdp453"},
	{ MACH_TYPE_IPQ9574_RDP454, "ipq9574-rdp454"},
	{ MACH_TYPE_IPQ9574_RDP433_MHT_SWT, "ipq9574-rdp433-mht-switch"},
	{ MACH_TYPE_IPQ9574_RDP467, "ipq9574-rdp467" },
	{ MACH_TYPE_IPQ9574_RDP455_C11, "ipq9574-rdp455"},
	{ MACH_TYPE_IPQ9574_RDP455_C12, "ipq9574-rdp455"},
	{ MACH_TYPE_IPQ9574_RDP459, "ipq9574-rdp459"},
	{ MACH_TYPE_IPQ9574_RDP457, "ipq9574-rdp457" },
	{ MACH_TYPE_IPQ9574_RDP456, "ipq9574-rdp456" },
	{ MACH_TYPE_IPQ9574_RDP458, "ipq9574-rdp458" },
	{ MACH_TYPE_IPQ9574_RDP469, "ipq9574-rdp469"},
	{ MACH_TYPE_IPQ9574_RDP461, "ipq9574-rdp461"},
	{ MACH_TYPE_IPQ9574_RDP475, "ipq9574-rdp475"},
	{ MACH_TYPE_IPQ9574_RDP475_QCA81XX, "ipq9574-rdp475-qca81xx"},
	{ MACH_TYPE_IPQ9574_RDP475_QCA81XX_I2C, "ipq9574-rdp475-qca81xx-i2c"},
	{ MACH_TYPE_IPQ9574_RDP476, "ipq9574-rdp476"},
	{ MACH_TYPE_IPQ9574_DB_AL01_C1, "ipq9574-db-al01-c1"},
	{ MACH_TYPE_IPQ9574_DB_AL01_C2, "ipq9574-db-al01-c1"},
	{ MACH_TYPE_IPQ9574_DB_AL01_C3, "ipq9574-db-al01-c3"},
	{ MACH_TYPE_IPQ9574_DB_AL02_C1, "ipq9574-db-al02-c1"},
	{ MACH_TYPE_IPQ9574_DB_AL02_C2, "ipq9574-db-al02-c1"},
	{ MACH_TYPE_IPQ9574_DB_AL02_C3, "ipq9574-db-al02-c3"},
};

struct multidtb_config ipq9574_dtb_info = {
	.list = machid_dts,
	.ncount = ARRAY_SIZE(machid_dts),
};

struct multidtb_config *g_board_dtb_info = &ipq9574_dtb_info;

void ipq_update_board_name(int machid, struct multidtb_config *dtb)
{
	switch(machid) {
	case MACH_TYPE_IPQ9574_RDP455_C11:
		strlcpy(dtb->dts_name, "ipq9574-rdp455-c11",
			BOARD_DTS_MAX_NAMELEN);
	break;
	case MACH_TYPE_IPQ9574_RDP455_C12:
		strlcpy(dtb->dts_name, "ipq9574-rdp455-c12",
			BOARD_DTS_MAX_NAMELEN);
	break;
	default:
		strlcpy(dtb->dts_name, dtb->dts_base, BOARD_DTS_MAX_NAMELEN);
	}

}
#endif /* CONFIG_DTB_RESELECT */
