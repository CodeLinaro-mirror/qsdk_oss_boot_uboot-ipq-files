// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2018 The Linux Foundation. All rights reserved.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <mach/ipq.h>
#include <mach/smem_info.h>
#include <linux/delay.h>
#include <command.h>
#ifdef CONFIG_PHY_AQUANTIA
#include <u-boot/crc.h>
#include <miiphy.h>
#endif
#ifdef CONFIG_MMC
#include <mmc.h>
#endif

#ifndef IPQ_ETH_FW_PART_NAME
#define IPQ_ETH_FW_PART_NAME			"0:ETHPHYFW"
#endif

#ifndef IPQ_ETH_FW_PART_SIZE
#define IPQ_ETH_FW_PART_SIZE			0x80000
#endif


#ifdef CONFIG_PHY_AQUANTIA
static int ipq_aquantia_load_memory(struct phy_device *phydev, u32 addr,
				const u8 *data, size_t len)
{
	size_t pos;
	u16 crc = 0, up_crc;

	phy_write(phydev, MDIO_MMD_VEND1, 0x200, BIT(12));
	phy_write(phydev, MDIO_MMD_VEND1, 0x202, addr >> 16);
	phy_write(phydev, MDIO_MMD_VEND1, 0x203, addr & 0xfffc);

	for (pos = 0; pos < len; pos += min(sizeof(u32), len - pos)) {
		u32 word = 0;

		memcpy(&word, &data[pos], min(sizeof(u32), len - pos));

		phy_write(phydev, MDIO_MMD_VEND1, 0x204,
			  (word >> 16));
		phy_write(phydev, MDIO_MMD_VEND1, 0x205,
			  word & 0xffff);

		phy_write(phydev, MDIO_MMD_VEND1, 0x200,
			  BIT(15) | BIT(14));

		/* keep a big endian CRC to match the phy processor */
		word = cpu_to_be32(word);
		crc = crc16_ccitt(crc, (u8 *)&word, sizeof(word));
	}

	up_crc = phy_read(phydev, MDIO_MMD_VEND1, 0x201);
	if (crc != up_crc) {
		printf("%s CRC Mismatch: Calculated 0x%04x PHY 0x%04x\n",
		       phydev->dev->name, crc, up_crc);
		return -EINVAL;
	}

	return 0;
}

