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

#ident	"@(#)arm/vm/hment.c"

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/atomic.h>
#include <sys/bitmap.h>
#include <sys/systm.h>
#include <sys/bootconf.h>
#include <sys/prom_debug.h>
#include <sys/archsystm.h>
#include <sys/machsystm.h>
#include <sys/cmn_err.h>
#include <sys/lpg_config.h>
#include <vm/seg_kmem.h>
#include <vm/hat.h>
#include <vm/hat_arm.h>
#include <vm/vm_dep.h>
#include <vm/hment.h>

/*
 * Value returned by hment_walk() when dealing with a single mapping
 * embedded in the page_t.
 */
#define	HMENT_EMBEDDED ((hment_t *)(uintptr_t)1)

kmem_cache_t *hment_cache;

/*
 * XXX_PREALLOC_XXX:
 *	Number of objects to be allocated at boottime.
 * XXX_RESERVE_XXX:
 *	Number of objects to be reserved in private reserve pool.
 *	The reserved objects will be used when the system is low on memory.
 */
#define	HMENT_PREALLOC_DEFAULT		64
#define	HMENT_PREALLOC_MIN		16
#define	HMENT_PREALLOC_MAX		1024
#define	HMENT_RESERVE_DEFAULT		(MMU_PAGESIZE / sizeof(hment_t))
#define	HMENT_RESERVE_MIN		0
#define	HMENT_RESERVE_MAX		10240

uint32_t	hment_prealloc_size;	/* Total count of preallocated hment */
uint32_t	hment_reserve_size;	/* Number of hment to be reserved */
uint32_t	hment_reserve_nfree;	/* Number of reserved hment */
kmutex_t	hment_reserve_mutex;
hment_t		*hment_reserve_pool;

/*
 * Number of hash entries will be determined by hment_boot_alloc().
 */
#define	HMENT_HASH_MINSIZE	512
static uint_t	hment_hash_entries;
static hment_t	**hment_hash;

/*
 * Lots of highly shared pages will have the same value for "entry" (consider
 * the starting address of "xterm" or "sh"). So we'll distinguish them by
 * adding the pfn of the page table into both the high bits.
 * The shift by 9 corresponds to the range of values for entry (0..511).
 */
#define	HMENT_HASH(pfn, entry)						\
	(uint32_t)((((pfn) << 9) + entry + (pfn)) & (hment_hash_entries - 1))

/*
 * "mlist_lock" is a hashed mutex lock for protecting per-page mapping
 * lists and "hash_lock" is a similar lock protecting the hment hash
 * table.  The hashed approach is taken to avoid the spatial overhead of
 * maintaining a separate lock for each page, while still achieving better
 * scalability than a single lock would allow.
 */
#define	MLIST_NUM_LOCK	256		/* must be power of two */
static kmutex_t mlist_lock[MLIST_NUM_LOCK];

/*
 * the shift by 9 is so that all large pages don't use the same hash bucket
 */
#define	MLIST_MUTEX(pp)							\
	&mlist_lock[((pp)->p_pagenum + ((pp)->p_pagenum >> 9)) &	\
		    (MLIST_NUM_LOCK - 1)]

#define	HASH_NUM_LOCK	256		/* must be power of two */
static kmutex_t hash_lock[HASH_NUM_LOCK];

#define	HASH_MUTEX(idx)	(&hash_lock[(idx) & (HASH_NUM_LOCK - 1)])

/*
 * We assume that any buffer located below econtig is static buffer.
 */
#define	HMENT_IS_STATIC(hm)	((caddr_t)(hm) < KSTATIC_END)

/* Internal prototypes */
static inline uint32_t	hment_hashfunc(void *pt, uint16_t ptszc, uint_t entry);
static void	hment_put_reserve(hment_t *hm);
static void	hment_put_reserve(hment_t *hm);
static hment_t	*hment_get_reserve(void);
static hment_t	*hment_alloc(hat_t *hat);
static void	hment_insert(hment_t *hm, page_t *pp);
static hment_t	*hment_steal(void);

static boolean_t	hment_l1pt_is_locked(hment_t *hm);
static boolean_t	hment_l2pt_is_locked(hment_t *hm);

/*
 * Hash function for global hment hash table.
 */
