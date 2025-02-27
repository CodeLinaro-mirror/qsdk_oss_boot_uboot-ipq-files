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
#include <dt-bindings/clock/qcom,ipq5424-gcc.h>
#include <dt-bindings/reset/qcom,ipq5424-gcc.h>

#include "clock-qcom.h"

#define	GCC_QUPV3_UART1_CMD_RCGR		0x302C
#define GCC_QUPV3_UART1_CBCR			0x3040
#define GCC_SDCC1_APPS_CMD_RCGR			0x33004
#define GCC_SDCC1_APPS_CBCR			0x3302C
#define GCC_SDCC1_AHB_CBCR			0x33034
#define GCC_QUPV3_SPI0_CMD_RCGR			0x04004
#define GCC_QUPV3_SPI0_CBCR			0x04020

static ulong ipq5424_set_rate(struct clk *clk, ulong rate)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);

	switch (clk->id) {
	case GCC_QUPV3_UART1_CLK:
		clk_rcg_set_rate_mnd(priv->base, GCC_QUPV3_UART1_CMD_RCGR,
				     0, 144, 15625, CFG_CLK_SRC_GPLL0, 16);
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

static int ipq5424_enable(struct clk *clk)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);

	switch (clk->id) {
	case GCC_QUPV3_UART1_CLK:
		clk_enable_cbc(priv->base + GCC_QUPV3_UART1_CBCR);
		break;
	case GCC_SDCC1_AHB_CLK:
		clk_enable_cbc(priv->base + GCC_SDCC1_AHB_CBCR);
		break;
	case GCC_SDCC1_APPS_CLK:
		clk_enable_cbc(priv->base + GCC_SDCC1_APPS_CBCR);
		break;
	case GCC_QUPV3_SPI0_CLK:
		clk_enable_cbc(priv->base + GCC_QUPV3_SPI0_CBCR);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct qcom_reset_map ipq5424_gcc_resets[] = {
	[GCC_SDCC_BCR] = {0x33000, 0},
};

static struct msm_clk_data ipq5424_gcc_data = {
	.resets = ipq5424_gcc_resets,
	.num_resets = ARRAY_SIZE(ipq5424_gcc_resets),
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
