// SPDX-License-Identifier: GPL-2.0
/*
 * QCE1204 PHY Driver for U-Boot
 * Based on NSS driver reference and QCA81xx skeleton
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <config.h>
#include <command.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/bitfield.h>
#include <linux/bitops.h>
#include <linux/mdio.h>
#include <phy.h>
#include <miiphy.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <log.h>

/*
 * REGISTER DEFINITIONS (from NSS driver)
 */

/* PHY ID */
#define QCE1204_PHY_ID                          0x004dd190

/* Address Offsets */
enum qce1204_addr_offset {
	PCS0_ADDR_OFFSET = 4,
	PCS1_ADDR_OFFSET = 4,
	SOC_ADDR_OFFSET = 6,
};

/* PHY Registers */
#define QCE1204_PHY_SPEC_STATUS                 0x11
#define QCE1204_PHY_SS_LINK_STATUS              0x400
#define QCE1204_PHY_SS_SPEED_MASK               0x380
#define QCE1204_PHY_SS_SPEED_2500               0x200
#define QCE1204_PHY_SS_SPEED_1000               0x100
#define QCE1204_PHY_SS_SPEED_100                0x80
#define QCE1204_PHY_SS_SPEED_10                 0
#define QCE1204_PHY_SS_DUPLEX_FULL              0x2000
#define QCE1204_PHY_CONTROL                     0x19
#define QCE1204_PHY_FIFO_RESET                  0x3
#define QCE1204_PHY_MMD7_IPG_OP                 0x901d
#define QCE1204_PHY_IPG_10_TO_11_EN             0x1

/* PCS MMD1 Registers */
#define QCE1204_PCS_MMD1_CALIBRATION4           0x78
#define QCE1204_PCS_MMD1_MODE_CTRL              0x11b
#define QCE1204_PCS_MMD1_BYPASS_TUNING_IPG      0x189
#define QCE1204_PCS_MMD1_GMII_DATAPASS_SEL      0x180
#define QCE1204_PCS_MMD1_QUSGMII_RESET          0x18c
#define QCE1204_PCS_MMD1_CDA_CONTROL1           0x20
#define QCE1204_PCS_MMD1_PLL_POWER_ON_AND_RESET 0x1e0

/* PCS MMD1 Register Fields */
#define QCE1204_PCS_MMD1_CALIBRATION_DONE       0x80
#define QCE1204_PCS_MMD1_XPCS_MODE              0x1000
#define QCE1204_PCS_MMD1_DATAPASS_MASK          0x1
#define QCE1204_PCS_MMD1_DATAPASS_QUSGMII       0x1
#define QCE1204_PCS_MMD1_SSCG_ENABLE            0x8
#define QCE1204_PCS_MMD1_ANA_SOFT_RESET_MASK    0x40
#define QCE1204_PCS_MMD1_ANA_SOFT_RESET         0
#define QCE1204_PCS_MMD1_ANA_SOFT_RELEASE       0x40
#define QCE1204_PCS_MMD1_QUSGMII_FUNC_RESET     0x10

/* PCS MMD3 Registers */
#define QCE1204_PCS_MMD3_PCS_CTRL2              0x7
#define QCE1204_PCS_MMD3_10GBASE_PCS_STATUS1    0x20
#define QCE1204_PCS_MMD3_DIG_CTRL1              0x8000
#define QCE1204_PCS_MMD3_VR_RPCS_TPC            0x8007
#define QCE1204_PCS_MMD3_MII_AM_INTERVAL        0x800a

/* PCS MMD3 Register Fields */
#define QCE1204_PCS_MMD3_PCS_TYPE_10GBASE_R     0
#define QCE1204_PCS_MMD3_10GBASE_UP             0x1000
#define QCE1204_PCS_MMD3_QUSGMII_EN             0x200
#define QCE1204_PCS_MMD3_QUSGMII_MODE           0x1400
#define QCE1204_PCS_MMD3_MII_AM_INTERVAL_VAL    0x6018
#define QCE1204_PCS_MMD3_XPCS_SOFT_RESET        0x8000
#define QCE1204_PCS_MMD3_QUSGMII_FIFO_RESET     0x400

/* PCS MMD31 (VEND2) Registers */
#define QCE1204_PCS_MMD_MII_CTRL                0
#define QCE1204_PCS_MMD_MII_DIG_CTRL            0x8000
#define QCE1204_PCS_MMD_MII_AN_INT_MSK          0x8001
#define QCE1204_PCS_MMD_MII_XAUI_MODE_CTRL      0x8004

