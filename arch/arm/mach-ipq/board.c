// SPDX-License-Identifier: GPL-2.0+
/*
 * Common initialisation for Qualcomm ipq boards.
 *
 * Copyright (c) 2024 Linaro Ltd.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * Author: Caleb Connolly <caleb.connolly@linaro.org>
 */

#ifdef CONFIG_ARM64
#include <asm/armv8/mmu.h>
#endif
#include <mach/ipq.h>
#include <asm/system.h>
#include <asm/cache.h>
#include <mach/smem_info.h>
#include <linux/err.h>
#include <linux/psci.h>
#include <linux/sizes.h>
#include <sysreset.h>
#include <cpu_func.h>
#include <dm/device-internal.h>
#include <dm/device.h>
#include <dm/read.h>
#include <env.h>
#include <fdt_support.h>

/******************************************************************
 * Globals and constant
 *****************************************************************/
DECLARE_GLOBAL_DATA_PTR;

#ifndef BOOT_PARAMS_ADDR
#define BOOT_PARAMS_ADDR			(KERNEL_START_ADDR + 0x100)
#endif
uint32_t g_board_machid;
struct ipq_board_info *ipq_bdinfo;
uint32_t g_recovery_path __section(".data");
#if defined(CONFIG_ENV_IS_IN_SPI_FLASH)
uint32_t g_env_offset __section(".data") = 0;
#endif

#define DTS_SUFFIX			"-ub"

/****************************************************************
 * Weak function definition
 * this placeholder for generic weak function definition.
 ****************************************************************/
__weak void ipq_board_early_init_f(void) {}

__weak int ipq_uboot_fdt_fixup(void *blob, enum fixup_type)
{
	return 0;
}

__weak void board_cache_init(void)
{
	icache_enable();
#if !CONFIG_IS_ENABLED(SYS_DCACHE_OFF)
	dcache_enable();
#endif
}

/*
 * Global func definition
 */
struct ipq_board_info *ipq_get_bdinfo(void)
{
	return ipq_bdinfo;
}

#if defined(CONFIG_SMEM) && defined(CONFIG_MSM_SMEM)
struct ipq_smem_flash_info *ipq_get_smem_info(void)
{
	if (ipq_bdinfo)
		return &ipq_bdinfo->smem_info;
	else
		return NULL;
}

struct soc_info *ipq_get_socinfo(void)
{
	if (ipq_bdinfo)
		return &ipq_bdinfo->ipq_socinfo;
	else
		return NULL;
}

struct smem_ptable *ipq_get_part_table(void)
{
	if (ipq_bdinfo)
		return ipq_bdinfo->ptable;
	else
		return NULL;
}
#else
struct ipq_smem_flash_info *ipq_get_smem_info(void)
{
	return NULL;
}

struct soc_info *ipq_get_socinfo(void)
{
	return NULL;
}

struct smem_ptable *ipq_get_part_table(void)
{
	return NULL;
}
#endif

static void check_recovery_mode(void)
{
	if (g_recovery_path == 1)
		gd->board_type |= RECOVERY_MODE;
#if defined(CRASH_DUMP_ADDR_IMEM)
	else {
		g_recovery_path = readl(CRASH_DUMP_ADDR_IMEM) & 0xffffffff;
		gd->board_type |= (g_recovery_path == MAGIC_RECOVERY_PATH) ?
		RECOVERY_MODE : 0;
	}
#endif
}

#ifdef CONFIG_ARM64
static struct mm_region ipq_mem_map[CONFIG_NR_DRAM_BANKS + 3] = { { 0 } };
struct mm_region *mem_map = ipq_mem_map;

