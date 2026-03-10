// SPDX-License-Identifier: GPL-2.0
/*
 * Clock drivers for Qualcomm ipq9650
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
#include <asm/arch/dt-bindings/clock/qcom,ipq9650-gcc.h>
#include <asm/arch/dt-bindings/reset/qcom,ipq9650-gcc.h>
#include "clock-qcom.h"

#define	GCC_QUPV3_UART1_CMD_RCGR		0x03018
#define GCC_SDCC1_APPS_CMD_RCGR			0x33004
#define GCC_QUPV3_SPI0_CMD_RCGR			0x02018
#define GCC_QUPV3_WRAP_SE2_CMD_RCGR		0x03034
#define GCC_QUPV3_WRAP_SE3_CMD_RCGR		0x03050
#define GCC_QPIC_CMD_RCGR			(0x32020)
#define GCC_QPIC_IO_MACRO_CMD_RCGR		(0x32004)
#define GCC_USB0_MASTER_CMD_RCGR		(0x2C004)
#define GCC_USB0_MOCK_UTMI_DIV_CDIVR		(0x2C040)
#define GCC_USB0_MOCK_UTMI_CMD_RCGR		(0x2C02C)
#define GCC_USB0_AUX_CMD_RCGR			(0x2C018)
#define GCC_USB1_MOCK_UTMI_DIV_CDIVR		(0x3C018)
#define GCC_USB1_MOCK_UTMI_CMD_RCGR		(0x3C004)

#define CFG_CLK_SRC_GPLL4_OUT_AUX		(1 << 8)

#define PCIE_GPLL4_OUT_MAIN				(2 << 8)
#define PCIE_GPLL0_OUT_MAIN				(1 << 8)

/* PCIE clock control registers */
#define GCC_PCIE0_RCHNG_CMD_RCGR			0x28028
#define GCC_PCIE0_AXI_M_CMD_RCGR 			0x28018
#define GCC_PCIE0_AXI_S_CMD_RCGR			0x28020

#define GCC_PCIE4_RCHNG_CMD_RCGR			0x25014
#define GCC_PCIE4_AXI_M_CMD_RCGR 			0x25004
#define GCC_PCIE4_AXI_S_CMD_RCGR			0x2500C

#define GCC_PCIE3_RCHNG_CMD_RCGR			0x2B028
#define GCC_PCIE3_AXI_M_CMD_RCGR 			0x2B018
#define GCC_PCIE3_AXI_S_CMD_RCGR			0x2B020

#define GCC_PCIE1_RCHNG_CMD_RCGR			0x29028
#define GCC_PCIE1_AXI_M_CMD_RCGR 			0x29018
#define GCC_PCIE1_AXI_S_CMD_RCGR			0x29020

#define GCC_PCIE2_RCHNG_CMD_RCGR			0x2A028
#define GCC_PCIE2_AXI_M_CMD_RCGR 			0x2A018
#define GCC_PCIE2_AXI_S_CMD_RCGR			0x2A020


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