/* PCS MMD31 Register Fields */
#define QCE1204_PCS_MMD_MII_AN_ENABLE           0x1000
#define QCE1204_PCS_MMD_PHY_MODE_CTRL_EN        0x1
#define QCE1204_PCS_MMD_AN_COMPLETE_INT         0x1
#define QCE1204_PCS_MMD_MII_4BITS_CTRL          0x0
#define QCE1204_PCS_MMD_TX_CONFIG_CTRL          0x8
#define QCE1204_PCS_MMD_TX_IPG_CHECK_DISABLE    0x1

/* CDT Threshold Registers */
#define QCE1204_MMD3_CDT_THRESH_CTRL2           0x8073
#define QCE1204_MMD3_CDT_THRESH_CTRL3           0x8074
#define QCE1204_MMD3_CDT_THRESH_CTRL4           0x8075
#define QCE1204_MMD3_CDT_THRESH_CTRL5           0x8076
#define QCE1204_MMD3_CDT_THRESH_CTRL6           0x8077
#define QCE1204_MMD3_CDT_THRESH_CTRL7           0x8078
#define QCE1204_MMD3_CDT_THRESH_CTRL9           0x807a
#define QCE1204_MMD3_CDT_THRESH_CTRL13          0x807e
#define QCE1204_MMD3_CDT_THRESH_CTRL14          0x807f

/* CDT Threshold Values */
#define QCE1204_MMD3_CDT_THRESH_CTRL2_VAL       0xb03f
#define QCE1204_MMD3_CDT_THRESH_CTRL3_VAL       0xc040
#define QCE1204_MMD3_CDT_THRESH_CTRL4_VAL       0xa060
#define QCE1204_MMD3_CDT_THRESH_CTRL5_VAL       0xc040
#define QCE1204_MMD3_CDT_THRESH_CTRL6_VAL       0xa060
#define QCE1204_MMD3_CDT_THRESH_CTRL7_VAL       0xae50
#define QCE1204_MMD3_CDT_THRESH_CTRL9_VAL       0xc060
#define QCE1204_MMD3_CDT_THRESH_CTRL13_VAL      0xb060
#define QCE1204_MMD3_CDT_THRESH_CTRL14_VAL      0xb6b0

/* Channel MMD mapping */
#define QCE1204_PCS_MMD_CH2                     26
#define QCE1204_PCS_MMD_CH3                     27
#define QCE1204_PCS_MMD_CH4                     28

/* EEE Registers */
#define QCE1204_PCS_MMD3_AN_LP_BASE_ABL2        0x14
#define QCE1204_PCS_MMD3_EEE_MODE_CTRL          0x8006
#define QCE1204_PCS_MMD3_EEE_TX_TIMER           0x8008
#define QCE1204_PCS_MMD3_EEE_RX_TIMER           0x8009
#define QCE1204_PCS_MMD3_EEE_MODE_CTRL1         0x800b

/* EEE Register Fields */
#define QCE1204_PCS_MMD3_XPCS_EEE_CAP           0x40
#define QCE1204_PCS_MMD3_EEE_RES_REGS           0x100
#define QCE1204_PCS_MMD3_EEE_SIGN_BIT_REGS      0x40
#define QCE1204_PCS_MMD3_EEE_EN                 0x3
#define QCE1204_PCS_MMD3_EEE_TSL_REGS           0xa
#define QCE1204_PCS_MMD3_EEE_TLU_REGS           0xc0
#define QCE1204_PCS_MMD3_EEE_TWL_REGS           0x1600
#define QCE1204_PCS_MMD3_EEE_100US_REG_REGS     0xc8
#define QCE1204_PCS_MMD3_EEE_RWR_REG_REGS       0x1c00
#define QCE1204_PCS_MMD3_EEE_TRANS_LPI_MODE     0x1
#define QCE1204_PCS_MMD3_EEE_TRANS_RX_LPI_MODE  0x100

/* 2.5G EEE TX LPI Control */
#define QCE1024_PHY_2P5G_EEE_TX_LPI_CTRL        0xa10c
#define QCE1024_PHY_TX_LPI_DELAY_SEL_MASK       0xf00
#define QCE1024_PHY_TX_LPI_DELAY_SEL_1          0x100

/* Speed definitions for U-Boot compatibility */
#ifndef SPEED_UNKNOWN
#define SPEED_UNKNOWN   -1
#endif

/*
 * FPGA SUPPORT FUNCTIONS
 */

