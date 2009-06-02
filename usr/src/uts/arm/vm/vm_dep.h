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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2006-2008 NEC Corporation
 */

/*
 * UNIX ARM dependent virtual memory support.
 * This header is used for kernel build only.
 */

#ifndef	_VM_DEP_H
#define	_VM_DEP_H

#ident	"@(#)arm/vm/vm_dep.h"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/cpuvar.h>
#include <sys/cache_l220.h>
#include <vm/page.h>
#include <sys/pte.h>

/*
 * WARNING: vm_dep.h is included by files in common. As such, macros
 * dependent upon platform cannot be used in this file.
 */

/*
 * arm_gettick() must be implemented as function because GETTICK() is used
 * from common source.
 */
#define	GETTICK()	arm_gettick()

/*
 * Return the log2(pagesize(szc) / MMU_PAGESIZE) --- or the shift count
 * for the number of base pages in this pagesize
 */
#define	PAGE_BSZS_SHIFT(szc)	((szc) << 2)

#define	MAX_MEM_TYPES	1		/* only one memory type */

#ifdef	_MACHDEP
/*
 * page list count per mnode.
 */
typedef struct {
	/* maintain page list stats */
	pgcnt_t	plc_mt_pgmax;		/* mnode/mtype max page cnt */
	pgcnt_t	plc_mt_clpgcnt;		/* cache list cnt */
	pgcnt_t	plc_mt_flpgcnt;		/* free list cnt - small pages */
	pgcnt_t	plc_mt_lgpgcnt;		/* free list cnt - large pages */
#ifdef	DEBUG
	struct plc_mts {		/* mnode/mtype szc stats */
		pgcnt_t	plc_mts_pgcnt;	/* per page size count */
		int	plc_mts_colors;
		pgcnt_t *plc_mtsc_pgcnt;	/* per color bin count */
	} plc_mts[MMU_PAGE_SIZES];
#endif	/* DEBUG */
} plcnt_t;

#ifdef	DEBUG
#define	PLCNT_SZ(ctrs_sz)						\
	do {								\
		int __szc;						\
		for (__szc = 0; __szc < mmu_page_sizes; __szc++) {	\
			int __colors = page_get_pagecolors(__szc);	\
			ctrs_sz += sizeof(pgcnt_t) * __colors;		\
		}							\
	} while (0)


#define	PLCNT_INIT(addr)						\
	do {								\
		int __szc;						\
		for (__szc = 0; __szc < mmu_page_sizes; __szc++) {	\
			int __colors = page_get_pagecolors(__szc);	\
			plcnt.plc_mts[__szc].plc_mts_colors = __colors;	\
			plcnt.plc_mts[__szc].plc_mtsc_pgcnt =		\
				(pgcnt_t *)addr;			\
			addr += (sizeof(pgcnt_t) * __colors);		\
		}							\
	} while (0)

#define	PLCNT_DO(pp, mtype, szc, cnt, flags)				\
	do {								\
		int	__bin = PP_2_BIN(pp);				\
		if ((flags) & PG_CACHE_LIST) {				\
			atomic_add_long(&plcnt.plc_mt_clpgcnt, cnt);	\
		}							\
		else if (szc) {						\
			atomic_add_long(&plcnt.plc_mt_lgpgcnt, cnt);	\
		}							\
		else {							\
			atomic_add_long(&plcnt.plc_mt_flpgcnt, cnt);	\
		}							\
		atomic_add_long(&plcnt.plc_mts[szc].plc_mts_pgcnt, cnt); \
		atomic_add_long(&plcnt.plc_mts[szc].			\
				plc_mtsc_pgcnt[__bin], cnt);		\
	} while (0)

#else	/* !DEBUG */

#define	PLCNT_SZ(ctrs_sz)

#define	PLCNT_INIT(addr)

#define	PLCNT_DO(pp, mtype, szc, cnt, flags)				\
	do {								\
		if ((flags) & PG_CACHE_LIST) {				\
			atomic_add_long(&plcnt.plc_mt_clpgcnt, cnt);	\
		}							\
		else if (szc) {						\
			atomic_add_long(&plcnt.plc_mt_lgpgcnt, cnt);	\
		}							\
		else {							\
			atomic_add_long(&plcnt.plc_mt_flpgcnt, cnt);	\
		}							\
	} while (0)

#endif	/* DEBUG */

