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

#ifndef	_VM_HAT_ARMPT_H
#define	_VM_HAT_ARMPT_H

#ident	"@(#)arm/vm/hat_armpt.h"

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

/*
 * VM - Page table management for ARM architecture.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/sysmacros.h>
#include <sys/disp.h>
#include <sys/pte.h>
#include <sys/cachectl.h>
#include <sys/atomic.h>

struct hat;

/*
 * ARM architecture defines the size of L2PT as 1K, less than pagesize.
 * So We must allocate multiple L2PT from one page.
 *
 * In addition, ARM L2 PTE has no room to store software flags.
 * So the kernel allocates 2 times of the required size for L2PT.
 * Lower 1Kbytes used as real L2PT, and higher used as software PTE flags.
 *
 *   L2PT base address  +------------------+ 0x0
 *                      |                  |
 *                      |  Hardware PTEs   |
 *                      |                  |
 *                      +------------------+ 0x400
 *                      |                  |
 *                      |  Software flags  |
 *                      |                  |
 *                      +------------------+ 0x800
 *
 * The hat_l2pt structure manages level 2 page table.
 */
typedef struct hat_l2pt {
	l2pte_t		*l2_vaddr;	/* L2PT virtual address */
	uintptr_t	l2_paddr;	/* L2PT physical address */
	uint16_t	l2_l1index;	/* L1PT index for this L2PT.
					   Set -1 if this entry keeps
					   section mapping. */
	uint16_t	l2_index;	/* Index in this L2PT bundle */
	uint16_t	l2_nptes;	/* Number of active PTEs */
	uint16_t	l2_hold;	/* Hold counter */
} hat_l2pt_t;

/*
 * Size of L2 page table to be allocated at once.
 * The higher 1K will be used as software PTE flags.
 */
#define	L2PT_ALLOC_SIZE		(L2PT_SIZE << 1)

/*
 * Derive address for Software PTE flags associated with the specified
 * PTE address.
 */
#define	L2PT_SOFTFLAGS(ptep)	((l2pte_t *)(ptep) + L2PT_NPTES)

/* Derive virtual address from hat_l2pt and L2PT index. */
#define	HAT_L2PT_TO_VADDR(l2pt, index)					\
	(L1PT_IDX2VADDR((l2pt)->l2_l1index) + ((index) << MMU_PAGESHIFT))

/*
 * The hat structure must keep 4096 L2PT pointers to manage whole virtual
 * address space. But it is too large to embed in the hat structure that is
 * allocated per process. So we decide to bunch 16 L2PT pointers.
 * The hat_l2bundle structure manages a bunch of L2PT pointers, that keeps
 * 16 L2PT pointers. So the hat structure keeps 256 entries of hat_l2bundle
 * structure.
 */
#define	L2BD_SHIFT	4
#define	L2BD_SIZE	(1 << L2BD_SHIFT)	/* 16 */
#define	L2BD_OFFSET	(L2BD_SIZE - 1)

/* Number of bundles to manage whole 4G space */
#define	L2BD_NBSHIFT	((32 - L1PT_VSHIFT) - L2BD_SHIFT)
#define	L2BD_NBSIZE	(1 << L2BD_NBSHIFT)	/* 256 */

/* Number of bundles to manage whole kernel space */
#define	L2BD_KERN_NBSIZE	(L2BD_NBSIZE >> 2)

/* Virtual address space mapped by one bundle */
#define	L2BD_VSHIFT	(L1PT_VSHIFT + L2BD_SHIFT)
#define	L2BD_VSIZE	(1 << L2BD_VSHIFT)	/* 16M */
#define	L2BD_VOFFSET	(L2BD_VSIZE - 1)
#define	L2BD_VMASK	(~L2BD_VOFFSET)

typedef struct hat_l2bundle {
	struct hat	*l2b_hat;	/* hat this mapping comes from */
	uint32_t	l2b_active;	/* Active L2PT count */
	uint32_t	l2b_index;	/* Index of hat_l2bundle array */

	/*
	 * l2b_l2pt MUST be located at the end of structure, or
	 * HAT_L2PT_TO_BUNDLE() will go insane.
	 */
	hat_l2pt_t	l2b_l2pt[L2BD_SIZE];	/* L2PTs */
} hat_l2bundle_t;