static int qce1204_fpga_gty_reset(struct phy_device *phydev)
{
	struct phy_device local_phydev;
	int ret;

	debug("QCE1204: FPGA GTY reset\n");

	/* Reset GTY - use address 11 directly */
	memcpy(&local_phydev, phydev, sizeof(struct phy_device));
	local_phydev.addr = 11;

	ret = phy_write(&local_phydev, MDIO_DEVAD_NONE, 1, 0x1000);
	if (ret < 0) {
		debug("QCE1204: GTY reset assert failed: %d\n", ret);
		return ret;
	}

	mdelay(1000);

	ret = phy_write(&local_phydev, MDIO_DEVAD_NONE, 1, 0);
	if (ret < 0) {
		debug("QCE1204: GTY reset release failed: %d\n", ret);
		return ret;
	}

	/* Fix traffic issues (CRC) for all 4 channels */

	/* Channel 1 (addr=1) */
	memcpy(&local_phydev, phydev, sizeof(struct phy_device));
	local_phydev.addr = 1;
	ret = phy_write(&local_phydev, MDIO_MMD_PCS, 0xa132, 0x6003);
	if (ret < 0) {
		debug("QCE1204: CRC fix failed for channel 1: %d\n", ret);
		return ret;
	}

	/* Channel 2 (addr=2) */
	local_phydev.addr = 2;
	ret = phy_write(&local_phydev, MDIO_MMD_PCS, 0xa132, 0x6003);
	if (ret < 0) {
		debug("QCE1204: CRC fix failed for channel 2: %d\n", ret);
		return ret;
	}

	/* Channel 3 (addr=3) */
	local_phydev.addr = 3;
	ret = phy_write(&local_phydev, MDIO_MMD_PCS, 0xa132, 0x6003);
	if (ret < 0) {
		debug("QCE1204: CRC fix failed for channel 3: %d\n", ret);
		return ret;
	}

	/* Channel 4 (addr=4) */
	local_phydev.addr = 4;
	ret = phy_write(&local_phydev, MDIO_MMD_PCS, 0xa132, 0x6003);
	if (ret < 0) {
		debug("QCE1204: CRC fix failed for channel 4: %d\n", ret);
		return ret;
	}
	return 0;
}

static int qce1204_fpga_xpcs_assert(struct phy_device *phydev)
{
	int addr = phydev->addr;

	phydev->addr = 11;
	/* Assert XPCS reset */
	phy_write(phydev, MDIO_DEVAD_NONE, 6, 0x2000);
	phydev->addr = addr;
	return 0;
}

static int qce1204_fpga_xpcs_deassert(struct phy_device *phydev)
{
	int addr = phydev->addr;

	phydev->addr = 11;
	/* De-assert XPCS reset */
	phy_write(phydev, MDIO_DEVAD_NONE, 6, 0);
	phydev->addr = addr;
	return 0;
}

static int qce1204_fpga_port_clk_reset(struct phy_device *phydev)
{
	int addr = phydev->addr;

	phydev->addr = 11;
	/* Interface reset */
	phy_write(phydev, MDIO_DEVAD_NONE, 6, 0x2fff);
	mdelay(1000);
	phy_write(phydev, MDIO_DEVAD_NONE, 6, 0x2000);
	phydev->addr = addr;
	return 0;
}

/*
 * HELPER FUNCTIONS
 */

/* PCS MMD Register Read */
static int qce1204_pcs_read_mmd(struct phy_device *phydev, int devad,
				int regnum)
{
	struct phy_device local_phydev;
	int ret;

	memcpy(&local_phydev, phydev, sizeof(struct phy_device));
	local_phydev.addr = phydev->addr + PCS1_ADDR_OFFSET;

	ret = phy_read(&local_phydev, devad, regnum);
	return ret;
}

/* PCS MMD Register Write */
static int qce1204_pcs_write_mmd(struct phy_device *phydev, int devad,
				 int regnum, u16 val)
{
	struct phy_device local_phydev;
	int ret;

	memcpy(&local_phydev, phydev, sizeof(struct phy_device));
	local_phydev.addr = phydev->addr + PCS1_ADDR_OFFSET;

	ret = phy_write(&local_phydev, devad, regnum, val);
	return ret;
}

/* PCS MMD Register Modify */
static int qce1204_pcs_modify_mmd(struct phy_device *phydev, int devad,
				  int regnum, u16 mask, u16 set)
{
	int new, ret;
	struct phy_device local_phydev;

	memcpy(&local_phydev, phydev, sizeof(struct phy_device));
	local_phydev.addr = phydev->addr + PCS1_ADDR_OFFSET;

	ret = phy_read(&local_phydev, devad, regnum);
	if (ret < 0)
		return ret;

	new = (ret & ~mask) | set;
	ret = phy_write(&local_phydev, devad, regnum, new);

	return ret;
}

