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
 *  xramdisk: MI part implementation.
 */

#undef XRAMDISK_TRACE

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/buf.h>
#include <sys/modctl.h>
#include <sys/open.h>
#include <sys/kmem.h>
#include <sys/poll.h>
#include <sys/conf.h>
#include <sys/cmn_err.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/vnode.h>
#include <vm/seg_kmem.h>
#include <vm/hat.h>

#include <sys/xramdisk.h>

extern struct vnode kvp;

/*
 * An opaque handle where information about our set of xramdisk devices lives.
 */
static void	*xramdisk_statep;
#define XRD_GET_SOFTC(m)	(ddi_get_soft_state(xramdisk_statep,(m)))

/*
 * Pointer to devinfo for the 'pseudo' xramdisks.
 */
static dev_info_t *xramdisk_dip = NULL;

/*
 * Global state lock.
 */
static kmutex_t	xramdisk_lock;
#define XRD_LOCK_ALL()		(mutex_enter(&xramdisk_lock))
#define XRD_UNLOCK_ALL()	(mutex_exit(&xramdisk_lock))
#define XRD_ENSURE_LOCK_ALL()	(ASSERT(mutex_owned(&xramdisk_lock)))

/*
 * Maximum number of xramdisks supported by this driver.
 */
static uint32_t	xramdisk_max_disks = XRD_DFLT_DISKS;

/* Internal prototypes */
static int	xramdisk_getapage(xramdev_t *xdp, pfn_t pfn,
				  vnode_t *vp, offset_t voff, page_t **ppp);
static int	xramdisk_putapage(xramdev_t *xdp, pfn_t pfn, page_t *pp);


/*
 * Is the driver busy,
 * i.e. are there any pseudo xramdisk devices
 * (created by ioctl for control node) in existence?
 */
static int
xramdisk_is_busy(void)
{
	minor_t		minor;
	xramdisk_t	*xrdp;

	XRD_ENSURE_LOCK_ALL();

	for (minor = 1; minor <= xramdisk_max_disks; ++minor) {
		if ((xrdp = XRD_GET_SOFTC(minor)) != NULL &&
		    xrdp->xrd_dip == xramdisk_dip) {
			return (EBUSY);
		}
	}
	return (0);
}

/*
 * Find the first free minor number; returns zero if there isn't one.
 */
static minor_t
xramdisk_find_free_minor(void)
{
	minor_t		minor;

	XRD_ENSURE_LOCK_ALL();

	for (minor = 1; minor <= xramdisk_max_disks; ++minor) {
		if (XRD_GET_SOFTC(minor) == NULL) {
			return (minor);
		}
	}
	return (0);
}

/*
 * Locate the xramdisk_t for the named xramdisk; returns NULL if not found.
 * Each xramdisk is identified uniquely by name,
 */
static xramdisk_t *
xramdisk_find_named_disk(const char *name)
{
	minor_t		minor;
	xramdisk_t	*xrdp;

	XRD_ENSURE_LOCK_ALL();

	for (minor = 1; minor <= xramdisk_max_disks; ++minor) {
		if ((xrdp = XRD_GET_SOFTC(minor)) != NULL &&
		    strcmp(xrdp->xrd_name, name) == 0) {
			return (xrdp);
		}
	}
	return (NULL);
}

/*****************
 *
 * Disk volume managements.
 *
 *****************/

/*
 * NOTE:
 *  This device is not a regular block device, so, it's not required to
 *  fake disk geometry (like native ramdisk).
 */

/*
 * Allocate xramdisk instance.
 */
