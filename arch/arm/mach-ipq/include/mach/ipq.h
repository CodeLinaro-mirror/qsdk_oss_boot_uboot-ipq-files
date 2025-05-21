/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <asm/global_data.h>
#include <asm/cache.h>
#ifndef CONFIG_ARM64
#include <asm/system.h>
#include <asm/armv7.h>
#endif
#include <cpu_func.h>
#include <linux/types.h>
#include <linux/sizes.h>
#include <mach/smem_info.h>
#include <mach/scm.h>
#include <part.h>
#include <stddef.h>
#include <linux/err.h>
#include <asm/io.h>
#include <dm.h>
#include <memalign.h>
#include <smem.h>
#if CONFIG_IS_ENABLED(DM_SPI_FLASH)
#include <spi.h>
#include <spi_flash.h>
#endif

#if defined(CONFIG_TARGET_IPQ9574)
#include <asm/arch/ipq9574.h>
#endif
#if defined(CONFIG_TARGET_IPQ5332)
#include <asm/arch/ipq5332.h>
#endif
#if defined(CONFIG_TARGET_IPQ5424)
#include <asm/arch/ipq5424.h>
#endif

#ifndef __IPQ_H__
#define __IPQ_H__

/***********************************************************************
 * Global and constant
 **********************************************************************/
#define BOARD_DTS_MAX_NAMELEN				32
#define BLK_PART_GET_INFO_S(_ptr, _name, _info, _fl, _verbose)	\
	do {							\
		(&(_ptr))->name = _name;			\
		(&(_ptr))->info = _info;			\
		(&(_ptr))->flash_type = _fl;			\
		(&(_ptr))->devnum = 0;				\
		(&(_ptr))->verbose = _verbose;			\
	} while (0)

#define WHICH_FLASH(p)			(((p)->attr & 0xff000000) >> 24)
#define SECURE_BOARD			BIT(8)
#define ATF_ENABLED			BIT(9)
#define KERNEL_AUTH_SUCCESS		BIT(10)
#define ROOTFS_AUTH_SUCCESS		BIT(11)
#define ACTIVE_BOOT_SET			BIT(12)
#define INVALID_BOOT			BIT(13)
#define FLASH_TYPE_MASK			0xFF

#ifndef IPQ_NAND_FLASH_VALID_BIT
#define IPQ_NAND_FLASH_VALID_BIT        3
#endif
#define CFG_IPQ_NAND_PART               BIT(IPQ_NAND_FLASH_VALID_BIT)

#if defined(CONFIG_BOOTCONFIG_V3)
#define SET_AB_USABLE			(0x0UL)
#define DONT_USE_SET_A			(0x1UL)
#define DONT_USE_SET_B			(0x2UL)
#define DONT_USE_SET_AB			(0x3UL)
#define BOOT_SET_A			0x0
#define BOOT_SET_B			0x1
#define BOOT_SET_INVALID		0xFFFF
#define BC_UBOOT_OWNER			0x1
#define BOOTCONFIG_V3_MAGIC		(0x72637279UL)
#endif

/*
 * TCSR bit
 */
#define DLOAD_MAGIC_COOKIE                      0x10
#define DLOAD_DISABLED                          0x40
#define DLOAD_ENABLE                            BIT(4)
#define DLOAD_DISABLE                           (~BIT(4))
#define CRASHDUMP_RESET                         BIT(11)
#ifndef MARK_UBOOT_MILESTONE
#define MARK_UBOOT_MILESTONE                    BIT(8)
#endif

#if defined(CONFIG_BOOTCONFIG_V2)
#define BOOTCONFIG_HEALTH_MASK                  BIT(13)
#define BOOTCONFIG1_HEALTH_MASK                 BIT(14)
#endif

#define ROOTFS_AUTH_EN				0x20

extern struct ipq_board_info *ipq_bdinfo;
extern struct multidtb_config *g_board_dtb_info;
extern struct dts_fixup *mmc_fixup;
extern struct dts_fixup *usb_fixup;

/**********************************************************************
 * Structure enum and static
 *********************************************************************/
