// SPDX-License-Identifier: GPL-2.0
/*
 * Clock drivers for Qualcomm ipq9574
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
#include <dt-bindings/clock/qcom,ipq9574-gcc.h>
#include <dt-bindings/reset/qcom,ipq9574-gcc.h>

#include "clock-qcom.h"

#define GCC_BLSP1_AHB_CBCR			0x1004
#define	GCC_BLSP1_UART3_APPS_CMD_RCGR		0x402C
#define GCC_BLSP1_UART3_APPS_CBCR		0x4054
#define GCC_SDCC1_APPS_CMD_RCGR			0x33004
#define GCC_SDCC1_APPS_CBCR			0x3302C
#define GCC_SDCC1_AHB_CBCR			0x33034

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

#define GCC_UNIPHY0_SYS_CBCR				(0x17048)
#define GCC_UNIPHY0_AHB_CBCR				(0x1704C)
#define GCC_UNIPHY_SYS_CBCR(id)		((id < 1) ? \
					(GCC_UNIPHY0_SYS_CBCR) : \
					(GCC_UNIPHY0_SYS_CBCR + (0x10 * id)))
#define GCC_UNIPHY_AHB_CBCR(id)		((id < 1) ? \
					(GCC_UNIPHY0_AHB_CBCR) : \
					(GCC_UNIPHY0_AHB_CBCR + (0x10 * id)))

#define NSS_CC_PORT1_MAC_CBCR		(0x2824C)
#define NSS_CC_PORT_MAC_CBCR(id)	((id <= 1) ? \
					(NSS_CC_PORT1_MAC_CBCR) : \
					(NSS_CC_PORT1_MAC_CBCR + (0x4*(id-1))))

#define NSS_CC_PORT1_RX_CBCR				(0x281A0)
#define NSS_CC_PORT1_TX_CBCR				(0x281A4)
#define NSS_CC_PORT_RX_CBCR(id)		((id <= 1) ? \
					(NSS_CC_PORT1_RX_CBCR) : \
					(NSS_CC_PORT1_RX_CBCR + (0x8*(id-1))))
#define NSS_CC_PORT_TX_CBCR(id)		((id <= 1) ? \
					(NSS_CC_PORT1_TX_CBCR) : \
					(NSS_CC_PORT1_TX_CBCR + (0x8*(id-1))))


#define NSS_CC_UNIPHY_PORT1_RX_CBCR			(0x28904)
#define NSS_CC_UNIPHY_PORT1_TX_CBCR			(0x28908)
#define NSS_CC_UNIPHY_PORT_RX_CBCR(id)	((id <= 1) ? \
					(NSS_CC_UNIPHY_PORT1_RX_CBCR) : \
					(NSS_CC_UNIPHY_PORT1_RX_CBCR +\
					(0x8 * (id-1))))
#define NSS_CC_UNIPHY_PORT_TX_CBCR(id)	((id <= 1) ? \
					(NSS_CC_UNIPHY_PORT1_TX_CBCR) : \
					(NSS_CC_UNIPHY_PORT1_TX_CBCR +\
					(0x8 * (id-1))))


#define NSS_CC_CFG_CMD_RCGR				(0x28104)
#define NSS_CC_PORT1_RX_CMD_RCGR			(0x28110)
#define NSS_CC_PORT1_TX_CMD_RCGR			(0x2811C)
#define NSS_CC_PORT_RX_CMD_RCGR(id)	((id <= 1) ? \
					(NSS_CC_PORT1_RX_CMD_RCGR) : \
					(NSS_CC_PORT1_RX_CMD_RCGR +\
					 (0x18*(id-1))))
#define NSS_CC_PORT_TX_CMD_RCGR(id)	((id <= 1) ? \
					(NSS_CC_PORT1_TX_CMD_RCGR) : \
					(NSS_CC_PORT1_TX_CMD_RCGR +\
					 (0x18*(id-1))))

#define NSS_CC_PORT1_RX_DIV_CDIVR			(0x28118)
#define NSS_CC_PORT_RX_DIV_CDIVR(id)	((id <= 1) ? \
					(NSS_CC_PORT1_RX_DIV_CDIVR) : \
					(NSS_CC_PORT1_RX_DIV_CDIVR +\
					(0x18*(id-1))))


#define NSS_CC_PORT1_TX_DIV_CDIVR			(0x28124)
#define NSS_CC_PORT_TX_DIV_CDIVR(id)	((id <= 1) ? \
					(NSS_CC_PORT1_TX_DIV_CDIVR) : \
					(NSS_CC_PORT1_TX_DIV_CDIVR +\
					(0x18*(id-1))))

#define NSS_CC_PPE_CMD_RCGR				(0x28204)

#define NSS_CC_CFG_SRC_SEL_GCC_GPLL0_OUT_AUX		(2 << 8)
#define NSS_CC_PPE_SRC_SEL_BIAS_PLL_UBI_NC_CLK		(1 << 8)
#define NSS_CC_PORT1_RX_SRC_SEL_UNIPHY0_NSS_RX_CLK	(2 << 8)
#define NSS_CC_PORT1_TX_SRC_SEL_UNIPHY0_NSS_TX_CLK	(3 << 8)
#define NSS_CC_PORT5_RX_SRC_SEL_UNIPHY0_NSS_RX_CLK	(2 << 8)
#define NSS_CC_PORT5_TX_SRC_SEL_UNIPHY0_NSS_TX_CLK	(3 << 8)
#define NSS_CC_PORT5_RX_SRC_SEL_UNIPHY1_NSS_RX_CLK	(4 << 8)
#define NSS_CC_PORT5_TX_SRC_SEL_UNIPHY1_NSS_TX_CLK	(5 << 8)
#define NSS_CC_PORT6_RX_SRC_SEL_UNIPHY2_NSS_RX_CLK	(2 << 8)
#define NSS_CC_PORT6_TX_SRC_SEL_UNIPHY2_NSS_TX_CLK	(3 << 8)

#define CLK_1_25_MHZ			(1250000UL)
#define CLK_2_5_MHZ			(2500000UL)
#define CLK_12_5_MHZ			(12500000UL)
#define CLK_25_MHZ			(25000000UL)
#define CLK_78_125_MHZ			(78125000UL)
#define CLK_50_MHZ			(50000000UL)
#define CLK_125_MHZ			(125000000UL)
#define CLK_156_25_MHZ			(156250000UL)
#define CLK_312_5_MHZ			(312500000UL)

#define GCC_NSSNOC_MEMNOC_BFDCD_SRC_SEL_GPLL0_OUT_MAIN	(1 << 8)
#define GCC_QDSS_AT_SRC_SEL_GPLL0_OUT_MAIN		(1 << 8)
#define GCC_PCNOC_BFDCD_SRC_SEL_GPLL0_OUT_MAIN		(1 << 8)
#define GCC_SYSTEM_NOC_BFDCD_SRC_SEL_GPLL4_OUT_MAIN	(2 << 8)


#define GCC_UNIPHY_SYS_CMD_RCGR				(0x17090)
#define GCC_PCNOC_BFDCD_CMD_RCGR			(0x31004)
#define GCC_SYSTEM_NOC_BFDCD_CMD_RCGR			(0x2E004)
#define GCC_NSSNOC_MEMNOC_BFDCD_CMD_RCGR		(0x17004)
#define GCC_QDSS_AT_CMD_RCGR				(0x2D004)

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
		case CLK_1_25_MHZ:
			*div = 19;
			*cdiv = 24;
			break;
		case CLK_12_5_MHZ:
			*div = 9;
			*cdiv = 4;
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

static ulong ipq9574_set_rate(struct clk *clk, ulong rate)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);
	int ret, src, div = 0, cdiv = 0;
	struct clk *pclk = NULL;

	switch (clk->id) {
	case GCC_BLSP1_UART3_APPS_CLK:
		clk_rcg_set_rate_mnd(priv->base, GCC_BLSP1_UART3_APPS_CMD_RCGR,
					0, 144, 15625, CFG_CLK_SRC_GPLL0, 16);
		break;
	case GCC_SDCC1_APPS_CLK:
		clk_rcg_set_rate_mnd(priv->base, GCC_SDCC1_APPS_CMD_RCGR,
					23, 0, 0, CFG_CLK_SRC_GPLL2, 16);
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
	case GCC_BLSP1_QUP4_SPI_APPS_CLK:
		/* QUP4 SPI APPS CLK: 50MHz */
		clk_rcg_set_rate_mnd(priv->base,
				BLSP1_QUP_SPI_APPS_CMD_RCGR(3), 16, 0, 0,
				CFG_CLK_SRC_GPLL0, 16);
		break;
	case GCC_BLSP1_QUP5_SPI_APPS_CLK:
		/* QUP5 SPI APPS CLK: 50MHz */
		clk_rcg_set_rate_mnd(priv->base,
				BLSP1_QUP_SPI_APPS_CMD_RCGR(4), 16, 0, 0,
				CFG_CLK_SRC_GPLL0, 16);
		break;
	case GCC_BLSP1_QUP6_SPI_APPS_CLK:
		/* QUP6 SPI APPS CLK: 50MHz */
		clk_rcg_set_rate_mnd(priv->base,
				     BLSP1_QUP_SPI_APPS_CMD_RCGR(5), 16, 0, 0,
				     CFG_CLK_SRC_GPLL0, 16);
	case GCC_UNIPHY_SYS_CLK:
		clk_rcg_set_rate_v2(priv->base, GCC_UNIPHY_SYS_CMD_RCGR,
			0, 1, 0, 0);
		break;
	case GCC_PCNOC_BFDCD_CLK:
		clk_rcg_set_rate_v2(priv->base, GCC_PCNOC_BFDCD_CMD_RCGR,
			0, 15, 0, GCC_PCNOC_BFDCD_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_SYSTEM_NOC_BFDCD_CLK:
		clk_rcg_set_rate_v2(priv->base, GCC_SYSTEM_NOC_BFDCD_CMD_RCGR,
			0, 6, 0, GCC_SYSTEM_NOC_BFDCD_SRC_SEL_GPLL4_OUT_MAIN);
		break;
	case GCC_NSSNOC_MEMNOC_BFDCD_CLK:
		clk_rcg_set_rate_v2(priv->base, GCC_NSSNOC_MEMNOC_BFDCD_CMD_RCGR,
			0, 2, 0, GCC_NSSNOC_MEMNOC_BFDCD_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_QDSS_AT_CLK:
		clk_rcg_set_rate_v2(priv->base, GCC_QDSS_AT_CMD_RCGR,
			0, 9, 0, GCC_QDSS_AT_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	/*
	 * NSS controlled clock
	 */
	case NSS_CC_CFG_CLK:
		clk_rcg_set_rate_v2(priv->base, NSS_CC_CFG_CMD_RCGR,
			0, 15, 0, NSS_CC_CFG_SRC_SEL_GCC_GPLL0_OUT_AUX);
		break;
	case NSS_CC_PPE_CLK:
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PPE_CMD_RCGR,
			0, 1, 0, NSS_CC_PPE_SRC_SEL_BIAS_PLL_UBI_NC_CLK);
		break;
	case NSS_CC_PORT1_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT_RX_CMD_RCGR(1),
				NSS_CC_PORT_RX_DIV_CDIVR(1), div, cdiv,
				NSS_CC_PORT1_RX_SRC_SEL_UNIPHY0_NSS_RX_CLK);
		break;
	case NSS_CC_PORT1_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT_TX_CMD_RCGR(1),
				NSS_CC_PORT_TX_DIV_CDIVR(1), div, cdiv,
				NSS_CC_PORT1_TX_SRC_SEL_UNIPHY0_NSS_TX_CLK);
		break;
	case NSS_CC_PORT2_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT_RX_CMD_RCGR(2),
				NSS_CC_PORT_RX_DIV_CDIVR(2), div, cdiv,
				NSS_CC_PORT1_RX_SRC_SEL_UNIPHY0_NSS_RX_CLK);
		break;
	case NSS_CC_PORT2_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT_TX_CMD_RCGR(2),
				NSS_CC_PORT_TX_DIV_CDIVR(2), div, cdiv,
				NSS_CC_PORT1_TX_SRC_SEL_UNIPHY0_NSS_TX_CLK);
		break;
	case NSS_CC_PORT3_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT_RX_CMD_RCGR(3),
				NSS_CC_PORT_RX_DIV_CDIVR(3), div, cdiv,
				NSS_CC_PORT1_RX_SRC_SEL_UNIPHY0_NSS_RX_CLK);
		break;
	case NSS_CC_PORT3_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT_TX_CMD_RCGR(3),
				NSS_CC_PORT_TX_DIV_CDIVR(3), div, cdiv,
				NSS_CC_PORT1_TX_SRC_SEL_UNIPHY0_NSS_TX_CLK);
		break;
	case NSS_CC_PORT4_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT_RX_CMD_RCGR(4),
				NSS_CC_PORT_RX_DIV_CDIVR(4), div, cdiv,
				NSS_CC_PORT1_RX_SRC_SEL_UNIPHY0_NSS_RX_CLK);
		break;
	case NSS_CC_PORT4_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT_TX_CMD_RCGR(4),
				NSS_CC_PORT_TX_DIV_CDIVR(4), div, cdiv,
				NSS_CC_PORT1_TX_SRC_SEL_UNIPHY0_NSS_TX_CLK);
		break;
	case NSS_CC_PORT5_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;

		pclk = clk_get_parent(clk);
		if (!pclk) {
			ret = -ENODEV;
			break;
		}

		if (pclk->id == UNIPHY0_NSS_RX_CLK)
			src = NSS_CC_PORT5_RX_SRC_SEL_UNIPHY0_NSS_RX_CLK;
		else if (pclk->id == UNIPHY1_NSS_RX_CLK)
			src = NSS_CC_PORT5_RX_SRC_SEL_UNIPHY1_NSS_RX_CLK;
		else {
			ret = -EINVAL;
			break;
		}
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT_RX_CMD_RCGR(5),
				NSS_CC_PORT_RX_DIV_CDIVR(5), div, cdiv, src);
		break;
	case NSS_CC_PORT5_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;

		pclk = clk_get_parent(clk);
		if (!pclk) {
			ret = -ENODEV;
			break;
		}

		if (pclk->id == UNIPHY0_NSS_TX_CLK)
			src = NSS_CC_PORT5_TX_SRC_SEL_UNIPHY0_NSS_TX_CLK;
		else if (pclk->id == UNIPHY1_NSS_TX_CLK)
			src = NSS_CC_PORT5_TX_SRC_SEL_UNIPHY1_NSS_TX_CLK;
		else {
			ret = -EINVAL;
			break;
		}
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT_TX_CMD_RCGR(5),
				NSS_CC_PORT_TX_DIV_CDIVR(5), div, cdiv, src);
		break;
	case NSS_CC_PORT6_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT_RX_CMD_RCGR(6),
				NSS_CC_PORT_RX_DIV_CDIVR(6), div, cdiv,
				NSS_CC_PORT6_RX_SRC_SEL_UNIPHY2_NSS_RX_CLK);
		break;
	case NSS_CC_PORT6_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT_TX_CMD_RCGR(6),
				NSS_CC_PORT_TX_DIV_CDIVR(6), div, cdiv,
				NSS_CC_PORT6_TX_SRC_SEL_UNIPHY2_NSS_TX_CLK);
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