static xramdisk_t *
xramdisk_alloc_resources(char *name, pfn_t presetbase, pgcnt_t numpages,
			 dev_info_t *dip)
{
	xramdisk_t	*xrdp;
	char		namebuf[XRD_NAME_LEN + 1];
	minor_t		minor;
	dev_t		fulldev;

	int64_t		Nblocks_prop_val;
	int64_t		Size_prop_val;
	int64_t		BasePage_prop_val;
	int64_t		KVA_prop_val;

	ASSERT(dip == xramdisk_dip);

	XRD_ENSURE_LOCK_ALL();

	/* Allocate device instance. */
	minor = xramdisk_find_free_minor();
	if (minor == 0)
		goto rollback_softc;

	if (ddi_soft_state_zalloc(xramdisk_statep, minor) == DDI_FAILURE) {
		/* softc couldn't be allocated. */
		goto rollback_softc;
	}
	xrdp = ddi_get_soft_state(xramdisk_statep, minor);

	/* Fill base members. */
	mutex_init(&xrdp->xrd_lock, NULL, MUTEX_DRIVER, NULL);
	(void) strcpy(xrdp->xrd_name, name);
	xrdp->xrd_dip = dip;
	xrdp->xrd_minor = minor;

	/* Allocate phy-mem and KAS mapping resource. */
	xrdp->xrd_numpages = numpages;
	xrdp->xrd_basepage = presetbase;	/* != 0 means configured
						   with preset image. */
	if (xramdisk_arch_attach(xrdp) != 0) {
		goto rollback_phymem;
	}

	xrdp->xrd_getapage = xramdisk_getapage;
	xrdp->xrd_putapage = xramdisk_putapage;

	if (xrdp->xrd_numpages != numpages) {
#ifdef XRAMDISK_TRACE
		printf("xramdisk: %s: numpages overridden by arch-dep driver: "
		       "from %" PRIu64 " to %" PRIu64 "\n",
		       name, (uint64_t)numpages, (uint64_t)xrdp->xrd_numpages);
#endif
		numpages = xrdp->xrd_numpages;
	}

	/* Allocate memory management infos. */
	xrdp->xrd_pageinfo = kmem_zalloc(XRD_PAGEINFO_SIZE(xrdp), KM_SLEEP);
	if (xrdp->xrd_pageinfo == NULL) {
		goto rollback_pageinfo;
	}

	/* Create device node. */
	/*
	 *  device node: /devices/pseudo/xramdisk@0:<diskname>
	 */
	strcpy(namebuf, name);
	if (ddi_create_minor_node(dip, namebuf, S_IFCHR, minor, DDI_PSEUDO, 0)
	    == DDI_FAILURE) {
		goto rollback_devnode;
	}

	/* Register "Size" and "Nblocks" properties. */
	fulldev = makedevice(ddi_driver_major(dip), minor);

	Size_prop_val = (int64_t)XRD_SIZE(xrdp);
	if ((ddi_prop_update_int64(fulldev, dip, XRD_SIZE_PROP_NAME,
				   Size_prop_val)) != DDI_PROP_SUCCESS) {
		goto rollback_prop_size;
	}

	Nblocks_prop_val = (int64_t)xrdp->xrd_numpages;
	if ((ddi_prop_update_int64(fulldev, dip, XRD_NBLOCKS_PROP_NAME,
				   Nblocks_prop_val)) != DDI_PROP_SUCCESS) {
		goto rollback_prop_nblocks;
	}

	BasePage_prop_val = (int64_t)xrdp->xrd_basepage;
	if ((ddi_prop_update_int64(fulldev, dip, XRD_BASEPAGE_PROP_NAME,
				   BasePage_prop_val)) != DDI_PROP_SUCCESS) {
		goto rollback_prop_basepage;
	}

	KVA_prop_val = (int64_t)((intptr_t)xrdp->xrd_kasaddr);
	if ((ddi_prop_update_int64(fulldev, dip, XRD_KVA_PROP_NAME,
				   KVA_prop_val)) != DDI_PROP_SUCCESS) {
		goto rollback_prop_kva;
	}

#ifdef XRAMDISK_TRACE
	printf("xramdisk: %s: allocated %" PRIu64 " pages starting from "
	       "%" PRIu64 " (va=%p)\n",
	       name, (uint64_t)xrdp->xrd_numpages, (uint64_t)xrdp->xrd_basepage,
	       xrdp->xrd_kasaddr);
#endif

	return (xrdp);


	/*
	 * Error rollback operations.
	 */
rollback_prop_kva:
	(void) ddi_prop_remove(fulldev, dip, XRD_BASEPAGE_PROP_NAME);
rollback_prop_basepage:
	(void) ddi_prop_remove(fulldev, dip, XRD_NBLOCKS_PROP_NAME);
rollback_prop_nblocks:
	(void) ddi_prop_remove(fulldev, dip, XRD_SIZE_PROP_NAME);
rollback_prop_size:
	ddi_remove_minor_node(dip, namebuf);
rollback_devnode:
	kmem_free(xrdp->xrd_pageinfo, XRD_PAGEINFO_SIZE(xrdp));
rollback_pageinfo:	
	(*xrdp->xrd_arch_detach)(xrdp);
rollback_phymem:
	mutex_destroy(&xrdp->xrd_lock);
	ddi_soft_state_free(xramdisk_statep, minor);
rollback_softc:

	return (NULL);
}

