// SPDX-License-Identifier: GPL-2.0
/*
 * Clock drivers for Qualcomm ipq5332
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
#include <dt-bindings/clock/qcom,ipq5332-gcc.h>

#include "clock-qcom.h"

#define GCC_BLSP1_AHB_CBCR				0x1008
#define	GCC_BLSP1_UART1_APPS_CMD_RCGR			0x202C
#define GCC_BLSP1_UART1_APPS_CBCR			0x2040
#define GCC_SDCC1_APPS_CMD_RCGR				0x33004
#define GCC_SDCC1_APPS_CBCR				0x3302C
#define GCC_SDCC1_AHB_CBCR				0x33034
#define GCC_QDSS_AT_CMD_RCGR				(0x2D004)
#define PCCNOC_BFDCD_SRC_SEL_GPLL0_OUT_MAIN		(1 << 8)
#define QDSS_SRC_SEL_GPLL4_OUT_MAIN			(1 << 8)
#define GCC_SYSTEM_NOC_BFDCD_SRC_SEL_GPLL4_OUT_MAIN	(2 << 8)
#define GCC_PCNOC_BFDCD_CMD_RCGR			(0x31004)
#define GCC_SYSTEM_NOC_BFDCD_CMD_RCGR			(0x2E004)
#define NSS_CC_CFG_CMD_RCGR				(0x005E0)
#define NSS_CC_PPE_CMD_RCGR				(0x003E8)
#define NSS_CC_PORT1_RX_CMD_RCGR			(0x00450)
#define NSS_CC_PORT1_RX_CFG_RCGR			(0x00454)
#define NSS_CC_PORT1_RX_DIV_CDIVR			(0x00458)
#define NSS_CC_PORT1_TX_CMD_RCGR			(0x0045C)
#define NSS_CC_PORT1_TX_CFG_RCGR			(0x00460)
#define NSS_CC_PORT1_TX_DIV_CDIVR			(0x00464)
#define NSS_CC_PORT2_RX_CMD_RCGR			(0x00468)
#define NSS_CC_PORT2_RX_CFG_RCGR			(0x0046C)
#define NSS_CC_PORT2_RX_DIV_CDIVR			(0x00470)
#define NSS_CC_PORT2_TX_CMD_RCGR			(0x00474)
#define NSS_CC_PORT2_TX_CFG_RCGR			(0x00478)
#define NSS_CC_PORT2_TX_DIV_CDIVR			(0x0047C)
#define NSS_CC_SRC_SEL_GCC_GPLL0_OUT_AUX		(2 << 8)
#define NSS_CC_PPE_SRC_SEL_CMN_PLL_NSS_CLK_200M		(6 << 8)
#define NSS_CC_PORT_RX_SRC_SEL_UNIPHY_NSS_RX_CLK	(3 << 8)
#define NSS_CC_PORT_TX_SRC_SEL_UNIPHY_NSS_TX_CLK	(4 << 8)

#define CLK_2_5_MHZ					(2500000UL)
#define CLK_12_5_MHZ					(12500000UL)
#define CLK_25_MHZ					(25000000UL)
#define CLK_78_125_MHZ					(78125000UL)
#define CLK_50_MHZ					(50000000UL)
#define CLK_125_MHZ					(125000000UL)
#define CLK_156_25_MHZ					(156250000UL)
#define CLK_312_5_MHZ					(312500000UL)

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
	} else
		return -EINVAL;

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

/* BLSP QUP SPI clock register */
#define BLSP1_QUP1_SPI_BCR		0x02000

#define BLSP1_QUP_SPI_BCR(id)		((id < 1) ? \
					(BLSP1_QUP1_SPI_BCR) : \
					(BLSP1_QUP1_SPI_BCR + (0x1000 * id)))

