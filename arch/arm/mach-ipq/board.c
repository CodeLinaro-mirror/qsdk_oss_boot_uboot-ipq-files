// SPDX-License-Identifier: GPL-2.0+
/*
 * Common initialisation for Qualcomm ipq boards.
 *
 * Copyright (c) 2024 Linaro Ltd.
 * Copyright (c) 2023-2025, Qualcomm Innovation Center,Inc.All rights reserved.
 * Author: Caleb Connolly <caleb.connolly@linaro.org>
 */

#ifdef CONFIG_ARM64
#include <asm/armv8/mmu.h>
#endif
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
#include <smem.h>
#include <mach/ipq.h>

#define EXEC_CACHE_OPTION		0x100e
#define NONEXEC_CACHE_OPTION		0x101e

DECLARE_GLOBAL_DATA_PTR;

uint32_t g_board_machid;

#ifdef CONFIG_ARM64
static struct mm_region ipq_mem_map[CONFIG_NR_DRAM_BANKS + 3] = { { 0 } };
struct mm_region *mem_map = ipq_mem_map;
/*
 * Weak function definition
 */
__weak void ipq_board_early_init_f(void)
{
	return;
}

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

	icache_enable();
	dcache_enable();
}

#else /* CONFIG_ARMV7 */
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
	icache_enable();
	dcache_enable();
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
	rpt = smem_get(dev, 0, SMEM_USABLE_RAM_PARTITION_TABLE, &size);

	if (rpt == NULL)
		return -ENODEV;

	rpe = &rpt->ram_part_entry[0];

	for (j = i = 0; i < rpt->num_partitions; i++, rpe++)
		if ((rpe->partition_category == RAM_PARTITION_SDRAM) &&
			(rpe->partition_type == RAM_PARTITION_SYS_MEMORY))
			gd->ram_size += rpe->available_length;

	return 0;
}

int dram_init_banksize(void)
{
	int i, j;
	size_t size;
	struct udevice *dev;
	struct usable_ram_partition_table *rpt;
	struct ram_partition_entry *rpe;

	uclass_get_device(UCLASS_SMEM, 0, &dev);
	rpt = smem_get(dev, 0, SMEM_USABLE_RAM_PARTITION_TABLE, &size);

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

void reset_cpu(void)
{
	psci_sys_reset(SYSRESET_COLD);
}

int ft_board_setup(void *blob, struct bd_info __maybe_unused *bd)
{
	return 0;
}

int board_early_init_f(void)
{
	size_t size;
	struct udevice *dev;
	union ipq_platform *platform_type;

	uclass_get_device(UCLASS_SMEM, 0, &dev);
	platform_type = smem_get(dev, 0, SMEM_HW_SW_BUILD_ID, &size);
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

	/*
	 * SoC specific early init
	 */
	ipq_board_early_init_f();

	return 0;
}

int board_init(void)
{
	return 0;
}

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
			break;
		}
	}

	ipq_update_board_name(g_board_machid, dtb);

	fdtdec_resetup(&rescan);

	return 0;
}
#endif /* CONFIG_DTB_RESELECT */

#ifdef CONFIG_MULTI_DTB_FIT
int board_fit_config_name_match(const char *name)
{
	struct multidtb_config *dtb = g_board_dtb_info;

	if (!strcmp(name, dtb->dts_base)) {
		printf("Booting %s\n", dtb->dts_name);
		return 0;
	}

	return -1;
}
#endif /* CONFIG_MULTI_DTB_FIT */