#define	PLCNT_INCR(pp, mnode, mtype, szc, flags)			\
	do {								\
		long	__cnt = (1 << PAGE_BSZS_SHIFT(szc));		\
		ASSERT(mtype == PP_2_MTYPE(pp));			\
		PLCNT_DO(pp, mtype, szc, __cnt, flags);			\
	} while (0)

#define	PLCNT_DECR(pp, mnode, mtype, szc, flags)			\
	do {								\
		long	__cnt = ((-1) << PAGE_BSZS_SHIFT(szc));		\
		ASSERT(mtype == PP_2_MTYPE(pp));			\
		PLCNT_DO(pp, mtype, szc, __cnt, flags);			\
	} while (0)

/*
 * macros to update page list max counts.  no-op on ARM.
 */
#define	PLCNT_XFER_NORELOC(pp)

/*
 * Macro to modify the page list max counts when memory is added to
 * the page lists during startup (add_physmem).
 */
#define	PLCNT_MODIFY_MAX(pfn, cnt)			\
	atomic_add_long(&plcnt.plc_mt_pgmax, (cnt))

extern plcnt_t		plcnt;
#endif	/* _MACHDEP */

/*
 * Per page size free lists. Allocated dynamically.
 * dimensions [mmu_page_sizes][colors]
 */
extern page_t ***page_freelists;

#define	PAGE_FREELISTS(mnode, szc, color, mtype)		\
	(*(page_freelists[szc] + (color)))

/*
 * For now there is only a single size cache list. Allocated dynamically.
 * dimensions [colors]
 */
extern page_t **page_cachelists;

#define	PAGE_CACHELISTS(mnode, color, mtype) 		\
	(*(page_cachelists + (color)))

/*
 * There are mutexes for both the page freelist
 * and the page cachelist.  We want enough locks to make contention
 * reasonable, but not too many -- otherwise page_freelist_lock() gets
 * so expensive that it becomes the bottleneck!
 */

#define	NPC_MUTEX	16

extern kmutex_t	*fpc_mutex[NPC_MUTEX];
extern kmutex_t	*cpc_mutex[NPC_MUTEX];

extern page_t *page_get_mnode_freelist(int, uint_t, int, uchar_t, uint_t);
extern page_t *page_get_mnode_cachelist(uint_t, uint_t, int, int);

#define PFN_2_COLOR(pfn, szc, it)			\
	(((pfn) & page_colors_mask) >> PNUM_SHIFT(szc))

#define	PNUM_SIZE(szc)			(hw_page_array[(szc)].hp_pgcnt)
#define	PNUM_SHIFT(szc)							\
	(hw_page_array[(szc)].hp_shift - hw_page_array[0].hp_shift)
#define	PAGE_GET_SHIFT(szc)		(hw_page_array[(szc)].hp_shift)
#define	PAGE_GET_PAGECOLORS(szc)	(hw_page_array[(szc)].hp_colors)

/*
 * This macro calculates the next sequential pfn with the specified
 * color using color equivalency mask
 */
#define PAGE_NEXT_PFN_FOR_COLOR(pfn, szc, color, ceq_mask, color_mask, it) \
        do {								\
		uint_t	__pfn_shift = PAGE_BSZS_SHIFT(szc);		\
                pfn_t	__spfn = (pfn) >> __pfn_shift;			\
		pfn_t	__stride = (ceq_mask) + 1;			\
									\
		ASSERT(((color) & ~(ceq_mask)) == 0);			\
		ASSERT((((ceq_mask) + 1) & (ceq_mask)) == 0);		\
		if (((__spfn ^ (color)) & (ceq_mask)) == 0) {		\
			(pfn) += __stride << __pfn_shift;		\
                }							\
		else {							\
			pfn_t	__p;					\
									\
                        (pfn) = (__spfn & ~(pfn_t)(ceq_mask)) | (color); \
                        __p = ((pfn) > __spfn)				\
				? (pfn)					\
				: ((pfn) + __stride);			\
			(pfn) = __p << __pfn_shift;			\
		}							\
	} while (0)

/* get the color equivalency mask for the next szc */
#define PAGE_GET_NSZ_MASK(szc, mask)					\
	((mask) >> (PAGE_GET_SHIFT((szc) + 1) - PAGE_GET_SHIFT(szc)))
 
/* get the color of the next szc */
#define PAGE_GET_NSZ_COLOR(szc, color)					\
	((color) >> (PAGE_GET_SHIFT((szc) + 1) - PAGE_GET_SHIFT(szc)))

/* Find the bin for the given page if it was of size szc */
#define	PP_2_BIN_SZC(pp, szc)	PFN_2_COLOR(pp->p_pagenum, szc, NULL)

