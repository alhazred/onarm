/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2006-2008 NEC Corporation
 */

#ifndef	_VM_HAT_ARM_H
#define	_VM_HAT_ARM_H

#ident	"@(#)arm/vm/hat_arm.h"

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

/*
 * VM - Hardware Address Translation management.
 *
 * This file describes the contents of the ARMv6 HAT data structures.
 */

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/cpuvar.h>
#include <sys/pte.h>
#include <sys/atomic.h>
#include <sys/controlregs.h>
#include <sys/privregs.h>
#include <vm/seg.h>
#include <vm/page.h>
#include <vm/hat_armpt.h>
#include <vm/hment.h>
#include <sys/vmparam.h>
#include <sys/param.h>

/* Links for hat structure */
typedef struct hat_list {
	struct hat_list	*hl_next;	/* Forward link */
	struct hat_list	*hl_prev;	/* Backward link */
} hat_list_t;

/*
 * The hat struct exists for each address space.
 */
struct hat;
typedef struct hat	hat_t;

/*
 * struct hat definition is defined only if _MACHDEP is defined,
 * because it depends on machine-dependant header.
 */
#ifdef	_MACHDEP
struct hat {
	hat_list_t	hat_link;	/* Must comes first */
	kmutex_t	hat_mutex;
	kmutex_t	hat_switch_mutex;
	struct as	*hat_as;
	uint_t		hat_stats;
	pgcnt_t		hat_pages_mapped[MMU_PAGE_SIZES];

	/*
	 * hat_cpus is used to send cross call event related to this
	 * address space.
	 */
	cpuset_t	hat_cpus;

	/* Virtual and physical address of L1PT for this address space */
	l1pte_t		*hat_l1vaddr;
	uintptr_t	hat_l1paddr;
	hat_l2bundle_t	**hat_l2bundle;	/* L2PT bundle entries */

	/* Context ID for this address space */
	uint32_t	hat_context[NCPU];

	/* Generation ID of ASID for this address space */
	uint32_t	hat_asid_gen[NCPU];

	uint16_t	hat_flags;	/* Flags (See below) */
	uint16_t	hat_basel1idx;	/* Base L1PT index of this mapping */
	uint16_t	hat_numbundle;	/* Number of hat_l2bundle entries */
};
#endif	/* _MACHDEP */

#define	hat_next	hat_link.hl_next
#define	hat_prev	hat_link.hl_prev

/*
 * hat list management
 */

/* Link hat at the top of the list */
#define	HAT_LIST_INSERT(hat, head)					\
	do {								\
		(hat)->hat_next = (head)->hat_next;			\
		(hat)->hat_prev = (hat_list_t *)(head);			\
		(head)->hat_next->hl_prev = (hat_list_t *)(hat);	\
		(head)->hat_next = (hat_list_t *)(hat);			\
	} while (0)

/* Unlink hat from the list */
#define	HAT_LIST_UNLINK(hat)					\
	do {							\
		(hat)->hat_next->hl_prev = (hat)->hat_prev;	\
		(hat)->hat_prev->hl_next = (hat)->hat_next;	\
	} while (0)

#define	HAT_LIST_FOREACH(hat, head)			\
	for ((hat) = (hat_t *)((head)->hat_next);	\
	     (hat) != (hat_t *)&((head)->hat_link);	\
	     (hat) = (hat_t *)((hat)->hat_next))

#define	PGCNT_COUNT(hat, szc, cnt)					\
	atomic_add_long(&((hat)->hat_pages_mapped[szc]), (cnt));
#define	PGCNT_INC(hat, szc)	PGCNT_COUNT(hat, szc, 1)
#define	PGCNT_DEC(hat, szc)	PGCNT_COUNT(hat, szc, -1)

