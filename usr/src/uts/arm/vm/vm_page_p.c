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
 * Copyright (c) 2006-2008 NEC Corporation
 */

#ident	"@(#)arm/vm/vm_page_p.c"

/*
 * ARM processor-dependant page management.
 */

#ifndef	_MACHDEP
#error	vm_page_p.c must be built under platform dependant environment.
#endif	/* !_MACHDEP */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/memnode.h>
#include <sys/memlist.h>
#include <sys/archsystm.h>
#include <vm/vm_dep.h>
#include <vm/page.h>

#include <sys/ddidmareq.h>
#include <sys/vmem.h>
#include <vm/seg_kmem.h>
#include <sys/mman.h>

#include <sys/vtrace.h>
#include <sys/vm.h>
#include <sys/cmn_err.h>
#include <sys/atomic.h>
#include <sys/platform.h>
#include <sys/lpg_config.h>
#include <sys/cache_l220.h>
#include <sys/mach_dma.h>

#define	PFN_4G		(4ull * (1024 * 1024 * 1024 / MMU_PAGESIZE))

extern uint_t page_create_new;
extern uint_t page_create_exists;
extern uint_t page_create_putbacks;
static kmutex_t	contig_lock;

static page_t *is_contigpage_free(pfn_t *pfnp, pgcnt_t *pgcnt, pgcnt_t minctg,
				  uint64_t pfnseg, int iolock);
static page_t *page_get_contigpage(pgcnt_t *pgcnt, ddi_dma_attr_t *mattr,
				   int iolock);
static page_t *page_get_mnode_anylist(ulong_t origbin, uchar_t szc,
				      uint_t flags, int mnode, int mtype,
				      ddi_dma_attr_t *dma_attr);
static page_t *page_get_anylist(struct vnode *vp, u_offset_t off,
				struct as *as, caddr_t vaddr, size_t size,
				uint_t flags, ddi_dma_attr_t *dma_attr);
static void	contig_alloc_map(caddr_t addr, size_t size, page_t *ppl,
				 uint_t attr, int pflag);

/*
 * Determine whether page coloring should be activated or not.
 * If L220 cache is not implemented, page coloring is always disabled.
 */
int do_pg_coloring = ARMPF_L220_EXIST;

/*
 * initialized by page_coloring_init()
 */
uint_t	page_colors;
uint_t	page_colors_mask;
uint_t	page_coloring_shift;

int	cpu_page_colors;
uint_t	vac_colors = 1;

kmutex_t *fpc_mutex[NPC_MUTEX];
kmutex_t *cpc_mutex[NPC_MUTEX];

/*
 * Used by page layer to know about page sizes
 */
hw_pagesize_t hw_page_array[] = {
	{
		/* 4K page */
		MMU_PAGESIZE, MMU_PAGESHIFT, 0, MMU_PAGESIZE >> MMU_PAGESHIFT
	},
#ifndef	LPG_DISABLE
	{
		/* 64K page */
		MMU_PAGESIZE_LARGE, MMU_PAGESHIFT_LARGE, 0,
		MMU_PAGESIZE_LARGE >> MMU_PAGESHIFT
	},
	{
		/* 1M page */
		MMU_PAGESIZE_SECTION, MMU_PAGESHIFT_SECTION, 0,
		MMU_PAGESIZE_SECTION >> MMU_PAGESHIFT
	},
	{
		/* 16M page */
		MMU_PAGESIZE_SPSECTION, MMU_PAGESHIFT_SPSECTION, 0,
		MMU_PAGESIZE_SPSECTION >> MMU_PAGESHIFT
	},
#endif	/* !LPG_DISABLE */
	{0, 0, 0, 0}
};

plcnt_t	plcnt;

page_t ***page_freelists;
page_t **page_cachelists;

size_t	auto_lpg_va_default = MMU_PAGESIZE;	/* used by zmap() */

/*
 * Number of threads that wait for available memory in page_create_io().
 * Memory scheduler will be activated if this value is not zero.
 */
volatile uint_t	page_create_io_waiters;

/*
 * void
 * page_coloring_init(void)
 *	Initialize page coloring.
 */
void
page_coloring_init(void)
{
	int	i;

	if (ARMPF_L220_EXIST == 0 || do_pg_coloring == 0) {
		/* Disable coloring. */
		page_colors = 1;
		for (i = 0; i < mmu_page_sizes; i++) {
			colorequivszc[i] = 0;
			hw_page_array[i].hp_colors = 1;
		}
		return;
	}

	/*
	 * Calculate page_colors from ecache_setsize.
	 * On MPCore these variables are derived from L220 cache size.
	 */
	page_colors = L220_WAYSIZE / MMU_PAGESIZE;
	page_colors_mask = page_colors - 1;

	/*
	 * We don't need to set cpu_page_colors because a page color may not
	 * spread across multiple cache bins.
	 */
	cpu_page_colors = 0;

	page_coloring_shift = lowbit(L220_WAYSIZE);

	/* initialize number of colors per page size */
	for (i = 0; i < mmu_page_sizes; i++) {
		hw_page_array[i].hp_colors =
			(page_colors_mask >> PNUM_SHIFT(i)) + 1;
		colorequivszc[i] = 0;
	}
}

/*
 * caddr_t
 * page_coloring_alloc(caddr_t base)
 *	Called once at startup to configure page_coloring data structures.
 */
