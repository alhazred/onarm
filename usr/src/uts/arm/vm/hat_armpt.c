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
 * Copyright (c) 2006-2008 NEC Corporation
 * All rights reserved.
 */

#ident	"@(#)arm/vm/hat_armpt.c"

/*
 * Page table management for ARM architecture.
 *
 * This file must be linked to machine-specific kernel object because
 * it includes machine-specific header.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/bootconf.h>
#include <sys/pte.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/prom_debug.h>
#include <sys/mutex.h>
#include <sys/machsystm.h>
#include <sys/ddidmareq.h>
#include <sys/archsystm.h>
#include <sys/var.h>
#include <sys/vmsystm.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <vm/page.h>
#include <vm/vm_dep.h>
#include <vm/hat_arm.h>
#include <vm/hat_armpt.h>
#include <vm/hat_machdep.h>

#ifdef	DEBUG
static int	hatpt_debug;

#define	HATPT_DEBUG(x)				\
	do {					\
		if (hatpt_debug) {		\
			PRM_DEBUG(x);		\
		}				\
	} while (0)

#define	HATPT_PRINTF(...)				\
	do {						\
		if (hatpt_debug) {			\
			PRM_PRINTF(__VA_ARGS__);	\
		}					\
	} while (0)

#else	/* !DEBUG */

#define	HATPT_DEBUG(x)
#define	HATPT_PRINTF(...)

#endif	/* DEBUG */

/*
 * XXX_PREALLOC_XXX:
 *	Number of objects to be allocated at boottime.
 * XXX_RESERVE_XXX:
 *	Number of objects to be reserved in private reserve pool.
 *	The reserved objects will be used when the system is low on memory.
 */
#define	L2PT_PREALLOC_DEFAULT		32
#define	L2PT_PREALLOC_MIN		8
#define	L2PT_PREALLOC_MAX		128
#define	L2PT_RESERVE_DEFAULT		4
#define	L2PT_RESERVE_MIN		0
#define	L2PT_RESERVE_MAX		256
#define	L2BUNDLE_PREALLOC_DEFAULT	16
#define	L2BUNDLE_PREALLOC_MIN		8
#define	L2BUNDLE_PREALLOC_MAX		128
#define	L2BUNDLE_RESERVE_DEFAULT	4
#define	L2BUNDLE_RESERVE_MIN		0
#define	L2BUNDLE_RESERVE_MAX		256

/* L2PT and hat_l2bundle structure for kernel mapping. */
hat_kresv_t	hatpt_kresv_l2pt;
hat_kresv_t	hatpt_kresv_l2bundle;

#define	KRESV_L2PT_ALLOC()					\
	((l2pte_t *)hatpt_kresv_alloc(&hatpt_kresv_l2pt))
#define	KRESV_L2PT_FREE(l2pt)				\
	hatpt_kresv_free(&hatpt_kresv_l2pt, l2pt)

#define	KRESV_L2BUNDLE_ALLOC()					\
	((hat_l2bundle_t *)hatpt_kresv_alloc(&hatpt_kresv_l2bundle))
#define	KRESV_L2BUNDLE_FREE(l2b)				\
	hatpt_kresv_free(&hatpt_kresv_l2bundle, l2b)

/* Caches for user page table. */
static kmem_cache_t	*hatpt_l2ptable_cache;
static kmem_cache_t	*hatpt_l2bundle_cache;

/* Internal prototypes */
static page_t	*hatpt_page_create(uintptr_t vaddr);
static void	hatpt_l2pt_init(hat_t *hat, hat_l2pt_t *l2pt, l2pte_t *l2,
				uint16_t l1idx, uintptr_t vaddr,
				uint32_t domain);

static void	hatpt_kresv_init(hat_kresv_t *krp, const char *prefix,
				 size_t bufsize, size_t bufalign,
				 uint32_t nbuf, uint32_t min_nbuf,
				 uint32_t max_nbuf, uint32_t resv,
				 uint32_t min_resv, uint32_t max_resv);
static boolean_t	hatpt_kresv_grow(hat_kresv_t *krp);
static void	*hatpt_kresv_alloc(hat_kresv_t *krp);
static void	hatpt_kresv_free(hat_kresv_t *krp, void *buf);
static hat_l2bundle_t	*hatpt_kas_l2bundle_prepare(int bindex);
static hat_l2pt_t *	hatpt_kas_l2pt_prepare(uintptr_t vaddr,
					       boolean_t *allocated);
static hat_l2bundle_t	*hatpt_uas_l2bundle_prepare(hat_t *hat, int bindex,
						    hat_l2bundle_t **freel2b);
static void	hatpt_kresv_dump(hat_kresv_t *krp);

/*
 * void
 * hatpt_boot_linkl2pt(caddr_t vaddr, size_t size, boolean_t doalloc)
 *	Prepare L2PT for the specified virtual address.
 *	This function is called at boottime.
 *
 *	If doalloc is true, hatpt_boot_linkl2pt() will allocate L2PT and
 *	bundle structure using BOP_ALLOC().
 *	Otherwise, try to allocate reserved pools.
 *	Exhaustion of reserved pool causes fatal error.
 *
 * Remakrs:
 *	L2PT allocated by hatpt_boot_linkl2pt() is always held to prevent
 *	releasing L2PT.
 */
