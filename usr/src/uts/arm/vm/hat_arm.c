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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2006-2009 NEC Corporation
 */

#ident	"@(#)arm/vm/hat_arm.c"

/*
 * VM - Hardware Address Translation management for ARM (v6 or later).
 *
 * Implementation of the interfaces described in <common/vm/hat.h>
 *
 * Nearly all the details of how the hardware is managed should not be
 * visible outside this layer except for misc. machine specific functions
 * that work in conjunction with this code.
 */

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/bootconf.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/kobj.h>
#include <sys/kobj_lex.h>
#include <sys/prom_debug.h>
#include <sys/cachectl.h>
#include <sys/cpuvar.h>
#include <sys/vnode.h>
#include <sys/x_call.h>
#include <sys/controlregs.h>
#include <sys/xramdev_impl.h>
#include <sys/lpg_config.h>
#include <asm/tlb.h>
#include <vm/hat.h>
#include <vm/hat_arm.h>
#include <vm/hat_machdep.h>
#include <vm/vm_dep.h>
#include <vm/seg_kmem.h>

#define	HAT_MULTI_CPU()		(max_ncpus > 1) 

 
#ifdef	DEBUG
static int	hat_debug;

#define	HAT_DEBUG(x)				\
	do {					\
		if (hat_debug) {		\
			PRM_DEBUG(x);		\
		}				\
	} while (0)

#define	HAT_PRINTF(...)					\
	do {						\
		if (hat_debug) {			\
			PRM_PRINTF(__VA_ARGS__);	\
		}					\
	} while (0)

#else	/* !DEBUG */

#define	HAT_DEBUG(x)
#define	HAT_PRINTF(...)

#endif	/* DEBUG */

/*
 * Check whether the new mapping should be visible mapping (chain page using
 * hment structure) or not.
 */
#define	IS_VISIBLE_MAP(pp, hatflag)				\
	((pp) != NULL && ((hatflag) & HAT_LOAD_NOCONSIST) == 0)

/*
 * Useful stuff for atomic access/clearing/setting REF/MOD/RO bits in
 * page structure.
 */
#define	PP_GETRM(pp, rmmask)	((pp)->p_nrm & (rmmask))
#define	PP_GENERIC_ATTR(pp)	PP_GETRM(pp, P_MOD|P_REF|P_RO)
#define	PP_ISMOD(pp)		PP_GETRM(pp, P_MOD)
#define	PP_ISREF(pp)		PP_GETRM(pp, P_REF)
#define	PP_ISRO(pp)		PP_GETRM(pp, P_RO)

#define	PP_SETRM(pp, rm)	atomic_or_8(&((pp)->p_nrm), (rm))
#define	PP_SETMOD(pp)		PP_SETRM(pp, P_MOD)
#define	PP_SETREF(pp)		PP_SETRM(pp, P_REF)
#define	PP_SETRO(pp)		PP_SETRM(pp, P_RO)

#define	PP_CLRRM(pp, rm)	atomic_and_8(&((pp)->p_nrm), ~(rm))
#define	PP_CLRMOD(pp)		PP_CLRRM(pp, P_MOD)
#define	PP_CLRREF(pp)		PP_CLRRM(pp, P_REF)
#define	PP_CLRRO(pp)		PP_CLRRM(pp, P_RO)
#define	PP_CLRALL(pp)		PP_CLRRM(pp, P_MOD|P_REF|P_RO)

/*
 * Determine page size to use for the mapping.
 */
#define	HAT_CHOOSE_PAGESIZE(vaddr, pfn, rsize, szc, l1pt, l1ptep, el1ptep, \
			    pgsz)					\
	do {								\
		if (L1PT_SPSECTION_ALIGNED(vaddr) &&			\
		    HAT_PFN_IS_SPSECTION_ALIGNED(pfn) &&		\
		    (rsize) >= L1PT_SPSECTION_VSIZE) {			\
			(szc) = SZC_SPSECTION;				\
			(l1ptep) = (l1pt) + L1PT_INDEX(vaddr);		\
			(el1ptep) = (l1ptep) + L1PT_SPSECTION_NPTES;	\
			(pgsz) = L1PT_SPSECTION_VSIZE;			\
		}							\
		else if (L1PT_SECTION_ALIGNED(vaddr) &&			\
			 HAT_PFN_IS_SECTION_ALIGNED(pfn) &&		\
			 (rsize) >= L1PT_SECTION_VSIZE) {		\
			(szc) = SZC_SECTION;				\
			(l1ptep) = (l1pt) + L1PT_INDEX(vaddr);		\
			(el1ptep) = (l1ptep) + 1;			\
			(pgsz) = L1PT_SECTION_VSIZE;			\
		}							\
		else if (L2PT_LARGE_ALIGNED(vaddr) &&			\
			 HAT_PFN_IS_LARGE_ALIGNED(pfn) &&		\
			 (rsize) >= L2PT_LARGE_VSIZE) {			\
			(szc) = SZC_LARGE;				\
			(l1ptep) = NULL;				\
			(pgsz) = L2PT_LARGE_VSIZE;			\
		}							\
		else {							\
			(l1ptep) = NULL;				\
			(szc) = 0;					\
			(pgsz) = MMU_PAGESIZE;				\
		}							\
									\
		if ((l1ptep) != NULL) {					\
			l1pte_t	*__p;					\
									\
			/* Check whether we can use section mapping. */	\
			for (__p = (l1ptep); __p < (el1ptep); __p++) {	\
				if (L1PT_PTE_IS_COARSE(*__p)) {		\
					/*				\
					 * At least one L2PT exists.	\
					 * We can't use section mapping.\
					 */				\
					(l1ptep) = NULL;		\
					(szc) = 0;			\
					(pgsz) = MMU_PAGESIZE;		\
					break;				\
				}					\
			}						\
		}							\
	} while (0)

/*
 * Simple statistics
 */
struct hatstats	hatstat;

#ifndef	LPG_DISABLE
uint_t	mmu_page_sizes;

/* How many page sizes the users can see */
uint_t	mmu_exported_page_sizes;
#endif	/* !LPG_DISABLE */

/* Use static hat for kernel */
hat_t		hat_kas;

/* Keep address range of kmap for optimization. */
uintptr_t	hat_kmap_start;
uintptr_t	hat_kmap_end;

uint_t	hat_use_boot_reserve = 1;	/* Cleared after early boot process */
uint_t	hat_can_steal_post_boot = 0;	/* Set late in boot to enable
					   stealing */
uint_t	hat_mp_startup_running = 0;	/* Set 1 while mp_startup() is
					   running. */

/* Mutex to serialize active hat list. */
kmutex_t	hat_list_lock;

/* Parameters to keep cache coherency. */
uint32_t		hat_ttb_shared;

/* Caches for user hat. */
static kmem_cache_t	*hat_cache;
static kmem_cache_t	*hat_l2btable_cache;

/* Parameters to keep cache coherency. */
static l1pte_t		hat_l1pt_shared;
static l2pte_t		hat_l2pt_shared;

/* Internal prototypes */
static void	hat_kmap_memload(hat_t *hat, caddr_t addr, page_t *pp,
				 uint_t attr, uint_t flags);
static void	hat_kmap_unload(hat_t *hat, caddr_t addr, size_t len,
				uint_t flags);
static void	hat_ptesync(page_t *pp, uint32_t sw, pgcnt_t pgcnt);
static void	hat_l1pt_pteupdate(hat_t *hat, uintptr_t vaddr,
				   l1pte_t *l1ptep, hat_l2pt_t *swflags,
				   l1pte_t newpte, l1pte_t newsw, page_t *pp,
				   tlbf_ctx_t *tlbf);
static void	hat_l2pt_pteupdate(hat_t *hat, uintptr_t vaddr, l2pte_t *ptep,
				   l2pte_t *swflags, l2pte_t newpte,
				   l2pte_t newsw, page_t *pp,
				   tlbf_ctx_t *tlbf);
static void	hat_l1pt_pteload(hat_t *hat, uintptr_t vaddr, pfn_t pfn,
				 uint32_t szc, page_t *pp, uint_t attr,
				 uint_t flags, hat_l2pt_t *swflags,
				 tlbf_ctx_t *tlbf);
static void	hat_l2pt_pteload(hat_t *hat, hat_l2pt_t *l2pt, uintptr_t vaddr,
				 pfn_t pfn, uint32_t szc, page_t *pp,
				 uint_t attr, uint_t flags, tlbf_ctx_t *tlbf);
static boolean_t	hat_l1pt_unload(hat_t *hat, l1pte_t *l1ptepp,
					uint16_t l1idx, uintptr_t *vaddrp,
					uint_t flags, tlbf_ctx_t *tlbf,
					boolean_t *coarse);
static boolean_t	hat_l2pt_unload(hat_t *hat, hat_l2pt_t *l2pt,
					uintptr_t *vaddrp, uint_t flags,
					tlbf_ctx_t *tlbf);
static int	hat_flushtlb_vaddr(void *a1, void *a2, void *a3);
static int	hat_flushtlb_asid(void *a1, void *a2, void *a3);
static int	hat_constructor(void *buf, void *handle, int kmflags);
static void	hat_destructor(void *buf, void *handle);
static void	hat_page_clrwrt(page_t *pp);
static void	hat_do_pageunload(page_t *pp, uint_t minszc);
static boolean_t	hat_hment_tryenter(page_t *pp, hat_t *hat);

/*
 * l1pte_t
 * hat_l1pt_mkpte(pfn_t pfn, uint32_t attr, uint32_t flags, uint32_t szc,
 *		  l1pte_t *swflags)
 *	Create L1PT entry to map the specified pfn.
 *	szc is a page size code in p_szc field in the page structure.
 *
 *	swflags is a pointer to room for software PTE flags.
 *	hat_l1pt_mkpte() stores software flags into it.
 *
 * Remarks:
 *	This function always grant read permission.
 *
 *	This function is used to create section or supersection mapping.
 *	Use L1PT_MKL2PT() macro instead to create L1PT entry to link L2PT.
 */
l1pte_t
hat_l1pt_mkpte(pfn_t pfn, uint32_t attr, uint32_t flags, uint32_t szc,
	       l1pte_t *swflags)
{
	l1pte_t		pte, sw = 0, tex = 0, chpte = 0;
	uint32_t	chattr;
	uint32_t	domain;

	ASSERT(szc == SZC_SPSECTION || szc == SZC_SECTION);
	ASSERT(PFN_BASE(pfn, szc) == pfn);

	chattr = attr & HAT_ORDER_MASK;
	pte = (l1pte_t)((pfn) << MMU_PAGESHIFT) | L1PT_TYPE_SECTION;

	if (szc == SZC_SPSECTION) {
		/* Supersection mapping */
		pte |= L1PT_SPSECTION;
	}

	if (attr & PROT_USER) {
		pte |= L1_PROT_USER;

		/* Set non-global bit */
		pte |= L1PT_NG;

		domain = L1PT_DOMAIN(HAT_DOMAIN_USER);
	}
	else {
		domain = L1PT_DOMAIN(HAT_DOMAIN_KERNEL);
	}

	/*
	 * Although domain field is valid only section mapping, this code
	 * is safe because domain field in supersection mapping is always
	 * ignored.
	 */
	pte |= domain;

	if (attr & PROT_EXEC) {
		sw |= PTE_S_READ|PTE_S_EXEC;
	}
	else {
		pte |= L1PT_XN;
	}

	if (attr & HAT_NOSYNC) {
		sw |= PTE_S_NOSYNC;
	}
	if (flags & HAT_LOAD_NOCONSIST) {
		sw |= PTE_S_NOCONSIST|PTE_S_NOSYNC;
	}
	if (flags & HAT_LOAD_LOCK) {
		sw |= PTE_S_LOCKED;
	}

	/* Set protection bits */
	if (attr & PROT_WRITE) {
		sw |= PTE_S_READ|PTE_S_WRITE;
	}
	else if (attr & PROT_READ) {
		sw |= PTE_S_READ;
	}
	if (NEED_SOFTFAULT(sw)) {
		/*
		 * Turn off hardware access bit to detect references or
		 * modifications to this mapping.
		 */
		L1PT_PTE_CLR_RW(pte);
	}
	else {
		/* No software bit emulation is needed. */
		pte |= L1_PROT_RW;

		if (!(attr & PROT_WRITE)) {
			pte |= L1_PROT_READONLY;
		}
	}

	*swflags = sw;

	/* Set caching attributes */
	switch (chattr) {
	case HAT_LOADCACHING_OK:
	case HAT_MERGING_OK:
		/*
		 * Uncached, bufferable device.
		 * HAT_LOADCACHING_OK is ignored because MPCore doesn't
		 * support write-through cache.
		 */
		chpte = L1PT_BUFFERABLE;
		/* FALLTHROUGH */

	case HAT_STRICTORDER:
		/* Strongly Ordered Device */
		tex = L1PT_TEX(PTE_TEX_NOALLOC);
		break;

	case HAT_UNORDERED_OK:
		/* Treat as cached. */
		/* FALLTHROUGH */

	case HAT_STORECACHING_OK:
		/*
		 * Normal cached memory
		 * (Outer and Inner Write-Back, Write Allocate)
		 */
		tex = L1PT_TEX(PTE_TEX_WALLOC);
		chpte = L1PT_BUFFERABLE|L1PT_CACHED;
		break;

	default:
		panic("hat_l1pt_mkpte: Bad caching attributes: 0x%x", chattr);
		/* NOTREACHED */
	}

	if (attr & HAT_PLAT_NOEXTCACHE) {
		/* Force to disable external cache. */
		tex = L1PT_TEX(PTE_TEX_EXT_NOCACHE);

		/*
		 * If 0b100 bit is set in TEX field, we must use different
		 * encoding to specify inner cache policy.
		 */
		if (chpte & L1PT_CACHED) {
			/* Write back, allocate on write, bufferred. */
			chpte = L1PT_BUFFERABLE;
		}
		else {
			/*
			 * We must treat as uncached because MPCore level 1
			 * cache doesn't support cache policy without
			 * allocation on write.
			 */
			chpte = 0;
		}
	}
	pte |= tex;
	pte |= chpte;

	/* Set shared bit if 2 or more CPUs are online. */
	pte |= hat_l1pt_shared;

	return pte;
}

/*
 * l2pte_t
 * hat_l2pt_mkpte(pfn_t pfn, uint32_t attr, uint32_t flags, uint32_t szc,
 *		  l2pte_t *swflags)
 *	Create L2PT entry to map the specified pfn.
 *	szc is a page size code in p_szc field in the page structure.
 *
 *	swflags is a pointer to room for software PTE flags.
 *	hat_l2pt_mkpte() stores software flags into it.
 *
 * Remarks:
 *	This function always grant read permission.
 */
l2pte_t
hat_l2pt_mkpte(pfn_t pfn, uint32_t attr, uint32_t flags, uint32_t szc,
	       l2pte_t *swflags)
{
	l2pte_t		pte, xn, sw = 0, noalloc, walloc, noext, tex = 0;
	l2pte_t		chpte = 0;
	uint32_t	chattr;

	ASSERT(szc == SZC_SMALL || szc == SZC_LARGE);
	ASSERT(PFN_BASE(pfn, szc) == pfn);

	chattr = attr & HAT_ORDER_MASK;
	pte = (l2pte_t)((pfn) << MMU_PAGESHIFT);

	if (szc == SZC_SMALL) {
		/* Small page (4K) */
		pte |= L2PT_TYPE_SMALL;
		xn = L2PT_SMALL_XN;
		noalloc = L2PT_SMALL_TEX(PTE_TEX_NOALLOC);
		walloc = L2PT_SMALL_TEX(PTE_TEX_WALLOC);
		noext = L2PT_SMALL_TEX(PTE_TEX_EXT_NOCACHE);
	}
	else {
		/* Large page (64K) */
		pte |= L2PT_TYPE_LARGE;
		xn = L2PT_LARGE_XN;
		noalloc = L2PT_LARGE_TEX(PTE_TEX_NOALLOC);
		walloc = L2PT_LARGE_TEX(PTE_TEX_WALLOC);
		noext = L2PT_LARGE_TEX(PTE_TEX_EXT_NOCACHE);
	}

	if (attr & PROT_USER) {
		pte |= L2_PROT_USER;

		/* Set non-global bit */
		pte |= L2PT_NG;
	}

	if (attr & PROT_EXEC) {
		sw |= PTE_S_READ|PTE_S_EXEC;
	}
	else {
		pte |= xn;
	}

	if (attr & HAT_NOSYNC) {
		sw |= PTE_S_NOSYNC;
	}
	if (flags & HAT_LOAD_NOCONSIST) {
		sw |= PTE_S_NOCONSIST|PTE_S_NOSYNC;
	}
	if (flags & HAT_LOAD_LOCK) {
		sw |= PTE_S_LOCKED;
	}

	/* Set protection bits */
	if (attr & PROT_WRITE) {
		sw |= PTE_S_READ|PTE_S_WRITE;
	}
	else if (attr & PROT_READ) {
		sw |= PTE_S_READ;
	}
	if (NEED_SOFTFAULT(sw)) {
		/*
		 * Turn off hardware access bit to detect references or
		 * modifications to this mapping.
		 */
		L2PT_PTE_CLR_RW(pte);
	}
	else {
		/* No software bit emulation is needed. */
		pte |= L2_PROT_RW;

		if (!(attr & PROT_WRITE)) {
			pte |= L2_PROT_READONLY;
		}
	}

	*swflags = sw;

	/* Set caching attributes */
	switch (chattr) {
	case HAT_LOADCACHING_OK:
	case HAT_MERGING_OK:
		/*
		 * Uncached, bufferable device.
		 * HAT_LOADCACHING_OK is ignored because MPCore doesn't
		 * support write-through cache.
		 */
		chpte = L2PT_BUFFERABLE;
		/* FALLTHROUGH */

	case HAT_STRICTORDER:
		/* Strongly Ordered Device */
		tex = noalloc;
		break;

	case HAT_UNORDERED_OK:
		/* Treat as cached. */
		/* FALLTHROUGH */

	case HAT_STORECACHING_OK:
		/*
		 * Normal cached memory
		 * (Outer and Inner Write-Back, Write Allocate)
		 */
		tex = walloc;
		chpte = L2PT_BUFFERABLE|L2PT_CACHED;
		break;

	default:
		panic("hat_l2pt_mkpte: Bad caching attributes: 0x%x", chattr);
		/* NOTREACHED */
	}

	if (attr & HAT_PLAT_NOEXTCACHE) {
		/* Force to disable external cache. */
		tex = noext;

		/*
		 * If 0b100 bit is set in TEX field, we must use different
		 * encoding to specify inner cache policy.
		 */
		if (chpte & L2PT_CACHED) {
			/* Write back, allocate on write, bufferred. */
			chpte = L2PT_BUFFERABLE;
		}
		else {
			/*
			 * We must treat as uncached because MPCore level 1
			 * cache doesn't support cache policy without
			 * allocation on write.
			 */
			chpte = 0;
		}
	}
	pte |= tex;
	pte |= chpte;

	/* Set shared bit if 2 or more CPUs are online. */
	pte |= hat_l2pt_shared;

	return pte;
}

/*
 * static inline boolean_t
 * hat_hment_tryenter(page_t *pp, hat_t *hat)
 *	Try to acquire hment lock without releasing hat lock.
 *
 * Calling/Exit State:
 *	Returns B_TRUE if hat_hment_tryenter() has acquired hment mutex
 *	without releasing hat lock. Otherwise, it releases hat lock, and
 *	acquires hment lock, reclaims hat lock, and then returns B_FALSE.
 */
static inline boolean_t
hat_hment_tryenter(page_t *pp, hat_t *hat)
{
	boolean_t	ret = hment_tryenter(pp);

	ASSERT(HAT_IS_LOCKED(hat));

	if (!ret) {
		/* Acquire hment and HAT lock in lock hierarchy order. */
		HAT_UNLOCK(hat);
		hment_enter(pp);
		HAT_LOCK(hat);
	}

	return ret;
}

/*
 * uint32_t
 * hat_bootparam(char *param, uint32_t def, uint32_t min, uint32_t max)
 *	Read boot property named param.
 *	If no property, def will be returned.
 */
uint32_t
hat_bootparam(char *param, uint32_t def, uint32_t min, uint32_t max)
{
	char		prop[32];
	uint32_t	value = def;
	u_longlong_t	lvalue;

	if ((BOP_GETPROPLEN(bootops, param) <= sizeof(prop)) &&
	    (BOP_GETPROP(bootops, param, prop) >= 0) &&
	    (kobj_getvalue(prop, &lvalue) != -1)) {
		value = (uint32_t)lvalue;
	}
	if (value < min) {
		value = min;
	}
	else if (value > max) {
		value = max;
	}

	return value;
}

/*
 * void
 * hat_mmu_init(void)
 *	Initialize MMU-related data structure.
 */
void
hat_mmu_init(void)
{
#ifndef	LPG_DISABLE
	/* Currently, only 4K page is supported for normal mapping */
	mmu_page_sizes = 1;
	mmu_exported_page_sizes = 1;
#endif	/* !LPG_DISABLE */
}

/*
 * void
 * hat_mlsetup(void)
 *	mlsetup() work for HAT layer.
 *
 * Remarks:
 *	This function must be called just after mp_ncpus initialization.
 */
void
hat_mlsetup(void)
{
	/* Initialize shared flags. */
	if (HAT_MULTI_CPU()) {
		hat_l1pt_shared = L1PT_SHARED;
		hat_l2pt_shared = L2PT_SHARED;
		hat_ttb_shared = TTB_SHARED;
	}
	hat_ttb_shared |= TTB_RGN_WALLOC;	/* Write Allocate */
}

/*
 * void
 * hat_bootstrap(void)
 *	Early initialization of HAT layer.
 *	- Initialize hat structure for kernel.
 *	- Allocate static data for HAT management.
 */
void
hat_bootstrap(void)
{
	int	i;

#ifdef	DEBUG
	hat_debug = hat_bootparam("hat-debug", 0, 0, 1);
#endif	/* DEBUG */

	/* Initialize hat structure for kernel. */
	kas.a_hat = &hat_kas;
	hat_kas.hat_as = &kas;
	hat_kas.hat_flags = HAT_KERNEL;

	/*
	 * Initialize hat_mutex as adaptive.
	 * We don't need to initialize hat_switch_mutex because kernel's hat
	 * never use hat_switch_mutex.
	 */
	mutex_init(&hat_kas.hat_mutex, NULL, MUTEX_DEFAULT, NULL);

	/*
	 * All hat structures in the system will be linked to hat_kas
	 * as doubly-linked ring chain.
	 */
	hat_kas.hat_next = hat_kas.hat_prev = &hat_kas.hat_link;

	/*
	 * Initialize mutex for active hat list.
	 * This mutex must be initialized as spinlock because it will be
	 * acquired while context switch.
	 */
	mutex_init(&hat_list_lock, NULL, MUTEX_DRIVER,
		   (void *)ipltospl(DISP_LEVEL));

	/* Allocate hat_l2bundle pointer table. */
	BOOT_ALLOC(hat_kas.hat_l2bundle, hat_l2bundle_t **,
		   sizeof(hat_l2bundle_t *) * L2BD_KERN_NBSIZE, BO_NO_ALIGN,
		   "Failed to allocate kernel l2bundle table");
	hat_kas.hat_numbundle = L2BD_KERN_NBSIZE;
	for (i = 0; i < L2BD_KERN_NBSIZE; i++) {
		hat_kas.hat_l2bundle[i] = NULL;
	}

	/*
	 * Set L1PT base index.
	 * hat_basel1idx is used to determine index of hat_l2bundle table
	 * associated with the virtual address.
	 */
	hat_kas.hat_basel1idx = L1PT_INDEX(KERNELBASE);

	/* Allocate static data. */
	hatpt_boot_alloc();
	hment_boot_alloc();
}