caddr_t
page_coloring_alloc(caddr_t base)
{
	int	sz, k;
	caddr_t	addr;

	addr = base;

	/* Allocate mutex for page lists */
	for (k = 0; k < NPC_MUTEX; k++) {
		fpc_mutex[k] = (kmutex_t *)addr;
		addr += (max_mem_nodes * sizeof (kmutex_t));
	}
	for (k = 0; k < NPC_MUTEX; k++) {
		cpc_mutex[k] = (kmutex_t *)addr;
		addr += (max_mem_nodes * sizeof (kmutex_t));
	}

	/* Allocate page_freelists. */
	page_freelists = (page_t ***)addr;
	addr += (sizeof (page_t **) * mmu_page_sizes);

	for (sz = 0; sz < mmu_page_sizes; sz++) {
		int	colors = page_get_pagecolors(sz);

		page_freelists[sz] = (page_t **)addr;
		addr += (colors * sizeof (page_t *));
	}

	/* Allocate page_cachelists. */
	page_cachelists = (page_t **)addr;
	addr += (page_colors * sizeof (page_t *));

	return addr;
}

/*
 * int
 * pf_is_memory(pfn_t pf)
 *	Determine whether the specified page frame number is memory or not.
 *
 * Calling/Exit State:
 *	Return true if the specified pfn is memory.
 */
int
pf_is_memory(pfn_t pf)
{
	return (address_in_memlist(phys_install, mmu_ptob((uint64_t)pf), 1));
}


/*
 * static page_t *
 * is_contigpage_free(pfn_t *pfnp, pgcnt_t *pgcnt, pgcnt_t minctg,
 *                    uint64_t pfnseg, int iolock):
 *
 *	returns a page list of contiguous pages. It minimally has to return
 *	minctg pages. Caller determines minctg based on the scatter-gather
 *	list length.
 *
 *	pfnp is set to the next page frame to search on return.
 */
static page_t *
is_contigpage_free(
	pfn_t *pfnp,
	pgcnt_t *pgcnt,
	pgcnt_t minctg,
	uint64_t pfnseg,
	int iolock)
{
	int	i = 0;
	pfn_t	pfn = *pfnp;
	page_t	*pp;
	page_t	*plist = NULL;

	/*
	 * fail if pfn + minctg crosses a segment boundary.
	 * Adjust for next starting pfn to begin at segment boundary.
	 */

	if (((*pfnp + minctg - 1) & pfnseg) < (*pfnp & pfnseg)) {
		*pfnp = roundup(*pfnp, pfnseg + 1);
		return (NULL);
	}

	do {
retry:
		pp = page_numtopp_nolock(pfn + i);
		if ((pp == NULL) ||
		    (page_trylock(pp, SE_EXCL) == 0)) {
			(*pfnp)++;
			break;
		}
		if (page_pptonum(pp) != pfn + i) {
			page_unlock(pp);
			goto retry;
		}

		if (!(PP_ISFREE(pp))) {
			page_unlock(pp);
			(*pfnp)++;
			break;
		}

		if (!PP_ISAGED(pp)) {
			page_list_sub(pp, PG_CACHE_LIST);
			page_hashout(pp, (kmutex_t *)NULL);
		} else {
			page_list_sub(pp, PG_FREE_LIST);
		}

		if (iolock)
			page_io_lock(pp);
		page_list_concat(&plist, &pp);

		/*
		 * exit loop when pgcnt satisfied or segment boundary reached.
		 */

	} while ((++i < *pgcnt) && ((pfn + i) & pfnseg));

	*pfnp += i;		/* set to next pfn to search */

	if (i >= minctg) {
		*pgcnt -= i;
		return (plist);
	}

	/*
	 * failure: minctg not satisfied.
	 *
	 * if next request crosses segment boundary, set next pfn
	 * to search from the segment boundary.
	 */
	if (((*pfnp + minctg - 1) & pfnseg) < (*pfnp & pfnseg))
		*pfnp = roundup(*pfnp, pfnseg + 1);

	/* clean up any pages already allocated */

	while (plist) {
		pp = plist;
		page_sub(&plist, pp);
		page_list_add(pp, PG_FREE_LIST | PG_LIST_TAIL);
		if (iolock)
			page_io_unlock(pp);
		page_unlock(pp);
	}
	return (NULL);
}

/*
 * verify that pages being returned from allocator have correct DMA attribute
 */
#ifndef DEBUG
#define	check_dma(a, b, c) (0)
#else
static void
check_dma(ddi_dma_attr_t *dma_attr, page_t *pp, int cnt)
{
	uint64_t	firstaddr;

	if (dma_attr == NULL)
		return;

	firstaddr = mmu_ptob((uint64_t)pp->p_pagenum);
	if ((firstaddr & (dma_attr->dma_attr_align - 1)) != 0) {
		panic("PFN 0x%lx (pp=%p) bad align: 0x%llx",
		      pp->p_pagenum, pp, dma_attr->dma_attr_align);
	}

	while (cnt-- > 0) {
		if (mmu_ptob((uint64_t)pp->p_pagenum) <
		    dma_attr->dma_attr_addr_lo)
			panic("PFN (pp=%p) below dma_attr_addr_lo", pp);
		if (mmu_ptob((uint64_t)pp->p_pagenum) >=
		    dma_attr->dma_attr_addr_hi)
			panic("PFN (pp=%p) above dma_attr_addr_hi", pp);
		pp = pp->p_next;
	}
}
#endif

