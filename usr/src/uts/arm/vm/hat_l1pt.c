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
 * Copyright (c) 2007-2008 NEC Corporation
 * All rights reserved.
 */

/*
 * Level 1 page table management for user space.
 *
 * This file must be linked to machine-specific kernel object because
 * it includes machine-specific header.
 */

#ident	"@(#)arm/vm/hat_l1pt.c"

#include <sys/types.h>
#include <sys/var.h>
#include <sys/sysmacros.h>
#include <sys/vmsystm.h>
#include <sys/cmn_err.h>
#include <sys/prom_debug.h>
#include <vm/hat_arm.h>
#include <vm/hat_armpt.h>
#include <vm/hat_machdep.h>

/* Simple list entry for user L1PT freelist. */
typedef struct l1free {
	uintptr_t	lf_paddr;	/* Physical address of L1PT. */
	struct l1free	*lf_next;
} l1free_t;

static uint32_t	hatpt_l1pt_cache_nfree;	/* Number of L1PT on the freelist. */
static kmutex_t	hatpt_l1pt_cache_mutex;
static l1free_t	*hatpt_l1pt_cache;	/* Head of L1PT freelist. */
static uint32_t	hatpt_l1pt_count;	/* Number of active L1PT. */

/*
 * This code assumes that HAT_L1PT_NSYSPROC processes has no virtual space.
 * (sched, pageout, and fsflush)
 */
#define	HAT_L1PT_NSYSPROC	3

/*
 * Memory attributes for user L1PT page allocation without memory range
 * constraints.
 */ 
ddi_dma_attr_t	hat_l1pt_mattr = {
	DMA_ATTR_V0, 0x0ULL, 0xffffffffULL, 0xffffffffULL,
	(uint64_t)L1PT_USER_SIZE, 1, 1, 0xffffffffULL, 0xffffffffULL,
	0x1, 1, 0
};

#ifdef	HAT_L1PT_MATTR_LIST_COUNT

#if	HAT_L1PT_MATTR_LIST_COUNT <= 0
#error	HAT_L1PT_MATTR_LIST_COUNT must be larger than zero.
#endif	/* HAT_L1PT_MATTR_LIST_COUNT <= 0 */

/* hat_l1pt_mattr_list is defined in platform-specific code. */
extern ddi_dma_attr_t	*hat_l1pt_mattr_list[];

#else	/* !HAT_L1PT_MATTR_LIST_COUNT */

/* Define common memory attribute list for user L1PT page allocation. */
ddi_dma_attr_t	*hat_l1pt_mattr_list[] = {&hat_l1pt_mattr};

#define	HAT_L1PT_MATTR_LIST_COUNT	1

#endif	/* HAT_L1PT_MATTR_LIST_COUNT */

/* Internal prototypes */
static l1pte_t		*hatpt_l1pt_alloc_impl(uintptr_t *paddrp);
static uintptr_t	hatpt_ul1pt_alloc(void);

/*
 * void
 * hat_l1pt_init(void)
 *	Initialize user L1PT management layer.
 */
void
hatpt_l1pt_init(void)
{
	uint32_t	ncache, i;
	l1free_t	*lfhead = NULL;

	/*
	 * Preallocate user L1PT.
	 * Note that sched, pageout, and fsflush doesn't require user L1PT.
	 */
	ncache = MIN(L1PT_PREALLOC, v.v_proc - HAT_L1PT_NSYSPROC);
	for (i = 0; i < ncache; i++) {
		uintptr_t	paddr;
		l1free_t	*lfp;

		lfp = (l1free_t *)hatpt_l1pt_alloc_impl(&paddr);
		if (lfp == NULL) {
			PRM_PRINTF("Failed to preallocate L1PT: %u\n", i);
			break;
		}

		lfp->lf_paddr = paddr;
		lfp->lf_next = lfhead;
		lfhead = lfp;
	}

	hatpt_l1pt_cache = lfhead;
	hatpt_l1pt_cache_nfree = i;
}

/*
 * l1pte_t *
 * hatpt_l1pt_alloc(int cansleep, uintptr_t *paddrp)
 *	Allocate L1 page table for user process.
 *
 *	If cansleep is true, hatpt_l1pt_alloc() will sleep until L1PT can be
 *	allocated.
 *
 * Calling/Exit State:
 *	Upon successful completion, hatpt_l1pt_alloc() returns virtual address
 *	of new L1PT, and set physical address of new L1PT into *paddrp.
 *	On failure, it returns NULL.
 *
 * Remarks:
 *	hatpt_l1pt_alloc() doesn't zero L1PT entries. It's up to the caller.
 */