#define	PP_2_BIN(pp)		(PP_2_BIN_SZC(pp, pp->p_szc))

#define	PP_2_MEM_NODE(pp)	(PFN_2_MEM_NODE(pp->p_pagenum))
#define	PP_2_MTYPE(pp)		(0)
#define	PP_2_SZC(pp)		(pp->p_szc)

#define	SZCPAGES(szc)		(1 << PAGE_BSZS_SHIFT(szc))
#define	PFN_BASE(pfnum, szc)	((pfnum) & ~(SZCPAGES(szc) - 1))

struct page_list_walker;
typedef struct page_list_walker	page_list_walker_t;

#ifdef	_MACHDEP
/*
 * this structure is used for walking free page lists
 * controls when to split large pages into smaller pages,
 * and when to coalesce smaller pages into larger pages
 */
struct page_list_walker {
	uint_t	plw_colors;		/* num of colors for szc */
	uint_t	plw_color_mask;		/* colors-1 */
	uint_t	plw_bin_step;		/* next bin: 1 or 2 */
	uint_t	plw_count;		/* loop count */
	uint_t	plw_bin0;		/* starting bin */
	uint_t	plw_bin_marker;		/* bin after initial jump */
	uint_t	plw_bin_split_prev;	/* last bin we tried to split */
	uint_t	plw_do_split;		/* set if OK to split */
	uint_t	plw_split_next;		/* next bin to split */
	uint_t	plw_ceq_dif;		/* number of different color groups
					   to check */
	uint_t	plw_ceq_mask[MMU_PAGE_SIZES + 1];	/* color equiv mask */
	uint_t	plw_bins[MMU_PAGE_SIZES + 1];		/* num of bins */
};
#endif	/* _MACHDEP */

extern void	page_list_walk_init(uchar_t szc, uint_t flags, uint_t bin,
				    int can_split, int use_ceq,
				    page_list_walker_t *plw);
extern uint_t	page_list_walk_next_bin(uchar_t szc, uint_t bin,
					page_list_walker_t *plw);

/* Convert PAGESHIFT value to page size code. */
#define	ARM_PAGESHIFT_SZC(shft)	(((shft) - MMU_PAGESHIFT) >> 2)

#define	SZC_SPSECTION	ARM_PAGESHIFT_SZC(L1PT_SPSECTION_VSHIFT)
#define	SZC_SECTION	ARM_PAGESHIFT_SZC(L1PT_SECTION_VSHIFT)
#define	SZC_LARGE	ARM_PAGESHIFT_SZC(L2PT_LARGE_VSHIFT)
#define	SZC_SMALL	(0)

extern cpu_t		*cpu_boot;
#define	CPU0		cpu_boot

/*
 * set the mtype range
 */

#define	MTYPE_INIT(mtype, vp, vaddr, flags, pgsz)	\
	do {						\
		(mtype) = 0;				\
	} while (0)

/*
 * macros to loop through the mtype range (page_get_mnode_{free,cache,any}list,
 * and page_get_contig_pages)
 *
 * MTYPE_START sets the initial mtype. -1 if the mtype range specified does
 * not contain mnode.
 *
 * MTYPE_NEXT sets the next mtype. -1 if there are no more valid
 * mtype in the range.
 */

/* Only one mtype. */
#define	MTYPE_START(mnode, mtype, flags)	\
	do {					\
		(mtype) = 0;			\
	} while (0)

#define	MTYPE_NEXT(mnode, mtype, flags)		\
	do {					\
		(mtype) = -1;			\
	} while (0)

/* mtype init for page_get_replacement_page */

#define	MTYPE_PGR_INIT(mtype, flags, pp, mnode, pgcnt)	\
	do {						\
		mtype = 0;				\
	} while (0)

#define	MNODE_PGCNT(mnode)						\
	(plcnt.plc_mt_clpgcnt + plcnt.plc_mt_flpgcnt + plcnt.plc_mt_lgpgcnt)

#define	MNODETYPE_2_PFN(mnode, mtype, pfnlo, pfnhi)			\
	do {								\
		ASSERT(mtype == 0);					\
		pfnlo = mem_node_config[0].physbase;			\
		pfnhi = mem_node_config[0].physmax;			\
	} while (0)

/*
 * candidate counters in vm_pagelist.c are indexed by color and range
 */