static inline uint32_t
hment_hashfunc(void *pt, uint16_t ptszc, uint_t entry)
{
	uint32_t	ret;
	pfn_t		pfn;

	SZC_ASSERT(ptszc);
	if (SZC_EVAL(ptszc) >= SZC_SECTION) {
		hat_t		*hat = (hat_t *)pt;

		pfn = mmu_btop(hat->hat_l1paddr) >> 2;
	}
	else {
		hat_l2pt_t	*l2pt = (hat_l2pt_t *)pt;

		ASSERT(l2pt->l2_paddr != NULL);
		pfn = l2pt->l2_paddr >> L2PT_SHIFT;
	}

	return HMENT_HASH(pfn, entry);
}

/*
 * put one hment onto the reserves list
 */
static void
hment_put_reserve(hment_t *hm)
{
	HATSTAT_INC(hs_hm_put_reserve);
	mutex_enter(&hment_reserve_mutex);
	hm->hm_next = hment_reserve_pool;
	hment_reserve_pool = hm;
	++hment_reserve_nfree;
	mutex_exit(&hment_reserve_mutex);
}

/*
 * Take one hment from the reserve.
 */
static hment_t *
hment_get_reserve(void)
{
	hment_t *hm = NULL;

	/*
	 * We rely on a "donation system" to refill the hment reserve
	 * list, which only takes place when we are allocating hments for
	 * user mappings.  It is theoretically possible that an incredibly
	 * long string of kernel hment_alloc()s with no intervening user
	 * hment_alloc()s could exhaust that pool.
	 */
	HATSTAT_INC(hs_hm_get_reserve);
	mutex_enter(&hment_reserve_mutex);
	if (hment_reserve_nfree != 0) {
		hm = hment_reserve_pool;
		hment_reserve_pool = hm->hm_next;
		--hment_reserve_nfree;
	}
	mutex_exit(&hment_reserve_mutex);
	return hm;
}

/*
 * Allocate an hment
 */
static hment_t *
hment_alloc(hat_t *hat)
{
	hment_t	*hm = NULL;
	int	use_reserves, warn = 0;

	use_reserves = (hat_use_boot_reserve || panicstr != NULL);

	ASSERT(!HAT_IS_LOCKED(hat));

	/*
	 * If we aren't using the reserves, try using kmem to get an hment.
	 * Donate any successful allocations to reserves if low.
	 *
	 * If we're in panic, resort to using the reserves.
	 */
	HATSTAT_INC(hs_hm_alloc);

	while (1) {
		if (use_reserves) {
			hm = hment_get_reserve();
		}
		else {
			int	km_flag;

			km_flag = (hat == &hat_kas || hat_can_steal_post_boot)
				? KM_NOSLEEP : KM_SLEEP;

			for (;;) {
				ASSERT(!HAT_KAS_IS_LOCKED());
				hm = kmem_cache_alloc(hment_cache, km_flag);

				/*
				 * It's harmless to check hment_reserve_nfree
				 * without locking.
				 */
				if (hment_reserve_nfree >= hment_reserve_size ||
				    hm == NULL) {
					break;
				}
				hment_put_reserve(hm);
			}
		}
		if (hm != NULL) {
			break;
		}

		/*
		 * If allocation failed, we need to tap the reserves or steal
		 */

		/*
		 * If we still haven't gotten an hment, attempt to steal one
		 * by victimizing a user mapping.
		 */
		if (hat_can_steal_post_boot) {
			/* Kick kmem_reap() to shrink kmem caches. */
			kmem_reap();

			if ((hm = hment_steal()) != NULL) {
				break;
			}
		}

		/*
		 * we're in dire straights, try the reserve
		 */
		if ((hm = hment_get_reserve()) != NULL) {
			break;
		}

		/*
		 * Spin here because the system is very low on memory.
		 * Consider increase "hment-prealloc" or "hment-reserve" value.
		 */
		ASSERT(hat == &hat_kas);
		if (!warn) {
			cmn_err(CE_WARN, "Failed to allocate hment for "
				"kernel mapping.\n");
			warn++;
		}
		tenmicrosec();
	}

	hm->hm_entry = 0;
	hm->hm_htable = NULL;
	hm->hm_hashnext = NULL;
	hm->hm_next = NULL;
	hm->hm_prev = NULL;
	hm->hm_ptszc = 0;
	hm->hm_pfn = PFN_INVALID;

	return hm;
}

/*
 * Free an hment, possibly to the reserves list when called from the
 * thread using the reserves. For example, when freeing an hment during an
 * htable_steal(), we can't recurse into the kmem allocator, so we just
 * push the hment onto the reserve list.
 */
