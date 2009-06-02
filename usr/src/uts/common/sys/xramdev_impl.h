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
 * Copyright (c) 2008 NEC Corporation
 * All rights reserved.
 */

#ifndef	_SYS_XRAMDEV_IMPL_H
#define	_SYS_XRAMDEV_IMPL_H

#ident	"@(#)common/sys/xramdev_impl.h"

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

/*
 * Common definitions for xramfs device. (kernel build environment private)
 *
 * Remarks:
 *	The build environment tool xramconf(1) may need to be rewritten
 *	if you change definitions in this file.
 */

#include <sys/types.h>
#include <sys/dditypes.h>

/*
 * Common definition of physical memory segment reserved as device use.
 */
struct xmemdev;
typedef struct xmemdev	xmemdev_t;
struct xmemdev {
	const pfn_t	xd_base;		/* base PFN */
	const pgcnt_t	xd_count;		/* page count */
	const uint_t	xd_attr;		/* HAT attributes */
	const xmemdev_t	*xd_lower;		/* lower PFN device */
	const xmemdev_t	*xd_higher;		/* higher PFN device */
};

/*
 * Physical memory segment for xramfs device.
 */
typedef struct xdseg {
	const xmemdev_t	xs_memory;		/* memory attributes */
	const uint_t	xs_flags;		/* flags */
	const char	*xs_name;		/* node name */
	uintptr_t	xs_vaddr;		/* kernel virtual address */
} xdseg_t;

#define	xs_base		xs_memory.xd_base
#define	xs_count	xs_memory.xd_count
#define	xs_attr		xs_memory.xd_attr

/* Flags for xs_flags */
#define	XDSEG_ROOTFS		0x1	/* device for root filesystem */
#define	XDSEG_SYSDUMP		0x2	/* dump into system dump */

#ifdef	_KERNEL

/* Common prototypes */
extern xdseg_t	*xramdev_impl_getseg(int unit);
extern uint_t	xramdev_impl_segcount(void);

#ifdef	__arm

/* Debugging macros */
#include <sys/prom_debug.h>
#define	XRAMDEV_PRM_DEBUG(x)		PRM_DEBUG(x)
#define	XRAMDEV_PRM_PRINTF(...)		PRM_PRINTF(__VA_ARGS__)

#ifdef	XRAMDEV_CONFIG

/*
 * "xramdev memory" means that is reserved memory for device usage.
 * xramdev memory are located in physically contiguous area,
 * [xramdev_start_paddr, xramdev_end_paddr).
 * And xramfs device pages are located in physically contiguous area,
 * [xramdev_pagestart_paddr, xramdev_end_paddr).
 */
extern const uintptr_t	xramdev_start_paddr;
extern const uintptr_t	xramdev_end_paddr;
extern const uintptr_t	xramdev_pagestart_paddr;

extern const uintptr_t	data_paddr;
extern const uintptr_t	data_paddr_base;

/* Calculate number of xramfs device pages. */
#define	XRAMDEV_IMPL_NPAGES()					\
	mmu_btop(xramdev_end_paddr - xramdev_pagestart_paddr)

/* Calculate number of non-pagable device pages. */
#define	XRAMDEV_IMPL_NDEVPAGES()				\
	mmu_btop(xramdev_pagestart_paddr - xramdev_start_paddr)

/* Calculate number of pages for kernel data backup area. */
#define	XRAMDEV_IMPL_BACKUP_NPAGES()				\
	mmu_btopr(xramdev_start_paddr - data_paddr_base)

/* Calculate number of pages between xramfs device and data section. */
#define	XRAMDEV_IMPL_HOLE_NPAGES()			\
	mmu_btop(data_paddr - xramdev_end_paddr)

/* Prototypes */
extern void		xramdev_impl_reserve_vaddr(void);
extern void		xramdev_impl_mapinit(void);
extern void		xramdev_impl_probe(dev_info_t *parent);
extern void		xramdev_impl_dump(void);
extern boolean_t	xramdev_impl_mapattr(pfn_t pfn, uint_t *attrp);

#else	/* !XRAMDEV_CONFIG */

#define	XRAMDEV_IMPL_NPAGES()		(0)
#define	XRAMDEV_IMPL_NDEVPAGES()	(0)
#define	XRAMDEV_IMPL_BACKUP_NPAGES()	(0)
#define	XRAMDEV_IMPL_HOLE_NPAGES()	(0)

#define	xramdev_impl_reserve_vaddr()
#define	xramdev_impl_mapinit()
#define	xramdev_impl_probe(parent)
#define	xramdev_impl_dump()
#define	xramdev_impl_mapattr(pfn, attrp)	(B_FALSE)

#endif	/* XRAMDEV_CONFIG */

#endif	/* __arm */

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* !_SYS_XRAMDEV_IMPL_H */