#ifdef	DEBUG
#define	ASSERT_NOMAP_SPSECTION(l1ptep)					\
	do {								\
		l1pte_t	*__ptep, *__end;				\
									\
		__end = (l1ptep) + L1PT_SPSECTION_NPTES;		\
		for (__ptep = (l1ptep); __ptep < __end; __ptep++) {	\
			ASSERT(*__ptep == 0);				\
		}							\
	} while (0)

#define	ASSERT_NOMAP_LARGE(ptep)					\
	do {								\
		l2pte_t	*__ptep, *__end;				\
									\
		__end = (ptep) + L2PT_LARGE_NPTES;			\
		for (__ptep = (ptep); __ptep < __end; __ptep++) {	\
			ASSERT(*__ptep == 0);				\
		}							\
	} while (0)
#else	/* !DEBUG */
#define	ASSERT_NOMAP_SPSECTION(l1ptep)
#define	ASSERT_NOMAP_LARGE(ptep)
#endif	/* DEBUG */

/*
 * void
 * hat_boot_mapin(caddr_t start, uintptr_t paddr, size_t size, uint32_t flags)
 *	Establish contiguous mapping for kernel static data.
 *	Physical address [paddr, paddr + size) is mapped to
 *	[start, start + size). All parameters must be page-aligned.
 *
 *	Mapping attributes can be specified via attr parameter.
 *	If you want to map RAM, use HAT_STORECACHING_OK.
 *	If you want to map device, use HAT_LOADCACHING_OK or HAT_STRICTORDER.
 *	HAT_LOADCACHING_OK will map the specified paddr with uncached and
 *	bufferable mapping.
 *	If no permission attribute is specified to attr, all permissions
 *	(Read/Write/Exec) are granted.
 *
 *	hat_boot_mapin() is called while bootstrapping.
 *
 * Remarks:
 *	- hat_boot_mapin() choose the largest page size for efficient mapping.
 *	- All pfn are mapped as cached, and NOCONSIST mapping.
 *	- Read/Write/Exec permissions are granted for all mappings.
 *	- Section mapping created by this function doesn't have software
 *	  PTE flags.
 */
void
hat_boot_mapin(caddr_t start, uintptr_t paddr, size_t size, uint32_t attr)
{
	uintptr_t	vaddr;
	pfn_t		pfn;
	l1pte_t		*l1pt = hat_kas.hat_l1vaddr;
	uint32_t	flags;
	l2pte_t		*ptep, *swptep, pte;
	hat_l2pt_t	*l2pt;

	ASSERT(hat_use_boot_reserve || hat_mp_startup_running);
	ASSERT(IS_PAGEALIGNED(start));
	ASSERT(IS_PAGEALIGNED(paddr));
	ASSERT(IS_PAGEALIGNED(size));
	ASSERT(l1pt);
	ASSERT(size > 0);

	vaddr = (uintptr_t)start;
	if ((attr & HAT_PROT_KERNEL) == 0) {
		attr |= HAT_PROT_KERNEL;
	}
	flags = HAT_LOAD_NOCONSIST;

	while (size > 0) {
		uint16_t	l1idx = L1PT_INDEX(vaddr);
		uint32_t	szc;
		pfn_t		pfn = paddr >> MMU_PAGESHIFT;

		if (L1PT_SPSECTION_ALIGNED(vaddr) &&
		    L1PT_SPSECTION_ALIGNED(paddr) &&
		    size >= L1PT_SPSECTION_VSIZE) {
			l1pte_t	*l1ptep, l1pte, sw;

			/* Map using supersection. */
			l1ptep = l1pt + l1idx;
			szc = SZC_SPSECTION;
			l1pte = hat_l1pt_mkpte(pfn, attr, flags, szc, &sw);
			HAT_PRINTF("boot_mapin: SP:vaddr[0x%08lx] paddr"
				   "[0x%08lx] ptep[0x%p] pte[0x%08x]\n",
				   vaddr, paddr, l1ptep, l1pte);
			ASSERT_NOMAP_SPSECTION(l1ptep);
			HAT_L1PTE_SPSECTION_SET(l1ptep, l1pte);

			vaddr += L1PT_SPSECTION_VSIZE;
			paddr += L1PT_SPSECTION_VSIZE;
			size -= L1PT_SPSECTION_VSIZE;
			continue;
		}
		if (L1PT_SECTION_ALIGNED(vaddr) &&
		    L1PT_SECTION_ALIGNED(paddr) &&
		    size >= L1PT_SECTION_VSIZE) {
			l1pte_t	*l1ptep, l1pte, sw;

			/* Map using section. */
			l1ptep = l1pt + l1idx;
			szc = SZC_SECTION;
			l1pte = hat_l1pt_mkpte(pfn, attr, flags, szc, &sw);
			HAT_PRINTF("boot_mapin: SC:vaddr[0x%08lx] paddr"
				   "[0x%08lx] ptep[0x%p] pte[0x%08x]\n",
				   vaddr, paddr, l1ptep, l1pte);
			ASSERT(*l1ptep == 0);
			HAT_L1PTE_SET(l1ptep, l1pte);

			vaddr += L1PT_SECTION_VSIZE;
			paddr += L1PT_SECTION_VSIZE;
			size -= L1PT_SECTION_VSIZE;
			continue;
		}
		if (L2PT_LARGE_ALIGNED(vaddr) && L2PT_LARGE_ALIGNED(paddr) &&
		    size >= L2PT_LARGE_VSIZE) {
			/*
			 * Map using large page.
			 * Large page mapping requires L2PT.
			 */
			hatpt_boot_linkl2pt(vaddr, L2PT_LARGE_VSIZE, B_FALSE);
			l2pt = hatpt_l2pt_lookup(&hat_kas, vaddr);
			ASSERT(l2pt);
			szc = SZC_LARGE;
			ptep = l2pt->l2_vaddr + L2PT_INDEX(vaddr);
			swptep = L2PT_SOFTFLAGS(ptep);
			pte = hat_l2pt_mkpte(pfn, attr, flags, szc, swptep);
			HAT_PRINTF("boot_mapin: LG:vaddr[0x%08lx] paddr"
				   "[0x%08lx] ptep[0x%p:0x%p] "
				   "pte[0x%08x:0x%08x]\n",
				   vaddr, paddr, ptep, swptep, pte, *swptep);
			ASSERT_NOMAP_LARGE(ptep);
			HAT_L2PTE_LARGE_SET(ptep, pte);
			HAT_L2PT_COUNT(l2pt, L2PT_LARGE_NPTES);

			vaddr += L2PT_LARGE_VSIZE;
			paddr += L2PT_LARGE_VSIZE;
			size -= L2PT_LARGE_VSIZE;
			continue;
		}

		/*
		 * Use small page.
		 * Small page mapping requires L2PT.
		 */
		hatpt_boot_linkl2pt(vaddr, MMU_PAGESIZE, B_FALSE);
		l2pt = hatpt_l2pt_lookup(&hat_kas, vaddr);
		ASSERT(l2pt);
		szc = 0;
		ptep = l2pt->l2_vaddr + L2PT_INDEX(vaddr);
		swptep = L2PT_SOFTFLAGS(ptep);
		pte = hat_l2pt_mkpte(pfn, attr, flags, szc, swptep);
		HAT_PRINTF("boot_mapin: SM:vaddr[0x%08lx] paddr"
			   "[0x%08lx] ptep[0x%p:0x%p] pte[0x%08x:0x%08x]\n",
			   vaddr, paddr, ptep, swptep, pte, *swptep);
		ASSERT(*ptep == 0);
		HAT_L2PTE_SET(ptep, pte);
		HAT_L2PT_INC(l2pt);

		vaddr += MMU_PAGESIZE;
		paddr += MMU_PAGESIZE;
		size -= MMU_PAGESIZE;
	}
}

/*
 * void
 * hat_kernpt_init(void)
 *	Install official kernel L1PT.
 *	PA == VA mappings mapped by _start are no longer available
 *	after this function call.
 */
void
hat_kernpt_init(void)
{
	hat_t		*hat = &hat_kas;
	uint32_t	dacr, ttb;

	/* At first, we need to clean L1 data cache. */
	DCACHE_CLEAN_ALL();

	/* Drain write buffer. */
	SYNC_BARRIER();

	/* Set DACR. */
	dacr = DACR_VALUE(HAT_DOMAIN_USER, DACR_CLIENT) |
		DACR_VALUE(HAT_DOMAIN_KERNEL, DACR_CLIENT);
	DACR_SET(dacr);

	/* Set zero into Context ID Register. */
	CONTEXT_ID_SET(0);

	/* Flush branch target cache. */
	BTC_FLUSH_ALL();

	/* Setup TTB(0). */
	ttb = (uint32_t)hat->hat_l1paddr;
	ttb |= hat_ttb_shared;
	TTB_KERN_INIT(ttb);

	/* Flush all TLB entries. */
	TLB_FLUSH();

	CPU->cpu_current_hat = &hat_kas;
}

/*
 * Initialize a special area in the kernel that always holds some PTEs for
 * faster performance. This always holds segmap's PTEs.
 */
void
hat_kmap_init(uintptr_t base, size_t len)
{
	ASSERT(IS_PAGEALIGNED(base));
	ASSERT(IS_PAGEALIGNED(len));

	hat_kmap_start = base;
	hat_kmap_end = base + len;

	/* Prepare L2PT for kmap. */
	hatpt_boot_linkl2pt(hat_kmap_start, len, B_FALSE);
}

/*
 * void
 * hat_boot_finish(void)
 *	Finalize bootstrap of HAT.
 */
void
hat_boot_finish(void)
{
	hat_use_boot_reserve = 0;
}

/*
 * void
 * hat_init(void)
 *	Initialize HAT layer.
 */
void
hat_init(void)
{
	size_t	tblsize = sizeof(hat_l2bundle_t *) * L2BD_USER_NBSIZE;

	/* Initialize cache for hat_t. */
	hat_cache = kmem_cache_create("hat_t", sizeof(hat_t), 0,
				      hat_constructor, hat_destructor, NULL,
				      NULL, NULL, 0);

	/* Initialize hat_l2bundle table cache for user space. */
	hat_l2btable_cache = kmem_cache_create("hat_l2btable", tblsize, 0,
					       NULL, NULL, NULL, NULL,
					       NULL, 0);

	hatpt_init();
	hment_init();
}

/*
 * static int
 * hat_constructor(void *buf, void *handle, int kmflags)
 *	Constructor for user process hat structure.
 */
static int
hat_constructor(void *buf, void *handle, int kmflags)
{
	hat_t	*hat = (hat_t *)buf;

	mutex_init(&hat->hat_mutex, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&hat->hat_switch_mutex, NULL, MUTEX_DRIVER,
		   (void *)ipltospl(DISP_LEVEL));

	return 0;
}

/*
 * static void
 * hat_destructor(void *buf, void *handle)
 *	Destructor for user process hat structure.
 */
static void
hat_destructor(void *buf, void *handle)
{
	hat_t	*hat = (hat_t *)buf;

	mutex_destroy(&hat->hat_mutex);
	mutex_destroy(&hat->hat_switch_mutex);
}

/*
 * hat_t *
 * hat_alloc(struct as *as)
 *	Allocate new hat structure for user space mapping.
 *
 * Calling/Exit State:
 *	The caller must hold as writer lock.
 */
hat_t *
hat_alloc(struct as *as)
{
	hat_t	*hat;
	int	i;
	l1pte_t	*l1pt;

	ASSERT(AS_WRITE_HELD(as, &as->a_lock));

	hat_can_steal_post_boot = 1;

	/* Allocate hat structure. */
	hat = kmem_cache_alloc(hat_cache, KM_SLEEP);
	hat->hat_as = as;

	/* Allocate hat_l2bundle table. */
	hat->hat_l2bundle = (hat_l2bundle_t **)
		kmem_cache_alloc(hat_l2btable_cache, KM_SLEEP);
	hat->hat_numbundle = L2BD_USER_NBSIZE;
	hat->hat_basel1idx = 0;

	hat->hat_stats = 0;
	CPUSET_ZERO(hat->hat_cpus);

	/* Initialize hat_l2bundle array. */
	HAT_L2BUNDLE_USER_INIT(hat);

	for (i = 0; i < NCPU; i++) {
		hat->hat_context[i] = 0;
		hat->hat_asid_gen[i] = 0;
	}
	hat->hat_flags = 0;
	for (i = 0; i < MMU_PAGE_SIZES; i++) {
		hat->hat_pages_mapped[i] = 0;
	}

	/* Allocate L1PT. */
	l1pt = hat->hat_l1vaddr = hatpt_l1pt_alloc(1, &hat->hat_l1paddr);
	HAT_USERPT_INIT(hat, l1pt);

	return hat;
}

/*
 * void
 * hat_free_start(hat_t *hat)
 *	hat_free_start() is called just before address space is destroyed.
 */
void
hat_free_start(hat_t *hat)
{
	ASSERT(AS_WRITE_HELD(hat->hat_as, &hat->hat_as->a_lock));

	HAT_LOCK(hat);
	hat->hat_flags |= HAT_FREEING;
	HAT_UNLOCK(hat);
}

/*
 * void
 * hat_free_end(hat_t *hat)
 *	Free up hat structure.
 */
void
hat_free_end(hat_t *hat)
{
	int	i;
	l1pte_t	*l1pt = hat->hat_l1vaddr;

#ifdef	DEBUG
	for (i = 0; i < MMU_PAGE_SIZES; i++) {
		ASSERT(hat->hat_pages_mapped[i] == 0);
	}
	for (i = 0; i < L1PT_INDEX(USERLIMIT); i++) {
		ASSERT(*(l1pt + i) == 0);
	}
	for (i = 0; i < hat->hat_numbundle; i++) {
		ASSERT(hat->hat_l2bundle[i] == NULL);
	}

#endif	/* DEBUG */

	/* Remove it from the global hat list. */
	mutex_enter(&hat_list_lock);
	ASSERT(CPU->cpu_current_hat != hat);
	HAT_LIST_UNLINK(hat);
	mutex_exit(&hat_list_lock);

	/* Free L1PT. */
	hatpt_l1pt_free(l1pt, hat->hat_l1paddr);

	kmem_cache_free(hat_l2btable_cache, hat->hat_l2bundle);
	kmem_cache_free(hat_cache, hat);
}

/*
 * static void
 * hat_ptesync(page_t *pp, uint32_t sw, pgcnt_t pgcnt)
 *	Sync reference and mofify bit in PTE to the page structure.
 *
 * Calling/Exit State:
 *	The caller must call hment_enter(pp) in advance.
 *
 *	The caller guarantee NOSYNC is not set as mapping attributes.
 *
 *	The caller must use L1PT_PTE_REFMOD_SYNC() or L2PT_PTE_REFMOD_SYNC()
 *	macro in advance. They set PTE_S_REF bit if the page has been
 *	referenced, and PTE_S_MOD bit if the page is modified.
 */
static void
hat_ptesync(page_t *pp, uint32_t sw, pgcnt_t pgcnt)
{
	uint_t	pgflag = 0;

	ASSERT(sw & PTE_S_REFMOD_SYNC);
	ASSERT(!SWPTE_IS_NOSYNC(sw));

	if (sw & PTE_S_REF) {
		pgflag |= P_REF;
	}
	if (sw & PTE_S_MOD) {
		pgflag |= P_MOD;
	}

	if (pgflag == 0) {
		return;
	}

	ASSERT(hment_owned(pp));
	ASSERT(pgcnt > 0);
	for (; pgcnt > 0; pgcnt--, pp++) {
		hat_page_setattr(pp, pgflag);
	}
}

/*
 * static void
 * hat_l1pt_pteupdate(hat_t *hat, uintptr_t vaddr, l1pte_t *l1ptep,
 *		      hat_l2pt_t *swflags, l1pte_t newpte, l1pte_t newsw,
 *		      page_t *pp, tlbf_ctx_t *tlbf)
 *	Update a L1PT entry.
 *
 * Calling/Exit State:
 *	The caller must hold hat lock.
 *	Note that hat_l1pt_pteupdate() may release hat lock.
 *	
 * Remarks:
 *	This function is used to change protection or software bits.
 *	Page frame number and pagesize can't be changed.
 */
static void
hat_l1pt_pteupdate(hat_t *hat, uintptr_t vaddr, l1pte_t *l1ptep,
		   hat_l2pt_t *swflags, l1pte_t newpte, l1pte_t newsw,
		   page_t *pp, tlbf_ctx_t *tlbf)
{
	uint_t		pgflags = 0;
	l1pte_t		l1pte = *l1ptep;
	l1pte_t		sw = HAT_L2PT_GET_SECTION_FLAGS(swflags);
	boolean_t	dirty = B_FALSE;

	ASSERT(HAT_IS_LOCKED(hat));

	if (!(sw & (PTE_S_NOSYNC|PTE_S_NOCONSIST)) &&
	    L1PT_PTE_IS_REFMOD(l1pte) &&
	    (SWPTE_IS_NOSYNC(newsw) || !SWPTE_IS_WRITABLE(newsw) ||
	     L1PT_PTE_IS_REFMOD_CHANGED(l1pte, newpte))) {
		ASSERT(pp);
		ASSERT(hment_owned(pp));
		if (L1PT_PTE_IS_MOD(l1pte)) {
			pgflags |= P_MOD;
			dirty = B_TRUE;
		}
		if (L1PT_PTE_IS_REF(l1pte)) {
			pgflags |= P_REF;
		}
		L1PT_PTE_CLR_REFMOD(newpte);
	}

	if (!SWPTE_IS_WRITABLE(newsw)) {
		ASSERT(SWPTE_IS_READABLE(newsw));
		if (NEED_SOFTFAULT(newsw)) {
			L1PT_PTE_CLR_WRITABLE(newpte);
		}
		else {
			L1PT_PTE_SET_READONLY(newpte);
		}
	}
	else if (!NEED_SOFTFAULT(newsw)) {
		L1PT_PTE_SET_RW(newpte);
	}

	if (SWPTE_IS_EXECUTABLE(newsw)) {
		if (L1PT_PTE_IS_USER(newpte) && !SWPTE_IS_NOSYNC(newsw) &&
		    (l1pte & L1PT_CACHED) &&
		    (dirty || !L1PT_PTE_IS_EXECUTABLE(l1pte))) {
			/*
			 * Set up software exec fault for instruction cache
			 * maintenance.
			 */
			L1PT_PTE_CLR_EXECUTABLE(newpte);
		}
		else {
			L1PT_PTE_SET_EXECUTABLE(newpte);
		}
	}
	else {
		L1PT_PTE_CLR_EXECUTABLE(newpte);
	}

	if (l1pte != newpte || sw != newsw) {
		if (L1PT_PTE_IS_SPSECTION(newpte)) {
			HAT_L1PTE_SPSECTION_SET(l1ptep, newpte);
		}
		else {
			HAT_L1PTE_SET(l1ptep, newpte);
		}
		HAT_L2PT_SET_SECTION_FLAGS(swflags, newsw);
		TLBF_CTX_FLUSH(tlbf, hat, vaddr);
	}

	if (pgflags) {
		pgcnt_t	pgcnt;

		HAT_UNLOCK(hat);
		if (L1PT_PTE_IS_SPSECTION(newpte)) {
			pgcnt = mmu_btop(L1PT_SPSECTION_VSIZE);
		}
		else {
			ASSERT(L1PT_PTE_IS_SECTION(newpte));
			pgcnt = mmu_btop(L1PT_SECTION_VSIZE);
		}

		ASSERT(IS_P2ALIGNED(pp->p_pagenum, pgcnt));
		for (; pgcnt > 0; pgcnt--, pp++) {
			hat_page_setattr(pp, pgflags);
		}
		HAT_LOCK(hat);
	}
}

/*
 * static void
 * hat_l2pt_pteupdate(hat_t *hat, uintptr_t vaddr, l2pte_t *ptep,
 *		      l2pte_t *swflags, l2pte_t newpte, l2pte_t newsw,
 *		      page_t *pp, tlbf_ctx_t *tlbf)
 *	Update a L2PT entry.
 *
 * Calling/Exit State:
 *	The caller must hold hat lock.
 *	Note that hat_l2pt_pteupdate() may release hat lock.
 *	
 * Remarks:
 *	This function is used to change protection or software bits.
 *	Page frame number and pagesize can't be changed.
 */
static void
hat_l2pt_pteupdate(hat_t *hat, uintptr_t vaddr, l2pte_t *ptep,
		   l2pte_t *swflags, l2pte_t newpte, l2pte_t newsw,
		   page_t *pp, tlbf_ctx_t *tlbf)
{
	uint_t		pgflags = 0;
	l2pte_t		pte = *ptep;
	l2pte_t		sw = *swflags;
	boolean_t	dirty = B_FALSE;

	ASSERT(HAT_IS_LOCKED(hat));

	if (!(sw & (PTE_S_NOSYNC|PTE_S_NOCONSIST)) &&
	    L2PT_PTE_IS_REFMOD(pte) &&
	    (SWPTE_IS_NOSYNC(newsw) || !SWPTE_IS_WRITABLE(newsw) ||
	     L2PT_PTE_IS_REFMOD_CHANGED(pte, newpte))) {
		ASSERT(pp);
		ASSERT(hment_owned(pp));
		if (L2PT_PTE_IS_MOD(pte)) {
			pgflags |= P_MOD;
			dirty = B_TRUE;
		}
		if (L2PT_PTE_IS_REF(pte)) {
			pgflags |= P_REF;
		}
		L2PT_PTE_CLR_REFMOD(newpte);
	}

	if (!SWPTE_IS_WRITABLE(newsw)) {
		ASSERT(SWPTE_IS_READABLE(newsw));
		if (NEED_SOFTFAULT(newsw)) {
			L2PT_PTE_CLR_WRITABLE(newpte);
		}
		else {
			L2PT_PTE_SET_READONLY(newpte);
		}
	}
	else if (!NEED_SOFTFAULT(newsw)) {
		L2PT_PTE_SET_RW(newpte);
	}

	if (SWPTE_IS_EXECUTABLE(newsw)) {
		if (L2PT_PTE_IS_USER(newpte) && !SWPTE_IS_NOSYNC(newsw) &&
		    (newpte & L2PT_CACHED)) {
			/*
			 * Set up software exec fault for instruction cache
			 * maintenance.
			 */
			if (L2PT_PTE_IS_SMALL(newpte)) {
				if (dirty ||
				    !L2PT_PTE_SMALL_IS_EXECUTABLE(pte)) {
					L2PT_PTE_SMALL_CLR_EXECUTABLE(newpte);
				}
			}
			else if (dirty || !L2PT_PTE_LARGE_IS_EXECUTABLE(pte)) {
				L2PT_PTE_LARGE_CLR_EXECUTABLE(newpte);
			}
		}
		else {
			L2PT_PTE_SET_EXECUTABLE(newpte);
		}
	}
	else {
		L2PT_PTE_CLR_EXECUTABLE(newpte);
	}

	if (pte != newpte || sw != newsw) {
		if (L2PT_PTE_IS_SMALL(newpte)) {
			HAT_L2PTE_SET(ptep, newpte);
		}
		else {
			HAT_L2PTE_LARGE_SET(ptep, newpte);
		}
		*swflags = newsw;
		TLBF_CTX_FLUSH(tlbf, hat, vaddr);
	}

	if (pgflags) {
		pgcnt_t	pgcnt;

		HAT_UNLOCK(hat);
		if (L2PT_PTE_IS_SMALL(newpte)) {
			pgcnt = 1;
		}
		else {
			ASSERT(L2PT_PTE_IS_LARGE(newpte));
			pgcnt = mmu_btop(L2PT_LARGE_VSIZE);
		}

		ASSERT(IS_P2ALIGNED(pp->p_pagenum, pgcnt));
		for (; pgcnt > 0; pgcnt--, pp++) {
			hat_page_setattr(pp, pgflags);
		}
		HAT_LOCK(hat);
	}
}