enum bank {
	INVALID_BOOT_AB = -1,
	VALID_BOOT_A,
	VALID_BOOT_B,
};

enum {
	SECURE_SYS_UPGRADE      = 0,
};

#ifdef CONFIG_DTB_RESELECT
struct machid_dts_map {
	int machid;
	char *dts;
	char *priconfig;
	char *secconfig;
};

struct multidtb_config {
	struct machid_dts_map *list;
	int ncount;
	int index;
	char dts_base[BOARD_DTS_MAX_NAMELEN];
	char dts_name[BOARD_DTS_MAX_NAMELEN];
};
#endif /* CONFIG_DTB_RESELECT */

#if defined(CONFIG_MMC) || defined(CONFIG_NOR_BLK)
struct gpt_pte_info {
	gpt_entry *gpt_pte;
	int ncount;
};
#endif

struct ipq_board_info {
#if defined(CONFIG_SMEM) && defined(CONFIG_MSM_SMEM)
	struct udevice *smem_dev;
	struct ipq_smem_flash_info smem_info;
	struct ipq_smem_bootconfig_info bconfig_info;
	struct smem_ptable *ptable;
	struct soc_info ipq_socinfo;
#endif
#if CONFIG_IS_ENABLED(DM_SPI_FLASH)
	struct spi_flash *sf;
#endif
#if defined(CONFIG_MMC) || defined(CONFIG_NOR_BLK)
	struct gpt_pte_info mmc_gpt_pte;
	struct gpt_pte_info nor_gpt_pte;
#endif
};

#if IS_ENABLED(CONFIG_MMC) || IS_ENABLED(CONFIG_NOR_BLK)
/* BLK part info */
struct blkpart_info {
	struct disk_partition *info;
	struct blk_desc *desc;
	char *name;
	int flash_type;
	int devnum;
	int isnand;
	bool verbose;
};
#endif

struct mbn_header {
	unsigned int image_type;
	unsigned int header_vsn_num;
	unsigned int image_src;
	unsigned int image_dest_ptr;
	unsigned int image_size;
	unsigned int code_size;
	unsigned int signature_ptr;
	unsigned int signature_size;
	unsigned int cert_chain_ptr;
	unsigned int cert_chain_size;
};

struct auth_cmd_buf {
	unsigned long type;
	unsigned long size;
	unsigned long addr;
};

struct dts_fixup {
	char *path;
	char *fixup[5];
	int ncount;
};

/*********************************************************************
 * Function declaration
 ********************************************************************/
/**
 * ipq_update_board_name() - Update rdp for non available dts rdps
 */
void ipq_update_board_name(int machid, struct multidtb_config *dtb);
/**
 * ipq_board_early_init_f() - Do board specific early init f
 */
void ipq_board_early_init_f(void);
/**
 * ipq_board_read_smem_info() - read and save smem information
 *
 * @pbdinfo - Board context  pointer to save smem infrmation.
 */
void ipq_board_read_smem_info(struct ipq_board_info *pbdinfo);
/**
 * ipq_board_info() - Get board context pointer
 *
 * Return struct ipq_board_info global pointer
 */
struct ipq_board_info *ipq_get_bdinfo(void);
/**
 * ipq_get_smem_info() - Get smem information pointer
 *
 * Return struct ipq_smem_flash_info - ipq_bdinfo->smem_info
 */
struct ipq_smem_flash_info *ipq_get_smem_info(void);
/**
 * ipq_get_socinfo() - Get SoC information
 *
 * Return struct soc_info - ipq_bdinfo->ipq_socinfo
 */
struct soc_info *ipq_get_socinfo(void);
/**
 * ipq_get_part_table() - Get SMEM partition table
 *
 * Return struct smem_ptable - ipq_bdinfo->ptable
 */
struct smem_ptable *ipq_get_part_table(void);
/**
 * ipq_get_valid_bank() - Get valid bank details
 *
 * Return 1 for secondary , 0 for primary
 */
int ipq_get_valid_bank(void);
/**
 * ipq_runtime_sf_env_update() - Update the ENV offset dynamically.
 * Applicable only for NOR flash
 */