void
hatpt_boot_linkl2pt(uintptr_t vaddr, size_t size, boolean_t doalloc)
{
	uintptr_t	eaddr;
	hat_t		*hat = &hat_kas;

	ASSERT(vaddr >= KERNELBASE);
	ASSERT(IS_PAGEALIGNED(size));
	ASSERT(hat_use_boot_reserve);

	eaddr = PAGE_ROUNDUP(vaddr + size);
	vaddr = PAGE_ROUNDDOWN(vaddr);
	while (vaddr < eaddr) {
		uintptr_t	nextaddr;
		hat_l2bundle_t	*l2b;
		hat_l2pt_t	*l2pt;
		uint16_t	l1idx = L1PT_INDEX(vaddr);
		int		bidx, l2idx;

		bidx = HAT_L2BD_INDEX(hat, l1idx);
		if ((l2b = hat->hat_l2bundle[bidx]) == NULL) {
			if (doalloc) {
				/* Allocate hat_l2bundle. */
				BOOT_ALLOC(l2b, hat_l2bundle_t *, sizeof(*l2b),
					   BO_NO_ALIGN,
					   "Failed to allocate hat_l2bundle");
			}
			else {
				l2b = KRESV_L2BUNDLE_ALLOC();
			}
			HAT_L2BD_INIT(hat, l2b, bidx);
		}

		l2idx = HAT_L2BD_L2PT_INDEX(l1idx);
		l2pt = &l2b->l2b_l2pt[l2idx];
		if (l2pt->l2_vaddr == NULL) {
			l2pte_t	*l2;
			l1pte_t	*l1ptep, l1pte;

			if (doalloc) {
				/* Allocate L2PT. */
				BOOT_ALLOC(l2, l2pte_t *, L2PT_ALLOC_SIZE,
					   L2PT_ALLOC_SIZE,
					   "Failed to allocate L2PT at 0x%lx",
					   vaddr);
			}
			else {
				l2 = KRESV_L2PT_ALLOC();
			}
			fast_bzero(l2, L2PT_ALLOC_SIZE);
			l2pt->l2_vaddr = l2;
			l2pt->l2_paddr = (uintptr_t)KVTOP_DATA(l2);
			l2pt->l2_l1index = l1idx;
			l2b->l2b_active++;

			HAT_L2PT_HOLD(l2pt);

			/* Set L1PTE. */
			l1ptep = hat->hat_l1vaddr + l1idx;
			ASSERT(*l1ptep == 0);
			l1pte = L1PT_MKL2PT(l2pt->l2_paddr, HAT_DOMAIN_KERNEL);
			HAT_L1PTE_SET(l1ptep, l1pte);
		}

		nextaddr = L1PT_NEXT_VADDR(vaddr);
		if (vaddr >= nextaddr) {
			break;
		}
		vaddr = nextaddr;
	}
}

/*
 * void
 * hatpt_boot_alloc(void)
 *	Allocate static page tables.
 */
void
hatpt_boot_alloc(void)
{
	l1pte_t		*kl1pt;
	uintptr_t	kl1pt_paddr;
	uint32_t	i;
	uintptr_t	l2;

#ifdef	DEBUG
	hatpt_debug = hat_bootparam("hatpt-debug", 0, 0, 1);
#endif	/* DEBUG */

	/* Allocate kernel L1 page table. */
	BOOT_ALLOC(kl1pt, l1pte_t *, L1PT_SIZE, L1PT_SIZE,
		   "Failed to allocate kernel L1PT");
	kl1pt_paddr = KVTOP_DATA((uintptr_t)kl1pt);
	HATPT_DEBUG(kl1pt);
	HATPT_DEBUG(kl1pt_paddr);
	fast_bzero(kl1pt, L1PT_SIZE);

	hat_kas.hat_l1vaddr = kl1pt;
	hat_kas.hat_l1paddr = kl1pt_paddr;

	/* Initialize hat_kresv for kernel L2PT and L2 bundle. */
	hatpt_kresv_init(&hatpt_kresv_l2pt, "l2pt", L2PT_ALLOC_SIZE,
			 L2PT_ALLOC_SIZE, L2PT_PREALLOC_DEFAULT,
			 L2PT_PREALLOC_MIN, L2PT_PREALLOC_MAX,
			 L2PT_RESERVE_DEFAULT, L2PT_RESERVE_MIN,
			 L2PT_RESERVE_MAX);
	hatpt_kresv_init(&hatpt_kresv_l2bundle, "l2bundle",
			 sizeof(hat_l2bundle_t), sizeof(uint32_t),
			 L2BUNDLE_PREALLOC_DEFAULT, L2BUNDLE_PREALLOC_MIN,
			 L2BUNDLE_PREALLOC_MAX, L2BUNDLE_RESERVE_DEFAULT,
			 L2BUNDLE_RESERVE_MIN, L2BUNDLE_RESERVE_MAX);
}

/*
 * void
 * hatpt_init(void)
 *	Initialize page table management layer.
 *	This function initializes kmem caches for page table data.
 */
void
hatpt_init(void)
{
	/*
	 * Initialize kmem caches.
	 * We use memload arena to avoid recursive mutex enter for hment.
	 */

	/* hat_l2bundle for user space. */
	hatpt_l2bundle_cache = kmem_cache_create("hat_l2bundle_t",
						 sizeof(hat_l2bundle_t), 0,
						 NULL, NULL, NULL, NULL, 
						 hat_memload_arena,
						 KMC_NODEBUG);

	/*
	 * hatpt_l2ptable_cache is used to allocate L2PT itself for user
	 * process. Although L2PT must be physically contiguous, we can use
	 * kmem_cache_alloc() under the following constraints:
	 *
	 * - L2PT_ALLOC_SIZE is less than MMU_PAGESIZE.
	 * - L2PT_ALLOC_SIZE is a divisor of MMU_PAGESIZE.
	 *
	 * Set hatpt_l1pt_reap() as reclaim function for this cache so that
	 * user L1PT cache will be shrinked when the system is low on memory.
	 */
	ASSERT(L2PT_ALLOC_SIZE < MMU_PAGESIZE);
	ASSERT((MMU_PAGESIZE % L2PT_ALLOC_SIZE) == 0);
	hatpt_l2ptable_cache = kmem_cache_create("L2_page_table",
						 L2PT_ALLOC_SIZE,
						 L2PT_ALLOC_SIZE,
						 NULL, NULL,
						 HAT_L2PT_RECLAIM_FUNC,
						 NULL, hat_memload_arena,
						 KMC_NODEBUG);

	/* Initialize user L1PT management layer. */
	hatpt_l1pt_init();
}

/*
 * hat_l2pt_t *
 * hatpt_l2pt_lookup(hat_t *hat, uintptr_t vaddr)
 *	Find the hat_l2pt structure at the specified hat and virtual address.
 *	Return NULL if not found.
 */
hat_l2pt_t *
hatpt_l2pt_lookup(hat_t *hat, uintptr_t vaddr)
{
	hat_l2bundle_t	*l2b;
	uint16_t	l1idx = L1PT_INDEX(vaddr);

	ASSERT(hat_use_boot_reserve || HAT_IS_LOCKED(hat));

	if ((l2b = hat->hat_l2bundle[HAT_L2BD_INDEX(hat, l1idx)]) != NULL) {
		hat_l2pt_t	*l2pt;

		l2pt = &l2b->l2b_l2pt[HAT_L2BD_L2PT_INDEX(l1idx)];
		if (l2pt->l2_vaddr != NULL) {
			return l2pt;
		}
	}

	return NULL;
}