void
hment_free(hment_t *hm)
{
#ifdef DEBUG
	/*
	 * zero out all fields to try and force any race conditions to segfault
	 */
	bzero(hm, sizeof (*hm));
#endif
	HATSTAT_INC(hs_hm_free);
	ASSERT(!HAT_KAS_IS_LOCKED());

	if (HMENT_IS_STATIC(hm) || hment_reserve_nfree < hment_reserve_size) {
		hment_put_reserve(hm);
	}
	else {
		kmem_cache_free(hment_cache, hm);
	}
}

int
hment_owned(page_t *pp)
{
	ASSERT(pp != NULL);

	return MUTEX_HELD(MLIST_MUTEX(pp));
}

void
hment_enter(page_t *pp)
{
	ASSERT(pp != NULL);

	mutex_enter(MLIST_MUTEX(pp));
}

/*
 * boolean_t
 * hment_tryenter(page_t *pp)
 *	Try to acquire hment lock.
 *
 * Calling/Exit State:
 *	Return B_TRUE if we can acquire hment mutex.
 *	Otherwise B_FALSE.
 */
boolean_t
hment_tryenter(page_t *pp)
{
	ASSERT(pp != NULL);

	return mutex_tryenter(MLIST_MUTEX(pp));
}

void
hment_exit(page_t *pp)
{
	ASSERT(pp != NULL);

	mutex_exit(MLIST_MUTEX(pp));
}

/*
 * Internal routine to add a full hment to a page_t mapping list
 */
static void
hment_insert(hment_t *hm, page_t *pp)
{
	uint_t		idx;

	ASSERT(hment_owned(pp));
	ASSERT(!pp->p_embed);

	/*
	 * Add the hment to the page's mapping list.
	 */
	++pp->p_share;
	hm->hm_next = pp->p_mapping;
	if (pp->p_mapping != NULL) {
		((hment_t *)pp->p_mapping)->hm_prev = hm;
	}
	pp->p_mapping = hm;

	/*
	 * Add the hment to the system-wide hash table.
	 */
	idx = hment_hashfunc(hm->hm_htable, hm->hm_ptszc, hm->hm_entry);

	mutex_enter(HASH_MUTEX(idx));
	hm->hm_hashnext = hment_hash[idx];
	hment_hash[idx] = hm;
	mutex_exit(HASH_MUTEX(idx));
}

/*
 * Prepare a mapping list entry to the given page.
 *
 * There are 4 different situations to deal with:
 *
 * - Adding the first mapping to a page_t as an embedded hment
 * - Refaulting on an existing embedded mapping
 * - Upgrading an embedded mapping when adding a 2nd mapping
 * - Adding another mapping to a page_t that already has multiple mappings
 *	 note we don't optimized for the refaulting case here.
 *
 * Due to competition with other threads that may be mapping/unmapping the
 * same page and the need to drop all locks while allocating hments, any or
 * all of the 3 situations can occur (and in almost any order) in any given
 * call. Isn't this fun!
 */