/*
 * static page_t *
 * page_get_contigpage(pgcnt_t *pgcnt, ddi_dma_attr_t *mattr, int iolock)
 *
 *     allocate contiguous pages to satisfy the '*pgcnt' and DMA
 *     attributes specified in '*mattr'.
 *  
 *  return : 
 *     if contiguous physical page secured, return plist ptr.
 */
static page_t *
page_get_contigpage(pgcnt_t *pgcnt, ddi_dma_attr_t *mattr, int iolock)
{
	pfn_t		pfn;
	int		sgllen;
	uint64_t	pfnseg;
	pgcnt_t		minctg;
	page_t		*pplist = NULL, *plist;
	uint64_t	lo, hi;
	pgcnt_t		pfnalign = 0;
	static pfn_t	startpfn;
	static pgcnt_t	lastctgcnt;
	uintptr_t	align;


	mutex_enter(&contig_lock);

	if (mattr) {
		lo = mmu_btop((mattr->dma_attr_addr_lo + MMU_PAGEOFFSET));
		hi = mmu_btop(mattr->dma_attr_addr_hi);
		if (hi >= physmax)
			hi = physmax - 1;
		sgllen = mattr->dma_attr_sgllen;
		pfnseg = mmu_btop(mattr->dma_attr_seg);

		align = maxbit(mattr->dma_attr_align, mattr->dma_attr_minxfer);
		if (align > MMU_PAGESIZE)
			pfnalign = mmu_btop(align);

		/*
		 * in order to satisfy the request, must minimally
		 * acquire minctg contiguous pages
		 */
		minctg = howmany(*pgcnt, sgllen);

		ASSERT(hi >= lo);

		/*
		 * start from where last searched if the minctg >= lastctgcnt
		 */
		if (minctg < lastctgcnt || startpfn < lo || startpfn > hi)
			startpfn = lo;
	} else {
		hi = physmax - 1;
		lo = 0;
		sgllen = 1;
		pfnseg = PFN_4G - 1; /* ARM MMU manages memory space of 32bit */
		minctg = *pgcnt;

		if (minctg < lastctgcnt)
			startpfn = lo;
	}
	lastctgcnt = minctg;

	ASSERT(pfnseg + 1 >= (uint64_t)minctg);

	pfn = startpfn;
	if (pfnalign)
		pfn = P2ROUNDUP(pfn, pfnalign);

	while (pfn + minctg - 1 <= hi) {
		plist = is_contigpage_free(&pfn, pgcnt, minctg, pfnseg, iolock);
		if (plist) {
			page_list_concat(&pplist, &plist);
			if (mattr)
				sgllen--;
			/*
			 * return when contig pages no longer needed
			 */
			if (!*pgcnt || ((*pgcnt <= sgllen) && !pfnalign)) {
				startpfn = pfn;
				mutex_exit(&contig_lock);
				check_dma(mattr, pplist, *pgcnt);
				return (pplist);
			}
			if (mattr)
				minctg = howmany(*pgcnt, sgllen);
		}
		if (pfnalign)
			pfn = P2ROUNDUP(pfn, pfnalign);
	}

	/* cannot find contig pages in specified range */
	if (startpfn == lo) {
		mutex_exit(&contig_lock);
		return (NULL);
	}

	/* did not start with lo previously */
	pfn = lo;
	if (pfnalign)
		pfn = P2ROUNDUP(pfn, pfnalign);

	/* allow search to go above startpfn */
	while (pfn < startpfn) {
		plist = is_contigpage_free(&pfn, pgcnt, minctg, pfnseg, iolock);
		if (plist != NULL) {
			page_list_concat(&pplist, &plist);
			if (mattr)
				sgllen--;
			/*
			 * return when contig pages no longer needed
			 */
			if (!*pgcnt || ((*pgcnt <= sgllen) && !pfnalign)) {
				startpfn = pfn;
				mutex_exit(&contig_lock);
				check_dma(mattr, pplist, *pgcnt);
				return (pplist);
			}
			if (mattr)
				minctg = howmany(*pgcnt, sgllen);
		}
		if (pfnalign)
			pfn = P2ROUNDUP(pfn, pfnalign);
	}
	mutex_exit(&contig_lock);
	return (NULL);
}

/*
 * get a page from any list with the given mnode
 */