/*
 * hat_l2pt_t *
 * hatpt_l1pt_softflags_lookup(hat_t *hat, uintptr_t vaddr, int szc)
 *	Find the hat_l2pt structure that keeps software PTE flags for the
 *	specified virtual address.
 *	
 *	Return NULL if not found.
 */
hat_l2pt_t *
hatpt_l1pt_softflags_lookup(hat_t *hat, uintptr_t vaddr, int szc)
{
	hat_l2bundle_t	*l2b;
	uint16_t	l1idx = L1PT_INDEX(vaddr);

	ASSERT(HAT_IS_LOCKED(hat));
	ASSERT(szc == SZC_SPSECTION || szc == SZC_SECTION);

	if (szc == SZC_SPSECTION) {
		/* Only group leader PTE has software flags. */
		l1idx = P2ALIGN(l1idx, L1PT_SPSECTION_NPTES);
	}
	if ((l2b = hat->hat_l2bundle[HAT_L2BD_INDEX(hat, l1idx)]) != NULL) {
		hat_l2pt_t	*l2pt;

		l2pt = &l2b->l2b_l2pt[HAT_L2BD_L2PT_INDEX(l1idx)];
		if (HAT_L2PT_IS_SECTION(l2pt)) {
			return l2pt;
		}
	}

	return NULL;
}

/*
 * ssize_t
 * hatpt_pte_lookup(hat_t *hat, uintptr_t vaddr, l1pte_t **l1ptepp,
 *		    l2pte_t **ptepp, hat_l2pt_t **l2ptpp)
 *	Find page table entry associated with the specified virtual address.
 *
 * Calling/Exit State:
 *	Upon successful completion, hatpt_pte_lookup() returns pagesize
 *	of mapping at the specified virtual address, and set address of
 *	page table entry to *l1ptepp and *ptepp.
 *	On failure, it returns -1.
 *
 *	The caller must acquire the hat mutex.
 *
 * Remarks:
 *	NULL is set as PTE address if no page table. For instance,
 *	*ptepp will be NULL if the mapping is supersection or section mapping.
 */
ssize_t
hatpt_pte_lookup(hat_t *hat, uintptr_t vaddr, l1pte_t **l1ptepp,
		 l2pte_t **ptepp, hat_l2pt_t **l2ptpp)
{
	uint16_t	l1idx;
	hat_l2pt_t	*l2pt;
	l1pte_t		*l1ptep, l1pte;
	l2pte_t		*ptep = NULL, pte;
	int		l1type;
	ssize_t		pgsz = -1;

	ASSERT(HAT_IS_LOCKED(hat));

	l1idx = L1PT_INDEX(vaddr);
	l1ptep = hat->hat_l1vaddr + l1idx;
	l1pte = *l1ptep;
	l1type = L1PT_PTE_TYPE(l1pte);
	if (l1type == L1PT_TYPE_SECTION) {
		int	szc;

		if (l1pte & L1PT_SPSECTION) {
			/* Supersection */
			pgsz = L1PT_SPSECTION_VSIZE;
			szc = SZC_SPSECTION;
		}
		else {
			/* Section */
			pgsz = L1PT_SECTION_VSIZE;
			szc = SZC_SECTION;
		}

		/* hat_l2pt is used as software flags. */
		if (l2ptpp != NULL) {
			l2pt = hatpt_l1pt_softflags_lookup(hat, vaddr, szc);
			*l2ptpp = l2pt;
		}
		goto out;
	}
	if (l1type != L1PT_TYPE_COARSE) {
		/* Invalid PTE. */
		goto out;
	}

	/* L1PT entry is a link to L2PT. */
	l2pt = hatpt_l2pt_lookup(hat, vaddr);
	if (l2ptpp != NULL) {
		*l2ptpp = l2pt;
	}
	if (l2pt == NULL) {
		/* No L2PT for this address. */
		goto out;
	}

	ptep = l2pt->l2_vaddr + L2PT_INDEX(vaddr);
	pte = *ptep;
	if (L2PT_PTE_IS_INVALID(pte)) {
		/* Invalid PTE. */
		goto out;
	}

	*((l2pte_t **)ptepp) = ptep;
	if (L2PT_PTE_IS_SMALL(pte)) {
		/* Small page */
		pgsz = MMU_PAGESIZE;
	}
	else {
		/* Large page */
		ASSERT(L2PT_PTE_IS_LARGE(pte));
		pgsz = L2PT_LARGE_VSIZE;
	}

 out:
	*l1ptepp = l1ptep;
	*ptepp = ptep;

	return pgsz;
}

/*
 * ssize_t
 * hatpt_pte_walk(hat_t *hat, uintptr_t *vaddrp, l1pte_t **l1ptepp,
 *		  l2pte_t **ptepp, hat_l2pt_t **l2ptpp, pfn_t *pfnp,
 *		  page_t **ppp)
 *	Examine PTE at the given virtual address.
 *
 * 	This is common code to walk through page table entries, such as
 *	hat_sync(). hatpt_pte_walk() set address of L1PT, L2PT entry,
 *	and page frame number and page structure associated with the
 *	given virtual address.
 *
 * Calling/Exit State:
 *	Return pagesize of mapping if the specified address has a valid
 *	mapping. Return -1 if no mapping.
 *
 *	hatpt_pte_walk() updates *vaddrp as address to be examined in
 *	next loop.
 *
 *	The caller must acquire the hat mutex.
 */