/* Initialize hat_l2bundle structure. */
#define	HAT_L2BD_INIT(hat, l2b, index)					\
	do {								\
		int	__i;						\
		hat_l2pt_t	*__l2pt;				\
		(l2b)->l2b_hat = (hat);					\
		(l2b)->l2b_active = 0;					\
		(l2b)->l2b_index = (index);				\
		__l2pt = (l2b)->l2b_l2pt;				\
		for (__i = 0; __i < L2BD_SIZE; __i++, __l2pt++) {	\
			__l2pt->l2_vaddr = NULL;			\
			__l2pt->l2_paddr = 0;				\
			__l2pt->l2_l1index = 0;				\
			__l2pt->l2_index = (uint16_t)__i;		\
			__l2pt->l2_nptes = 0;				\
			__l2pt->l2_hold = 0;				\
		}							\
		(hat)->hat_l2bundle[(index)] = (l2b);			\
	} while (0)

/* Convert hat_l2pt address into hat_l2bundle */
#define	HAT_L2PT_TO_BUNDLE(l2pt)					\
	((hat_l2bundle_t *)((uintptr_t)(l2pt) -				\
			    (offsetof(hat_l2bundle_t, l2b_l2pt) +	\
			    ((l2pt)->l2_index * sizeof(hat_l2pt_t)))))

/* Derive index of hat_l2bundle array from L1PT index */
#define	HAT_L2BD_INDEX(hat, l1idx)				\
	(((l1idx) - (hat)->hat_basel1idx) >> L2BD_SHIFT)

/* Derive index of l2b_l2pt array from L1PT index */
#define	HAT_L2BD_L2PT_INDEX(l1idx)	((l1idx) & L2BD_OFFSET)


/* L1PT entry uses hat_l2bundle as room for software flags. */
#define	HAT_L2PT_SECTION_INDEX		((uint16_t)-1)

#define	HAT_L2PT_IS_SECTION(l2pt)			\
	((l2pt)->l2_nptes == HAT_L2PT_SECTION_INDEX)

#define	HAT_L2PT_SET_SECTION_FLAGS(l2pt, flags)			\
	do {							\
		(l2pt)->l2_nptes = HAT_L2PT_SECTION_INDEX;	\
		(l2pt)->l2_paddr = (flags);			\
	} while (0)

#define	HAT_L2PT_RELE_SECTION_FLAGS(l2pt)			\
	do {							\
		(l2pt)->l2_nptes = 0;				\
		(l2pt)->l2_paddr = 0;				\
	} while (0)

#define	HAT_L2PT_GET_SECTION_FLAGS(l2pt)	((l1pte_t)((l2pt)->l2_paddr))

/*
 * hat_kresv manages reserved virtual space for kernel use.
 *
 * Features:
 * - Virtual space managed by hat_kresv is reserved at bootstrap.
 * - Each space has "break value". Break value is the highest address boundary
 *   of the valid mapping.
 *
 *      +---------------+  Base address
 *      |               |
 *      | Valid Mapping |
 *      |               |
 *      +---------------+  Break value
 *      |   Not Mapped  |
 *      +---------------+  Base address + space size
 *
 * - Break value moves only to higher direction. That is, once a page is
 *   mapped to the space, it is never freed.
 * - All pages mapped to the space is divided into the specified buffer size.
 *   And each space has a freelist to link free buffer.
 *
 * hat_kresv is used to manage L2PT and hat_l2bundle structure for kernel
 * mapping.
 */

typedef struct hat_kresv_buf {
	struct hat_kresv_buf	*hkb_next;
} hat_kresv_buf_t;

typedef struct hat_kresv {
	uintptr_t	hk_addr;	/* Base address */
	size_t		hk_size;	/* Space size */
	uintptr_t	hk_brk;		/* Break value */
	size_t		hk_bufsize;	/* Buffer size */
	size_t		hk_bufalign;	/* Required address alignment. */
	hat_kresv_buf_t	*hk_free;	/* Buffer freelist */
	uint32_t	hk_nfree;	/* Number of free buffers. */
	uint32_t	hk_prealloc;	/* Number of buffers to be allocated
					   at boottime. */
	uint32_t	hk_reserve;	/* hat_kresv tries to keep at least
					   # buffers in freelist.*/
} hat_kresv_t;

/*
 * Software PTE flags
 */