void ipq_runtime_sf_env_update(void);
#if IS_ENABLED(CONFIG_MMC) || IS_ENABLED(CONFIG_NOR_BLK)
/**
 * ipq_part_get_info_by_name() - Get partition information from GPT
 *
 * @blkpart this structure contails of class and name of blk device
 * Return 0 if Found , otherwise error code
 */
int ipq_part_get_info_by_name(struct blkpart_info *blkpart);
#endif
/**
 * get_gpt_entry() - Get the GPT entry table
 *
 * @dev_desc blk device descriptor details
 * Return valid secondary GPT table. else NULL if Invalid GPT
 */
gpt_entry *get_gpt_entry(struct blk_desc *dev_desc);
/**
 * ipq_spi_probe() - Probe SPI-NOR
 *
 * Return spi_flash pointer if success otherwise NULL if failed
 */
struct spi_flash *ipq_spi_probe(void);
/**
 * ipq_board_late_init() - Board specific late init seq
 *
 * Return 0
 */
int ipq_board_late_init(void);
/**
 * is_board_support_image_auth() -  Find current board config support Image
 * Authentication or not
 * Return 1 if support otherwise 0.
 */
uint32_t is_board_support_image_auth(void);
/**
 * is_part_exceed_flash_size() - Find if smem partition exceed flash size or not
 *
 * @p smem table pointer
 * @psize total size of partition , start + size
 * Return true if exceed otherwise false.
 */
bool is_part_exceed_flash_size(struct smem_ptn *p, uint64_t psize);
/**
 * ipq_smem_get_flash_size() -  Find flash size of NOR or NAND
 *
 * @flash_type 0 for NOR , 1 for NAND
 * Return size of flash if found otherwise 0
 */
uint64_t ipq_smem_get_flash_size(uint32_t flash_type);
/**
 * ipq_get_part_block_size() -  Find erase blk size of NOR or NAND
 *
 * @p SMEM table pointer
 * @sfi SMEM table info
 * Return block size based on NAND or NOR based on partition attribute.
 */
uint32_t ipq_get_part_block_size(struct smem_ptn *p,
				struct ipq_smem_flash_info *sfi);
/**
 * ipq_get_current_board_flash_config() -  Find current board flash config
 *
 * @flash_type SMEM based flash type
 * Return NOR PLUS varinat otheriwse input flash type.
 */
int ipq_get_current_board_flash_config(int flash_type);
/**
 * ipq_getpart_offset_size - retrieve partition offset and size
 * @part_name - partition name
 * @offset - location where the offset of partition to be stored
 * @size - location where partition size to be stored
 *
 * retrieve partition offset and size in bytes with respect to the
 * partition specific flash block size
 */
int ipq_getpart_offset_size(char *part_name, uint32_t *offset, uint32_t *size);
/**
 * This function should only be used when sfi->flash_type is
 * SMEM_BOOT_SPI_FLASH
 * retrieve the which_flash flag based on partition name.
 * flash_var is 1 if partition is in NAND.
 * flash_var is 0 if partition is in NOR.
 * flash_var is -1 if partition is in EMMC.
 */
uint32_t ipq_find_flash_by_name(char *part_name);
/**
 * ipq_smem_getpart - retrieve partition start and size
 * @part_name: partition name
 * @start: location where the start offset is to be stored
 * @size: location where the size is to be stored
 *
 * retrieve the start offset in blocks and size in blocks, of the
 * specified partition.
 */
int ipq_smem_getpart(char *part_name, uint32_t *start, uint32_t *size);
/**
 * mibib_ptable_init - initializes SMEM partition table
 *
 * @addr smem address in DDR
 * Return 0 if success otherwise error code.
 */
int mibib_ptable_init(unsigned int *addr);
/**
 * ipq_get_kernel_fs_part_details() - Update rootfs and HLOS partition
 *					details in smem
 * @flash_type flash type like SPI-NOR etc
 */
