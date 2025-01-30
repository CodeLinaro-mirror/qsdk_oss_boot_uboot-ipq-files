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
#include "ipq5332.h"

#ifdef CONFIG_DTB_RESELECT
struct machid_dts_map machid_dts[] = {
	{ MACH_TYPE_IPQ5332_RDP468, "ipq5332-rdp468"},
	{ MACH_TYPE_IPQ5332_RDP441, "ipq5332-rdp441"},
	{ MACH_TYPE_IPQ5332_RDP441_QCA81XX, "ipq5332-rdp441-qca81xx"},
	{ MACH_TYPE_IPQ5332_RDP441_QCA81XX_I2C, "ipq5332-rdp441-qca81xx-i2c"},
	{ MACH_TYPE_IPQ5332_RDP442, "ipq5332-rdp442"},
	{ MACH_TYPE_IPQ5332_RDP446, "ipq5332-rdp446"},
	{ MACH_TYPE_IPQ5332_RDP474, "ipq5332-rdp474"},
	{ MACH_TYPE_IPQ5332_RDP473, "ipq5332-rdp480"},
	{ MACH_TYPE_IPQ5332_RDP472, "ipq5332-rdp472"},
	{ MACH_TYPE_IPQ5332_RDP477, "ipq5332-rdp477"},
	{ MACH_TYPE_IPQ5332_RDP478, "ipq5332-rdp478"},
	{ MACH_TYPE_IPQ5332_RDP479, "ipq5332-rdp479"},
	{ MACH_TYPE_IPQ5332_RDP480, "ipq5332-rdp480"},
	{ MACH_TYPE_IPQ5332_RDP481, "ipq5332-rdp481"},
	{ MACH_TYPE_IPQ5332_RDP483, "ipq5332-rdp483"},
	{ MACH_TYPE_IPQ5332_RDP484, "ipq5332-rdp484"},
	{ MACH_TYPE_IPQ5332_RDP486, "ipq5332-rdp442"},
	{ MACH_TYPE_IPQ5332_RDP477_256M, "ipq5332-rdp477-256m"},
	{ MACH_TYPE_IPQ5332_RDP478_256M, "ipq5332-rdp478-256m"},
	{ MACH_TYPE_IPQ5332_DB_MI01_1, "ipq5332-db-mi01.1"},
	{ MACH_TYPE_IPQ5332_DB_MI02_1, "ipq5332-db-mi02.1"},
	{ MACH_TYPE_IPQ5332_DB_MI03_1, "ipq5332-db-mi03.1"},
	{ MACH_TYPE_IPQ5332_TB_MI03_1, "ipq5332-tb-mi03.1"},
	{ MACH_TYPE_IPQ5332_TB_MI05_1, "ipq5332-tb-mi05.1"},
};

struct multidtb_config ipq5332_dtb_info = {
	.list = machid_dts,
	.ncount = ARRAY_SIZE(machid_dts),
};

struct multidtb_config *g_board_dtb_info = &ipq5332_dtb_info;

void ipq_update_board_name(int machid, struct multidtb_config *dtb)
{
	switch(machid) {
	case MACH_TYPE_IPQ5332_RDP473:
		strlcpy(dtb->dts_name, "ipq5332-rdp473",
			BOARD_DTS_MAX_NAMELEN);
	break;
	case MACH_TYPE_IPQ5332_RDP486:
		strlcpy(dtb->dts_name, "ipq5332-rdp486",
			BOARD_DTS_MAX_NAMELEN);
	break;
	default:
		strlcpy(dtb->dts_name, dtb->dts_base, BOARD_DTS_MAX_NAMELEN);
	}

}
#endif /* CONFIG_DTB_RESELECT */