#define	PTE_S_READ		0x1	/* Software readable bit */
#define	PTE_S_WRITE		0x2	/* Software writable bit */
#define	PTE_S_EXEC		0x4	/* Software executable bit */
#define	PTE_S_NOSYNC		0x8	/* Never sync REF/MOD to page_t */
#define	PTE_S_NOCONSIST		0x10	/* Hidden mapping (no p_mapping) */
#define	PTE_S_LOCKED		0x20	/* Mapping is locked */

#define	PTE_S_PROT_MASK		(PTE_S_READ|PTE_S_WRITE|PTE_S_EXEC)

/*
 * The following flags are never set in software PTE bits, but used as
 * function interface.
 */
#define	PTE_S_REF		0x20000000	/* Page is referenced */
#define	PTE_S_MOD		0x40000000	/* Page is modified */
#ifdef	DEBUG
#define	PTE_S_REFMOD_SYNC	0x80000000
#else	/* DEBUG */
#define	PTE_S_REFMOD_SYNC	0
#endif	/* DEBUG */

#define	SWPTE_IS_READABLE(swpte)	((swpte) & PTE_S_READ)
#define	SWPTE_IS_WRITABLE(swpte)	((swpte) & PTE_S_WRITE)
#define	SWPTE_IS_EXECUTABLE(swpte)	((swpte) & PTE_S_EXEC)
#define	SWPTE_SET_READABLE(swpte)		\
	do {					\
		(swpte) |= PTE_S_READ;		\
	} while (0)
#define	SWPTE_SET_WRITABLE(swpte)		\
	do {					\
		(swpte) |= PTE_S_WRITE;		\
	} while (0)
#define	SWPTE_SET_EXECUTABLE(swpte)		\
	do {					\
		(swpte) |= PTE_S_EXEC;		\
	} while (0)
#define	SWPTE_CLR_READABLE(swpte)		\
	do {					\
		(swpte) &= ~PTE_S_READ;		\
	} while (0)
#define	SWPTE_CLR_WRITABLE(swpte)		\
	do {					\
		(swpte) &= ~PTE_S_WRITE;	\
	} while (0)
#define	SWPTE_CLR_EXECUTABLE(swpte)		\
	do {					\
		(swpte) &= ~PTE_S_EXEC;	\
	} while (0)

#define	SWPTE_IS_NOSYNC(swpte)		((swpte) & PTE_S_NOSYNC)
#define	SWPTE_IS_NOCONSIST(swpte)	((swpte) & PTE_S_NOCONSIST)
#define	SWPTE_IS_LOCKED(swpte)		((swpte) & PTE_S_LOCKED)

/* Determine whether ref/mod bit emulation is required. */
#define	NEED_SOFTFAULT(swpte)	(!((swpte) & (PTE_S_NOSYNC|PTE_S_LOCKED)))

/*
 * The design of permission bits in PTE:
 *
 *    We use the following ARMv6 PTE flags for access control.
 *
 *       APX   AP[1:0]      User          Kernel
 *      ==============================================
 *        0      00         No Access     No Access
 *        0      01         No Access     Read/Write
 *        0      11         Read/Write    Read/Write
 *        1      01         No Access     Read Only
 *        1      11         Read Only     Read Only
 *
 *      - Use APX bit as "read only" bit.
 *      - Use AP[1] as "user mode" bit.
 *      - Use AP[0] as "access" bit. See the below comments carefully.
 *
 *    We use "kernel mode read/write" as default permission, and append
 *    appropriate bits according to the protection attributes.
 *
 *    In addition, we use XN (eXecute Never) bit to protect unexpected
 *    execution on the mapping.
 *
 * Reference/Modification bit emulation
 *
 *    On ARM architecture, software is responsible for detecting references
 *    or modifications to the mapping. So HAT layer should emulate ref/mod
 *    bit in PTE using protection fault.
 *
 *    References:
 *      If bit 29 in Control Register is set, we can use AP[0] as "access bit".
 *      Reading a page table entry into TLB where the AP[0] is zero raises
 *      a access bit fault. We use this feature to detect references or
 *      modifications to the mapping. When an access bit fault is raised,
 *      HAT will set AP[0] bit to the PTE corresponding to the fault address.
 *      So AP[0] bit behaves just like "access bit".
 *
 *    Modifications:
 *      When the HAT layer creates writable mapping, it sets APX bit in the
 *      PTE. When writing to the mapping raises permission fault, the HAT
 *      layer will clear APX bit. So APX bit behaves just like
 *      "NOT(modified bit)".
 */