ssize_t
hatpt_pte_walk(hat_t *hat, uintptr_t *vaddrp, l1pte_t **l1ptepp,
	       l2pte_t **ptepp, hat_l2pt_t **l2ptpp, pfn_t *pfnp, page_t **ppp)
{
	uintptr_t	vaddr, nextaddr, paddr;
	l1pte_t		*l1ptep;
	l2pte_t		*ptep, *swptep;
	hat_l2pt_t	*l2pt, *swflags;
	ssize_t		pgsz;
	page_t		*pp;
	pfn_t		pfn;

	ASSERT(HAT_IS_LOCKED(hat));

	vaddr = *vaddrp;
	l1ptep = NULL;
	ptep = NULL;
	l2pt = NULL;
	pfn = PFN_INVALID;
	pp = NULL;

	pgsz = hatpt_pte_lookup(hat, vaddr, &l1ptep, &ptep, &l2pt);
	if (pgsz == -1) {
		if (L1PT_PTE_IS_COARSE(*l1ptep)) {
			/* No L2PT entry. */
			nextaddr = vaddr + MMU_PAGESIZE;
		}
		else {
			/* No L2PT. */
			nextaddr = L1PT_NEXT_VADDR(vaddr);
		}
		goto out;
	}

	/* We must round up virtual address to leader page. */
	if (pgsz != MMU_PAGESIZE) {
		ssize_t	sz;

		vaddr = P2ALIGN(vaddr, pgsz);
		sz = hatpt_pte_lookup(hat, vaddr, &l1ptep, &ptep, &l2pt);
		ASSERT(sz == pgsz);
	}

	nextaddr = vaddr + pgsz;

	if (pgsz >= L1PT_SECTION_VSIZE) {
		l1pte_t	l1pte = *l1ptep;

		paddr = (uintptr_t)l1pte;
		if (pgsz == L1PT_SPSECTION_VSIZE) {
			paddr &= L1PT_SPSECTION_ADDRMASK;
		}
		else {
			paddr &= L1PT_SECTION_ADDRMASK;
		}
	}
	else {
		l2pte_t	pte;

		ASSERT(ptep);
		pte = *ptep;
		paddr = (uintptr_t)pte;
		if (pgsz == L2PT_LARGE_VSIZE) {
			paddr &= L2PT_LARGE_ADDRMASK;
		}
		else {
			paddr &= L2PT_SMALL_ADDRMASK;
		}
	}

	pfn = mmu_btop(paddr);
	if (ppp != NULL) {
		pp = page_numtopp_nolock(pfn);
	}

 out:
	*vaddrp = nextaddr;
	*l1ptepp = l1ptep;
	*ptepp = ptep;
	*l2ptpp = l2pt;
	*pfnp = pfn;
	if (ppp != NULL) {
		*ppp = pp;
	}
	return pgsz;
}

/*
 * static page_t *
 * hatpt_page_create(uintptr_t vaddr)
 *	Allocate one page for kernel L2PT use.
 */
static page_t *
hatpt_page_create(uintptr_t vaddr)
{
	struct seg	kseg;
	page_t		*pp;

	ASSERT(!HAT_KAS_IS_LOCKED());

	if (page_resv(1, KM_NOSLEEP) == 0) {
		return NULL;
	}

	kseg.s_as = &kas;
	pp = page_create_va(&kvp, (u_offset_t)vaddr, MMU_PAGESIZE,
			    PG_EXCL|PG_NORELOC, &kseg, (caddr_t)vaddr);
	if (pp == NULL) {
		page_unresv(1);
	}

	return pp;
}

#define	CHECK_L2PT(l2pt, vaddr)						\
	do {								\
		if (HAT_L2PT_IS_SECTION(l2pt)) {			\
			panic("Section mapping is active: vaddr=0x%08lx, " \
			      "l2pt=0x%p", (vaddr), (l2pt));		\
		}							\
	} while (0)

/*
 * static void
 * hatpt_l2pt_init(hat_t *hat, hat_l2pt_t *l2pt, l2pte_t *l2, uint16_t l1idx,
 * 		   uintptr_t vaddr, uint32_t domain)
 *	This function is used to initialize L2 page table that has been
 *	newly allocated.
 */
static void
hatpt_l2pt_init(hat_t *hat, hat_l2pt_t *l2pt, l2pte_t *l2, uint16_t l1idx,
		uintptr_t vaddr, uint32_t domain)
{
	l1pte_t		*l1ptep, l1pte;
	pfn_t		pfn;
	uintptr_t	off;
	hat_l2bundle_t	*l2b;

	ASSERT(HAT_IS_LOCKED(hat));
	ASSERT(IS_P2ALIGNED(l2, L2PT_ALLOC_SIZE));

	kpreempt_disable();

	/* Zero out L2PT. */
	fast_bzero(l2, L2PT_ALLOC_SIZE);
	PTE_SYNC_RANGE(l2, L2PT_PTE_SIZE, L2PT_NPTES);

	pfn = hat_getkpfnum((caddr_t)l2);
	ASSERT(pfn != PFN_INVALID);
	off = (uintptr_t)l2 & MMU_PAGEOFFSET;

	CHECK_L2PT(l2pt, vaddr);
	l2pt->l2_vaddr = l2;
	l2pt->l2_paddr = mmu_ptob(pfn) + off;
	l2pt->l2_l1index = l1idx;

	l2b = HAT_L2PT_TO_BUNDLE(l2pt);
	l2b->l2b_active++;

	/* Set L1PTE. */
	l1ptep = hat->hat_l1vaddr + l1idx;
	if (*l1ptep != 0) {
		panic("hatpt_l2pt_init: changing mapping: l1idx=%d, "
		      "l1ptep=0x%p, l1pte=0x%08x",
		      l1idx, l1ptep, *l1ptep);
	}
	l1pte = L1PT_MKL2PT(l2pt->l2_paddr, domain);
	HAT_L1PTE_SET(l1ptep, l1pte);
	kpreempt_enable();
}

#define	MAX_PROPNAMELEN		32

/*
 * static void
 * hatpt_kresv_init(hat_kresv_t *krp, const char *prefix,
 *		    size_t bufsize, size_t bufalign,
 *		    uint32_t nbuf, uint32_t min_nbuf, uint32_t max_nbuf,
 *		    uint32_t resv, uint32_t min_resv, uint32_t max_resv)
 *	Initialize hat_kresv structure.
 *	Derive number of buffers to be allocated from boot property,
 *	and pre-allocate buffer.
 *
 *	bufalign must be a power of 2 or zero.
 */