#define BLSP1_QUP_SPI_APPS_CMD_RCGR(id)	(BLSP1_QUP_SPI_BCR(id) + 0x04)
#define BLSP1_QUP_SPI_APPS_CFG_RCGR(id)	(BLSP1_QUP_SPI_BCR(id) + 0x08)
#define BLSP1_QUP_SPI_APPS_M(id)	(BLSP1_QUP_SPI_BCR(id) + 0x0c)
#define BLSP1_QUP_SPI_APPS_N(id)	(BLSP1_QUP_SPI_BCR(id) + 0x10)
#define BLSP1_QUP_SPI_APPS_D(id)	(BLSP1_QUP_SPI_BCR(id) + 0x14)
#define BLSP1_QUP_SPI_APPS_CBCR(id)	(BLSP1_QUP_SPI_BCR(id) + 0x20)

static ulong ipq5332_set_rate(struct clk *clk, ulong rate)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);
	int ret, div = 0, cdiv = 0;

	switch (clk->id) {
	case GCC_BLSP1_UART1_APPS_CLK:
		clk_rcg_set_rate_mnd(priv->base, GCC_BLSP1_UART1_APPS_CMD_RCGR,
				     0, 144, 15625, CFG_CLK_SRC_GPLL0, 16);
		break;
	case GCC_BLSP1_QUP1_SPI_APPS_CLK:
		/* QUP1 SPI APPS CLK: 50MHz */
		clk_rcg_set_rate_mnd(priv->base,
				     BLSP1_QUP_SPI_APPS_CMD_RCGR(0), 16, 0, 0,
				     CFG_CLK_SRC_GPLL0, 16);
		break;
	case GCC_BLSP1_QUP2_SPI_APPS_CLK:
		/* QUP2 SPI APPS CLK: 50MHz */
		clk_rcg_set_rate_mnd(priv->base,
				     BLSP1_QUP_SPI_APPS_CMD_RCGR(1), 16, 0, 0,
				     CFG_CLK_SRC_GPLL0, 16);
		break;
	case GCC_BLSP1_QUP3_SPI_APPS_CLK:
		/* QUP3 SPI APPS CLK: 50MHz */
		clk_rcg_set_rate_mnd(priv->base,
				     BLSP1_QUP_SPI_APPS_CMD_RCGR(2), 16, 0, 0,
				     CFG_CLK_SRC_GPLL0, 16);
		break;
	case GCC_SDCC1_APPS_CLK:
		clk_rcg_set_rate_mnd(priv->base, GCC_SDCC1_APPS_CMD_RCGR,
				     11, 0, 0, CFG_CLK_SRC_GPLL2, 16);
		break;
	case GCC_QDSS_AT_CLK:
		clk_rcg_set_rate_v2(priv->base, GCC_QDSS_AT_CMD_RCGR, 0, 9, 0,
				QDSS_SRC_SEL_GPLL4_OUT_MAIN);
		break;
	case GCC_PCNOC_BFDCD_CLK_SRC:
		clk_rcg_set_rate_v2(priv->base, GCC_PCNOC_BFDCD_CMD_RCGR, 0, 15, 0,
				PCCNOC_BFDCD_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_SYSTEM_NOC_BFDCD_CLK_SRC:
		clk_rcg_set_rate_v2(priv->base, GCC_SYSTEM_NOC_BFDCD_CMD_RCGR,
				0, 8, 0,
				GCC_SYSTEM_NOC_BFDCD_SRC_SEL_GPLL4_OUT_MAIN);
		break;
	/*
	 * NSS controlled clock
	 */
	case NSS_CC_CFG_CLK:
		clk_rcg_set_rate_v2(priv->base, NSS_CC_CFG_CMD_RCGR, 0, 15, 0,
				NSS_CC_SRC_SEL_GCC_GPLL0_OUT_AUX);
		break;
	case NSS_CC_PPE_CLK:
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PPE_CMD_RCGR,
				0, 1, 0, NSS_CC_PPE_SRC_SEL_CMN_PLL_NSS_CLK_200M);
		break;
	case NSS_CC_PORT1_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT1_RX_CMD_RCGR,
				NSS_CC_PORT1_RX_DIV_CDIVR, div, cdiv,
				NSS_CC_PORT_RX_SRC_SEL_UNIPHY_NSS_RX_CLK);
		break;
	case NSS_CC_PORT1_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT1_TX_CMD_RCGR,
				NSS_CC_PORT1_TX_DIV_CDIVR, div, cdiv,
				NSS_CC_PORT_TX_SRC_SEL_UNIPHY_NSS_TX_CLK);
		break;
	case NSS_CC_PORT2_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT2_RX_CMD_RCGR,
				NSS_CC_PORT2_RX_DIV_CDIVR, div, cdiv,
				NSS_CC_PORT_RX_SRC_SEL_UNIPHY_NSS_RX_CLK);
		break;
	case NSS_CC_PORT2_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT2_TX_CMD_RCGR,
				NSS_CC_PORT2_TX_DIV_CDIVR, div, cdiv,
				NSS_CC_PORT_TX_SRC_SEL_UNIPHY_NSS_TX_CLK);
		break;

	case UNIPHY0_NSS_RX_CLK:
	case UNIPHY0_NSS_TX_CLK:
	case UNIPHY1_NSS_RX_CLK:
	case UNIPHY1_NSS_TX_CLK:
		if (rate == CLK_125_MHZ)
			clk->rate = CLK_125_MHZ;
		else if (rate == CLK_312_5_MHZ)
			clk->rate = CLK_312_5_MHZ;
		else
			ret = -EINVAL;
		break;
	default:
		ret = -EINVAL;
	}
	return rate;
}

