// SPDX-License-Identifier: GPL-2.0
/*
 * Clock drivers for Qualcomm ipq5424
 *
 * (C) Copyright 2024 Linaro Ltd.
 * Copyright (c) 2025, Qualcomm Innovation Center,Inc.All rights reserved.
 */

#include <linux/types.h>
#include <clk-uclass.h>
#include <dm.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <linux/bug.h>
#include <linux/bitops.h>
#include <dm/device-internal.h>
#include <dt-bindings/clock/qcom,ipq5424-gcc.h>
#include <dt-bindings/reset/qcom,ipq5424-gcc.h>

#include "clock-qcom.h"

#define MHZ(X)					((X) * 1000000UL)
#define	GCC_QUPV3_UART1_CMD_RCGR		0x302C
#define GCC_QUPV3_UART1_CBCR			0x3040
#define GCC_SDCC1_APPS_CMD_RCGR			0x33004
#define GCC_SDCC1_APPS_CBCR			0x3302C
#define GCC_SDCC1_AHB_CBCR			0x33034
#define GCC_QUPV3_SPI0_CMD_RCGR			0x04004
#define GCC_QUPV3_SPI0_CBCR			0x04020
#define GCC_PCNOC_BFDCD_CMD_RCGR		(0x31004)
#define GCC_SYSTEM_NOC_BFDCD_CMD_RCGR		(0x2E004)
#define GCC_NSSNOC_MEMNOC_BFDCD_CMD_RCGR	(0x17004)
#define NSS_CC_PORT1_RX_CMD_RCGR		(0x004B4)
#define NSS_CC_PORT1_RX_CFG_RCGR		(0x004B8)
#define NSS_CC_PORT1_RX_DIV_CDIVR		(0x004BC)
#define NSS_CC_PORT1_TX_CMD_RCGR		(0x004C0)
#define NSS_CC_PORT1_TX_CFG_RCGR		(0x004C4)
#define NSS_CC_PORT1_TX_DIV_CDIVR		(0x004C8)
#define NSS_CC_PORT2_RX_CMD_RCGR		(0x004CC)
#define NSS_CC_PORT2_RX_CFG_RCGR		(0x004D0)
#define NSS_CC_PORT2_RX_DIV_CDIVR		(0x004D4)
#define NSS_CC_PORT2_TX_CMD_RCGR		(0x004D8)
#define NSS_CC_PORT2_TX_CFG_RCGR		(0x004DC)
#define NSS_CC_PORT2_TX_DIV_CDIVR		(0x004E0)
#define NSS_CC_PORT3_RX_CMD_RCGR		(0x004E4)
#define NSS_CC_PORT3_RX_CFG_RCGR		(0x004E8)
#define NSS_CC_PORT3_RX_DIV_CDIVR		(0x004EC)
#define NSS_CC_PORT3_TX_CMD_RCGR		(0x004F0)
#define NSS_CC_PORT3_TX_CFG_RCGR		(0x004F4)
#define NSS_CC_PORT3_TX_DIV_CDIVR		(0x004F8)
#define NSS_CC_PPE_CMD_RCGR			(0x003EC)
#define NSS_CC_PPE_CFG_RCGR			(0x003F0)
#define NSS_CC_CE_CMD_RCGR			(0x005E0)
#define NSS_CC_CE_CFG_RCGR			(0x005E4)
#define NSS_CC_CFG_CMD_RCGR			(0x006A8)
#define NSS_CC_CFG_CFG_RCGR			(0x006AC)
#define GCC_QUPV3_I2C0_CMD_RCGR			(0x02018)
#define GCC_QUPV3_I2C0_DIV_CDIVR		(0x02020)
#define GCC_QUPV3_I2C1_CMD_RCGR			(0x03018)
#define GCC_QUPV3_I2C1_DIV_CDIVR		(0x03020)
#define GCC_QUPV3_SPI1_CMD_RCGR			(0x05004)