static void
hatpt_kresv_init(hat_kresv_t *krp, const char *prefix,
		 size_t bufsize, size_t bufalign,
		 uint32_t nbuf, uint32_t min_nbuf, uint32_t max_nbuf,
		 uint32_t resv, uint32_t min_resv, uint32_t max_resv)
{
	char	prop[MAX_PROPNAMELEN];
	int	i;

	ASSERT(bufsize > sizeof(hat_kresv_buf_t) && bufsize <= MMU_PAGESIZE);
	ASSERT(bufalign <= MMU_PAGESIZE);

	/* Derive amount of boottime reservation. */
	snprintf(prop, MAX_PROPNAMELEN, "%s-prealloc", prefix);
	krp->hk_prealloc = hat_bootparam(prop, nbuf, min_nbuf, max_nbuf);
	HATPT_PRINTF("prefix = \"%s\"\n", prefix);
	HATPT_DEBUG(krp->hk_prealloc);

	/* Derive number of buffers to be reserved. */
	snprintf(prop, MAX_PROPNAMELEN, "%s-reserve", prefix);
	krp->hk_reserve = hat_bootparam(prop, resv, min_resv, max_resv);
	HATPT_DEBUG(krp->hk_reserve);

	krp->hk_bufsize = bufsize;
	krp->hk_bufalign = bufalign;
	HATPT_DEBUG(krp->hk_bufsize);

	/* Allocate static buffers. */
	for (i = 0; i < krp->hk_prealloc; i++) {
		hat_kresv_buf_t	*buf;

		BOOT_ALLOC(buf, hat_kresv_buf_t	*, bufsize, bufalign,
			   "Failed allocate buffer for \"%s\"", prefix);
		HATPT_DEBUG(buf);

		hatpt_kresv_free(krp, buf);
	}
	ASSERT(krp->hk_nfree == krp->hk_prealloc);
}

/*
 * void
 * hatpt_kresv_spaceinit(hat_kresv_t *krp, uintptr_t vaddr, size_t size)
 *	Initialize virtual space configuration in hat_kresv structure.
 */
void
hatpt_kresv_spaceinit(hat_kresv_t *krp, uintptr_t vaddr, size_t size)
{
	ASSERT(IS_PAGEALIGNED(vaddr));
	ASSERT(IS_PAGEALIGNED(size));

	krp->hk_addr = vaddr;
	krp->hk_size = size;
	krp->hk_brk = vaddr;

	/* Prepare L2PT for reserved space. */
	hatpt_boot_linkl2pt(vaddr, size, B_FALSE);
}

/*
 * static boolean_t
 * hatpt_kresv_grow(hat_kresv_t *krp)
 *	Grow break value of the specified space.
 *
 * Calling/Exit State:
 *	Upon successful completion, hatpt_kresv_grow() returns B_TRUE.
 *	Otherwise B_FALSE.
 *
 *	The caller must acquire the kernel hat mutex.
 *
 * Remarks:
 *	hatpt_kresv_grow() releases the kernel hat mutex.
 *	The caller must check again status serialized by kernel hat mutex.
 *
 *	hatpt_kresv_grow() maps a page without unlocking page lock acquired
 *	by page_create_va(). This would prevent unexpected I/O to kernel
 *	L2PT and L2 bundle page.
 */
static boolean_t
hatpt_kresv_grow(hat_kresv_t *krp)
{
	uintptr_t	limit, vaddr, vpage;
	page_t		*pp;
	hat_l2pt_t	*l2pt;
	l2pte_t		*ptep, pte;
	size_t		align;

	ASSERT(!hat_use_boot_reserve);
	ASSERT(HAT_KAS_IS_LOCKED());

	limit = krp->hk_addr + krp->hk_size - MMU_PAGESIZE;

	if (krp->hk_brk > limit) {
		/* No virtual space is available. */
		return B_FALSE;
	}

	/* Allocate page for backing store. */
	vaddr = PAGE_ROUNDUP(krp->hk_brk);
	HAT_KAS_UNLOCK();
	if ((pp = hatpt_page_create(vaddr)) == NULL) {
		HAT_KAS_LOCK();
		return B_FALSE;
	}
	ASSERT(PAGE_EXCL(pp));
	HAT_KAS_LOCK();

	/* We must check again the virtual address to map. */
	vaddr = (volatile uintptr_t)krp->hk_brk;
	if (vaddr > limit) {
		HAT_KAS_UNLOCK();
		page_free(pp, 1);
		page_unresv(1);
		HAT_KAS_LOCK();
		return B_FALSE;
	}
	vpage = PAGE_ROUNDUP(vaddr);

	/* Create mapping. */
	l2pt = hatpt_l2pt_lookup(&hat_kas, vpage);
	ASSERT(l2pt);
	ptep = l2pt->l2_vaddr + L2PT_INDEX(vpage);
	ASSERT(*ptep == 0);
	pte = hat_l2pt_mkpte(pp->p_pagenum,
			     PROT_READ|PROT_WRITE|HAT_STORECACHING_OK,
			     HAT_LOAD_NOCONSIST, 0, L2PT_SOFTFLAGS(ptep));
	HAT_L2PTE_SET(ptep, pte);
	PGCNT_INC(&hat_kas, 0);
	HAT_L2PT_INC(l2pt);

	/* Create buffers and link them to the pool. */
	limit = vpage + MMU_PAGESIZE;
	align = krp->hk_bufalign;
	while (vaddr + krp->hk_bufsize <= limit) {
		hat_kresv_buf_t	*buf = (hat_kresv_buf_t *)vaddr;

		hatpt_kresv_free(krp, buf);
		vaddr += krp->hk_bufsize;
		if (align > 0) {
			vaddr = P2ROUNDUP(vaddr, align);
		}
	}

	/* Update break value. */
	krp->hk_brk = vaddr;
	return B_TRUE;
}

/*
 * static void *
 * hatpt_kresv_alloc(hat_kresv_t *krp)
 *	Allocate one buffer from the specified hat_kresv structure.
 *
 * Calling/Exit State:
 *	The caller must acquire the kernel hat mutex.
 *
 * Remarks:
 *	This function may release the kernel hat mutex.
 *	The caller must check again status serialized by kernel hat mutex.
 */
static void *
hatpt_kresv_alloc(hat_kresv_t *krp)
{
	hat_kresv_buf_t	*buf = NULL;
	int	try = 0;

	ASSERT(hat_use_boot_reserve || HAT_KAS_IS_LOCKED());

	while (1) {
		if (krp->hk_free != NULL) {
			/* Allocate one buffer. */
			buf = krp->hk_free;
			krp->hk_free = buf->hkb_next;
			krp->hk_nfree--;
			break;
		}
		if (try) {
			/* Failed to grow page. */
			break;
		}

		/* No buffer on freelist. Grow buffer page. */
		if (hat_use_boot_reserve) {
			panic("No pre-allocated buffer. hat_kresv = 0x%p",
			      krp);
		}
		hatpt_kresv_grow(krp);
		try++;
	}

	/* Try to keep at least hk_reserve buffers. */
	if (!hat_use_boot_reserve) {
		while (krp->hk_nfree < krp->hk_reserve) {
			if (!hatpt_kresv_grow(krp)) {
				break;
			}
		}
	}

	return (void *)buf;
}