static const struct gate_clk ipq5332_clks[] = {
	/*UART*/
	GATE_CLK(GCC_BLSP1_UART1_APPS_CLK,	0x2040, 0x00000001),
	GATE_CLK(GCC_BLSP1_AHB_CLK,		0x1008, 0x00000001),
	/*SDHCI*/
	GATE_CLK(GCC_SDCC1_AHB_CLK,		0x33034, 0x00000001),
	GATE_CLK(GCC_SDCC1_APPS_CLK,		0x3302C, 0x00000001),
	GATE_CLK(GCC_QDSS_AT_CLK,		0x2D038, 0x00000001),
	GATE_CLK(GCC_NSSCFG_CLK,		0x1702C, 0x00000001),
	GATE_CLK(GCC_NSSNOC_ATB_CLK,		0x17014, 0x00000001),
	GATE_CLK(GCC_NSSNOC_QOSGEN_REF_CLK,	0x1701C, 0x00000001),
	GATE_CLK(GCC_NSSNOC_TIMEOUT_REF_CLK,	0x17020, 0x00000001),
	GATE_CLK(GCC_NSSCC_CLK,			0x17034, 0x00000001),
	GATE_CLK(GCC_NSSNOC_NSSCC_CLK,		0x17030, 0x00000001),
	GATE_CLK(NSS_CC_NSS_CSR_CLK,		0x005E8, 0x00000001),
	GATE_CLK(NSS_CC_NSSNOC_NSS_CSR_CLK,	0x005EC, 0x00000001),
	GATE_CLK(GCC_IM_SLEEP_CLK,		0x34020, 0x00000001),
	GATE_CLK(GCC_CMN_12GPLL_AHB_CLK,	0x3A004, 0x00000001),
	GATE_CLK(GCC_CMN_12GPLL_SYS_CLK,	0x3A008, 0x00000001),
	GATE_CLK(GCC_UNIPHY0_SYS_CLK,		0x1600C, 0x00000001),
	GATE_CLK(GCC_UNIPHY1_SYS_CLK,		0x16018, 0x00000001),
	GATE_CLK(GCC_UNIPHY0_AHB_CLK,		0x16010, 0x00000001),
	GATE_CLK(GCC_UNIPHY1_AHB_CLK,		0x1601C, 0x00000001),
	GATE_CLK(NSS_CC_PORT1_MAC_CLK,		0x00428, 0x00000001),
	GATE_CLK(NSS_CC_PORT2_MAC_CLK,		0x00430, 0x00000001),
	GATE_CLK(NSS_CC_PPE_SWITCH_IPE_CLK,	0x003F8, 0x00000001),
	GATE_CLK(NSS_CC_PPE_SWITCH_CLK,		0x00408, 0x00000001),
	GATE_CLK(NSS_CC_PPE_SWITCH_CFG_CLK,	0x00410, 0x00000001),
	GATE_CLK(NSS_CC_PPE_EDMA_CLK,		0x00414, 0x00000001),
	GATE_CLK(NSS_CC_PPE_EDMA_CFG_CLK,	0x0041C, 0x00000001),
	GATE_CLK(NSS_CC_NSSNOC_PPE_CLK,		0x00420, 0x00000001),
	GATE_CLK(NSS_CC_NSSNOC_PPE_CFG_CLK,	0x00424, 0x00000001),
	GATE_CLK(NSS_CC_PPE_SWITCH_BTQ_CLK,	0x00400, 0x00000001),
	GATE_CLK(GCC_NSSNOC_SNOC_CLK,		0x17028, 0x00000001),
	GATE_CLK(GCC_NSSNOC_SNOC_1_CLK,		0x1707C, 0x00000001),
	GATE_CLK(NSS_CC_PORT1_RX_CLK,		0x00480, 0x00000001),
	GATE_CLK(NSS_CC_PORT1_TX_CLK,		0x00488, 0x00000001),
	GATE_CLK(NSS_CC_PORT2_RX_CLK,		0x00490, 0x00000001),
	GATE_CLK(NSS_CC_PORT2_TX_CLK,		0x00498, 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT1_RX_CLK,	0x004B4, 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT1_TX_CLK,	0x004B8, 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT2_RX_CLK,	0x004BC, 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT2_TX_CLK,	0x004C0, 0x00000001),
	GATE_CLK(GCC_MDIO_MASTER_AHB_CLK,	0x12004, 0x00000001)
};