/* Flags for hat_updateattr() */
typedef enum {
	HAT_ATTR_LOAD,		/* Apply given attribute set */
	HAT_ATTR_LOADPROT,	/* Apply protection bits in attribute set */
	HAT_ATTR_SET,		/* Grant given attributes */
	HAT_ATTR_CLEAR		/* Revoke given attributes */
} hat_updateattr_t;

/*
 * This is a part of hat_updateattr() for L1PT handling.
 */
static inline void
hat_l1pt_updateattr(hat_t *hat, uintptr_t vaddr, uint_t attr, l1pte_t *l1ptep,
		    hat_l2pt_t *swflags, page_t *pp, tlbf_ctx_t *tlbf,
		    hat_updateattr_t what)
{
	l1pte_t		pte, newpte;
	uint32_t	sw, newsw, chprot = 0;

	newsw = sw = HAT_L2PT_GET_SECTION_FLAGS(swflags);
	newpte = pte = *l1ptep;

	if (what == HAT_ATTR_LOADPROT) {
		chprot = 1;
		what = HAT_ATTR_LOAD;
	}

	if (what == HAT_ATTR_SET || what == HAT_ATTR_LOAD) {
		/* Set specified attributes. */
		if (attr & HAT_NOSYNC) {
			newsw |= PTE_S_NOSYNC;
		}
		if (attr & PROT_WRITE) {
			SWPTE_SET_WRITABLE(newsw);
		}
		if (attr & PROT_EXEC) {
			SWPTE_SET_EXECUTABLE(newsw);
		}

		if (what == HAT_ATTR_LOAD) {
			/* Clear unwanted attributes. */
			if (!(attr & HAT_NOSYNC)) {
				newsw &= ~PTE_S_NOSYNC;
			}
			if (!(attr & PROT_WRITE)) {
				SWPTE_CLR_WRITABLE(newsw);
			}
			if (!(attr & PROT_EXEC)) {
				SWPTE_CLR_EXECUTABLE(newsw);
			}
		}
	}
	else {
		ASSERT(what == HAT_ATTR_CLEAR);

		/* Clear specified attributes. */
		if (attr & HAT_NOSYNC) {
			newsw &= ~PTE_S_NOSYNC;
		}
		if (attr & PROT_WRITE) {
			SWPTE_CLR_WRITABLE(newsw);
		}
		if (attr & PROT_EXEC) {
			SWPTE_CLR_EXECUTABLE(newsw);
		}
	}

	if (chprot) {
		/* Load protection bits only. */
		newsw = (sw & ~PTE_S_PROT_MASK)|(newsw & PTE_S_PROT_MASK);
	}

	/* Update L1PT entry. */
	hat_l1pt_pteupdate(hat, vaddr, l1ptep, swflags, newpte, newsw,
			   pp, tlbf);
}

/*
 * This is a part of hat_updateattr() for L2PT handling.
 */
static inline void
hat_l2pt_updateattr(hat_t *hat, uintptr_t vaddr, uint_t attr, l2pte_t *ptep,
		    page_t *pp, tlbf_ctx_t *tlbf, hat_updateattr_t what)
{
	l2pte_t		pte, *swptep, newpte;
	uint32_t	sw, newsw, chprot = 0;

	swptep = L2PT_SOFTFLAGS(ptep);
	newsw = sw = *swptep;
	newpte = pte = *ptep;

	if (what == HAT_ATTR_LOADPROT) {
		chprot = 1;
		what = HAT_ATTR_LOAD;
	}

	if (what == HAT_ATTR_SET || what == HAT_ATTR_LOAD) {
		/* Set specified attributes. */
		if (attr & HAT_NOSYNC) {
			newsw |= PTE_S_NOSYNC;
		}
		if (attr & PROT_WRITE) {
			SWPTE_SET_WRITABLE(newsw);
		}
		if (attr & PROT_EXEC) {
			SWPTE_SET_EXECUTABLE(newsw);
		}

		if (what == HAT_ATTR_LOAD) {
			/* Clear unwanted attributes. */
			if (!(attr & HAT_NOSYNC)) {
				newsw &= ~PTE_S_NOSYNC;
			}
			if (!(attr & PROT_WRITE)) {
				SWPTE_CLR_WRITABLE(newsw);
			}
			if (!(attr & PROT_EXEC)) {
				SWPTE_CLR_EXECUTABLE(newsw);
			}
		}
	}
	else {
		ASSERT(what == HAT_ATTR_CLEAR);

		/* Clear specified attributes. */
		if (attr & HAT_NOSYNC) {
			newsw &= ~PTE_S_NOSYNC;
		}
		if (attr & PROT_WRITE) {
			SWPTE_CLR_WRITABLE(newsw);
		}
		if (attr & PROT_EXEC) {
			SWPTE_CLR_EXECUTABLE(newsw);
		}
	}

	if (chprot) {
		/* Load protection bits only. */
		newsw = (sw & ~PTE_S_PROT_MASK)|(newsw & PTE_S_PROT_MASK);
	}

	/* Update L2PT entry. */
	hat_l2pt_pteupdate(hat, vaddr, ptep, swptep, newpte, newsw,
			   pp, tlbf);
}

/*
 * static void
 * hat_updateattr(hat_t *hat, caddr_t addr, size_t len, uint_t attr,
 *	       hat_updateattr_t what)
 *	Common code to update PTE attributes.
 *
 * Remarks:
 *	hat_updateattr() doesn't treat read permission.
 */
static void
hat_updateattr(hat_t *hat, caddr_t addr, size_t len, uint_t attr,
	       hat_updateattr_t what)
{
	uintptr_t	vaddr = (uintptr_t)addr, endaddr;
	tlbf_ctx_t	tlbf;

	ASSERT(IS_PAGEALIGNED(vaddr));
	ASSERT(IS_PAGEALIGNED(len));
	ASSERT(HAT_IS_KERNEL(hat) ||
	       AS_LOCK_HELD(hat->hat_as, &hat->hat_as->a_lock));

	endaddr = vaddr + len;
	TLBF_CTX_INIT(&tlbf, 0);
	HAT_LOCK(hat);

	while (vaddr < endaddr) {
		uintptr_t	nextaddr;
		l1pte_t		*l1ptep;
		l2pte_t		*ptep;
		hat_l2pt_t	*l2pt;
		ssize_t		pgsz;
		page_t		*pp, *lkpp = NULL;
		pfn_t		pfn;

	tryagain:
		nextaddr = vaddr;
		pgsz = hatpt_pte_walk(hat, &nextaddr, &l1ptep, &ptep, &l2pt,
				      &pfn, &pp);
		if (pgsz == -1) {
			pp = lkpp;
			goto nextloop;
		}
		if (l2pt == NULL) {
			/* Can't change attributes for static kernel page. */
			pp = lkpp;
			goto nextloop;
		}

		if (pp != NULL) {
			if (lkpp == NULL) {
				if (!hat_hment_tryenter(pp, hat)) {
					/*
					 * Check again because we lost
					 * the race.
					 */
					lkpp = pp;
					goto tryagain;
				}
			}
			else if (pp != lkpp) {
				/*
				 * Check again because the mapping has been
				 * changed.
				 */
				hment_exit(lkpp);
				lkpp = NULL;
				goto tryagain;
			}
		}
		else if (lkpp != NULL) {
			hment_exit(lkpp);
		}

		if (pgsz >= L1PT_SECTION_VSIZE) {
			uintptr_t	va = nextaddr - pgsz;

			hat_l1pt_updateattr(hat, va, attr, l1ptep, l2pt, pp,
					    &tlbf, what);
			HATPT_KAS_SYNC_L(va, pgsz, HAT_IS_KERNEL(hat));
		}
		else {
			ASSERT(ptep);
			hat_l2pt_updateattr(hat, nextaddr - pgsz, attr, ptep,
					    pp, &tlbf, what);
		}

	nextloop:
		if (pp != NULL) {
			hment_exit(pp);
		}

		if (nextaddr < vaddr) {
			break;
		}
		vaddr = nextaddr;
	}

	TLBF_CTX_FINI(&tlbf);
	HAT_UNLOCK(hat);
}

/*
 * The set of L1PTE bits for pfn, permission and caching that require a
 * TLB flush if changed on a HAT_LOAD_REMAP.
 */
#define	L1PT_SECTION_REMAP_BITS						\
	(L1PT_SECTION_ADDRMASK|L1PT_APX|L1PT_AP_MASK|L1PT_TEX_MASK)
#define	L1PT_SPSECTION_REMAP_BITS					\
	(L1PT_SPSECTION_ADDRMASK|L1PT_APX|L1PT_AP_MASK|L1PT_TEX_MASK)