/*
 * Deallocate xramdisk instance.
 */
static void
xramdisk_dealloc_resources(xramdisk_t *xrdp)
{
	dev_info_t	*dip = xrdp->xrd_dip;
	char		namebuf[XRD_NAME_LEN + 1];
	minor_t		minor = xrdp->xrd_minor;
	dev_t		fulldev;

	ASSERT(dip == xramdisk_dip);
	XRD_ENSURE_LOCK_ALL();

	/* Remove "Size" and "Nblocks" properties. */
	fulldev = makedevice(ddi_driver_major(dip), minor);
	(void) ddi_prop_remove(fulldev, dip, XRD_KVA_PROP_NAME);
	(void) ddi_prop_remove(fulldev, dip, XRD_BASEPAGE_PROP_NAME);
	(void) ddi_prop_remove(fulldev, dip, XRD_NBLOCKS_PROP_NAME);
	(void) ddi_prop_remove(fulldev, dip, XRD_SIZE_PROP_NAME);

	/* Destroy device node. */
	strcpy(namebuf, xrdp->xrd_name);
	ddi_remove_minor_node(dip, namebuf);

	/* Deallocate page info table. */
	kmem_free(xrdp->xrd_pageinfo, XRD_PAGEINFO_SIZE(xrdp));

	/* Deallocate memory resource. */
	(*xrdp->xrd_arch_detach)(xrdp);

	/* Deallocate softc. */
	mutex_destroy(&xrdp->xrd_lock);
	ddi_soft_state_free(xramdisk_statep, minor);
}

/*
 * Create xramdisk instance.
 */
static int
xramdisk_create_disk(char *name, pgcnt_t numpages)
{
	xramdisk_t	*xrdp;
	int		error = 0;

	XRD_LOCK_ALL();

	/* Check for existence. */
	if (xramdisk_find_named_disk(name) != NULL) {
		/* named disk drive already exists. */
		error = EEXIST;
		goto leave;
	}

	/* Allocate xramdisk_t instance and pageinfo table. */
	xrdp = xramdisk_alloc_resources(name, XRAMDISK_NO_PRESET, numpages,
					xramdisk_dip);
	if (xrdp == NULL) {
		/* no kernel memory. */
		error = EAGAIN;
		goto leave;
	}

leave:
	XRD_UNLOCK_ALL();
	return (error);
}


/*
 * Destroy xramdisk instance.
 */
static int
xramdisk_delete_disk(char *name)
{
	xramdisk_t	*xrdp;
	int		error = 0;

	XRD_LOCK_ALL();

	/* Check for existence. */
	xrdp = xramdisk_find_named_disk(name);
	if (xrdp == NULL) {
		error = ENXIO;
		goto leave;
	}

	if (xrdp->xrd_dip != xramdisk_dip) {
		error = EINVAL;
		goto leave;
	}

	/* Check for references. */
	if (XRD_IS_OPENED(xrdp)) {
		error = EBUSY;
		goto leave;
	}

	/* Deallocate resources. */
	xramdisk_dealloc_resources(xrdp);

leave:
	XRD_UNLOCK_ALL();
	return (error);
}

/*****************
 *
 * FS driver interface side.
 *
 *****************/