/* Get MMD ID for specific channel */
static int qce1204_pcs_mmd_get(struct phy_device *phydev, int channel)
{
	switch (channel) {
	case 1:
		return MDIO_MMD_VEND2;  /* Channel 1 uses MMD31 */
	case 2:
		return QCE1204_PCS_MMD_CH2;  /* Channel 2 uses MMD26 */
	case 3:
		return QCE1204_PCS_MMD_CH3;  /* Channel 3 uses MMD27 */
	case 4:
		return QCE1204_PCS_MMD_CH4;  /* Channel 4 uses MMD28 */
	default:
		return -EOPNOTSUPP;
	}
}

/* Modify MMD register for specific channel */
static int qce1204_pcs_modify_channel_mmd(struct phy_device *phydev,
					  int channel, int regnum,
					   u16 mask, u16 set)
{
	int mmd_id = qce1204_pcs_mmd_get(phydev, channel);

	if (mmd_id < 0)
		return -EOPNOTSUPP;

	return qce1204_pcs_modify_mmd(phydev, mmd_id, regnum, mask, set);
}

/*
 * CALIBRATION AND RESET FUNCTIONS
 */

/* PCS Calibration - simplified version */
static int qce1204_pcs_calibration(struct phy_device *phydev)
{
	u16 pcs_data = 0;
	u32 retries = 100;
	u32 calibration_done = 0;

	/* Poll for calibration done */
	while (calibration_done != QCE1204_PCS_MMD1_CALIBRATION_DONE) {
		mdelay(1);
		if (retries-- == 0) {
			debug("QCE1204: PCS calibration timeout\n");
			return -ETIMEDOUT;
		}

		pcs_data = qce1204_pcs_read_mmd(phydev, MDIO_MMD_PMAPMD,
						QCE1204_PCS_MMD1_CALIBRATION4);
		calibration_done = (pcs_data & QCE1204_PCS_MMD1_CALIBRATION_DONE);
	}
	return 0;
}

/* Analog Soft Reset Assert/Release */
static int qce1204_pcs_assert(struct phy_device *phydev, bool assert)
{
	int ret;

	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PMAPMD,
				     QCE1204_PCS_MMD1_PLL_POWER_ON_AND_RESET,
				     QCE1204_PCS_MMD1_ANA_SOFT_RESET_MASK,
				     assert ? QCE1204_PCS_MMD1_ANA_SOFT_RESET :
					     QCE1204_PCS_MMD1_ANA_SOFT_RELEASE);

	if (ret < 0)
		debug("QCE1204: Analog soft reset failed: %d\n", ret);

	return ret;
}

/* Wait for 10G Base-R Link Up */
static int qce1204_pcs_10g_linkup(struct phy_device *phydev)
{
	u16 xpcs_data = 0;
	u32 retries = 100;
	u32 linkup = 0;

	while (linkup != QCE1204_PCS_MMD3_10GBASE_UP) {
		mdelay(1);
		if (retries-- == 0) {
			debug("QCE1204: 10G Base-R link timeout\n");
			return -ETIMEDOUT;
		}

		xpcs_data = qce1204_pcs_read_mmd(phydev, MDIO_MMD_PCS,
						 QCE1204_PCS_MMD3_10GBASE_PCS_STATUS1);
		linkup = (xpcs_data & QCE1204_PCS_MMD3_10GBASE_UP);
	}
	return 0;
}

/* XPCS Soft Reset */
static int qce1204_pcs_soft_reset(struct phy_device *phydev)
{
	int ret;
	u16 pcs_data = 0;
	u32 retries = 100;
	u32 reset_done = QCE1204_PCS_MMD3_XPCS_SOFT_RESET;

	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
				     QCE1204_PCS_MMD3_DIG_CTRL1,
				     0x8000,
				     QCE1204_PCS_MMD3_XPCS_SOFT_RESET);
	if (ret < 0) {
		debug("QCE1204: XPCS soft reset failed: %d\n", ret);
		return ret;
	}

	while (reset_done) {
		mdelay(1);
		if (retries-- == 0) {
			debug("QCE1204: XPCS soft reset timeout\n");
			return -ETIMEDOUT;
		}

		pcs_data = qce1204_pcs_read_mmd(phydev, MDIO_MMD_PCS,
						QCE1204_PCS_MMD3_DIG_CTRL1);
		reset_done = (pcs_data & QCE1204_PCS_MMD3_XPCS_SOFT_RESET);
	}
	return 0;
}

/*
 * ABILITY FIX-UP AND EEE SUPPORT
 */

/* Disable half-duplex modes for QUSGMII interface */
static int qce1204_ability_fix_up(struct phy_device *phydev)
{
	/* Clear half-duplex bits from supported features */
	phydev->supported &= ~(SUPPORTED_10baseT_Half | SUPPORTED_100baseT_Half);

	/* Clear half-duplex bits from advertised features */
	phydev->advertising &= ~(ADVERTISED_10baseT_Half | ADVERTISED_100baseT_Half);

	return 0;
}