/*
 * static void
 * hatpt_kresv_free(hat_kresv_t *krp, void *buf)
 *	Free buffer allocated by hatpt_kresv_alloc().
 *	This function just puts the given buffer into freelist in hat_kresv.
 *
 * Calling/Exit State:
 *	The caller must acquire the kernel hat mutex.
 */
static void
hatpt_kresv_free(hat_kresv_t *krp, void *buf)
{
	hat_kresv_buf_t	*kbuf = (hat_kresv_buf_t *)buf;

	ASSERT(hat_use_boot_reserve || HAT_KAS_IS_LOCKED());

	kbuf->hkb_next = krp->hk_free;
	krp->hk_free = kbuf;
	krp->hk_nfree++;
}

/*
 * static hat_l2bundle_t *
 * hatpt_kas_l2bundle_prepare(int bindex)
 *	Prepare hat_l2bundle strucure at the specified index for kernel
 *	mapping.
 *
 * Calling/Exit State:
 *	If hat_l2bundle already exists at the specified index in hat_kas,
 *	return it. 
 *	If it doesn't exist, create it and return it.
 *
 *	The caller must acquire the kernel hat mutex.
 *
 * Remarks:
 *	This function may release the kernel hat mutex.
 *	The caller must check again status serialized by kernel hat mutex.
 */
static hat_l2bundle_t *
hatpt_kas_l2bundle_prepare(int bindex)
{
	hat_t		*hat = &hat_kas;
	hat_l2bundle_t	*l2b;
	int		warn = 0;

	ASSERT(hat_use_boot_reserve || HAT_KAS_IS_LOCKED());

	while (1) {
		if ((l2b = hat->hat_l2bundle[bindex]) != NULL) {
			break;
		}

		/* At first, try to allocate from hat_kresv pool. */
		l2b = KRESV_L2BUNDLE_ALLOC();
		if (l2b != NULL) {
			/*
			 * We must check status again because hat_kas mutex may
			 * be released.
			 */
			if (hat->hat_l2bundle[bindex] != NULL) {
				/* Lost the race. */
				KRESV_L2BUNDLE_FREE(l2b);
				l2b = hat->hat_l2bundle[bindex];
			}
			else {
				HAT_L2BD_INIT(hat, l2b, bindex);
			}
			break;
		}

		/*
		 * Spin here because the system is very low on memory.
		 * Consider increase "l2bundle-prealloc" or
		 * "l2bundle-reserve" value.
		 */
		if (!warn) {
			cmn_err(CE_WARN, "Failed to allocate hat_l2bundle for "
				"kernel mapping.\n");
			warn++;
		}
		HAT_KAS_UNLOCK();

		/* Kick kmem_reap() to shrink kmem caches. */
		ASSERT(hat_can_steal_post_boot);
		kmem_reap();

		tenmicrosec();
		HAT_KAS_LOCK();
	}
	return l2b;
}

/*
 * static hat_l2pt_t *
 * hatpt_kas_l2pt_prepare(uintptr_t vaddr, boolean_t *allocated)
 *	Prepare L2PT for the specified virtual address.
 *	This function is used for kernel mapping only.
 *
 * Calling/Exit State:
 *	If L2PT for the specified address already exists, set B_FALSE into
 *	*allocated and return L2PT.
 *	If it doesn't exist, set B_TRUE into *allocated, then create L2PT
 *	and return it. 
 *
 *	The caller must acquire the kernel hat mutex.
 *
 * Remarks:
 *	This function may release the kernel hat mutex.
 *	The caller must check again status serialized by kernel hat mutex.
 *
 *	This function hold hat_l2pt structure.
 *	The caller is responsible to call HAT_L2PT_RELE().
 */
static hat_l2pt_t *
hatpt_kas_l2pt_prepare(uintptr_t vaddr, boolean_t *allocated)
{
	hat_t		*hat = &hat_kas;
	hat_l2pt_t	*l2pt;
	hat_l2bundle_t	*l2b;
	uint16_t	l1idx;
	int		bidx, l2idx, warn = 0;
	boolean_t	alloc = B_FALSE;

	ASSERT(HAT_KAS_IS_LOCKED());
	ASSERT(vaddr >= KERNELBASE);

	/* Prepare hat_l2bundle. */
	l1idx = L1PT_INDEX(vaddr);
	bidx = HAT_L2BD_INDEX(hat, l1idx);
	l2b = hatpt_kas_l2bundle_prepare(bidx);

	/* Prepare L2PT. */
	l2idx = HAT_L2BD_L2PT_INDEX(l1idx);
	l2pt = &l2b->l2b_l2pt[l2idx];

	while (1) {
		l2pte_t	*l2;

		if (l2pt->l2_vaddr != NULL) {
			break;
		}

		/* At first, try to allocate from hat_kresv pool. */
		l2 = KRESV_L2PT_ALLOC();
		if (l2 != NULL) {
			/*
			 * We must check status again because hat_kas mutex may
			 * be released.
			 */
			if (l2pt->l2_vaddr != NULL) {
				/* Lost the race. */
				KRESV_L2PT_FREE(l2);
			}
			else {
				hatpt_l2pt_init(hat, l2pt, l2, l1idx, vaddr,
						HAT_DOMAIN_KERNEL);
				alloc = B_TRUE;
			}
			break;
		}

		/*
		 * Spin here because the system is very low on memory.
		 * Consider increase "l2pt-prealloc" or "l2pt-reserve" value.
		 */
		if (!warn) {
			cmn_err(CE_WARN, "Failed to allocate L2PT for "
				"kernel mapping.\n");
			warn++;
		}
		HAT_KAS_UNLOCK();

		/* Kick kmem_reap() to shrink kmem caches. */
		ASSERT(hat_can_steal_post_boot);
		kmem_reap();

		tenmicrosec();
		HAT_KAS_LOCK();
	}

	*allocated = alloc;
	HAT_L2PT_HOLD(l2pt);
	return l2pt;
}