/* from sys/xramdisk.h */

/*
 * for mount/unmount (nearly open/close)
 */

/*ARGUSED*/
static int
xramdisk_bind(vnode_t *vp, vfs_t *vfsp, int flags, xramdev_t **xdpp)
{
	dev_t		dev = vp->v_rdev;
	xramdisk_t	*xrdp = NULL;
	xramdev_t	*xdp;
	major_t		major, xmajor;
	minor_t		minor;
	int		error;

	/* Search xramdisk node that has the specified device number. */
	ASSERT(xramdisk_dip);
	xmajor = ddi_driver_major(xramdisk_dip);
	major = getmajor(dev);
	if (major != xmajor) {
		return (ENODEV);
	}

	XRD_LOCK_ALL();
	minor = getminor(dev);
	if ((xrdp = XRD_GET_SOFTC(minor)) == NULL) {
		error = ENODEV;
		goto out;
	}

	xdp = XRDTOXD(xrdp);
	if ((flags & VWRITE) && !XRAMDEV_WRITABLE(xdp)) {
		/* Deny writable mount request on read-only device. */
		error = EACCES;
		goto out;
	}
	if (XRAMDEV_ISMOUNTED(xdp)) {
		error = EBUSY;
		goto out;
	}
	XRAMDEV_SETMOUNTED(xdp);
	*xdpp = xdp;
	error = 0;

 out:
	XRD_UNLOCK_ALL();

	return (error);
}

/*ARGUSED*/
static int
xramdisk_unbind(xramdev_t *xdp, vfs_t *vfsp)
{
	ASSERT(xdp->xd_dip == xramdisk_dip);

	XRD_LOCK_ALL();
	XRAMDEV_CLRMOUNTED(xdp);
	XRD_UNLOCK_ALL();

	return (0);
}

/* get/put a page. */
static int
xramdisk_getapage(xramdev_t *xdp, pfn_t pfn /* in volume. */,
		  vnode_t *vp, offset_t voff, page_t **ppp /*OUT*/)
{
	xramdisk_t	*xrdp = XDTOXRD(xdp);
	page_t	*pp, *npp;
	int	error;

	/* Sanity check. */
	if (pfn >= xrdp->xrd_numpages)
		return (ENXIO);

	XRD_LOCK(xrdp);

	/* Fetch a page instance from MD memory driver,
	   when it doesn't exist in xramdisk page_t-cache table. */
	pp = xrdp->xrd_pageinfo[pfn].xp_page;
	if (pp == NULL) {
		/* Fetch, */
		error = xrdp->xrd_arch_getapage(xrdp, pfn, &pp);
		if (error) {
			XRD_UNLOCK(xrdp);
			return (error);
		}

		/* and cache ever. */
		xrdp->xrd_pageinfo[pfn].xp_page = pp;
	} else if (xrdp->xrd_pageinfo[pfn].xp_state != XP_KVP) {
		/* Someone created the page after lookup. */
		XRD_UNLOCK(xrdp);
		return (EAGAIN);
	} else {
		/* Get exclusive-lock for the page. */
		ASSERT(PAGE_SHARED(pp));
		if (!page_tryupgrade(pp)) {
			page_unlock(pp);
			while (!page_lock(pp, SE_EXCL,
					  (kmutex_t *)NULL, P_RECLAIM));
		}
	}

	/* Note: the page gotten is locked exclusively. */
	ASSERT(pp);

	/* At here, the page should be bound for kvp:kva */
	ASSERT(xrdp->xrd_pageinfo[pfn].xp_state == XP_KVP);
	ASSERT(pp->p_vnode == &kvp);

	/* Rename the page binding to vp:off */
	page_rename(pp, vp, voff);

	/* And change the state of page_t cache entry. */
	xrdp->xrd_pageinfo[pfn].xp_state = XP_VNODE;

	/* Acquire IO-lock (preparation for page list operation). */
	page_io_lock(pp);
#ifdef XRAMDISK_TRACE
	printf("   page %p, io-locked\n", pp);
#endif

	XRD_UNLOCK(xrdp);
	*ppp = pp;
	return (0);
}