/*
 * Flags for the hat_flags field
 *
 * HAT_FREEING - set when HAT is being destroyed - mostly used to detect that
 *	demap()s can be avoided.
 *
 * HAT_VLP - indicates a 32 bit process has a virtual address range less than
 *	the hardware's physical address range. (VLP->Virtual Less-than Physical)
 *
 * HAT_VICTIM - This is set while a hat is being examined for page table
 *	stealing and prevents it from being freed.
 *
 * HAT_SHARED - The hat has exported it's page tables via hat_share()
 *
 * HAT_KERNEL - The hat is kernel master hat.
 */
#define	HAT_FREEING	(0x0001)
#define	HAT_VLP		(0x0002)
#define	HAT_VICTIM	(0x0004)
#define	HAT_SHARED	(0x0008)
#define	HAT_KERNEL	(0x0010)

/* Determine whether the hat is kernel hat */
#define	HAT_IS_KERNEL(hat)	((hat)->hat_flags & HAT_KERNEL)

/*
 * Additional platform attribute for hat_devload() to force no caching.
 */
#define	HAT_PLAT_NOCACHE	0x100000

/*
 * Additional platform attribute for hat_devload() to force disable external
 * cache.
 */
#define	HAT_PLAT_NOEXTCACHE	0x200000

/*
 * Additional platform attribute for hat_unload() that spares locked mappings
 */
#define	HAT_PLAT_UNLOAD_NOLOCK	0x100000

/*
 * Domain used to map virtual address.
 * We use 2 domains. HAT_DOMAIN_KERNEL is for kernel space, and
 * HAT_DOMAIN_USER is for user.
 * The kernel context must own access permission to access both domains
 * or domain fault may be raised on TLB single entry flush.
 */
#define	HAT_DOMAIN_KERNEL	15
#define	HAT_DOMAIN_USER		0

/*
 * Simple statistics for the HAT. These are just counters that are
 * atomically incremented. They can be reset directly from the kernel
 * debugger.
 */
struct hatstats {
	uint64_t	hs_l1pt_allocs;
	uint64_t	hs_l1pt_frees;
	uint64_t	hs_l2pt_allocs;
	uint64_t	hs_l2pt_frees;
	uint64_t	hs_l2pt_resvallocs;	/* Allocs from reserve */
	uint64_t	hs_l2pt_resvfrees;	/* Puts back to reserve */
	uint64_t	hs_l2bundle_allocs;
	uint64_t	hs_l2bundle_frees;
	uint64_t	hs_l2bundle_resvallocs;	/* Allocs from reserve */
	uint64_t	hs_l2bundle_resvfrees;	/* Puts back to reserve */
	uint64_t	hs_hm_alloc;
	uint64_t	hs_hm_free;
	uint64_t	hs_hm_put_reserve;
	uint64_t	hs_hm_get_reserve;
	uint64_t	hs_hm_steals;
	uint64_t	hs_hm_steal_exam;
};

/* Doubleword alignment is for optimization of atomic_add_64(). */
#pragma align	8(hatstat)
extern struct hatstats hatstat;
#define	HATSTAT_INC(x)	(atomic_add_64(&hatstat.x, 1))

#if defined(_KERNEL)

/*
 * tlbf_ctx structure is used to reduce cross call to flush TLB.
 * If kernel TLB flush is invoked more than TLBF_MAX_FLUSH times,
 * flush entrire TLB.
 */
#define	TLBF_MAX_FLUSH	4

typedef struct tlbf_ctx {
	uint_t	t_flushcnt;	/* Flush counter for kernel space */
	uint_t	t_flushall;	/* Whether entire TLB should be flushed */
	hat_t	*t_uhat;	/* user hat structure to be flushed */
	uint_t	t_flags;
#ifdef	DEBUG
	uint_t	t_initialized;
#endif	/* DEBUG */
} tlbf_ctx_t;

/* Flags for t_flags */
#define	TLBF_UHAT_SYNC		0x1	/* Flush user hat immediately */

#ifdef	DEBUG

#define	TLBF_CTX_INIT_MAGIC		0xdeadbeef
#define	TLBF_CTX_DEBUG_INIT(ctx)				\
	do {							\
		(ctx)->t_initialized = TLBF_CTX_INIT_MAGIC;	\
	} while (0)
