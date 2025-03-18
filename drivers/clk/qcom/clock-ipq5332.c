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
#include <dt-bindings/clock/qcom,ipq5332-gcc.h>

#include "clock-qcom.h"

#define GCC_BLSP1_AHB_CBCR			0x1008
#define	GCC_BLSP1_UART1_APPS_CMD_RCGR		0x202C
#define GCC_BLSP1_UART1_APPS_CBCR		0x2040
#define GCC_SDCC1_APPS_CMD_RCGR			0x33004
#define GCC_SDCC1_APPS_CBCR			0x3302C
#define GCC_SDCC1_AHB_CBCR			0x33034

/* BLSP QUP SPI clock register */
#define BLSP1_QUP1_SPI_BCR		0x02000

#define BLSP1_QUP_SPI_BCR(id)		((id < 1) ? \
					(BLSP1_QUP1_SPI_BCR):\
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
	default:
		return -EINVAL;
	}
	return rate;
}

static int ipq5332_enable(struct clk *clk)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);

	switch (clk->id) {
	case GCC_BLSP1_UART1_APPS_CLK:
		clk_enable_cbc(priv->base + GCC_BLSP1_UART1_APPS_CBCR);
		break;
	case GCC_BLSP1_AHB_CLK:
		clk_enable_cbc(priv->base + GCC_BLSP1_AHB_CBCR);
		break;
	case GCC_BLSP1_QUP1_SPI_APPS_CLK:
		clk_enable_cbc(priv->base + BLSP1_QUP_SPI_APPS_CBCR(0));
		break;
	case GCC_BLSP1_QUP2_SPI_APPS_CLK:
		clk_enable_cbc(priv->base + BLSP1_QUP_SPI_APPS_CBCR(1));
		break;
	case GCC_BLSP1_QUP3_SPI_APPS_CLK:
		clk_enable_cbc(priv->base + BLSP1_QUP_SPI_APPS_CBCR(2));
		break;
	case GCC_SDCC1_AHB_CLK:
		clk_enable_cbc(priv->base + GCC_SDCC1_AHB_CBCR);
		break;
	case GCC_SDCC1_APPS_CLK:
		clk_enable_cbc(priv->base + GCC_SDCC1_APPS_CBCR);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct qcom_reset_map ipq5332_gcc_resets[] = {
	[GCC_SDCC_BCR] = {0x33000, 0},
};

static struct msm_clk_data ipq5332_gcc_data = {
	.resets = ipq5332_gcc_resets,
	.num_resets = ARRAY_SIZE(ipq5332_gcc_resets),
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