static const struct gate_clk ipq9574_clks[] = {
	/*UART*/
	GATE_CLK(GCC_BLSP1_UART3_APPS_CLK,	0x04054, 0x00000001),
	GATE_CLK(GCC_BLSP1_AHB_CLK,		0x01004, 0x00000001),
	/*MMC*/
	GATE_CLK(GCC_SDCC1_AHB_CLK,		0x33034, 0x00000001),
	GATE_CLK(GCC_SDCC1_APPS_CLK,		0x3302C, 0x00000001),
	/*ETHERNET*/
	GATE_CLK(GCC_MDIO_AHB_CLK,		0x17040, 0x00000001),
	GATE_CLK(GCC_MEM_NOC_NSSNOC_CLK,	0x19014, 0x00000001),
	GATE_CLK(GCC_NSSCFG_CLK,		0x1702C, 0x00000001),
	GATE_CLK(GCC_NSSNOC_ATB_CLK,		0x17014, 0x00000001),
	GATE_CLK(GCC_NSSNOC_MEM_NOC_1_CLK,	0x17084, 0x00000001),
	GATE_CLK(GCC_NSSNOC_MEMNOC_CLK,		0x17024, 0x00000001),
	GATE_CLK(GCC_NSSNOC_QOSGEN_REF_CLK,	0x1701C, 0x00000001),
	GATE_CLK(GCC_NSSNOC_TIMEOUT_REF_CLK,	0x17020, 0x00000001),
	GATE_CLK(GCC_CMN_12GPLL_AHB_CLK,	0x3A004, 0x00000001),
	GATE_CLK(GCC_CMN_12GPLL_SYS_CLK,	0x3A008, 0x00000001),
	GATE_CLK(GCC_UNIPHY0_SYS_CLK,		GCC_UNIPHY_SYS_CBCR(0), 0x00000001),
	GATE_CLK(GCC_UNIPHY0_AHB_CLK,		GCC_UNIPHY_AHB_CBCR(0), 0x00000001),
	GATE_CLK(GCC_UNIPHY1_SYS_CLK,		GCC_UNIPHY_SYS_CBCR(1), 0x00000001),
	GATE_CLK(GCC_UNIPHY1_AHB_CLK,		GCC_UNIPHY_AHB_CBCR(1), 0x00000001),
	GATE_CLK(GCC_UNIPHY2_SYS_CLK,		GCC_UNIPHY_SYS_CBCR(2), 0x00000001),
	GATE_CLK(GCC_UNIPHY2_AHB_CLK,		GCC_UNIPHY_AHB_CBCR(2), 0x00000001),
	GATE_CLK(GCC_NSSNOC_SNOC_CLK,		0x17028, 0x00000001),
	GATE_CLK(GCC_NSSNOC_SNOC_1_CLK,		0x1707C, 0x00000001),
	GATE_CLK(GCC_MEM_NOC_SNOC_AXI_CLK,	0x19018, 0x00000001),
	GATE_CLK(NSS_CC_NSS_CSR_CLK,		0x281D0, 0x00000001),
	GATE_CLK(NSS_CC_NSSNOC_NSS_CSR_CLK,	0x281D4, 0x00000001),
	GATE_CLK(NSS_CC_PORT1_MAC_CLK,		NSS_CC_PORT_MAC_CBCR(1), 0x00000001),
	GATE_CLK(NSS_CC_PORT2_MAC_CLK,		NSS_CC_PORT_MAC_CBCR(2), 0x00000001),
	GATE_CLK(NSS_CC_PORT3_MAC_CLK,		NSS_CC_PORT_MAC_CBCR(3), 0x00000001),
	GATE_CLK(NSS_CC_PORT4_MAC_CLK,		NSS_CC_PORT_MAC_CBCR(4), 0x00000001),
	GATE_CLK(NSS_CC_PORT5_MAC_CLK,		NSS_CC_PORT_MAC_CBCR(5), 0x00000001),
	GATE_CLK(NSS_CC_PORT6_MAC_CLK,		NSS_CC_PORT_MAC_CBCR(6), 0x00000001),
	GATE_CLK(NSS_CC_PPE_SWITCH_IPE_CLK,	0x2822C, 0x00000001),
	GATE_CLK(NSS_CC_PPE_SWITCH_CLK,		0x28230, 0x00000001),
	GATE_CLK(NSS_CC_PPE_SWITCH_CFG_CLK,	0x28234, 0x00000001),
	GATE_CLK(NSS_CC_PPE_EDMA_CLK,		0x28238, 0x00000001),
	GATE_CLK(NSS_CC_PPE_EDMA_CFG_CLK,	0x2823C, 0x00000001),
	GATE_CLK(NSS_CC_CRYPTO_PPE_CLK,		0x28240, 0x00000001),
	GATE_CLK(NSS_CC_NSSNOC_PPE_CLK,		0x28244, 0x00000001),
	GATE_CLK(NSS_CC_NSSNOC_PPE_CFG_CLK,	0x28248, 0x00000001),
	GATE_CLK(NSS_CC_PPE_SWITCH_BTQ_CLK,	0x2827C, 0x00000001),
	GATE_CLK(NSS_CC_PORT1_RX_CLK,		NSS_CC_PORT_RX_CBCR(1), 0x00000001),
	GATE_CLK(NSS_CC_PORT1_TX_CLK,		NSS_CC_PORT_TX_CBCR(1), 0x00000001),
	GATE_CLK(NSS_CC_PORT2_RX_CLK,		NSS_CC_PORT_RX_CBCR(2), 0x00000001),
	GATE_CLK(NSS_CC_PORT2_TX_CLK,		NSS_CC_PORT_TX_CBCR(2), 0x00000001),
	GATE_CLK(NSS_CC_PORT3_RX_CLK,		NSS_CC_PORT_RX_CBCR(3), 0x00000001),
	GATE_CLK(NSS_CC_PORT3_TX_CLK,		NSS_CC_PORT_TX_CBCR(3), 0x00000001),
	GATE_CLK(NSS_CC_PORT4_RX_CLK,		NSS_CC_PORT_RX_CBCR(4), 0x00000001),
	GATE_CLK(NSS_CC_PORT4_TX_CLK,		NSS_CC_PORT_TX_CBCR(4), 0x00000001),
	GATE_CLK(NSS_CC_PORT5_RX_CLK,		NSS_CC_PORT_RX_CBCR(5), 0x00000001),
	GATE_CLK(NSS_CC_PORT5_TX_CLK,		NSS_CC_PORT_TX_CBCR(5), 0x00000001),
	GATE_CLK(NSS_CC_PORT6_RX_CLK,		NSS_CC_PORT_RX_CBCR(6), 0x00000001),
	GATE_CLK(NSS_CC_PORT6_TX_CLK,		NSS_CC_PORT_TX_CBCR(6), 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT1_RX_CLK,	NSS_CC_UNIPHY_PORT_RX_CBCR(1), 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT1_TX_CLK,	NSS_CC_UNIPHY_PORT_TX_CBCR(1), 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT2_RX_CLK,	NSS_CC_UNIPHY_PORT_RX_CBCR(2), 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT2_TX_CLK,	NSS_CC_UNIPHY_PORT_TX_CBCR(2), 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT3_RX_CLK,	NSS_CC_UNIPHY_PORT_RX_CBCR(3), 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT3_TX_CLK,	NSS_CC_UNIPHY_PORT_TX_CBCR(3), 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT4_RX_CLK,	NSS_CC_UNIPHY_PORT_RX_CBCR(4), 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT4_TX_CLK,	NSS_CC_UNIPHY_PORT_TX_CBCR(4), 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT5_RX_CLK,	NSS_CC_UNIPHY_PORT_RX_CBCR(5), 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT5_TX_CLK,	NSS_CC_UNIPHY_PORT_TX_CBCR(5), 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT6_RX_CLK,	NSS_CC_UNIPHY_PORT_RX_CBCR(6), 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT6_TX_CLK,	NSS_CC_UNIPHY_PORT_TX_CBCR(6), 0x00000001),
};

