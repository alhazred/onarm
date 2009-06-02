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
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2008 NEC Corporation
 * All rights reserved.
 */

/*
 * XRAMFS-prototype:
 *  xramdisk: base declarations.
 */

#ifndef	_SYS_XRAMDISK_H
#define	_SYS_XRAMDISK_H

#pragma ident	"xramdisk.h"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/vtoc.h>
#include <sys/dkio.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/param.h>
#include <sys/xramdev.h>
#include <vm/page.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * /dev names:
 *	/dev/xramdiskctl		- control device
 *	/dev/xramdisk/<name>	- character devices
 */
#define	XRD_DRIVER_NAME		"xramdisk"
#define	XRD_CHAR_NAME		XRD_DRIVER_NAME

#define	XRD_CTL_NODE		"ctl"
#define	XRD_CTL_NAME		XRD_DRIVER_NAME XRD_CTL_NODE

/*
 * Minor device number for control node.
 */
#define	XRD_CTL_MINOR		0

/*
 * Maximum number of xramdisks supported by this driver.
 */
#define	XRD_MAX_DISKS		4

/*
 * Properties exported by the driver.
 */
#define	XRD_NBLOCKS_PROP_NAME	"Nblocks"
#define	XRD_SIZE_PROP_NAME	"Size"
#define XRD_BASEPAGE_PROP_NAME	"BasePage"
#define XRD_KVA_PROP_NAME	"KVA"

/* for OBP volume. */
#define	XRD_PRESETBASE_PROP_NAME	"presetbase"
#define	XRD_NUMPAGES_PROP_NAME		"numpages"

/* devfsadm support */
/*#define XRD_DEVFSADM_SUPPORT*/

/*
 * Strip any "xramdisk-" prefix from the name of OBP-created ramdisks.
 */
#define	XRD_OBP_PFXSTR		"xramdisk-"
#define	XRD_OBP_PFXLEN		(sizeof (XRD_OBP_PFXSTR) - 1)

#define	XRD_STRIP_PREFIX(newname, oldname) \
	{ \
		char	*onm = oldname; \
		newname = strncmp(onm, XRD_OBP_PFXSTR, XRD_OBP_PFXLEN) == 0 ? \
		    (onm + XRD_OBP_PFXLEN) : onm; \
	}

/*
 * Strip any ",raw" suffix from the name of pseudo xramdisk devices.
 */
#define	XRD_STRIP_SUFFIX(name) \
	{ \
		char	*str = strstr((name), ",raw"); \
		if (str != NULL) \
			*str = '\0'; \
	}

/*
 * Interface between the xramdisk(7D) driver and xramdiskadm(1M).  Use is:
 *
 *	fd = open("/dev/xramdiskctl", O_RDWR | O_EXCL);
 *
 * ioctl usage:
 *
 *	struct xrd_ioctl xri;
 *
 *	strlcpy(xri.xri_name, "somediskname", sizeof (xri.xri_name));
 *	xri.xri_numpages = somedisksize_in_pages;
 *	ioctl(fd, XRD_CREATE_DISK, &xri);
 *
 *	strlcpy(xri.xri_name, "somediskname", sizeof (xri.xri_name));
 *	ioctl(fd, XRD_DELETE_DISK, &xri);
 *
 * (only xramdisks created using the RD_CREATE_DISK ioctl can be deleted
 *  by the RD_DELETE_DISK ioctl).
 *
 * Note that these ioctls are completely private, and only for the use of
 * xramdiskadm(1M).
 */
#define	XRD_IOC_BASE		(('X' << 16) | ('D' << 8))

#define	XRD_CREATE_DISK		(XRD_IOC_BASE | 0x01)
#define	XRD_DELETE_DISK		(XRD_IOC_BASE | 0x02)

#define	XRD_NAME_LEN		32	/* Max length of xramdisk name */
#define	XRD_NAME_PAD		7	/* Pad ri_name to 8-bytes */

struct xrd_ioctl {
	char		xri_name[XRD_NAME_LEN + 1];
	char		_xri_pad[XRD_NAME_PAD];
	uint64_t	xri_numpages;
};

/*
 * Inside-Kernel part below.
 */
#if defined(_KERNEL)

#define	XRD_DFLT_DISKS	2
#if 0
#define	XRD_DEFAULT_PERCENT_PHYSMEM	25
#define	XRD_DEFAULT_MAXPHYS	(63 * 1024)	/* '126b' */
#endif

/*
 * Page management structure.
 */
typedef struct xrampage {
	page_t *	xp_page;	/* page instance;
					   NULL means "no rev-lookup yet". */
	uint8_t		xp_state;	/* XP_KVP | XP_VNODE */
	uint8_t		_xp_reserved;
	uint16_t	xp_refcnt;	/* unused. */
} xrampage_t;

#define XP_KVP		0	/* the page is mapped only in kernel as. */
#define XP_VNODE	1	/* the page is also mapped in vp-cache. */

/*
 * The entire state of each xramdisk device.
 */
struct xramdisk;
typedef struct xramdisk		xramdisk_t;

struct xramdisk {
	xramdev_t	xrd_xdev;	/* common disk properties */

	/* device node behavior... */
	char		xrd_name[XRD_NAME_LEN + 1];	/* symbolic name. */