/*
 * static hat_l2bundle_t *
 * hatpt_uas_l2bundle_prepare(hat_t *hat, int bindex, hat_l2bundle_t **freel2b)
 *	Prepare hat_l2bundle strucure at the specified index for user
 *	process mapping.
 *
 * Calling/Exit State:
 *	If hat_l2bundle already exists at the specified index in the
 *	specified hat, return it. 
 *	If it doesn't exist, create it and return it.
 *
 *	This function may set hat_l2bundle address to *freel2b.
 *	The caller must free it if *freel2b is not NULL.
 *
 *	The caller must acquire the hat mutex.
 *
 * Remarks:
 *	This function may release the hat mutex.
 *	The caller must check again status serialized by hat mutex.
 */
static hat_l2bundle_t *
hatpt_uas_l2bundle_prepare(hat_t *hat, int bindex, hat_l2bundle_t **freel2b)
{
	hat_l2bundle_t	*l2b, *fl2b = NULL;

	ASSERT(HAT_IS_LOCKED(hat));

	if ((l2b = hat->hat_l2bundle[bindex]) == NULL) {
		/* Allocate new hat_l2bundle. */
		HAT_UNLOCK(hat);
		l2b = kmem_cache_alloc(hatpt_l2bundle_cache, KM_SLEEP);
		HAT_LOCK(hat);

		/*
		 * We must check status again because hat mutex has been
		 * released.
		 */
		if (hat->hat_l2bundle[bindex] != NULL) {
			/* Lost the race. */
			fl2b = l2b;
			l2b = hat->hat_l2bundle[bindex];
		}
		else {
			HAT_L2BD_INIT(hat, l2b, bindex);
		}
	}

	*freel2b = fl2b;
	return l2b;
}

/*
 * hat_l2pt_t *
 * hatpt_l2pt_prepare(hat_t *hat, uintptr_t vaddr, boolean_t *allocated)
 *	Prepare L2PT for the specified virtual address.
 *
 * Calling/Exit State:
 *	If L2PT for the specified address already exists, set B_FALSE into
 *	*allocated and return L2PT.
 *	If it doesn't exist, set B_TRUE into *allocated, then create L2PT
 *	and return it. 
 *
 *	The caller must acquire the hat mutex.
 *
 * Remarks:
 *	This function may release the hat mutex.
 *	The caller must check again status serialized by hat mutex.
 *
 *	This function hold hat_l2pt structure.
 *	The caller is responsible to call HAT_L2PT_RELE().
 */
hat_l2pt_t *
hatpt_l2pt_prepare(hat_t *hat, uintptr_t vaddr, boolean_t *allocated)
{
	uint16_t	l1idx;
	int		bidx, l2idx;
	hat_l2bundle_t	*l2b, *freel2b;
	hat_l2pt_t	*l2pt;
	l2pte_t		*l2, *freel2 = NULL;
	boolean_t	alloc = B_FALSE;

	if (HAT_IS_KERNEL(hat)) {
		return hatpt_kas_l2pt_prepare(vaddr, allocated);
	}

	ASSERT(HAT_IS_LOCKED(hat));

	/* Prepare hat_l2bundle. */
	l1idx = L1PT_INDEX(vaddr);
	bidx = HAT_L2BD_INDEX(hat, l1idx);
	l2b = hatpt_uas_l2bundle_prepare(hat, bidx, &freel2b);

	/* Prepare L2PT. */
	l2idx = HAT_L2BD_L2PT_INDEX(l1idx);
	l2pt = &l2b->l2b_l2pt[l2idx];

	if (l2pt->l2_vaddr == NULL) {
		/*
		 * Bump up l2b_active to prevent unexpected release of
		 * this l2bundle.
		 */
		l2b->l2b_active++;

		/* Allocate new L2PT. */
		HAT_UNLOCK(hat);
		l2 = kmem_cache_alloc(hatpt_l2ptable_cache, KM_SLEEP);
		HAT_LOCK(hat);

		/*
		 * We must check status again because hat mutex has been
		 * released.
		 */
		if (l2pt->l2_vaddr != NULL) {
			freel2 = l2;
		}
		else {
			hatpt_l2pt_init(hat, l2pt, l2, l1idx, vaddr,
					HAT_DOMAIN_USER);
			alloc = B_TRUE;
		}
		l2b->l2b_active--;
	}
	HAT_L2PT_HOLD(l2pt);

	if (freel2b || freel2) {
		HAT_UNLOCK(hat);
		if (freel2b) {
			kmem_cache_free(hatpt_l2bundle_cache, freel2b);
		}
		if (freel2) {
			kmem_cache_free(hatpt_l2ptable_cache, freel2);
		}
		HAT_LOCK(hat);
	}

	*allocated = alloc;
	return l2pt;
}

/*
 * hat_l2pt_t *
 * hatpt_l1pt_softflags_prepare(hat_t *hat, uintptr_t vaddr)
 *	Prepare L1PT software flags room.
 *	This function allocates hat_l2bundle structure, and use it as
 *	room for software flag.
 *
 * Calling/Exit State:
 *	hatpt_l1pt_softflags_prepare() returns hat_l2pt structure to store
 *	software flags for L1PT entry. HAT_L2PT_SET_SECTION_FLAGS() and
 *	HAT_L2PT_GET_SECTION_FLAGS() can be used to set or get software flags.
 *
 * Remarks:
 *	This function may release the hat mutex.
 *	The caller must check again status serialized by hat mutex.
 *
 *	This function hold hat_l2pt structure.
 *	The caller is responsible to call HAT_L2PT_RELE() and
 *	hatpt_l2pt_release().
 */
hat_l2pt_t *
hatpt_l1pt_softflags_prepare(hat_t *hat, uintptr_t vaddr)
{
	hat_l2bundle_t	*l2b, *freel2b = NULL;
	hat_l2pt_t	*l2pt;
	uint16_t	l1idx;
	int		bidx, l2idx;

	ASSERT(HAT_IS_LOCKED(hat));

	l1idx = L1PT_INDEX(vaddr);
	bidx = HAT_L2BD_INDEX(hat, l1idx);
	if (HAT_IS_KERNEL(hat)) {
		ASSERT(vaddr >= KERNELBASE);
		l2b = hatpt_kas_l2bundle_prepare(bidx);
	}
	else {
		ASSERT(vaddr < USERLIMIT);
		l2b = hatpt_uas_l2bundle_prepare(hat, bidx, &freel2b);
	}

	l2idx = HAT_L2BD_L2PT_INDEX(l1idx);
	l2pt = &l2b->l2b_l2pt[l2idx];
	if (l2pt->l2_vaddr) {
		panic("hatpt_l1pt_softflags_prepare: L2PT is active: "
		      "vaddr=0x%08lx, l2pt=0x%p", vaddr, l2pt);
	}
	if (!HAT_L2PT_IS_SECTION(l2pt)) {
		ASSERT(l2pt->l2_nptes == 0);

		l2b->l2b_active++;
		HAT_L2PT_SET_SECTION_FLAGS(l2pt, 0);
	}

	if (freel2b) {
		HAT_UNLOCK(hat);
		kmem_cache_free(hatpt_l2bundle_cache, freel2b);
		HAT_LOCK(hat);
	}

	HAT_L2PT_HOLD(l2pt);

	return l2pt;
}

