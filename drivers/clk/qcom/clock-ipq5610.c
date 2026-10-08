// SPDX-License-Identifier: GPL-2.0
/*
 * Clock drivers for Qualcomm ipq5610
 *
 * (C) Copyright 2024 Linaro Ltd.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/types.h>
#include <clk-uclass.h>
#include <dm.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <linux/bug.h>
#include <linux/bitops.h>
#include <dm/device-internal.h>
#include <asm/arch/dt-bindings/clock/qcom,ipq5610-gcc.h>
#include <asm/arch/dt-bindings/reset/qcom,ipq5610-gcc.h>
#include "clock-qcom.h"

#define	GCC_QUPV3_UART1_CMD_RCGR		0x5004
#define GCC_SDCC1_APPS_CMD_RCGR			0x33004
#define GCC_QUPV3_SPI0_CMD_RCGR			0x02018
#define GCC_QPIC_CMD_RCGR			0x32020
#define GCC_QPIC_IO_MACRO_CMD_RCGR		0x32004
#define GCC_QUPV3_2X_CORE_CMD_RCGR		0x0100C
#define GCC_PCNOC_BFDCD_CMD_RCGR		0x31004
#define GCC_SYSTEM_NOC_BFDCD_CMD_RCGR		0x2E004
#define GCC_ANOC0_AXI_BFDCD_CMD_RCGR		0x2E088
#define GCC_QDSS_TSCTR_CMD_RCGR			0x2D01C
#define GCC_XO_CMD_RCGR				0x34004
#define GCC_APSS_AHB_CMD_RCGR			0x2400C
#define GCC_APSS_AXI_CMD_RCGR			0x24004
#define GCC_QDSS_AT_CMD_RCGR			0x2D004

/* Clock source selections */
#define GCC_QPIC_SRC_SEL_GPLL0_OUT_MAIN			BIT(8)
#define GCC_QPIC_IO_MACRO_SRC_SEL_GPLL0_OUT_MAIN	BIT(8)
#define GCC_QUPV3_2X_CORE_SRC_SEL_GPLL0_OUT_MAIN	BIT(8)
#define GCC_ANOC0_AXI_BFDCD_SRC_SEL_GPLL4_OUT_MAIN	(2 << 8)
#define GCC_QDSS_TSCTR_SRC_SEL_GPLL4_OUT_MAIN		BIT(8)
#define GCC_XO_SRC_SEL_XO				(0 << 8)
#define GCC_QDSS_AT_SRC_SEL_GPLL4_OUT_MAIN		BIT(8)
#define GCC_APSS_AHB_SRC_SEL_GPLL0_OUT_MAIN		BIT(8)
#define GCC_APSS_AXI_SRC_SEL_GPLL0_OUT_MAIN		BIT(8)
#define GCC_PCNOC_BFDCD_SRC_SEL_GPLL0_OUT_MAIN		BIT(8)
#define GCC_SYSTEM_NOC_BFDCD_SRC_SEL_GPLL4_OUT_MAIN	(2 << 8)

/* IO Macro clock rates */
#define IO_MACRO_CLK_400_MHZ				(400000000)
#define IO_MACRO_CLK_320_MHZ				(320000000)
#define IO_MACRO_CLK_266_MHZ				(266000000)
#define IO_MACRO_CLK_228_MHZ				(228000000)
#define IO_MACRO_CLK_200_MHZ				(200000000)
#define IO_MACRO_CLK_100_MHZ				(100000000)
#define IO_MACRO_CLK_50_MHZ				(50000000)
#define IO_MACRO_CLK_24_MHZ				(24000000)

int msm_set_parent(struct clk *clk, struct clk *parent)
{
	assert(clk);
	assert(parent);
	clk->dev->parent = parent->dev;
	dev_set_uclass_priv(parent->dev, parent);
	return 0;
}

ulong msm_get_rate(struct clk *clk)
{
	return (ulong)clk->rate;
}