#define NSS_CC_PPE_SRC_SEL_CMN_PLL_NSS_CLK_375M		(6 << 8)
#define NSS_CC_PPE_SRC_SEL_GCC_GPLL0_OUT_AUX		(2 << 8)
#define NSS_CC_PORT_RX_SRC_SEL_UNIPHY_NSS_RX_CLK	(3 << 8)
#define NSS_CC_PORT_TX_SRC_SEL_UNIPHY_NSS_TX_CLK	(4 << 8)

#define PCNOC_BFDCD_SRC_SEL_GPLL0_OUT_MAIN		BIT(8)
#define SYSTEM_NOC_BFDCD_SRC_SEL_GPLL4_OUT_MAIN		(2 << 8)
#define NSSNOC_MEMNOC_BFDCD_SRC_SEL_NSS_CMN_CLK		BIT(8)

static int calc_div_for_nss_port_clk(struct clk *clk, ulong rate,
				     int *div, int *cdiv)
{
	int pclk_rate = clk_get_parent_rate(clk);

	if (pclk_rate == CLK_125_MHZ) {
		switch (rate) {
		case CLK_2_5_MHZ:
			*div = 9;
			*cdiv = 9;
			break;
		case CLK_25_MHZ:
			*div = 9;
			break;
		case CLK_125_MHZ:
			*div = 1;
			break;
		default:
			return -EINVAL;
		}
	} else if (pclk_rate == CLK_312_5_MHZ) {
		switch (rate) {
		case CLK_2_5_MHZ:
			break;
		case CLK_12_5_MHZ:
			*div = 9;
			*cdiv = 4;
			break;
		case CLK_25_MHZ:
			break;
		case CLK_78_125_MHZ:
			*div = 7;
			break;
		case CLK_125_MHZ:
			*div = 4;
			break;
		case CLK_156_25_MHZ:
			*div = 3;
			break;
		case CLK_312_5_MHZ:
			*div = 1;
			break;
		default:
			return -EINVAL;
		}
	} else {
		return -EINVAL;
	};

	return 0;
}

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