#define	L1_PROT_RW		L1PT_AP(PTE_AP_KRW)
#define	L1_PROT_USER		L1PT_AP(2)
#define	L1_PROT_READONLY	L1PT_APX

#define	L2_PROT_RW		L2PT_AP(PTE_AP_KRW)
#define	L2_PROT_USER		L2PT_AP(2)
#define	L2_PROT_READONLY	L2PT_APX

/* L1PT entry macros */
#define	L1PT_PTE_SET_RW(l1pte)			\
	do {					\
		(l1pte) |= L1_PROT_RW;		\
		(l1pte) &= ~L1_PROT_READONLY;	\
	} while (0)
#define	L1PT_PTE_CLR_RW(l1pte)					\
	do {							\
		(l1pte) &= ~(L1_PROT_RW|L1_PROT_READONLY);	\
	} while (0)
#define	L1PT_PTE_SET_READONLY(l1pte)				\
	do {							\
		(l1pte) |= (L1_PROT_RW|L1_PROT_READONLY);	\
	} while (0)

#define	L1PT_PTE_IS_USER(l1pte)		((l1pte) & L1_PROT_USER)
#define	L1PT_PTE_IS_READABLE(l1pte)	((l1pte) & L1_PROT_RW)
#define	L1PT_PTE_IS_WRITABLE(l1pte)					\
	(L1PT_PTE_IS_READABLE(l1pte) && (((l1pte) & L1_PROT_READONLY) == 0))
#define	L1PT_PTE_SET_WRITABLE(l1pte)		\
	do {					\
		(l1pte) &= ~L1_PROT_READONLY;	\
	} while (0)
#define	L1PT_PTE_CLR_WRITABLE(l1pte)		\
	do {					\
		(l1pte) |= L1_PROT_READONLY;	\
	} while (0)

/* Note that EXECUTABLE macros don't see AP and APX bits. */
#define	L1PT_PTE_IS_EXECUTABLE(l1pte)	(((l1pte) & L1PT_XN) == 0)
#define	L1PT_PTE_SET_EXECUTABLE(l1pte)		\
	do {					\
		(l1pte) &= ~L1PT_XN;		\
	} while (0)
#define	L1PT_PTE_CLR_EXECUTABLE(l1pte)		\
	do {					\
		(l1pte) |= L1PT_XN;		\
	} while (0)

/* L2PT entry macros */
#define	L2PT_PTE_SET_RW(pte)			\
	do {					\
		(pte) |= L2_PROT_RW;		\
		(pte) &= ~L2_PROT_READONLY;	\
	} while(0)
#define	L2PT_PTE_CLR_RW(pte)					\
	do {							\
		(pte) &= ~(L2_PROT_RW|L2_PROT_READONLY);	\
	} while (0)
#define	L2PT_PTE_SET_READONLY(pte)			\
	do {						\
		(pte) |= (L2_PROT_RW|L2_PROT_READONLY);	\
	} while (0)

#define	L2PT_PTE_IS_USER(pte)		((pte) & L2_PROT_USER)
#define	L2PT_PTE_IS_READABLE(pte)	((pte) & L2_PROT_RW)
#define	L2PT_PTE_IS_WRITABLE(pte)	\
	(L2PT_PTE_IS_READABLE(pte) && (((pte) & L2_PROT_READONLY) == 0))
#define	L2PT_PTE_SET_WRITABLE(pte)		\
	do {					\
		(pte) &= ~L2_PROT_READONLY;	\
	} while (0)
#define	L2PT_PTE_CLR_WRITABLE(pte)		\
	do {					\
		(pte) |= L2_PROT_READONLY;	\
	} while (0)

/* Note that EXECUTABLE macros don't see AP and APX bits. */
#define	L2PT_PTE_LARGE_IS_EXECUTABLE(pte)	(((pte) & L2PT_LARGE_XN) == 0)
#define	L2PT_PTE_LARGE_SET_EXECUTABLE(pte)	\
	do {					\
		(pte) &= ~L2PT_LARGE_XN;	\
	} while (0)
#define	L2PT_PTE_LARGE_CLR_EXECUTABLE(pte)	\
	do {					\
		(pte) |= L2PT_LARGE_XN;		\
	} while (0)