static ulong ipq9650_set_rate(struct clk *clk, ulong rate)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);
	int src, div = 0;

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
	case GCC_QUPV3_I2C_SE2_CLK:
		break;
	case GCC_QUPV3_I2C_SE3_CLK:
		break;
	case GCC_QPIC_CLK:
		/* GCC_QPIC_CLK: 100 MHz  */
		clk_rcg_set_rate_v2(priv->base, GCC_QPIC_CMD_RCGR,
				    0, 0xF, 0, CFG_CLK_SRC_GPLL0);
		break;
	case GCC_QPIC_IO_MACRO_CLK:
		src = CFG_CLK_SRC_GPLL0;
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
		break;
	case GCC_USB0_MASTER_CLK:
		/* Default: 200MHz */
		clk_rcg_set_rate_mnd(priv->base, GCC_USB0_MASTER_CMD_RCGR, 7,
					0, 0, CFG_CLK_SRC_GPLL0, 8);
		break;
	case GCC_USB0_MOCK_UTMI_CLK:
		/* Default: 60MHz */
		writel(1, priv->base + GCC_USB0_MOCK_UTMI_DIV_CDIVR);
		clk_rcg_set_rate_mnd(priv->base, GCC_USB0_MOCK_UTMI_CMD_RCGR,
					19, 0, 0, CFG_CLK_SRC_GPLL4_OUT_AUX, 16);
		break;
	case GCC_USB0_AUX_CLK:
		/* Default: 24MHz */
		clk_rcg_set_rate_mnd(priv->base, GCC_USB0_AUX_CMD_RCGR, 1,
					0, 0, CFG_CLK_SRC_CXO, 8);
		break;
	case GCC_USB1_MOCK_UTMI_CLK:
		/* Default: 60MHz */
		writel(1, priv->base + GCC_USB1_MOCK_UTMI_DIV_CDIVR);
		clk_rcg_set_rate_mnd(priv->base, GCC_USB1_MOCK_UTMI_CMD_RCGR,
					19, 0, 0, CFG_CLK_SRC_GPLL4_OUT_AUX, 8);
		break;
	case GCC_PCIE0_AUX_CLK:
		fallthrough;
	case GCC_PCIE0_RCHNG_CLK:
		/* GCC_PCIE0_RCHNG_CLK: 100 MHz */
		clk_rcg_set_rate(priv->base, GCC_PCIE0_RCHNG_CMD_RCGR,
					8, PCIE_GPLL0_OUT_MAIN);
	case GCC_PCIE0_AXI_M_CLK:
		/* GCC_PCIE0_AXI_M_CLK: 200 MHz */
		clk_rcg_set_rate(priv->base, GCC_PCIE0_AXI_M_CMD_RCGR,
					6, PCIE_GPLL4_OUT_MAIN);
	case GCC_PCIE0_AXI_S_CLK:
		/* GCC_PCIE0_AXI_S_CLK: 200 MHz */
		clk_rcg_set_rate(priv->base, GCC_PCIE0_AXI_S_CMD_RCGR,
					6, PCIE_GPLL4_OUT_MAIN);

	case GCC_PCIE4_AUX_CLK:
		fallthrough;
	case GCC_PCIE4_RCHNG_CLK:
		/* GCC_PCIE4_RCHNG_CLK: 100 MHz */
		clk_rcg_set_rate(priv->base, GCC_PCIE4_RCHNG_CMD_RCGR,
					8, PCIE_GPLL0_OUT_MAIN);
	case GCC_PCIE4_AXI_M_CLK:
		/* GCC_PCIE4_AXI_M_CLK: 200 MHz */
		clk_rcg_set_rate(priv->base, GCC_PCIE4_AXI_M_CMD_RCGR,
					6, PCIE_GPLL4_OUT_MAIN);
	case GCC_PCIE4_AXI_S_CLK:
		/* GCC_PCIE4_AXI_S_CLK: 200 MHz */
		clk_rcg_set_rate(priv->base, GCC_PCIE4_AXI_S_CMD_RCGR,
					6, PCIE_GPLL4_OUT_MAIN);

	case GCC_PCIE3_AUX_CLK:
		fallthrough;
	case GCC_PCIE3_RCHNG_CLK:
		/* GCC_PCIE3_RCHNG_CLK: 100 MHz */
		clk_rcg_set_rate(priv->base, GCC_PCIE3_RCHNG_CMD_RCGR,
					8, PCIE_GPLL0_OUT_MAIN);
	case GCC_PCIE3_AXI_M_CLK:
		/* GCC_PCIE3_AXI_M_CLK: 266.67 MHz */
		clk_rcg_set_rate_v2(priv->base, GCC_PCIE3_AXI_M_CMD_RCGR,
					0, 8, 0, PCIE_GPLL4_OUT_MAIN);
	case GCC_PCIE3_AXI_S_CLK:
		/* GCC_PCIE3_AXI_S_CLK: 200 MHz */
		clk_rcg_set_rate(priv->base, GCC_PCIE3_AXI_S_CMD_RCGR,
					6, PCIE_GPLL4_OUT_MAIN);

	case GCC_PCIE1_AUX_CLK:
		fallthrough;
	case GCC_PCIE1_RCHNG_CLK:
		/* GCC_PCIE1_RCHNG_CLK: 100 MHz */
		clk_rcg_set_rate(priv->base, GCC_PCIE1_RCHNG_CMD_RCGR,
					8, PCIE_GPLL0_OUT_MAIN);
	case GCC_PCIE1_AXI_M_CLK:
		/* GCC_PCIE1_AXI_M_CLK: 266.67 MHz */
		clk_rcg_set_rate_v2(priv->base, GCC_PCIE1_AXI_M_CMD_RCGR,
					0, 8, 0, PCIE_GPLL4_OUT_MAIN);
	case GCC_PCIE1_AXI_S_CLK:
		/* GCC_PCIE1_AXI_S_CLK: 200 MHz */
		clk_rcg_set_rate(priv->base, GCC_PCIE1_AXI_S_CMD_RCGR,
					6, PCIE_GPLL4_OUT_MAIN);

	case GCC_PCIE2_AUX_CLK:
		fallthrough;
	case GCC_PCIE2_RCHNG_CLK:
		/* GCC_PCIE2_RCHNG_CLK: 100 MHz */
		clk_rcg_set_rate(priv->base, GCC_PCIE2_RCHNG_CMD_RCGR,
					8, PCIE_GPLL0_OUT_MAIN);
	case GCC_PCIE2_AXI_M_CLK:
		/* GCC_PCIE2_AXI_M_CLK: 266.67 MHz */
		clk_rcg_set_rate_v2(priv->base, GCC_PCIE2_AXI_M_CMD_RCGR,
					0, 8, 0, PCIE_GPLL4_OUT_MAIN);
	case GCC_PCIE2_AXI_S_CLK:
		/* GCC_PCIE2_AXI_S_CLK: 200 MHz */
		clk_rcg_set_rate(priv->base, GCC_PCIE2_AXI_S_CMD_RCGR,
					6, PCIE_GPLL4_OUT_MAIN);

	default:
		return -EINVAL;
	}

	return rate;
}