/* Enable EEE (802.3az Energy Efficient Ethernet) */
static int qce1204_pcs_8023az_enable(struct phy_device *phydev)
{
	u16 pcs_data = 0;
	int ret;

	/* Check if link partner supports EEE */
	pcs_data = qce1204_pcs_read_mmd(phydev, MDIO_MMD_PCS,
					QCE1204_PCS_MMD3_AN_LP_BASE_ABL2);
	if (!(pcs_data & QCE1204_PCS_MMD3_XPCS_EEE_CAP))
		return 0;  /* Not an error, just not supported */

	/* Configure EEE related timer */
	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
				     QCE1204_PCS_MMD3_EEE_MODE_CTRL,
				     0x0f40,
				     QCE1204_PCS_MMD3_EEE_RES_REGS |
				     QCE1204_PCS_MMD3_EEE_SIGN_BIT_REGS);
	if (ret < 0)
		return ret;

	/* Configure TX timer */
	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
				     QCE1204_PCS_MMD3_EEE_TX_TIMER,
				     0x1fff,
				     QCE1204_PCS_MMD3_EEE_TSL_REGS |
				     QCE1204_PCS_MMD3_EEE_TLU_REGS |
				     QCE1204_PCS_MMD3_EEE_TWL_REGS);
	if (ret < 0)
		return ret;

	/* Configure RX timer */
	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
				     QCE1204_PCS_MMD3_EEE_RX_TIMER,
				     0x1fff,
				     QCE1204_PCS_MMD3_EEE_100US_REG_REGS |
				     QCE1204_PCS_MMD3_EEE_RWR_REG_REGS);
	if (ret < 0)
		return ret;

	/* Enable TRN_LPI */
	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
				     QCE1204_PCS_MMD3_EEE_MODE_CTRL1,
				     0x101,
				     QCE1204_PCS_MMD3_EEE_TRANS_LPI_MODE |
				     QCE1204_PCS_MMD3_EEE_TRANS_RX_LPI_MODE);
	if (ret < 0)
		return ret;

	/* Enable TX/RX LPI pattern */
	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
				     QCE1204_PCS_MMD3_EEE_MODE_CTRL,
				     0x3,
				     QCE1204_PCS_MMD3_EEE_EN);
	return 0;
}

/*
 * CDT THRESHOLD INITIALIZATION
 */

/* Initialize CDT thresholds for cable diagnostics */
static int qce1204_phy_cdt_thresh_init(struct phy_device *phydev)
{
	int ret = 0;

	/* Write all CDT threshold control registers */
	ret = phy_write(phydev, MDIO_MMD_PCS,
			QCE1204_MMD3_CDT_THRESH_CTRL2,
			QCE1204_MMD3_CDT_THRESH_CTRL2_VAL);
	if (ret)
		goto fail;

	ret = phy_write(phydev, MDIO_MMD_PCS,
			QCE1204_MMD3_CDT_THRESH_CTRL3,
			QCE1204_MMD3_CDT_THRESH_CTRL3_VAL);
	if (ret)
		goto fail;

	ret = phy_write(phydev, MDIO_MMD_PCS,
			QCE1204_MMD3_CDT_THRESH_CTRL4,
			QCE1204_MMD3_CDT_THRESH_CTRL4_VAL);
	if (ret)
		goto fail;

	ret = phy_write(phydev, MDIO_MMD_PCS,
			QCE1204_MMD3_CDT_THRESH_CTRL5,
			QCE1204_MMD3_CDT_THRESH_CTRL5_VAL);
	if (ret)
		goto fail;

	ret = phy_write(phydev, MDIO_MMD_PCS,
			QCE1204_MMD3_CDT_THRESH_CTRL6,
			QCE1204_MMD3_CDT_THRESH_CTRL6_VAL);
	if (ret)
		goto fail;

	ret = phy_write(phydev, MDIO_MMD_PCS,
			QCE1204_MMD3_CDT_THRESH_CTRL7,
			QCE1204_MMD3_CDT_THRESH_CTRL7_VAL);
	if (ret)
		goto fail;

	ret = phy_write(phydev, MDIO_MMD_PCS,
			QCE1204_MMD3_CDT_THRESH_CTRL9,
			QCE1204_MMD3_CDT_THRESH_CTRL9_VAL);
	if (ret)
		goto fail;

	ret = phy_write(phydev, MDIO_MMD_PCS,
			QCE1204_MMD3_CDT_THRESH_CTRL13,
			QCE1204_MMD3_CDT_THRESH_CTRL13_VAL);
	if (ret)
		goto fail;

	ret = phy_write(phydev, MDIO_MMD_PCS,
			QCE1204_MMD3_CDT_THRESH_CTRL14,
			QCE1204_MMD3_CDT_THRESH_CTRL14_VAL);
	if (ret)
		goto fail;

	return 0;

fail:
	return ret;
}