l1pte_t *
hatpt_l1pt_alloc(int cansleep, uintptr_t *paddrp)
{
	uintptr_t	vaddr;
	l1free_t	*lfp;
	l1pte_t		*l1ptep;
	int		warn = 0;

	while (1) {
		/* Try to allocate from cache. */
		mutex_enter(&hatpt_l1pt_cache_mutex);
		if ((lfp = hatpt_l1pt_cache) != NULL) {
			ASSERT(hatpt_l1pt_cache_nfree > 0);
			ASSERT(mmu_btop(lfp->lf_paddr) ==
			       hat_getkpfnum((caddr_t)lfp));

			hatpt_l1pt_cache = lfp->lf_next;
			*paddrp = lfp->lf_paddr;
			hatpt_l1pt_cache_nfree--;
			hatpt_l1pt_count++;
			mutex_exit(&hatpt_l1pt_cache_mutex);

			l1ptep = (l1pte_t *)lfp;
			break;
		}
		mutex_exit(&hatpt_l1pt_cache_mutex);

		l1ptep = hatpt_l1pt_alloc_impl(paddrp);
		if (l1ptep != NULL) {
			/* Succeeded. */
			mutex_enter(&hatpt_l1pt_cache_mutex);
			hatpt_l1pt_count++;
			mutex_exit(&hatpt_l1pt_cache_mutex);
			break;
		}
		if (!cansleep) {
			/* Failed without sleep. */
			break;
		}

		/*
		 * Can't allocate L1PT.
		 * Kick memory scheduler as hard as possible.
		 */
		if (!warn) {
			atomic_inc_uint(&page_create_io_waiters);
			cmn_err(CE_WARN, "Can't allocate L1PT: freemem=0x%lx",
				freemem);
			warn = 1;
		}

		/* Kick pageout scanner. */
		cv_signal(&proc_pageout->p_cv);

		delay(10);
	}

	if (warn) {
		atomic_dec_uint(&page_create_io_waiters);
	}

	return l1ptep;
}

/*
 * static uintptr_t
 * hatpt_ul1pt_alloc(void)
 *	Allocate buffer for user L1PT.
 *	This function uses contig_alloc() to allocate physically-contiguous
 *	memory that satisfies memory attributes specified by
 *	hat_l1pt_mattr_list.
 *
 * Calling/Exit State:
 *	hatpt_ul1pt_alloc() returns NULL if no memory is available.
 *	This function never sleeps.
 */
static uintptr_t
hatpt_ul1pt_alloc(void)
{
	uintptr_t	vaddr;
	int		i;
	ddi_dma_attr_t	**mpp;
	const uint_t	attr = PROT_READ|PROT_WRITE|HAT_NOSYNC;

	for (i = 0, mpp = hat_l1pt_mattr_list;
	     i < HAT_L1PT_MATTR_LIST_COUNT; i++, mpp++) {
		ddi_dma_attr_t	*mattr = *mpp;

		vaddr = (uintptr_t)contig_alloc(L1PT_USER_SIZE, mattr,
						PAGESIZE, attr, KM_NOSLEEP);
		if (vaddr != NULL) {
			break;
		}
	}

	return vaddr;
}

/*
 * l1pte_t *
 * hatpt_l1pt_alloc_impl(uintptr_t *paddrp)
 *	Allocate new L1 page table for user process, without seeing L1PT cache.
 *
 * Calling/Exit State:
 *	Upon successful completion, hatpt_l1pt_alloc_impl() returns virtual
 *	address of new L1PT, and set physical address of new L1PT into *paddrp.
 *	On failure, it returns NULL.
 *
 * Remarks:
 *	hatpt_l1pt_alloc_impl() doesn't zero L1PT entries.
 *	It's up to the caller.
 */
static l1pte_t *
hatpt_l1pt_alloc_impl(uintptr_t *paddrp)
{
	uintptr_t	vaddr = hatpt_ul1pt_alloc();

	if (vaddr != NULL) {
		pfn_t		pfn;
		uintptr_t	paddr;
#ifdef	DEBUG
		uintptr_t	va, pa;
#endif	/* DEBUG */

		pfn = hat_getkpfnum((caddr_t)vaddr);
		ASSERT(pfn != PFN_INVALID);
		paddr = mmu_ptob(pfn);
		ASSERT(IS_P2ALIGNED(paddr, L1PT_USER_SIZE));
#ifdef	DEBUG
		for (va = vaddr + MMU_PAGESIZE, pa = paddr + MMU_PAGESIZE;
		     va < vaddr + L1PT_USER_SIZE;
		     va += MMU_PAGESIZE, pa += MMU_PAGESIZE) {
			pfn_t	pfnum;

			pfnum = hat_getkpfnum((caddr_t)va);
			ASSERT(pfnum != PFN_INVALID);
			ASSERT(mmu_ptob(pfnum) == pa);
		}
#endif	/* DEBUG */

		*paddrp = paddr;
	}

	return (l1pte_t *)vaddr;
}

/*
 * void
 * hatpt_l1pt_free(l1pte_t *l1pt, uintptr_t paddr)
 *	Release L1PT that was allocated by hatpt_l1pt_alloc().
 */
