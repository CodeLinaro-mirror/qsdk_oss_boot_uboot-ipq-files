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

#define IM_SLEEP_CLK				0x1834020
#define MACH_TYPE_IPQ5424_RDP464_C2		0x8070000
#define MACH_TYPE_IPQ5424_RDP464		0x8070001
#define MACH_TYPE_IPQ5424_RDP466_C2		0x8070100
#define MACH_TYPE_IPQ5424_RDP466_C3		0x8070102
#define MACH_TYPE_IPQ5424_RDP466		0x8071100
#define MACH_TYPE_IPQ5424_RDP496		0x8071110
#define MACH_TYPE_IPQ5424_RDP466_RFFE		0x8070110
#define MACH_TYPE_IPQ5424_RDP485_C3		0x8070103
#define MACH_TYPE_IPQ5424_RDP485_C2		0x8070101
#define MACH_TYPE_IPQ5424_RDP485		0x8071101
#define MACH_TYPE_IPQ5424_RDP485_RFFE_C2	0x8071111
#define MACH_TYPE_IPQ5424_RDP485_RFFE		0x8070111
#define MACH_TYPE_IPQ5424_RDP487		0x8070200
#define MACH_TYPE_IPQ5424_DB_MR01_1		0x1070000

struct dts_fixup ipq5424_mmc_fixup[] = {
	{ "/soc@0/nand@79b0000/", {"/soc@0/nand@79b0000/%status%?disabled"}, 1},
	{ "/soc@0/mmc@7804000/", {"/soc@0/mmc@7804000/%status%?okay"}, 1},
	{NULL}
};

struct dts_fixup *mmc_fixup = ipq5424_mmc_fixup;

struct dts_fixup ipq5424_usb_fixup[] = {
	{ "/soc@0/usb3@8a00000/dwc3@8a00000/",
		{"/soc@0/usb3@8a00000/dwc3@8a00000%dr_mode%?peripheral",
		"/soc@0/usb3@8a00000/dwc3@8a00000%maximum-speed%?high-speed"},
		2},
	{ "/soc@0/usb2@1e00000/dwc3@1e00000/",
		{ "/soc@0/usb2@1e00000/dwc3@1e00000%dr_mode%?peripheral",
		"/soc@0/usb2@1e00000/dwc3@1e00000%maximum-speed%?high-speed"},
		2},
	{NULL}
};

struct dts_fixup *usb_fixup = ipq5424_usb_fixup;

#ifdef CONFIG_DTB_RESELECT
struct machid_dts_map machid_dts[] = {
	{
		MACH_TYPE_IPQ5424_RDP464,
		"ipq5424-rdp464",
		"rdp464",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP464_C2,
		"ipq5424-rdp464-c2",
		"rdp464-c2",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP466,
		"ipq5424-rdp466",
		"rdp466",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP466_C2,
		"ipq5424-rdp466-c2",
		"rdp466-c2",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP466_C3,
		"ipq5424-rdp466",
		"rdp466",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP485,
		"ipq5424-rdp485",
		"rdp485",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP485_C2,
		"ipq5424-rdp485-c2",
		"rdp485-c2",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP485_C3,
		"ipq5424-rdp485",
		"rdp485-c3",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP487,
		"ipq5424-rdp487",
		"rdp487",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP466_RFFE,
		"ipq5424-rdp466",
		"rdp466-rffe",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP496,
		"ipq5424-rdp485",
		"rdp496",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP485_RFFE,
		"ipq5424-rdp485",
		"rdp485-rffe",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP485_RFFE_C2,
		"ipq5424-rdp485-c2",
		"rdp485-rffe-c2",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_DB_MR01_1,
		"ipq5424-db-mr01.1",
		"db-mr01.1",
		NULL
	},
};

struct multidtb_config ipq5424_dtb_info = {
	.list = machid_dts,
	.ncount = ARRAY_SIZE(machid_dts),
	.index = 0,
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

void ipq_board_early_init_f(void)
{
	/*
	 * Enable the IM_SLEEP clock
	 * This clk require for Block control reset
	 */
	writel((readl(IM_SLEEP_CLK) | BIT(0)), IM_SLEEP_CLK);
}

/**
 * ipq_read_tcsr_boot_misc() - read boot tcsr register
 */
__weak int ipq_read_tcsr_boot_misc(void)
{
	u32 *dmagic = TCSR_BOOT_MISC_REG;

	return *dmagic;
}

bool is_atf_enbled(void)
{
	uint32_t atf_status = 0;

	ipq_smem_get_item((void *)&atf_status, SMEM_ATF_ENABLE, 0,
				sizeof(uint32_t));

	return atf_status ? true : false;
}

uint32_t is_board_support_image_auth(void)
{

	uint32_t board_type = gd->board_type;
	uint32_t ret = 0;

	switch (gd->ram_size) {
	case SZ_128M:
		break;
	default:
		ret = (board_type & SECURE_BOARD);
		break;
	}

	return ret;
}