#define	REMAP_L1PT_ASSERT(ex)					\
	do {							\
		if (!(ex)) {					\
			panic("hat_l1pt_pteload: " #ex);	\
		}						\
	} while (0)

/*
 * static void
 * hat_l1pt_pteload(hat_t *hat, uintptr_t vaddr, pfn_t pfn, uint32_t szc,
 *		    page_t *pp, uint_t attr, uint_t flags, hat_l2pt_t *swflags)
 *	Load translation for one page frame.
 *	This function is used to load L1PT entry.
 *	swflags is used as software PTE flags.
 *
 * Calling/Exit State:
 *	The caller must hold hat lock, and this function releases it.
 *	If pp is not null, the caller must hold hment lock for the page.
 */
static void
hat_l1pt_pteload(hat_t *hat, uintptr_t vaddr, pfn_t pfn, uint32_t szc,
		 page_t *pp, uint_t attr, uint_t flags, hat_l2pt_t *swflags,
		 tlbf_ctx_t *tlbf)
{
	hment_t	*hm = NULL;
	int	visible;
	l1pte_t	*l1ptep, pte, *p, *eptep, sw;
	uint_t	entry, new, remap, samepfn = 0;
	uint32_t	type, otype;

	ASSERT(HAT_IS_LOCKED(hat));

	/* Check whether this is a consistant mapping (visible map). */
	visible = IS_VISIBLE_MAP(pp, flags);
	ASSERT((visible && hment_owned(pp)) ||
	       (!visible && (pp == NULL || !hment_owned(pp))));
	ASSERT(!(flags & HAT_NO_KALLOC) || pp->p_mapping == NULL);
	LPG_DISABLE_ASSERT(!visible);

	entry = L1PT_INDEX(vaddr);
	if (LPG_EVAL(visible)) {
		hm = hment_prepare(hat, hat, szc, entry, pp);
		ASSERT(!(flags & HAT_NO_KALLOC) || hm == NULL);
	}

	l1ptep = hat->hat_l1vaddr + entry;
	pte = hat_l1pt_mkpte(pfn, attr, flags, szc, &sw);
	if (szc == SZC_SPSECTION) {
		eptep = l1ptep + L1PT_SPSECTION_NPTES;
	}
	else {
		ASSERT(szc == SZC_SECTION);
		eptep = l1ptep + 1;
	}

	for (new = 0, remap = 0, p = l1ptep; p < eptep; p++) {
		l1pte_t	opte = *p;
		pfn_t	opfn;

		if (pte == opte && sw == HAT_L2PT_GET_SECTION_FLAGS(swflags)) {
			continue;
		}
		if (L1PT_PTE_IS_INVALID(opte)) {
			new++;
			continue;
		}

		/*
		 * Attempting to change existing mapping.
		 * - HAT_LOAD_REMAP must be specified if changing the pfn.
		 *   We also require that NOCONSIST be specified.
		 * - Otherwise only permission or caching bis may change.
		 */
		remap++;
		REMAP_L1PT_ASSERT(L1PT_PTE_TYPE(opte) == L1PT_TYPE_SECTION);
		opfn = HAT_L1PTE2PFN(opte);
		if (pfn != opfn) {
			REMAP_L1PT_ASSERT(flags & HAT_LOAD_REMAP);
			REMAP_L1PT_ASSERT(flags & HAT_LOAD_NOCONSIST);
			REMAP_L1PT_ASSERT(pf_is_memory(pfn) ==
					  pf_is_memory(opfn));
			REMAP_L1PT_ASSERT(!visible);
		}
		else {
			samepfn++;
		}

		if (L1PT_PTE_IS_SECTION(opte)) {
			ASSERT((opte & ~L1PT_SECTION_REMAP_BITS) ==
			       (pte & ~L1PT_SECTION_REMAP_BITS));
		}
		else {
			ASSERT(L1PT_PTE_IS_SPSECTION(opte));
			ASSERT((opte & ~L1PT_SPSECTION_REMAP_BITS) ==
			       (pte & ~L1PT_SPSECTION_REMAP_BITS));
		}
	}
	if (remap == 0) {
		if (new) {
			REMAP_L1PT_ASSERT(new == (eptep - l1ptep));
			PGCNT_INC(hat, szc);

			if (LPG_EVAL(visible) && (attr & PROT_USER) &&
			    (pte & L1PT_CACHED) && SWPTE_IS_EXECUTABLE(sw)) {
				/*
				 * Turn off hardware executable bit for
				 * instruction cache maintenance.
				 */
				L1PT_PTE_CLR_EXECUTABLE(pte);
			}
		}
		else {
			/* Nothing to do. */
			goto done;
		}
	}
	else {
		REMAP_L1PT_ASSERT(remap == (eptep - l1ptep));

		if (samepfn) {
			/*
			 * Let hat_l1pt_pteupdate() do the rest.
			 * This is required to sync ref/mod bit in old PTE
			 * to the page.
			 */
			ASSERT(samepfn == remap);
			hat_l1pt_pteupdate(hat, vaddr, l1ptep, swflags,
					   pte, sw, pp, tlbf);
			goto done;
		}
	}

	/* Install new PTEs. */
	if (szc == SZC_SPSECTION) {
		HAT_L1PTE_SPSECTION_SET(l1ptep, pte);
	}
	else {
		HAT_L1PTE_SET(l1ptep, pte);
	}
	HAT_L2PT_SET_SECTION_FLAGS(swflags, sw);

	if (new) {
		HAT_UNLOCK(hat);
		if (LPG_EVAL(visible)) {
			hment_assign(hat, szc, entry, pp, hm);
			hment_exit(pp);
		}
		else {
			ASSERT(flags & HAT_LOAD_NOCONSIST);
		}
		return;
	}

	ASSERT(remap);
	TLBF_CTX_FLUSH(tlbf, hat, vaddr);

 done:
	HAT_UNLOCK(hat);
	if (LPG_EVAL(visible)) {
		hment_exit(pp);
		if (hm != NULL) {
			hment_free(hm);
		}
	}
}

/*
 * The set of L2PTE bits for pfn, permission and caching that require a
 * TLB flush if changed on a HAT_LOAD_REMAP.
 */
#define	L2PT_SMALL_REMAP_BITS						\
	(L2PT_SMALL_ADDRMASK|L2PT_APX|L2PT_AP_MASK|L2PT_SMALL_TEX_MASK|	\
	 L2PT_SMALL_XN)
#define	L2PT_LARGE_REMAP_BITS						\
	(L2PT_LARGE_ADDRMASK|L2PT_APX|L2PT_AP_MASK|L2PT_LARGE_TEX_MASK|	\
	 L2PT_LARGE_XN)

#define	REMAP_L2PT_ASSERT(ex)					\
	do {							\
		if (!(ex)) {					\
			panic("hat_l2pt_pteload: " #ex);	\
		}						\
	} while (0)

/*
 * static void
 * hat_l2pt_pteload(hat_t *hat, hat_l2pt_t *l2pt, uintptr_t vaddr, pfn_t pfn,
 *		    uint32_t szc, page_t *pp, uint_t attr, uint_t flags)
 *	Load translation for one page frame.
 *	This function is used to load L2PT entry.
 *
 * Calling/Exit State:
 *	The caller must hold hat lock, and this function releases it.
 *	If pp is not null, the caller must hold hment lock for the page.
 */
static void
hat_l2pt_pteload(hat_t *hat, hat_l2pt_t *l2pt, uintptr_t vaddr, pfn_t pfn,
		 uint32_t szc, page_t *pp, uint_t attr, uint_t flags,
		 tlbf_ctx_t *tlbf)
{
	l2pte_t	*ptep, pte, *p, *eptep, *swptep, sw;
	hment_t	*hm = NULL;
	int	visible;
	uint_t	entry, new, remap, samepfn = 0;
	pfn_t	opfn;
	uint32_t	type, otype;

	ASSERT(HAT_IS_LOCKED(hat));
	ASSERT(HAT_L2PT_IS_HELD(l2pt));

	/* Check whether this is a consistant mapping (visible map). */
	visible = IS_VISIBLE_MAP(pp, flags);
	ASSERT((visible && hment_owned(pp)) ||
	       (!visible && (pp == NULL || !hment_owned(pp))));
	ASSERT(!(flags & HAT_NO_KALLOC) || pp->p_mapping == NULL);

	entry = L2PT_INDEX(vaddr);
	if (visible) {
		SZC_ASSERT(szc);
		hm = hment_prepare(hat, l2pt, szc, entry, pp);
		ASSERT(!(flags & HAT_NO_KALLOC) || hm == NULL);
	}

	ptep = l2pt->l2_vaddr + entry;
	swptep = L2PT_SOFTFLAGS(ptep);
	pte = hat_l2pt_mkpte(pfn, attr, flags, szc, &sw);
	if (szc == SZC_LARGE) {
		eptep = ptep + L2PT_LARGE_NPTES;
	}
	else {
		ASSERT(szc == 0);
		eptep = ptep + 1;
	}

	for (new = 0, remap = 0, p = ptep; p < eptep; p++) {
		l2pte_t	opte = *p;
		pfn_t	opfn;

		if (pte == opte && sw == *swptep) {
			continue;
		}
		if (L2PT_PTE_IS_INVALID(opte)) {
			new++;
			continue;
		}

		/*
		 * Attempting to change existing mapping.
		 * - HAT_LOAD_REMAP must be specified if changing the pfn.
		 *   We also require that NOCONSIST be specified.
		 * - Otherwise only permission or caching bis may change.
		 */
		remap++;
		opfn = HAT_L2PTE2PFN(opte);
		if (pfn != opfn) {
			REMAP_L2PT_ASSERT(flags & HAT_LOAD_REMAP);
			REMAP_L2PT_ASSERT(flags & HAT_LOAD_NOCONSIST);
			REMAP_L2PT_ASSERT(pf_is_memory(pfn) ==
					  pf_is_memory(opfn));
			REMAP_L2PT_ASSERT(!visible);
		}
		else {
			samepfn++;
		}

		if (L2PT_PTE_IS_SMALL(opte)) {
			ASSERT((opte & ~L2PT_SMALL_REMAP_BITS) ==
			       (pte & ~L2PT_SMALL_REMAP_BITS));
		}
		else {
			ASSERT(L2PT_PTE_IS_LARGE(opte));
			ASSERT((opte & ~L2PT_LARGE_REMAP_BITS) ==
			       (pte & ~L2PT_LARGE_REMAP_BITS));
		}
	}

	if ((flags & HAT_LOAD_LOCK) && !HAT_IS_KERNEL(hat)) {
		/*
		 * Lock this L2PT.
		 * No need to lock kernel L2PT because it is always locked.
		 */
		HAT_L2PT_HOLD(l2pt);
	}
	if (remap == 0) {
		if (new) {
			int	nptes = eptep - ptep;

			REMAP_L2PT_ASSERT(new == nptes);
			HAT_L2PT_COUNT(l2pt, nptes);
			PGCNT_INC(hat, szc);

			if (visible && (attr & PROT_USER) &&
			    (pte & L2PT_CACHED) && SWPTE_IS_EXECUTABLE(sw)) {
				/*
				 * Turn off hardware executable bit for
				 * instruction cache maintenance.
				 */
				if (SZC_EVAL(szc) == SZC_LARGE) {
					L2PT_PTE_LARGE_CLR_EXECUTABLE(pte);
				}
				else {
					L2PT_PTE_SMALL_CLR_EXECUTABLE(pte);
				}
			}
		}
		else {
			/* Nothing to do. */
			HAT_L2PT_RELE(l2pt);
			goto done;
		}
	}
	else {
		REMAP_L2PT_ASSERT(remap == (eptep - ptep));

		if (samepfn) {
			/*
			 * Let hat_l2pt_pteupdate() do the rest.
			 * This is required to sync ref/mod bit in old PTE
			 * to the page.
			 */
			ASSERT(samepfn == remap);
			hat_l2pt_pteupdate(hat, vaddr, ptep, swptep, pte,
					   sw, pp, tlbf);
			HAT_L2PT_RELE(l2pt);
			goto done;
		}
	}
	HAT_L2PT_RELE(l2pt);

	/* Install new PTEs. */
	if (szc == SZC_LARGE) {
		HAT_L2PTE_LARGE_SET(ptep, pte);
	}
	else {
		HAT_L2PTE_SET(ptep, pte);
	}
	*swptep = sw;

	if (new) {
		HAT_UNLOCK(hat);
		if (visible) {
			hment_assign(l2pt, SZC_EVAL(szc), entry, pp, hm);
			hment_exit(pp);
		}
		else {
			ASSERT(flags & HAT_LOAD_NOCONSIST);
		}
		return;
	}

	ASSERT(remap);
	TLBF_CTX_FLUSH(tlbf, hat, vaddr);

 done:
	HAT_UNLOCK(hat);
	if (visible) {
		hment_exit(pp);
		if (hm != NULL) {
			hment_free(hm);
		}
	}
}

/*
 * static boolean_t
 * hat_l1pt_unload(hat_t *hat, l1pte_t *l1ptepp, uint16_t l1idx,
 *		   uintptr_t *vaddrp, uint_t flags, tlbf_ctx_t *tlbf,
 *		   boolean_t *coarse)
 *	Unload L1PT entry.
 *	This function is used to unmap section mapping.
 *
 * Calling/Exit State:
 *	Returns B_TRUE if the specified PTE is actually unloaded.
 *
 *	If the specified L1PT entry has link to L2PT, B_TRUE is set to
 *	*coarse. Otherwise, B_FALSE is set to *coarse, and virtual address
 *	to be unmapped in next loop is set to *vaddrp.
 *
 *	The caller must hold hat lock.
 *	Note that hat_l1pt_unload() may release hat lock.
 *
 * Remarks:
 *	hat_l1pt_unload() remove mappings even if it is locked unless
 *	HAT_PLAT_UNLOAD_NOLOCK is specified to "flags".
 */
static boolean_t
hat_l1pt_unload(hat_t *hat, l1pte_t *l1ptep, uint16_t l1idx, uintptr_t *vaddrp,
		uint_t flags, tlbf_ctx_t *tlbf, boolean_t *coarse)
{
	hat_l2bundle_t	*l2b;
	hat_l2pt_t	*swflags;
	l1pte_t		pte, sw;
	uintptr_t	vaddr = *vaddrp;
	uint32_t	type;
	page_t		*pp, *lkpp = NULL;
	pgcnt_t		pgcnt = 0;
	uint16_t	szc;
	hment_t		*hm = NULL;
	pfn_t		pfn;
	int		unlock;
	boolean_t	unloaded;

 tryagain:
	ASSERT(HAT_IS_LOCKED(hat));
	pte = *l1ptep;
	type = L1PT_PTE_TYPE(pte);
	if (type == L1PT_TYPE_COARSE) {
		/*
		 * This entry keeps link to L2PT.
		 * This mapping should be unmapped by hat_l2pt_unload().
		 */
		*coarse = B_TRUE;
		if (lkpp != NULL) {
			hment_exit(lkpp);
		}
		return B_FALSE;
	}

	*coarse = B_FALSE;
	if (type == L1PT_TYPE_INVALID) {
		/* Invalid mapping. Nothing to do. */
		ASSERT(pte == 0);	/* INVALID type is not used. */
		*vaddrp = L1PT_NEXT_VADDR(vaddr);
		if (lkpp != NULL) {
			hment_exit(lkpp);
		}
		return B_FALSE;
	}

	unloaded = B_FALSE;
	l2b = hat->hat_l2bundle[HAT_L2BD_INDEX(hat, l1idx)];
	ASSERT(l2b);
	swflags = &l2b->l2b_l2pt[HAT_L2BD_L2PT_INDEX(l1idx)];
	sw = HAT_L2PT_GET_SECTION_FLAGS(swflags);
	pp = NULL;

	/* Check whether mapping list exists. */
	pfn = HAT_L1PTE2PFN(pte);

	if (!SWPTE_IS_NOCONSIST(sw)) {
		pp = page_numtopp_nolock(pfn);
		if (pp != NULL) {
			if (lkpp == NULL) {
				if (!hat_hment_tryenter(pp, hat)) {
					/*
					 * Check PTE again because hat lock
					 * has been released.
					 */
					lkpp = pp;
					goto tryagain;
				}
			}
			else if (pp != lkpp) {
				/*
				 * Check again because the mapping has been
				 * changed.
				 */
				hment_exit(lkpp);
				lkpp = NULL;
				goto tryagain;
			}
		}
		else {
			panic("no page structure on visible mapping: "
			      "pte = 0x%p:0x%08x swflags:0x%p:0x%08x",
			      l1ptep, pte, swflags, sw);
		}
	}
	else if (lkpp != NULL) {
		hment_exit(lkpp);
	}

	unlock = (!(flags & HAT_PLAT_UNLOAD_NOLOCK) ||
		  (hat->hat_flags & HAT_FREEING));

	/*
	 * If HAT_FREEING is set in hat_flags, skip invalidation because
	 * this hat is being destroyed.
	 *
	 * And we don't unload locked mapping.
	 */
	if (L1PT_PTE_IS_SPSECTION(pte)) {
		if (!L1PT_SPSECTION_ALIGNED(vaddr)) {
			panic("Unmap inside supersection: hat=0x%p "
			      "vaddr=0x%08lx ptep=0x%p", hat, vaddr, l1ptep);
		}
		if (unlock || !SWPTE_IS_LOCKED(sw)) {
			if (!(hat->hat_flags & HAT_FREEING)) {
				HAT_L1PTE_SPSECTION_SET(l1ptep, 0);
			}
			unloaded = B_TRUE;
			HAT_L2PT_RELE(swflags);
			PGCNT_DEC(hat, SZC_SPSECTION);
			pgcnt = mmu_btop(L1PT_SPSECTION_VSIZE);
		}
		*vaddrp = vaddr + L1PT_SPSECTION_VSIZE;
		szc = SZC_SPSECTION;
	}
	else {
		ASSERT(L1PT_PTE_IS_SECTION(pte));
		if (!L1PT_SECTION_ALIGNED(vaddr)) {
			panic("Unmap inside section: hat=0x%p vaddr=0x%08lx "
			      "ptep=0x%p", hat, vaddr, l1ptep);
		}
		if (unlock || !SWPTE_IS_LOCKED(sw)) {
			if (!(hat->hat_flags & HAT_FREEING)) {
				HAT_L1PTE_SET(l1ptep, 0);
			}
			unloaded = B_TRUE;
			HAT_L2PT_RELE(swflags);
			PGCNT_DEC(hat, SZC_SECTION);
			pgcnt = mmu_btop(L1PT_SECTION_VSIZE);
		}
		*vaddrp = vaddr + L1PT_SECTION_VSIZE;
		szc = SZC_SECTION;
	}

	if (unloaded) {
		TLBF_CTX_FLUSH(tlbf, hat, vaddr);
	}
	HAT_UNLOCK(hat);

	if (pp != NULL) {
		if (unloaded) {
			if (!(flags & HAT_UNLOAD_NOSYNC) &&
			    !SWPTE_IS_NOSYNC(sw)) {
				L1PT_PTE_REFMOD_SYNC(pte, sw);
				hat_ptesync(pp, sw, pgcnt);
			}
			hm = hment_remove(pp, hat, szc, l1idx);
			hment_exit(pp);
			if (hm != NULL) {
				hment_free(hm);
			}
		}
		else {
			hment_exit(pp);
		}
	}

	HAT_LOCK(hat);
	return unloaded;
}

/*
 * static boolean_t
 * hat_l2pt_unload(hat_t *hat, hat_l2pt_t *l2pt, uintptr_t *vaddrp,
 *		   uint_t flags, tlbf_ctx_t *tlbf)
 *	Unload L2PT entry.
 *
 * Calling/Exit State:
 *	Return B_TRUE if the specified PTE is actually unloaded.
 *
 *	The caller must hold hat lock.
 *	Note that hat_l2pt_unload() may release hat lock.
 *
 *	This function doesn't handle releasing of L2PT.
 *
 * Remarks:
 *	hat_l2pt_unload() remove mappings even if it is locked unless
 *	HAT_PLAT_UNLOAD_NOLOCK is specified to "flags".
 */
static boolean_t
hat_l2pt_unload(hat_t *hat, hat_l2pt_t *l2pt, uintptr_t *vaddrp, uint_t flags,
		tlbf_ctx_t *tlbf)
{
	l2pte_t		*ptep, pte, *swptep, sw;
	uintptr_t	vaddr = *vaddrp;
	pfn_t		pfn;
	page_t		*pp, *lkpp = NULL;
	uint_t		entry;
	pgcnt_t		pgcnt = 0;
	uint16_t	szc;
	hment_t		*hm = NULL;
	int		unlock;
	boolean_t	unloaded;

	ASSERT(l2pt->l2_vaddr != NULL);
	HAT_L2PT_HOLD(l2pt);

 tryagain:
	ASSERT(HAT_IS_LOCKED(hat));
	entry = L2PT_INDEX(vaddr);
	if (l2pt->l2_vaddr == NULL) {
		goto nopte;
	}
	ptep = l2pt->l2_vaddr + entry;
	pte = *ptep;

	if (L2PT_PTE_IS_INVALID(pte)) {
	nopte:
		*vaddrp = vaddr + MMU_PAGESIZE;
		if (lkpp != NULL) {
			hment_exit(lkpp);
		}
		HAT_L2PT_RELE(l2pt);
		return B_FALSE;
	}

	unloaded = B_FALSE;
	swptep = L2PT_SOFTFLAGS(ptep);
	sw = *swptep;
	pp = NULL;

	/* Check whether mapping list exists. */
	pfn = HAT_L2PTE2PFN(pte);
	ASSERT(pfn != PFN_INVALID);

	if (!SWPTE_IS_NOCONSIST(sw)) {
		pp = page_numtopp_nolock(pfn);
		if (pp != NULL) {
			if (lkpp == NULL) {
				if (!hat_hment_tryenter(pp, hat)) {
					/*
					 * Check PTE again because hat lock
					 * has been released.
					 */
					lkpp = pp;
					goto tryagain;
				}
			}
			else if (pp != lkpp) {
				/*
				 * Check again because the mapping has been
				 * changed.
				 */
				hment_exit(lkpp);
				lkpp = NULL;
				goto tryagain;
			}
		}
		else {
			panic("no page structure on visible mapping: "
			      "pte = 0x%p:0x%08x swptep:0x%p:0x%08x",
			      ptep, pte, swptep, sw);
		}
	}
	else if (lkpp != NULL) {
		hment_exit(lkpp);
	}

	if ((flags & HAT_UNLOAD_UNLOCK) && !HAT_IS_KERNEL(hat)) {
		/* Unlock L2PT. */
		HAT_L2PT_RELE(l2pt);
	}
	unlock = (!(flags & HAT_PLAT_UNLOAD_NOLOCK) ||
		  (hat->hat_flags & HAT_FREEING));

	/*
	 * If HAT_FREEING is set in hat_flags, skip invalidation because
	 * this hat is being destroyed.
	 *
	 * And we don't unload locked mapping.
	 */
	if (L2PT_PTE_IS_LARGE(pte)) {
		if (!L2PT_LARGE_ALIGNED(vaddr)) {
			panic("Unmap inside large page: hat=0x%p vaddr=0x%08lx"
			      " ptep=0x%p", hat, vaddr, ptep);
		}
		if (unlock || !SWPTE_IS_LOCKED(sw)) {
			if (!(hat->hat_flags & HAT_FREEING)) {
				HAT_L2PTE_LARGE_SET(ptep, 0);
				*swptep = 0;
			}
			unloaded = B_TRUE;
			PGCNT_DEC(hat, SZC_LARGE);
			ASSERT(l2pt->l2_nptes > 0);
			HAT_L2PT_COUNT(l2pt, -L2PT_LARGE_NPTES);
			pgcnt = L2PT_LARGE_NPTES;
		}
		*vaddrp = vaddr + L2PT_LARGE_VSIZE;
		szc = SZC_LARGE;
	}
	else {
		ASSERT(L2PT_PTE_IS_SMALL(pte));
		ASSERT(IS_PAGEALIGNED(vaddr));
		if (unlock || !SWPTE_IS_LOCKED(sw)) {
			if (!(hat->hat_flags & HAT_FREEING)) {
				HAT_L2PTE_SET(ptep, 0);
				*swptep = 0;
			}
			unloaded = B_TRUE;
			PGCNT_DEC(hat, 0);
			ASSERT(l2pt->l2_nptes > 0);
			HAT_L2PT_DEC(l2pt);
			pgcnt = 1;
		}
		*vaddrp = vaddr + MMU_PAGESIZE;
		szc = SZC_SMALL;
	}

	if (unloaded) {
		TLBF_CTX_FLUSH(tlbf, hat, vaddr);
	}
	HAT_UNLOCK(hat);

	if (pp != NULL) {
		if (unloaded) {
			if (!(flags & HAT_UNLOAD_NOSYNC) &&
			    !SWPTE_IS_NOSYNC(sw)) {
				L2PT_PTE_REFMOD_SYNC(pte, sw);
				hat_ptesync(pp, sw, pgcnt);
			}
			hm = hment_remove(pp, l2pt, szc, entry);
			hment_exit(pp);
			if (hm != NULL) {
				hment_free(hm);
			}
		}
		else {
			hment_exit(pp);
		}
	}

	HAT_LOCK(hat);
	HAT_L2PT_RELE(l2pt);
	return unloaded;
}

/*
 * static void
 * hat_kmap_memload(hat_t *hat, caddr_t addr, page_t *pp, uint_t attr,
 *		    uint_t flags)
 *	Load a translation to the address range specified by hat_kmap_init().
 */
static void
hat_kmap_memload(hat_t *hat, caddr_t addr, page_t *pp, uint_t attr,
		 uint_t flags)
{
	hat_l2pt_t	*l2pt;
	uintptr_t	vaddr = (uintptr_t)addr;
	tlbf_ctx_t	tlbf;

	ASSERT(HAT_IS_KERNEL(hat));

	TLBF_CTX_INIT(&tlbf, 0);
	hment_enter(pp);
	HAT_LOCK(hat);
	l2pt = hatpt_l2pt_lookup(hat, vaddr);
	ASSERT(l2pt);
	HAT_L2PT_HOLD(l2pt);

	ASSERT(pp->p_szc == 0);
	attr &= ~PROT_USER;

	/*
	 * Turn off HAT_LOAD_LOCK flag to activate ref/mod emulation fault.
	 * It's harmless even F_SOFTLOCK because ref/mod emulation fault
	 * never causes I/O wait.
	 */
	flags &= ~HAT_LOAD_LOCK;

	hat_l2pt_pteload(hat, l2pt, vaddr, pp->p_pagenum, 0, pp, attr, flags,
			 &tlbf);

	/* hat_l2pt_pteload() releases hat and hment lock. */
	ASSERT(!HAT_KAS_IS_LOCKED());
	ASSERT(!hment_owned(pp));
	TLBF_CTX_FINI(&tlbf);
}

/*
 * static void
 * hat_kmap_unload(hat_t *hat, caddr_t addr, size_t len, uint_t flags)
 *	Unload translation for kmap address space.
 */
static void
hat_kmap_unload(hat_t *hat, caddr_t addr, size_t len, uint_t flags)
{
	uintptr_t	vaddr = (uintptr_t)addr, eaddr;
	hat_l2pt_t	*l2pt;
	int		l2idx;
	tlbf_ctx_t	tlbf;

	ASSERT(HAT_IS_KERNEL(hat));

	TLBF_CTX_INIT(&tlbf, 0);
	eaddr = vaddr + len;
	HAT_LOCK(hat);
	l2pt = hatpt_l2pt_lookup(hat, vaddr);
	l2idx = L2PT_INDEX(vaddr);
	ASSERT(l2pt);
	while (vaddr < eaddr) {
		uintptr_t	nextaddr = vaddr;

		(void)hat_l2pt_unload(hat, l2pt, &nextaddr, flags, &tlbf);
		ASSERT(nextaddr == vaddr + MMU_PAGESIZE);

		l2idx++;
		vaddr = nextaddr;
		if (l2idx >= L2PT_NPTES) {
			l2pt = hatpt_l2pt_lookup(hat, vaddr);
			ASSERT(l2pt);
			ASSERT(L2PT_INDEX(vaddr) == 0);
			l2idx = 0;
		}
	}
	TLBF_CTX_FINI(&tlbf);
	HAT_UNLOCK(hat);
}

#define	VALID_MEMLOAD_FLAGS						\
	(HAT_LOAD|HAT_LOAD_LOCK|HAT_LOAD_ADV|HAT_LOAD_NOCONSIST|	\
	 HAT_NO_KALLOC|HAT_LOAD_REMAP|HAT_LOAD_TEXT)

/*
 * void
 * hat_memload(hat_t *hat, caddr_t addr, page_t *pp, uint_t attr, uint_t flags)
 *	Load a translation to the given page.
 *
 * Flags for hat_memload()/hat_devload()/hat_*attr():
 *	HAT_LOAD	Default flags to load a translation to the page.
 *
 *	HAT_LOAD_LOCK	Lock donw mapping resources.
 *
 *	HAT_LOAD_NOCONSIST
 *			Do not add mapping to page_t mapping list.
 *
 *	HAT_LOAD_SHARE	Currently not supported.
 *
 *	HAT_LOAD_REMAP	Reload a valid PTE with a different page frame.
 *
 *	HAT_NO_KALLOC	Do not kmem_alloc() while creating the mapping.
 *			Currently this flag is used to allocate hment,
 *			user L2PT and L2 bundle.
 *
 * Protection attribute:
 *	HAT_NOSYNC	The ref/mod bits for this mapping are never cleared.
 *
 * Remarks:
 *	hat_memload() updates only kernel master L1PT if the kernel space is
 *	specified.
 */
void
hat_memload(hat_t *hat, caddr_t addr, page_t *pp, uint_t attr, uint_t flags)
{
	uintptr_t	vaddr = (uintptr_t)addr;
	hat_l2pt_t	*l2pt;
	tlbf_ctx_t	tlbf;
	boolean_t	l1loaded;

	ASSERT(IS_PAGEALIGNED(addr));
	ASSERT(HAT_IS_KERNEL(hat) || vaddr < USERLIMIT);
	ASSERT(HAT_IS_KERNEL(hat) ||
	       AS_LOCK_HELD(hat->hat_as, &hat->hat_as->a_lock));
	ASSERT((flags & VALID_MEMLOAD_FLAGS) == flags);
	ASSERT(!PP_ISFREE(pp));

	/*
	 * We always set HAT_STORECACHING_OK because hat_memload() is used
	 * to map normal memory.
	 */
	attr |= HAT_STORECACHING_OK;

	/*
	 * Check whether we must use special attribute for the mapping.
	 */
	hat_plat_memattr(pp->p_pagenum, &attr);

	/* Use hat_kmap_memload() for request to map kmap address. */
	if (hat_kmap_start <= vaddr && vaddr < hat_kmap_end) {
		hat_kmap_memload(hat, addr, pp, attr, flags);
		return;
	}

	TLBF_CTX_INIT(&tlbf, 0);

	if (vaddr >= KERNELBASE) {
		ASSERT(HAT_IS_KERNEL(hat));

		attr &= ~PROT_USER;
	}
	else {
		ASSERT(!HAT_IS_KERNEL(hat));
		ASSERT(attr & PROT_USER);
	}

	HAT_LOCK(hat);

	/* Prepare L2PT for this mapping. */
	l2pt = hatpt_l2pt_prepare(hat, vaddr, &l1loaded);

	/*
	 * Need to sync master kernel L1PT changes into all user
	 * hat structures.
	 */
	HATPT_KAS_SYNC_L(vaddr, MMU_PAGESIZE, l1loaded && HAT_IS_KERNEL(hat));

	if ((flags & HAT_LOAD_NOCONSIST) == 0) {
		/*
		 * Acquire hment lock for this page.
		 * We can ignore return value of hat_hment_tryenter() because
		 * prepared L2PT can't be released even if the hat
		 * lock is released.
		 */
		(void)hat_hment_tryenter(pp, hat);
	}

	/* Load a translation. */
	hat_l2pt_pteload(hat, l2pt, vaddr, pp->p_pagenum, 0, pp, attr, flags,
			 &tlbf);

	/* hat_l2pt_pteload() releases hat and hment lock. */
	ASSERT(!HAT_IS_LOCKED(hat));
	ASSERT(!hment_owned(pp));
	hment_adjust_reserve();
	TLBF_CTX_FINI(&tlbf);
}

/*
 * void
 * hat_memload_region(struct hat *hat, caddr_t addr, struct page *pp,
 *		      uint_t attr, uint_t flags, hat_region_cookie_t rcookie)
 *	Load a transration for shared region.
 *	This function simply calls hat_memload().
 */
/* ARGSUSED5 */
void
hat_memload_region(struct hat *hat, caddr_t addr, struct page *pp,
		   uint_t attr, uint_t flags, hat_region_cookie_t rcookie)
{
	hat_memload(hat, addr, pp, attr, flags);
}

#ifndef	LPG_DISABLE
/*
 * void
 * hat_memload_array(hat_t *hat, caddr_t addr, size_t len, page_t **pages,
 *		  uint_t attr, uint_t flags)
 *	Load the given array of page structures using large pages
 *	when possible.
 *
 *	See hat_memload() comment for flags.
 *
 * Remarks:
 *	hat_memload_array() updates only kernel master L1PT if the kernel
 *	space is specified.
 */
void
hat_memload_array(hat_t *hat, caddr_t addr, size_t len, page_t **pages,
		  uint_t attr, uint_t flags)
{
	uintptr_t	vaddr = (uintptr_t)addr;
	uintptr_t	endaddr = vaddr + len;
	size_t		rsize;
	tlbf_ctx_t	tlbf;
	page_t		**ppp = pages;
	l1pte_t		*l1pt = hat->hat_l1vaddr;
	boolean_t	l1loaded = B_FALSE, noconsist;
	boolean_t	khat = HAT_IS_KERNEL(hat);

	ASSERT(IS_PAGEALIGNED(addr));
	ASSERT(khat || endaddr <= KERNELBASE);
	ASSERT(khat || AS_LOCK_HELD(hat->hat_as, &hat->hat_as->a_lock));
	ASSERT((flags & VALID_MEMLOAD_FLAGS) == flags);

	rsize = PAGE_ROUNDUP(endaddr - vaddr);

	/*
	 * We always set HAT_STORECACHING_OK because hat_memload_array() is
	 * used to map normal memory.
	 */
	attr |= HAT_STORECACHING_OK;

	TLBF_CTX_INIT(&tlbf, 0);

	if (vaddr >= KERNELBASE) {
		ASSERT(khat);

		attr &= ~PROT_USER;
	}
	else {
		ASSERT(!khat);
		ASSERT(attr & PROT_USER);
	}

	noconsist = (flags & HAT_LOAD_NOCONSIST) ? B_TRUE : B_FALSE;

	while (rsize > 0) {
		uint_t		szc;
		pfn_t		pfn;
		uint_t		i, mattr;
		size_t		pgsz, contig_size;
		pgcnt_t		npages, npgs;
		l1pte_t		*l1ptep, *el1ptep;
		hat_l2pt_t	*l2pt;
		page_t		*pp = *ppp;

		/*
		 * Check whether we must use special attribute for the mapping.
		 */
		pfn = pp->p_pagenum;
		mattr = attr;
		hat_plat_memattr(pfn, &mattr);

		/* Determine page size to use for this mapping. */
		npages = SZCPAGES(pp->p_szc);
		contig_size = mmu_ptob(npages);
		for (i = 1; i < npages; i++) {
			page_t	*p = *(ppp + i);

			if (pfn + i != p->p_pagenum) {
				break;
			}
			ASSERT(PP_PAGEROOT(p) == PP_PAGEROOT(pp));
			ASSERT(pp + i == *(ppp + i));
		}
		if (i != npages) {
			contig_size = mmu_ptob(i);
		}

		HAT_LOCK(hat);

	tryagain:
		HAT_CHOOSE_PAGESIZE(vaddr, pfn, contig_size, szc, l1pt, l1ptep,
				    el1ptep, pgsz);

		/* Load this mapping. */
		if (l1ptep == NULL) {
			boolean_t	newpt;

			/* Prepare L2PT. */
			l2pt = hatpt_l2pt_prepare(hat, vaddr, &newpt);
			if (!noconsist && !hment_tryenter(pp)) {
				hat_l2bundle_t	*l2b =
					HAT_L2PT_TO_BUNDLE(l2pt);

				/* Need to check again. */
				HAT_L2PT_RELE(l2pt);
				hatpt_l2pt_release(l2b, L1PT_INDEX(vaddr),
						   l2pt);

				/*
				 * We must acquire hment lock at once,
				 * or deadlock may occur on interrupt thread.
				 */
				HAT_UNLOCK(hat);
				hment_enter(pp);
				HAT_LOCK(hat);
				hment_exit(pp);
				goto tryagain;
			}

			/*
			 * Kernel L1PT entry for coarse page table must be
			 * copied to all L1PTs immediately.
			 */
			HATPT_KAS_SYNC_L(vaddr, MMU_PAGESIZE, newpt && khat);
			hat_l2pt_pteload(hat, l2pt, vaddr, pfn, szc, pp, mattr,
					 flags, &tlbf);
		}
		else {
			/* Prepare software flags for L1PTE. */
			l2pt = hatpt_l1pt_softflags_prepare(hat, vaddr);
			if (!noconsist && !hment_tryenter(pp)) {
				hat_l2bundle_t	*l2b =
					HAT_L2PT_TO_BUNDLE(l2pt);

				/* Need to check again. */
				HAT_L2PT_RELE(l2pt);
				hatpt_l2pt_release(l2b, L1PT_INDEX(vaddr),
						   l2pt);

				/*
				 * We must acquire hment lock at once,
				 * or deadlock may occur on interrupt thread.
				 */
				HAT_UNLOCK(hat);
				hment_enter(pp);
				HAT_LOCK(hat);
				hment_exit(pp);
				goto tryagain;
			}

			hat_l1pt_pteload(hat, vaddr, pfn, szc, pp, mattr,
					 flags, l2pt, &tlbf);
			l1loaded = B_TRUE;
		}
		ASSERT(!HAT_IS_LOCKED(hat));
		ASSERT(!hment_owned(pp));

		npgs = mmu_btop(pgsz);
		pfn += npgs;
		ppp += npgs;
		vaddr += pgsz;
		rsize -= pgsz;
	}

	ASSERT(!HAT_IS_LOCKED(hat));

	/*
	 * Need to sync master kernel L1PT changes into all user
	 * hat structures.
	 */
	HATPT_KAS_SYNC(addr, len, l1loaded && khat);

	TLBF_CTX_FINI(&tlbf);
}

/*
 * void
 * hat_memload_array_region(struct hat *hat, caddr_t addr, size_t len,
 *			    struct page **pps, uint_t attr, uint_t flags,
 *			    hat_region_cookie_t rcookie)
 *	Load the given array of page structures for shared region.
 *	This function simply calls hat_memload_array().
 */
/* ARGSUSED6 */
void
hat_memload_array_region(struct hat *hat, caddr_t addr, size_t len,
			 struct page **pps, uint_t attr, uint_t flags,
			 hat_region_cookie_t rcookie)
{
	hat_memload_array(hat, addr, len, pps, attr, flags);
}
#endif	/* !LPG_DISABLE */

#define	VALID_DEVLOAD_FLAGS	(HAT_LOAD|HAT_LOAD_NOCONSIST|HAT_LOAD_LOCK)

/*
 * void
 * hat_devload(hat_t *hat, caddr_t addr, size_t len, pfn_t pfn, uint_t attr,
 *	       int flags)
 *	Load translation to the given page frame number.
 *	Equivalent of hat_memload(), but can be used for device memory where
 *	there are no page structures and we support additional flags.
 *	Note that we can have large page mappings with this interface.
 *
 * Advisory ordering attributes:
 *	HAT_STRICTORDER:	Strongly ordered access.
 *				This is the default.
 *
 *	HAT_UNORDERED_OK:	Treated as HAT_STORECACHING_OK.
 *
 *	HAT_MERGING_OK:		Uncached, bufferable mapping.
 *
 *	HAT_LOADCACHING_OK:	Treated as HAT_MERGING_OK.
 *
 *	HAT_STORECACHING_OK:	Cached, bufferable mapping.
 *				If the specified page frame number is memory,
 *				this attribute is chosen unless
 *				HAT_PLAT_NOCACHE is set in attr.
 *
 * Remarks:
 *	hat_devload() updates only kernel master L1PT if the kernel space is
 *	specified.
 */
void
hat_devload(hat_t *hat, caddr_t addr, size_t len, pfn_t pfn, uint_t attr,
	    int flags)
{
	uintptr_t	vaddr = PAGE_ROUNDDOWN(addr);
	uintptr_t	endaddr = (uintptr_t)(addr + len);
	size_t		rsize;
	l1pte_t		*l1pt = hat->hat_l1vaddr;
	tlbf_ctx_t	tlbf;
	boolean_t	l1loaded = B_FALSE, khat = HAT_IS_KERNEL(hat);

	ASSERT(IS_PAGEALIGNED(vaddr));
	ASSERT(khat || endaddr <= KERNELBASE);
	ASSERT(khat || AS_LOCK_HELD(hat->hat_as, &hat->hat_as->a_lock));
	ASSERT((flags & VALID_DEVLOAD_FLAGS) == flags);

	rsize = PAGE_ROUNDUP(endaddr - vaddr);

	if (vaddr >= KERNELBASE) {
		ASSERT(khat);
		attr &= ~PROT_USER;
	}
	else {
		ASSERT(!khat);
		ASSERT(attr & PROT_USER);
	}

	TLBF_CTX_INIT(&tlbf, 0);
	while (rsize > 0) {
		uint_t		szc;
		l1pte_t		*l1ptep, *el1ptep;
		hat_l2pt_t	*l2pt;
		uint_t		a, f;
		size_t		pgsz;
		page_t		*pp;

		a = attr;
		f = flags;
		if (pf_is_memory(pfn)) {
			/*
			 * We should map memory as cached unless
			 * HAT_PLAT_NOCACHE is specified.
			 */
			if ((a & HAT_PLAT_NOCACHE) == 0) {
				a |= HAT_STORECACHING_OK;
			}
			if (f & HAT_LOAD_NOCONSIST) {
				pp = NULL;
			}
			else {
				if ((pp = page_numtopp_nolock(pfn)) == NULL) {
					f |= HAT_LOAD_NOCONSIST;
				}
			}

			/*
			 * Check whether we must use special attribute
			 * for the mapping.
			 */
			hat_plat_memattr(pfn, &a);
		}
		else {
			pp = NULL;
			f |= HAT_LOAD_NOCONSIST;
		}

		HAT_LOCK(hat);

	tryagain:
		if (pp == NULL) {
			HAT_CHOOSE_PAGESIZE(vaddr, pfn, rsize, szc, l1pt,
					    l1ptep, el1ptep, pgsz);
		}
		else {
			/* Use small page if consist mapping. */
			l1ptep = NULL;
			szc = 0;
			pgsz = MMU_PAGESIZE;
		}

		if (l1ptep == NULL) {
			boolean_t	newpt;

			/* Prepare L2PT. */
			l2pt = hatpt_l2pt_prepare(hat, vaddr, &newpt);
			if (pp != NULL && !hment_tryenter(pp)) {
				hat_l2bundle_t	*l2b =
					HAT_L2PT_TO_BUNDLE(l2pt);

				/* Need to check again. */
				HAT_L2PT_RELE(l2pt);
				hatpt_l2pt_release(l2b, L1PT_INDEX(vaddr),
						   l2pt);

				/*
				 * We must acquire hment lock at once,
				 * or deadlock may occur on interrupt thread.
				 */
				HAT_UNLOCK(hat);
				hment_enter(pp);
				HAT_LOCK(hat);
				hment_exit(pp);
				goto tryagain;
			}

			/*
			 * Kernel L1PT entry for coarse page table must be
			 * copied to all L1PTs immediately.
			 */
			HATPT_KAS_SYNC_L(vaddr, MMU_PAGESIZE, newpt && khat);

			/* Load this mapping using large or small page. */
			hat_l2pt_pteload(hat, l2pt, vaddr, pfn, szc, pp, a, f,
					 &tlbf);
		}
		else {
			/* Prepare software flags for L1PTE. */
			l2pt = hatpt_l1pt_softflags_prepare(hat, vaddr);
			if (pp != NULL && !hment_tryenter(pp)) {
				hat_l2bundle_t	*l2b =
					HAT_L2PT_TO_BUNDLE(l2pt);

				/* Need to check again. */
				HAT_L2PT_RELE(l2pt);
				hatpt_l2pt_release(l2b, L1PT_INDEX(vaddr),
						   l2pt);

				/*
				 * We must acquire hment lock at once,
				 * or deadlock may occur on interrupt thread.
				 */
				HAT_UNLOCK(hat);
				hment_enter(pp);
				HAT_LOCK(hat);
				hment_exit(pp);
				goto tryagain;
			}
			l1loaded = B_TRUE;
			ASSERT(l2pt);

			/* Load this mapping using section or supersection. */
			hat_l1pt_pteload(hat, vaddr, pfn, szc, pp, a, f, l2pt,
					 &tlbf);
		}
		ASSERT(!HAT_IS_LOCKED(hat));
		ASSERT(pp == NULL || !hment_owned(pp));

		vaddr += pgsz;
		pfn += mmu_btop(pgsz);
		rsize -= pgsz;
	}

	ASSERT(!HAT_IS_LOCKED(hat));

	/*
	 * Need to sync master kernel L1PT changes into all user
	 * hat structures.
	 */
	HATPT_KAS_SYNC(addr, len, l1loaded && khat);

	TLBF_CTX_FINI(&tlbf);
}

/*
 * void
 * hat_unlock(hat_t *hat, caddr_t addr, size_t len)
 *	Unlock the specified mappings.
 */
void
hat_unlock(hat_t *hat, caddr_t addr, size_t len)
{
	uintptr_t	vaddr = (uintptr_t)addr, endaddr;
	int		khat;

	khat = HAT_IS_KERNEL(hat);

	endaddr = vaddr + len;

	ASSERT(IS_PAGEALIGNED(vaddr));
	ASSERT(IS_PAGEALIGNED(endaddr));

	if (!khat) {
		ASSERT(AS_LOCK_HELD(hat->hat_as, &hat->hat_as->a_lock));

		if (endaddr > USERLIMIT) {
			panic("hat_unlock() address out of range - "
			      "above USERLIMIT");
		}
	}

	HAT_LOCK(hat);

	while (vaddr < endaddr) {
		uintptr_t	nextaddr;
		l1pte_t		*l1ptep;
		l2pte_t		*ptep;
		hat_l2pt_t	*l2pt;
		ssize_t		pgsz;
		pfn_t		pfn;

		nextaddr = vaddr;
		pgsz = hatpt_pte_walk(hat, &nextaddr, &l1ptep, &ptep, &l2pt,
				      &pfn, NULL);
		if (pgsz == -1) {
			goto nextloop;
		}
		ASSERT(l2pt);

		if (pgsz >= L1PT_SECTION_VSIZE) {
			uint32_t	sw;

			/* Use hat_l2pt as software flags. */
			sw = HAT_L2PT_GET_SECTION_FLAGS(l2pt);
			sw &= ~PTE_S_LOCKED;
			HAT_L2PT_SET_SECTION_FLAGS(l2pt, sw);
		}
		else {
			l2pte_t	*swptep, sw;

			ASSERT(ptep);
			swptep = L2PT_SOFTFLAGS(ptep);
			sw = *swptep;
			sw &= ~PTE_S_LOCKED;
			*swptep = sw;

			/* Release L2PT held by pteload with HAT_LOAD_LOCK. */
			if (!khat) {
				HAT_L2PT_RELE(l2pt);
			}
		}

	nextloop:
		if (nextaddr < vaddr) {
			break;
		}
		vaddr = nextaddr;
	}

	HAT_UNLOCK(hat);
}

/*
 * void
 * hat_unload(hat_t *hat, caddr_t addr, size_t len, uint_t flags)
 *	Unload a range of virtual address space without callback.
 */
void
hat_unload(hat_t *hat, caddr_t addr, size_t len, uint_t flags)
{
	uintptr_t	vaddr = (uintptr_t)addr;

	/* Use hat_kmap_unload() for request to unmap kmap address. */
	if (hat_kmap_start <= vaddr && vaddr < hat_kmap_end) {
		hat_kmap_unload(hat, addr, len, flags);
		return;
	}

	hat_unload_callback(hat, addr, len, flags, NULL);
}

/*
 * Used by hat_unload_callback().
 * This merges callbacks for contiguous space.
 */
typedef struct cb_ctx {
	caddr_t	cb_start;
	caddr_t	cb_end;
} cb_ctx_t;

#define	CB_CTX_INIT(cb)				\
	do {					\
		(cb)->cb_start = (caddr_t)-1;	\
	} while (0)

#define	CB_CTX_INVOKE(ctx, hat, cb, saddr, eaddr)		\
	do {							\
		if ((ctx)->cb_start == (caddr_t)-1) {		\
			(ctx)->cb_start = (caddr_t)(saddr);	\
			(ctx)->cb_end = (caddr_t)(eaddr);	\
		}						\
		else if ((ctx)->cb_end == (caddr_t)(saddr)) {	\
			(ctx)->cb_end = (caddr_t)(eaddr);	\
		}						\
		else {						\
			(cb)->hcb_start_addr = (ctx)->cb_start;	\
			(cb)->hcb_end_addr = (ctx)->cb_end;	\
			HAT_UNLOCK(hat);			\
			(cb)->hcb_function(cb);			\
			HAT_LOCK(hat);				\
			(ctx)->cb_start = (caddr_t)(saddr);	\
			(ctx)->cb_end = (caddr_t)(eaddr);	\
		}						\
	} while (0)

#define	CB_CTX_FINI(ctx, hat, cb)				\
	do {							\
		if ((ctx)->cb_start != (caddr_t)-1) {		\
			(cb)->hcb_start_addr = (ctx)->cb_start;	\
			(cb)->hcb_end_addr = (ctx)->cb_end;	\
			(cb)->hcb_function(cb);			\
		}						\
	} while (0)

/*
 * void
 * hat_unload_callback(hat_t *hat, caddr_t addr, size_t len, uint_t flags,
 *		       hat_callback_t *callback)
 *	Unload a given range of addresses.
 *	If callback is not NULL, callback function is called for each
 *	unmapped address range.
 */
void
hat_unload_callback(hat_t *hat, caddr_t addr, size_t len, uint_t flags,
		    hat_callback_t *callback)
{
	uintptr_t	vaddr = (uintptr_t)addr, endaddr;
	l1pte_t		*l1pt;
	tlbf_ctx_t	tlbf;
	cb_ctx_t	cb;
	boolean_t	l1unloaded = B_FALSE, khat = HAT_IS_KERNEL(hat);

	ASSERT(IS_PAGEALIGNED(vaddr));
	ASSERT(IS_PAGEALIGNED(len));
	ASSERT(khat || vaddr + len <= KERNELBASE);

	endaddr = vaddr + len;
	TLBF_CTX_INIT(&tlbf, 0);
	CB_CTX_INIT(&cb);

	HAT_LOCK(hat);
	l1pt = hat->hat_l1vaddr;
	while (vaddr < endaddr) {
		uintptr_t	nextaddr = vaddr;
		uint16_t	l1idx = L1PT_INDEX(vaddr);
		l1pte_t		*l1ptep, l1pte;
		int		bindex;
		hat_l2bundle_t	*l2b;
		hat_l2pt_t	*l2pt = NULL;
		boolean_t	unloaded, coarse, ptunloaded;

		/* Check L1PT entry. */
		l1ptep = l1pt + l1idx;
		bindex = HAT_L2BD_INDEX(hat, l1idx);
		l2b = hat->hat_l2bundle[bindex];
		unloaded = hat_l1pt_unload(hat, l1ptep, l1idx, &nextaddr,
					   flags, &tlbf, &coarse);
		l1unloaded = (l1unloaded || unloaded);
		if (l2b == NULL) {
			/* No L2PT bundle. */
			nextaddr = (vaddr & L2BD_VMASK) + L2BD_VSIZE;
			goto nextloop;
		}

		ASSERT(l2b->l2b_index == bindex);
		l2pt = &l2b->l2b_l2pt[HAT_L2BD_L2PT_INDEX(l1idx)];
		if (coarse) {
			unloaded = hat_l2pt_unload(hat, l2pt, &nextaddr,
						   flags, &tlbf);
		}
		ASSERT(HAT_IS_LOCKED(hat));
		ptunloaded = hatpt_l2pt_release(l2b, l1idx, l2pt);

		/*
		 * Kernel L1PT entry for coarse page table must be
		 * copied to all L1PTs immediately.
		 */
		HATPT_KAS_SYNC_L(vaddr, MMU_PAGESIZE, ptunloaded && khat);

	nextloop:
		if (unloaded && callback) {
			CB_CTX_INVOKE(&cb, hat, callback, vaddr, nextaddr);
		}
		if (nextaddr < vaddr) {
			break;
		}
		vaddr = nextaddr;
	}

	/*
	 * Need to sync master kernel L1PT changes into all user
	 * hat structures.
	 */
	HATPT_KAS_SYNC_L((uintptr_t)addr, len, l1unloaded && khat);

	TLBF_CTX_FINI(&tlbf);
	HAT_UNLOCK(hat);

	CB_CTX_FINI(&cb, hat, callback);
	hment_adjust_reserve();
}

/*
 * void
 * hat_sync(hat_t *hat, caddr_t addr, size_t len, uint_t flags)
 *	Synchronize software PTE bits to page associated with the mapping.
 */
void
hat_sync(hat_t *hat, caddr_t addr, size_t len, uint_t flags)
{
	uintptr_t	vaddr = (uintptr_t)addr, endaddr;
	tlbf_ctx_t	tlbf;

	ASSERT(IS_PAGEALIGNED(vaddr));
	ASSERT(IS_PAGEALIGNED(len));
	ASSERT(flags == HAT_SYNC_DONTZERO || flags == HAT_SYNC_ZERORM);

	endaddr = vaddr + len;
	TLBF_CTX_INIT(&tlbf, 0);
	HAT_LOCK(hat);

	while (vaddr < endaddr) {
		uintptr_t	nextaddr;
		l1pte_t		*l1ptep;
		l2pte_t		*ptep, *swptep;
		hat_l2pt_t	*l2pt, *swflags;
		ssize_t		pgsz;
		uint32_t	sw;
		page_t		*pp, *lkpp = NULL;
		pfn_t		pfn;

	tryagain:
		nextaddr = vaddr;
		swflags = NULL;
		swptep = NULL;
		pgsz = hatpt_pte_walk(hat, &nextaddr, &l1ptep, &ptep, &l2pt,
				      &pfn, &pp);
		if (pgsz == -1 || pp == NULL) {
			pp = lkpp;
			goto nextloop;
		}
		if (l2pt == NULL) {
			/* This must be a static kernel page. */
			pp = lkpp;
			goto nextloop;
		}
		if (lkpp == NULL) {
			if (!hat_hment_tryenter(pp, hat)) {
				/* Try again because we lost the race. */
				lkpp = pp;
				goto tryagain;
			}
		}
		else if (pp != lkpp) {
			/* Check again because the mapping has been changed. */
			hment_exit(lkpp);
			lkpp = NULL;
			goto tryagain;
		}

		if (pgsz >= L1PT_SECTION_VSIZE) {
			if (!L1PT_PTE_IS_REFMOD(*l1ptep)) {
				goto nextloop;
			}
			/* Use hat_l2pt as software flags. */
			swflags = l2pt;
			sw = HAT_L2PT_GET_SECTION_FLAGS(swflags);
		}
		else {
			ASSERT(ptep);
			if (!L2PT_PTE_IS_REFMOD(*ptep)) {
				goto nextloop;
			}
			swptep = L2PT_SOFTFLAGS(ptep);
			sw = *swptep;
		}

		if (SWPTE_IS_NOSYNC(sw)) {
			goto nextloop;
		}

		if (flags == HAT_SYNC_ZERORM) {
			/* Need to clear ref or mod bits. */

			if (swflags) {
				l1pte_t	l1pte = *l1ptep;

				L1PT_PTE_CLR_REFMOD(l1pte);
				hat_l1pt_pteupdate(hat, vaddr, l1ptep, swflags,
						   l1pte, sw, pp, &tlbf);
			}
			else {
				l2pte_t	pte = *ptep;

				ASSERT(swptep);
				L2PT_PTE_CLR_REFMOD(pte);
				hat_l2pt_pteupdate(hat, vaddr, ptep, swptep,
						   pte, sw, pp, &tlbf);
			}
		}
		else if (!SWPTE_IS_NOSYNC(sw)) {
			/* Sync PTE flags to page structure. */

			if (swflags) {
				l1pte_t	l1pte = *l1ptep;

				L1PT_PTE_REFMOD_SYNC(l1pte, sw);
			}
			else {
				l2pte_t	pte = *ptep;

				L2PT_PTE_REFMOD_SYNC(pte, sw);
			}
			HAT_UNLOCK(hat);
			hat_ptesync(pp, sw, mmu_btop(pgsz));
			HAT_LOCK(hat);
		}

	nextloop:
		if (pp != NULL) {
			hment_exit(pp);
		}

		if (nextaddr < vaddr) {
			break;
		}
		vaddr = nextaddr;
	}

	TLBF_CTX_FINI(&tlbf);
	HAT_UNLOCK(hat);
}

/*
 * void
 * hat_map(hat_t *hat, caddr_t addr, size_t len, uint_t flags)
 *	Allocate any hat resources needed for a new segment.
 *
 *	Currently, hat_map() does nothing. It's harmless because hat_map()
 *	is an advisory function to preload mapping.
 */
void
hat_map(hat_t *hat, caddr_t addr, size_t len, uint_t flags)
{
}

/*
 * uint_t
 * hat_getattr(hat_t *hat, caddr_t addr, uint_t *attr)
 *	Examine mapping attributes for the specified address.
 *
 * Calling/Exit State:
 *	Upon successful completion, hat_getattr() returns 0 and set mapping
 *	attributes in *attr. On error, return non-zero (actually -1)
 *	without updating *attr.
 */
uint_t
hat_getattr(hat_t *hat, caddr_t addr, uint_t *attr)
{
	uintptr_t	vaddr = (uintptr_t)addr, nextaddr;
	l1pte_t		*l1ptep;
	l2pte_t		*ptep, *swptep;
	hat_l2pt_t	*swflags;
	ssize_t		pgsz;
	page_t		*pp;
	pfn_t		pfn;
	uint_t		a;

	HAT_LOCK(hat);

	vaddr = PAGE_ROUNDDOWN(vaddr);
	nextaddr = vaddr;
	pgsz = hatpt_pte_walk(hat, &nextaddr, &l1ptep, &ptep, &swflags,
			      &pfn, &pp);
	if (pgsz == -1) {
		HAT_UNLOCK(hat);
		return (uint_t)-1;
	}

	a = PROT_READ;

	if (pgsz >= L1PT_SECTION_VSIZE) {
		l1pte_t	pte, sw;

		pte = *l1ptep;
		if (swflags) {
			sw = HAT_L2PT_GET_SECTION_FLAGS(swflags);
			if (SWPTE_IS_NOSYNC(sw)) {
				a |= HAT_NOSYNC;
			}
		}
		else {
			sw = 0;
			a |= HAT_NOSYNC;
		}

		if (SWPTE_IS_WRITABLE(sw)) {
			a |= PROT_WRITE;
		}
		if (L1PT_PTE_IS_USER(pte)) {
			a |= PROT_USER;
		}
		if (SWPTE_IS_EXECUTABLE(sw)) {
			a |= PROT_EXEC;
		}
	}
	else {
		l2pte_t	*swptep, sw, pte;

		swptep = L2PT_SOFTFLAGS(ptep);
		sw = *swptep;
		pte = *ptep;

		if (SWPTE_IS_NOSYNC(sw)) {
			a |= HAT_NOSYNC;
		}
		if (SWPTE_IS_WRITABLE(sw)) {
			a |= PROT_WRITE;
		}
		if (L2PT_PTE_IS_USER(pte)) {
			a |= PROT_USER;
		}
		if (SWPTE_IS_EXECUTABLE(sw)) {
			a |= PROT_EXEC;
		}
	}

	*attr = a;
	HAT_UNLOCK(hat);

	return 0;
}

/*
 * void
 * hat_setattr(hat_t *hat, caddr_t addr, size_t len, uint_t attr)
 *	Append attributes to attributes for the specified address.
 */
void
hat_setattr(hat_t *hat, caddr_t addr, size_t len, uint_t attr)
{
	hat_updateattr(hat, addr, len, attr, HAT_ATTR_SET);
}

/*
 * void
 * hat_clrattr(hat_t *hat, caddr_t addr, size_t len, uint_t attr)
 *	Remove attributes from attributes for the specified address.
 */
void
hat_clrattr(hat_t *hat, caddr_t addr, size_t len, uint_t attr)
{
	hat_updateattr(hat, addr, len, attr, HAT_ATTR_CLEAR);
}

/*
 * void
 * hat_chgattr(hat_t *hat, caddr_t addr, size_t len, uint_t attr)
 *	Set mapping attributes for the specified address.
 */
void
hat_chgattr(hat_t *hat, caddr_t addr, size_t len, uint_t attr)
{
	hat_updateattr(hat, addr, len, attr, HAT_ATTR_LOAD);
}

/*
 * void
 * hat_chgprot(hat_t *hat, caddr_t addr, size_t len, uint_t vprot)
 *	Set protection for the specified address.
 */
void
hat_chgprot(hat_t *hat, caddr_t addr, size_t len, uint_t vprot)
{
	hat_updateattr(hat, addr, len, vprot & HAT_PROT_MASK,
		       HAT_ATTR_LOADPROT);
}

/*
 * ssize_t
 * hat_getpagesize(hat_t *hat, caddr_t addr)
 *	Returns pagesize of mapping at the specified virtual address.
 *
 * Calling/Exit State:
 *	hat_getpagesize() returns -1 if no page is mapped.
 */
ssize_t
hat_getpagesize(hat_t *hat, caddr_t addr)
{
	l1pte_t	*l1ptep;
	l2pte_t	*ptep;
	ssize_t	pagesize;

	HAT_LOCK(hat);
	pagesize = hatpt_pte_lookup(hat, (uintptr_t)addr, &l1ptep,
				    &ptep, NULL);
	HAT_UNLOCK(hat);

	return pagesize;
}

/*
 * pfn_t
 * hat_getpfnum(hat_t *hat, caddr_t addr)
 *	Return page frame number mapped the specified address in the hat.
 *
 * Calling/Exit State:
 *	Return PFN_INVALID if no page is mapped.
 */
pfn_t
hat_getpfnum(hat_t *hat, caddr_t addr)
{
	uintptr_t	vaddr = (uintptr_t)addr;
	uintptr_t	off, paddr;
	l1pte_t		*l1ptep, l1pte;
	l2pte_t		*ptep, pte;
	pfn_t		pfn = PFN_INVALID;
	ssize_t		pagesize;

	HAT_LOCK(hat);

	pagesize = hatpt_pte_lookup(hat, vaddr, &l1ptep, &ptep, NULL);
	if (pagesize == -1) {
		goto out;
	}

	switch (pagesize) {
	case L1PT_SPSECTION_VSIZE:
		/* Supersection */
		l1pte = *l1ptep;
		off = vaddr & L1PT_SPSECTION_VOFFSET;
		paddr = (uintptr_t)l1pte & L1PT_SPSECTION_ADDRMASK;
		break;

	case L1PT_SECTION_VSIZE:
		/* Section */
		l1pte = *l1ptep;
		off = vaddr & L1PT_SECTION_VOFFSET;
		paddr = (uintptr_t)l1pte & L1PT_SECTION_ADDRMASK;
		break;

	case L2PT_LARGE_VSIZE:
		/* Large page */
		pte = *ptep;
		off = vaddr & L2PT_LARGE_VOFFSET;
		paddr = (uintptr_t)pte & L2PT_LARGE_ADDRMASK;
		break;

	default:
		/* Small page */
		ASSERT(pagesize == MMU_PAGESIZE);
		pte = *ptep;
		off = 0;
		paddr = (uintptr_t)pte & L2PT_SMALL_ADDRMASK;
		break;
	}

	paddr += off;
	pfn = mmu_btop(paddr);

 out:
	HAT_UNLOCK(hat);

	return pfn;
}

/*
 * pfn_t
 * hat_getkpfnum(caddr_t addr)
 *	Return page frame number mapped at the specified kernel space.
 *
 *	Although hat_getkpfnum() is an obsolete DDI routine, we use this
 *	for internal use.
 *
 * Remarks:
 *	hat_getkpfnum() uses VA to PA register instead of page table.
 *	So it can be called from even HAT critical section, but it may
 *	return PFN_INVALID if the specified address is not locked.
 */
pfn_t
hat_getkpfnum(caddr_t addr)
{
	pfn_t		pfn;
	uintptr_t	vaddr, paddr;

	vaddr = PAGE_ROUNDDOWN(addr);

	if (vaddr < KERNELBASE) {
		return PFN_INVALID;
	}

	/* Set VA to PA register (kernel read access). */
	paddr = VTOP_GET_PADDR(vaddr, 0);
	if (VTOP_ERROR(paddr)) {
		/* No mapping. */
		pfn = PFN_INVALID;
	}
	else {
		pfn = mmu_btop(VTOP_PADDR(paddr));
	}

	return pfn;
}

/*
 * int
 * hat_probe(hat_t *hat, caddr_t addr)
 *	Determine whether the valid mapping is present at the specified
 *	virtual address.
 *
 * Calling/Exit State:
 *	Return 1 if mapping is valid. Return 0 if no mapping.
 */
int
hat_probe(hat_t *hat, caddr_t addr)
{
	return (hat_getpfnum(hat, addr) != PFN_INVALID) ? 1 : 0;
}

/* Page table sharing is not supported. */
int
hat_share(hat_t *hat, caddr_t addr, hat_t *ism_hat, caddr_t sic_addr,
	  size_t len, uint_t ismszc)
{
	return ENOTSUP;
}

void
hat_unshare(hat_t *hat, caddr_t addr, size_t len, uint_t ismszc)
{
}

/*
 * void
 * hat_reserve(struct as *as, caddr_t addr, size_t len)
 *	Does nothing.
 */
void
hat_reserve(struct as *as, caddr_t addr, size_t len)
{
}

/*
 * void
 * hat_page_setattr(struct page *pp, uint_t flag)
 *	Set reference/modify bits in the specified page.
 */
void
hat_page_setattr(struct page *pp, uint_t flag)
{
	vnode_t		*vp = pp->p_vnode;
	kmutex_t	*vphm = NULL;

	if (PP_GETRM(pp, flag) == flag) {
		return;
	}

	if ((flag & P_MOD) != 0 && vp != NULL && IS_VMODSORT(vp)) {
		vphm = page_vnode_mutex(vp);
		mutex_enter(vphm);
	}

	PP_SETRM(pp, flag);

	if (vphm != NULL) {
		/*
		 * Some File Systems examine v_pages for NULL w/o
		 * grabbing the vphm mutex. Must not let it become NULL when
		 * pp is the only page on the list.
		 */
		if (pp->p_vpnext != pp) {
			page_t	**pplist;

			page_vpsub(&vp->v_pages, pp);
			if (vp->v_pages != NULL) {
				pplist = &vp->v_pages->p_vpprev->p_vpnext;
			}
			else {
				pplist = &vp->v_pages;
			}
			page_vpadd(pplist, pp);
                }
		mutex_exit(vphm);
	}
}

/*
 * static void
 * hat_page_clrwrt(page_t *pp)
 *	Walk all mappings of a page, removing write permission and clearing
 *	the ref/mod bits.
 */
static void
hat_page_clrwrt(page_t *pp)
{
	hment_t		*hm = NULL;
	void		*pt;
	uint16_t	szc;
	uint_t		entry, pszc = 0;
	tlbf_ctx_t	tlbf;

	TLBF_CTX_INIT(&tlbf, TLBF_UHAT_SYNC);

 next_size:
	hment_enter(pp);
	while ((hm = hment_walk(pp, &pt, &szc, &entry, hm)) != NULL) {
		/*
		 * If we are looking for large mappings and this hment doesn't
		 * reach the range we are seeking, just ignore it.
		 */
		SZC_ASSERT(szc);
		szc = SZC_EVAL(szc);
		if (szc < pszc) {
			continue;
		}
		if (szc >= SZC_SECTION) {
			hat_t		*hat = (hat_t *)pt;
			l1pte_t		*l1ptep, l1pte;
			pfn_t		pfn;
			uintptr_t	mask, vaddr;
			hat_l2pt_t	*swflags;
			uint32_t	sw;

			l1ptep = hat->hat_l1vaddr + entry;
			HAT_LOCK(hat);

			l1pte = *l1ptep;
			if (szc == SZC_SPSECTION) {
				mask = L1PT_SPSECTION_ADDRMASK;
			}
			else {
				ASSERT(szc == SZC_SECTION);
				mask = L1PT_SECTION_ADDRMASK;
			}
			pfn = mmu_btop((uintptr_t)l1pte & mask);
			vaddr = L1PT_IDX2VADDR(entry);
			swflags = hatpt_l1pt_softflags_lookup(hat, vaddr, szc);
			ASSERT(swflags);
			sw = HAT_L2PT_GET_SECTION_FLAGS(swflags);
			if (pfn == pp->p_pagenum && SWPTE_IS_WRITABLE(sw)) {
				/*
				 * Clear ref/mod writable bits.
				 * This requires cross calls to flush existing
				 * TLB entries.
				 */
				SWPTE_CLR_WRITABLE(sw);
				L1PT_PTE_CLR_REFMOD(l1pte);
				hat_l1pt_pteupdate(hat, vaddr, l1ptep, swflags,
						   l1pte, sw, pp, &tlbf);
				HATPT_KAS_SYNC_L(vaddr,
						 mmu_ptob(SZCPAGES(szc)),
						 HAT_IS_KERNEL(hat));
			}

			HAT_UNLOCK(hat);
		}
		else {
			hat_l2pt_t	*l2pt = (hat_l2pt_t *)pt;
			hat_l2bundle_t	*l2b = HAT_L2PT_TO_BUNDLE(l2pt);
			hat_t		*hat = l2b->l2b_hat;
			l2pte_t		*ptep, pte, *swptep, sw;
			pfn_t		pfn;
			uintptr_t	mask, vaddr;

			ASSERT(hat->hat_l2bundle[l2b->l2b_index] == l2b);
			ptep = l2pt->l2_vaddr + entry;
			HAT_LOCK(hat);

			pte = *ptep;
			if (szc == SZC_LARGE) {
				mask = L2PT_LARGE_ADDRMASK;
			}
			else {
				ASSERT(szc == SZC_SMALL);
				mask = L2PT_SMALL_ADDRMASK;
			}
			pfn = mmu_btop((uintptr_t)pte & mask);
			vaddr = HAT_L2PT_TO_VADDR(l2pt, entry);
			swptep = L2PT_SOFTFLAGS(ptep);
			sw = *swptep;
			if (pfn == pp->p_pagenum && SWPTE_IS_WRITABLE(sw)) {
				SWPTE_CLR_WRITABLE(sw);
				L2PT_PTE_CLR_REFMOD(pte);
				hat_l2pt_pteupdate(hat, vaddr, ptep, swptep,
						   pte, sw, pp, &tlbf);
			}

			HAT_UNLOCK(hat);
		}
	}
	hment_exit(pp);

	SZC_ASSERT(pp->p_szc);
	while (pszc < SZC_EVAL(pp->p_szc)) {
		page_t	*tpp;

		pszc++;
		tpp = PP_GROUPLEADER(pp, pszc);
		if (pp != tpp) {
			pp = tpp;
			goto next_size;
		}
	}

	TLBF_CTX_FINI(&tlbf);
}

/*
 * void
 * hat_page_clrattr(struct page *pp, uint_t flag)
 *	Clear attributes in "flag" from the specified page.
 */
void
hat_page_clrattr(struct page *pp, uint_t flag)
{
	vnode_t		*vp = pp->p_vnode;

	ASSERT(!(flag & ~(P_MOD|P_REF|P_RO)));

	/*
	 * Caller is expected to hold page's io lock for VMODSORT to work
	 * correctly with pvn_vplist_dirty() and pvn_getdirty() when mod
	 * bit is cleared.
	 * We don't have assert to avoid tripping some existing third party
	 * code. The dirty page is moved back to top of the v_page list
	 * after IO is done in pvn_write_done().
	 */
	PP_CLRRM(pp, flag);

	if ((flag & P_MOD) != 0 && vp != NULL && IS_VMODSORT(vp)) {
		/*
		 * VMODSORT works by removing write permissions and getting
		 * a fault when a page is made dirty. At this point
		 * we need to remove write permission from all mappings
		 * to this page.
		 */
		hat_page_clrwrt(pp);
	}
}

/*
 * uint_t
 * hat_page_getattr(struct page *pp, uint_t flag)
 *	Return attributes for the specified page.
 *
 *	If flag is specified, return 0 if attribute is disabled and
 *	non zero if enabled. If flag specifies multiple attributes then
 *	returns 0 if ALL attributes are disabled. This is an advisory call.
 */
uint_t
hat_page_getattr(struct page *pp, uint_t flag)
{
	ASSERT(!(flag & ~(P_MOD|P_REF|P_RO)));

	return PP_GETRM(pp, flag);
}

/*
 * hment_t *
 * hat_page_unmap(page_t *pp, void *pt, uint_t entry, uint_t szc,
 *		  tlbf_ctx_t *tlbf)
 *	Unload translation for the specified page.
 *	This is common code used by hat_pageunload() and hment_steal().
 *
 * Calling/Exit State:
 *	The caller must acquire hat and hment lock.
 *	Note that this function releases them.
 */
hment_t *
hat_page_unmap(page_t *pp, void *pt, uint_t entry, uint_t szc,
	       tlbf_ctx_t *tlbf)
{
	hat_t		*hat;
	uint32_t	sw;
	pgcnt_t		pgcnt;
	uintptr_t	vaddr;
	hment_t		*hm;
	uint_t		idx;
	boolean_t	l1unloaded = B_FALSE;

	SZC_ASSERT(szc);
	szc = SZC_EVAL(szc);
	if (szc >= SZC_SECTION) {
		hat_l2pt_t	*swflags;
		l1pte_t		*l1ptep;

		hat = (hat_t *)pt;
		ASSERT(HAT_IS_LOCKED(hat));

		if (szc == SZC_SPSECTION) {
			idx = P2ALIGN(entry, L1PT_SPSECTION_NPTES);
		}
		else {
			ASSERT(szc == SZC_SECTION);
			idx = entry;
		}
		l1ptep = hat->hat_l1vaddr + idx;
		vaddr = L1PT_IDX2VADDR(idx);
		swflags = hatpt_l1pt_softflags_lookup(hat, vaddr, szc);
		ASSERT(swflags);
		sw = HAT_L2PT_GET_SECTION_FLAGS(swflags);
		L1PT_PTE_REFMOD_SYNC(*l1ptep, sw);

		/* Clear L1PT entry. */
		HAT_L2PT_RELE(swflags);
		if (szc == SZC_SPSECTION) {
			HAT_L1PTE_SPSECTION_SET(l1ptep, 0);
			pgcnt = mmu_btop(L1PT_SPSECTION_VSIZE);
		}
		else {
			HAT_L1PTE_SET(l1ptep, 0);
			pgcnt = mmu_btop(L1PT_SECTION_VSIZE);
		}

		/* Remove mapping list entry. */
		hm = hment_remove(pp, pt, szc, entry);

		(void)hatpt_l2pt_release(HAT_L2PT_TO_BUNDLE(swflags),
					 idx, swflags);
		l1unloaded = B_TRUE;
	}
	else {
		hat_l2pt_t	*l2pt = (hat_l2pt_t *)pt;
		hat_l2bundle_t	*l2b = HAT_L2PT_TO_BUNDLE(l2pt);
		int		 bindex;
		l2pte_t		*ptep, *swptep;

		hat = l2b->l2b_hat;
		ASSERT(hat->hat_l2bundle[l2b->l2b_index] == l2b);
		ASSERT(HAT_IS_LOCKED(hat));

		/* Only PTE for group leader page has software flags. */
		if (szc == SZC_LARGE) {
			idx = P2ALIGN(entry, L2PT_LARGE_NPTES);
		}
		else {
			ASSERT(szc == SZC_SMALL);
			idx = entry;
		}

		vaddr = HAT_L2PT_TO_VADDR(l2pt, idx);
		ptep = l2pt->l2_vaddr + idx;
		swptep = L2PT_SOFTFLAGS(ptep);
		sw = *swptep;
		L2PT_PTE_REFMOD_SYNC(*ptep, sw);

		/* Clear L2PT entry. */
		*swptep = 0;
		if (szc == SZC_LARGE) {
			HAT_L2PTE_LARGE_SET(ptep, 0);
			pgcnt = L2PT_LARGE_NPTES;
		}
		else {
			HAT_L2PTE_SET(ptep, 0);
			pgcnt = 1;
		}
		HAT_L2PT_COUNT(l2pt, -pgcnt);

		/* Remove mapping list entry. */
		hm = hment_remove(pp, pt, szc, entry);

		l1unloaded = hatpt_l2pt_release(l2b, L1PT_INDEX(vaddr), l2pt);
	}
	PGCNT_DEC(hat, szc);
	TLBF_CTX_FLUSH(tlbf, hat, vaddr);

	/*
	 * Need to sync master kernel L1PT changes into all user
	 * hat structures.
	 */
	HATPT_KAS_SYNC_L(vaddr, mmu_ptob(SZCPAGES(szc)),
			 l1unloaded && HAT_IS_KERNEL(hat));

	/* Sync software flags. */
	HAT_UNLOCK(hat);
	if (!SWPTE_IS_NOSYNC(sw)) {
		hat_ptesync(pp, sw, pgcnt);
	}

	hment_exit(pp);
	return hm;
}

/*
 * static void
 * hat_do_pageunload(page_t *pp, uint_t minszc)
 *	Do pageunload work.
 *
 *	Unload all translations to the specified page. If the page is a subpage
 *	of a large page whose size code is larger than "minszc", the large page
 *	mappings are also removed.
 */
static void
hat_do_pageunload(page_t *pp, uint_t minszc)
{
	page_t		*curpp = pp;
	tlbf_ctx_t	tlbf;

	TLBF_CTX_INIT(&tlbf, TLBF_UHAT_SYNC);

 next_size:
	while (1) {
		hat_t		*hat;
		hment_t		*hm, *prev;
		void		*pt;
		uint16_t	szc;
		uint_t		entry;

		hment_enter(curpp);
		for (prev = NULL; ; prev = hm) {
			hm = hment_walk(curpp, &pt, &szc, &entry, prev);
			if (hm == NULL) {
				hment_exit(curpp);

				/* If not part of a larger page, we're done. */
				SZC_ASSERT(curpp->p_szc);
				if (SZC_EVAL(curpp->p_szc) <= minszc) {
					goto out;
				}

				/*
				 * Check the next larger page size.
				 * hat_page_demote() may decrease p_szc
				 * but that's ok we'll just take an extra
				 * trip discover there're no larger mappings
				 * and return.
				 */
				minszc++;
				curpp = PP_GROUPLEADER(curpp, minszc);
				goto next_size;
			}

			/* If this mapping size matches, remove it. */
			if (SZC_EVAL(szc) == minszc) {
				break;
			}
		}

		/*
		 * Remove the mapping list entry for this page.
		 * Note that hat_page_unmap() releases hat and hment mutex.
		 */
		if (SZC_EVAL(szc) >= SZC_SECTION) {
			hat = (hat_t *)pt;
		}
		else {
			hat_l2pt_t	*l2pt = (hat_l2pt_t *)pt;
			hat_l2bundle_t	*l2b = HAT_L2PT_TO_BUNDLE(l2pt);

			hat = l2b->l2b_hat;
		}

		HAT_LOCK(hat);
		hm = hat_page_unmap(curpp, pt, entry, SZC_EVAL(szc), &tlbf);
		ASSERT(!HAT_IS_LOCKED(hat));
		ASSERT(!hment_owned(curpp));

		if (hm != NULL) {
			hment_free(hm);
		}
	}

 out:
	TLBF_CTX_FINI(&tlbf);
}

/*
 * int
 * hat_pageunload(struct page *pp, uint_t forceflag)
 *	Unload all translations to the specified pp.
 *	Currently forceflag is not used.
 *
 * Calling/Exit State:
 *	Currently, hat_pageunload() always returns 0.
 *
 *	The page must be EXCL locked.
 */
/*ARGSUSED*/
int
hat_pageunload(struct page *pp, uint_t forceflag)
{
	ASSERT(PAGE_EXCL(pp));
	hat_do_pageunload(pp, 0);

	return 0;
}

#ifndef	LPG_DISABLE

/*
 * void
 * hat_page_demote(page_t *pp)
 *	Unload all large mappings to pp and reduce by 1 p_szc field of every
 *	large page level that included pp.
 *
 *	pp must be locked EXCL. Even though no other constituent pages are
 *	locked * it's legal to unload large mappings to pp because all
 *	constituent pages of large locked mappings have to be locked SHARED.
 *	therefore if we have EXCL lock on one of constituent pages none of
 *	the large mappings to pp are locked.
 *
 *	Change (always decrease) p_szc field starting from the last constituent
 *	page and ending with root constituent page so that root's pszc always
 *	shows the area where hat_page_demote() may be active.
 *
 *	This mechanism is only used for file system pages where it's not always
 *	possible to get EXCL locks on all constituent pages to demote the size
 *	code (as is done for anonymous or kernel large pages).
 *
 * Calling/Exit State:
 *	The page must be EXCL locked.
 */
void
hat_page_demote(page_t *pp)
{
	uint_t		pszc, rszc, szc;
	page_t		*rootpp, *firstpp, *lastpp;
	pgcnt_t		pgcnt;

	ASSERT(PAGE_EXCL(pp));
	ASSERT(!PP_ISFREE(pp));
	ASSERT(page_szc_lock_assert(pp));

	if (pp->p_szc == 0) {
		return;
	}

	/* Unload translation to this page. */
	rootpp = PP_GROUPLEADER(pp, 1);
	hat_do_pageunload(rootpp, 1);

	/*
	 * All large mappings to pp are gone and no new can be setup
	 * since pp is locked exclusively.
	 *
	 * Lock the root to make sure there's only one hat_page_demote()
	 * outstanding within the area of this root's pszc.
	 *
	 * Second potential hat_page_demote() is already eliminated by upper
	 * VM layer via page_szc_lock() but we don't rely on it and use our
	 * own locking (so that upper layer locking can be changed without
	 * assumptions that hat depends on upper layer VM to prevent multiple
	 * hat_page_demote() to be issued simultaneously to the same large
	 * page).
	 */

 again:
	pszc = pp->p_szc;
	if (pszc == 0) {
		return;
	}
	rootpp = PP_GROUPLEADER(pp, pszc);
	hment_enter(rootpp);

	/*
	 * If root's p_szc is different from pszc we raced with another
	 * hat_page_demote().  Drop the lock and try to find the root again.
	 * If root's p_szc is greater than pszc previous hat_page_demote() is
	 * not done yet.  Take and release mlist lock of root's root to wait
	 * for previous hat_page_demote() to complete.
	 */
	if ((rszc = rootpp->p_szc) != pszc) {
		hment_exit(rootpp);
		if (rszc > pszc) {
			/* p_szc of a locked non free page can't increase. */
			ASSERT(pp != rootpp);

			rootpp = PP_GROUPLEADER(rootpp, rszc);
			hment_enter(rootpp);
			hment_exit(rootpp);
		}
		goto again;
	}
	ASSERT(pp->p_szc == pszc);

	/*
	 * Decrement by 1 p_szc of every constituent page of a region that
	 * covered pp. For example if original szc is 3 it gets changed to 2
	 * everywhere except in region 2 that covered pp. Region 2 that
	 * covered pp gets demoted to 1 everywhere except in region 1 that
	 * covered pp. The region 1 that covered pp is demoted to region
	 * 0. It's done this way because from region 3 we removed level 3
	 * mappings, from region 2 that covered pp we removed level 2 mappings
	 * and from region 1 that covered pp we removed level 1 mappings.  All
	 * changes are done from from high pfn's to low pfn's so that roots
	 * are changed last allowing one to know the largest region where
	 * hat_page_demote() is stil active by only looking at the root page.
	 *
	 * This algorithm is implemented in 2 while loops. First loop changes
	 * p_szc of pages to the right of pp's level 1 region and second
	 * loop changes p_szc of pages of level 1 region that covers pp
	 * and all pages to the left of level 1 region that covers pp.
	 * In the first loop p_szc keeps dropping with every iteration
	 * and in the second loop it keeps increasing with every iteration.
	 *
	 * First loop description: Demote pages to the right of pp outside of
	 * level 1 region that covers pp.  In every iteration of the while
	 * loop below find the last page of szc region and the first page of
	 * (szc - 1) region that is immediately to the right of (szc - 1)
	 * region that covers pp.  From last such page to first such page
	 * change every page's szc to szc - 1. Decrement szc and continue
	 * looping until szc is 1. If pp belongs to the last (szc - 1) region
	 * of szc region skip to the next iteration.
	 */
	szc = pszc;
	while (szc > 1) {
		lastpp = PP_GROUPLEADER(pp, szc);
		pgcnt = page_get_pagecnt(szc);
		lastpp += pgcnt - 1;
		firstpp = PP_GROUPLEADER(pp, (szc - 1));
		pgcnt = page_get_pagecnt(szc - 1);
		if (lastpp - firstpp < pgcnt) {
			szc--;
			continue;
		}
		firstpp += pgcnt;
		while (lastpp != firstpp) {
			ASSERT(lastpp->p_szc == pszc);
			lastpp->p_szc = szc - 1;
			lastpp--;
		}
		firstpp->p_szc = szc - 1;
		szc--;
	}

	/*
	 * Second loop description:
	 * First iteration changes p_szc to 0 of every
	 * page of level 1 region that covers pp.
	 * Subsequent iterations find last page of szc region
	 * immediately to the left of szc region that covered pp
	 * and first page of (szc + 1) region that covers pp.
	 * From last to first page change p_szc of every page to szc.
	 * Increment szc and continue looping until szc is pszc.
	 * If pp belongs to the fist szc region of (szc + 1) region
	 * skip to the next iteration.
	 *
	 */
	szc = 0;
	while (szc < pszc) {
		firstpp = PP_GROUPLEADER(pp, (szc + 1));
		if (szc == 0) {
			pgcnt = page_get_pagecnt(1);
			lastpp = firstpp + (pgcnt - 1);
		} else {
			lastpp = PP_GROUPLEADER(pp, szc);
			if (firstpp == lastpp) {
				szc++;
				continue;
			}
			lastpp--;
			pgcnt = page_get_pagecnt(szc);
		}
		while (lastpp != firstpp) {
			ASSERT(lastpp->p_szc == pszc);
			lastpp->p_szc = szc;
			lastpp--;
		}
		firstpp->p_szc = szc;
		if (firstpp == rootpp) {
			break;
		}
		szc++;
	}
	hment_exit(rootpp);
}

#endif	/* !LPG_DISABLE */

/*
 * uint_t
 * hat_pagesync(struct page *pp, uint_t flags)
 *	Sync PTE status into page structure and reset PTE status.
 *
 * Calling/Exit State:
 *	hat_pagesync() returns new page status bits.
 */
uint_t
hat_pagesync(struct page *pp, uint_t flags)
{
	hment_t		*hm = NULL;
	void		*pt;
	uint16_t	szc;
	uint_t		entry, pszc = 0;
	tlbf_ctx_t	tlbf;
	page_t		*save_pp = pp;
	extern ulong_t	po_share;

	ASSERT(PAGE_LOCKED(pp) || panicstr);

	if (PP_ISRO(pp) && (flags & HAT_SYNC_STOPON_MOD)) {
		return PP_GENERIC_ATTR(pp);
	}
	if ((flags & HAT_SYNC_ZERORM) == 0) {
		if ((flags & HAT_SYNC_STOPON_REF) && PP_ISREF(pp)) {
			return PP_GENERIC_ATTR(pp);
		}
		if ((flags & HAT_SYNC_STOPON_MOD) && PP_ISMOD(pp)) {
			return PP_GENERIC_ATTR(pp);
		}

		if ((flags & HAT_SYNC_STOPON_SHARED) &&
		    hat_page_getshare(pp) > po_share) {
			if (PP_ISRO(pp)) {
				PP_SETREF(pp);
			}
			return PP_GENERIC_ATTR(pp);
		}
	}

	TLBF_CTX_INIT(&tlbf, TLBF_UHAT_SYNC);
 next_size:
	/* Walk through the mapping chain syncing and clearing ref/mod bits. */
	hment_enter(pp);
	SZC_ASSERT(pp->p_szc);
	while ((hm = hment_walk(pp, &pt, &szc, &entry, hm)) != NULL) {
		uint32_t	sw;

		/*
		 * If we are looking for large mappings and this hment doesn't
		 * reach the range we are seeking, just ignore it.
		 */
		szc = SZC_EVAL(szc);
		if (szc < pszc) {
			continue;
		}
		if (szc >= SZC_SECTION) {
			hat_t		*hat = (hat_t *)pt;
			l1pte_t		*l1ptep, l1pte;
			uintptr_t	mask, vaddr;
			hat_l2pt_t	*swflags;

			l1ptep = hat->hat_l1vaddr + entry;
			HAT_LOCK(hat);

			l1pte = *l1ptep;
#ifdef	DEBUG
			if (szc == SZC_SPSECTION) {
				mask = L1PT_SPSECTION_ADDRMASK;
			}
			else {
				ASSERT(szc == SZC_SECTION);
				mask = L1PT_SECTION_ADDRMASK;
			}
			ASSERT(mmu_btop((uintptr_t)l1pte & mask) ==
			       pp->p_pagenum);
#endif	/* DEBUG */
			vaddr = L1PT_IDX2VADDR(entry);
			swflags = hatpt_l1pt_softflags_lookup(hat, vaddr, szc);
			ASSERT(swflags);
			sw = HAT_L2PT_GET_SECTION_FLAGS(swflags);
			if (flags & HAT_SYNC_ZERORM) {
				/* Need to clear ref/mod bits. */
				L1PT_PTE_CLR_REFMOD(l1pte);
				hat_l1pt_pteupdate(hat, vaddr, l1ptep, swflags,
						   l1pte, sw, pp, &tlbf);
				HATPT_KAS_SYNC_L(vaddr,
						 mmu_ptob(SZCPAGES(szc)),
						 HAT_IS_KERNEL(hat));
			}
			else {
				L1PT_PTE_REFMOD_SYNC(l1pte, sw);
			}
			HAT_UNLOCK(hat);
		}
		else {
			hat_l2pt_t	*l2pt = (hat_l2pt_t *)pt;
			hat_l2bundle_t	*l2b = HAT_L2PT_TO_BUNDLE(l2pt);
			hat_t		*hat = l2b->l2b_hat;
			l2pte_t		*ptep, pte, *swptep;
			uintptr_t	mask, vaddr;

			ASSERT(hat->hat_l2bundle[l2b->l2b_index] == l2b);
			ptep = l2pt->l2_vaddr + entry;
			HAT_LOCK(hat);

			pte = *ptep;
#ifdef	DEBUG
			if (szc == SZC_LARGE) {
				mask = L2PT_LARGE_ADDRMASK;
			}
			else {
				ASSERT(szc == SZC_SMALL);
				mask = L2PT_SMALL_ADDRMASK;
			}
			ASSERT(mmu_btop((uintptr_t)pte & mask) ==
			       pp->p_pagenum);
#endif	/* DEBUG */
			vaddr = HAT_L2PT_TO_VADDR(l2pt, entry);
			swptep = L2PT_SOFTFLAGS(ptep);
			sw = (uint32_t)*swptep;
			if (flags & HAT_SYNC_ZERORM) {
				/* Need to clear ref/mod bits. */
				L2PT_PTE_CLR_REFMOD(pte);
				hat_l2pt_pteupdate(hat, vaddr, ptep, swptep,
						   pte, sw, pp, &tlbf);
			}
			else {
				L2PT_PTE_REFMOD_SYNC(pte, sw);
			}
			HAT_UNLOCK(hat);
		}

		/* Sync the PTE. */
		if (!(flags & HAT_SYNC_ZERORM) && !SWPTE_IS_NOSYNC(sw)) {
			hat_ptesync(pp, sw, SZCPAGES(szc));
		}
		if (((flags & HAT_SYNC_STOPON_MOD) && PP_ISMOD(save_pp)) ||
		    ((flags & HAT_SYNC_STOPON_REF) && PP_ISREF(save_pp))) {
			hment_exit(pp);
			goto out;
		}
	}
	hment_exit(pp);

	while (pszc < SZC_EVAL(pp->p_szc)) {
		page_t	*tpp;

		pszc++;
		tpp = PP_GROUPLEADER(pp, pszc);
		if (pp != tpp) {
			pp = tpp;
			goto next_size;
		}
	}

 out:
	TLBF_CTX_FINI(&tlbf);
	return PP_GENERIC_ATTR(save_pp);
}

/*
 * ulong_t
 * hat_page_getshare(page_t *pp)
 *	Return approximate number of mappings to the specified page.
 */
ulong_t
hat_page_getshare(page_t *pp)
{
	return (ulong_t)hment_mapcnt(pp);
}

/*
 * int
 * hat_page_checkshare(page_t *pp, ulong_t sh_thresh)
 *	Return 1 the number of mappings exceeds sh_thresh. Return 0 otherwise.
 */
int
hat_page_checkshare(page_t *pp, ulong_t sh_thresh)
{
	return (hat_page_getshare(pp) > sh_thresh);
}

/*
 * faultcode_t
 * hat_softlock(struct hat *hat, caddr_t addr, size_t *lenp, page_t **ppp,
 *		uint_t flags)
 *	This function is currently not supported on this platform.
 */
/* ARGSUSED */
faultcode_t
hat_softlock(struct hat *hat, caddr_t addr, size_t *lenp, page_t **ppp,
	     uint_t flags)
{
	return FC_NOSUPPORT;
}

/*
 * int
 * hat_supported(enum hat_features feature, void *arg)
 *	Determine whether the specified HAT feature is supported
 *	on this architecture.
 *
 *	Currently, shared page table is not supported.
 */
int
hat_supported(enum hat_features feature, void *arg)
{
	return (feature == HAT_VMODSORT) ? 1 : 0;
}

/*
 * void
 * hat_thread_exit(kthread_t *thd)
 *	Called when a thread is exiting and has been switched to
 *	the kernel context.
 */
void
hat_thread_exit(kthread_t *thd)
{
	ASSERT(thd->t_procp->p_as == &kas);

	hat_switch(thd->t_procp->p_as->a_hat);
}

/*
 * void
 * hat_setup(hat_t *hat, int flags)
 *	Setup the given brand new hat structure as the new HAT on
 *	this CPU's MMU.
 */
void
hat_setup(hat_t *hat, int flags)
{
	kpreempt_disable();
	hat_switch(hat);
	kpreempt_enable();
}

/*
 * void
 * hat_enter(hat_t *hat)
 *	Acquire HAT mutex.
 */
void
hat_enter(hat_t *hat)
{
	HAT_LOCK(hat);
}

/*
 * void
 * hat_exit(hat_t *hat)
 *	Release HAT mutex.
 */
void
hat_exit(hat_t *hat)
{
	HAT_UNLOCK(hat);
}

/*
 * int
 * hat_dup(hat_t *old, hat_t *new, caddr_t addr, size_t len, uint_t flag)
 *	Duplicate address translations of the parent the child.
 *
 *	Currently, hat_dup() does nothing. It's harmless because hat_dup()
 *	is an advisory function to preload child mappings.
 */
/*ARGSUSED*/
int
hat_dup(hat_t *old, hat_t *new, caddr_t addr, size_t len, uint_t flag)
{
	return 0;
}

/*
 * void
 * hat_swapin(hat_t *hat)
 *	Allocate any HAT resources required for a process being swapped in.
 *
 *	Currently, hat_swapin() does nothing. We let everything fault back in.
 */
/*ARGSUSED*/
void
hat_swapin(hat_t *hat)
{
}

/*
 * void
 * hat_swapout(hat_t *hat)
 *	Unload all translations corresponding to the specified hat.
 *	This is called to unload translations for an process that is
 *	being swapped out.
 */
void
hat_swapout(hat_t *hat)
{
	uintptr_t	endaddr;

	ASSERT(!HAT_IS_KERNEL(hat));
	ASSERT(AS_LOCK_HELD(hat->hat_as, &hat->hat_as->a_lock));

	endaddr = MIN(USERLIMIT, (uintptr_t)hat->hat_as->a_userlimit);

	/*
	 * Unload all translations but locked mappings.
	 *
	 * Remarks:
	 *	This code should be changed when shared page table is
	 *	supported. seg_spt space and shared page table can't be
	 *	swapped out.
	 */
	hat_unload_callback(hat, (caddr_t)0, (size_t)endaddr,
			    HAT_PLAT_UNLOAD_NOLOCK, NULL);
}

/*
 * size_t
 * hat_get_mapped_size(hat_t *hat)
 *	Returns number of bytes that have valid mapping in the given hat.
 */
size_t
hat_get_mapped_size(hat_t *hat)
{
	size_t	ret = 0;
	int	szc;

	for (szc = 0; szc < mmu_page_sizes; szc++) {
		ret += (mmu_ptob(hat->hat_pages_mapped[szc]) <<
			PAGE_BSZS_SHIFT(szc));
	}

	return ret;
}

/*
 * Enable/Disable collection of stats for hat.
 */
int
hat_stats_enable(hat_t *hat)
{
	atomic_add_32(&(hat->hat_stats), 1);
	return 1;
}

void
hat_stats_disable(hat_t *hat)
{
	atomic_add_32(&(hat->hat_stats), -1);
}

/*
 * void
 * hat_dump(void)
 *	Dump all page tables into the system dump.
 */
void
hat_dump(void)
{
	hat_t	*hat;

	/* Dump kernel hat. */
	hatpt_dump(&hat_kas);

	/* Dump KERNELPHYSBASE. */
	dump_page(mmu_btop(KERNELPHYSBASE));

	/* Dump vector page. */
	dump_page(mmu_btop(VECTOR_PAGE_PADDR));

	/* Call platform-specific dump page chooser. */
	hat_plat_dump();

	/* Dump user hat. */
	mutex_enter(&hat_list_lock);
	HAT_LIST_FOREACH(hat, &hat_kas) {
		hatpt_dump(hat);
	}
	mutex_exit(&hat_list_lock);

	/* Dump xramfs device pages. */
	xramdev_impl_dump();
}

/*
 * No kpm functionality is supported.
 */

caddr_t
hat_kpm_page2va(struct page *pp, int checkswap)
{
	return NULL;
}

caddr_t
hat_kpm_mapin(struct page *pp, struct kpme *kpme)
{
	return NULL;
}

void
hat_kpm_mapout(struct page *pp, struct kpme *kpme, caddr_t vaddr)
{
}

void
hat_kpm_mseghash_clear(int nentries)
{
}

void
hat_kpm_mseghash_update(pgcnt_t inx, struct memseg *msp)
{
}

/*
 * void
 * hat_cpu_online(cpu_t *cp)
 *	Mark the specified cpu as online.
 */
void
hat_cpu_online(cpu_t *cp)
{
	CPUSET_ATOMIC_ADD(hat_kas.hat_cpus, cp->cpu_id);
}

/*
 * void
 * hat_cpu_offline(cpu_t *cp)
 *	Mark the specified cpu as offline.
 */
void
hat_cpu_offline(cpu_t *cp)
{
	CPUSET_ATOMIC_DEL(hat_kas.hat_cpus, cp->cpu_id);
}

/*
 * Special address that indicates hat_flushtlb_vaddr() and hat_flushtlb_asid()
 * should sync entire instruction cache.
 */
#define	ISYNC_ALL	((uintptr_t)-1)

/*
 * static int
 * hat_flushtlb_vaddr(void *a1, void *a2, void *a3)
 *	Flush single TLB entry that maps the specified virtual address.
 *	This function is used to unmap kernel address.
 */
static int
hat_flushtlb_vaddr(void *a1, void *a2, void *a3)
{
	hat_t		*hat = (hat_t *)a1;
	uintptr_t	vaddr = (uintptr_t)a2;
	uintptr_t	syncaddr = (uintptr_t)a3;

	ASSERT(HAT_IS_KERNEL(hat));

	if (syncaddr == NULL) {
		SYNC_BARRIER();
	}
	else {
		/* Do instruction cache maintenance. */
		if (syncaddr != ISYNC_ALL) {
			local_sync_icache(syncaddr, PAGESIZE, NULL);
		}
		else {
			/* Flush entire L1 cache. */
			syncaddr = NULL;
			DCACHE_CLEAN_ALL();
			ICACHE_INV_ALL();
			SYNC_BARRIER();
		}
	}

	if (vaddr == HAT_FLUSHTLB_ALL) {
		/* Flush entire TLB. */
		TLB_FLUSH();
	}
	else {
		/* Flush TLB single entry with vaddr. */
		vaddr = PAGE_ROUNDDOWN(vaddr);
		TLB_FLUSH_VADDR(vaddr);

		if (syncaddr != NULL) {
			/* Flush TLB entry associated with syncaddr. */
			TLB_FLUSH_VADDR(syncaddr);
		}
	}

	return 0;
}

/*
 * static int
 * hat_flushtlb_asid(void *a1, void *a2, void *a3)
 *	Flush all TLB entries corresponding to the specified ASID.
 *	This function is used to unmap user address.
 */
static int
hat_flushtlb_asid(void *a1, void *a2, void *a3)
{
	hat_t		*hat = (hat_t *)a1;
	uintptr_t	syncaddr = (uintptr_t)a2;
	cpu_t		*cp = CPU;
	processorid_t	cpuid = cp->cpu_id;
	uint32_t	asid;

	ASSERT(!HAT_IS_KERNEL(hat));

	if (syncaddr == NULL) {
		SYNC_BARRIER();
	}
	else {
		/* Do instruction cache maintenance. */
		if (syncaddr != ISYNC_ALL) {
			/*
			 * No SYNC_BARRIER() is required because
			 * local_sync_icache() implies SYNC_BARRIER().
			 */
			local_sync_icache(syncaddr, PAGESIZE, NULL);
			TLB_FLUSH_VADDR(syncaddr);
		}
		else {
			/* Flush entire L1 cache. */
			DCACHE_CLEAN_ALL();
			ICACHE_INV_ALL();
			SYNC_BARRIER();
		}
	}

	asid = hat->hat_context[cpuid] & CONTEXT_ID_ASID_MASK;
	TLB_FLUSH_ASID(asid);

	if (cp->cpu_current_hat != hat) {
		/*
		 * All TLB entries associated with this address space has
		 * been invalidated. So we can safely remove this CPU
		 * from hat_cpus until this hat is activated on this CPU.
		 */
		CPUSET_ATOMIC_DEL(hat->hat_cpus, cpuid);
	}

	return 0;
}

/*
 * void
 * hat_xcall(hat_t *hat, hat_xcallfunc_t func, void *a1, void *a2, void *a3,
 *	     uint_t flags)
 *	Common routine to use cross call to send message to the CPUs
 *	where the hat is running.
 *
 * Flags:
 *	HATXC_ALLCPU	Send cross call to all CPU.
 */
void
hat_xcall(hat_t *hat, hat_xcallfunc_t func, void *a1, void *a2, void *a3,
	  uint_t flags)
{
	extern int	flushes_require_xcalls;
	cpuset_t	set, me;

	if (panicstr || !flushes_require_xcalls) {
		/* No need to use xcall. */
		(void)(*func)(a1, a2, a3);
		return;
	}

	if (HAT_IS_KERNEL(hat)) {
		/* Send cross call to all active CPUs. */
		kpreempt_disable();
		xc_call((xc_arg_t)a1, (xc_arg_t)a2, (xc_arg_t)a3,
			X_CALL_HIPRI, hat->hat_cpus, (xc_func_t)func);
		kpreempt_enable();
		return;
	}

	/* Notify CPUs currently running in this hat. */
	HAT_SWITCH_LOCK(hat);
	kpreempt_disable();
	if (flags & HATXC_ALLCPU) {
		set = hat_kas.hat_cpus;
	}
	else {
		set = hat->hat_cpus;
	}
	CPUSET_ONLY(me, CPU->cpu_id);
	if (CPUSET_ISEQUAL(set, me)) {
		(void)(*func)(a1, a2, a3);
	}
	else {
		xc_call((xc_arg_t)a1, (xc_arg_t)a2, (xc_arg_t)a3,
			X_CALL_HIPRI, set, (xc_func_t)func);
	}
	kpreempt_enable();
	HAT_SWITCH_UNLOCK(hat);
}

/*
 * void
 * hat_flushtlb(hat_t *hat, uintptr_t vaddr, page_t *syncpp)
 *	Flush TLB entry associated with the specified virtual address.
 *	If 2 or more CPUs are activated, hat_flushtlb() uses xcall
 *	to flush TLB on other CPUs.
 *
 *	If syncpp is not NULL, hat_flushtlb() maintains instruction cache
 *	coherency associated with the page. If HAT_FLUSHTLB_SYNCALL is
 *	specified to syncpp, it cleans entire L1 data cache, and invalidate
 *	entire L1 instruction cache.
 */
void
hat_flushtlb(hat_t *hat, uintptr_t vaddr, page_t *syncpp)
{
	hat_xcallfunc_t	func;
	void		*a1, *a2, *a3;
	cpuset_t	me;
	caddr_t		syncaddr = NULL;
	l2pte_t		*syncptep = NULL;
	cpu_t		*cp;
	uint_t		flags;

#ifdef	DEBUG
	cp = NULL;
#endif	/* DEBUG */

	if ((hat->hat_flags & HAT_FREEING) && syncpp == NULL) {
		/* No need to flush TLB because this hat is being destroyed. */
		return;
	}

	if (syncpp == NULL) {
		flags = 0;
	}
	else {
		if (syncpp != HAT_FLUSHTLB_SYNCALL) {
			uint_t	attr = PROT_READ|HAT_STORECACHING_OK;

			/*
			 * This page is a small page. We map it to the
			 * CPU local space to sync instruction cache associated
			 * with the page.
			 */
			kpreempt_disable();
			cp = CPU_GLOBAL;
			syncaddr = cp->cpu_isync_addr;
			syncptep = cp->cpu_isync_pte;
			mutex_enter(&(cp->cpu_isync_mutex));
			hat_mempte_remap(syncpp->p_pagenum, syncaddr,
					 syncptep, attr, HAT_LOAD_NOCONSIST);
		}
		else {
			/*
			 * We don't want to create any mapping because
			 * page size is too large to create temporary mapping.
			 * In this case we do flush entire L1 cache.
			 */
			syncaddr = (caddr_t)ISYNC_ALL;
		}

		/* Instruction cache maintenance must be done on all CPU. */
		flags = HATXC_ALLCPU;
	}

	a1 = (void *)hat;
	if (HAT_IS_KERNEL(hat)) {
		func = hat_flushtlb_vaddr;
		a2 = (void *)vaddr;
		a3 = (void *)syncaddr;
	}
	else {
		func = hat_flushtlb_asid;
		a2 = (void *)syncaddr;
		a3 = NULL;
	}

	hat_xcall(hat, func, a1, a2, a3, flags);

	if (syncptep != NULL) {
		ASSERT(cp);
		ASSERT(syncaddr != (caddr_t)ISYNC_ALL);

		/* Clean up temporary mapping. */
		hat_mempte_remap(PFN_INVALID, syncaddr, syncptep, 0, 0);
		mutex_exit(&(cp->cpu_isync_mutex));
		kpreempt_enable();
	}
}

/*
 * void
 * hat_mempte_remap(pfn_t pfn, caddr_t addr, l2pte_t *ptep, uint_t attr,
 *		    uint_t flags)
 *	Establish a temporary CPU private mapping to a page.
 *	The attr and flags parameters are the same as hat_devload().
 *	If pfn is PFN_INVALID, hat_mempte_remap() invalidates the mapping.
 *
 * Calling/Exit State:
 *	The caller must disable preemption.
 *
 * Remarks:
 *	hat_mempte_remap() doesn't take care of TLB entry on other CPUs.
 *
 *	Currently, hat_mempte_remap() assumes that the specified pfn
 *	is a page frame number for normal memory.
 */
void
hat_mempte_remap(pfn_t pfn, caddr_t addr, l2pte_t *ptep, uint_t attr,
		 uint_t flags)
{
	l2pte_t		pte, *swptep = L2PT_SOFTFLAGS(ptep);
#ifdef	DEBUG
	uintptr_t	vaddr = (uintptr_t)addr;
	hat_l2pt_t	*l2pt;
	l2pte_t		*required;

	ASSERT(IS_PAGEALIGNED(vaddr));

	HAT_KAS_LOCK();
	l2pt = hatpt_l2pt_lookup(&hat_kas, vaddr);
	ASSERT(l2pt);
	required = l2pt->l2_vaddr + L2PT_INDEX(vaddr);
	ASSERT(ptep == required);
	HAT_KAS_UNLOCK();
#endif	/* DEBUG */

	/*
	 * Update PTE without hat mutex. So the caller must guarantee that
	 * no one touches this PTE.
	 */
	if (pfn == PFN_INVALID) {
		pte = 0;
		*swptep = 0;
	}
	else {
		/*
		 * Check whether we must use special attribute
		 * for the mapping.
		 *
		 * Currently, hat_mempte_remap() is used only for normal
		 * memory mapping. So we can use hat_plat_memattr() here.
		 */
		ASSERT(pf_is_memory(pfn));
		hat_plat_memattr(pfn, &attr);

		pte = hat_l2pt_mkpte(pfn, attr, flags, 0, swptep);
	}
	HAT_L2PTE_SET(ptep, pte);

	/* Flush single TLB entry. */
	TLB_FLUSH_VADDR(addr);
}

#ifdef	HAT_USE_PTELOAD_RETRY

typedef union arm_mcr {
	struct {
#ifdef	_LITTLE_ENDIAN
		uint32_t	mcru_crm:4,	/* CRm */
				mcru_f3:1,	/* SBO */
				mcru_opcode2:3,	/* opcode_2 */
				mcru_cpnum:4,	/* cp_num */
				mcru_rd:4,	/* Rd */
				mcru_crn:4,	/* CRn */
				mcru_f2:1,	/* SBZ */
				mcru_opcode1:3,	/* opcode_1 */
				mcru_f1:4,	/* 0xe */
				mcru_cond:4;	/* condition */
#else	/* !_LITTLE_ENDIAN */
		uint32_t	mcru_cond:4,	/* condition */
				mcru_f1:4,	/* 0xe */
				mcru_opcode1:3,	/* opcode_1 */
				mcru_f2:1,	/* SBZ */
				mcru_crn:4,	/* CRn */
				mcru_rd:4,	/* Rd */
				mcru_cpnum:4,	/* cp_num */
				mcru_opcode2:3,	/* opcode_2 */
				mcru_f3:1,	/* SBO */
				mcru_crm:4;	/* CRm */
#endif	/* _LITTLE_ENDIAN */
	} mcr_u;
	uint32_t	mcr_value;
} arm_mcr_t;

#define	mcr_cond	mcr_u.mcru_cond
#define	mcr_f1		mcr_u.mcru_f1
#define	mcr_opcode1	mcr_u.mcru_opcode1
#define	mcr_f2		mcr_u.mcru_f2
#define	mcr_crn		mcr_u.mcru_crn
#define	mcr_rd		mcr_u.mcru_rd
#define	mcr_cpnum	mcr_u.mcru_cpnum
#define	mcr_opcode2	mcr_u.mcru_opcode2
#define	mcr_f3		mcr_u.mcru_f3
#define	mcr_crm		mcr_u.mcru_crm

#define	INSTR_COND_RESERVED	0xfU
#define	INSTR_MCR_F1_VALUE	0xeU
#define	INSTR_MCR_F2_VALUE	0x0U
#define	INSTR_MCR_F3_VALUE	0x1U

#define	INSTR_IS_MCR(mcr)				\
	((mcr)->mcr_cond != INSTR_COND_RESERVED &&	\
	 (mcr)->mcr_f1 == INSTR_MCR_F1_VALUE &&		\
	 (mcr)->mcr_f2 == INSTR_MCR_F2_VALUE &&		\
	 (mcr)->mcr_f3 == INSTR_MCR_F3_VALUE)

#define	INSTR_IS_CACHE_MAINT_MVA(mcr)				\
	(INSTR_IS_MCR(mcr) && (mcr)->mcr_cpnum == 15 &&		\
	 (mcr)->mcr_opcode1 == 0 && (mcr)->mcr_crn == 7 &&	\
	 (((mcr)->mcr_opcode2 == 1 &&				\
	   ((mcr)->mcr_crm == 5 || (mcr)->mcr_crm == 6 ||	\
	    (mcr)->mcr_crm == 10 || (mcr)->mcr_crm == 14)) ||	\
	  (mcr)->mcr_opcode2 == 7 && (mcr)->mcr_crm == 5))

/*
 * static boolean_t
 * hat_is_cache_maint_fault(struct regs *rp)
 *	Determine whether the fault is raised by a cache maintenance
 *	instruction.
 *
 * Calling/Exit State:
 *	hat_is_cache_main_fault() returns B_TRUE if the fault raised by a
 *	cache maintenance instruction.
 *
 * Remarks.
 *	hat_is_cache_maint_fault() never considers the fault is raised by
 *	instruction fetch. So this function must be called from data abort
 *	handler.
 */
static boolean_t
hat_is_cache_maint_fault(struct regs *rp)
{
	uintptr_t	pc = rp->r_pc;
	arm_mcr_t	mcr;

	if (pc < KERNELBASE) {
		/* Cache maintenance must be issued from kernel mode. */
		return B_FALSE;
	}
	if (!IS_P2ALIGNED(pc, 4)) {
		/* Program counter is corrupted. */
		return B_FALSE;
	}

	mcr.mcr_value = *((uint32_t *)pc);
	return (INSTR_IS_CACHE_MAINT_MVA(&mcr)) ? B_TRUE : B_FALSE;
}

#define	HAT_PTELOAD_RETRY_COUNT		16

volatile uint_t	hat_pteload_retry_count[NCPU];
volatile uint_t	hat_pteload_cache_retry_count[NCPU];

/*
 * boolean_t
 * hat_pteload_retry(struct regs *rp, caddr_t addr, enum seg_rw rw)
 *	Try to set again PTE.
 *
 *	MPCore processor seems unable to see PTE just after changing it.
 *	So if the given fault address can't be accessed even though it has
 *	a valid PTE, hat_pteload_retry() tries to set it again with
 *	invalidating TLB entry.
 *
 * Calling/Exit State:
 *	hat_pteload_retry() returns B_TRUE if the fault is resolved.
 *	It returns B_FALSE if the given address is not valid.
 *
 *	Note that hat_pteload_retry() goes panic if the given address
 *	can't be accessed even though it's valid.
 *
 * Remarks:
 *	Currently, hat_pteload_retry() has the folllowing constraints:
 *
 *	  + It treats 4K page only.
 *	  + The given address must be in the kernel space.
 */
boolean_t
hat_pteload_retry(struct regs *rp, caddr_t addr, enum seg_rw rw)
{
	uintptr_t	vaddr = PAGE_ROUNDDOWN(addr), paddr;
	hat_l2pt_t	*l2pt;
	boolean_t	ret = B_FALSE, locked = B_FALSE;
	int		i, write;
	l1pte_t		l1pte;
	l2pte_t		pte;
	hat_t		*curhat;
	cpu_t		*curcpu;
	processorid_t	cpuid;
	volatile l1pte_t	*curl1ptep;
	volatile l1pte_t	*l1ptep;
	volatile l2pte_t	*ptep;

	if (vaddr < KERNELBASE) {
		/* Not kernel space. */
		return B_FALSE;
	}

	kpreempt_disable();
	curcpu = CPU;
	curhat = curcpu->cpu_current_hat;
	cpuid = curcpu->cpu_id;
	if (curhat == NULL) {
		curhat = &hat_kas;
	}

	if (!HAT_KAS_IS_LOCKED()) {
		if (CPU_ON_INTR(curcpu) || getpil() > LOCK_LEVEL) {
			caddr_t	va = (caddr_t)vaddr;

			/*
			 * We can't acquire adaptive mutex here.
			 * In this case, we treat only CPU private mapping.
			 */
			ptep = NULL;
			for (i = 0; i <= max_cpuid; i++) {
				cpu_t	*cp = cpu[i];
			
				if (cp == NULL) {
					continue;
				}
				if (va == cp->cpu_isync_addr) {
					ptep = cp->cpu_isync_pte;
				}
				else if (va == cp->cpu_caddr1) {
					ptep = cp->cpu_caddr1pte;
				}
				else if (va == cp->cpu_caddr2) {
					ptep = cp->cpu_caddr2pte;
				}

				if (ptep != NULL) {
					uint_t	l1idx = L1PT_INDEX(vaddr);

					if ((pte = *ptep) == 0) {
						goto out;
					}
					l1ptep = hat_kas.hat_l1vaddr + l1idx;
					l1pte = *l1ptep;
					curl1ptep =
						HAT_KERN_L1PT(curhat) + l1idx;
					ASSERT(*curl1ptep == l1pte);
					goto docheck;
				}
			}

			/* We can't help this situation... */
			goto out;
		}
		else {
			HAT_KAS_LOCK();
			locked = B_TRUE;
		}
	}

	/* Check whether L1PT entry is valid. */
	l1ptep = hat_kas.hat_l1vaddr + L1PT_INDEX(vaddr);
	l1pte = *l1ptep;

	curl1ptep = HAT_KERN_L1PT(curhat) + L1PT_INDEX(vaddr);
	if (*curl1ptep != l1pte) {
		panic("kernel L1 pte is not consistent: "
		      "master=0x%p:0x%08x current=0x%p:0x%08x",
		      l1ptep, l1pte, curl1ptep, *curl1ptep);
	}

	if (!L1PT_PTE_IS_COARSE(l1pte)) {
		/* No L1PT entry. */
		goto out;
	}

	/* Lookup L2 PTE corresponding to the address. */
	l2pt = hatpt_l2pt_lookup(&hat_kas, vaddr);
	if (l2pt == NULL) {
		/* No L2PT. */
		goto out;
	}
	ptep = l2pt->l2_vaddr + L2PT_INDEX(vaddr);
	if ((pte = *ptep) == 0 || !L2PT_PTE_IS_SMALL(pte)) {
		/* No valid PTE or not small page. */
		goto out;
	}

 docheck:
	if (hat_is_cache_maint_fault(rp)) {
		/*
		 * Although cache maintenance instruction doesn't require
		 * write permission, MPCore reports the fault raised by
		 * a cache maintenance instruction as write fault.
		 * So we treat this fault as read fault.
		 */
		if (!L2PT_PTE_IS_READABLE(pte)) {
			/* Not readable mapping. */
			goto out;
		}
		write = 0;
		atomic_inc_uint(&hat_pteload_cache_retry_count[cpuid]);
	} 
	else if (rw == S_WRITE) {
		if (!L2PT_PTE_IS_WRITABLE(pte)) {
			/* Not writable mapping. */
			goto out;
		}
		write = 1;
	}
	else {
		if (!L2PT_PTE_IS_READABLE(pte)) {
			/* Not readable mapping. */
			goto out;
		}
		write = 0;
	}

	atomic_inc_uint(&hat_pteload_retry_count[cpuid]);

	for (i = 0; i < HAT_PTELOAD_RETRY_COUNT; i++) {
		/* Check whether the specified mapping can be accessed. */
		paddr = VTOP_GET_PADDR(vaddr, write);
		if (!VTOP_ERROR(paddr)) {
			/* Valid. */
			ret = B_TRUE;
			goto out;
		}

		/*
		 * The specified address can't be accessed even though it's
		 * valid. Set pte again.
		 */
		PRM_PRINTF("pteload_retry[%d]: addr=0x%lx, l1pte:0x%p:0x%08x "
			   "pte:0x%p:0x%08x, pa=0x%08lx\n", i, vaddr,
			   l1ptep, *l1ptep, ptep, *ptep, paddr);
		PRM_PRINTF("curl1pte:0x%p:0x%08x\n", curl1ptep, *curl1ptep);
		TLB_FLUSH_VADDR(vaddr);
		HAT_L1PTE_SET(curl1ptep, l1pte);
		HAT_L2PTE_SET(ptep, pte);
		drv_usecwait(TICK_TO_USEC(1));
	}

	/* Give up. */
	panic("hat_pteload_retry: retry over: addr=0x%lx, l1pte:0x%p:0x%08x, "
	      "pte:0x%p:0x%08x, pa=0x%08lx", vaddr, l1ptep, *l1ptep,
	      ptep, *ptep, paddr);

 out:
	if (locked) {
		HAT_KAS_UNLOCK();
	}
	kpreempt_enable();

	return ret;
}
#endif	/* HAT_USE_PTELOAD_RETRY */

/*
 * int
 * hat_softfault(caddr_t addr, enum seg_rw rw, boolean_t usemode,
 *		 boolean_t *userpte)
 *	Handle software fault.
 *
 *	ARM architecture can't detect reference or modification to the
 *	mapping. So we set initial PTE without hardware protection bits.
 *	If an protection failt is raised, hat_softfault() is called and
 *	it maintains software ref/mod bits.
 *
 * Calling/Exit State:
 *	hat_softfault() returns one of the followings:
 *
 *	HAT_SF_HANDLED
 *		The fault is protection fault to maintain ref/mod bits,
 *		and hat_softfault() has handled.
 *		B_TRUE is set in *userpte if the given address is mapped
 *		with user mode attribute.
 *
 *	HAT_SF_HASPERM
 *		The fault is protection fault to maintain ref/mod bits,
 *		and hardware PTE has already appropriate permission.
 *		B_TRUE is set in *userpte if the given address is mapped
 *		with user mode attribute.
 *
 *	HAT_SF_UNCLAIMED
 *		The fault is not ref/mod bit emulation fault.
 *		The value of *userpte is unspecified.
 */
int
hat_softfault(caddr_t addr, enum seg_rw rw, boolean_t usermode,
	      boolean_t *userpte)
{
	uintptr_t	vaddr = PAGE_ROUNDDOWN(addr);
	hat_t		*hat;
	uintptr_t	nextaddr;
	l1pte_t		*l1ptep;
	l2pte_t		*ptep;
	hat_l2pt_t	*l2pt;
	ssize_t		pgsz;
	pfn_t		pfn;
	boolean_t	handled = B_FALSE, hasperm = B_FALSE;
	boolean_t	usermap = B_FALSE;
	int		ret;
	page_t		*syncpp = NULL, *pp;

#ifdef	DEBUG
	uint32_t	x;

	/* Protection fault should not be raised on the interrupt level. */
	x = DISABLE_IRQ_SAVE();
	ASSERT(CPU_ON_INTR(CPU) == 0 && !(x & PSR_I_BIT));
	RESTORE_INTR(x);
#endif	/* DEBUG */

	if (vaddr >= KERNELBASE) {
		hat = &hat_kas;
	}
	else {
		hat = curproc->p_as->a_hat;
	}

	HAT_LOCK(hat);

	nextaddr = vaddr;
	pgsz = hatpt_pte_walk(hat, &nextaddr, &l1ptep, &ptep, &l2pt, &pfn,
			      &pp);
	if (pgsz == -1 || l2pt == NULL) {
		goto out;
	}

	if (pgsz >= L1PT_SECTION_VSIZE) {
		l1pte_t		l1pte = *l1ptep;
		uint32_t	sw;

		if (L1PT_PTE_IS_USER(l1pte)) {
			usermap = B_TRUE;
		}
		else if (usermode) {
			/*
			 * The kernel mapping can't be accessed from
			 * user mode.
			 */
			goto out;
		}
		sw = HAT_L2PT_GET_SECTION_FLAGS(l2pt);
		if (rw == S_EXEC && !SWPTE_IS_EXECUTABLE(sw)) {
			goto out;
		}
		if (rw == S_WRITE && SWPTE_IS_WRITABLE(sw)) {
			if (L1PT_PTE_IS_WRITABLE(l1pte)) {
				hasperm = B_TRUE;
			}
			else {
				/* Detected modification. */
				ASSERT(!SWPTE_IS_NOSYNC(sw));
				L1PT_PTE_SET_MOD(l1pte);
				handled = B_TRUE;
			}
		}
		else if (rw != S_WRITE && SWPTE_IS_READABLE(sw)) {
			if (L1PT_PTE_IS_READABLE(l1pte)) {
				hasperm = B_TRUE;
			}
			else {
				/* Detected reference. */
				ASSERT(!SWPTE_IS_NOSYNC(sw));
				L1PT_PTE_SET_REF(l1pte);
				handled = B_TRUE;
			}
		}

		if (rw == S_EXEC) {
			ASSERT(SWPTE_IS_EXECUTABLE(sw));
			if (L1PT_PTE_IS_EXECUTABLE(l1pte)) {
				hasperm = B_TRUE;
			}
			else {
				/*
				 * Detect first instruction fetch from this
				 * mapping. We need to maintain instruction
				 * cache.
				 */
				ASSERT(!SWPTE_IS_NOSYNC(sw));
				ASSERT(L1PT_PTE_IS_USER(l1pte));
				L1PT_PTE_SET_EXECUTABLE(l1pte);
				handled = B_TRUE;

				/* Sync entire L1 instruction cache. */
				syncpp = HAT_FLUSHTLB_SYNCALL;
			}
		}

		if (handled) {
			if (L1PT_PTE_IS_SPSECTION(l1pte)) {
				HAT_L1PTE_SPSECTION_SET(l1ptep, l1pte);
			}
			else {
				HAT_L1PTE_SET(l1ptep, l1pte);
			}
			HAT_L2PT_SET_SECTION_FLAGS(l2pt, sw);

			hat_flushtlb(hat, nextaddr - pgsz, syncpp);
		}
	}
	else {
		l2pte_t	*swptep, sw, pte;

		ASSERT(ptep);
		pte = *ptep;
		if (L2PT_PTE_IS_USER(pte)) {
			usermap = B_TRUE;
		}
		else if (usermode) {
			/*
			 * The kernel mapping can't be accessed from
			 * user mode.
			 */
			goto out;
		}

		swptep = L2PT_SOFTFLAGS(ptep);
		sw = *swptep;
		if (rw == S_EXEC && !SWPTE_IS_EXECUTABLE(sw)) {
			goto out;
		}
		if (rw == S_WRITE && SWPTE_IS_WRITABLE(sw)) {
			if (L2PT_PTE_IS_WRITABLE(pte)) {
				hasperm = B_TRUE;
			}
			else {
				/* Detected modification. */
				ASSERT(!SWPTE_IS_NOSYNC(sw));
				L2PT_PTE_SET_MOD(pte);
				handled = B_TRUE;
			}
		}
		else if (rw != S_WRITE && SWPTE_IS_READABLE(sw)) {
			if (L2PT_PTE_IS_READABLE(pte)) {
				hasperm = B_TRUE;
			}
			else {
				/* Detected reference. */
				ASSERT(!SWPTE_IS_NOSYNC(sw));
				L2PT_PTE_SET_REF(pte);
				handled = B_TRUE;
			}
		}

		if (rw == S_EXEC) {
			/*
			 * Check whether we should do instruction cache
			 * sync.
			 */
			ASSERT(SWPTE_IS_EXECUTABLE(sw));
			if (L2PT_PTE_IS_SMALL(pte)) {
				if (L2PT_PTE_SMALL_IS_EXECUTABLE(pte)) {
					hasperm = B_TRUE;
				}
				else {
					ASSERT(!SWPTE_IS_NOSYNC(sw));
					ASSERT(L2PT_PTE_IS_USER(pte));
					L2PT_PTE_SMALL_SET_EXECUTABLE(pte);
					handled = B_TRUE;

					/*
					 * Sync L1 instruction cache lines
					 * associated with this page.
					 */
					syncpp = pp;
					ASSERT(syncpp);
				}
			}
			else if (L2PT_PTE_LARGE_IS_EXECUTABLE(pte)) {
				hasperm = B_TRUE;
			}
			else {
				ASSERT(L2PT_PTE_IS_LARGE(pte));
				ASSERT(!SWPTE_IS_NOSYNC(sw));
				ASSERT(L2PT_PTE_IS_USER(pte));
				L2PT_PTE_LARGE_SET_EXECUTABLE(pte);
				handled = B_TRUE;

				/* Sync entire L1 instruction cache. */
				syncpp = HAT_FLUSHTLB_SYNCALL;
			}
		}

		if (handled) {
			if (L2PT_PTE_IS_SMALL(pte)) {
				HAT_L2PTE_SET(ptep, pte);
			}
			else {
				HAT_L2PTE_LARGE_SET(ptep, pte);
			}
			*swptep = sw;

			hat_flushtlb(hat, nextaddr - pgsz, syncpp);
		}
	}

 out:
	if (handled) {
		ret = HAT_SF_HANDLED;
		if (userpte != NULL) {
			*userpte = usermap;
		}
	}
	else if (hasperm) {
		/*
		 * Ref/mod bit emulation must be done by another thread,
		 * and it must have already invalidated old TLB entry.
		 */
		ret = HAT_SF_HASPERM;
		if (userpte != NULL) {
			*userpte = usermap;
		}
	}
	else {
		ret = HAT_SF_UNCLAIMED;
	}

	HAT_UNLOCK(hat);
	return ret;
}

/* Obsolete function */
pfn_t
va_to_pfn(void *vaddr)
{
	panic("va_to_pfn: Obsolete.");
	return PFN_INVALID;
}

/*
 * Unsupported functions on ARM
 */

/* ARGSUSED */
void
hat_join_srd(struct hat *hat, vnode_t *evp)
{
}

static const char	region_panic_msg[] = "No shared region support on ARM";

/* ARGSUSED */
hat_region_cookie_t
hat_join_region(struct hat *hat, caddr_t r_saddr, size_t r_size, void *r_obj,
		u_offset_t r_objoff, uchar_t r_perm, uchar_t r_pgszc,
		hat_rgn_cb_func_t r_cb_function, uint_t flags)
{
	panic(region_panic_msg);
	return HAT_INVALID_REGION_COOKIE;
}

/* ARGSUSED */
void
hat_leave_region(struct hat *hat, hat_region_cookie_t rcookie, uint_t flags)
{
	panic(region_panic_msg);
}

/* ARGSUSED */
void
hat_dup_region(struct hat *hat, hat_region_cookie_t rcookie)
{
	panic(region_panic_msg);
}

/* ARGSUSED */
void
hat_unlock_region(struct hat *hat, caddr_t addr, size_t len,
		  hat_region_cookie_t rcookie)
{
	panic(region_panic_msg);
}