static ulong ipq5424_set_rate(struct clk *clk, ulong rate)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);
	int ret, div = 0, cdiv = 0;

	switch (clk->id) {
	case GCC_QUPV3_I2C0_CLK:
		/* Default: 64MHz */
		clk_rcg_set_rate_v2(priv->base, GCC_QUPV3_I2C0_CMD_RCGR,
				    GCC_QUPV3_I2C0_DIV_CDIVR, 24, 1,
				    CFG_CLK_SRC_GPLL0);
		break;
	case GCC_QUPV3_I2C1_CLK:
		/* Default: 64MHz */
		clk_rcg_set_rate_v2(priv->base, GCC_QUPV3_I2C1_CMD_RCGR,
				    GCC_QUPV3_I2C1_DIV_CDIVR, 24, 1,
				    CFG_CLK_SRC_GPLL0);
		break;
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
	case GCC_QUPV3_SPI1_CLK:
		switch (rate) {
		case MHZ(32):
			clk_rcg_set_rate_mnd(priv->base, GCC_QUPV3_SPI1_CMD_RCGR,
					     24, 0, 0, CFG_CLK_SRC_GPLL0, 8);
			break;
		default:
			/* Default: 50MHz */
			clk_rcg_set_rate_mnd(priv->base, GCC_QUPV3_SPI1_CMD_RCGR,
					     16, 0, 0, CFG_CLK_SRC_GPLL0, 8);
		}
		break;
	case GCC_PCNOC_BFDCD_CLK:
		clk_rcg_set_rate_v2(priv->base, GCC_PCNOC_BFDCD_CMD_RCGR, 0,
				    15, 0,
				    PCNOC_BFDCD_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_SYSTEM_NOC_BFDCD_CLK:
		clk_rcg_set_rate_v2(priv->base, GCC_SYSTEM_NOC_BFDCD_CMD_RCGR, 0,
				    8, 0,
				    SYSTEM_NOC_BFDCD_SRC_SEL_GPLL4_OUT_MAIN);
		break;
	case GCC_NSSNOC_MEMNOC_BFDCD_CLK:
		clk_rcg_set_rate_v2(priv->base, GCC_NSSNOC_MEMNOC_BFDCD_CMD_RCGR, 0,
				    1, 0,
				    NSSNOC_MEMNOC_BFDCD_SRC_SEL_NSS_CMN_CLK);
		break;

	/* NSS clocks */
	case NSS_CC_PPE_CLK:
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PPE_CMD_RCGR, 0,
				    1, 0,
				    NSS_CC_PPE_SRC_SEL_CMN_PLL_NSS_CLK_375M);
		break;
	case NSS_CC_CE_CLK:
		clk_rcg_set_rate_v2(priv->base, NSS_CC_CE_CMD_RCGR, 0,
				    1, 0,
				    NSS_CC_PPE_SRC_SEL_CMN_PLL_NSS_CLK_375M);
		break;
	case NSS_CC_CFG_CLK:
		clk_rcg_set_rate_v2(priv->base, NSS_CC_CFG_CMD_RCGR, 0,
				    15, 0,
				    NSS_CC_PPE_SRC_SEL_GCC_GPLL0_OUT_AUX);
		break;
	case NSS_CC_PORT1_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT1_RX_CMD_RCGR,
				    NSS_CC_PORT1_RX_DIV_CDIVR,
				    div, cdiv,
				    NSS_CC_PORT_RX_SRC_SEL_UNIPHY_NSS_RX_CLK);
		break;
	case NSS_CC_PORT1_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT1_TX_CMD_RCGR,
				    NSS_CC_PORT1_TX_DIV_CDIVR,
				    div, cdiv,
				    NSS_CC_PORT_TX_SRC_SEL_UNIPHY_NSS_TX_CLK);
		break;
	case NSS_CC_PORT2_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT2_RX_CMD_RCGR,
				    NSS_CC_PORT2_RX_DIV_CDIVR,
				    div, cdiv,
				    NSS_CC_PORT_RX_SRC_SEL_UNIPHY_NSS_RX_CLK);
		break;
	case NSS_CC_PORT2_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT2_TX_CMD_RCGR,
				    NSS_CC_PORT2_TX_DIV_CDIVR,
				    div, cdiv,
				    NSS_CC_PORT_TX_SRC_SEL_UNIPHY_NSS_TX_CLK);
		break;
	case NSS_CC_PORT3_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT3_RX_CMD_RCGR,
				    NSS_CC_PORT3_RX_DIV_CDIVR,
				    div, cdiv,
				    NSS_CC_PORT_RX_SRC_SEL_UNIPHY_NSS_RX_CLK);
		break;
	case NSS_CC_PORT3_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT3_TX_CMD_RCGR,
				    NSS_CC_PORT3_TX_DIV_CDIVR,
				    div, cdiv,
				    NSS_CC_PORT_TX_SRC_SEL_UNIPHY_NSS_TX_CLK);
		break;
	case UNIPHY0_NSS_RX_CLK:
	case UNIPHY0_NSS_TX_CLK:
	case UNIPHY1_NSS_RX_CLK:
	case UNIPHY1_NSS_TX_CLK:
	case UNIPHY2_NSS_RX_CLK:
	case UNIPHY2_NSS_TX_CLK:
		if (rate == CLK_125_MHZ)
			clk->rate = CLK_125_MHZ;
		else if (rate == CLK_312_5_MHZ)
			clk->rate = CLK_312_5_MHZ;
		else
			ret = -EINVAL;
		break;
	default:
		return -EINVAL;
	}
	return rate;
}