static page_t *
page_get_mnode_anylist(ulong_t origbin, uchar_t szc, uint_t flags,
		       int mnode, int mtype, ddi_dma_attr_t *dma_attr)
{
	kmutex_t	*pcm;
	int		i;
	page_t		*pp;
	page_t		*first_pp;
	uint64_t	pgaddr;
	ulong_t		bin;
	int		mtypestart;
	int		plw_initialized;
	page_list_walker_t	plw;

	ASSERT((flags & PG_MATCH_COLOR) == 0);
	ASSERT(szc == 0);
	ASSERT(dma_attr != NULL);

	szc = SZC_EVAL(szc);

	MTYPE_START(mnode, mtype, flags);
	if (mtype < 0) {
		return NULL;
	}

	mtypestart = mtype;

	bin = origbin;

	/*
	 * check up to page_colors + 1 bins - origbin may be checked twice
	 * because of BIN_STEP skip
	 */
	do {
		plw_initialized = 0;

		for (plw.plw_count = 0;
		     plw.plw_count < page_colors; plw.plw_count++) {

			if (PAGE_FREELISTS(mnode, szc, bin, mtype) == NULL) {
				goto nextfreebin;
			}

			pcm = PC_BIN_MUTEX(mnode, bin, PG_FREE_LIST);
			mutex_enter(pcm);
			pp = PAGE_FREELISTS(mnode, szc, bin, mtype);
			first_pp = pp;
			while (pp != NULL) {
				if (page_trylock(pp, SE_EXCL) == 0) {
					pp = pp->p_next;
					if (pp == first_pp) {
						pp = NULL;
					}
					continue;
				}

				ASSERT(PP_ISFREE(pp));
				ASSERT(PP_ISAGED(pp));
				ASSERT(pp->p_vnode == NULL);
				ASSERT(pp->p_hash == NULL);
				ASSERT(pp->p_offset == (u_offset_t)-1);
				ASSERT(pp->p_szc == szc);
				ASSERT(PFN_2_MEM_NODE(pp->p_pagenum) == mnode);
				/* check if page within DMA attributes */
				pgaddr = mmu_ptob((uint64_t)(pp->p_pagenum));

				if ((pgaddr >= dma_attr->dma_attr_addr_lo) &&
				    (pgaddr + MMU_PAGESIZE - 1 <=
				     dma_attr->dma_attr_addr_hi)) {
					break;
				}

				/* continue looking */
				page_unlock(pp);
				pp = pp->p_next;
				if (pp == first_pp) {
					pp = NULL;
				}

			}
			if (pp != NULL) {
				ASSERT(mtype == PP_2_MTYPE(pp));
				ASSERT(pp->p_szc == 0);

				/* found a page with specified DMA attributes */
				page_sub(&PAGE_FREELISTS(mnode, szc, bin,
							 mtype), pp);
				page_ctr_sub(mnode, mtype, pp, PG_FREE_LIST);

				if ((PP_ISFREE(pp) == 0) ||
				    (PP_ISAGED(pp) == 0)) {
					cmn_err(CE_PANIC, "page %p is not free",
						(void *)pp);
				}

				mutex_exit(pcm);
				check_dma(dma_attr, pp, 1);
				return (pp);
			}
			mutex_exit(pcm);
nextfreebin:
			if (plw_initialized == 0) {
				page_list_walk_init(szc, 0, bin, 1, 0, &plw);
				ASSERT(plw.plw_ceq_dif == page_colors);
				plw_initialized = 1;
			}

			LPG_DISABLE_ASSERT(plw.plw_do_split == 0);
			if (LPG_EVAL(plw.plw_do_split)) {
				pfn_t	pfn;

				pfn = mmu_btop(dma_attr->dma_attr_addr_hi + 1),
				pp = page_freelist_split(szc, bin, mnode,
							 mtype, pfn, &plw);
				if (pp != NULL)
					return (pp);
			}

			bin = page_list_walk_next_bin(szc, bin, &plw);
		}
		MTYPE_NEXT(mnode, mtype, flags);
	} while (mtype >= 0);

	/* failed to find a page in the freelist; try it in the cachelist */

	/* reset mtype start for cachelist search */
	mtype = mtypestart;
	ASSERT(mtype >= 0);

	/* start with the bin of matching color */
	bin = origbin;

	do {
		for (i = 0; i <= page_colors; i++) {
			if (PAGE_CACHELISTS(mnode, bin, mtype) == NULL) {
				goto nextcachebin;
			}
			pcm = PC_BIN_MUTEX(mnode, bin, PG_CACHE_LIST);
			mutex_enter(pcm);
			pp = PAGE_CACHELISTS(mnode, bin, mtype);
			first_pp = pp;
			while (pp != NULL) {
				if (page_trylock(pp, SE_EXCL) == 0) {
					pp = pp->p_next;
					if (pp == first_pp) {
						break;
					}
					continue;
				}
				ASSERT(pp->p_vnode);
				ASSERT(PP_ISAGED(pp) == 0);
				ASSERT(pp->p_szc == 0);
				ASSERT(PFN_2_MEM_NODE(pp->p_pagenum) == mnode);

				/* check if page within DMA attributes */

				pgaddr = ptob((uint64_t)(pp->p_pagenum));

				if ((pgaddr >= dma_attr->dma_attr_addr_lo) &&
				    (pgaddr + MMU_PAGESIZE - 1 <=
				     dma_attr->dma_attr_addr_hi)) {
					break;
				}

				/* continue looking */
				page_unlock(pp);
				pp = pp->p_next;
				if (pp == first_pp) {
					pp = NULL;
				}
			}

			if (pp != NULL) {
				ASSERT(mtype == PP_2_MTYPE(pp));
				ASSERT(pp->p_szc == 0);

				/* found a page with specified DMA attributes */
				page_sub(&PAGE_CACHELISTS(mnode, bin,
							  mtype), pp);
				page_ctr_sub(mnode, mtype, pp, PG_CACHE_LIST);

				mutex_exit(pcm);
				ASSERT(pp->p_vnode);
				ASSERT(PP_ISAGED(pp) == 0);
				check_dma(dma_attr, pp, 1);
				return pp;
			}
			mutex_exit(pcm);
nextcachebin:
			bin += (i == 0) ? BIN_STEP : 1;
			bin &= page_colors_mask;
		}
		MTYPE_NEXT(mnode, mtype, flags);
	} while (mtype >= 0);

	return NULL;
}