static int
xramdisk_putapage(xramdev_t *xdp, pfn_t pfn, page_t *pp)
{
	xramdisk_t	*xrdp = XDTOXRD(xdp);

	/* Sanity check. */
	ASSERT(pfn < xrdp->xrd_numpages);
	ASSERT(pp == xrdp->xrd_pageinfo[pfn].xp_page);

	/* At here, the page should be bound for vp:off */
	ASSERT(xrdp->xrd_pageinfo[pfn].xp_state == XP_VNODE);

	XRD_LOCK(xrdp);

	/* Purge existing translations. */
	(void)hat_pageunload(pp, HAT_FORCE_PGUNLOAD);

	/* Rename the page binding to kvp:virt-offs */
	page_io_unlock(pp);	/* NOTE safety */
	page_rename(pp, &kvp,
		    (u_offset_t)(uintptr_t)(xrdp->xrd_kasaddr +
					    pfn * PAGESIZE));

	/* Downgrade page lock. */
	page_downgrade(pp);

	/* And change the state of page_t cache entry. */
	xrdp->xrd_pageinfo[pfn].xp_state = XP_KVP;

	XRD_UNLOCK(xrdp);
	return (0);
}


/*****************
 *
 * Device driver interface side.
 *
 *****************/

/*
 * ddi-op: getinfo()
 */
/*ARGUSED*/
static int
xramdisk_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
    void **result)
{
	xramdisk_t *xrdp;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		xrdp = XRD_GET_SOFTC(getminor((dev_t)arg));
		if (xrdp == NULL) {
			*result = NULL;
			return (DDI_FAILURE);
		}
		*result = xrdp->xrd_dip;
		return (DDI_SUCCESS);

	case DDI_INFO_DEVT2INSTANCE:
		xrdp = XRD_GET_SOFTC(getminor((dev_t)arg));
		if (xrdp == NULL) {
			*result = NULL;
			return (DDI_FAILURE);
		}
		*result = (void *)(uintptr_t)ddi_get_instance(xrdp->xrd_dip);
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}
}

/*
 * ddi-op: attach()
 */
static int
xramdisk_attach_ctl(dev_info_t *dip)
{
	xramdisk_t		*xrdp;

	XRD_ENSURE_LOCK_ALL();

	xramdisk_dip = dip;

	/* Allocate instance for control node. */
	if (ddi_soft_state_zalloc(xramdisk_statep, XRD_CTL_MINOR)
	    == DDI_FAILURE) {
		goto rollback_softc;
	}

	xrdp = XRD_GET_SOFTC(XRD_CTL_MINOR);
	xrdp->xrd_dip = dip;

	/* Create minor devnode for control node. */
	if (ddi_create_minor_node(dip, XRD_CTL_NODE, S_IFCHR, XRD_CTL_MINOR,
				  DDI_PSEUDO, NULL) == DDI_FAILURE) {
		goto rollback_devnode;
	}

	return (DDI_SUCCESS);

	/* Rollback on error. */
rollback_devnode:
	ddi_soft_state_free(xramdisk_statep, XRD_CTL_MINOR);
rollback_softc:
	return (DDI_FAILURE);
}

static int
xramdisk_attach(dev_info_t *dip, ddi_attach_cmd_t attachcmd)
{
	int	rc;
	switch (attachcmd) {
	case DDI_ATTACH:
		XRD_LOCK_ALL();
		rc = xramdisk_attach_ctl(dip);
		XRD_UNLOCK_ALL();

		if (rc == DDI_SUCCESS) {
			ddi_report_dev(dip);
		}
		return (rc);

	case DDI_RESUME:
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}
}

/*
 * ddi-op: detach()
 */

/*ARGUSED*/
static int
xramdisk_detach_ctl(dev_info_t *dip)
{
	XRD_ENSURE_LOCK_ALL();

	/* Check that driver is busy. */
	if (xramdisk_is_busy()) {
		return (DDI_FAILURE);
	}

	/* Deallocate control node instance. */
	ddi_soft_state_free(xramdisk_statep, XRD_CTL_MINOR);
	xramdisk_dip = NULL;

	ddi_remove_minor_node(dip, NULL);

	return (DDI_SUCCESS);
}