static void build_mem_map(void)
{
	int i, j;

	/*
	 * Ensure the peripheral block is sized to correctly
	 * cover the address range up to the first memory bank.
	 * Don't map the first page to ensure that we actually trigger
	 * an abort on a null pointer access rather than just hanging.
	 */
	mem_map[0].phys = 0x1000;
	mem_map[0].virt = mem_map[0].phys;
	mem_map[0].size = gd->bd->bi_dram[0].start - mem_map[0].phys;
	mem_map[0].attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN;

	for (i = 1, j = 0; i < ARRAY_SIZE(ipq_mem_map) - 2 &&
		j < ARRAY_SIZE(gd->bd->bi_dram) &&
		gd->bd->bi_dram[j].size; i++, j++) {
		mem_map[i].phys = gd->bd->bi_dram[j].start;
		mem_map[i].virt = mem_map[i].phys;
		mem_map[i].size = gd->bd->bi_dram[j].size;
		mem_map[i].attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
				PTE_BLOCK_INNER_SHARE |
				PTE_BLOCK_PXN | PTE_BLOCK_UXN;
	}

	/*
	 * Mark only uboot relocated text region with executable permission
	 */
	mem_map[i].phys = round_down(gd->relocaddr, SZ_4K);
	mem_map[i].virt = mem_map[i].phys;
	mem_map[i].size =
		round_up(gd->relocaddr + gd->mon_len, SZ_4K) - mem_map[i].phys;
	mem_map[i].attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
				PTE_BLOCK_INNER_SHARE;

	i++;
	mem_map[i].phys = UINT64_MAX;
	mem_map[i].size = 0;

#ifdef DEBUG
	debug("Configured memory map:\n");
	for (i = 0; mem_map[i].size; i++)
		debug("  0x%016llx - 0x%016llx: entry %d\n",
		      mem_map[i].phys, mem_map[i].phys + mem_map[i].size, i);
#endif
}

u64 get_page_table_size(void)
{
	return SZ_64K;
}

/* This function open-codes setup_all_pgtables() so that we can
 * insert additional mappings *before* turning on the MMU.
 */
void enable_caches(void)
{
	u64 tlb_addr = gd->arch.tlb_addr;
	u64 tlb_size = gd->arch.tlb_size;
	u64 pt_size;

	gd->arch.tlb_fillptr = tlb_addr;

	build_mem_map();

	/* Create normal system page tables */
	setup_pgtables();

	pt_size = (uintptr_t)gd->arch.tlb_fillptr -
			(uintptr_t)gd->arch.tlb_addr;
	debug("Primary pagetable size: %lluKiB\n", pt_size / 1024);

	/* Create emergency page tables */
	gd->arch.tlb_size -= pt_size;
	gd->arch.tlb_addr = gd->arch.tlb_fillptr;
	setup_pgtables();
	gd->arch.tlb_emerg = gd->arch.tlb_addr;
	gd->arch.tlb_addr = tlb_addr;
	gd->arch.tlb_size = tlb_size;

	check_recovery_mode();
	board_cache_init();
}

#else /* CONFIG_ARMV7 */
#define EXEC_CACHE_OPTION				0x100e
#define NONEXEC_CACHE_OPTION				0x101e

int arch_cpu_init(void)
{
	u32 val;
	/* Read SCTLR */
	asm volatile ("mrc p15, 0, %0, c1, c0, 0" : "=r" (val));
	/* set the cp15 barrier enable bit */
	val |= 0x20;
	/* write back to SCTLR */
	asm volatile ("mcr p15, 0, %0, c1, c0, 0" : : "r" (val));

	return 0;
}

static bool isexecute_configured(int i)
{
	bool done = false;

	/*
	 * Set XN bit for all dram regions except uboot code region
	 */
	if (i >= (round_down(gd->relocaddr, SZ_1M) >> MMU_SECTION_SHIFT) &&
		i < (round_up(gd->relocaddr + gd->mon_len, SZ_1M)
			>> MMU_SECTION_SHIFT)) {
		set_section_dcache(i, EXEC_CACHE_OPTION);
		done = true;
	}

	return done;
}

