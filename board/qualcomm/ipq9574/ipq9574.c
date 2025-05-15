// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <asm/io.h>
#include <string.h>
#include <stdlib.h>
#include <mach/ipq.h>
#include <fdtdec.h>
#include <config.h>

/* MACH IDs for various RDPs */
#define MACH_TYPE_IPQ9574_RDP417		0x8050000
#define MACH_TYPE_IPQ9574_RDP418		0x8050001
#define MACH_TYPE_IPQ9574_RDP418_EMMC		0x8050101
#define MACH_TYPE_IPQ9574_RDP437		0x8050201
#define MACH_TYPE_IPQ9574_RDP433		0x8050301
#define MACH_TYPE_IPQ9574_RDP449		0x8050501
#define MACH_TYPE_IPQ9574_RDP433_MHT_PHY	0x8050601
#define MACH_TYPE_IPQ9574_RDP453		0x8050701
#define MACH_TYPE_IPQ9574_RDP454		0x8050801
#define MACH_TYPE_IPQ9574_RDP433_MHT_SWT	0x8050901
#define MACH_TYPE_IPQ9574_RDP467		0x8051301
#define MACH_TYPE_IPQ9574_RDP455_C11		0x8050A01
#define MACH_TYPE_IPQ9574_RDP455_C12		0x8050B01
#define MACH_TYPE_IPQ9574_RDP459		0x8050C01
#define MACH_TYPE_IPQ9574_RDP457		0x8050E01
#define MACH_TYPE_IPQ9574_RDP456		0x8050F01
#define MACH_TYPE_IPQ9574_RDP458		0x8050102
#define MACH_TYPE_IPQ9574_RDP469		0x8051001
#define MACH_TYPE_IPQ9574_RDP461		0x8051201
#define MACH_TYPE_IPQ9574_RDP475		0x8050003
#define MACH_TYPE_IPQ9574_RDP475_QCA81XX	0x8050103
#define MACH_TYPE_IPQ9574_RDP475_QCA81XX_I2C	0x8050203
#define MACH_TYPE_IPQ9574_RDP476		0x8050004
#define MACH_TYPE_IPQ9574_DB_AL01_C1		0x1050000
#define MACH_TYPE_IPQ9574_DB_AL01_C2		0x1050100
#define MACH_TYPE_IPQ9574_DB_AL01_C3		0x1050200
#define MACH_TYPE_IPQ9574_DB_AL02_C1		0x1050001
#define MACH_TYPE_IPQ9574_DB_AL02_C2		0x1050101
#define MACH_TYPE_IPQ9574_DB_AL02_C3		0x1050201
#define MACH_TYPE_IPQ9574_RDP433_SFP		0x8051101

struct dts_fixup ipq9574_mmc_fixup[] = {
	{ "/soc@0/nand@79b0000/", {"/soc@0/nand@79b0000/%status%?disabled"}, 1},
	{ "/soc@0/mmc@7804000/", {"/soc@0/mmc@7804000/%status%?okay"}, 1},
	{ "/soc/nand@79b0000/", {"/soc/nand@79b0000/%status%?disabled"}, 1},
	{ "/soc/sdhci@7804000/", {"/soc/sdhci@7804000/%status%?okay"}, 1},
	{NULL}
};

struct dts_fixup *mmc_fixup = ipq9574_mmc_fixup;

struct dts_fixup ipq9574_usb_fixup[] = {
	{ "/soc@0/usb3@8a00000/dwc3@8a00000/",
		{"/soc@0/usb3@8a00000/dwc3@8a00000%dr_mode%?peripheral",
		"/soc@0/usb3@8a00000/dwc3@8a00000%maximum-speed%?high-speed"},
		2},
	{ "/soc/usb3@8A00000/dwc3@8A00000/",
		{ "/soc/usb3@8A00000/dwc3@8A00000%dr_mode%?peripheral",
		"/soc/usb3@8A00000/dwc3@8A00000%maximum-speed%?high-speed"},
		2},
	{NULL}
};

struct dts_fixup *usb_fixup = ipq9574_usb_fixup;