static int
xramdisk_detach(dev_info_t *dip, ddi_detach_cmd_t detachcmd)
{
	int	rc;

	switch (detachcmd) {
	case DDI_DETACH:
		XRD_LOCK_ALL();
		rc = xramdisk_detach_ctl(dip);
		XRD_UNLOCK_ALL();
		return (rc);

	case DDI_SUSPEND:
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}
}

/*
 * devop: open()
 */

/*ARGUSED*/
static int
xramdisk_open(dev_t *devp, int flag, int otyp, cred_t *credp)
{
	minor_t		minor;
	xramdisk_t	*xrdp;
	int		error = 0;

	if (otyp != OTYP_CHR) {
		return (EINVAL);
	}

	XRD_LOCK_ALL();

	minor = getminor(*devp);

	xrdp = XRD_GET_SOFTC(minor);
	if (xrdp == NULL) {
		error = ENXIO;		/* some insanity? panic? for ctlnode */
		goto leave;
	}

	if (minor == XRD_CTL_MINOR) {
		/* Open for control device: must be opened exclusively. */
		if ((flag & FEXCL) != FEXCL) {
			error = EINVAL;
			goto leave;
		}

		if (XRD_IS_OPENED(xrdp)) {
			error = EBUSY;
			goto leave;
		}
	} else {
		if (XRAMDEV_ISMOUNTED(XRDTOXD(xrdp))) {
			error = EBUSY;
			goto leave;
		}
	}		

	/* Mark it's opened. */
	XRD_REF(xrdp);

leave:
	XRD_UNLOCK_ALL();
	return (error);
}

/*
 * devop: close()
 */

/*ARGUSED*/
static int
xramdisk_close(dev_t dev, int flag, int otyp, cred_t *credp)
{
	minor_t		minor;
	xramdisk_t	*xrdp;
	int		error = 0;

	XRD_LOCK_ALL();

	minor = getminor(dev);
	xrdp = XRD_GET_SOFTC(minor);
	ASSERT(xrdp != NULL);

	/* Unmark it's opened. */
	XRD_UNREF(xrdp);

	XRD_UNLOCK_ALL();
	return (error);
}

/*
 * devop: read()
 */
static int
xramdisk_read(dev_t dev, struct uio *uiop, cred_t *credp)
{
	xramdisk_t	*xrdp;
	ssize_t		nbytes;
	int		error;

	xrdp = XRD_GET_SOFTC(getminor(dev));
	ASSERT(xrdp != NULL);

	nbytes = uiop->uio_resid;

	/* Transfer size clipping. */
	if (uiop->uio_loffset >= XRD_SIZE(xrdp)) {
		return (ENXIO);
	}

	if (uiop->uio_loffset + (offset_t)nbytes > XRD_SIZE(xrdp)) {
		nbytes = XRD_SIZE(xrdp) - uiop->uio_loffset;
	}

	error = uiomove(xrdp->xrd_kasaddr + uiop->uio_loffset, nbytes,
			UIO_READ, uiop);
	return (error);
}

/*
 * devop: write()
 */
static int
xramdisk_write(dev_t dev, struct uio *uiop, cred_t *credp)
{
	xramdisk_t	*xrdp;
	ssize_t		nbytes;
	int		error;

	xrdp = XRD_GET_SOFTC(getminor(dev));
	ASSERT(xrdp != NULL);

	nbytes = uiop->uio_resid;

	/* Transfer size clipping. */
	if (uiop->uio_loffset >= XRD_SIZE(xrdp)) {
		return (ENXIO);
	}

	if (uiop->uio_loffset + (offset_t)nbytes > XRD_SIZE(xrdp)) {
		nbytes = XRD_SIZE(xrdp) - uiop->uio_loffset;
	}

	error = uiomove(xrdp->xrd_kasaddr + uiop->uio_loffset, nbytes,
			UIO_WRITE, uiop);
	return (error);
}

