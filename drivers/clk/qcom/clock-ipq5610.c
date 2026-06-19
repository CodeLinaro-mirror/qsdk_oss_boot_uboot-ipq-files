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
#define GCC_QPIC_CMD_RCGR			(0x32020)
#define GCC_QPIC_IO_MACRO_CMD_RCGR		(0x32004)

#define IO_MACRO_CLK_400_MHZ			(400000000)
#define IO_MACRO_CLK_320_MHZ			(320000000)
#define IO_MACRO_CLK_266_MHZ			(266000000)
#define IO_MACRO_CLK_228_MHZ			(228000000)
#define IO_MACRO_CLK_200_MHZ			(200000000)
#define IO_MACRO_CLK_100_MHZ			(100000000)
#define IO_MACRO_CLK_50_MHZ			(50000000)
#define IO_MACRO_CLK_24_MHZ			(24000000)

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
	int src, div;

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
		/* GCC_QPIC_CLK: 100 MHz */
		clk_rcg_set_rate_v2(priv->base, GCC_QPIC_CMD_RCGR,
				    0, 0xF, 0, CFG_CLK_SRC_GPLL0);
		break;
	case GCC_QPIC_IO_MACRO_CLK:
		src = CFG_CLK_SRC_GPLL0;
		div = 0;

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
	default:
		return -EINVAL;
	}

	return rate;
}

static const struct gate_clk ipq5610_clks[] = {
	GATE_CLK(GCC_QUPV3_UART1_CLK,		0x5020, 0x00000001),
	GATE_CLK(GCC_SDCC1_AHB_CLK,		0x3303C, 0x00000001),
	GATE_CLK(GCC_SDCC1_APPS_CLK,		0x3302C, 0x00000001),
	GATE_CLK(GCC_QUPV3_SPI0_CLK,		0x0202C, 0x00000001),
	GATE_CLK(GCC_QPIC_CLK,			0x32028, 0x00000001),
	GATE_CLK(GCC_QPIC_AHB_CLK,		0x32010, 0x00000001),
	GATE_CLK(GCC_QPIC_IO_MACRO_CLK,		0x3200C, 0x00000001),
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