/*
 * This function is similar to page_get_freelist()/page_get_cachelist()
 * but it searches both the lists to find a page with the specified
 * color (or no color) and DMA attributes. The search is done in the
 * freelist first and then in the cache list within the highest memory
 * range (based on DMA attributes) before searching in the lower
 * memory ranges.
 *
 * Note: This function is called only by page_create_io().
 */
/*ARGSUSED*/
static page_t *
page_get_anylist(struct vnode *vp, u_offset_t off, struct as *as, caddr_t vaddr,
		 size_t size, uint_t flags, ddi_dma_attr_t *dma_attr)
{
	uint_t		bin;
	page_t		*pp;
	int		n;
	int		m;
	int		fullrange;
	int		mnode;
	lgrp_t		*lgrp;

	/* only base pagesize currently supported */
	if (size != MMU_PAGESIZE) {
		return NULL;
	}

	/* LINTED */
	AS_2_BIN(as, seg, vp, vaddr, bin, 0);

	/*
	 * Only hold one freelist or cachelist lock at a time, that way we
	 * can start anywhere and not have to worry about lock
	 * ordering.
	 */
	if (dma_attr != NULL) {
		pfn_t pfnlo = mmu_btop(dma_attr->dma_attr_addr_lo);
		pfn_t pfnhi = mmu_btop(dma_attr->dma_attr_addr_hi);

		if (pfnlo >= pfnhi) {
			return NULL;
		}

		/*
		 * We can guarantee alignment only for page boundary.
		 */
		if (dma_attr->dma_attr_align > MMU_PAGESIZE) {
			return NULL;
		}
	}

	/*
	 * Allocate pages from high pfn to low.
	 * Note that ARM architecture has currently only one memory node.
	 */
	if (dma_attr == NULL) {
		pp = page_get_mnode_freelist(0, bin, 0, 0, flags);
		if (pp == NULL) {
			pp = page_get_mnode_cachelist(bin, flags, 0, 0);
		}
	}
	else {
		pp = page_get_mnode_anylist(bin, 0, flags, 0, 0, dma_attr);
	}
	if (pp != NULL) {
		check_dma(dma_attr, pp, 1);
		return pp;
	}

	/*
	 * Currently, ARM architecture doesn't support multiple
	 * locality groups.
	 */
	lgrp = lgrp_home_lgrp();
	lgrp_stat_add(lgrp->lgrp_id, LGRP_NUM_ALLOC_FAIL, 1);

	return NULL;
}

#define PAGE_HASH_SEARCH(index, pp, vp, off)				\
	do {								\
		for ((pp) = page_hash[(index)]; (pp) != NULL;		\
		     (pp) = (pp)->p_hash) {				\
			if ((pp)->p_vnode == (vp) &&			\
			    (pp)->p_offset == (off)) {			\
				break;					\
			}						\
		}							\
	} while (0)

/* Use this macro when no page satisfies the requisites. */
#define	PAGE_CREATE_IO_FAIL(warnflag)					\
	do {								\
		if (!warnflag) {					\
			atomic_inc_uint(&page_create_io_waiters);	\
			cmn_err(CE_WARN, "page_create_io: can't get page " \
				"that satisfies the requisites: "	\
				"freemem=0x%lx", freemem);		\
			(warnflag) = 1;					\
		}							\
									\
		/* Kick pageout scanner. */				\
		cv_signal(&proc_pageout->p_cv);				\
									\
		delay(10);						\
	} while (0)

/*
 * page_t *
 * page_create_io(struct vnode *vp, u_offset_t off, uint_t bytes,
 *                uint_t flags, struct as *as, caddr_t vaddr,
 *                ddi_dma_attr_t *mattr)
 *
 * This function is a copy of page_create_va() with an additional
 * argument 'mattr' that specifies DMA memory requirements to
 * the page list functions. This function is used by the segkmem
 * allocator so it is only to create new pages (i.e PG_EXCL is set).
 *
 * return:
 *    ptr of a list of a physics page for appointed size.
 */
page_t *
page_create_io(struct vnode *vp, u_offset_t off, uint_t bytes, uint_t flags,
	       struct as *as, caddr_t vaddr, ddi_dma_attr_t *mattr)
{
	page_t		*plist = NULL;
	uint_t		plist_len = 0;
	pgcnt_t		npages;
	page_t		*npp = NULL;
	uint_t		pages_req;
	page_t		*pp;
	kmutex_t	*phm = NULL;
	uint_t		index;
	int		warn = 0;

	TRACE_4(TR_FAC_VM, TR_PAGE_CREATE_START,
		"page_create_start:vp %p off %llx bytes %u flags %x",
		vp, off, bytes, flags);

	ASSERT((flags & ~(PG_EXCL | PG_WAIT | PG_PHYSCONTIG)) == 0);

 tryagain:
	pages_req = npages = mmu_btopr(bytes);

	/*
	 * Do the freemem and pcf accounting.
	 */
	if (!page_create_wait(npages, flags)) {
		return NULL;
	}

