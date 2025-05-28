// SPDX-License-Identifier: GPL-2.0
/*
 * Clock drivers for Qualcomm ipq5200
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
#include <asm/arch/dt-bindings/clock/qcom,ipq5200-gcc.h>
#include <asm/arch/dt-bindings/reset/qcom,ipq5200-gcc.h>
#include "clock-qcom.h"

#define	GCC_QUPV3_UART1_CMD_RCGR		0x5004
#define GCC_SDCC1_APPS_CMD_RCGR			0x33004
#define GCC_QUPV3_SPI0_CMD_RCGR			0x3018

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

static ulong ipq5200_set_rate(struct clk *clk, ulong rate)
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
	default:
		return -EINVAL;
	}

	return rate;
}

static const struct gate_clk ipq5200_clks[] = {
	GATE_CLK(GCC_QUPV3_UART1_CLK,		0x05020, 0x00000001),
	GATE_CLK(GCC_SDCC1_AHB_CLK,		0x3303C, 0x00000001),
	GATE_CLK(GCC_SDCC1_APPS_CLK,		0x3302C, 0x00000001),
	GATE_CLK(GCC_QUPV3_SPI0_CLK,		0x0302C, 0x00000001),
};

static int ipq5200_enable(struct clk *clk)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);

	if (priv->data->num_clks <= clk->id) {
		debug("%s: unknown clk id %lu\n", __func__, clk->id);
		return 0;
	}

	debug("%s: clk %s\n", __func__, ipq5200_clks[clk->id].name);

	qcom_gate_clk_en(priv, clk->id);

	return 0;
}

static const struct qcom_reset_map ipq5200_gcc_resets[] = {
	[GCC_SDCC_BCR] = {0x33000, 0},
};

static struct msm_clk_data ipq5200_gcc_data = {
	.resets = ipq5200_gcc_resets,
	.num_resets = ARRAY_SIZE(ipq5200_gcc_resets),
	.clks = ipq5200_clks,
	.num_clks = ARRAY_SIZE(ipq5200_clks),
	.enable = ipq5200_enable,
	.set_rate = ipq5200_set_rate,
};

static const struct udevice_id gcc_ipq5200_of_match[] = {
	{
		.compatible = "qcom,ipq5200-gcc",
		.data = (ulong)&ipq5200_gcc_data,
	},
	{ }
};

U_BOOT_DRIVER(gcc_ipq5200) = {
	.name		= "gcc_ipq5200",
	.id		= UCLASS_NOP,
	.of_match	= gcc_ipq5200_of_match,
	.bind		= qcom_cc_bind,
	.flags		= DM_FLAG_PRE_RELOC | DM_FLAG_DEFAULT_PD_CTRL_OFF,
};