void dram_bank_mmu_setup(int bank)
{
	struct bd_info *bd = gd->bd;
	int i;
	uint32_t dram_size;

	/* bd->bi_dram is available only after relocation */
	if ((gd->flags & GD_FLG_RELOC) == 0)
		return;

	/*
	 * check and limit the DDR region to avoid overflow
	 */
	if (((uint64_t)bd->bi_dram[bank].start +
		bd->bi_dram[bank].size) > ULONG_MAX) {
		dram_size = bd->bi_dram[bank].start +
				(ULONG_MAX - bd->bi_dram[bank].size);
	} else {
		dram_size = bd->bi_dram[bank].start + bd->bi_dram[bank].size;
	}

	dram_size >>= MMU_SECTION_SHIFT;

	debug("%s: bank: %d\n", __func__, bank);
	for (i = bd->bi_dram[bank].start >> MMU_SECTION_SHIFT; i < dram_size;
		i++) {
		if (isexecute_configured(i))
			continue;

		set_section_dcache(i, NONEXEC_CACHE_OPTION);
	}
}

void enable_caches(void)
{
	check_recovery_mode();
	board_cache_init();
}
#endif

int dram_init(void)
{
	int i, j;
	size_t size;
	struct udevice *dev;
	struct usable_ram_partition_table *rpt;
	struct ram_partition_entry *rpe;

	uclass_get_device(UCLASS_SMEM, 0, &dev);
	rpt = smem_get(dev, -1, SMEM_USABLE_RAM_PARTITION_TABLE, &size);

	if (rpt == NULL)
		return -ENODEV;

	rpe = &rpt->ram_part_entry[0];

	for (j = i = 0; i < rpt->num_partitions; i++, rpe++)
		if ((rpe->partition_category == RAM_PARTITION_SDRAM) &&
			(rpe->partition_type == RAM_PARTITION_SYS_MEMORY))
			gd->ram_size += rpe->available_length;

	return 0;
}

#if !defined(CONFIG_SPL)
int dram_init_banksize(void)
{
	int i, j;
	size_t size;
	struct udevice *dev;
	struct usable_ram_partition_table *rpt;
	struct ram_partition_entry *rpe;

	uclass_get_device(UCLASS_SMEM, 0, &dev);
	rpt = smem_get(dev, -1, SMEM_USABLE_RAM_PARTITION_TABLE, &size);

	if (rpt == NULL)
		return -ENODEV;

	rpe = &rpt->ram_part_entry[0];

	for (j = i = 0; i < rpt->num_partitions; i++, rpe++) {
		if ((rpe->partition_category == RAM_PARTITION_SDRAM) &&
			(rpe->partition_type == RAM_PARTITION_SYS_MEMORY)) {
			gd->bd->bi_dram[i].start = rpe->start_address;
			gd->bd->bi_dram[i].size = rpe->available_length;
		}
	}

	return 0;
}
#endif /* CONFIG_SPL */

int ft_board_setup(void *blob, struct bd_info __maybe_unused *bd)
{
	ipq_ft_board_setup(blob, bd);

	return 0;
}

int board_early_init_f(void)
{
#if defined(CONFIG_SMEM) && defined(CONFIG_MSM_SMEM)
	size_t size;
	struct udevice *dev;
	union ipq_platform *platform_type;

	uclass_get_device(UCLASS_SMEM, 0, &dev);
	platform_type = smem_get(dev, -1, SMEM_HW_SW_BUILD_ID, &size);
	if (IS_ERR_OR_NULL(platform_type)) {
		debug("Failed to get SMEM item: SMEM_HW_SW_BUILD_ID\n");
		return -ENODEV;
	}

	g_board_machid = ((platform_type->v1.hw_platform << 24) |
				((SOCINFO_VERSION_MAJOR(
				platform_type->v1.platform_version)) << 16) |
				((SOCINFO_VERSION_MINOR(
				platform_type->v1.platform_version)) << 8) |
				(platform_type->v1.hw_platform_subtype));
#endif
	/*
	 * SoC specific early init
	 */
	ipq_board_early_init_f();

	return 0;
}