#ifdef CONFIG_DTB_RESELECT
struct machid_dts_map machid_dts[] = {
	{
		MACH_TYPE_IPQ9574_RDP417,
		"ipq9574-rdp417",
		"rdp-417",
		"al01-c1"
	},
	{
		MACH_TYPE_IPQ9574_RDP418,
		"ipq9574-rdp418",
		"rdp418",
		"al02-c1"
	},
	{
		MACH_TYPE_IPQ9574_RDP418_EMMC,
		"ipq9574-rdp418",
		"rdp418",
		"al02-c2"
	},
	{
		MACH_TYPE_IPQ9574_RDP437,
		"ipq9574-rdp437",
		"rdp437",
		"al02-c3"
	},
	{
		MACH_TYPE_IPQ9574_RDP433,
		"ipq9574-rdp433",
		"rdp433",
		"al02-c4"
	},
	{
		MACH_TYPE_IPQ9574_RDP433_SFP,
		"ipq9574-rdp433-sfp",
		"rdp433-sfp",
		"al02-c18"
	},
	{
		MACH_TYPE_IPQ9574_RDP449,
		"ipq9574-rdp449",
		"rdp449",
		NULL
	},
	{
		MACH_TYPE_IPQ9574_RDP433_MHT_PHY,
		"ipq9574-rdp433-mht-phy",
		"rdp433-mht-phy",
		"al02-c7"
	},
	{
		MACH_TYPE_IPQ9574_RDP453,
		"ipq9574-rdp453",
		"rdp453",
		"al02-c8"
	},
	{
		MACH_TYPE_IPQ9574_RDP454,
		"ipq9574-rdp454",
		"rdp454",
		"al02-c9"
	},
	{
		MACH_TYPE_IPQ9574_RDP433_MHT_SWT,
		"ipq9574-rdp433-mht-switch",
		"rdp433-mht-switch",
		"al02-c10"
	},
	{
		MACH_TYPE_IPQ9574_RDP467,
		"ipq9574-rdp467",
		"rdp467",
		"al02-c20"
	},
	{
		MACH_TYPE_IPQ9574_RDP455_C11,
		"ipq9574-rdp455",
		"rdp455-c11",
		"al02-c11"
	},
	{
		MACH_TYPE_IPQ9574_RDP455_C12,
		"ipq9574-rdp455",
		"rdp455-c12",
		"al02-c12"
	},
	{
		MACH_TYPE_IPQ9574_RDP459,
		"ipq9574-rdp459",
		"rdp459",
		"al02-c13"
	},
	{
		MACH_TYPE_IPQ9574_RDP457,
		"ipq9574-rdp457",
		"rdp457",
		"al02-c15"
	},
	{
		MACH_TYPE_IPQ9574_RDP456,
		"ipq9574-rdp456",
		"rdp456",
		"al02-c16"
	},
	{
		MACH_TYPE_IPQ9574_RDP458,
		"ipq9574-rdp458",
		"rdp458",
		"al03-c2"
	},
	{
		MACH_TYPE_IPQ9574_RDP469,
		"ipq9574-rdp469",
		"rdp469",
		"al02-c17"
	},
	{
		MACH_TYPE_IPQ9574_RDP461,
		"ipq9574-rdp461",
		"rdp461",
		"al02-c19"
	},
	{
		MACH_TYPE_IPQ9574_RDP475,
		"ipq9574-rdp475",
		"rdp475",
		"al05"
	},
	{
		MACH_TYPE_IPQ9574_RDP475_QCA81XX,
		"ipq9574-rdp475-qca81xx",
		"rdp475-qca81xx",
		"al05-qca81xx"
	},
	{
		MACH_TYPE_IPQ9574_RDP475_QCA81XX_I2C,
		"ipq9574-rdp475-qca81xx-i2c",
		"rdp475-qca81xx-i2c",
		"al05-qca81xx-i2c"
	},
	{
		MACH_TYPE_IPQ9574_RDP476,
		"ipq9574-rdp476",
		"rdp476",
		"al06"
	},
	{
		MACH_TYPE_IPQ9574_DB_AL01_C1,
		"ipq9574-db-al01-c1",
		"db-al01-c1",
		NULL
	},
	{
		MACH_TYPE_IPQ9574_DB_AL01_C2,
		"ipq9574-db-al01-c1",
		"db-al01-c2",
		NULL
	},
	{
		MACH_TYPE_IPQ9574_DB_AL01_C3,
		"ipq9574-db-al01-c3",
		"db-al01-c3",
		NULL
	},
	{
		MACH_TYPE_IPQ9574_DB_AL02_C1,
		"ipq9574-db-al02-c1",
		"db-al02-c1",
		NULL
	},
	{
		MACH_TYPE_IPQ9574_DB_AL02_C2,
		"ipq9574-db-al02-c1",
		"db-al02-c2",
		NULL
	},
	{
		MACH_TYPE_IPQ9574_DB_AL02_C3,
		"ipq9574-db-al02-c3",
		"db-al02-c3",
		NULL
	},
};

