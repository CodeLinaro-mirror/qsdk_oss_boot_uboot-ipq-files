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
#define MACH_TYPE_IPQ5332_RDP468		0x8060000
#define MACH_TYPE_IPQ5332_RDP441		0x8060001
#define MACH_TYPE_IPQ5332_RDP441_QCA81XX	0x8060301
#define MACH_TYPE_IPQ5332_RDP441_QCA81XX_I2C	0x8060401
#define MACH_TYPE_IPQ5332_RDP442		0x8060002
#define MACH_TYPE_IPQ5332_RDP446		0x8060004
#define MACH_TYPE_IPQ5332_RDP474		0x8060006
#define MACH_TYPE_IPQ5332_RDP472		0x8060101
#define MACH_TYPE_IPQ5332_RDP473		0x8060008
#define MACH_TYPE_IPQ5332_RDP477		0x8060102
#define MACH_TYPE_IPQ5332_RDP477_256M		0x8060602
#define MACH_TYPE_IPQ5332_RDP478		0x8060007
#define MACH_TYPE_IPQ5332_RDP478_256M		0x8060207
#define MACH_TYPE_IPQ5332_RDP479		0x8060202
#define MACH_TYPE_IPQ5332_RDP480		0x8060402
#define MACH_TYPE_IPQ5332_RDP481		0x8060302
#define MACH_TYPE_IPQ5332_RDP483		0x8060107
#define MACH_TYPE_IPQ5332_RDP484		0x8060201
#define MACH_TYPE_IPQ5332_RDP486		0x8060502
#define MACH_TYPE_IPQ5332_DB_MI01_1		0x1060001
#define MACH_TYPE_IPQ5332_DB_MI02_1		0x1060003
#define MACH_TYPE_IPQ5332_DB_MI03_1		0x1060002
#define MACH_TYPE_IPQ5332_TB_MI03_1		0x1060102
#define MACH_TYPE_IPQ5332_TB_MI05_1		0x1060007

extern uint8_t g_recovery_path;

struct dts_fixup ipq5332_mmc_fixup[] = {
	{ "/soc@0/nand@79b0000/", {"/soc@0/nand@79b0000/%status%?disabled"}, 1},
	{ "/soc@0/mmc@7804000/", {"/soc@0/mmc@7804000/%status%?okay"}, 1},
	{ "/soc/nand@79b0000/", {"/soc/nand@79b0000/%status%?disabled"}, 1},
	{ "/soc/sdhci@7804000/", {"/soc/sdhci@7804000/%status%?okay"}, 1},
	{NULL}
};

struct dts_fixup *mmc_fixup = ipq5332_mmc_fixup;