void
hatpt_l1pt_free(l1pte_t *l1pt, uintptr_t paddr)
{
	uint_t	ncache;

	/*
	 * Determine whether this L1PT should be cached.
	 * We don't need to cache if the cached L1PT is enough to create
	 * process.
	 */
	mutex_enter(&hatpt_l1pt_cache_mutex);
	hatpt_l1pt_count--;
	if (hatpt_l1pt_count + HAT_L1PT_NSYSPROC <= v.v_proc) {
		ncache = MIN(v.v_proc - hatpt_l1pt_count - HAT_L1PT_NSYSPROC,
			     L1PT_PREALLOC + L1PT_CACHE);
	}
	else {
		/* This should not happen. */
		ncache = 0;
	}
	if (hatpt_l1pt_cache_nfree < ncache) {
		l1free_t	*lfp = (l1free_t *)l1pt;

		/* Put this L1PT onto the cache. */
		ASSERT(mmu_btop(paddr) == hat_getkpfnum((caddr_t)l1pt));
		lfp->lf_paddr = paddr;
		lfp->lf_next = hatpt_l1pt_cache;
		hatpt_l1pt_cache = lfp;
		hatpt_l1pt_cache_nfree++;
		mutex_exit(&hatpt_l1pt_cache_mutex);
		return;
	}
	mutex_exit(&hatpt_l1pt_cache_mutex);

	/* Destroy L1PT. */
	contig_free(l1pt, L1PT_USER_SIZE);
}

/*
 * void
 * hatpt_l1pt_reap(void *notused)
 *	Reap cached user L1PTs.
 */
void
hatpt_l1pt_reap(void *notused)
{
	l1free_t	*lfp, *head, *next;
	uint32_t	i, nfree, ncache;

	/*
	 * We should keep at least L1PT_PREALLOC L1PTs. But if L1PT_PREALLOC
	 * is larger than the number of available process slot, we keep L1PTs
	 * only for them.
	 */
	mutex_enter(&hatpt_l1pt_cache_mutex);
	if (hatpt_l1pt_count + HAT_L1PT_NSYSPROC <= v.v_proc) {
		ncache = MIN(v.v_proc - hatpt_l1pt_count - HAT_L1PT_NSYSPROC,
			     L1PT_PREALLOC);
	}
	else {
		/* This should not happen. */
		ncache = 0;
	}
	if (hatpt_l1pt_cache_nfree <= ncache) {
		mutex_exit(&hatpt_l1pt_cache_mutex);
		return;
	}

	nfree = hatpt_l1pt_cache_nfree - ncache;
	for (lfp = hatpt_l1pt_cache, i = 0; lfp != NULL && i < nfree;
	     lfp = lfp->lf_next, i++);
	nfree = i;
	ASSERT(nfree <= hatpt_l1pt_cache_nfree);
	head = hatpt_l1pt_cache;
	hatpt_l1pt_cache = lfp;
	hatpt_l1pt_cache_nfree -= nfree;

#ifdef	DEBUG
	for (lfp = hatpt_l1pt_cache, i = 0; lfp != NULL;
	     lfp = lfp->lf_next, i++);
	ASSERT(i == hatpt_l1pt_cache_nfree);
#endif	/* DEBUG */

	mutex_exit(&hatpt_l1pt_cache_mutex);

	for (lfp = head, i = 0; i < nfree; lfp = next, i++) {
		next = lfp->lf_next;
		contig_free(lfp, L1PT_USER_SIZE);
	}
}

/*
 * void
 * hatpt_kas_sync(uintptr_t vaddr, size_t len)
 *	Sync kernel mapping changes into all user spaces.
 *
 *	Kernel space managed by kas_hat, and it has master L1PT for
 *	kernel mappings. But user HAT is used when the user context enters
 *	kernel mode. So kernel L1PT entries needs to be copied to user L1PT
 *	when kernel mapping is changed.
 *
 *	hatpt_kas_sync() copies L1PT entries corresponding to the specified
 *	kernel address range into all user L1PTs.
 *
 * Calling/Exit State:
 *	The caller must acquire the kernel hat mutex, and hatpt_kas_sync()
 *	returns with holding it.
 */
void
hatpt_kas_sync(uintptr_t vaddr, size_t len)
{
	hat_t		*hat;
	uint16_t	l1start, l1end;
	l1pte_t		*kl1pt, *src, *dst, *dstart;

	ASSERT(HAT_KAS_IS_LOCKED());
	ASSERT(len > 0);
	ASSERT(vaddr >= KERNELBASE);
	ASSERT(vaddr + len >= KERNELBASE);

	l1start = L1PT_INDEX(vaddr);
	l1end = L1PT_INDEX(vaddr + len - 1) + 1;
	ASSERT(l1start < l1end);
	ASSERT(l1start >= L1PT_INDEX(KERNELBASE));
	ASSERT(l1end <= L1PT_NPTES);

	kl1pt = hat_kas.hat_l1vaddr + l1start;

	/* Sync to all user hat. */
	mutex_enter(&hat_list_lock);
	HAT_LIST_FOREACH(hat, &hat_kas) {
		dstart = hat->hat_l1vaddr + l1start;
		for (src = kl1pt, dst = hat->hat_l1vaddr + l1start;
		     dst < hat->hat_l1vaddr + l1end; src++, dst++) {
			*dst = *src;
		}
		PTE_SYNC_RANGE(dstart, L1PT_PTE_SIZE, l1end - l1start);
	}
	mutex_exit(&hat_list_lock);
}