#define	L2PT_PTE_SMALL_IS_EXECUTABLE(pte)	(((pte) & L2PT_SMALL_XN) == 0)
#define	L2PT_PTE_SMALL_SET_EXECUTABLE(pte)	\
	do {					\
		(pte) &= ~L2PT_SMALL_XN;	\
	} while (0)
#define	L2PT_PTE_SMALL_CLR_EXECUTABLE(pte)	\
	do {					\
		(pte) |= L2PT_SMALL_XN;		\
	} while (0)

#define	L2PT_PTE_SET_EXECUTABLE(pte)				\
	do {							\
		if (L2PT_PTE_IS_SMALL(pte)) {			\
			L2PT_PTE_SMALL_SET_EXECUTABLE(pte);	\
		}						\
		else {						\
			L2PT_PTE_LARGE_SET_EXECUTABLE(pte);	\
		}						\
	} while (0)

#define	L2PT_PTE_CLR_EXECUTABLE(pte)				\
	do {							\
		if (L2PT_PTE_IS_SMALL(pte)) {			\
			L2PT_PTE_SMALL_CLR_EXECUTABLE(pte);	\
		}						\
		else {						\
			L2PT_PTE_LARGE_CLR_EXECUTABLE(pte);	\
		}						\
	} while (0)

/*
 * Reference/Modified bit handling
 */
#define	L1PT_PTE_IS_REF(l1pte)		L1PT_PTE_IS_READABLE(l1pte)
#define	L1PT_PTE_IS_MOD(l1pte)		L1PT_PTE_IS_WRITABLE(l1pte)
#define	L1PT_PTE_SET_REF(l1pte)		L1PT_PTE_SET_READONLY(l1pte)
#define	L1PT_PTE_SET_MOD(l1pte)		L1PT_PTE_SET_RW(l1pte)
#define	L1PT_PTE_CLR_REFMOD(l1pte)	L1PT_PTE_CLR_RW(l1pte)

#define	L2PT_PTE_IS_REF(pte)		L2PT_PTE_IS_READABLE(pte)
#define	L2PT_PTE_IS_MOD(pte)		L2PT_PTE_IS_WRITABLE(pte)
#define	L2PT_PTE_SET_REF(pte)		L2PT_PTE_SET_READONLY(pte)
#define	L2PT_PTE_SET_MOD(pte)		L2PT_PTE_SET_RW(pte)
#define	L2PT_PTE_CLR_REFMOD(pte)	L2PT_PTE_CLR_RW(pte)

/*
 * Check whether the PTE has been referenced or modified.
 * We don't need to see APX because AP[0] bit should be set if the
 * page is modified.
 */
#define	L1PT_PTE_IS_REFMOD(l1pte)	L1PT_PTE_IS_REF(l1pte)
#define	L2PT_PTE_IS_REFMOD(pte)		L2PT_PTE_IS_REF(pte)

/*
 * Check whether the ref/mod state has been changed.
 */
#define	L1PT_PTE_IS_REFMOD_CHANGED(l1pte, newl1pte)	\
	(((l1pte) & (L1_PROT_READONLY|L1_PROT_RW)) !=	\
	 ((newl1pte) & (L1_PROT_READONLY|L1_PROT_RW)))
#define	L2PT_PTE_IS_REFMOD_CHANGED(pte, newpte)	\
	(((pte) & (L2_PROT_READONLY|L2_PROT_RW)) !=	\
	 ((newpte) & (L2_PROT_READONLY|L2_PROT_RW)))

/*
 * The following macros need to be used before hat_ptesync() call.
 */
#define	L1PT_PTE_REFMOD_SYNC(l1pte, sw)					\
	do {								\
		if (L1PT_PTE_IS_REF(l1pte)) {				\
			(sw) |= PTE_S_REF;				\
			if (((l1pte) & L1_PROT_READONLY) == 0) {	\
				(sw) |= PTE_S_MOD;			\
			}						\
		}							\
		(sw) |= PTE_S_REFMOD_SYNC;				\
	} while (0)
#define	L2PT_PTE_REFMOD_SYNC(pte, sw)				\
	do {							\
		if (L2PT_PTE_IS_REF(pte)) {			\
			(sw) |= PTE_S_REF;			\
			if (((pte) & L2_PROT_READONLY) == 0) {	\
				(sw) |= PTE_S_MOD;		\
			}					\
		}						\
		(sw) |= PTE_S_REFMOD_SYNC;			\
	} while (0)