#if defined(CONFIG_ARM64) && defined(CFG_EMUL_FREQUENCY_DIVIDER)
void setup_arch_cntfreq(void)
{
	unsigned long freq = CONFIG_COUNTER_FREQUENCY /
				CFG_EMUL_FREQUENCY_DIVIDER;
	asm volatile("msr cntfrq_el0, %0" : : "r" (freq) : "memory");

        return;
}
#endif

int board_init(void)
{
#if defined(CONFIG_ARM64) && defined(CFG_EMUL_FREQUENCY_DIVIDER)
	struct ipq_smem_flash_info *sfi = ipq_get_smem_info();
	if ((current_el() == 3) && (sfi->flash_type != SMEM_BOOT_NO_FLASH))
		setup_arch_cntfreq();
#endif

	/*
	 * create device pointer
	 */
	ipq_bdinfo = (struct ipq_board_info *)malloc_cache_aligned(
			sizeof(struct ipq_board_info));
	if (ipq_bdinfo == NULL) {
		printf("%s No emough Space\n", __func__);
		return -ENOMEM;
	}

	memset(ipq_bdinfo, 0, sizeof(struct ipq_board_info));

#if defined(CONFIG_SMEM) && defined(CONFIG_MSM_SMEM)
	ipq_board_read_smem_info(ipq_bdinfo);
#endif

	/*
	 * update Global bdinfo table
	 */
	gd->bd->bi_boot_params = BOOT_PARAMS_ADDR;

	return 0;
}

#if defined(CONFIG_BOARD_EARLY_INIT_R)
int board_early_init_r(void)
{
	ipq_update_sfi_block_size();
	/*
	 * Update env address in runtime , support only in Nor flash
	 */
	ipq_runtime_sf_env_update();

	ipq_update_lmb_reservation();

	return 0;
}
#endif

#if defined(CONFIG_BOARD_LATE_INIT)
int board_late_init(void)
{
	ipq_board_late_init();

	return 0;
}
#endif

int mach_cpu_init(void)
{
	gd->flags |= GD_FLG_SKIP_RELOC;
	return 0;
}

int arch_setup_dest_addr(void)
{
	gd->relocaddr = CONFIG_TEXT_BASE;
	gd->reloc_off = gd->relocaddr - CONFIG_TEXT_BASE;
	return 0;
}

int arm_reserve_mmu(void)
{
	/* reserve TLB table */
	gd->arch.tlb_size = PGTABLE_SIZE;
	gd->arch.tlb_addr = CONFIG_TEXT_BASE + gd->mon_len;
	gd->arch.tlb_addr += (0x10000 - 1);
	gd->arch.tlb_addr &= ~(0x10000 - 1);
	return 0;
}

#ifdef CONFIG_DTB_RESELECT
int embedded_dtb_select(void)
{
	int rescan, i;
	struct multidtb_config *dtb = g_board_dtb_info;

	for (i = 0; i < dtb->ncount; ++i) {
		if (dtb->list[i].machid == g_board_machid) {
			strlcpy(dtb->dts_base, dtb->list[i].dts,
				BOARD_DTS_MAX_NAMELEN);
			dtb->index = i;
			break;
		}
	}

	ipq_update_board_name(g_board_machid, dtb);

	fdtdec_resetup(&rescan);

	return 0;
}
#endif /* CONFIG_DTB_RESELECT */

