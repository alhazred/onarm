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

#ifndef _SYS_PTE_H
#define	_SYS_PTE_H

#ident	"@(#)pte.h"

#ifndef	_ASM
#include <sys/types.h>
#endif	/* !_ASM */
#include <sys/int_const.h>

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

/*
 * Definitions of page table entry (Only ARMv6 is supported)
 */

#ifndef	_ASM
typedef	uint32_t	l1pte_t;	/* First level page table entry */
typedef uint32_t	l2pte_t;	/* Second level page table entry */
#endif	/* !_ASM */

/*
 * =========================================================================
 * Level 1 page table definitions
 * =========================================================================
 */

/* Size of L1 page table entry */
#define	L1PT_PTE_SIZE		4

/* Size of L1 page table */
#define	L1PT_SHIFT		14
#define	L1PT_SIZE		(1 << L1PT_SHIFT)	/* 16K */

/* Number of ptes in L1 page table */
#define	L1PT_NPTES		(L1PT_SIZE / L1PT_PTE_SIZE)

/*
 * L1 page table parameters for user process are defined in pte_impl.h.
 */
#if	defined(_KERNEL) && defined(_KERNEL_BUILD_TREE)
#include <sys/pte_impl.h>
#endif	/* defined(_KERNEL) && defined(_KERNEL_BUILD_TREE) */

#define	L1PT_TYPEMASK		0x3
#define	L1PT_TYPE_INVALID	0x0	/* Raise translation fault */
#define	L1PT_TYPE_COARSE	0x1	/* Has L2PT entries */
#define	L1PT_TYPE_SECTION	0x2	/* Section (or subsection) mapping */

#ifndef	_ASM
#define	L1PT_PTE_TYPE(l1pte)	((l1pte) & L1PT_TYPEMASK)
#define	L1PT_PTE_IS_COARSE(l1pte)			\
	(L1PT_PTE_TYPE(l1pte) == L1PT_TYPE_COARSE)
#define	L1PT_PTE_IS_SECTION(l1pte)				\
	(L1PT_PTE_TYPE(l1pte) == L1PT_TYPE_SECTION &&		\
	 ((l1pte) & L1PT_SPSECTION) == 0)
#define	L1PT_PTE_IS_SPSECTION(l1pte)				\
	(L1PT_PTE_TYPE(l1pte) == L1PT_TYPE_SECTION &&		\
	 ((l1pte) & L1PT_SPSECTION))
#define	L1PT_PTE_IS_INVALID(l1pte)			\
	(L1PT_PTE_TYPE(l1pte) == L1PT_TYPE_INVALID)
#endif	/* !_ASM */

/* The following 3 bits are available on section and supersection. */
#define	L1PT_BUFFERABLE		0x4	/* Bufferable */
#define	L1PT_CACHED		0x8	/* Cached mapping */
#define	L1PT_XN			0x10	/* eXecute Never bit */

/*
 * Convert domain number in L1PT entry bits.
 * Available on coarse and section.
 */
#define	L1PT_DOMAIN_SHIFT	5
#define	L1PT_DOMAIN(domain)	((domain) << L1PT_DOMAIN_SHIFT)
#define	L1PT_DOMAIN_MASK	L1PT_DOMAIN(0xf)

#define	L1PT_P			0x200	/* ECC enabled */

/*
 * Convert AP value in L1PT entry bits.
 * Available on section and supersection.
 */
#define	L1PT_AP_SHIFT		10
#define	L1PT_AP(ap)		((ap) << L1PT_AP_SHIFT)
#define	L1PT_AP_MASK		L1PT_AP(3)

/*
 * Type Extension.
 * Available on section and supersection.
 */
#define	L1PT_TEX_SHIFT		12
#define	L1PT_TEX(tex)		((tex) << L1PT_TEX_SHIFT)
#define	L1PT_TEX_MASK		L1PT_TEX(7)

/* The following 4 bits are available on section and supersection. */
#define	L1PT_APX		0x8000	/* AP extension */
#define	L1PT_SHARED		0x10000	/* Shared mapping */
#define	L1PT_NG			0x20000	/* Non-global mapping */
#define	L1PT_SPSECTION		0x40000	/* Supersection bit */

/* Address mask in L1PT entry. */
#define	L1PT_SECTION_ADDRMASK	0xfff00000
#define	L1PT_SPSECTION_ADDRMASK	0xff000000