/* Default permission for kernel mapping */
#define	HAT_PROT_KERNEL		(PROT_READ|PROT_WRITE|PROT_EXEC)

/* Default ordering attributes for kernel mapping */
#define	HAT_ORDER_KERNEL	HAT_STORECACHING_OK

/*
 * PTE_SYNC() and PTE_SYNC_RANGE() must be called in preemption disabled
 * section, and PTE change must be done in the same section.
 */
#define	PTE_SYNC(ptep, ptesize)						\
	do {								\
		DCACHE_CLEAN_VADDR(ptep);				\
		SYNC_BARRIER();						\
	} while (0)

#define	PTE_SYNC_RANGE(ptep, ptesize, nentries)				\
	do {								\
		size_t	__sz = (ptesize) * (nentries);			\
		uintptr_t	__va, __end;				\
									\
		__va = DCACHE_ROUNDDOWN(ptep);				\
		__end = DCACHE_ROUNDUP((uintptr_t)(ptep) + __sz);	\
		for (; __va < __end; __va += arm_pdcache_linesize) {	\
			DCACHE_CLEAN_VADDR(__va);			\
		}							\
		SYNC_BARRIER();						\
	} while (0)

/* Macros to set PTE. */
#define	HAT_L1PTE_SET(l1ptep, l1pte)			\
	do {						\
		kpreempt_disable();			\
		*(l1ptep) = (l1pte);			\
		PTE_SYNC(l1ptep, L1PT_PTE_SIZE);	\
		kpreempt_enable();			\
	} while (0)

#define	HAT_L1PTE_SET_RANGE(l1ptep, l1pte, nentries)			\
	do {								\
		int	__i;						\
		l1pte_t	*__ptep, *__end;				\
									\
		__ptep = (l1ptep);					\
		__end = __ptep + (nentries);				\
		kpreempt_disable();					\
		for (; __ptep < __end; __ptep++) {			\
			*__ptep = (l1pte);				\
		}							\
		PTE_SYNC_RANGE(l1ptep, L1PT_PTE_SIZE, nentries);	\
		kpreempt_enable();					\
	} while (0)

#define	HAT_L1PTE_SPSECTION_SET(l1ptep, l1pte)				\
	HAT_L1PTE_SET_RANGE(l1ptep, l1pte, L1PT_SPSECTION_NPTES)

#define	HAT_L2PTE_SET(ptep, pte)					\
	do {								\
		kpreempt_disable();					\
		*(ptep) = (pte);					\
		PTE_SYNC(ptep, L2PT_PTE_SIZE);				\
		kpreempt_enable();					\
	} while (0)

#define	HAT_L2PTE_LARGE_SET(ptep, pte)					\
	do {								\
		int	__i;						\
		l2pte_t	*__ptep, *__end;				\
									\
		__ptep = (ptep);					\
		__end = __ptep + L2PT_LARGE_NPTES;			\
		kpreempt_disable();					\
		for (; __ptep < __end; __ptep++) {			\
			*__ptep = (pte);				\
		}							\
		PTE_SYNC_RANGE(ptep, L2PT_PTE_SIZE, L2PT_LARGE_NPTES);	\
		kpreempt_enable();					\
	} while (0)

/* Return pfn in PTE. */
#define	HAT_L1PTE_SPSECTION_PFN(pte)					\
	(((pte) & L1PT_SPSECTION_ADDRMASK) >> MMU_PAGESHIFT)
#define	HAT_L1PTE_SECTION_PFN(pte)					\
	(((pte) & L1PT_SECTION_ADDRMASK) >> MMU_PAGESHIFT)
#define	HAT_L1PTE2PFN(pte)					\
	((L1PT_PTE_TYPE(pte) == L1PT_TYPE_SECTION)			\
	 ? (((pte) & L1PT_SPSECTION)					\
	    ? HAT_L1PTE_SPSECTION_PFN(pte)				\
	    : HAT_L1PTE_SECTION_PFN(pte))				\
	 : PFN_INVALID)

#define	HAT_L2PTE_LARGE_PFN(pte)				\
	(((pte) & L2PT_LARGE_ADDRMASK) >> MMU_PAGESHIFT)
#define	HAT_L2PTE_SMALL_PFN(pte)			\
	(((pte) & L2PT_SMALL_ADDRMASK) >> MMU_PAGESHIFT)