static int ipq_aquantia_upload_firmware(struct phy_device *phydev,
		uint8_t *addr,	uint32_t file_size)
{
	int ret;
	uint8_t *buf = addr;
	uint32_t primary_header_ptr = 0x00000000;
	uint32_t primary_iram_ptr = 0x00000000;
	uint32_t primary_dram_ptr = 0x00000000;
	uint32_t primary_iram_sz = 0x00000000;
	uint32_t primary_dram_sz = 0x00000000;
	uint32_t phy_img_hdr_off = 0x300;
	uint16_t recorded_ggp8_val, daisy_chain_dis;
	u16 computed_crc, file_crc;

	phy_write(phydev, MDIO_MMD_VEND1, 0x300, 0xdead);
	phy_write(phydev, MDIO_MMD_VEND1, 0x301, 0xbeaf);
	if ((phy_read(phydev, MDIO_MMD_VEND1, 0x300) != 0xdead) &&
			(phy_read(phydev, MDIO_MMD_VEND1, 0x301) != 0xbeaf)) {
		printf("PHY::Scratchpad Read/Write test fail\n");
		ret = -EIO;
		goto exit;
	}

	file_crc = buf[file_size - 2] << 8 | buf[file_size - 1];
	computed_crc = crc16_ccitt(0, buf, file_size - 2);
	if (file_crc != computed_crc) {
		printf("CRC check failed on phy fw file\n");
		ret = -EIO;
		goto exit;
	}

	printf("CRC check good on PHY FW (0x%04X)\n", computed_crc);
	daisy_chain_dis = phy_read(phydev, MDIO_MMD_VEND1, 0xc452);
	if (!(daisy_chain_dis & 0x1))
		phy_write(phydev, MDIO_MMD_VEND1, 0xc452, 0x1);

	phy_write(phydev, MDIO_MMD_VEND1, 0xc471, 0x40);
	recorded_ggp8_val = phy_read(phydev, MDIO_MMD_VEND1, 0xc447);
	if ((recorded_ggp8_val & 0x1f) != phydev->addr)
		phy_write(phydev, MDIO_MMD_VEND1, 0xc447, phydev->addr);

	phy_write(phydev, MDIO_MMD_VEND1, 0xc441, 0x4000);
	phy_write(phydev, MDIO_MMD_VEND1, 0xc001, 0x41);

	primary_header_ptr = (((buf[0x9] & 0x0F) << 8) | buf[0x8]) << 12;

	primary_iram_ptr = (buf[primary_header_ptr +
		phy_img_hdr_off + 0x4 + 2] << 16) |
		(buf[primary_header_ptr + phy_img_hdr_off + 0x4 + 1] << 8) |
		buf[primary_header_ptr + phy_img_hdr_off + 0x4];
	primary_iram_sz = (buf[primary_header_ptr +
		phy_img_hdr_off + 0x7 + 2] << 16) |
		(buf[primary_header_ptr + phy_img_hdr_off + 0x7 + 1] << 8) |
		buf[primary_header_ptr + phy_img_hdr_off + 0x7];
	primary_dram_ptr = (buf[primary_header_ptr +
		phy_img_hdr_off + 0xA + 2] << 16) |
		(buf[primary_header_ptr + phy_img_hdr_off + 0xA + 1] << 8) |
		buf[primary_header_ptr + phy_img_hdr_off + 0xA];
	primary_dram_sz = (buf[primary_header_ptr +
		phy_img_hdr_off + 0xD + 2] << 16) |
		(buf[primary_header_ptr + phy_img_hdr_off + 0xD + 1] << 8) |
		buf[primary_header_ptr + phy_img_hdr_off + 0xD];
	primary_iram_ptr += primary_header_ptr;
	primary_dram_ptr += primary_header_ptr;

	phy_write(phydev, MDIO_MMD_VEND1, 0x200, 0x1000);
	phy_write(phydev, MDIO_MMD_VEND1, 0x200, 0x0);
	computed_crc = 0;

	printf("PHYFW:Loading IRAM...........");
	ret = ipq_aquantia_load_memory(phydev, 0x40000000,
			&buf[primary_iram_ptr], primary_iram_sz);
	if (ret < 0)
		goto exit;
	printf("done.\n");

	printf("PHYFW:Loading DRAM..............");
	ret = ipq_aquantia_load_memory(phydev, 0x3ffe0000,
			&buf[primary_dram_ptr], primary_dram_sz);
	if (ret < 0)
		goto exit;
	printf("done.\n");

	phy_write(phydev, MDIO_MMD_VEND1, 0x0, 0x0);
	phy_write(phydev, MDIO_MMD_VEND1, 0xc001, 0x41);
	phy_write(phydev, MDIO_MMD_VEND1, 0xc001, 0x8041);
	mdelay(100);

	phy_write(phydev, MDIO_MMD_VEND1, 0xc001, 0x40);
	mdelay(100);
	printf("PHYFW loading done.\n");
exit:

	return ret;
}

int ipq_aquantia_load_fw(struct phy_device *phydev)
{
	struct ipq_smem_flash_info *sfi = ipq_get_smem_info();
	u8 *fw_load_addr = NULL;
	int ret = 0;
	struct mbn_header *fwimg_header;

	fw_load_addr = (u8 *)malloc_cache_aligned(IPQ_ETH_FW_PART_SIZE);
	/* We only need memory equivalent to max size ETHPHYFW
	 * which is currently assumed as 512 KB.
	 */
	if (fw_load_addr == NULL) {
		printf("ETHPHYFW Loading failed, size = 0x%x\n",
			IPQ_ETH_FW_PART_SIZE);
		ret = -ENOMEM;
		goto exit;
	}

	memset(fw_load_addr, 0, IPQ_ETH_FW_PART_SIZE);

	ret = ipq_get_partition_data(IPQ_ETH_FW_PART_NAME, 0, fw_load_addr,
					IPQ_ETH_FW_PART_SIZE, sfi->flash_type);
	if (ret < 0)
		goto free_nd_exit;

	fwimg_header = (struct mbn_header *)(fw_load_addr);

	if (fwimg_header->image_type == 0x13 &&
			fwimg_header->header_vsn_num == 0x3) {
		ret = ipq_aquantia_upload_firmware(phydev,
				(uint8_t *)((uint32_t)sizeof(struct mbn_header)
				+ fw_load_addr),
				(uint32_t)(fwimg_header->image_size));
		if (ret != 0)
			goto free_nd_exit;
	} else {
		printf("bad magic on ETHPHYFW partition\n");
		ret = -1;
		goto free_nd_exit;
	}

free_nd_exit:
	free(fw_load_addr);
exit:

	return ret;
}
#endif /* CONFIG_PHY_AQUANTIA */