struct multidtb_config ipq9574_dtb_info = {
	.list = machid_dts,
	.ncount = ARRAY_SIZE(machid_dts),
	.index = 0,
};

struct multidtb_config *g_board_dtb_info = &ipq9574_dtb_info;

void ipq_update_board_name(int machid, struct multidtb_config *dtb)
{
	switch(machid) {
	case MACH_TYPE_IPQ9574_RDP455_C11:
		strlcpy(dtb->dts_name, "ipq9574-rdp455-c11",
			BOARD_DTS_MAX_NAMELEN);
	break;
	case MACH_TYPE_IPQ9574_RDP455_C12:
		strlcpy(dtb->dts_name, "ipq9574-rdp455-c12",
			BOARD_DTS_MAX_NAMELEN);
	break;
	default:
		strlcpy(dtb->dts_name, dtb->dts_base, BOARD_DTS_MAX_NAMELEN);
	}

}
#endif /* CONFIG_DTB_RESELECT */

/**
 * ipq_read_tcsr_boot_misc() - read boot tcsr register
 */
__weak int ipq_read_tcsr_boot_misc(void)
{
	u32 *dmagic = TCSR_BOOT_MISC_REG;

	return *dmagic;
}

uint32_t ipq_get_soc_hw_version(void)
{
	return 0;
}

#if defined(CONFIG_SCM)
bool is_atf_enbled(void)
{
	enum atf_status_t {
		ATF_STATE_DISABLED,
		ATF_STATE_ENABLED,
		ATF_STATE_UNKNOWN,
	} atf_status = ATF_STATE_UNKNOWN;
	struct scm_param param;
	int ret = -1;

	if (likely(atf_status != ATF_STATE_UNKNOWN))
		return (atf_status == ATF_STATE_ENABLED);

	do {
		ret = -ENOTSUPP;
		IPQ_SCM_CHECK_SCM_SUPPORT(param, SCM_SIP_FNID(QCOM_SCM_SVC_INFO,
						QCOM_GET_SECURE_STATE_CMD));
		param.get_ret = true;
		ret = ipq_scm_call(&param);

		if (!ret && (le32_to_cpu(param.res.result[0]) > 0)) {
			do {
				ret = -ENOTSUPP;
				check_atf_support(param);
				param.get_ret = true;

				ret = ipq_scm_call(&param);
				if (ret == 0 && (param.res.result[0] & 0x80))
					atf_status = ATF_STATE_ENABLED;
			} while (0);

			if (ret == -ENOTSUPP) {
				printf("Unsupported SCM call\n");
				return false;
			}

		} else {
			return false;
		}

	} while (0);

	if (ret == -ENOTSUPP) {
		printf("Unsupported SCM call\n");
		return false;
	}

	return atf_status == ATF_STATE_ENABLED;
}

int execute_dprv1(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	int ret = CMD_RET_USAGE;
	unsigned long loadaddr;
	unsigned long default_hex_val = 0xFFFFFFFF;
	uint32_t dpr_status = 0;
	struct scm_param param;

	memset(&param, 0, sizeof(struct scm_param));
	if (argc > cmdtp->maxargs)
		goto fail;

	if (argc == cmdtp->maxargs)
		loadaddr = simple_strtoul(argv[1], NULL, 16);
	else {
		loadaddr = env_get_hex("fileaddr", default_hex_val);
		if (loadaddr == default_hex_val)
			goto fail;
	}

	do {
		ret = -ENOTSUPP;
		IPQ_SCM_EXECUTE_DPR(param, loadaddr);
		param.get_ret = true;
		ret = ipq_scm_call(&param);
		dpr_status = param.res.result[0];

		if (ret || dpr_status) {
			printf("Error in DPR Processing ret : %d, " \
					"dpr_status : %d\n",
					ret, dpr_status);
		} else
			printf("DPR Process Successful\n");
	} while (0);

	if (ret == -ENOTSUPP) {
		printf("Unsupported SCM call\n");
		goto fail;
	}

fail:
	return ret;
}

#endif
