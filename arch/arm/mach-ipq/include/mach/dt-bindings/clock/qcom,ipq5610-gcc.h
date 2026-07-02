/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause) */
/*
 * Copyright (c) 2018,2020 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _DT_BINDINGS_CLOCK_IPQ_GCC_IPQ5610_H
#define _DT_BINDINGS_CLOCK_IPQ_GCC_IPQ5610_H

#define GCC_QUPV3_AHB_MST_CLK		0
#define GCC_QUPV3_AHB_SLV_CLK		1
#define GCC_QUPV3_UART1_CLK		2
#define GCC_SDCC1_AHB_CLK		3
#define GCC_SDCC1_APPS_CLK		4
#define GCC_QUPV3_SPI0_CLK		5

#define GCC_QUPV3_2X_CORE_CLK		6
#define GCC_QUPV3_CORE_CLK		7
#define GCC_QUPV3_SLEEP_CLK		8

#define GCC_QUPV3_UART2_CLK		9
#define GCC_QPIC_CLK			10
#define GCC_QPIC_IO_MACRO_CLK		11
#define GCC_QPIC_AHB_CLK		12

/* System clocks for board initialization - added from IPQ9650 reference */
#define GCC_PCNOC_BFDCD_CLK		13
#define GCC_SYSTEM_NOC_BFDCD_CLK	14
#define GCC_APSS_AXI_CLK		15
#define GCC_APSS_AHB_CLK		16
#define GCC_QDSS_TSCTR_CLK		17
#define GCC_QDSS_AT_CLK			18
#define GCC_APSS_DBG_CLK		19
#define GCC_MPM_AHB_CLK			20
#define GCC_GEMNOC_ANOC_CLK		21
#define GCC_ANOC0_AXI_CLK		22
#define GCC_ANOC0_AXI_BFDCD_CLK		23
#define GCC_XO_CLK			24
#define GCC_APSS_TS_CLK			25
#define GCC_SNOC_QOSGEN_EXTREF_CLK	26
#define GCC_SYS_NOC_AT_CLK		27
#define GCC_SNOC_XO_DCD_CLK		28
#define GCC_SNOC_TS_CLK			29
#define GCC_PCNOC_AT_CLK		30
#define GCC_PCNOC_TS_CLK		31
#define GCC_CNOC_QOSGEN_EXTREF_CLK	32
#define GCC_AGGRNOC_ATB_CLK		33
#define GCC_AGGRNOC_TS_CLK		34
#define GCC_ANOC1_AXI_CLK		35
#define GCC_ANOC_QOSGEN_EXTREF_CLK	36
#define GCC_ANOC_XO_DCD_CLK		37
#define GCC_PCNOC_CE_EDPD_CLK		38
#define GCC_CMN_LDO_CLK			39
#define GCC_SPDM_PCNOC_CY_CLK		40

#endif