#if defined(CONFIG_MMC) && defined(CONFIG_SUPPORT_EMMC_BOOT)
static int do_switch_to_boot(struct cmd_tbl *cmdtp, int flag, int argc,
				char *const argv[])
{
	int ret = CMD_RET_FAILURE;
	char runcmd[32] = {0};
	int boot_sel = 0;
	struct mmc *mmc = find_mmc_device(0);

	if (!mmc) {
		printf("no mmc device at slot 0\n");
		return ret;
	}

	if (mmc->part_config == MMCPART_NOAVAILABLE) {
		printf("No part_config info for ver. 0x%x\n", mmc->version);
		return ret;
	}

	switch (argc) {
	case 1:
		boot_sel = EXT_CSD_EXTRACT_BOOT_PART(mmc->part_config);
		if (!((boot_sel == 1) || (boot_sel == 2))) {
			printf("BOOT0 / BOOT1 is not selected ...\n");
			return ret;
		}
		break;
	case 2:
		boot_sel = simple_strtoul(argv[1], NULL, 10) + 1;
		if (!((boot_sel == 1) || (boot_sel == 2)))
			return CMD_RET_USAGE;
		break;
	};

	snprintf(runcmd, sizeof(runcmd), "mmc dev 0 %d", boot_sel);
	ret = run_command(runcmd, 0);
	if (ret) {
		printf("switching to boot%d partition layout failed,ret %d\n",
			boot_sel, ret);
		goto exit;
	}

	snprintf(runcmd, sizeof(runcmd), "mmc partconf 0 0 %d %d",
		 boot_sel, boot_sel);
	ret = run_command(runcmd, 0);
	if (ret) {
		printf("set boot%d select failed, ret %d\n", boot_sel, ret);

#ifndef CONFIG_BLK
		snprintf(runcmd, sizeof(runcmd), "mmc dev 0 %d",
				mmc->block_dev.hwpart);
#else
		snprintf(runcmd, sizeof(runcmd), "mmc dev 0 %d",
				mmc_get_blk_desc(mmc)->hwpart);
#endif
		ret = run_command(runcmd, 0);
		if (ret) {
			printf("switching back to the existing partition");
			printf("layout failed, ret %d\n", ret);
			goto exit;
		}
	} else
		printf("Switched to boot%d partition layout successfully ...\n",
			boot_sel - 1);
exit:
	return ret ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}

U_BOOT_CMD(switch_to_boot, 2, 0, do_switch_to_boot,
	   "switch to the boot partition layout\n",
	   "- switch to the current boot partition layout\n"
	   "switch_boot 0 - switch to the boot0 layout\n"
	   "switch_boot 1 - switch to the boot1 layout\n");

static int do_switch_to_user(struct cmd_tbl *cmdtp, int flag, int argc,
				char *const argv[])
{
	int ret = CMD_RET_FAILURE;
	char runcmd[32] = {0};
	struct mmc *mmc = find_mmc_device(0);

	if (!mmc) {
		printf("no mmc device at slot 0\n");
		return ret;
	}

	if (mmc->part_config == MMCPART_NOAVAILABLE) {
		printf("No part_config info for ver. 0x%x\n", mmc->version);
		return ret;
	}

	snprintf(runcmd, sizeof(runcmd), "mmc dev 0 0");
	ret = run_command(runcmd, 0);
	if (!ret)
		printf("Switched to user partition layout successfully ...\n");

	return ret ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}

U_BOOT_CMD(switch_to_user, 1, 0, do_switch_to_user,
	   "switch to the user partition layout\n",
	   "- switch to the user partition layout\n");
#endif