hment_t *
hment_prepare(hat_t *hat, void *htable, uint16_t ptszc, uint_t entry,
	      page_t *pp)
{
	hment_t		*hm = NULL;

	ASSERT(HAT_IS_LOCKED(hat));
	ASSERT(hment_owned(pp));
	SZC_ASSERT(ptszc);

	ptszc = SZC_EVAL(ptszc);
	for (;;) {

		/*
		 * The most common case is establishing the first mapping to a
		 * page, so check that first. This doesn't need any allocated
		 * hment.
		 */
		if (pp->p_mapping == NULL) {
			ASSERT(!pp->p_embed);
			ASSERT(pp->p_share == 0);
			if (hm == NULL) {
				break;
			}

			/*
			 * we had an hment already, so free it and retry
			 */
			goto free_and_continue;
		}

		/*
		 * If there is an embedded mapping, we may need to
		 * convert it to an hment.
		 */
		if (pp->p_embed) {

			/* should point to htable */
			ASSERT(pp->p_mapping != NULL);

			/*
			 * If we are faulting on a pre-existing mapping
			 * there is no need to promote/allocate a new hment.
			 * This happens a lot due to segmap.
			 */
			if (pp->p_mapping == htable &&
			    pp->p_mlentry == entry) {
				ASSERT(pp->p_ptszc == ptszc);

				if (hm == NULL) {
					break;
				}
				goto free_and_continue;
			}

			/*
			 * If we have an hment allocated, use it to promote the
			 * existing embedded mapping.
			 */
			if (hm != NULL) {
				SZC_ASSERT(pp->p_ptszc);

				hm->hm_htable = pp->p_mapping;
				hm->hm_entry = pp->p_mlentry;
				hm->hm_ptszc = SZC_EVAL(pp->p_ptszc);
				hm->hm_pfn = pp->p_pagenum;
				pp->p_mapping = NULL;
				pp->p_share = 0;
				pp->p_embed = 0;
				hment_insert(hm, pp);
			}

			/*
			 * We either didn't have an hment allocated or we just
			 * used it for the embedded mapping. In either case,
			 * allocate another hment and restart.
			 */
			goto allocate_and_continue;
		}

		/*
		 * Last possibility is that we're adding an hment to a list
		 * of hments.
		 */
		if (hm != NULL) {
			break;
		}
allocate_and_continue:
		HAT_UNLOCK(hat);
		hment_exit(pp);
		hm = hment_alloc(hat);
		hment_enter(pp);
		HAT_LOCK(hat);
		continue;

free_and_continue:
		/*
		 * we allocated an hment already, free it and retry
		 */
		HAT_UNLOCK(hat);
		hment_exit(pp);
		hment_free(hm);
		hm = NULL;
		hment_enter(pp);
		HAT_LOCK(hat);
	}
	ASSERT(hment_owned(pp));
	ASSERT(HAT_IS_LOCKED(hat));

	return hm;
}

/*
 * Record a mapping list entry for the htable/entry to the given page.
 *
 * hment_prepare() should have properly set up the situation.
 */
void
hment_assign(void *htable, uint16_t ptszc, uint_t entry, page_t *pp,
	     hment_t *hm)
{
	ASSERT(hment_owned(pp));
	SZC_ASSERT(ptszc);

	/*
	 * The most common case is establishing the first mapping to a
	 * page, so check that first. This doesn't need any allocated
	 * hment.
	 */
	ptszc = SZC_EVAL(ptszc);
	if (pp->p_mapping == NULL) {
		ASSERT(hm == NULL);
		ASSERT(!pp->p_embed);
		ASSERT(pp->p_share == 0);
		pp->p_embed = 1;
		pp->p_mapping = htable;
		pp->p_mlentry = entry;
		pp->p_ptszc = ptszc;
		return;
	}

	/*
	 * We should never get here with a pre-existing embedded maping
	 */
	ASSERT(!pp->p_embed);

	/*
	 * add the new hment to the mapping list
	 */
	ASSERT(hm != NULL);
	hm->hm_htable = htable;
	hm->hm_entry = entry;
	hm->hm_ptszc = ptszc;
	hm->hm_pfn = pp->p_pagenum;
	hment_insert(hm, pp);
}

/*
 * Walk through the mappings for a page.
 *
 * must already have done an hment_enter()
 */
hment_t *
hment_walk(page_t *pp, void **ht, uint16_t *ptszc, uint_t *entry,
	   hment_t *prev)
{
	hment_t		*hm;

	ASSERT(hment_owned(pp));

	if (pp->p_embed) {
		if (prev == NULL) {
			SZC_ASSERT(pp->p_ptszc);

			*ht = (uintptr_t *)pp->p_mapping;
			*ptszc = SZC_EVAL(pp->p_ptszc);
			*entry = pp->p_mlentry;
			hm = HMENT_EMBEDDED;
		}
		else {
			ASSERT(prev == HMENT_EMBEDDED);
			hm = NULL;
		}
	}
	else {
		if (prev == NULL) {
			ASSERT(prev != HMENT_EMBEDDED);
			hm = (hment_t *)pp->p_mapping;
		}
		else {
			hm = prev->hm_next;
		}

		if (hm != NULL) {
			SZC_ASSERT(hm->hm_ptszc);

			*ht = hm->hm_htable;
			*ptszc = SZC_EVAL(hm->hm_ptszc);
			*entry = hm->hm_entry;
		}
	}
	return hm;
}

/*
 * Remove a mapping to a page from its mapping list. Must have
 * the corresponding mapping list locked.
 * Finds the mapping list entry with the given pte_t and
 * unlinks it from the mapping list.
 */