struct dts_fixup ipq5332_usb_fixup[] = {
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

struct dts_fixup *usb_fixup = ipq5332_usb_fixup;

#ifdef CONFIG_DTB_RESELECT
struct machid_dts_map machid_dts[] = {
	{
		MACH_TYPE_IPQ5332_RDP468,
		"ipq5332-rdp468",
		"rdp468",
		"mi01.6"
	},
	{
		MACH_TYPE_IPQ5332_RDP441,
		"ipq5332-rdp441",
		"rdp441",
		"mi01.2"
	},
	{
		MACH_TYPE_IPQ5332_RDP441_QCA81XX,
		"ipq5332-rdp441-qca81xx",
		"rdp441-qca81xx",
		"mi01.2-qca81xx"
	},
	{
		MACH_TYPE_IPQ5332_RDP441_QCA81XX_I2C,
		"ipq5332-rdp441-qca81xx-i2c",
		"rdp441-qca81xx-i2c",
		"mi01.2-qca81xx-i2c"
	},
	{
		MACH_TYPE_IPQ5332_RDP442,
		"ipq5332-rdp442",
		"rdp442",
		"mi01.3"
	},
	{
		MACH_TYPE_IPQ5332_RDP446,
		"ipq5332-rdp446",
		"rdp446",
		"mi04.1"
	},
	{
		MACH_TYPE_IPQ5332_RDP474,
		"ipq5332-rdp474",
		"rdp474",
		"mi01.9"
	},
	{
		MACH_TYPE_IPQ5332_RDP473,
		"ipq5332-rdp480",
		"rdp473",
		"mi01.7"
	},
	{
		MACH_TYPE_IPQ5332_RDP472,
		"ipq5332-rdp472",
		"rdp472",
		"mi01.2-qcn9160-c1"
	},
	{
		MACH_TYPE_IPQ5332_RDP477,
		"ipq5332-rdp477",
		"rdp477",
		"mi01.3-c4"
	},
	{
		MACH_TYPE_IPQ5332_RDP478,
		"ipq5332-rdp478",
		"rdp478",
		"mi04.1-c3"
	},
	{
		MACH_TYPE_IPQ5332_RDP479,
		"ipq5332-rdp479",
		"rdp479",
		"ap-rdp479"
	},
	{
		MACH_TYPE_IPQ5332_RDP480,
		"ipq5332-rdp480",
		"rdp480",
		"mi01.13"
	},
	{
		MACH_TYPE_IPQ5332_RDP481,
		"ipq5332-rdp481",
		"rdp481",
		"ap-rdp481"
	},
	{
		MACH_TYPE_IPQ5332_RDP483,
		"ipq5332-rdp483",
		"rdp483",
		"mi04.3"
	},
	{
		MACH_TYPE_IPQ5332_RDP484,
		"ipq5332-rdp484",
		"rdp484",
		"mi01.2-c2"
	},
	{
		MACH_TYPE_IPQ5332_RDP486,
		"ipq5332-rdp442",
		"rdp442",
		"mi01.3-c3"
	},
	{
		MACH_TYPE_IPQ5332_RDP477_256M,
		"ipq5332-rdp477-256m",
		"rdp477-256m",
		"mi01.3-c4"
	},
	{
		MACH_TYPE_IPQ5332_RDP478_256M,
		"ipq5332-rdp478-256m",
		"rdp478-256m",
		"mi04.1-c3"
	},
	{
		MACH_TYPE_IPQ5332_DB_MI01_1,
		"ipq5332-db-mi01.1",
		"db-mi01.1",
		NULL
	},
	{
		MACH_TYPE_IPQ5332_DB_MI02_1,
		"ipq5332-db-mi02.1",
		"db-mi02.1",
		NULL
	},
	{
		MACH_TYPE_IPQ5332_DB_MI03_1,
		"ipq5332-db-mi03.1",
		"db-mi03.1",
		NULL
	},
	{
		MACH_TYPE_IPQ5332_TB_MI03_1,
		"ipq5332-tb-mi03.1",
		"tb-mi03.1",
		NULL
	},
	{
		MACH_TYPE_IPQ5332_TB_MI05_1,
		"ipq5332-tb-mi05.1",
		"tb-mi05.1",
		NULL
	},
};

struct multidtb_config ipq5332_dtb_info = {
	.list = machid_dts,
	.ncount = ARRAY_SIZE(machid_dts),
	.index = 0,
};

struct multidtb_config *g_board_dtb_info = &ipq5332_dtb_info;

void ipq_update_board_name(int machid, struct multidtb_config *dtb)
{
	switch(machid) {
	case MACH_TYPE_IPQ5332_RDP473:
		strlcpy(dtb->dts_name, "ipq5332-rdp473",
			BOARD_DTS_MAX_NAMELEN);
	break;
	case MACH_TYPE_IPQ5332_RDP486:
		strlcpy(dtb->dts_name, "ipq5332-rdp486",
			BOARD_DTS_MAX_NAMELEN);
	break;
	default:
		strlcpy(dtb->dts_name, dtb->dts_base, BOARD_DTS_MAX_NAMELEN);
	}

}
#endif /* CONFIG_DTB_RESELECT */

int ipq_read_tcsr_boot_misc(void)
{
	u32 dmagic;
#ifdef CONFIG_SCM
	struct scm_param param;
	int feat_avail = 0;
	int ret;

	if (!g_recovery_path) {
		/* The TCSR DLOAD register is protected in latest TZ
		 * for the IPQ5332 target.
		 * Old TZ will allow direct read.
		 * Use the qca_scm_is_feature_available() call to know
		 * if TZ supports direct or scm read. Based on return
		 * value, read the TCSR WONCE register appropriately.
		 */
		do {
			ret = -ENOTSUPP;
			CHECK_FEATURE(param, 0x6);
			param.get_ret = true;
			ret = ipq_scm_call(&param);
			if (ret) {
				printf("Feature check scm failed\n");
				return 0;
			}
			feat_avail = le32_to_cpu(param.res.result[0]);
		} while (0);

		if (ret == -ENOTSUPP) {
			printf("Unsupported SCM call\n");
			return 0;
		}
	}

	if (feat_avail == 0x401000) {
		do {
			ret = -ENOTSUPP;
			IPQ_SCM_IO_READ(param, (uintptr_t)TCSR_BOOT_MISC_REG);
			param.get_ret = true;
			ret = ipq_scm_call(&param);
			if (ret) {
				printf("dload magic read failed\n");
				return 0;
			}
			dmagic = le32_to_cpu(param.res.result[0]);
		} while (0);

		if (ret == -ENOTSUPP) {
			printf("Unsupported SCM call\n");
			return 0;
		}
	} else {
		dmagic = *(TCSR_BOOT_MISC_REG);
	}
#else
	dmagic = *(TCSR_BOOT_MISC_REG);
#endif

	return dmagic;
}

#ifdef CONFIG_SCM
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
				if ((ret == 0) && (param.res.result[0] & 0x80))
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
#endif

uint32_t is_board_support_image_auth(void)
{

	uint32_t board_type = gd->board_type;
	uint32_t ret = 0;

	switch (gd->ram_size) {
	case SZ_128M:
		break;
	default:
		ret = (board_type & SECURE_BOARD);
		break;
	}

	return ret;
}

int execute_dprv2(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	int ret = CMD_RET_USAGE;
	unsigned long loadaddr, filesize;
	unsigned long default_hex_val = 0xFFFFFFFF;
	uint32_t dpr_status = 0;
	struct scm_param param;

	memset(&param, 0, sizeof(struct scm_param));
	if (argc > cmdtp->maxargs || argc == 2)
		goto fail;

	if (argc == cmdtp->maxargs) {
		loadaddr = simple_strtoul(argv[1], NULL, 16);
		filesize = simple_strtoul(argv[2], NULL, 16);
	} else {
		loadaddr = env_get_hex("fileaddr", default_hex_val);
		if (loadaddr == default_hex_val)
			goto fail;

		filesize = env_get_hex("filesize", default_hex_val);
		if (filesize == default_hex_val)
			goto fail;
	}

	do {
		ret = -ENOTSUPP;
		IPQ_SCM_EXECUTE_DPR(param, loadaddr, filesize);
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

