/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright (c) 2006-2009 NEC Corporation
 * All rights reserved.
 */

#ident	"@(#)armpf/vm/hat_machdep.c"

/*
 * ARMPF-specific HAT management.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/pte.h>
#include <sys/platform.h>
#include <sys/prom_debug.h>
#include <sys/mutex.h>
#include <sys/cache_l220.h>
#include <sys/ddidmareq.h>
#include <sys/archsystm.h>
#include <sys/machsystm.h>
#include <vm/hat_arm.h>
#include <vm/hat_armpt.h>
#include <vm/vm_dep.h>
#include <asm/tlb.h>
#include <sys/mp_cpu.h>
#include <sys/mach_boot.h>

extern hat_kresv_t	hatpt_kresv_l2pt;
extern hat_kresv_t	hatpt_kresv_l2bundle;

/* Preallocated virtual space and L2PT entries for ppcopy() use. */
static caddr_t	hat_ppcopy_addr[NCPU][2];
static l2pte_t	*hat_ppcopy_ptep[NCPU][2];

/*
 * Preallocated virtual space and L2PT entry to maintain instruction cache
 * coherency.
 */
static caddr_t	hat_isync_addr[NCPU];
static l2pte_t	*hat_isync_ptep[NCPU];

extern uint_t		hat_mp_startup_running;

uint_t		hat_xfb_dump = 0;

/*
 * caddr_t
 * hat_plat_reserve_space(caddr_t base)
 *	Reserve virtual address space for kernel L2PT use.
 *	base is base address of free virtual space.
 *
 * Calling/Exit State:
 *	hat_plat_reserve_space() returns end address boundary of
 *	reserved space. Returned address is page-aligned.
 */
caddr_t
hat_plat_reserve_space(caddr_t base)
{
	uintptr_t	kl2pt_vaddr, kl2bundle_vaddr, vaddr;
	size_t		kl2pt_size, kl2bundle_size, kvsize, nl2pt, nbundle;
	int		i;

	/* Calculate size of kernel virtual space excluding static area. */
	kvsize = 0 - (uintptr_t)KSTATIC_END;

	/*
	 * Eliminate size of "static space".
	 * "Static space" means the virtual address whose mapping is never
	 * changed.
	 */
	kvsize -= L1PT_VSIZE;		/* Vector page, SCU register, ... */
	kvsize -= ARMPF_SYS_SIZE;	/* System register */
	kvsize -= ARMPF_XWINDOW_SIZE;	/* X Window */

	PRM_DEBUG(kvsize);

	/*
	 * Calculate number of L2PT required for kernel mapping.
	 */
	nl2pt = (kvsize + L1PT_PAGEOFFSET) >> L1PT_VSHIFT;
	PRM_DEBUG(nl2pt);

	/*
	 * Reserve L2PT space for kernel mapping.
	 * We can eliminate number of reserved L2PT because they are used
	 * only for kernel mapping.
	 */
	kl2pt_vaddr = (uintptr_t)base;
	kl2pt_size = PAGE_ROUNDUP((nl2pt - hatpt_kresv_l2pt.hk_nfree) *
				  L2PT_ALLOC_SIZE);
	PRM_DEBUG(kl2pt_vaddr);
	PRM_DEBUG(kl2pt_size);

	/*
	 * Calculate number of hat_l2bundle required for kernel mapping.
	 * We can eliminate number of reserved hat_l2bundle because they are
	 * used only for kernel mapping.
	 */
	nbundle = (nl2pt + L2BD_SIZE - 1) / L2BD_SIZE;
	nbundle -= hatpt_kresv_l2bundle.hk_nfree;
	PRM_DEBUG(nbundle);

	/* Reserve hat_l2bundle space for kernel mapping. */
	kl2bundle_vaddr = kl2pt_vaddr + kl2pt_size;
	kl2bundle_size = PAGE_ROUNDUP(nbundle * sizeof(hat_l2bundle_t));
	PRM_DEBUG(kl2bundle_vaddr);
	PRM_DEBUG(kl2bundle_size);

	/* Initialize hat_kresv structure. */
	hatpt_kresv_spaceinit(&hatpt_kresv_l2pt, kl2pt_vaddr, kl2pt_size);
	hatpt_kresv_spaceinit(&hatpt_kresv_l2bundle, kl2bundle_vaddr,
			      kl2bundle_size);

	/* Reserve virtual spaces for ppcopy() use. */
	vaddr = PAGE_ROUNDUP(kl2bundle_vaddr + kl2bundle_size);

	ASSERT(max_ncpus >= 1 && max_ncpus <= NCPU);
	for (i = 0; i < max_ncpus; i++) {
		int		j;
		hat_l2pt_t	*l2pt;
		l2pte_t		*ptep;

		for (j = 0; j < 2; j++) {
			hatpt_boot_linkl2pt(vaddr, MMU_PAGESIZE, B_FALSE);
			l2pt = hatpt_l2pt_lookup(&hat_kas, vaddr);
			ASSERT(l2pt);

			ptep = l2pt->l2_vaddr + L2PT_INDEX(vaddr);
			PRM_PRINTF("ppcopy[cpu%d][%d]:  addr=0x%08lx, "
				   "ptep=0x%p\n", i, j, vaddr, ptep);

			hat_ppcopy_addr[i][j] = (caddr_t)vaddr;
			hat_ppcopy_ptep[i][j] = ptep;

			vaddr += MMU_PAGESIZE;
		}

		/* Reserve space for instruction cache maintenance. */
		hatpt_boot_linkl2pt(vaddr, MMU_PAGESIZE, B_FALSE);
		l2pt = hatpt_l2pt_lookup(&hat_kas, vaddr);
		ASSERT(l2pt);

		ptep = l2pt->l2_vaddr + L2PT_INDEX(vaddr);
		PRM_PRINTF("sync[cpu%d]:  addr=0x%08lx, ptep=0x%p\n",
			   i, vaddr, ptep);
		hat_isync_addr[i] = (caddr_t)vaddr;
		hat_isync_ptep[i] = ptep;
		vaddr += MMU_PAGESIZE;
	}

	return (caddr_t)vaddr;
}