#define	TLBF_CTX_INITIALIZED(ctx)				\
	ASSERT((ctx)->t_initialized == TLBF_CTX_INIT_MAGIC)

#else	/* !DEBUG */

#define	TLBF_CTX_DEBUG_INIT(ctx)	/* NOP */
#define	TLBF_CTX_INITIALIZED(ctx)	/* NOP */

#endif	/* DEBUG */

#define	TLBF_CTX_INIT(ctx, flags)		\
	do {					\
		(ctx)->t_flushcnt = 0;		\
		(ctx)->t_uhat = NULL;		\
		(ctx)->t_flushall = 0;		\
		(ctx)->t_flags = (flags);	\
		TLBF_CTX_DEBUG_INIT(ctx);	\
	} while (0)

#define	TLBF_CTX_FLUSH(ctx, hat, vaddr)					\
	do {								\
		ASSERT(HAT_IS_LOCKED(hat));				\
		TLBF_CTX_INITIALIZED(ctx);				\
		if (HAT_IS_KERNEL(hat) ||				\
		    ((ctx)->t_flags & TLBF_UHAT_SYNC)) {		\
			if ((ctx)->t_flushcnt < TLBF_MAX_FLUSH) {	\
				/*					\
				 * vaddr is not used unless hat is	\
				 * kernel hat.				\
				 */					\
				hat_flushtlb((hat), (vaddr), NULL);	\
				(ctx)->t_flushcnt++;			\
			}						\
			else {						\
				(ctx)->t_flushall = 1;			\
			}						\
		}							\
		else if (!((hat)->hat_flags & HAT_FREEING)) {		\
			if ((ctx)->t_uhat == NULL) {			\
				/*					\
				 * TLB entries for user space will be	\
				 * flushed by TLBF_CTX_FINI().		\
				 */					\
				(ctx)->t_uhat = (hat);			\
			}						\
			else {						\
				(ctx)->t_flushall = 1;			\
			}						\
		}							\
	} while (0)

#define	TLBF_CTX_FINI(ctx)						\
	do {								\
		TLBF_CTX_INITIALIZED(ctx);				\
		if ((ctx)->t_flushall) {				\
			hat_flushtlb(&hat_kas, HAT_FLUSHTLB_ALL, NULL);	\
		}							\
		else if ((ctx)->t_uhat != NULL) {			\
			ASSERT(!((ctx)->t_flags & TLBF_UHAT_SYNC));	\
			hat_flushtlb((ctx)->t_uhat, HAT_FLUSHTLB_ALL,	\
				     NULL);				\
		}							\
	} while (0)

/*
 * Useful macro to align hat_XXX() address arguments to a page boundary
 */
#define	PAGE_ROUNDDOWN(a)	((uintptr_t)(a) & MMU_PAGEMASK)
#define	PAGE_ROUNDUP(a)		(mmu_btopr(a) << MMU_PAGESHIFT)
#define	IS_PAGEALIGNED(a)	(((uintptr_t)(a) & MMU_PAGEOFFSET) == 0)

/* Mutex macros */
#define	HAT_LOCK(hat)		mutex_enter(&(hat)->hat_mutex)
#define	HAT_UNLOCK(hat)		mutex_exit(&(hat)->hat_mutex)
#define	HAT_SWITCH_LOCK(hat)	mutex_enter(&(hat)->hat_switch_mutex)
#define	HAT_SWITCH_UNLOCK(hat)	mutex_exit(&(hat)->hat_switch_mutex)
#define	HAT_IS_LOCKED(hat)	mutex_owned(&(hat)->hat_mutex)
#define	HAT_KAS_LOCK()		HAT_LOCK(&hat_kas)
#define	HAT_KAS_UNLOCK()	HAT_UNLOCK(&hat_kas)
#define	HAT_KAS_IS_LOCKED()	HAT_IS_LOCKED(&hat_kas)