static int ipq9574_enable(struct clk *clk)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);

	if (priv->data->num_clks <= clk->id) {
		debug("%s: unknown clk id %lu\n", __func__, clk->id);
		return 0;
	}

	debug("%s: clk %s\n", __func__, ipq9574_clks[clk->id].name);

	qcom_gate_clk_en(priv, clk->id);

	return 0;
}

static const struct qcom_reset_map ipq9574_gcc_resets[] = {
	[GCC_SDCC_BCR]				= {0x33000, 0},
	[GCC_UNIPHY0_SOFT_RESET]		= {0x17050, 0},
	[GCC_UNIPHY1_SOFT_RESET]		= {0x17060, 0},
	[GCC_UNIPHY2_SOFT_RESET]		= {0x17070, 0},
	[GCC_UNIPHY0_XPCS_RESET]		= {0x17050, 2},
	[GCC_UNIPHY1_XPCS_RESET]		= {0x17060, 2},
	[GCC_UNIPHY2_XPCS_RESET]		= {0x17070, 2},
	[NSS_CC_PPE_CFG_RESET]			= {0x28A08, 17},
	[NSS_CC_PPE_EDMA_RESET]			= {0x28A08, 16},
	[NSS_CC_PORT1_MAC_RESET]		= {0x28A08, 11},
	[NSS_CC_PORT2_MAC_RESET]		= {0x28A08, 10},
	[NSS_CC_PORT3_MAC_RESET]		= {0x28A08, 9},
	[NSS_CC_PORT4_MAC_RESET]		= {0x28A08, 8},
	[NSS_CC_PORT5_MAC_RESET]		= {0x28A08, 7},
	[NSS_CC_PORT6_MAC_RESET]		= {0x28A08, 6},
	[NSS_CC_UNIPHY_PORT1_RX_RESET]		= {0x28A24, 23},
	[NSS_CC_UNIPHY_PORT1_TX_RESET]		= {0x28A24, 22},
	[NSS_CC_UNIPHY_PORT2_RX_RESET]		= {0x28A24, 21},
	[NSS_CC_UNIPHY_PORT2_TX_RESET]		= {0x28A24, 20},
	[NSS_CC_UNIPHY_PORT3_RX_RESET]		= {0x28A24, 19},
	[NSS_CC_UNIPHY_PORT3_TX_RESET]		= {0x28A24, 18},
	[NSS_CC_UNIPHY_PORT4_RX_RESET]		= {0x28A24, 17},
	[NSS_CC_UNIPHY_PORT4_TX_RESET]		= {0x28A24, 16},
	[NSS_CC_UNIPHY_PORT5_RX_RESET]		= {0x28A24, 15},
	[NSS_CC_UNIPHY_PORT5_TX_RESET]		= {0x28A24, 14},
	[NSS_CC_UNIPHY_PORT6_RX_RESET]		= {0x28A24, 13},
	[NSS_CC_UNIPHY_PORT6_TX_RESET]		= {0x28A24, 12},
	[NSS_CC_PORT1_RX_RESET]		        = {0x28A24, 11},
	[NSS_CC_PORT1_TX_RESET]		        = {0x28A24, 10},
	[NSS_CC_PORT2_RX_RESET]		        = {0x28A24, 9},
	[NSS_CC_PORT2_TX_RESET]		        = {0x28A24, 8},
	[NSS_CC_PORT3_RX_RESET]		        = {0x28A24, 7},
	[NSS_CC_PORT3_TX_RESET]		        = {0x28A24, 6},
	[NSS_CC_PORT4_RX_RESET]		        = {0x28A24, 5},
	[NSS_CC_PORT4_TX_RESET]		        = {0x28A24, 4},
	[NSS_CC_PORT5_RX_RESET]		        = {0x28A24, 3},
	[NSS_CC_PORT5_TX_RESET]		        = {0x28A24, 2},
	[NSS_CC_PORT6_RX_RESET]		        = {0x28A24, 1},
	[NSS_CC_PORT6_TX_RESET]		        = {0x28A24, 0},
};

static struct msm_clk_data ipq9574_gcc_data = {
	.resets = ipq9574_gcc_resets,
	.num_resets = ARRAY_SIZE(ipq9574_gcc_resets),
	.clks = ipq9574_clks,
	.num_clks = ARRAY_SIZE(ipq9574_clks),
	.enable = ipq9574_enable,
	.set_rate = ipq9574_set_rate,
};

static const struct udevice_id gcc_ipq9574_of_match[] = {
	{
		.compatible = "qcom,ipq9574-gcc",
		.data = (ulong)&ipq9574_gcc_data,
	},
	{ }
};

U_BOOT_DRIVER(gcc_ipq9574) = {
	.name		= "gcc_ipq9574",
	.id		= UCLASS_NOP,
	.of_match	= gcc_ipq9574_of_match,
	.bind		= qcom_cc_bind,
	.flags		= DM_FLAG_PRE_RELOC | DM_FLAG_DEFAULT_PD_CTRL_OFF,
};