static const struct gate_clk ipq9650_clks[] = {
	GATE_CLK(GCC_QUPV3_UART1_CLK,		0x0302C, 0x00000001),
	GATE_CLK(GCC_SDCC1_AHB_CLK,		0x3303C, 0x00000001),
	GATE_CLK(GCC_SDCC1_APPS_CLK,		0x3302C, 0x00000001),
	GATE_CLK(GCC_QUPV3_SPI0_CLK,		0x0202C, 0x00000001),
	GATE_CLK(GCC_QUPV3_I2C_SE2_CLK,		0x03048, 0x00000001),
	GATE_CLK(GCC_QUPV3_I2C_SE3_CLK,		0x03064, 0x00000001),
	GATE_CLK(GCC_QPIC_CLK,			0x32028, 0x00000001),
	GATE_CLK(GCC_QPIC_AHB_CLK,		0x32010, 0x00000001),
	GATE_CLK(GCC_QPIC_IO_MACRO_CLK,		0x3200C, 0x00000001),
	GATE_CLK(GCC_USB0_PIPE_CLK,		0x2C054,  0x00000001),
	GATE_CLK(GCC_USB0_PHY_CFG_AHB_CLK,	0x2C05C,  0x00000001),
	GATE_CLK(GCC_USB1_PHY_CFG_AHB_CLK,	0x3C01C,  0x00000001),
	GATE_CLK(GCC_USB0_MASTER_CLK,		0x2C044,  0x00000001),
	GATE_CLK(GCC_USB0_MOCK_UTMI_CLK,	0x2C050,  0x00000001),
	GATE_CLK(GCC_USB0_AUX_CLK,		0x2C04C,  0x00000001),
	GATE_CLK(GCC_USB0_SLEEP_CLK,		0x2C058,  0x00000001),
	GATE_CLK(GCC_USB1_MOCK_UTMI_CLK,	0x3C024,  0x00000001),
	GATE_CLK(GCC_USB1_MASTER_CLK,		0x3C028,  0x00000001),
	GATE_CLK(GCC_USB1_SLEEP_CLK,		0x3C020,  0x00000001),
	GATE_CLK(GCC_SNOC_USB_CLK,		0x2E0C4,  0x00000001),

	GATE_CLK(GCC_PCIE0_AUX_CLK,		0x28070,  0x00000001),
	GATE_CLK(GCC_PCIE0_AHB_CLK,		0x28030,  0x00000001),
	GATE_CLK(GCC_PCIE0_PIPE_CLK,		0x28068,  0x00000001),
	GATE_CLK(GCC_ANOC_PCIE0_1LANE_M_CLK,	0x2E07C,  0x00000001),
	GATE_CLK(GCC_ANOC_PCIE0_1LANE_S_CLK,	0x2E0CC,  0x00000001),
	GATE_CLK(GCC_PCIE0_AXI_M_CLK,		0x28038,  0x00000001),
	GATE_CLK(GCC_PCIE0_AXI_S_CLK,		0x28040,  0x00000001),
	GATE_CLK(GCC_PCIE0_AXI_S_BRIDGE_CLK,	0x28048,  0x00000001),

	GATE_CLK(GCC_PCIE4_AUX_CLK,		0x25020,  0x00000001),
	GATE_CLK(GCC_PCIE4_AHB_CLK,		0x2501C,  0x00000001),
	GATE_CLK(GCC_PCIE4_PIPE_CLK,		0x2503C,  0x00000001),
	GATE_CLK(GCC_ANOC_PCIE4_1LANE_M_CLK,	0x2E0C0,  0x00000001),
	GATE_CLK(GCC_ANOC_PCIE4_1LANE_S_CLK,	0x2E0DC,  0x00000001),
	GATE_CLK(GCC_PCIE4_AXI_M_CLK,		0x25028,  0x00000001),
	GATE_CLK(GCC_PCIE4_AXI_S_CLK,		0x25030,  0x00000001),
	GATE_CLK(GCC_PCIE4_AXI_S_BRIDGE_CLK,	0x25038,  0x00000001),

	GATE_CLK(GCC_PCIE3_AUX_CLK,		0x2B07C,  0x00000001),
	GATE_CLK(GCC_PCIE3_AHB_CLK,		0x2B030,  0x00000001),
	GATE_CLK(GCC_PCIE3_PIPE_CLK,		0x2B068,  0x00000001),
	GATE_CLK(GCC_ANOC_PCIE3_2LANE_M_CLK,	0x2E0BC,  0x00000001),
	GATE_CLK(GCC_ANOC_PCIE3_2LANE_S_CLK,	0x2E0D8,  0x00000001),
	GATE_CLK(GCC_PCIE3_AXI_M_CLK,		0x2B038,  0x00000001),
	GATE_CLK(GCC_PCIE3_AXI_S_CLK,		0x2B040,  0x00000001),
	GATE_CLK(GCC_PCIE3_AXI_S_BRIDGE_CLK,	0x2B048,  0x00000001),

	GATE_CLK(GCC_PCIE1_AUX_CLK,		0x29074,  0x00000001),
	GATE_CLK(GCC_PCIE1_AHB_CLK,		0x29030,  0x00000001),
	GATE_CLK(GCC_PCIE1_PIPE_CLK,		0x29068,  0x00000001),
	GATE_CLK(GCC_ANOC_PCIE1_2LANE_M_CLK,	0x2E07C,  0x00000001),
	GATE_CLK(GCC_ANOC_PCIE1_2LANE_S_CLK,	0x2E084,  0x00000001),
	GATE_CLK(GCC_PCIE1_AXI_M_CLK,		0x29038,  0x00000001),
	GATE_CLK(GCC_PCIE1_AXI_S_CLK,		0x29040,  0x00000001),
	GATE_CLK(GCC_PCIE1_AXI_S_BRIDGE_CLK,	0x29048,  0x00000001),

	GATE_CLK(GCC_PCIE2_AUX_CLK,		0x2A078,  0x00000001),
	GATE_CLK(GCC_PCIE2_AHB_CLK,		0x2A030,  0x00000001),
	GATE_CLK(GCC_PCIE2_PIPE_CLK,		0x2A068,  0x00000001),
	GATE_CLK(GCC_ANOC_PCIE2_2LANE_M_CLK,	0x2E07C,  0x00000001),
	GATE_CLK(GCC_ANOC_PCIE2_2LANE_S_CLK,	0x2E0D4,  0x00000001),
	GATE_CLK(GCC_PCIE2_AXI_M_CLK,		0x2A038,  0x00000001),
	GATE_CLK(GCC_PCIE2_AXI_S_CLK,		0x2A040,  0x00000001),
	GATE_CLK(GCC_PCIE2_AXI_S_BRIDGE_CLK,	0x2A048,  0x00000001),
};