#define	HAT_L2PTE2PFN(pte)						\
	((L2PT_PTE_IS_SMALL(pte)) ? HAT_L2PTE_SMALL_PFN(pte)		\
	 : ((L2PT_PTE_IS_LARGE(pte)) ? HAT_L2PTE_LARGE_PFN(pte)		\
	    : PFN_INVALID))

/* Maintain PTE count */
#define	HAT_L2PT_COUNT(l2pt, cnt)					\
	do {								\
		int16_t	__c = (int16_t)cnt;				\
									\
		if ((__c) > 0) {					\
			ASSERT((l2pt)->l2_nptes <= L2PT_NPTES - (__c));	\
		}							\
		else {							\
			ASSERT((l2pt)->l2_nptes >= ABS(__c));		\
			ASSERT((l2pt)->l2_nptes <= L2PT_NPTES);		\
		}							\
		(l2pt)->l2_nptes += (__c);				\
	} while (0)
#define	HAT_L2PT_INC(l2pt)		HAT_L2PT_COUNT(l2pt, 1)
#define	HAT_L2PT_DEC(l2pt)		HAT_L2PT_COUNT(l2pt, -1)

/*
 * Hold/Release L2PT.
 * This is used to prevent L2PT from unexpected freeing.
 * These must be used with hat lock.
 */
#define	HAT_L2PT_IS_HELD(l2pt)		((l2pt)->l2_hold != 0)
#define	HAT_L2PT_HOLD(l2pt)						\
	do {								\
		(l2pt)->l2_hold++;					\
		ASSERT((l2pt)->l2_hold != 0);	/* Overflow check */	\
	} while (0)
#define	HAT_L2PT_RELE(l2pt)			\
	do {					\
		ASSERT(HAT_L2PT_IS_HELD(l2pt));	\
		(l2pt)->l2_hold--;		\
	} while(0)

#define	HAT_PFN_IS_SPSECTION_ALIGNED(pfn)		\
	(PFN_BASE((pfn), SZC_SPSECTION) == (pfn))
#define	HAT_PFN_IS_SECTION_ALIGNED(pfn)		\
	(PFN_BASE((pfn), SZC_SECTION) == (pfn))
#define	HAT_PFN_IS_LARGE_ALIGNED(pfn)		\
	(PFN_BASE((pfn), SZC_LARGE) == (pfn))

/* Prototypes */
extern void		hatpt_boot_alloc(void);
extern void		hatpt_init(void);
extern ssize_t		hatpt_pte_lookup(struct hat *hat, uintptr_t vaddr,
					 l1pte_t **l1ptepp, l2pte_t **ptepp,
					 hat_l2pt_t **l2ptpp);
extern ssize_t		hatpt_pte_walk(struct hat *hat, uintptr_t *vaddrp,
				       l1pte_t **l1ptepp, l2pte_t **ptepp,
				       hat_l2pt_t **l2ptpp, pfn_t *pfnp,
				       page_t **ppp);
extern void		hatpt_boot_linkl2pt(uintptr_t vaddr, size_t size,
					    boolean_t doalloc);
extern void		hatpt_kresv_spaceinit(hat_kresv_t *krp,
					      uintptr_t vaddr, size_t size);
extern hat_l2pt_t	*hatpt_l2pt_lookup(struct hat *hat, uintptr_t vaddr);
extern hat_l2pt_t	*hatpt_l2pt_prepare(struct hat *hat, uintptr_t vaddr,
					    boolean_t *allocated);
extern boolean_t	hatpt_l2pt_release(hat_l2bundle_t *l2b, int l1index,
					   hat_l2pt_t *l2pt);
extern hat_l2pt_t	*hatpt_l1pt_softflags_lookup(struct hat *hat,
						     uintptr_t vaddr, int szc);
extern hat_l2pt_t	*hatpt_l1pt_softflags_prepare(struct hat *hat,
						      uintptr_t vaddr);
extern void		hatpt_dump(struct hat *hat);

#if	defined(_KERNEL) && defined(_KERNEL_BUILD_TREE)
/* Include build tree private definitions. */
#include <vm/hat_armpt_impl.h>
#endif	/* defined(_KERNEL) && defined(_KERNEL_BUILD_TREE) */

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* !_VM_HAT_ARMPT_H */
