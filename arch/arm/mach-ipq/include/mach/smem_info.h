// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2023-2025,Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _SMEM_INFO_H_
#define _SMEM_INFO_H_

/*
 * SMEM supported item IDs
 */

typedef enum {
	SMEM_SPINLOCK_ARRAY = 7,
	SMEM_AARM_PARTITION_TABLE = 9,
	SMEM_HW_SW_BUILD_ID = 137,
	SMEM_USABLE_RAM_PARTITION_TABLE = 402,
	SMEM_POWER_ON_STATUS_INFO = 403,
	SMEM_MACHID_INFO_LOCATION = 425,
	SMEM_IMAGE_VERSION_TABLE = 469,
	SMEM_BOOT_FLASH_TYPE = 498,
	SMEM_BOOT_FLASH_INDEX = 499,
	SMEM_BOOT_FLASH_CHIP_SELECT = 500,
	SMEM_BOOT_FLASH_BLOCK_SIZE = 501,
	SMEM_BOOT_FLASH_DENSITY = 502,
	SMEM_BOOT_DUALPARTINFO = 503,
	SMEM_PARTITION_TABLE_OFFSET = 504,
	SMEM_SPI_FLASH_ADDR_LEN = 505,
	SMEM_TRY_MODE_INPROGRESS = 507,
	SMEM_ATF_ENABLE = 509,
	SMEM_EDL_MODE = 510,
	SMEM_FIRST_VALID_TYPE = SMEM_SPINLOCK_ARRAY,
	SMEM_LAST_VALID_TYPE = SMEM_EDL_MODE,
	SMEM_MAX_SIZE = SMEM_EDL_MODE + 1,
} smem_mem_type_t;

/*
 * RAM partition info
 */

#define _SMEM_RAM_PTABLE_MAGIC_1	0x9DA5E0A8
#define _SMEM_RAM_PTABLE_MAGIC_2	0xAF9EC4E2

#define RAM_PARTITION_SDRAM 		14
#define RAM_PARTITION_SYS_MEMORY 	1

struct ram_partition_entry
{
	char name[CONFIG_RAM_PART_NAME_LENGTH];
				/**< Partition name, unused for now */
	u64 start_address;	/**< Partition start address in RAM */
	u64 length;		/**< Partition length in RAM in Bytes */
	u32 partition_attribute;/**< Partition attribute */
	u32 partition_category;	/**< Partition category */
	u32 partition_domain;	/**< Partition domain */
	u32 partition_type;	/**< Partition type */
	u32 num_partitions;	/**< Number of partitions on device */
	u32 hw_info;		/**< hw information such as type and freq */
	u8 highest_bank_bit;	/**< Highest bit corresponding to a bank */
	u8 reserve0;		/**< Reserved for future use */
	u8 reserve1;		/**< Reserved for future use */
	u8 reserve2;		/**< Reserved for future use */
	u32 reserved5;		/**< Reserved for future use */
	u64 available_length;	/**< Available Part length in RAM in Bytes */
};

struct usable_ram_partition_table
{
	u32 magic1;		/**< Magic number to identify valid RAM
					partition table */
	u32 magic2;		/**< Magic number to identify valid RAM
					partition table */
	u32 version;		/**< Version number to track structure
					definition changes and maintain
					backward compatibilities */
	u32 reserved1;		/**< Reserved for future use */

	u32 num_partitions;	/**< Number of RAM partition table entries */

	u32 reserved2;		/** < Added for 8 bytes alignment of header */

	/** RAM partition table entries */
	struct ram_partition_entry ram_part_entry[CONFIG_RAM_NUM_PART_ENTRIES];
};


/*
 * SoC info
 */

#define BUILD_ID_LEN			32

#define SOCINFO_VERSION_MAJOR(ver) 	((ver & 0xffff0000) >> 16)
#define SOCINFO_VERSION_MINOR(ver) 	(ver & 0x0000ffff)

typedef struct smem_pmic_type
{
	unsigned pmic_model;
	unsigned pmic_die_revision;
}pmic_type;

typedef struct ipq_platform_v1 {
	unsigned format;
	unsigned id;
	unsigned version;
	char     build_id[BUILD_ID_LEN];
	unsigned raw_id;
	unsigned raw_version;
	unsigned hw_platform;
	unsigned platform_version;
	unsigned accessory_chip;
	unsigned hw_platform_subtype;
}ipq_platform_v1;

typedef struct ipq_platform_v2 {
	ipq_platform_v1 v1;
	pmic_type pmic_info[3];
	unsigned foundry_id;
}ipq_platform_v2;

typedef struct ipq_platform_v3 {
	ipq_platform_v2 v2;
	unsigned chip_serial;
} ipq_platform_v3;

union ipq_platform {
	ipq_platform_v1 v1;
	ipq_platform_v2 v2;
	ipq_platform_v3 v3;
};

struct smem_machid_info {
	unsigned format;
	unsigned machid;
};
#endif /* _SMEM_INFO_H_ */