/*
 * QUSGMII MODE CONFIGURATION
 */

/* Internal helper function for QUSGMII mode configuration */
static int qce1204_pcs_qusgmii_mode_set_internal(struct phy_device *phydev)
{
	int ret = 0;
	u32 channel = 0;

	/* Assert XPCS */
	qce1204_fpga_xpcs_assert(phydev);

	/* Select XPCS mode */
	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PMAPMD,
				     QCE1204_PCS_MMD1_MODE_CTRL,
				     0x1f00, QCE1204_PCS_MMD1_XPCS_MODE);
	if (ret < 0)
		return ret;

	/* Set GMII datapass to QUSGMII */
	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PMAPMD,
				     QCE1204_PCS_MMD1_GMII_DATAPASS_SEL,
				     QCE1204_PCS_MMD1_DATAPASS_MASK,
				     QCE1204_PCS_MMD1_DATAPASS_QUSGMII);
	if (ret < 0)
		return ret;

	/* Reset and release PCS GMII/XGMII and PHY GMII */
	for (channel = 1; channel <= 4; channel++)
		qce1204_fpga_port_clk_reset(phydev);

	/* Analog soft reset assert */
	ret = qce1204_pcs_assert(phydev, true);
	if (ret < 0)
		return ret;
	mdelay(10);

	/* Analog soft reset release */
	ret = qce1204_pcs_assert(phydev, false);
	if (ret < 0)
		return ret;

	/* Wait for calibration */
	qce1204_pcs_calibration(phydev);

	/* Enable SSCG */
	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PMAPMD,
				     QCE1204_PCS_MMD1_CDA_CONTROL1,
				     0x8, QCE1204_PCS_MMD1_SSCG_ENABLE);
	if (ret < 0)
		return ret;

	/* De-assert XPCS */
	qce1204_fpga_xpcs_deassert(phydev);

	/* PHY software reset for all 4 channels */
	for (channel = 1; channel <= 4; channel++) {
		struct phy_device local_phydev;

		memcpy(&local_phydev, phydev, sizeof(struct phy_device));
		local_phydev.addr = phydev->addr + channel - 1;

		ret = phy_write(&local_phydev, MDIO_DEVAD_NONE, MII_BMCR, BMCR_RESET);
		if (ret < 0) {
			debug("QCE1204: PHY reset failed for channel %d\n", channel);
			return ret;
		}
	}

	/* Set BaseR mode */
	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
				     QCE1204_PCS_MMD3_PCS_CTRL2,
				     0xf,
				     QCE1204_PCS_MMD3_PCS_TYPE_10GBASE_R);
	if (ret < 0)
		return ret;

	/* Wait for 10G Base-R link up */
	ret = qce1204_pcs_10g_linkup(phydev);
	if (ret < 0)
		return ret;

	/* Enable QUSGMII mode */
	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
				     QCE1204_PCS_MMD3_DIG_CTRL1,
				     0x200, QCE1204_PCS_MMD3_QUSGMII_EN);
	if (ret < 0)
		return ret;

	/* Set QUSGMII mode */
	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
				     QCE1204_PCS_MMD3_VR_RPCS_TPC,
				     0x1c00, QCE1204_PCS_MMD3_QUSGMII_MODE);
	if (ret < 0)
		return ret;

	/* Set AM interval */
	ret = qce1204_pcs_write_mmd(phydev, MDIO_MMD_PCS,
				    QCE1204_PCS_MMD3_MII_AM_INTERVAL,
				    QCE1204_PCS_MMD3_MII_AM_INTERVAL_VAL);
	if (ret < 0)
		return ret;

	/* XPCS soft reset */
	ret = qce1204_pcs_soft_reset(phydev);

	return ret;
}

