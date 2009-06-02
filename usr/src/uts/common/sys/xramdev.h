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

#ifndef	_SYS_XRAMDEV_H
#define	_SYS_XRAMDEV_H

#ident	"@(#)common/sys/xramdev.h"

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

/*
 * Common definitions for xramfs device.
 */

#include <sys/types.h>
#include <sys/mutex.h>
#include <sys/dditypes.h>
#include <sys/vnode.h>
#include <sys/open.h>
#include <sys/vfs.h>
#include <sys/debug.h>
#include <vm/page.h>

/*
 * Common xramfs device.
 */
struct xramdev;
typedef struct xramdev	xramdev_t;

struct xramdev {
	kmutex_t	xd_lock;	/* device lock. */
	dev_info_t	*xd_dip;	/* device info handle. */
	uint_t		xd_flags;	/* flags */

	/*
	 * Device unit number.
	 * We use minor_t for xramdisk compatibility.
	 */
	minor_t		xd_unit;

	/*
	 * Parameters related to device page.
	 */
	pgcnt_t		xd_numpages;	/* partition size in pages. */
	pfn_t		xd_basepage;	/* start page number in phys-map. */
	caddr_t		xd_kasaddr;	/* base address in KAS. */

	/*
	 * Common xramdisk methods
	 */

	/* Get a page from xramdisk layer. */
	int		(*xd_getapage)(xramdev_t *xdp, pfn_t pfn, vnode_t *vp,
				       offset_t voff, page_t **ppp);

	/* Put back a page to xramdisk layer. */
	int		(*xd_putapage)(xramdev_t *xdp, pfn_t pfn, page_t *pp);
};

/* Flags for xd_flags */
#define	XDF_WRITABLE		0x1	/* writable device */
#define	XDF_MOUNTED		0x2	/* already mounted */
#define	XDF_CHR_OPENED		0x4	/* character device is opened */
#define	XDF_BLK_OPENED		0x8	/* block device is opened */

#define	XDF_OPENED		(XDF_CHR_OPENED|XDF_BLK_OPENED)
#define	XDF_OPENFLAG(otyp)						\
	(((otyp) == OTYP_CHR) ? XDF_CHR_OPENED : XDF_BLK_OPENED)

/* Max length of node name. */
#define	XRAMDEV_MAX_NAMELEN	32

#ifdef	_KERNEL

#define XRAMDEV_LOCK(xdp)		(mutex_enter(&((xdp)->xd_lock)))
#define XRAMDEV_UNLOCK(xdp)		(mutex_exit(&((xdp)->xd_lock)))

#define XRAMDEV_IS_OPENED(xdp)		((xdp)->xd_flags & XDF_OPENED)
#define XRAMDEV_REF(xdp, otyp)						\
	do {								\
		ASSERT((otyp) == OTYP_CHR || (otyp) == OTYP_BLK);	\
		(xdp)->xd_flags |= XDF_OPENFLAG(otyp);			\
	} while (0)
#define XRAMDEV_UNREF(xdp, otyp)					\
	do {								\
		uint_t	__flag = XDF_OPENFLAG(otyp);			\
									\
		ASSERT((otyp) == OTYP_CHR || (otyp) == OTYP_BLK);	\
		ASSERT((xdp)->xd_flags & __flag);			\
		(xdp)->xd_flags &= ~__flag;				\
	} while (0)

#define	XRAMDEV_SIZE(xdp)		((xdp)->xd_numpages << PAGESHIFT)
#define	XRAMDEV_PFN(xdp, pfnoff)	((xdp)->xd_basepage + (pfnoff))
#define	XRAMDEV_WRITABLE(xdp)		((xdp)->xd_flags & XDF_WRITABLE)

#define	XRAMDEV_ISMOUNTED(xdp)		((xdp)->xd_flags & XDF_MOUNTED)
#define	XRAMDEV_SETMOUNTED(xdp)				\
	do {						\
		ASSERT(XRAMDEV_IS_OPENED(xdp));		\
		(xdp)->xd_flags |= XDF_MOUNTED;		\
	} while (0)
#define	XRAMDEV_CLRMOUNTED(xdp)				\
	do {						\
		ASSERT(XRAMDEV_IS_OPENED(xdp));		\
		ASSERT(XRAMDEV_ISMOUNTED(xdp));		\
		(xdp)->xd_flags &= ~XDF_MOUNTED;	\
	} while (0)

/* Exported macros. (for xramfs) */
#define	XRAMDEV_GETAPAGE(xdp, pfn, vp, voff, ppp)	\
	((xdp)->xd_getapage(xdp, pfn, vp, voff, ppp))
#define	XRAMDEV_PUTAPAGE(xdp, pfn, pp)			\
	((xdp)->xd_putapage(xdp, pfn, pp))

/* Interfaces to register or unregister bind function. */
typedef int	(*xd_bindfunc_t)(vnode_t *vp, vfs_t *vfsp, int flags,
				 xramdev_t **xdpp);
typedef int	(*xd_unbindfunc_t)(xramdev_t *xdp, vfs_t *vfsp);

extern int	xramdev_bindfunc_register(xd_bindfunc_t bind,
					  xd_unbindfunc_t unbind);
extern int	xramdev_bindfunc_unregister(xd_bindfunc_t bind,
					    xd_unbindfunc_t unbind);

/* Interfaces for xramfs to attach or detach xramfs device. */
extern int	xramdev_bind(vnode_t *vp, vfs_t *vfsp, int flags,
			     xramdev_t **xdpp);
extern int	xramdev_unbind(xramdev_t *xdp, vfs_t *vfsp);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* !_SYS_XRAMDEV_H */