	/* memory resources... */
	xrampage_t	*xrd_pageinfo;	/* page info array. */

	/* MD private data (see NOT below). */
	void		*xrd_arch_data;	/* MD private data pointer. */
	void		(*xrd_arch_detach)(struct xramdisk *);
	int		(*xrd_arch_getapage)(struct xramdisk *, pfn_t,
					     page_t **);
};

#define XRAMDISK_NO_PRESET	((pfn_t)0)	/* for xrd_basepage */

/*
 * Member name aliases to access xramdev_t.
 *
 * Remarks.
 *	Unlike xramdev_t, xrd_open_count must be protected by
 *	global state lock.
 */
#define	xrd_lock		xrd_xdev.xd_lock
#define	xrd_dip			xrd_xdev.xd_dip
#define	xrd_numpages		xrd_xdev.xd_numpages
#define	xrd_basepage		xrd_xdev.xd_basepage
#define	xrd_kasaddr		xrd_xdev.xd_kasaddr
#define	xrd_getapage		xrd_xdev.xd_getapage
#define	xrd_putapage		xrd_xdev.xd_putapage

/* xramdisk uses xd_unit to store minor number. */
#define	xrd_minor		xrd_xdev.xd_unit

#define	XDTOXRD(xdp)		((xramdisk_t *)(xdp))
#define	XRDTOXD(xrdp)		((xramdev_t *)(xrdp))

#define XRD_LOCK(xrdp)		(mutex_enter(&((xrdp)->xrd_lock)))
#define XRD_UNLOCK(xrdp)	(mutex_exit(&((xrdp)->xrd_lock)))

#define XRD_IS_OPENED(xrdp)	(XRDTOXD(xrdp)->xd_flags & XDF_CHR_OPENED)
#define XRD_REF(xrdp)		(XRDTOXD(xrdp)->xd_flags |= XDF_CHR_OPENED)
#define XRD_UNREF(xrdp)		(XRDTOXD(xrdp)->xd_flags &= ~XDF_CHR_OPENED)

#define XRD_ENSURE_OPENED(xrdp)	(ASSERT(XRD_IS_OPENED(xrdp)))

#define XRD_SIZE(xrdp)	((offset_t)((xrdp)->xrd_numpages << PAGESHIFT))
#define XRD_PAGEINFO_SIZE(xrdp)	(((xrdp)->xrd_numpages) * sizeof(xrampage_t))

#define XRD_PTOB(pn)	((u_offset_t)(pn) << PAGESHIFT)
#define XRD_BTOP(bt)	((bt) >> PAGESHIFT)
#define XRD_BTOPR(bt)	XRD_BTOP((bt) + PAGEOFFSET)

/*
 * NOTE:
 *  MD (machine dependent, aka. arch-dep) memory driver must provide
 *  some methods:
 *
 *  int xramdisk_arch_attach(xramdisk_t *);
 *
 *    The method should allocate physical-continuous memory region,
 *    and map these pages into kernel address space (KAS).
 *    When preset-image address (by page-number) is specified,
 *    the attacher must grub pages beginning from the address.
 *
 *    The method's inputs are (in xramdisk_t):
 *      xrd_numpages:  number of pages must be allocated.
 *      xrd_basepage:  (not XRAMDISK_NO_PRESET) page offset of preset image
 *                     specified by ddi-properties.
 *
 *    and outputs are:
 *      xrd_kasaddr:   mapped address in KAS.
 *      xrd_basepage:  physical page number at beginning of region.
 *      xrd_numpages:  (may override by arch-dep driver.)
 *
 *    then, the method must set following fields in the xramdisk_t:
 *      xrd_arch_getapgage:  method pointer of page fetcher.
 *      xrd_arch_detach:     detacher.
 *
 *  MD memory driver may use "xrd_arch_data" for private management.
 *
 *  void (*xrd_arch_detach)(xramdisk_t *);
 *    To detach (deallocate) physical memory region for the diskimage.
 *    In basically, all pages in the region will be returned to KAS map.
 *    (All vnodes are invalidated before unmount.)
 *
 *  int (*xrd_arch_getapage)(xramdisk_t *, pfn_t pgno, page_t **ppp);
 *    To get a page info (page_t *) which corresponding to a physical page
 *    in the disk-image region.
 *    Acquired page info will be stored in xramdisk page info table,
 *    and not be put back to the MD memory driver.
 *    Two state are defined for one page:
 *      XP_KVP:    the page is only stay in KAS map.
 *      XP_VNODE:  the page is named that belonging to specific vnode:offset.
 *    When upper FS (usually, XRAMFS) request a page for vnode
 *    (via VOP_GETPAGE()), XRAMDISK changes the state of the page to XP_VNODE
 *    (and page_rename(vn, off) for it). FS may pass it into page-cache
 *    directly.
 *
 *    When upper FS release the page for vnode, XRAMDISK collects it and
 *    changes the state of the page to XP_KVP.
 *
 * TODO: Make interface to get multiple continuous page at once.
 *  (inter xramdisk-MDdriver, FS-xramdisk, each)
 */
extern int	xramdisk_arch_attach(xramdisk_t *);

extern int	is_pseudo_device(dev_info_t *);

#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_XRAMDISK_H */