/* Main QUSGMII mode configuration function */
static int qce1204_pcs_qusgmii_mode_set(struct phy_device *phydev)
{
	int ret = 0;
	u32 channel = 0;

	/* FPGA GTY reset */
	qce1204_fpga_gty_reset(phydev);

	/* Step 1: Disable IPG tuning bypass */
	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PMAPMD,
				     QCE1204_PCS_MMD1_BYPASS_TUNING_IPG,
				     0x0fff, 0);
	if (ret < 0)
		return ret;

	/* Step 2: Disable PCS GMII/XGMII clock and PHY GMII clock */
	for (channel = 1; channel <= 4; channel++)
		/* Clock disable would go here in full implementation */;

	/* Step 3: Configure QUSGMII mode */
	ret = qce1204_pcs_qusgmii_mode_set_internal(phydev);
	if (ret < 0)
		return ret;

	/* Step 4: Configure all 4 channels */
	for (channel = 1; channel <= 4; channel++) {
		/* Enable auto-neg complete interrupt, MII 4-bits mode, TX config */
		ret = qce1204_pcs_modify_channel_mmd(phydev, channel,
						     QCE1204_PCS_MMD_MII_AN_INT_MSK,
						     0x109,
						     QCE1204_PCS_MMD_AN_COMPLETE_INT |
						     QCE1204_PCS_MMD_MII_4BITS_CTRL |
						     QCE1204_PCS_MMD_TX_CONFIG_CTRL);
		if (ret < 0) {
			debug("QCE1204: Channel %d config failed\n", channel);
			return ret;
		}

		/* Enable autoneg ability */
		ret = qce1204_pcs_modify_channel_mmd(phydev, channel,
						     QCE1204_PCS_MMD_MII_CTRL,
						     QCE1204_PCS_MMD_MII_AN_ENABLE,
						     QCE1204_PCS_MMD_MII_AN_ENABLE);
		if (ret < 0) {
			debug("QCE1204: Channel %d autoneg failed\n", channel);
			return ret;
		}

		/* Disable TICD (TX IPG Check Disable) */
		ret = qce1204_pcs_modify_channel_mmd(phydev, channel,
						     QCE1204_PCS_MMD_MII_XAUI_MODE_CTRL,
						     QCE1204_PCS_MMD_TX_IPG_CHECK_DISABLE,
						     QCE1204_PCS_MMD_TX_IPG_CHECK_DISABLE);
		if (ret < 0) {
			debug("QCE1204: Channel %d TICD failed\n", channel);
			return ret;
		}

		/* Enable PHY mode control to sync PHY link info to XPCS */
		ret = qce1204_pcs_modify_channel_mmd(phydev, channel,
						     QCE1204_PCS_MMD_MII_DIG_CTRL,
						     BIT(0),
						     QCE1204_PCS_MMD_PHY_MODE_CTRL_EN);
		if (ret < 0) {
			debug("QCE1204: Channel %d PHY mode failed\n", channel);
			return ret;
		}
	}

	/* Step 5: Enable EEE for XPCS */
	ret = qce1204_pcs_8023az_enable(phydev);
	if (ret < 0)
		/* Don't return error - EEE is optional */;
	return 0;
}

/*
 * SPEED/LINK MANAGEMENT
 */

static int qce1204_phy_channel_get(struct phy_device *phydev)
{
	/* Calculate channel number based on address offset */
	return (phydev->addr - phydev->addr + 1);
}

static int qce1204_phy_fifo_reset(struct phy_device *phydev, bool enable)
{
	u16 phy_data = 0;

	if (!enable)
		phy_data |= QCE1204_PHY_FIFO_RESET;

	return phy_modify(phydev, MDIO_MMD_VEND2,
			  QCE1204_PHY_CONTROL,
			  QCE1204_PHY_FIFO_RESET,
			  phy_data);
}

/* PCS QUSGMII reset for specific channel */
static int qce1204_pcs_qusgmii_reset(struct phy_device *phydev, u32 channel)
{
	int ret;

	/* Clear the channel bit to assert reset */
	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PMAPMD,
				     QCE1204_PCS_MMD1_QUSGMII_RESET,
				     BIT(channel - 1), 0);
	if (ret < 0)
		return ret;

	mdelay(1);

	/* Set the channel bit to release reset */
	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PMAPMD,
				     QCE1204_PCS_MMD1_QUSGMII_RESET,
				     BIT(channel - 1), BIT(channel - 1));

	return ret;
}

/* PCS QUSGMII function reset for specific channel */
static int qce1204_pcs_qusgmii_function_reset(struct phy_device *phydev, u32 channel)
{
	int ret;

	/* For channel 1, use different register */
	if (channel == 1) {
		ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
					     QCE1204_PCS_MMD_MII_DIG_CTRL,
					     0x400, QCE1204_PCS_MMD3_QUSGMII_FIFO_RESET);
	} else {
		/* For channels 2-4, use channel-specific MMD */
		int mmd_id = qce1204_pcs_mmd_get(phydev, channel);

		if (mmd_id < 0)
			return -EOPNOTSUPP;

		ret = qce1204_pcs_modify_mmd(phydev, mmd_id,
					     QCE1204_PCS_MMD_MII_DIG_CTRL,
					     0x20, 0x20);
	}

	return ret;
}