hment_t *
hment_remove(page_t *pp, void *ht, uint16_t ptszc, uint_t entry)
{
	hment_t		*prev = NULL;
	hment_t		*hm;
	uint_t		idx;
	pfn_t		pfn;

	ASSERT(hment_owned(pp));
	SZC_ASSERT(ptszc);

	/*
	 * Check if we have only one mapping embedded in the page_t.
	 */
	if (pp->p_embed) {
		ASSERT(ht == (uint32_t *)pp->p_mapping);
		ASSERT(entry == pp->p_mlentry);
		ASSERT(ptszc == pp->p_ptszc);
		ASSERT(pp->p_share == 0);
		pp->p_mapping = NULL;
		pp->p_ptszc = 0;
		pp->p_mlentry = 0;
		pp->p_embed = 0;
		return NULL;
	}

	/*
	 * Otherwise it must be in the list of hments.
	 * Find the hment in the system-wide hash table and remove it.
	 */
	ASSERT(pp->p_share != 0);
	pfn = pp->p_pagenum;
	idx = hment_hashfunc(ht, ptszc, entry);
	mutex_enter(HASH_MUTEX(idx));
	hm = hment_hash[idx];
	while (hm && (hm->hm_htable != ht || hm->hm_entry != entry ||
		      hm->hm_pfn != pfn)) {
		prev = hm;
		hm = hm->hm_hashnext;
	}
	if (hm == NULL) {
		panic("hment_remove() missing in hash table pp=%lx, ht=%lx,"
		      "entry=0x%x hash index=0x%x", (uintptr_t)pp,
		      (uintptr_t)ht, entry, idx);
	}

	if (prev) {
		prev->hm_hashnext = hm->hm_hashnext;
	}
	else {
		hment_hash[idx] = hm->hm_hashnext;
	}
	mutex_exit(HASH_MUTEX(idx));

	/*
	 * Remove the hment from the page's mapping list
	 */
	if (hm->hm_next) {
		hm->hm_next->hm_prev = hm->hm_prev;
	}
	if (hm->hm_prev) {
		hm->hm_prev->hm_next = hm->hm_next;
	}
	else {
		pp->p_mapping = hm->hm_next;
	}

	--pp->p_share;
	hm->hm_hashnext = NULL;
	hm->hm_next = NULL;
	hm->hm_prev = NULL;

	return hm;
}

/*
 * Readjust the hment reserves after they may have been used.
 */
void
hment_adjust_reserve()
{
	hment_t	*hmlist = NULL, **hmpp, *hm, *n;

	/*
	 * Free up any excess reserves
	 */
	mutex_enter(&hment_reserve_mutex);
	hmpp = &hment_reserve_pool;
	while ((hm = *hmpp) != NULL &&
	       hment_reserve_nfree > hment_reserve_size) {
		if (HMENT_IS_STATIC(hm)) {
			hmpp = &(hm->hm_next);
			continue;
		}

		/* Unlink from reserve pool. */
		*hmpp = hm->hm_next;
		hment_reserve_nfree--;

		/* Link this hment to hmlist. */
		hm->hm_next = hmlist;
		hmlist = hm;
	}
	mutex_exit(&hment_reserve_mutex);

	ASSERT(!HAT_KAS_IS_LOCKED());
	for (hm = hmlist; hm != NULL; hm = n) {
		n = hm->hm_next;
		kmem_cache_free(hment_cache, hm);
	}
}

/*
 * void
 * hment_boot_alloc(void)
 *	Allocate static data for hment.
 */
