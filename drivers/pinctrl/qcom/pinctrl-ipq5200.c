// SPDX-License-Identifier: GPL-2.0+
/*
 * Qualcomm IPQ5200 pinctrl
 *
 * Copyright (c) 2019 Sartura Ltd.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * Author: Robert Marko <robert.marko@sartura.hr>
 */

#include <dm.h>

#include "pinctrl-qcom.h"

#define MAX_PIN_NAME_LEN 32
static char pin_name[MAX_PIN_NAME_LEN] __section(".data");

enum ipq5200_functions {
	msm_mux_gpio,
	msm_mux_uart1,
	msm_mux_sdc_clk,
	msm_mux_sdc_cmd,
	msm_mux_sdc_data,
	msm_mux_qspi_clk,
	msm_mux_qspi_cs,
	msm_mux_qspi_data,
	msm_mux_NA,
};

#define MSM_PIN_FUNCTION(fname)				\
	[msm_mux_##fname] = {#fname, msm_mux_##fname}

static const struct pinctrl_function msm_pinctrl_functions[] = {
	MSM_PIN_FUNCTION(gpio),
	MSM_PIN_FUNCTION(uart1),
	MSM_PIN_FUNCTION(sdc_clk),
	MSM_PIN_FUNCTION(sdc_cmd),
	MSM_PIN_FUNCTION(sdc_data),
	MSM_PIN_FUNCTION(qspi_clk),
	MSM_PIN_FUNCTION(qspi_cs),
	MSM_PIN_FUNCTION(qspi_data),
};

typedef unsigned int msm_pin_function[10];

#define PINGROUP(id, f1, f2, f3, f4, f5, f6, f7, f8, f9) \
	[id] = {        msm_mux_gpio, /* gpio mode */	\
			msm_mux_##f1,			\
			msm_mux_##f2,			\
			msm_mux_##f3,			\
			msm_mux_##f4,			\
			msm_mux_##f5,			\
			msm_mux_##f6,			\
			msm_mux_##f7,			\
			msm_mux_##f8,			\
			msm_mux_##f9,			\
	}

static const msm_pin_function ipq5200_pin_functions[] = {
	PINGROUP(0, sdc_data, qspi_data, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(1, sdc_data, qspi_data, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(2, sdc_data, qspi_data, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(3, sdc_data, qspi_data, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(4, sdc_cmd, qspi_cs, NA, NA, NA, NA, NA, NA,NA),
	PINGROUP(5, sdc_clk, qspi_clk, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(34, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(35, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(38, uart1, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(39, uart1, NA, NA, NA, NA, NA, NA, NA, NA),
};

static const char *ipq5200_get_function_name(struct udevice *dev,
					     unsigned int selector)
{
	return msm_pinctrl_functions[selector].name;
}

static const char *ipq5200_get_pin_name(struct udevice *dev,
					unsigned int selector)
{
	snprintf(pin_name, MAX_PIN_NAME_LEN, "gpio%u", selector);
	return pin_name;
}

static unsigned int ipq5200_get_function_mux(unsigned int pin,
					     unsigned int selector)
{
	unsigned int i;
	const msm_pin_function *func = ipq5200_pin_functions + pin;

	for (i = 0; i < 10; i++)
		if ((*func)[i] == selector)
			return i;

	pr_err("Can't find requested function for pin %u pin\n", pin);
	return -EINVAL;
}

static const struct msm_pinctrl_data ipq5200_data = {
	.pin_data = {
		.pin_count = 53,
		.special_pins_start = 53, /* There are no special pins */
	},
	.functions_count = ARRAY_SIZE(msm_pinctrl_functions),
	.get_function_name = ipq5200_get_function_name,
	.get_function_mux = ipq5200_get_function_mux,
	.get_pin_name = ipq5200_get_pin_name,
};

static const struct udevice_id msm_pinctrl_ids[] = {
	{ .compatible = "qcom,ipq5200-tlmm", .data = (ulong)&ipq5200_data },
	{ /* Sentinal */ }
};

U_BOOT_DRIVER(pinctrl_ipq5200) = {
	.name		= "pinctrl_ipq5200",
	.id		= UCLASS_NOP,
	.of_match	= msm_pinctrl_ids,
	.ops		= &msm_pinctrl_ops,
	.bind		= msm_pinctrl_bind,
	.flags = DM_FLAG_PRE_RELOC,
};