static int qce1204_phy_qusgmii_speed_fix_up(struct phy_device *phydev)
{
	u32 channel;
	int ret;

	channel = qce1204_phy_channel_get(phydev);

	/* Clock management would go here in full implementation */

	mdelay(100);

	/* PCS QUSGMII reset */
	ret = qce1204_pcs_qusgmii_reset(phydev, channel);
	if (ret < 0)
		return ret;

	/* PCS QUSGMII function reset */
	ret = qce1204_pcs_qusgmii_function_reset(phydev, channel);
	if (ret < 0)
		return ret;

	/* FIFO reset enable */
	ret = qce1204_phy_fifo_reset(phydev, true);
	if (ret < 0)
		return ret;
	mdelay(1);

	/* FIFO reset disable if link is up */
	if (phydev->link) {
		ret = qce1204_phy_fifo_reset(phydev, false);
		if (ret < 0)
			return ret;
	}

	/* Change IPG from 10 to 11 for 1G speed, clear for other speeds */
	ret = phy_modify(phydev, MDIO_MMD_AN,
			 QCE1204_PHY_MMD7_IPG_OP,
			 QCE1204_PHY_IPG_10_TO_11_EN,
			 phydev->speed == SPEED_1000 ? QCE1204_PHY_IPG_10_TO_11_EN : 0);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * MAIN DRIVER FUNCTIONS
 */

static int qce1204_probe(struct phy_device *phydev)
{
	return 0;
}

static int qce1204_config(struct phy_device *phydev)
{
	int ret = 0;

	/* Step 1: Configure QUSGMII mode (includes all 4 channels) */
	ret = qce1204_pcs_qusgmii_mode_set(phydev);
	if (ret < 0) {
		debug("QCE1204: QUSGMII config failed: %d\n", ret);
		return ret;
	}

	/* Step 2: Reduce delay for 2.5G EEE wake-up signal */
	ret = phy_modify(phydev, MDIO_MMD_PCS,
			 QCE1024_PHY_2P5G_EEE_TX_LPI_CTRL,
			 QCE1024_PHY_TX_LPI_DELAY_SEL_MASK,
			 QCE1024_PHY_TX_LPI_DELAY_SEL_1);
	if (ret < 0)
		/* Non-fatal - continue */;

	/* Step 3: Initialize CDT thresholds for cable diagnostics */
	ret = qce1204_phy_cdt_thresh_init(phydev);
	if (ret < 0) {
		debug("QCE1204: CDT init failed: %d\n", ret);
		return ret;
	}

	/* Step 4: Apply ability fix-up (disable half-duplex) */
	ret = qce1204_ability_fix_up(phydev);
	if (ret < 0) {
		debug("QCE1204: Ability fix-up failed: %d\n", ret);
		return ret;
	}
	return 0;
}

static int qce1204_startup(struct phy_device *phydev)
{
	u16 phy_data;
	u16 speed_bits;
	int link, speed;
	int old_link = phydev->link;
	int old_speed = phydev->speed;
	int ret;

	/* Read PHY status */
	phy_data = phy_read(phydev, MDIO_MMD_VEND2, QCE1204_PHY_SPEC_STATUS);

	/* Determine link status */
	link = (phy_data & QCE1204_PHY_SS_LINK_STATUS) ? 1 : 0;

	/* Determine speed */
	speed_bits = phy_data & QCE1204_PHY_SS_SPEED_MASK;

	switch (speed_bits) {
	case QCE1204_PHY_SS_SPEED_2500:
		speed = SPEED_2500;
		break;
	case QCE1204_PHY_SS_SPEED_1000:
		speed = SPEED_1000;
		break;
	case QCE1204_PHY_SS_SPEED_100:
		speed = SPEED_100;
		break;
	case QCE1204_PHY_SS_SPEED_10:
		speed = SPEED_10;
		break;
	default:
		speed = SPEED_UNKNOWN;
	}

	/* Determine duplex */
	if (phy_data & QCE1204_PHY_SS_DUPLEX_FULL)
		phydev->duplex = DUPLEX_FULL;
	else
		phydev->duplex = DUPLEX_HALF;

	/* Update link and speed */
	phydev->link = link;
	phydev->speed = speed;

	/* If link or speed changed, perform speed fixup */
	if (old_link != link || old_speed != speed) {
		ret = qce1204_phy_qusgmii_speed_fix_up(phydev);
		if (ret < 0) {
			debug("QCE1204: Speed fixup failed: %d\n", ret);
			return ret;
		}
	}

	return 0;
}

/*
 * DRIVER REGISTRATION
 */

U_BOOT_PHY_DRIVER(qce1204_driver) = {
	.name = "QCE1204 PHY Driver",
	.uid = QCE1204_PHY_ID,
	.mask = 0xfffffff0,
	.features = PHY_GBIT_FEATURES,
	.probe = &qce1204_probe,
	.config = &qce1204_config,
	.startup = &qce1204_startup,
	.shutdown = &genphy_shutdown,
};