static int ipq5332_enable(struct clk *clk)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);

	if (priv->data->num_clks <= clk->id) {
		debug("%s: unknown clk id %lu\n", __func__, clk->id);
		return 0;
	}

	debug("%s: clk %s\n", __func__, ipq5332_clks[clk->id].name);

	qcom_gate_clk_en(priv, clk->id);

	return 0;
}

static const struct qcom_reset_map ipq5332_gcc_resets[] = {
	[GCC_SDCC_BCR]			= {0x33000, 0},
	[GCC_UNIPHY0_BCR]		= {0x16000, 0},
	[GCC_UNIPHY1_BCR]		= {0x16014, 0},
	[GCC_UNIPHY0_SOFT_RESET]	= {0x1600C, 2},
	[GCC_UNIPHY1_SOFT_RESET]	= {0x16018, 2},
	[GCC_UNIPHY0_XPCS_RESET]	= {0x16050, 0},
	[GCC_UNIPHY1_XPCS_RESET]	= {0x16060, 0},
	[NSS_CC_PPE_BCR]		= {0x003E4, 0},
	[NSS_CC_PORT1_RX_RESET]		= {0x004B4, 2},
	[NSS_CC_PORT1_TX_RESET]		= {0x004B8, 2},
	[NSS_CC_PORT2_RX_RESET]		= {0x004BC, 2},
	[NSS_CC_PORT2_TX_RESET]		= {0x004C0, 2},
};

static struct msm_clk_data ipq5332_gcc_data = {
	.resets = ipq5332_gcc_resets,
	.num_resets = ARRAY_SIZE(ipq5332_gcc_resets),
	.clks = ipq5332_clks,
	.num_clks = ARRAY_SIZE(ipq5332_clks),
	.enable = ipq5332_enable,
	.set_rate = ipq5332_set_rate,
};

static const struct udevice_id gcc_ipq5332_of_match[] = {
	{
		.compatible = "qcom,ipq5332-gcc",
		.data = (ulong)&ipq5332_gcc_data,
	},
	{ }
};

U_BOOT_DRIVER(gcc_ipq5332) = {
	.name		= "gcc_ipq5332",
	.id		= UCLASS_NOP,
	.of_match	= gcc_ipq5332_of_match,
	.bind		= qcom_cc_bind,
	.flags		= DM_FLAG_PRE_RELOC | DM_FLAG_DEFAULT_PD_CTRL_OFF,
};
