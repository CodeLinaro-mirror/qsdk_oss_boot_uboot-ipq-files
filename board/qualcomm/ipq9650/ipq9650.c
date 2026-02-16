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
#include <linux/delay.h>

#define IM_SLEEP_CLK				0x1834020
/* MACH IDs for various RDPs */
#define MACH_TYPE_IPQ9650_EMULATION		0xF060000

static struct crashdump_infos dumpinfo_n[] = {
	{
		/* DDR Bank 0 */
		.name = "EBICS.BIN",
		.start_addr = CFG_SYS_SDRAM_BASE,
		.size = 0xBAD0FF5E,
		.dump_level = FULLDUMP,
		.split_bin_sz = SZ_1G,
		.is_aligned_access = false,
		.compression_support = true
	},
	{
		/* DDR Bank 1 */
		.name = "EBICS.BIN",
		.start_addr = 0xBAD0FF5E,
		.size = 0xBAD0FF5E,
		.dump_level = FULLDUMP,
		.split_bin_sz = SZ_1G,
		.is_aligned_access = false,
		.compression_support = true
	},
	{
		.name = "IMEM.BIN",
		.start_addr = 0x08600000,
		.size = 0x00020000,
		.dump_level = FULLDUMP,
		.split_bin_sz = 0,
		.is_aligned_access = false,
		.compression_support = false
	},
	{
		.name = "CPU_INFO.BIN",
		.start_addr = 0x0,
		.size = 0xBAD0FF5E,
		.dump_level = MINIDUMP,
		.split_bin_sz = 0,
		.is_aligned_access = false,
		.compression_support = false,
		.dumptoflash_support = true
	},
	{
		.name = "UNAME.BIN",
		.start_addr = 0x0,
		.size = 0xBAD0FF5E,
		.dump_level = MINIDUMP,
		.split_bin_sz = 0,
		.is_aligned_access = false,
		.compression_support = false,
		.dumptoflash_support = true
	},
	{
		.name = "DMESG.BIN",
		.start_addr = 0x0,
		.size = 0xBAD0FF5E,
		.dump_level = MINIDUMP,
		.split_bin_sz = 0,
		.is_aligned_access = false,
		.compression_support = false,
		.dumptoflash_support = false
	},
	{
		.name = "PT.BIN",
		.start_addr = 0x0,
		.size = 0xBAD0FF5E,
		.dump_level = MINIDUMP,
		.split_bin_sz = 0,
		.is_aligned_access = false,
		.compression_support = false,
		.dumptoflash_support = false
	},
	{
		.name = "WLAN_MOD.BIN",
		.start_addr = 0x0,
		.size = 0xBAD0FF5E,
		.dump_level = MINIDUMP,
		.split_bin_sz = 0,
		.is_aligned_access = false,
		.compression_support = false,
		.dumptoflash_support = false
	},
};

static uint8_t dump_entries_n = ARRAY_SIZE(dumpinfo_n);

struct crashdump_infos *board_dumpinfo = dumpinfo_n;

uint8_t *board_dump_entries = &dump_entries_n;

struct dts_fixup ipq9650_mmc_fixup [] = {
	{ "/soc@0/nand@79b0000/", {"/soc@0/nand@79b0000/%status%?disabled"},1},
	{ "/soc@0/mmc@7804000/", {"/soc@0/mmc@7804000/%status%?okay"}, 1},
	{NULL}
};

struct dts_fixup *mmc_fixup = ipq9650_mmc_fixup;

struct dts_fixup ipq9650_usb_fixup [] = {
	{ "/soc@0/usb3@8a00000/dwc3@8a00000/",
		{"/soc@0/usb3@8a00000/dwc3@8a00000%dr_mode%?peripheral",
		"/soc@0/usb3@8a00000/dwc3@8a00000%maximum-speed%?high-speed"},
		2},
	{NULL}
};

struct dts_fixup *usb_fixup = ipq9650_usb_fixup;

#if CONFIG_FDT_FIXUP_PARTITIONS
struct node_info ipq_fnodes[] = {
	{ "n25q128a11", MTD_DEV_TYPE_NOR},
	{ "micron,n25q128a11", MTD_DEV_TYPE_NOR},
	{ "spansion,s25fs128s1", MTD_DEV_TYPE_NOR},
	{ "qcom,ipq9650-nand", MTD_DEV_TYPE_NAND},
};

int ipq_fnode_entires = ARRAY_SIZE(ipq_fnodes);

struct node_info * fnodes = ipq_fnodes ;
int * fnode_entires = &ipq_fnode_entires;
#endif

struct machid_dts_map machid_dts[] = {
	{
		MACH_TYPE_IPQ9650_EMULATION,
		"ipq9650-emulation",
		"emulation-fbc",
		"1",
	},
};

struct multidtb_config ipq9650_dtb_info = {
	.list = machid_dts,
	.ncount = ARRAY_SIZE(machid_dts),
	.index = 0,
};

struct multidtb_config *g_board_dtb_info = &ipq9650_dtb_info;

/* Board-Specific Communication Type Mapping */
const u8 comm_type_map[FUNC_MAX] = {
	[FUNC_LIST_FUSE]       = COMM_TYPE_TME,
	[FUNC_DUMP_FUSE]       = COMM_TYPE_TME,
	[FUNC_SECURE_AUTH]     = COMM_TYPE_TME,
	[FUNC_CHECK_SECURE_BOOT] = COMM_TYPE_TME,
	[FUNC_IMAGE_AUTH]      = COMM_TYPE_TME,
	[FUNC_AUTH_ROOTFS_ELF] = COMM_TYPE_TME
};

#ifdef CONFIG_DTB_RESELECT
void ipq_update_board_name(int machid, struct multidtb_config *dtb)
{
	switch(machid) {
	default:
		strlcpy(dtb->dts_name, dtb->dts_base, BOARD_DTS_MAX_NAMELEN);
	}

}
#endif /* CONFIG_DTB_RESELECT */


static void ipq_enable_im_sleep_clk(void)
{
    /*
     * Enable the IM_SLEEP clock
     * This clk require for Block control reset
     */
    writel((readl(IM_SLEEP_CLK) | BIT(0)), IM_SLEEP_CLK);
}

#if defined(CONFIG_SPL)
void ipq_spl_board_early_init_f(void)
{
    ipq_enable_im_sleep_clk();
}
#endif

void ipq_board_early_init_f(void)
{
    ipq_enable_im_sleep_clk();
}

#if defined(CONFIG_SPL)
void reset_cpu(void) __attribute__((noreturn));
void reset_cpu(void)
{
    printf("SPL reset: not implemented, hanging...\n");
    /* SPL reset: ToDo */
    while(1) {
        /* Add delay to prevent CPU spinning at 100% */
        udelay(1000000); /* 1 second delay */
    }
}
#else
void reset_cpu(void)
{
#ifdef CONFIG_IPQ_CRASHDUMP
	reset_crashdump(RESET_V1);
#endif
	psci_sys_reset(SYSRESET_COLD);
}
#endif

uint32_t is_board_support_image_auth(void)
{
	u32 board_type = gd->board_type;
	u32 ret = 0;

	switch (gd->ram_size) {
	case SZ_128M:
		break;
	default:
		ret = (board_type & SECURE_BOARD);
		break;
	}

	return ret;
}

void ipq_update_comm_type(void)
{
	struct ipq_board_info *bdinfo = ipq_get_bdinfo();

	if (bdinfo)
		bdinfo->comm_type_map = comm_type_map;
}