/* Supersection bits */
#define	L1PT_TYPE_SPSECTION	(L1PT_TYPE_SECTION|L1PT_SPSECTION)

/* Size of virtual address space that is mapped by one L1PT entry. */
#define	L1PT_VSHIFT		20
#define	L1PT_VSIZE		(UINT32_C(1) << L1PT_VSHIFT)	/* 1MB */
#define	L1PT_PAGEOFFSET		(L1PT_VSIZE - 1)
#define	L1PT_PAGEMASK		(~L1PT_PAGEOFFSET)

#ifndef	_ASM
/* Return the lowest virtual address mapped by the next L1PT entry. */
#define	L1PT_NEXT_VADDR(vaddr)					\
	(((uintptr_t)(vaddr) & L1PT_PAGEMASK) + L1PT_VSIZE)

/* Derive L1PT index from virtual address */
#define	L1PT_INDEX(vaddr)	((uintptr_t)(vaddr) >> L1PT_VSHIFT)

/* Derive virtual address from L1PT index */
#define	L1PT_IDX2VADDR(idx)	((uintptr_t)(idx) << L1PT_VSHIFT)
#endif	/* !_ASM */

/* Section maps 1MB using one TLB entry. */
#define	L1PT_SECTION_VSHIFT	20
#define	L1PT_SECTION_VSIZE	(UINT32_C(1) << L1PT_SECTION_VSHIFT)
#define	L1PT_SECTION_VOFFSET	(L1PT_SECTION_VSIZE - 1)
#define	L1PT_SECTION_VMASK	(~L1PT_SECTION_VOFFSET)

#ifndef	_ASM
#define	L1PT_SECTION_ALIGNED(a)				\
	(((uintptr_t)(a) & L1PT_SECTION_VOFFSET) == 0)
#endif	/* !_ASM */

/*
 * Supersection maps 16MB using one TLB entry.
 * Note that supersection requires 16 copies of the same L1PT entry.
 */
#define	L1PT_SPSECTION_VSHIFT	24
#define	L1PT_SPSECTION_VSIZE	(UINT32_C(1) << L1PT_SPSECTION_VSHIFT)
#define	L1PT_SPSECTION_VOFFSET	(L1PT_SPSECTION_VSIZE - 1)
#define	L1PT_SPSECTION_VMASK	(~L1PT_SPSECTION_VOFFSET)

/* Number of L1PT entries required for one supersection mapping. */
#define	L1PT_SPSECTION_NPTES	(L1PT_SPSECTION_VSIZE >> L1PT_SECTION_VSHIFT)

#ifndef	_ASM
#define	L1PT_SPSECTION_ALIGNED(a)			\
	(((uintptr_t)(a) & L1PT_SPSECTION_VOFFSET) == 0)

/*
 * Create coarse L1PT entry.
 * l2paddr must be L2PT size aligned.
 */
#define	L1PT_MKL2PT(l2paddr, domain)					\
	((l1pte_t)(l2paddr) | L1PT_DOMAIN(domain) | L1PT_TYPE_COARSE)
#endif	/* !_ASM */

/*
 * =========================================================================
 * Level 2 page table definitions
 * =========================================================================
 */

/* Size of L2 page table entry */
#define	L2PT_PTE_SIZE		4

/* Size of L2 page table */
#define	L2PT_SHIFT		10
#define	L2PT_SIZE		(UINT32_C(1) << L2PT_SHIFT)	/* 1KB */

/* Size of virtual address space that is mapped by one L2PT entry. */
#define	L2PT_VSHIFT		12
#define	L2PT_VSIZE		(UINT32_C(1) << L2PT_VSHIFT)
#define	L2PT_PAGEOFFSET		(L2PT_VSIZE - 1)
#define	L2PT_PAGEMASK		(~L2PT_PAGEOFFSET)

/* Derive L2PT index from virtual address */
#define	L2PT_INDEX_MASK		(L1PT_PAGEOFFSET & L2PT_PAGEMASK)

#ifndef	_ASM
#define	L2PT_INDEX(vaddr)					\
	(((uintptr_t)(vaddr) & L2PT_INDEX_MASK) >> L2PT_VSHIFT)
#endif	/* !_ASM */

/* Number of ptes in L2 page table */
#define	L2PT_NPTES		(L2PT_SIZE / L2PT_PTE_SIZE)