	TRACE_2(TR_FAC_VM, TR_PAGE_CREATE_SUCCESS,
		"page_create_success:vp %p off %llx",
		vp, off);

	/*
	 * If satisfying this request has left us with too little
	 * memory, start the wheels turning to get some back.  The
	 * first clause of the test prevents waking up the pageout
	 * daemon in situations where it would decide that there's
	 * nothing to do.
	 */
	if (DESPERATE_FREEMEM() || (nscan < desscan && freemem < minfree)) {
		TRACE_1(TR_FAC_VM, TR_PAGEOUT_CV_SIGNAL,
			"pageout_cv_signal:freemem %ld", freemem);
		cv_signal(&proc_pageout->p_cv);
	}

	if (flags & PG_PHYSCONTIG) {
		while ((plist = page_get_contigpage(&npages, mattr, 1)) ==
		       NULL) {
			if (!(flags & PG_WAIT)) {
				page_create_putback(npages);
				return NULL;
			}

			/*
			 * No page satisfies the requisites.
			 * Try again until we can get.
			 */
			PAGE_CREATE_IO_FAIL(warn);
		}

		pp = plist;

		do {
			if (!page_hashin(pp, vp, off, NULL)) {
				panic("pg_creat_io: hashin failed %p %p %llx",
				    (void *)pp, (void *)vp, off);
			}
			VM_STAT_ADD(page_create_new);
			off += MMU_PAGESIZE;
			PP_CLRFREE(pp);
			PP_CLRAGED(pp);
			page_set_props(pp, P_REF);
			pp = pp->p_next;
		} while (pp != plist);

		if (!npages) {
			check_dma(mattr, plist, pages_req);
			if (warn) {
				atomic_dec_uint(&page_create_io_waiters);
			}
			return plist;
		}
		else {
			vaddr += (pages_req - npages) << MMU_PAGESHIFT;
		}

		/*
		 * fall-thru:
		 *
		 * page_get_contigpage returns when npages <= sgllen.
		 * Grab the rest of the non-contig pages below from anylist.
		 */
	}

	/*
	 * Loop around collecting the requested number of pages.
	 * Most of the time, we have to `create' a new page. With
	 * this in mind, pull the page off the free list before
	 * getting the hash lock.  This will minimize the hash
	 * lock hold time, nesting, and the like.  If it turns
	 * out we don't need the page, we put it back at the end.
	 */
	while (npages--) {
		phm = NULL;

		index = PAGE_HASH_FUNC(vp, off);
top:
		ASSERT(phm == NULL);
		ASSERT(index == PAGE_HASH_FUNC(vp, off));
		ASSERT(MUTEX_NOT_HELD(page_vnode_mutex(vp)));

		if (npp == NULL) {
			/*
			 * Try to get the page of any color either from
			 * the freelist or from the cache list.
			 */
			npp = page_get_anylist(vp, off, as, vaddr, MMU_PAGESIZE,
					       flags & ~PG_MATCH_COLOR, mattr);
			if (npp == NULL) {
				/*
				 * No page found! This can happen
				 * if we are looking for a page
				 * within a specific memory range
				 * for DMA purposes. If PG_WAIT is
				 * specified then we wait for a
				 * while and then try again. The
				 * wait could be forever if we
				 * don't get the page(s) we need.
				 *
				 * Note: XXX We really need a mechanism
				 * to wait for pages in the desired
				 * range. For now, we wait for any
				 * pages and see if we can use it.
				 */
				if (flags & PG_WAIT) {
					PAGE_CREATE_IO_FAIL(warn);
					goto top;
				}
				goto fail; /* undo accounting stuff */
			}

			if (PP_ISAGED(npp) == 0) {
				/*
				 * Since this page came from the
				 * cachelist, we must destroy the
				 * old vnode association.
				 */
				page_hashout(npp, (kmutex_t *)NULL);
			}
		}

		/*
		 * We own this page!
		 */
		ASSERT(PAGE_EXCL(npp));
		ASSERT(npp->p_vnode == NULL);
		ASSERT(!hat_page_is_mapped(npp));
		PP_CLRFREE(npp);
		PP_CLRAGED(npp);

		/*
		 * Here we have a page in our hot little mits and are
		 * just waiting to stuff it on the appropriate lists.
		 * Get the mutex and check to see if it really does
		 * not exist.
		 */
		phm = PAGE_HASH_MUTEX(index);
		mutex_enter(phm);
		PAGE_HASH_SEARCH(index, pp, vp, off);
		if (pp == NULL) {
			VM_STAT_ADD(page_create_new);
			pp = npp;
			npp = NULL;
			if (!page_hashin(pp, vp, off, phm)) {
				/*
				 * Since we hold the page hash mutex and
				 * just searched for this page, page_hashin
				 * had better not fail.  If it does, that
				 * means somethread did not follow the
				 * page hash mutex rules.  Panic now and
				 * get it over with.  As usual, go down
				 * holding all the locks.
				 */
				ASSERT(MUTEX_HELD(phm));
				panic("page_create_io: hashin fail %p %p "
				      "%llx %p", (void *)pp, (void *)vp, off,
				      (void *)phm);
			}
			ASSERT(MUTEX_HELD(phm));
			mutex_exit(phm);
			phm = NULL;

			/*
			 * Hat layer locking need not be done to set
			 * the following bits since the page is not hashed
			 * and was on the free list (i.e., had no mappings).
			 *
			 * Set the reference bit to protect
			 * against immediate pageout
			 *
			 * XXXmh modify freelist code to set reference
			 * bit so we don't have to do it here.
			 */
			page_set_props(pp, P_REF);
		}
		else {
			ASSERT(MUTEX_HELD(phm));
			mutex_exit(phm);
			phm = NULL;
			/*
			 * NOTE: This should not happen for pages associated
			 *	 with kernel vnode 'kvp'.
			 */
			/* XX64 - to debug why this happens! */
			ASSERT(!VN_ISKAS(vp));
			if (VN_ISKAS(vp)) {
				cmn_err(CE_NOTE,
					"page_create_io: page not expected "
					"in hash list for kernel vnode - pp "
					"0x%p", (void *)pp);
			}
			VM_STAT_ADD(page_create_exists);
			goto fail;
		}

		/*
		 * Got a page!  It is locked.  Acquire the i/o
		 * lock since we are going to use the p_next and
		 * p_prev fields to link the requested pages together.
		 */
		page_io_lock(pp);
		page_add(&plist, pp);
		plist = plist->p_next;
		off += MMU_PAGESIZE;
		vaddr += MMU_PAGESIZE;
	}