#define	MAX_MNODE_MRANGES		(1)
#define	MNODE_RANGE_CNT(mnode)		MAX_MNODE_MRANGES
#define	MNODE_MAX_MRANGE(mnode)		(MAX_MEM_TYPES - 1)
#define	MTYPE_2_MRANGE(mnode, mtype)	(mtype)

/* mem node iterator is not used on ARM */
#define MEM_NODE_ITERATOR_DECL(it)
#define MEM_NODE_ITERATOR_INIT(pfn, mnode, szc, it)

/*
 * interleaved_mnodes mode is never set on ARM, therefore,
 * simply return the limits of the given mnode, which then
 * determines the length of hpm_counters array for the mnode.
 */
#define HPM_COUNTERS_LIMITS(mnode, physbase, physmax, first)\
	do {							\
		(physbase) = mem_node_config[(mnode)].physbase;	\
		(physmax) = mem_node_config[(mnode)].physmax;	\
		(first) = (mnode);				\
        } while (0)

#define PAGE_CTRS_WRITE_LOCK(mnode)					\
	do {								\
		rw_enter(&page_ctrs_rwlock[(mnode)], RW_WRITER);	\
		page_freelist_lock(mnode);				\
        } while (0)

#define PAGE_CTRS_WRITE_UNLOCK(mnode)				   \
        do {							   \
		page_freelist_unlock(mnode);			   \
		rw_exit(&page_ctrs_rwlock[(mnode)]);		   \
        } while (0)


#define PAGE_GET_COLOR_SHIFT(szc, nszc)					\
	(hw_page_array[(nszc)].hp_shift - hw_page_array[(szc)].hp_shift)

#define PAGE_CONVERT_COLOR(ncolor, szc, nszc)			\
	((ncolor) << PAGE_GET_COLOR_SHIFT((szc), (nszc)))

#define	PC_BIN_MUTEX(mnode, bin, flags) \
	(((flags) & PG_FREE_LIST)					\
	 ? &fpc_mutex[(bin) & (NPC_MUTEX - 1)][mnode]			\
	 : &cpc_mutex[(bin) & (NPC_MUTEX - 1)][mnode])

#define	FPC_MUTEX(mnode, i)	(&fpc_mutex[i][mnode])
#define	CPC_MUTEX(mnode, i)	(&cpc_mutex[i][mnode])

#if	defined(DEBUG) && !defined(LPG_DISABLE)
#define	CHK_LPG(pp, szc)	chk_lpg(pp, szc)
extern void	chk_lpg(page_t *, uchar_t);
#else	/* !(DEBUG && !LPG_DISABLE) */
#define	CHK_LPG(pp, szc)
#endif	/* defined(DEBUG) && !defined(LPG_DISABLE) */

/*
 * For ARM each larger page is 16 times the size of the previous size page.
 */
#define	FULL_REGION_CNT(rg_szc)		(8)

/* Return the leader for this mapping size */
#define	PP_GROUPLEADER(pp, szc) \
	(&(pp)[-(int)((pp)->p_pagenum & (SZCPAGES(szc)-1))])

/* Return the root page for this page based on p_szc */
#define	PP_PAGEROOT(pp) ((pp)->p_szc == 0 ? (pp) : \
	PP_GROUPLEADER((pp), (pp)->p_szc))

/*
 * The counter base must be per page_counter element to prevent
 * races when re-indexing, and the base page size element should
 * be aligned on a boundary of the given region size.
 *
 * We also round up the number of pages spanned by the counters
 * for a given region to PC_BASE_ALIGN in certain situations to simplify
 * the coding for some non-performance critical routines.
 */

#define	PC_BASE_ALIGN		((pfn_t)1 << PAGE_BSZS_SHIFT(MMU_PAGE_SIZES-1))
#define	PC_BASE_ALIGN_MASK	(PC_BASE_ALIGN - 1)

/*
 * cpu/mmu-dependent vm variables
 */
#ifdef	LPG_DISABLE
#define	mmu_page_sizes			(1)
#define	mmu_exported_page_sizes		(1)
#else	/* !LPG_DISABLE */
extern uint_t mmu_page_sizes;
extern uint_t mmu_exported_page_sizes;
#endif	/* LPG_DISABLE */

#define	mmu_legacy_page_sizes	mmu_exported_page_sizes

/* For ARM, userszc is the same as the kernel's szc */
#define	USERSZC_2_SZC(userszc)	(userszc)
#define	SZC_2_USERSZC(szc)	(szc)

/*
 * for hw_page_map_t, sized to hold the ratio of large page to base
 * pagesize (1024 max)
 */
typedef	short	hpmctr_t;