/*
 * void
 * hat_plat_cpu_init(cpu_t *cp)
 *	Per CPU initialization for HAT layer.
 *
 *	- Set up two private addresses for use on a given CPU for use in
 *	  ppcopy() and pagezero().
 *	- Set up private address used to maintain instruction cache coherency.
 */
void
hat_plat_cpu_init(cpu_t *cp)
{
	processorid_t	id = cp->cpu_seqid;

	ASSERT(id < max_ncpus);
	cp->cpu_caddr1 = hat_ppcopy_addr[id][0];
	cp->cpu_caddr1pte = hat_ppcopy_ptep[id][0];
	cp->cpu_caddr2 = hat_ppcopy_addr[id][1];
	cp->cpu_caddr2pte = hat_ppcopy_ptep[id][1];

	mutex_init(&(cp->cpu_ppaddr_mutex), NULL, MUTEX_DEFAULT, NULL);

	cp->cpu_isync_addr = hat_isync_addr[id];
	cp->cpu_isync_pte = hat_isync_ptep[id];
	mutex_init(&(cp->cpu_isync_mutex), NULL, MUTEX_DEFAULT, NULL);
}

/*
 * void
 * hat_plat_mpstart_init(void)
 *	Initialize HAT layer for mp_startup().
 *
 *	Main purpose of this function is to create VA == PA mapping for
 *	SDRAM.
 */
void
hat_plat_mpstart_init(void)
{
	uintptr_t	*l1ptp;

	HAT_KAS_LOCK();
	kpreempt_disable();
	hat_mp_startup_running = 1;

	MP_CPU_MPSTART_INIT();

	/*
	 * We need to create VA == PA mapping for SDRAM because
	 * secondary CPUs are running in real mode (MMU disabled).
	 * We assume that section mapping is used for this mapping.
	 */
#if	(KERNELPHYSBASE & L1PT_SECTION_VOFFSET) != 0
#error	KERNELPHYSBASE must be section size aligned.
#endif	/* (KERNELPHYSBASE & L1PT_SECTION_VOFFSET) != 0 */
	hat_boot_mapin((caddr_t)KERNELPHYSBASE, KERNELPHYSBASE,
		       L1PT_SECTION_VSIZE, HAT_STORECACHING_OK);

	/* Install physical address of kernel L1PT. */
	l1ptp = (uintptr_t *)MP_STARTUP_L1PT_VADDR;
	*l1ptp = hat_kas.hat_l1paddr;
	PRM_PRINTF("Secondary startup L1PT: 0x%p, 0x%lx\n",
		   l1ptp, *l1ptp);

	/*
	 * We must writeback cache line corresponding to
	 * MP_STARTUP_L1PT_VADDR because it will be accessed without MMU.
	 */
	local_sync_data_memory((uintptr_t)l1ptp, sizeof(uintptr_t), NULL);

	/* local_sync_data_memory() implies SYNC_BARRIER() */
	CACHE_L220_FLUSH((caddr_t)l1ptp, sizeof(uintptr_t), L220_CLEAN);

	kpreempt_enable();
	HAT_KAS_UNLOCK();
}

/*
 * void
 * hat_plat_mpstart_fini(void)
 *	Declare end of mp_startup() processing.
 */
void
hat_plat_mpstart_fini(void)
{
	l1pte_t	*l1ptep;

	hat_mp_startup_running = 0;

	/*
	 * Disable VA == PA mapping.
	 * This code is valid because we know that only one section mapping
	 * is used for VA == PA mapping.
	 */
	l1ptep = hat_kas.hat_l1vaddr + L1PT_INDEX(KERNELPHYSBASE);

	HAT_KAS_LOCK();

	MP_CPU_MPSTART_FINI();

	kpreempt_disable();
	HAT_L1PTE_SET(l1ptep, 0);

	/*
	 * Although boot CPU never access VA == PA mappings,
	 * we should flush TLB entry here. We don't need to send cross
	 * call to other CPU because they flush entire TLB by themselves.
	 */
	TLB_FLUSH_VADDR(KERNELPHYSBASE);

	kpreempt_enable();
	HAT_KAS_UNLOCK();
}

/*
 * void
 * hat_plat_dump(void)
 *	Choose physical page to be dumped in the system dump.
 *	ARM platform specific.
 */
void
hat_plat_dump(void)
{
	int	cpuid;

	for (cpuid = 0; cpuid < max_ncpus; cpuid++) {
		int	i;
		l2pte_t	pte;

		/* Dump CPU private space for ppcopy() if mapped. */
		for (i = 0; i < 2; i++) {
			pte = *hat_ppcopy_ptep[cpuid][i];

			if (pte != 0) {
				dump_page(HAT_L2PTE_SMALL_PFN(pte));
			}
		}

		/* Dump CPU private space for I-cache maintenance. */
		pte = *hat_isync_ptep[cpuid];
		if (pte != 0) {
			dump_page(HAT_L2PTE_SMALL_PFN(pte));
		}
	}

	if (hat_xfb_dump) {
		pfn_t	xpfn;

		/* Dump X frame buffer. */
		for (xpfn = mmu_btop(ARMPF_XWINDOW_PADDR);
		     xpfn < mmu_btop(ARMPF_XWINDOW_PADDR + ARMPF_XWINDOW_SIZE);
		     xpfn++) {
			dump_page(xpfn);
		}
	}
}