static const struct gate_clk ipq5424_clks[] = {
	/*UART*/
	GATE_CLK(GCC_QUPV3_UART1_CLK,		0x03040, 0x00000001),
	/*SDHCI*/
	GATE_CLK(GCC_SDCC1_AHB_CLK,		0x3303C, 0x00000001),
	GATE_CLK(GCC_SDCC1_APPS_CLK,		0x3302C, 0x00000001),
	/*ETHERNET*/
	GATE_CLK(GCC_IM_SLEEP_CLK,		0x34020,  0x00000001),
	GATE_CLK(NSS_CC_NSS_CSR_CLK,		0x006B0,  0x00000001),
	GATE_CLK(NSS_CC_NSSNOC_NSS_CSR_CLK,	0x006B4,  0x00000001),
	GATE_CLK(NSS_CC_CE_APB_CLK,		0x005E8,  0x00000001),
	GATE_CLK(NSS_CC_CE_AXI_CLK,		0x005EC,  0x00000001),
	GATE_CLK(NSS_CC_NSSNOC_CE_APB_CLK,	0x005F4,  0x00000001),
	GATE_CLK(NSS_CC_NSSNOC_CE_AXI_CLK,	0x005F8,  0x00000001),
	GATE_CLK(GCC_CMN_12GPLL_AHB_CLK,	0x3A004,  0x00000001),
	GATE_CLK(GCC_CMN_12GPLL_SYS_CLK,	0x3A008,  0x00000001),
	GATE_CLK(GCC_NSSCC_CLK,			0x17034,  0x00000001),
	GATE_CLK(GCC_NSSCFG_CLK,		0x1702C,  0x00000001),
	GATE_CLK(GCC_NSSNOC_NSSCC_CLK,		0x17030,  0x00000001),
	GATE_CLK(GCC_NSSNOC_SNOC_CLK,		0x17028,  0x00000001),
	GATE_CLK(GCC_NSSNOC_SNOC_1_CLK,		0x1707C,  0x00000001),
	GATE_CLK(GCC_NSSNOC_MEMNOC_CLK,		0x17024,  0x00000001),
	GATE_CLK(GCC_NSSNOC_MEMNOC_1_CLK,	0x17084,  0x00000001),
	GATE_CLK(GCC_UNIPHY0_SYS_CLK,		0x17048,  0x00000001),
	GATE_CLK(GCC_UNIPHY1_SYS_CLK,		0x17058,  0x00000001),
	GATE_CLK(GCC_UNIPHY2_SYS_CLK,		0x17068,  0x00000001),
	GATE_CLK(GCC_UNIPHY0_AHB_CLK,		0x1704C,  0x00000001),
	GATE_CLK(GCC_UNIPHY1_AHB_CLK,		0x1705C,  0x00000001),
	GATE_CLK(GCC_UNIPHY2_AHB_CLK,		0x1706C,  0x00000001),
	GATE_CLK(NSS_CC_PORT1_MAC_CLK,		0x00428,  0x00000001),
	GATE_CLK(NSS_CC_PORT2_MAC_CLK,		0x00430,  0x00000001),
	GATE_CLK(NSS_CC_PORT3_MAC_CLK,		0x00438,  0x00000001),
	GATE_CLK(NSS_CC_NSSNOC_PPE_CLK,		0x00440,  0x00000001),
	GATE_CLK(NSS_CC_NSSNOC_PPE_CFG_CLK,	0x00444,  0x00000001),
	GATE_CLK(NSS_CC_PPE_EDMA_CLK,		0x0041C,  0x00000001),
	GATE_CLK(NSS_CC_PPE_EDMA_CFG_CLK,	0x00424,  0x00000001),
	GATE_CLK(NSS_CC_PPE_SWITCH_CLK,		0x00410,  0x00000001),
	GATE_CLK(NSS_CC_PPE_SWITCH_IPE_CLK,	0x00400,  0x00000001),
	GATE_CLK(NSS_CC_PPE_SWITCH_BTQ_CLK,	0x00408,  0x00000001),
	GATE_CLK(NSS_CC_PORT1_RX_CLK,		0x004FC,  0x00000001),
	GATE_CLK(NSS_CC_PORT1_TX_CLK,		0x00504,  0x00000001),
	GATE_CLK(NSS_CC_PORT2_RX_CLK,		0x0050C,  0x00000001),
	GATE_CLK(NSS_CC_PORT2_TX_CLK,		0x00514,  0x00000001),
	GATE_CLK(NSS_CC_PORT3_RX_CLK,		0x0051C,  0x00000001),
	GATE_CLK(NSS_CC_PORT3_TX_CLK,		0x00524,  0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT1_RX_CLK,	0x0057C,  0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT1_TX_CLK,	0x00580,  0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT2_RX_CLK,	0x00584,  0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT2_TX_CLK,	0x00588,  0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT3_RX_CLK,	0x0058C,  0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT3_TX_CLK,	0x00590,  0x00000001),
	GATE_CLK(GCC_MDIO_AHB_CLK,		0x17040,  0x00000001),
	GATE_CLK(GCC_QUPV3_SPI0_CLK,		0x04020,  0x00000001),
	GATE_CLK(GCC_QUPV3_SPI1_CLK,		0x05020,  0x00000001),
	GATE_CLK(GCC_QUPV3_I2C0_CLK,		0x02024,  0x00000001),
	GATE_CLK(GCC_QUPV3_I2C1_CLK,		0x03024,  0x00000001),

};