	check_dma(mattr, plist, pages_req);
	if (warn) {
		atomic_dec_uint(&page_create_io_waiters);
	}
	return plist;

fail:
	if (npp != NULL) {
		/*
		 * Did not need this page after all.
		 * Put it back on the free list.
		 */
		VM_STAT_ADD(page_create_putbacks);
		PP_SETFREE(npp);
		PP_SETAGED(npp);
		npp->p_offset = (u_offset_t)-1;
		page_list_add(npp, PG_FREE_LIST | PG_LIST_TAIL);
		page_unlock(npp);
	}

	/*
	 * Give up the pages we already got.
	 */
	while (plist != NULL) {
		pp = plist;
		page_sub(&plist, pp);
		page_io_unlock(pp);
		plist_len++;
		/*LINTED: constant in conditional ctx*/
		VN_DISPOSE(pp, B_INVAL, 0, kcred);
	}

	/*
	 * VN_DISPOSE does freemem accounting for the pages in plist
	 * by calling page_free. So, we need to undo the pcf accounting
	 * for only the remaining pages.
	 */
	VM_STAT_ADD(page_create_putbacks);
	page_create_putback(pages_req - plist_len);

	if (flags & PG_WAIT) {
		/*
		 * Can't allocate new pages. Try again from the start.
		 *
		 * Remarks:
		 *	Currently, this should not happen because all
		 *	page_create_io() callers specify kvp as vnode.
		 */
		plist = NULL;
		plist_len = 0;
		npp = NULL;
		phm = NULL;
		goto tryagain;
	}

	ASSERT(!warn);
	return NULL;
}

/*
 * static void
 * contig_alloc_map(caddr_t addr, size_t size, page_t *ppl, uint_t attr,
 *		    int pflag)
 *	Internal function for contig_alloc() to create mapping.
 */
static void
contig_alloc_map(caddr_t addr, size_t size, page_t *ppl, uint_t attr, int pflag)
{
	struct hat	*khat = kas.a_hat;
	const uint_t	hflags = HAT_LOAD_LOCK|HAT_LOAD_NOCONSIST;

	ASSERT(ppl != NULL);

	if (pflag & CA_DMA) {
		attr &= ~HAT_ORDER_MASK;
		attr |= ARMPF_DMA_HAT_FLAGS;
	}
	if (pflag & PG_PHYSCONTIG) {
		pgcnt_t	cnt = 0;
		page_t	*pp, *basepp = ppl;

		/*
		 * Map contiguous pages together to reduce hat_devload() call.
		 */
		do {
			page_t	*p;

			pp = ppl;
			page_sub(&ppl, pp);
			ASSERT(page_iolock_assert(pp));
			page_io_unlock(pp);
			if (pp == basepp + cnt) {
				cnt++;
				continue;
			}
			hat_devload(khat, (caddr_t)(uintptr_t)basepp->p_offset,
				    ptob(cnt), basepp->p_pagenum, attr, 
				    hflags);
			for (p = basepp; p < basepp + cnt; p++) {
				ASSERT(PAGE_EXCL(p));
				if (pflag & CA_DMA) {
					PP_ARM_SETDMA(p);
				}
				page_unlock(p);
			}
			cnt = 1;
			basepp = pp;
		} while (ppl != NULL);

		hat_devload(khat, (caddr_t)(uintptr_t)basepp->p_offset,
			    ptob(cnt), basepp->p_pagenum, attr, 
			    hflags);
		for (pp = basepp; pp < basepp + cnt; pp++) {
			ASSERT(PAGE_EXCL(pp));
			if (pflag & CA_DMA) {
				PP_ARM_SETDMA(pp);
			}
			page_unlock(pp);
		}
	}
	else {
		do {
			page_t	*pp = ppl;

			page_sub(&ppl, pp);
			ASSERT(page_iolock_assert(pp));
			page_io_unlock(pp);
			hat_devload(khat, (caddr_t)(uintptr_t)pp->p_offset,
				    PAGESIZE, pp->p_pagenum, attr, hflags);

			ASSERT(PAGE_EXCL(pp));
			if (pflag & CA_DMA) {
				PP_ARM_SETDMA(pp);
			}
			page_unlock(pp);
		} while (ppl != NULL);
	}
}