#if defined(CONFIG_SPL)
int board_fit_config_name_match(const char *name)
{
	/*
	 * SPL loads the pre-HLOS images from bootldr FIT image
	 * as below
	 *
	 * In borad_init_f() - Matches "pre-ddr" configuration node and
	 * load the images mentioned in its <loadables>
	 *
	 * In borad_init_r() - Matches "post-ddr" configuration node and
	 * load the images mentioned in its <loadables>
	 *
	 */
	if (!(gd->flags & GD_FLG_SPL_INIT)) {
		if (!strcmp(name, "pre-ddr")) {
			printf("Selected FIT Config: %s\n", name);
			return 0;
		}
	} else {
		if (!strcmp(name, "post-ddr")) {
			printf("Selected FIT Config: %s\n", name);
			return 0;
		}
	}

	return -EINVAL;
}
#else
#ifdef CONFIG_MULTI_DTB_FIT
int board_fit_config_name_match(const char *name)
{
	struct multidtb_config *dtb = g_board_dtb_info;
	char *suffix = strstr(name, DTS_SUFFIX);
	size_t len, base_len = strlen(dtb->dts_base);

	if (suffix)
		len = suffix - name;
	else
		len = strlen(name);

	if (len == base_len) {
		if (!strncmp(name, dtb->dts_base, len)) {
			printf("Booting %s\n", dtb->dts_name);
			return 0;
		}
	}

	return -1;
}
#endif /* CONFIG_MULTI_DTB_FIT */
#endif /* CONFIG_SPL */

/*
 * Flush range from all levels of d-cache/unified-cache.
 * Affects the range,
 *      if cache is algined,
 *              from : start
 *              to   : start + size - 1
 *      if cache is not aligned,
 *              from : start - cache aligne address
 *              to   : start + size - 1 + cache aligne address
 */
void flush_cache(unsigned long start, unsigned long size)
{
	unsigned long stop = start + size;

	if (start & (CONFIG_SYS_CACHELINE_SIZE - 1))
		start = start & ~(CONFIG_SYS_CACHELINE_SIZE - 1);

	if (stop & (CONFIG_SYS_CACHELINE_SIZE - 1))
		stop = CONFIG_SYS_CACHELINE_SIZE +
			(stop & ~(CONFIG_SYS_CACHELINE_SIZE - 1));

	flush_dcache_range(start, stop);
}

#if defined(CONFIG_SPL)
int fdtdec_board_setup(const void *fdt_blob)
{
	return 0;
}
#else
int fdtdec_board_setup(const void *fdt_blob)
{
	return ipq_uboot_fdt_fixup((void*)fdt_blob, UBOOT_FIXUP_SMEM);
}
#endif

#ifdef CONFIG_OF_BOARD_FIXUP
int board_fix_fdt(void *rw_fdt_blob)
{
	ipq_uboot_fdt_fixup(rw_fdt_blob, UBOOT_FIXUP_SMEM);
	ipq_uboot_fdt_fixup(rw_fdt_blob, UBOOT_FIXUP_USB);

	return 0;
}
#endif

phys_size_t get_effective_memsize(void)
{
	phys_size_t ram_size = min(gd->ram_size,
				(phys_size_t)CFG_SYS_SDRAM_BASE0_SIZE);

#ifndef CONFIG_ARM64
	if (((uint64_t)gd->ram_base + ram_size) > ULONG_MAX)
		ram_size = ULONG_MAX - gd->ram_base;
#endif
	return ram_size;
}

#if CONFIG_IS_ENABLED(NAND_QTI)
void board_nand_init(void)
{
	struct udevice *dev;
	int ret;

	struct ipq_smem_flash_info *sfi = ipq_get_smem_info();

	/*
	 * Since the training partition info present in the gpt table
	 * which resides inside SPI-NOR flash so spi nor probe is must
	 * before nand init in NOTGPT case.
	 */

	if (sfi && sfi->flash_type == SMEM_BOOT_NORGPT_FLASH) {
#ifdef CONFIG_IPQ_SPI_NOR
		ipq_spi_probe();
#endif
	}

	ret = uclass_get_device_by_driver(UCLASS_MTD,
					  DM_DRIVER_GET(qti_nand), &dev);
	if (ret && ret != -ENODEV)
		pr_err("Failed to initialize %s. (error %d)\n",
		       dev->name, ret);
}
#endif
