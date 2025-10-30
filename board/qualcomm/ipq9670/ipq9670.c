// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <asm/io.h>
#include <string.h>
#include <stdlib.h>
#include <mach/ipq.h>
#include <fdtdec.h>
#include <config.h>
#include <mtd_node.h>
#include <jffs2/load_kernel.h>

#define IM_SLEEP_CLK				0x1834020
/* MACH IDs for various RDPs */
#define MACH_TYPE_IPQ9670_EMULATION		0xF060000

struct dts_fixup ipq9670_mmc_fixup [] = {
	{ "/soc@0/nand@79b0000/", {"/soc@0/nand@79b0000/%status%?disabled"},1},
	{ "/soc@0/mmc@7804000/", {"/soc@0/mmc@7804000/%status%?okay"}, 1},
	{ "/soc/nand@79b0000/", {"/soc/nand@79b0000/%status%?disabled"},1},
	{ "/soc/sdhci@7804000/", {"/soc/sdhci@7804000/%status%?okay"}, 1},
	{}
};

struct dts_fixup *mmc_fixup = ipq9670_mmc_fixup;

struct dts_fixup ipq9670_usb_fixup [] = {
	{ "/soc@0/usb3@8a00000/dwc3@8a00000/",
		{"/soc@0/usb3@8a00000/dwc3@8a00000%dr_mode%?peripheral",
		"/soc@0/usb3@8a00000/dwc3@8a00000%maximum-speed%?high-speed"},
		2},
	{ "/soc/usb3@8A00000/dwc3@8A00000/",
		{ "/soc/usb3@8A00000/dwc3@8A00000%dr_mode%?peripheral",
		"/soc/usb3@8A00000/dwc3@8A00000%maximum-speed%?high-speed"},
		2},
	{}
};

struct dts_fixup *usb_fixup = ipq9670_usb_fixup;

#if CONFIG_FDT_FIXUP_PARTITIONS
struct node_info ipq_fnodes[] = {
	{ "n25q128a11", MTD_DEV_TYPE_NOR},
	{ "micron,n25q128a11", MTD_DEV_TYPE_NOR},
	{ "spansion,s25fs128s1", MTD_DEV_TYPE_NOR},
	{ "qcom,ipq9670-nand", MTD_DEV_TYPE_NAND},
};

int ipq_fnode_entires = ARRAY_SIZE(ipq_fnodes);

struct node_info * fnodes = ipq_fnodes ;
int * fnode_entires = &ipq_fnode_entires;
#endif

#ifdef CONFIG_DTB_RESELECT
struct machid_dts_map machid_dts[] = {
	{
		MACH_TYPE_IPQ9670_EMULATION,
		"ipq9670-emulation",
		"emulation-fbc",
		NULL
	},
};

struct multidtb_config ipq9670_dtb_info = {
	.list = machid_dts,
	.ncount = ARRAY_SIZE(machid_dts),
	.index = 0,
};

struct multidtb_config *g_board_dtb_info = &ipq9670_dtb_info;

void ipq_update_board_name(int machid, struct multidtb_config *dtb)
{
	switch(machid) {
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

void reset_cpu(void)
{
#ifdef CONFIG_IPQ_CRASHDUMP
	reset_crashdump(RESET_V1);
#endif
	psci_sys_reset(SYSRESET_COLD);
}