static int ipq5424_enable(struct clk *clk)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);

	if (priv->data->num_clks <= clk->id) {
		debug("%s: unknown clk id %lu\n", __func__, clk->id);
		return 0;
	}

	debug("%s: clk %s\n", __func__, ipq5424_clks[clk->id].name);

	qcom_gate_clk_en(priv, clk->id);

	return 0;
}

static const struct qcom_reset_map ipq5424_gcc_resets[] = {
	[GCC_SDCC_BCR]			= {0x33000, 0},
	[GCC_UNIPHY0_BCR]		= {0x17044, 0},
	[GCC_UNIPHY1_BCR]		= {0x17054, 0},
	[GCC_UNIPHY2_BCR]		= {0x17054, 0},
	[GCC_UNIPHY0_SOFT_RESET]	= {0x17048, 2},
	[GCC_UNIPHY1_SOFT_RESET]	= {0x17058, 2},
	[GCC_UNIPHY2_SOFT_RESET]	= {0x17068, 2},
	[GCC_UNIPHY0_XPCS_RESET]	= {0x17050, 2},
	[GCC_UNIPHY1_XPCS_RESET]	= {0x17060, 2},
	[GCC_UNIPHY2_XPCS_RESET]	= {0x17070, 2},
	[NSS_CC_PPE_BCR]		= {0x003E8, 0},
	[NSS_CC_PORT1_RX_RESET]		= {0x004FC, 2},
	[NSS_CC_PORT1_TX_RESET]		= {0x00504, 2},
	[NSS_CC_PORT2_RX_RESET]		= {0x0050C, 2},
	[NSS_CC_PORT2_TX_RESET]		= {0x00514, 2},
	[NSS_CC_PORT3_RX_RESET]		= {0x0051C, 2},
	[NSS_CC_PORT3_TX_RESET]		= {0x00524, 2},
	[GCC_UNIPHY0_AHB_RESET]		= {0x1704C, 2},
	[GCC_UNIPHY1_AHB_RESET]		= {0x1705C, 2},
	[GCC_UNIPHY2_AHB_RESET]		= {0x1706C, 2},
	[NSS_CC_PORT1_MAC_RESET]	= {0x00428, 2},
	[NSS_CC_PORT2_MAC_RESET]	= {0x00430, 2},
	[NSS_CC_PORT3_MAC_RESET]	= {0x00438, 2},
};

static struct msm_clk_data ipq5424_gcc_data = {
	.resets = ipq5424_gcc_resets,
	.num_resets = ARRAY_SIZE(ipq5424_gcc_resets),
	.clks = ipq5424_clks,
	.num_clks = ARRAY_SIZE(ipq5424_clks),
	.enable = ipq5424_enable,
	.set_rate = ipq5424_set_rate,
};

static const struct udevice_id gcc_ipq5424_of_match[] = {
	{
		.compatible = "qcom,ipq5424-gcc",
		.data = (ulong)&ipq5424_gcc_data,
	},
	{ }
};

U_BOOT_DRIVER(gcc_ipq5424) = {
	.name		= "gcc_ipq5424",
	.id		= UCLASS_NOP,
	.of_match	= gcc_ipq5424_of_match,
	.bind		= qcom_cc_bind,
	.flags		= DM_FLAG_PRE_RELOC | DM_FLAG_DEFAULT_PD_CTRL_OFF,
};