static int ipq9650_enable(struct clk *clk)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);

	if (priv->data->num_clks <= clk->id) {
		debug("%s: unknown clk id %lu\n", __func__, clk->id);
		return 0;
	}

	debug("%s: clk %s\n", __func__, ipq9650_clks[clk->id].name);

	qcom_gate_clk_en(priv, clk->id);

	return 0;
}

static const struct qcom_reset_map ipq9650_gcc_resets[] = {
	[GCC_SDCC_BCR] = {0x33000, 0},
	[GCC_USB0_PHY_BCR]		= {0x2C06C, 0},
	[GCC_USB3PHY_0_PHY_BCR]		= {0x2C070, 0},
	[GCC_QUSB2_0_PHY_BCR]		= {0x2C068, 0},
	[GCC_QUSB2_1_PHY_BCR]		= {0x3C030, 0},
	[GCC_USB_BCR]			= {0x2C000, 0},
	[GCC_USB1_BCR]			= {0x3C000, 0},

	[GCC_PCIE0_PHY_BCR]		= {0x28060, 0},
	[GCC_PCIE0PHY_PHY_BCR]		= {0x2805c, 0},
	[GCC_PCIE0_PIPE_ARES]		= {0x28068, 2},
	[GCC_PCIE0_CORE_STICKY_RESET]	= {0x28058, 1},
	[GCC_PCIE0_AXI_M_ARES]		= {0x28038, 2},
	[GCC_PCIE0_AXI_S_ARES]		= {0x28040, 2},
	[GCC_PCIE0_AXI_M_STICKY_RESET]	= {0x28058, 4},
	[GCC_PCIE0_AXI_S_STICKY_RESET]	= {0x28058, 2},
	[GCC_PCIE0_AHB_ARES]		= {0x28030, 2},
	[GCC_PCIE0_AUX_ARES]		= {0x28070, 2},

	[GCC_PCIE4_PHY_BCR]		= {0x2504C, 0},
	[GCC_PCIE4PHY_PHY_BCR]		= {0x25048, 0},
	[GCC_PCIE4_PIPE_ARES]		= {0x2503C, 2},
	[GCC_PCIE4_CORE_STICKY_RESET]	= {0x25054, 1},
	[GCC_PCIE4_AXI_M_ARES]		= {0x25028, 2},
	[GCC_PCIE4_AXI_S_ARES]		= {0x25030, 2},
	[GCC_PCIE4_AXI_M_STICKY_RESET]	= {0x25054, 4},
	[GCC_PCIE4_AXI_S_STICKY_RESET]	= {0x25054, 2},
	[GCC_PCIE4_AHB_ARES]		= {0x2501C, 2},
	[GCC_PCIE4_AUX_ARES]		= {0x25020, 2},

	[GCC_PCIE3_PHY_BCR]		= {0x2B060, 0},
	[GCC_PCIE3PHY_PHY_BCR]		= {0x2B05C, 0},
	[GCC_PCIE3_PIPE_ARES]		= {0x2B068, 2},
	[GCC_PCIE3_CORE_STICKY_RESET]	= {0x2B058, 1},
	[GCC_PCIE3_AXI_M_ARES]		= {0x2B038, 2},
	[GCC_PCIE3_AXI_S_ARES]		= {0x2B040, 2},
	[GCC_PCIE3_AXI_M_STICKY_RESET]	= {0x2B058, 4},
	[GCC_PCIE3_AXI_S_STICKY_RESET]	= {0x2B058, 2},
	[GCC_PCIE3_AHB_ARES]		= {0x2B030, 2},
	[GCC_PCIE3_AUX_ARES]		= {0x2B07C, 2},

	[GCC_PCIE1_PHY_BCR]		= {0x29060, 0},
	[GCC_PCIE1PHY_PHY_BCR]		= {0x2905C, 0},
	[GCC_PCIE1_PIPE_ARES]		= {0x29068, 2},
	[GCC_PCIE1_CORE_STICKY_RESET]	= {0x29058, 1},
	[GCC_PCIE1_AXI_M_ARES]		= {0x29038, 2},
	[GCC_PCIE1_AXI_S_ARES]		= {0x29040, 2},
	[GCC_PCIE1_AXI_M_STICKY_RESET]	= {0x29058, 4},
	[GCC_PCIE1_AXI_S_STICKY_RESET]	= {0x29058, 2},
	[GCC_PCIE1_AHB_ARES]		= {0x29030, 2},
	[GCC_PCIE1_AUX_ARES]		= {0x29074, 2},

	[GCC_PCIE2_PHY_BCR]		= {0x2A060, 0},
	[GCC_PCIE2PHY_PHY_BCR]		= {0x2A05C, 0},
	[GCC_PCIE2_PIPE_ARES]		= {0x2A068, 2},
	[GCC_PCIE2_CORE_STICKY_RESET]	= {0x2A058, 1},
	[GCC_PCIE2_AXI_M_ARES]		= {0x2A038, 2},
	[GCC_PCIE2_AXI_S_ARES]		= {0x2A040, 2},
	[GCC_PCIE2_AXI_M_STICKY_RESET]	= {0x2A058, 4},
	[GCC_PCIE2_AXI_S_STICKY_RESET]	= {0x2A058, 2},
	[GCC_PCIE2_AHB_ARES]		= {0x2A030, 2},
	[GCC_PCIE2_AUX_ARES]		= {0x2A078, 2},
};

static struct msm_clk_data ipq9650_gcc_data = {
	.resets = ipq9650_gcc_resets,
	.num_resets = ARRAY_SIZE(ipq9650_gcc_resets),
	.clks = ipq9650_clks,
	.num_clks = ARRAY_SIZE(ipq9650_clks),
	.enable = ipq9650_enable,
	.set_rate = ipq9650_set_rate,
};

static const struct udevice_id gcc_ipq9650_of_match[] = {
	{
		.compatible = "qcom,ipq9650-gcc",
		.data = (ulong)&ipq9650_gcc_data,
	},
	{ }
};

U_BOOT_DRIVER(gcc_ipq9650) = {
	.name		= "gcc_ipq9650",
	.id		= UCLASS_NOP,
	.of_match	= gcc_ipq9650_of_match,
	.bind		= qcom_cc_bind,
	.flags		= DM_FLAG_PRE_RELOC | DM_FLAG_DEFAULT_PD_CTRL_OFF,
};