#define	L2PT_TYPEMASK		0x3
#define	L2PT_TYPE_INVALID	0x0	/* Raise translation fault */
#define	L2PT_TYPE_LARGE		0x1	/* Large page */
#define	L2PT_TYPE_SMALL		0x2	/* Small page */

#ifndef	_ASM
#define	L2PT_PTE_IS_SMALL(pte)	((pte) & L2PT_TYPE_SMALL)
#define	L2PT_PTE_IS_LARGE(pte)	(((pte) & L2PT_TYPEMASK) == L2PT_TYPE_LARGE)
#define	L2PT_PTE_IS_INVALID(pte)	\
	(((pte) & L2PT_TYPEMASK) == L2PT_TYPE_INVALID)
#define	L2PT_PTE_TYPE(pte)				\
	((L2PT_PTE_IS_SMALL(pte)) ? L2PT_TYPE_SMALL	\
	 : ((pte) & L2PT_TYPEMASK))
#endif	/* !_ASM */

#define	L2PT_LARGE_XN		0x8000	/* eXecute Never bit */
#define	L2PT_SMALL_XN		0x1	/* eXecute Never bit */
#define	L2PT_BUFFERABLE		0x4	/* Bufferable */
#define	L2PT_CACHED		0x8	/* Cached mapping */

/* Convert AP value in L2PT entry bits */
#define	L2PT_AP_SHIFT		4
#define	L2PT_AP(ap)		((ap) << L2PT_AP_SHIFT)
#define	L2PT_AP_MASK		L2PT_AP(3)

/* Type Extension. */
#define	L2PT_LARGE_TEX_SHIFT		12
#define	L2PT_LARGE_TEX(tex)		((tex) << L2PT_LARGE_TEX_SHIFT)
#define	L2PT_LARGE_TEX_MASK		L2PT_LARGE_TEX(7)
#define	L2PT_SMALL_TEX_SHIFT		6
#define	L2PT_SMALL_TEX(tex)		((tex) << L2PT_SMALL_TEX_SHIFT)
#define	L2PT_SMALL_TEX_MASK		L2PT_SMALL_TEX(7)

#define	L2PT_APX		0x200	/* AP extension */
#define	L2PT_SHARED		0x400	/* Shared mapping */
#define	L2PT_NG			0x800	/* Non-global mapping */

/* Address mask in L2PT entry. */
#define	L2PT_LARGE_ADDRMASK	0xffff0000
#define	L2PT_SMALL_ADDRMASK	0xfffff000

/*
 * Large page maps 64KB using one TLB entry.
 * Note that large page requires 16 copies of the same L2PT entry.
 */
#define	L2PT_LARGE_VSHIFT	16
#define	L2PT_LARGE_VSIZE	(UINT32_C(1) << L2PT_LARGE_VSHIFT)
#define	L2PT_LARGE_VOFFSET	(L2PT_LARGE_VSIZE - 1)
#define	L2PT_LARGE_VMASK	(~L2PT_LARGE_VOFFSET)

#ifndef	_ASM
#define	L2PT_LARGE_ALIGNED(a)				\
	(((uintptr_t)(a) & L2PT_LARGE_VOFFSET) == 0)
#endif	/* !_ASM */

/* Number of L2PT entries required for one large page mapping. */
#define	L2PT_LARGE_NPTES	(L2PT_LARGE_VSIZE >> L2PT_VSHIFT)

/*
 * =========================================================================
 * PTE attributes
 * =========================================================================
 */

/*
 * AP bits that controls access permission.
 * Write permission will be revoked with APX bit is set.
 */
#define	PTE_AP_NONE		0	/* No access */
#define	PTE_AP_KRW		1	/* Kernel read/write */
#define	PTE_AP_URO		2	/* Kernel read/write, User read only */
#define	PTE_AP_URW		3	/* Kernel/User read/write */

/*
 * Values for Type Extension (TEX) field.
 */
#define	PTE_TEX_NOALLOC		0	/* No allocate on write */
#define	PTE_TEX_WALLOC		1	/* Write allocate */

/*
 * Values for Type Extention (TEX) field that determines external cache policy.
 */
#define	PTE_TEX_EXT_NOCACHE	4	/* Uncached, unbuffered */
#define	PTE_TEX_EXT_CACHED	5	/* Cached, write allocate, buffered */
#define	PTE_TEX_EXT_WTHRU	6	/* Write thru, no allocate, buffered */
#define	PTE_TEX_EXT_NOBUFFER	7	/* Cached, no allocate, unbuffered */

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* _SYS_PTE_H */