/*
 * boolean_t
 * hatpt_l2pt_release(hat_l2bundle_t *l2b, int l1index, hat_l2pt_t *l2pt)
 *	Release unused L2PT.
 *
 * Calling/Exit State:
 *	If hatpt_l2pt_release() unmap L1PT entry, it returns B_TRUE.
 *	Otherwise returns B_FALSE.
 *
 *	The caller must acquire the hat mutex.
 *	Although hatpt_l2pt_release() may release the hat mutex,
 *	it returns with holding mutex.
 *
 * Remarks:
 *	hat_l2bundle structure for kernel is never released.
 *
 *	If the specified hat is kernel hat, hatpt_l2pt_release() only
 *	changes kernel hat. It's up to the caller to sync kernel mapping
 *	changes to all user hat structures.
 */
boolean_t
hatpt_l2pt_release(hat_l2bundle_t *l2b, int l1index, hat_l2pt_t *l2pt)
{
	hat_t		*hat;
	int		khat;
	hat_l2bundle_t	*freel2b = NULL;
	l2pte_t		*freel2 = NULL;
	boolean_t	unloaded = B_FALSE;

	ASSERT(l2b != NULL);
	ASSERT(l2pt != NULL);
	hat = l2b->l2b_hat;
	khat = HAT_IS_KERNEL(hat);
	ASSERT(HAT_IS_LOCKED(hat));
	ASSERT(HAT_L2PT_TO_BUNDLE(l2pt) == l2b);

	if (!HAT_L2PT_IS_HELD(l2pt)) {
		if (HAT_L2PT_IS_SECTION(l2pt)) {
			/* Release software flags for L1PT entry. */
			ASSERT(l2b->l2b_active > 0);
			l2b->l2b_active--;
			HAT_L2PT_RELE_SECTION_FLAGS(l2pt);
		}
		else if (l2pt->l2_nptes == 0 && l2pt->l2_vaddr != NULL) {
			l1pte_t		*l1ptep = hat->hat_l1vaddr + l1index;
			l2pte_t		*l2;

			/* Release L2PT. */
			ASSERT(l2b->l2b_active > 0);
			l2b->l2b_active--;
			l2 = l2pt->l2_vaddr;

			/* Invalidate hat_l2pt structure. */
			l2pt->l2_vaddr = NULL;
			l2pt->l2_paddr = NULL;

			HAT_L1PTE_SET(l1ptep, 0);
			unloaded = B_TRUE;

			if (khat) {
				KRESV_L2PT_FREE(l2);
			}
			else {
				freel2 = l2;
			}
		}
	}
	if (!khat && l2b->l2b_active == 0) {
		/*
		 * Release user L2 bundle.
		 * Kernel L2 bundle is never released.
		 */
		ASSERT(hat->hat_l2bundle[l2b->l2b_index] == l2b);
		hat->hat_l2bundle[l2b->l2b_index] = NULL;
		freel2b = l2b;
	}

	if (freel2b || freel2) {
		HAT_UNLOCK(hat);
		if (freel2b) {
			kmem_cache_free(hatpt_l2bundle_cache, freel2b);
		}
		if (freel2) {
			kmem_cache_free(hatpt_l2ptable_cache, freel2);
		}
		HAT_LOCK(hat);
	}

	return unloaded;
}

/*
 * void
 * hatpt_dump(struct hat *hat)
 *	Dump all page tables corresponding to the specified hat into
 *	the system dump.
 */
void
hatpt_dump(struct hat *hat)
{
	pgcnt_t	pgoff;
	pfn_t	pfn;
	int	bidx;
	boolean_t	khat = HAT_IS_KERNEL(hat);
	size_t	l1pt_size = (khat) ? L1PT_SIZE : L1PT_USER_SIZE;

	/* Dump L1PT. */
	HAT_LOCK(hat);
	pfn = mmu_btop(hat->hat_l1paddr);
	for (pgoff = 0; pgoff < mmu_btopr(l1pt_size); pgoff++, pfn++) {
		dump_page(pfn);
	}

	/* Dump L2PT. */
	for (bidx = 0; bidx < hat->hat_numbundle; bidx++) {
		hat_l2bundle_t	*l2b = hat->hat_l2bundle[bidx];
		hat_l2pt_t	*l2pt;

		if (l2b == NULL) {
			continue;
		}

		for (l2pt = &l2b->l2b_l2pt[0];
		     l2pt < &l2b->l2b_l2pt[L2BD_SIZE]; l2pt++) {
			if (HAT_L2PT_IS_SECTION(l2pt) ||
			    l2pt->l2_vaddr == NULL || l2pt->l2_paddr == NULL) {
				continue;
			}
			dump_page(mmu_btop(l2pt->l2_paddr));
		}
	}

	if (khat) {
		/* Dump kresv free pages. */
		hatpt_kresv_dump(&hatpt_kresv_l2pt);
		hatpt_kresv_dump(&hatpt_kresv_l2bundle);
	}

	HAT_UNLOCK(hat);
}

/*
 * static void
 * hatpt_kresv_dump(hat_kresv_t *krp)
 *	Dump all free pages linked to the given hat_kresv structure.
 */
static void
hatpt_kresv_dump(hat_kresv_t *krp)
{
	uintptr_t	addr;

	for (addr = krp->hk_addr; addr < krp->hk_brk; addr += MMU_PAGESIZE) {
		pfn_t	pfn;

		pfn = hat_getkpfnum((caddr_t)addr);
		if (pfn != PFN_INVALID) {
			dump_page(pfn);
		}
	}
}