#ifdef	_MACHDEP
/*
 * get the setsize of the current cpu
 */
#define	L2CACHE_ALIGN		L220_LINESIZE
#define	L2CACHE_ALIGN_MAX	L220_LINESIZE
#define	CPUSETSIZE()		L220_WAYSIZE
#endif	/* _MACHDEP */

/*
 * Internal PG_ flags.
 * Unused flags on ARM are defined as zero.
 */
#define	PGI_RELOCONLY	0		/* opposite of PG_NORELOC */
#define	PGI_NOCAGE	0		/* cage is disabled */
#ifdef	LPG_DISABLE
#define	PGI_PGCPHIPRI	0		/* page_get_contig_page pri alloc */
#else	/* !LPG_DISABLE */
#define	PGI_PGCPHIPRI	0x040000	/* page_get_contig_page pri alloc */
#endif	/* LPG_DISABLE */
#define	PGI_PGCPSZC0	0		/* relocate base pagesize page */

/*
 * Disable page relocation if there is only one locality group and
 * large page is disabled.
 */
#if	defined(LGROUP_SINGLE) && defined(LPG_DISABLE)
#define	PGRELOC_DISABLE		1
#endif	/* defined(LGROUP_SINGLE) && defined(LPG_DISABLE) */

/*
 * hash as and addr to get a bin.
 */
#define	AS_2_BIN(as, seg, vp, addr, bin, szc)				\
	bin = (((((uintptr_t)(addr) >> PAGESHIFT) + ((uintptr_t)(as) >> 4)) \
		& page_colors_mask) >> PNUM_SHIFT(szc))

/*
 * cpu private vm data - accessed thru CPU->cpu_vm_data
 *	vc_pnum_memseg: tracks last memseg visited in page_numtopp_nolock()
 *	vc_pnext_memseg: tracks last memseg visited in page_nextn()
 *	vc_kmptr: orignal unaligned kmem pointer for this vm_cpu_data_t
 *	vc_kmsize: orignal kmem size for this vm_cpu_data_t
 */

typedef struct {
	struct memseg	*vc_pnum_memseg;
	struct memseg	*vc_pnext_memseg;
	void		*vc_kmptr;
	size_t		vc_kmsize;
} vm_cpu_data_t;

/* allocation size to ensure vm_cpu_data_t resides in its own cache line */
#define	VM_CPU_DATA_PADSIZE						\
	(P2ROUNDUP(sizeof (vm_cpu_data_t), L2CACHE_ALIGN_MAX))

/* for boot cpu before kmem is initialized */
#ifndef	_VM_CPU_DATA0_DECL
extern char	vm_cpu_data0[];
#endif	/* !_VM_CPU_DATA0_DECL */

extern uint_t	page_colors;

/*
 * When a bin is empty, and we can't satisfy a color request correctly,
 * we scan.  If we assume that the programs have reasonable spatial
 * behavior, then it will not be a good idea to use the adjacent color.
 * Using the adjacent color would result in virtually adjacent addresses
 * mapping into the same spot in the cache.  So, if we stumble across
 * an empty bin, skip a bunch before looking.  After the first skip,
 * then just look one bin at a time so we don't miss our cache on
 * every look. Be sure to check every bin.  Page_create() will panic
 * if we miss a page.
 *
 * This also explains the `<=' in the for loops in both page_get_freelist()
 * and page_get_cachelist().  Since we checked the target bin, skipped
 * a bunch, then continued one a time, we wind up checking the target bin
 * twice to make sure we get all of them bins.
 */
#define	BIN_STEP	19