void
hment_boot_alloc(void)
{
	size_t		hashsz;
	uint32_t	i;
	uint_t		nent;

	/* Determine number of hment hash entries. */
	if (HMENT_HASH_SIZE != 0) {
		/* Number of entries is defined explicitly. */
		nent = HMENT_HASH_SIZE;
	}
	else {
		uint_t	sz, nshift;

		/*
		 * Determine number of hash entries based on number of
		 * physical pages.
		 */
		sz = (uint_t)(physmem >> HMENT_HASH_SHIFT);
		nshift = highbit(sz);
		if (nshift > 0) {
			nent = MAX(HMENT_HASH_MINSIZE, 1 << (nshift - 1));
		}
		else {
			nent = HMENT_HASH_MINSIZE;
		}
	}
	hment_hash_entries = nent;
	PRM_DEBUG(hment_hash_entries);

	/* Allocate global hash table for hment. */
	hashsz = nent * sizeof (hment_t *);
	BOOT_ALLOC(hment_hash, hment_t **, hashsz, BO_NO_ALIGN,
		   "Failed to allocate kernel hment_hash");
	FAST_BZERO_ALIGNED(hment_hash, hashsz);
	PRM_DEBUG(hment_hash);

	mutex_init(&hment_reserve_mutex, NULL, MUTEX_DEFAULT, NULL);

	/* Preallocate hment. */
	hment_prealloc_size = hat_bootparam("hment-prealloc",
					    HMENT_PREALLOC_DEFAULT,
					    HMENT_PREALLOC_MIN,
					    HMENT_PREALLOC_MAX);
	hment_reserve_size = hat_bootparam("hment-reserve",
					   HMENT_RESERVE_DEFAULT,
					   HMENT_RESERVE_MIN,
					   HMENT_RESERVE_MAX);
	PRM_DEBUG(hment_prealloc_size);
	PRM_DEBUG(hment_reserve_size);
	for (i = 0; i < hment_prealloc_size; i++) {
		hment_t	*hm;

		BOOT_ALLOC(hm, hment_t *, sizeof(hment_t), BO_NO_ALIGN,
			   "Failed to allocate hment");
		hment_put_reserve(hm);
	}
}

/*
 * initialize hment data structures
 */
void
hment_init(void)
{
	int i;
	int flags = KMC_NOHASH | KMC_NODEBUG;

	/*
	 * Initialize kmem caches. On 32 bit kernel's we shut off
	 * debug information to save on precious kernel VA usage.
	 */
	hment_cache = kmem_cache_create("hment_t", sizeof (hment_t), 0,
					NULL, NULL, NULL, NULL,
					hat_memload_arena, flags);

	for (i = 0; i < MLIST_NUM_LOCK; i++) {
		mutex_init(&mlist_lock[i], NULL, MUTEX_DEFAULT, NULL);
	}

	for (i = 0; i < HASH_NUM_LOCK; i++) {
		mutex_init(&hash_lock[i], NULL, MUTEX_DEFAULT, NULL);
	}
}

/*
 * return the number of mappings to a page
 *
 * Note there is no ASSERT() that the MUTEX is held for this.
 * Hence the return value might be inaccurate if this is called without
 * doing an hment_enter().
 */
uint_t
hment_mapcnt(page_t *pp)
{
	uint_t cnt;
	uint_t szc;
	page_t *larger;
	hment_t	*hm;

	hment_enter(pp);
	SZC_ASSERT(pp->p_szc);
	if (pp->p_mapping == NULL) {
		cnt = 0;
	}
	else if (pp->p_embed) {
		cnt = 1;
	}
	else {
		cnt = pp->p_share;
	}
	hment_exit(pp);

	/*
	 * walk through all larger mapping sizes counting mappings
	 */
	for (szc = 1; szc <= SZC_EVAL(pp->p_szc); ++szc) {
		larger = PP_GROUPLEADER(pp, szc);
		if (larger == pp) {
			/* don't double count large mappings */
			continue;
		}

		hment_enter(larger);
		if (larger->p_mapping != NULL) {
			if (larger->p_embed && larger->p_ptszc == szc) {
				++cnt;
			}
			else if (!larger->p_embed) {
				for (hm = larger->p_mapping; hm;
				     hm = hm->hm_next) {
					if (hm->hm_ptszc == szc) {
						++cnt;
					}
				}
			}
		}
		hment_exit(larger);
	}
	return cnt;
}

/*
 * We need to steal an hment. Walk through all the page_t's until we
 * find one that has multiple mappings. Unload one of the mappings
 * and reclaim that hment. Note that we'll save/restart the starting
 * page to try and spread the pain.
 */
static page_t *last_page = NULL;