static ulong ipq5610_set_rate(struct clk *clk, ulong rate)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);

	switch (clk->id) {
	case GCC_QUPV3_UART1_CLK:
		clk_rcg_set_rate_mnd(priv->base, GCC_QUPV3_UART1_CMD_RCGR,
				     0x19, 0, 0, CFG_CLK_SRC_CXO, 16);
		break;
	case GCC_SDCC1_APPS_CLK:
		clk_rcg_set_rate_mnd(priv->base, GCC_SDCC1_APPS_CMD_RCGR,
				     5, 0, 0, CFG_CLK_SRC_GPLL2, 16);
		break;
	case GCC_QUPV3_SPI0_CLK:
		clk_rcg_set_rate_mnd(priv->base, GCC_QUPV3_SPI0_CMD_RCGR,
				     31, 0, 0, CFG_CLK_SRC_GPLL0, 16);
		break;
	case GCC_QPIC_CLK:
		/* GCC_QPIC_CLK: 100 MHz  */
		clk_rcg_set_rate_v2(priv->base, GCC_QPIC_CMD_RCGR,
				    0, 0xF, 0, GCC_QPIC_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_QPIC_IO_MACRO_CLK:
		{
			int src = GCC_QPIC_IO_MACRO_SRC_SEL_GPLL0_OUT_MAIN;
			int div = 0;
			switch (rate) {
			case IO_MACRO_CLK_24_MHZ:
				src = CFG_CLK_SRC_CXO;
				div = 0;
				break;
			case IO_MACRO_CLK_50_MHZ:
				div = 31;
				break;
			case IO_MACRO_CLK_100_MHZ:
				div = 15;
				break;
			case IO_MACRO_CLK_200_MHZ:
				div = 7;
				break;
			case IO_MACRO_CLK_228_MHZ:
				div = 6;
				break;
			case IO_MACRO_CLK_266_MHZ:
				div = 5;
				break;
			case IO_MACRO_CLK_320_MHZ:
				div = 4;
				break;
			case IO_MACRO_CLK_400_MHZ:
				div = 3;
				break;
			default:
				return -EINVAL;
			}
			clk_rcg_set_rate_v2(priv->base, GCC_QPIC_IO_MACRO_CMD_RCGR,
					    0, div, 0, src);
		}
		break;
	case GCC_QUPV3_2X_CORE_CLK:
		/* GCC_QUPV3_2X_CORE_CLK: 200 MHz  */
		clk_rcg_set_rate_v2(priv->base, GCC_QUPV3_2X_CORE_CMD_RCGR,
				    0, 7, 0,
				    GCC_QUPV3_2X_CORE_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_PCNOC_BFDCD_CLK:
		/* GCC_PCNOC_BFDCD_CLK: 100 MHz  */
		clk_rcg_set_rate_v2(priv->base, GCC_PCNOC_BFDCD_CMD_RCGR,
				    0, 0xF, 0,
				    GCC_PCNOC_BFDCD_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_SYSTEM_NOC_BFDCD_CLK:
		/* GCC_SYSTEM_NOC_BFDCD_CLK: 266 MHz  */
		clk_rcg_set_rate_v2(priv->base, GCC_SYSTEM_NOC_BFDCD_CMD_RCGR,
				    0, 8, 0,
				    GCC_SYSTEM_NOC_BFDCD_SRC_SEL_GPLL4_OUT_MAIN);
		break;
	case GCC_ANOC0_AXI_BFDCD_CLK:
		/* GCC_ANOC0_AXI_BFDCD_CLK: 342 MHz  */
		clk_rcg_set_rate_v2(priv->base, GCC_ANOC0_AXI_BFDCD_CMD_RCGR,
				    0, 6, 0,
				    GCC_ANOC0_AXI_BFDCD_SRC_SEL_GPLL4_OUT_MAIN);
		break;
	case GCC_QDSS_TSCTR_CLK:
		/* GCC_QDSS_TSCTR_CLK: 600 MHz  */
		clk_rcg_set_rate_v2(priv->base, GCC_QDSS_TSCTR_CMD_RCGR,
				    0, 3, 0,
				    GCC_QDSS_TSCTR_SRC_SEL_GPLL4_OUT_MAIN);
		break;
	case GCC_XO_CLK:
		/* GCC_XO_CLK: 24 MHz  */
		clk_rcg_set_rate_v2(priv->base, GCC_XO_CMD_RCGR,
				    0, 1, 0,
				    GCC_XO_SRC_SEL_XO);
		break;
	case GCC_QDSS_AT_CLK:
		/* GCC_QDSS_AT_CLK: 240 MHz  */
		clk_rcg_set_rate_v2(priv->base, GCC_QDSS_AT_CMD_RCGR,
				    0, 9, 0,
				    GCC_QDSS_AT_SRC_SEL_GPLL4_OUT_MAIN);
		break;
	case GCC_APSS_AHB_CLK:
		/* GCC_APSS_AHB_CLK: 100 MHz  */
		clk_rcg_set_rate_v2(priv->base, GCC_APSS_AHB_CMD_RCGR,
				    0, 0xF, 0,
				    GCC_APSS_AHB_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_APSS_AXI_CLK:
		/* GCC_APSS_AXI_CLK: 800 MHz  */
		clk_rcg_set_rate_v2(priv->base, GCC_APSS_AXI_CMD_RCGR,
				    0, 1, 0,
				    GCC_APSS_AXI_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	default:
		return -EINVAL;
	}

	return rate;
}

static const struct gate_clk ipq5610_clks[] = {
	GATE_CLK(GCC_QUPV3_AHB_MST_CLK,		0x01014, 0x00000001),
	GATE_CLK(GCC_QUPV3_AHB_SLV_CLK,		0x0102C, 0x00000001),
	GATE_CLK(GCC_QUPV3_UART1_CLK,		0x5020, 0x00000001),
	GATE_CLK(GCC_SDCC1_AHB_CLK,		0x3303C, 0x00000001),
	GATE_CLK(GCC_SDCC1_APPS_CLK,		0x3302C, 0x00000001),
	GATE_CLK(GCC_QUPV3_SPI0_CLK,		0x0202C, 0x00000001),
	GATE_CLK(GCC_QUPV3_2X_CORE_CLK,		0x01020, 0x00000001),
	GATE_CLK(GCC_QUPV3_CORE_CLK,		0x01018, 0x00000001),
	GATE_CLK(GCC_QUPV3_SLEEP_CLK,		0x01028, 0x00000001),
	GATE_CLK(GCC_QPIC_CLK,			0x32028, 0x00000001),
	GATE_CLK(GCC_QPIC_IO_MACRO_CLK,		0x3200C, 0x00000001),
	GATE_CLK(GCC_QPIC_AHB_CLK,		0x32010, 0x00000001),
	GATE_CLK(GCC_APSS_DBG_CLK, 0x2402C, 0x00000001),
	GATE_CLK(GCC_MPM_AHB_CLK, 0x0B004, 0x00002000),
	GATE_CLK(GCC_GEMNOC_ANOC_CLK, 0x19044, 0x00000001),
	GATE_CLK(GCC_ANOC0_AXI_CLK, 0x2E0B4, 0x00000001),
	GATE_CLK(GCC_APSS_TS_CLK, 0x24030, 0x00000001),
	GATE_CLK(GCC_SNOC_QOSGEN_EXTREF_CLK, 0x2E020, 0x00000001),
	GATE_CLK(GCC_SYS_NOC_AT_CLK, 0x2E038, 0x00000001),
	GATE_CLK(GCC_SNOC_XO_DCD_CLK, 0x2E060, 0x00000001),
	GATE_CLK(GCC_SNOC_TS_CLK, 0x2E068, 0x00000001),
	GATE_CLK(GCC_PCNOC_AT_CLK, 0x31024, 0x00000001),
	GATE_CLK(GCC_PCNOC_TS_CLK, 0x3102C, 0x00000001),
	GATE_CLK(GCC_CNOC_QOSGEN_EXTREF_CLK, 0x310B0, 0x00000001),
	GATE_CLK(GCC_AGGRNOC_ATB_CLK, 0x2E0AC, 0x00000001),
	GATE_CLK(GCC_AGGRNOC_TS_CLK, 0x2E0B0, 0x00000001),
	GATE_CLK(GCC_ANOC1_AXI_CLK, 0x2E0B8, 0x00000001),
	GATE_CLK(GCC_ANOC_QOSGEN_EXTREF_CLK, 0x2E098, 0x00000001),
	GATE_CLK(GCC_ANOC_XO_DCD_CLK, 0x2E09C, 0x00000001),
	GATE_CLK(GCC_PCNOC_CE_EDPD_CLK, 0x31020, 0x00000001),
	GATE_CLK(GCC_CMN_LDO_CLK, 0x3A014, 0x00000001),
	GATE_CLK(GCC_SPDM_PCNOC_CY_CLK, 0x36008, 0x00000001),
	GATE_CLK(GCC_APCS_CLOCK_BRANCH_ENA_VOTE, 0x0B004, 0x00000040),
	GATE_CLK(GCC_DDRSS_SMS_SLOW_CLK, 0x1902C, 0x00000001),
	GATE_CLK(GCC_GEMNOC_SNOC_CLK, 0x19018, 0x00000001),
	GATE_CLK(GCC_GEMNOC_ANOC_PCIE_CLK, 0x19078, 0x00000001),
	GATE_CLK(GCC_GEMNOC_TCU_CLK, 0x19058, 0x00000001),
	GATE_CLK(GCC_SHRM_CLK, 0x19094, 0x00000001),
	GATE_CLK(GCC_DDRSS_AHB_CLK, 0x19008, 0x00000001),
	GATE_CLK(GCC_DDRSS_TRIG_CLK, 0x19098, 0x00000001),
	GATE_CLK(GCC_DDRSS_APB_CLK, 0x1909C, 0x00000001),
	GATE_CLK(GCC_GEMNOC_QOSGEN_EXTREF_CLK, 0x19024, 0x00000001),
	GATE_CLK(GCC_DDRSS_LLCC_ATB_CLK, 0x1904C, 0x00000001),
	GATE_CLK(GCC_GEMNOC_XO_DBG_CLK, 0x19040, 0x00000001),
	GATE_CLK(GCC_GEMNOC_NSSNOC_CLK, 0x19014, 0x00000001),
	GATE_CLK(GCC_GEMNOC_CNOC_CLK, 0x19038, 0x00000001),
	GATE_CLK(GCC_LLCC_TPDM_CFG_CLK, 0x19050, 0x00000001),
	GATE_CLK(GCC_TURING_GEMNOC_CLK, 0x1907C, 0x00000001),
	GATE_CLK(GCC_GEMNOC_TS_CLK, 0x19028, 0x00000001),
	GATE_CLK(GCC_DDRSS_ATB_CLK, 0x19004, 0x00000001),
	GATE_CLK(GCC_DDRSS_MC_AHB_CLK, 0x19020, 0x00000001),
	GATE_CLK(GCC_GEMNOC_AHB_CLK, 0x1900C, 0x00000001),
	GATE_CLK(GCC_QMIP_CORE_CBCR, 0x0A00C, 0x00000001),
};

static int ipq5610_enable(struct clk *clk)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);

	if (priv->data->num_clks <= clk->id) {
		debug("%s: unknown clk id %lu\n", __func__, clk->id);
		return 0;
	}

	debug("%s: clk %s\n", __func__, ipq5610_clks[clk->id].name);

	qcom_gate_clk_en(priv, clk->id);

	return 0;
}

static const struct qcom_reset_map ipq5610_gcc_resets[] = {
	[GCC_SDCC_BCR] = {0x33000, 0},
};

static struct msm_clk_data ipq5610_gcc_data = {
	.resets = ipq5610_gcc_resets,
	.num_resets = ARRAY_SIZE(ipq5610_gcc_resets),
	.clks = ipq5610_clks,
	.num_clks = ARRAY_SIZE(ipq5610_clks),
	.enable = ipq5610_enable,
	.set_rate = ipq5610_set_rate,
};

static const struct udevice_id gcc_ipq5610_of_match[] = {
	{
		.compatible = "qcom,ipq5610-gcc",
		.data = (ulong)&ipq5610_gcc_data,
	},
	{ }
};

U_BOOT_DRIVER(gcc_ipq5610) = {
	.name		= "gcc_ipq5610",
	.id		= UCLASS_NOP,
	.of_match	= gcc_ipq5610_of_match,
	.bind		= qcom_cc_bind,
	.flags		= DM_FLAG_PRE_RELOC | DM_FLAG_DEFAULT_PD_CTRL_OFF,
};