/*
 * void *
 * contig_alloc(size_t size, ddi_dma_attr_t *mattr, uintptr_t align,
 *		uint_t attr, int flags)
 *
 *	allocates contiguous memory to satisfy the 'size' and dma attributes
 *	specified in 'attr'.
 *
 *	Not all of memory need to be physically contiguous if the
 *	scatter-gather list length is greater than 1.
 *
 *	The following flags can be specified to flags:
 *	  KM_SLEEP:	The caller sleeps until memory is available.
 *	  KM_NOSLEEP:	contig_alloc() returns NULL if no memory is available.
 *	  CA_DMA:	contig_alloc() allocates DMA buffer.
 *
 *  return:
 *     An address of the virtual memory that a physics page was mapped.
 */
/*ARGSUSED*/
void *
contig_alloc(size_t size, ddi_dma_attr_t *mattr, uintptr_t align, uint_t attr,
	     int flags)
{
	pgcnt_t		pgcnt = btopr(size);
	size_t		asize = ptob(pgcnt);
	page_t		*ppl;
	int		pflag, vmflag;
	void		*addr;

	/* NOTE: The value of VM_NOSLEEP is the same as KM_NOSLEEP. */
	vmflag = (flags & KM_NOSLEEP);
	if (align <= PAGESIZE) {
		addr = vmem_alloc(heap_arena, asize, vmflag);
	}
	else {
		addr = vmem_xalloc(heap_arena, asize, align, 0, 0, NULL, NULL,
				   vmflag);
	}

	if (addr) {
		ASSERT(!((uintptr_t)addr & (align - 1)));

		if (page_resv(pgcnt, vmflag) == 0) {
			vmem_free(heap_arena, addr, asize);
			return NULL;
		}
		pflag = PG_EXCL;

		if (!(flags & KM_NOSLEEP)) {
			pflag |= PG_WAIT;
		}

		/* 4k req gets from freelists rather than pfn search */
		if (pgcnt > 1 || align > PAGESIZE ||
		    (mattr != NULL && mattr->dma_attr_align > MMU_PAGESIZE)) {
			pflag |= PG_PHYSCONTIG;
		}

		ppl = page_create_io(&kvp, (u_offset_t)(uintptr_t)addr,
				     asize, pflag, &kas, (caddr_t)addr, mattr);

		if (!ppl) {
			vmem_free(heap_arena, addr, asize);
			page_unresv(pgcnt);
			return NULL;
		}

		pflag |= (flags & CA_DMA);
		contig_alloc_map(addr, asize, ppl, attr, pflag);

		if (flags & CA_DMA) {
			/* Invalidate cache lines. */
			sync_data_memory(addr, asize);
			CACHE_L220_FLUSH(addr, asize, L220_FLUSH);
		}
	}

	return addr;
}

/*
 * void
 * contig_free(void *addr, size_t size)
 *
 *	free contiguous memory.
 */
void
contig_free(void *addr, size_t size)
{
	pgcnt_t	pgcnt = btopr(size);
	size_t	asize = ptob(pgcnt);
	caddr_t	a, ea;
	page_t	*pp;

	hat_unload(kas.a_hat, addr, asize, HAT_UNLOAD_UNLOCK);

	for (a = addr, ea = a + asize; a < ea; a += PAGESIZE) {
		pp = page_lookup(&kvp, (u_offset_t)(uintptr_t)a, SE_EXCL);
		if (pp == NULL) {
			panic("contig_free: page freed");
		}
		PP_ARM_CLRDMA(pp);
		page_destroy(pp, 0);
	}

	page_unresv(pgcnt);
	vmem_free(heap_arena, addr, asize);
}

/*
 * void
 * add_physmem_dontfree(page_t *pp, pgcnt_t count, pfn_t pfn)
 *	Add physical chunk of memory to the system during bootstrap.
 *	add_physmem_dontfree() is similar to add_physmem(), but it doesn't
 *	put pages into page free list.
 *
 * Remarks:
 *	Currently, add_physmem_dontfree() assumes that the system doesn't
 *	support large page.
 *
 *	Note that p_lckcnt in each page is initialized as 1.
 */
void
add_physmem_dontfree(page_t *pp, pgcnt_t count, pfn_t pfn)
{
	extern void	set_max_page_get(pgcnt_t total);

	ASSERT(page_num_pagesizes() == 1);

	/* Update total number of pages. */
	total_pages += count;
	set_max_page_get(total_pages);
	PLCNT_MODIFY_MAX(pfn, (long)count);

	/* Iterate each pages in the specified range. */
	for (; count != 0; pp++, pfn++, count--) {
		add_physmem_cb(pp, pfn);

		/* Initialize any page lock state as unlocked. */
		page_iolock_init(pp);

		pp->p_offset = (u_offset_t)-1;
		pp->p_next = pp->p_prev = pp;

		/* Protect from pageout scanner. */
		pp->p_lckcnt = 1;

		/*
		 * Other fields are already initialized as zero.
		 * Nothing to do here.
		 */
	}
}