#ifdef	_MACHDEP
#ifdef VM_STATS
struct vmm_vmstats_str {
	ulong_t pgf_alloc[MMU_PAGE_SIZES];	/* page_get_freelist */
	ulong_t pgf_allocok[MMU_PAGE_SIZES];
	ulong_t pgf_allocokrem[MMU_PAGE_SIZES];
	ulong_t pgf_allocfailed[MMU_PAGE_SIZES];
	ulong_t	pgf_allocdeferred;
	ulong_t	pgf_allocretry[MMU_PAGE_SIZES];
	ulong_t pgc_alloc;			/* page_get_cachelist */
	ulong_t pgc_allocok;
	ulong_t pgc_allocokrem;
	ulong_t pgc_allocokdeferred;
	ulong_t pgc_allocfailed;
	ulong_t	pgcp_alloc[MMU_PAGE_SIZES];	/* page_get_contig_pages */
	ulong_t	pgcp_allocfailed[MMU_PAGE_SIZES];
	ulong_t	pgcp_allocempty[MMU_PAGE_SIZES];
	ulong_t	pgcp_allocok[MMU_PAGE_SIZES];
	ulong_t	ptcp[MMU_PAGE_SIZES];		/* page_trylock_contig_pages */
	ulong_t	ptcpfreethresh[MMU_PAGE_SIZES];
	ulong_t	ptcpfailexcl[MMU_PAGE_SIZES];
	ulong_t	ptcpfailszc[MMU_PAGE_SIZES];
	ulong_t	ptcpfailcage[MMU_PAGE_SIZES];
	ulong_t	ptcpok[MMU_PAGE_SIZES];
	ulong_t	pgmf_alloc[MMU_PAGE_SIZES];	/* page_get_mnode_freelist */
	ulong_t	pgmf_allocfailed[MMU_PAGE_SIZES];
	ulong_t	pgmf_allocempty[MMU_PAGE_SIZES];
	ulong_t	pgmf_allocok[MMU_PAGE_SIZES];
	ulong_t	pgmc_alloc;			/* page_get_mnode_cachelist */
	ulong_t	pgmc_allocfailed;
	ulong_t	pgmc_allocempty;
	ulong_t	pgmc_allocok;
	ulong_t	pladd_free[MMU_PAGE_SIZES];	/* page_list_add/sub */
	ulong_t	plsub_free[MMU_PAGE_SIZES];
	ulong_t	pladd_cache;
	ulong_t	plsub_cache;
	ulong_t	plsubpages_szcbig;
	ulong_t	plsubpages_szc0;
	ulong_t pfs_req[MMU_PAGE_SIZES];	/* page_freelist_split */
	ulong_t pfs_demote[MMU_PAGE_SIZES];
	ulong_t pfc_coalok[MMU_PAGE_SIZES][MAX_MNODE_MRANGES];
	ulong_t	ppr_reloc[MMU_PAGE_SIZES];	/* page_relocate */
	ulong_t ppr_relocnoroot[MMU_PAGE_SIZES];
	ulong_t ppr_reloc_replnoroot[MMU_PAGE_SIZES];
	ulong_t ppr_relocnolock[MMU_PAGE_SIZES];
	ulong_t ppr_relocnomem[MMU_PAGE_SIZES];
	ulong_t ppr_relocok[MMU_PAGE_SIZES];
	ulong_t ppr_copyfail;
	/* page coalesce counter */
	ulong_t page_ctrs_coalesce[MMU_PAGE_SIZES][MAX_MNODE_MRANGES];
	/* candidates useful */
	ulong_t page_ctrs_cands_skip[MMU_PAGE_SIZES][MAX_MNODE_MRANGES];
	/* ctrs changed after locking */
	ulong_t page_ctrs_changed[MMU_PAGE_SIZES][MAX_MNODE_MRANGES];
	/* page_freelist_coalesce failed */
	ulong_t page_ctrs_failed[MMU_PAGE_SIZES][MAX_MNODE_MRANGES];
	ulong_t page_ctrs_coalesce_all;	/* page coalesce all counter */
	ulong_t page_ctrs_cands_skip_all; /* candidates useful for all func */
};
extern struct vmm_vmstats_str vmm_vmstats;
#endif	/* VM_STATS */
#endif	/* _MACHDEP */

/*
 * DMA buffer flag in struct page.
 * On ARM architecture, p_index in struct page is not used.
 * So we can use it as ARM private page flag field.
 * Page exclusive lock must be acquired to change p_index field.
 */
#define	P_ARM_DMA		0x01		/* used as DMA buffer */
#define	PP_ARM_SETDMA(pp)	((pp)->p_index |= P_ARM_DMA)
#define	PP_ARM_CLRDMA(pp)	((pp)->p_index &= ~P_ARM_DMA)
#define	PP_ARM_ISDMA(pp)	((pp)->p_index & P_ARM_DMA)

extern size_t page_ctrs_sz(void);
extern caddr_t page_ctrs_alloc(caddr_t);
extern void page_ctr_sub(int, int, page_t *, int);
extern page_t *page_freelist_split(uchar_t, uint_t, int, int, pfn_t,
				   page_list_walker_t *);
extern page_t *page_freelist_coalesce(int, uchar_t, uint_t, uint_t, int,
				      pfn_t);
extern uint_t page_get_pagecolors(uint_t);

#ifdef	__cplusplus
}
#endif

#endif	/* _VM_DEP_H */