extern kmutex_t hat_list_lock;

extern hat_t		hat_kas;
extern uintptr_t	hat_kmap_start;
extern uintptr_t	hat_kmap_end;

/*
 * Interfaces to setup a cpu private mapping (ie. preemption disabled).
 * The attr and flags arguments are the same as for hat_devload().
 *
 * Used by ppcopy(), page_zero(), the memscrubber, and the kernel debugger.
 */
extern void	hat_mempte_remap(pfn_t pfn, caddr_t addr, l2pte_t *ptep,
				 uint_t attr, uint_t flags);

#define	HAT_USE_PTELOAD_RETRY	1

#ifdef	HAT_USE_PTELOAD_RETRY
/*
 * This function is a workaround for dubious behaviour of MPCore.
 */
extern boolean_t	hat_pteload_retry(struct regs *rp, caddr_t addr,
					  enum seg_rw rw);
#else	/* !HAT_USE_PTELOAD_RETRY */
#define	hat_pteload_retry(rp, addr, rw)	(B_FALSE)
#endif	/* HAT_USE_PTELOAD_RETRY */

/*
 * interfaces to manage which thread has access to htable and hment reserves
 */
extern uint_t hat_can_steal_post_boot;
extern uint_t hat_use_boot_reserve;

/*
 * initialization stuff needed by by startup, mp_startup...
 */
extern void	hat_cpu_online(struct cpu *cp);
extern void	hat_cpu_offline(struct cpu *cp);

/*
 * magic value to indicate that all TLB entries should be demapped.
 */
#define	HAT_FLUSHTLB_ALL	(~(uintptr_t)0)

/*
 * Hat switch function invoked to load a new context into TTB.
 */
extern void hat_switch(struct hat *hat);

extern uint32_t	hat_bootparam(char *param, uint32_t def, uint32_t min,
			      uint32_t max);
extern void	hat_mmu_init(void);
extern void	hat_mlsetup(void);
extern void	hat_bootstrap(void);
extern void	hat_boot_mapin(caddr_t start, uintptr_t paddr, size_t size,
			       uint32_t flags);
extern void	hat_kernpt_init(void);
extern void	hat_kmap_init(uintptr_t base, size_t len);
extern l1pte_t	hat_l1pt_mkpte(pfn_t pfn, uint32_t attr, uint32_t flags,
			       uint32_t szc, l1pte_t *swflags);
extern l2pte_t	hat_l2pt_mkpte(pfn_t pfn, uint32_t attr, uint32_t flags,
			       uint32_t szc, l2pte_t *swflags);

extern hment_t	*hat_page_unmap(page_t *pp, void *pt, uint_t entry,
				uint_t szc, tlbf_ctx_t *tlbf);
extern int	hat_softfault(caddr_t addr, enum seg_rw rw, boolean_t usermode,
			      boolean_t *userpte);

/* Function types for hat_xcall() */
typedef int	(*hat_xcallfunc_t)(void *a1, void *a2, void *a3);

/* Flags for hat_xcall() */
#define	HATXC_ALLCPU		0x1

extern void	hat_xcall(hat_t *hat, hat_xcallfunc_t func, void *a1, void *a2,
			  void *a3, uint_t flags);

/* Return value for hat_softfault(). */
#define	HAT_SF_UNCLAIMED	0x0		/* not handled */
#define	HAT_SF_HANDLED		0x1		/* handled */
#define	HAT_SF_HASPERM		0x2		/* already granted */

/*
 * Special value for syncpp parameter of hat_flushtlb().
 * If this value is specified, hat_flushtlb() cleans entire L1 data cache,
 * and invalidate entire L1 instruction cache.
 */
#define	HAT_FLUSHTLB_SYNCALL	((page_t *)-1)

extern void	hat_flushtlb(hat_t *hat, uintptr_t vaddr, page_t *syncpp);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* !_VM_HAT_ARM_H */