void ipq_get_kernel_fs_part_details(int flash_type);
/**
 * gpt_find_which_flash() - Find partition in NOR or NAND in NOT-GPT flash type
 *
 * @p partition information
 * Return 1 for nand , 0 for nor
 */
int gpt_find_which_flash(gpt_entry *p);
/**
 * ipq_ft_board_setup() - IPQ board specific fixup
 *
 * @blob kernel dts for fixup
 * @bd board info
 * Return 0
 */
int ipq_ft_board_setup(void *blob, struct bd_info *bd);
/**
 * parse_fdt_fixup() - Parse fixup provided in ENV
 *
 * @buf buffer with fixup strings
 * @blob of kernel dts
 */
void parse_fdt_fixup(char *buf, void *blob);
/**
 * ipq_get_partition_data() - read the data from partition at given offset.
 *
 * @part_name Partition name
 * @offset offset at partition
 * @buf to copy the read data
 * @size size to be read
 * @fl_type flash type
 * Return 0 if success otherwise error code.
 */
int ipq_get_partition_data(char *part_name, uint32_t offset, uint8_t *buf,
			size_t size, uint32_t fl_type);
/**
 * set_bootargs() - Set bootargs for kernel boot up
 *
 * Return 0 if success otherwise error code.
 */
int set_bootargs(void);
/**
 * config_select() - Set config(dts) for kernel boot up
 *
 * Return 0 if success otherwise error code.
 */
int config_select(void);
/**
 * boot_kernel() - Boot kernel
 *
 * Return if not successful
 */
int boot_kernel(void);
/**
 * image_authentication() - Auth the kernel images and rootfs in secure board
 *
 * Return 0 if success otherwise error code
 */
int image_authentication(void);
/**
 * read_kernel() - Read kernel images for kernel boot up
 *
 * Return 0 if success otherwise error code
 */
int read_kernel(void);
/**
 * check_bootconfig() - Find bootconfig active partition for kernel boot up
 *
 * Return 0 if success otherwise error code.
 */
int check_bootconfig(void);
/**
 * ipq_get_soc_hw_version() - Read SoC HW version
 *
 * Return HW version from TCSR read only register
 */
uint32_t ipq_get_soc_hw_version(void);
/**
 * ipq_setup_board_default_env() - Setup board specific default env.
 *
 */
void ipq_setup_board_default_env(void);
/**
 * ipq_set_ethmac_addr() - Push MAC address to env
 *
 */
void ipq_set_ethmac_addr(void);
/**
 * ipq_get_eth_mac_address() - Read MAC from ART partition.
 *
 * @enetaddr buffer for save MAC
 * @no_of_macs count
 * Return 0 or higher for success , negative error for failure.
 */
int ipq_get_eth_mac_address(uint8_t *enetaddr, int no_of_macs);
/**
 * ipq_read_tcsr_boot_misc() - Read TCSR BOOT MISK register
 *
  Return value of regsiter
 */
int ipq_read_tcsr_boot_misc(void);
/**
 * write_tcsr_boot_misc_reg() - write TCSR BOOT MISK register
 *
 * @mask masking bit
 * value value to be written
 * Return 0 if success , othwerwise error code.
 */
int write_tcsr_boot_misc_reg(uint32_t mask, uint32_t value);
/**
 * is_atf_enbled() - check ATF enabled or not
 *
 * Return true if enabled otherwise flase
 */
bool is_atf_enbled(void);
/**
 * ipq_smem_get_item() - Get the data from smem memory
 *
 * @ptr for store the data
 * @type - which type of data required like item type
 * @def default value
 * size of memory to be read
 * Return true if enabled otherwise flase
 */
void ipq_smem_get_item(void *ptr, int type, int def, size_t size);
/**
 * ipq_smem_get_item() - Get the data from smem memory
 *
 * @ptr for store the data
 * @type - which type of data required like item type
 * @def default value
 * size of memory to be read
 * Return true if enabled otherwise flase
 */
void ipq_smem_get_itemv2(void **ptr, int type, int def, size_t size);
/**
 * ipq_update_lmb_reservation() - update the lmb reserved region.
 *
 */
void ipq_update_lmb_reservation(void);
#endif