static hment_t *
hment_steal(void)
{
	page_t	*last = last_page;
	page_t	*pp = last;
	hment_t	*hm = NULL;
	hment_t	*hm2;
	uint_t	found_one = 0;
	tlbf_ctx_t	tlbf;

	HATSTAT_INC(hs_hm_steals);
	if (pp == NULL) {
		last = pp = page_first();
	}

	while (!found_one) {
		HATSTAT_INC(hs_hm_steal_exam);
		pp = page_next(pp);
		if (pp == NULL) {
			pp = page_first();
		}

		/*
		 * The loop and function exit here if nothing found to steal.
		 */
		if (pp == last) {
			return NULL;
		}

		/*
		 * Only lock the page_t if it has hments.
		 */
		if (pp->p_mapping == NULL || pp->p_embed) {
			continue;
		}

		/*
		 * Search the mapping list for a usable mapping.
		 */
		hment_enter(pp);
		if (!pp->p_embed) {
			for (hm = pp->p_mapping; hm; hm = hm->hm_next) {
				SZC_ASSERT(hm->hm_ptszc);
				if (SZC_EVAL(hm->hm_ptszc) >= SZC_SECTION) {
					if (!hment_l1pt_is_locked(hm)) {
						found_one = 1;
						break;
					}
				}
				else if (!hment_l2pt_is_locked(hm)) {
					found_one = 1;
					break;
				}
			}
		}
		if (!found_one) {
			hment_exit(pp);
		}
	}

	/*
	 * Steal the mapping we found.  Note that hati_page_unmap() will
	 * do the hment_exit().
	 */
	TLBF_CTX_INIT(&tlbf, TLBF_UHAT_SYNC);
	hm2 = hat_page_unmap(pp, hm->hm_htable, hm->hm_entry, hm->hm_ptszc,
			     &tlbf);
	TLBF_CTX_FINI(&tlbf);
	ASSERT(hm2 == hm);
	last_page = pp;
	return hm;
}

/*
 * static boolean_t
 * hment_l1pt_is_locked(hment_t *hm)
 *	Determine whether the specified hment is locked or not.
 *	This function is called if hment is associated with L1PT mapping.
 *
 * Calling/Exit State:
 *	Return B_TRUE if locked. Otherwise B_FALSE.
 *
 *	This function acquires hat lock corresponding to the specified hment.
 *	If it returns B_FALSE, it returns with holding hat lock.
 *	Otherwise it releases hat lock.
 */
static boolean_t
hment_l1pt_is_locked(hment_t *hm)
{
	hat_t		*hat = (hat_t *)hm->hm_htable;
	uint_t		entry = hm->hm_entry;
	uint_t		szc = SZC_EVAL(hm->hm_ptszc);
	hat_l2pt_t	*swflags;
	boolean_t	locked;
	uintptr_t	vaddr;
	l1pte_t		sw;

	if (hat == &hat_kas) {
		return B_TRUE;
	}

	HAT_LOCK(hat);

	vaddr = L1PT_IDX2VADDR(entry);
	swflags = hatpt_l1pt_softflags_lookup(hat, vaddr, szc);
	sw = HAT_L2PT_GET_SECTION_FLAGS(swflags);

	if (sw & PTE_S_LOCKED) {
		locked = B_TRUE;
		HAT_UNLOCK(hat);
	}
	else {
		locked = B_FALSE;
	}

	return locked;
}

/*
 * static boolean_t
 * hment_l2pt_is_locked(hment_t *hm)
 *	Determine whether the specified hment is locked or not.
 *	This function is called if hment is associated with L2PT mapping.
 *
 * Calling/Exit State:
 *	Return B_TRUE if locked. Otherwise B_FALSE.
 *
 *	This function acquires hat lock corresponding to the specified hment.
 *	If it returns B_FALSE, it returns with holding hat lock.
 *	Otherwise it releases hat lock.
 */
static boolean_t
hment_l2pt_is_locked(hment_t *hm)
{
	hat_l2pt_t	*l2pt = (hat_l2pt_t *)hm->hm_htable;
	hat_l2bundle_t	*l2b = HAT_L2PT_TO_BUNDLE(l2pt);
	hat_t		*hat = l2b->l2b_hat;
	uint_t		entry = hm->hm_entry;
	uint_t		szc = SZC_EVAL(hm->hm_ptszc);
	l2pte_t		*ptep, *swptep, sw;
	boolean_t	locked;

	SZC_ASSERT(hm->hm_ptszc);
	if (hat == &hat_kas) {
		return B_TRUE;
	}

	HAT_LOCK(hat);

	/* Only PTE for group leader page has software flags. */
	if (szc == SZC_LARGE) {
		entry = P2ALIGN(entry, L2PT_LARGE_NPTES);
	}
	else {
		ASSERT(szc == SZC_SMALL);
	}
	ptep = l2pt->l2_vaddr + entry;
	swptep = L2PT_SOFTFLAGS(ptep);
	sw = *swptep;

	if (sw & PTE_S_LOCKED) {
		locked = B_TRUE;
		HAT_UNLOCK(hat);
	}
	else {
		locked = B_FALSE;
	}

	return locked;
}