/*
 * devop: ioctl()
 */
/*ARGSUSED*/
static int
xramdisk_ioctl(dev_t dev, int cmd, intptr_t arg, int mode, cred_t *credp,
    int *rvalp)
{
	minor_t		minor;
	struct xrd_ioctl kxri;

	minor = getminor(dev);

	/*
	 * XRAMDISK ioctl is supported only for control node.
	 */
	if (minor != XRD_CTL_MINOR) {
		return (ENOTTY);
	}

	if (ddi_copyin((void *)arg, &kxri, sizeof(kxri), mode) == -1) {
		return (EFAULT);
	}

	kxri.xri_name[XRD_NAME_LEN] = '\0';

	switch (cmd) {
	case XRD_CREATE_DISK:
		if ((mode & FWRITE) == 0)
			return (EPERM);

		/* check overflow */
		if (kxri.xri_numpages >=
		    (1 << (sizeof(void *) * 8 - PAGESHIFT)))
			return (EINVAL);

		return (xramdisk_create_disk(kxri.xri_name,
					     (pgcnt_t)kxri.xri_numpages));

	case XRD_DELETE_DISK:
		if ((mode & FWRITE) == 0)
			return (EPERM);
		return (xramdisk_delete_disk(kxri.xri_name));

	default:
		return (ENOTTY);
	}
}


/*
 * Operation table for device driver interface.
 */
static struct cb_ops xramdisk_cb_ops = {
	xramdisk_open,
	xramdisk_close,
	nodev,		/* strategy */
	nodev,		/* print */
	nodev,		/* dump */
	xramdisk_read,
	xramdisk_write,
	xramdisk_ioctl,
	nodev,		/* devmap */
	nodev,		/* mmap */
	nodev,		/* segmap */
	nochpoll,	/* poll */
	ddi_prop_op,
	NULL,
	D_NEW | D_MP
};

static struct dev_ops xramdisk_ops = {
	DEVO_REV,
	0,
	xramdisk_getinfo,
	nulldev,	/* identify */
	nulldev,	/* probe */
	xramdisk_attach,
	xramdisk_detach,
	nodev,		/* reset */
	&xramdisk_cb_ops,
	(struct bus_ops *)0
};

/*****************
 *
 * Kernel module declaration.
 *
 */
extern struct mod_ops mod_driverops;

#define	XRAMDISK_VERSION_STR	"v1.00"

static struct modldrv modldrv = {
	&mod_driverops,
	"xramdisk driver " XRAMDISK_VERSION_STR,
	&xramdisk_ops
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
	0
};

int
MODDRV_ENTRY_INIT(void)
{
	int e;

	if ((e = ddi_soft_state_init(&xramdisk_statep,
	    sizeof (xramdisk_t), 0)) != 0) {
		return (e);
	}

	mutex_init(&xramdisk_lock, NULL, MUTEX_DRIVER, NULL);

	if ((e = xramdev_bindfunc_register(xramdisk_bind, xramdisk_unbind))
	    != 0) {
		mutex_destroy(&xramdisk_lock);
		ddi_soft_state_fini(&xramdisk_statep);
		return (e);
	}

	if ((e = mod_install(&modlinkage)) != 0)  {
		xramdev_bindfunc_unregister(xramdisk_bind, xramdisk_unbind);
		mutex_destroy(&xramdisk_lock);
		ddi_soft_state_fini(&xramdisk_statep);
	}

	return (e);
}

#ifndef STATIC_DRIVER
int
MODDRV_ENTRY_FINI(void)
{
	int e;

	if ((e = mod_remove(&modlinkage)) != 0)  {
		return (e);
	}

	(void)xramdev_bindfunc_unregister(xramdisk_bind, xramdisk_unbind);
	ddi_soft_state_fini(&xramdisk_statep);
	mutex_destroy(&xramdisk_lock);

	return (e);
}
#endif /* !STATIC_DRIVER */

int
MODDRV_ENTRY_INFO(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
