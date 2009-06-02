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
 * Physical device driver for xramfs.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/modctl.h>
#include <sys/open.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/cmn_err.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/xramdev.h>
#include <sys/xramdev_impl.h>
#include <sys/fs/snode.h>

/*
 * Handle to keep software state.
 */
static void	*xramdev_state;

/*
 * Functions to bind or unbind pseudo xramdisk device.
 * These pointers will be initialized when xramdisk driver is loaded.
 */
static xd_bindfunc_t	xramdev_bindfunc;
static xd_unbindfunc_t	xramdev_unbindfunc;
static kmutex_t		xramdev_bindfunc_lock;

#define	XRAMDEV_BINDFUNC_LOCK()		mutex_enter(&xramdev_bindfunc_lock)
#define	XRAMDEV_BINDFUNC_UNLOCK()	mutex_exit(&xramdev_bindfunc_lock)

#define	XRAMDEV_PROP_SIZE		"Size"
#define	XRAMDEV_PROP_NBLOCKS		"Nblocks"

/* Definitions for character special file. */
#define	XRAMDEV_CHR_SUFFIX		",raw"
#define	XRAMDEV_CHR_MAX_NAMELEN		(XRAMDEV_MAX_NAMELEN + 5)
#define	XRAMDEV_CHR_NODENAME(namebuf, namesz, name)		\
	snprintf((namebuf), (namesz), "%s" XRAMDEV_CHR_SUFFIX,	\
		 (name))

/* Determine whether the given open type is valid or not. */
#ifdef	XRAMDEV_CDEV
#define	XRAMDEV_VALID_OTYPE(otyp)			\
	((otyp) == OTYP_BLK || (otyp) == OTYP_CHR)
#else	/* !XRAMDEV_CDEV */
#define	XRAMDEV_VALID_OTYPE(otyp)	((otyp) == OTYP_BLK)
#endif	/* XRAMDEV_CDEV */

/* Internal prototypes */
static int	xramdev_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int	xramdev_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);
static int	xramdev_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg,
				void **result);
static int	xramdev_open(dev_t *devp, int flag, int otyp, cred_t *credp);
static int	xramdev_close(dev_t dev, int flag, int otyp, cred_t *credp);
static int	xramdev_strategy(struct buf *bp);

static int	xramdev_getapage(xramdev_t *xdp, pfn_t pfn, vnode_t *vp,
				 offset_t voff, page_t **ppp);
static int	xramdev_putapage(xramdev_t *xdp, pfn_t pfn, page_t *pp);

#ifdef	XRAMDEV_CDEV

static int	xramdev_read(dev_t dev, struct uio *uiop, cred_t *credp);
static int	xramdev_write(dev_t dev, struct uio *uiop, cred_t *credp);
static int	xramdev_rdwr(dev_t dev, struct uio *uiop, cred_t *credp,
			     enum uio_rw rw);

#define	XRAMDEV_OPS_READ	xramdev_read
#define	XRAMDEV_OPS_WRITE	xramdev_write

#else	/* !XRAMDEV_CDEV */

#define	XRAMDEV_OPS_READ	nodev
#define	XRAMDEV_OPS_WRITE	nodev

#endif	/* XRAMDEV_CDEV */

static void	xramdev_free(xramdev_t *xdp);

/* Lookup xramdev structure by minor number. */
#define	XRAMDEV_LOOKUP(minor)						\
	((xramdev_t *)ddi_get_soft_state(xramdev_state, (minor)))

/* Determine whether the specified dev_info is xramdev node. */
#define	XRAMDEV_IS_DEVDIP(dip)			\
	(ddi_get_driver(dip) == &xramdev_ops)

/*
 * DDI definitions 
 */

static struct cb_ops xramdev_cb_ops = {
	xramdev_open,
	xramdev_close,
	xramdev_strategy,
	nodev,			/* print */
	nodev,			/* dump */
	XRAMDEV_OPS_READ,
	XRAMDEV_OPS_WRITE,
	nodev,			/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev,			/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,
	NULL,
	D_NEW | D_MP
};

static struct dev_ops xramdev_ops = {
	DEVO_REV,
	0,
	xramdev_getinfo,
	nulldev,		/* identify */
	nulldev,		/* probe */
	xramdev_attach,
	xramdev_detach,
	nodev,			/* reset */
	&xramdev_cb_ops,
	(struct bus_ops *)0
};

#define	XRAMDEV_VERSION_STR	"v1.00"

extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops,
	"xramdev driver " XRAMDEV_VERSION_STR,
	&xramdev_ops
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
	0
};

/*
 * int
 * xramdev_bindfunc_register(xd_bindfunc_t bind, xd_unbindfunc_t unbind)
 *	Register xramdev bind functions.
 *	The function passed to "bind" will be called when xramdev_bind()
 *	tries to bind pseudo xramdisk device.
 *	The function passed to "unbind" will be called when xramdev_unbind()
 *	tries to unbind pseudo xramdisk device.
 *
 * Calling/Exit State:
 *	Upon successful completion, xramdev_bindfunc_register() returns zero.
 *	Otherwise, it returns error number that indicates the cause of error.
 *
 * Remarks:
 *	Both functions are called with holding xramdev_bindfunc_lock.
 *	So they must NOT call the following functions:
 *
 *	- xramdev_bindfunc_register()
 *	- xramdev_bindfunc_unregister()
 *	- xramdev_bind()
 *	- xramdev_unbind()
 */
int
xramdev_bindfunc_register(xd_bindfunc_t bind, xd_unbindfunc_t unbind)
{
	int	error;

	if (bind == NULL || unbind == NULL) {
		return (EINVAL);
	}

	XRAMDEV_BINDFUNC_LOCK();
	if (xramdev_bindfunc == NULL && xramdev_unbindfunc == NULL) {
		xramdev_bindfunc = bind;
		xramdev_unbindfunc = unbind;
		error = 0;
	}
	else {
		error = EBUSY;
	}
	XRAMDEV_BINDFUNC_UNLOCK();

	return (error);
}

/*
 * int
 * xramdev_bindfunc_unregister(xd_bindfunc_t bind, xd_unbindfunc_t unbind)
 *	Unregister xramdev bind functions registered by
 *	xramdev_bindfunc_register().
 *
 * Calling/Exit State:
 *	Upon successful completion, xramdev_bindfunc_register() returns zero.
 *	Otherwise, it returns error number that indicates the cause of error.
 */
int
xramdev_bindfunc_unregister(xd_bindfunc_t bind, xd_unbindfunc_t unbind)
{
	int	error;

	XRAMDEV_BINDFUNC_LOCK();
	if (xramdev_bindfunc == bind && xramdev_unbindfunc == unbind) {
		error = 0;
		xramdev_bindfunc = NULL;
		xramdev_unbindfunc = NULL;
	}
	else {
		error = EINVAL;
	}
	XRAMDEV_BINDFUNC_UNLOCK();

	return (error);
}

/*
 * int
 * xramdev_bind(vnode_t *vp, vfs_t *vfsp, int flags, xramdev_t **xdpp)
 *	Hold xramfs device, and bind it to the xramfs filesystem.
 *	"flags" should be VREAD on read-only mount, and be VREAD|VWRITE on
 *	writable mount.
 *
 * Calling/Exit State:
 *	Upon successful completion, xramdev_bind() returns 0 and set
 *	struct xramdev address into *xdpp.
 *	Otherwise, it returns error number that indicates the cause of error.
 *
 *	The caller must guarantee that vp is already opened.
 */
int
xramdev_bind(vnode_t *vp, vfs_t *vfsp, int flags, xramdev_t **xdpp)
{
	xramdev_t	*xdp;
	dev_info_t	*dip;
	int		error, instance;

	if (vp->v_type != VBLK || !vn_matchops(vp, spec_getvnodeops())) {
		goto bindfunc;
	}

	/* Lookup dev_info associated with the specified vnode. */
	if ((dip = spec_hold_devi_by_vp(vp)) == NULL) {
		goto bindfunc;
	}

	if (!XRAMDEV_IS_DEVDIP(dip)) {
		ddi_release_devi(dip);
		goto bindfunc;
	}

	instance = ddi_get_instance(dip);
	if ((xdp = XRAMDEV_LOOKUP(instance)) != NULL && xdp->xd_dip == dip) {
		XRAMDEV_LOCK(xdp);
		if ((flags & VWRITE) && !XRAMDEV_WRITABLE(xdp)) {
			/* Deny writable mount request on read-only device. */
			error = EACCES;
		}
		else if (XRAMDEV_ISMOUNTED(xdp)) {
			/* Already mounted. */
			error = EBUSY;
		}
		else {
			XRAMDEV_SETMOUNTED(xdp);
			error = 0;
			*xdpp = xdp;
		}
		XRAMDEV_UNLOCK(xdp);
		ddi_release_devi(dip);
		return (error);
	}
	ddi_release_devi(dip);

 bindfunc:
	/*
	 * Device not found. Try alternative driver.
	 */
	XRAMDEV_BINDFUNC_LOCK();
	if (xramdev_bindfunc != NULL) {
		error = (*xramdev_bindfunc)(vp, vfsp, flags, xdpp);
	}
	else {
		error = ENODEV;
	}
	XRAMDEV_BINDFUNC_UNLOCK();

	return (error);
}

/*
 * int
 * xramdev_unbind(xramdev_t *xdp, vfs_t *vfsp)
 *	Release reference to xramfs device.
 *
 * Calling/Exit State:
 *	Upon successful completion, xramdev_unbind() returns 0.
 *	Otherwise, it returns error number that indicates the cause of error.
 *
 *	The caller must guarantee that xdp is still opened.
 */
int
xramdev_unbind(xramdev_t *xdp, vfs_t *vfsp)
{
	dev_info_t	*dip = xdp->xd_dip;
	int		error;

	if (XRAMDEV_IS_DEVDIP(dip)) {
		XRAMDEV_LOCK(xdp);
		XRAMDEV_CLRMOUNTED(xdp);
		XRAMDEV_UNLOCK(xdp);
		return (0);
	}

	XRAMDEV_BINDFUNC_LOCK();
	if (xramdev_unbindfunc != NULL) {
		error = (*xramdev_unbindfunc)(xdp, vfsp);
	}
	else {
		error = ENOENT;
	}
	XRAMDEV_BINDFUNC_UNLOCK();

	return (error);
}

/*
 * static int
 * xramdev_getapage(xramdev_t *xdp, pfn_t pfn, vnode_t *vp, offset_t voff,
 *		    page_t **ppp)
 *	Get a page associated with the specified vnode and offset.
 *	Returned page is I/O and EXCL locked.
 *
 *	"pfn" must be a offset in the device.
 *	(i.e. pfn = real pfn - xdp->xd_basepage)
 *
 * Calling/Exit State:
 *	Upon successful completion, xramdev_getapage() returns zero.
 *	It returns EAGAIN if the specified page already exists.
 */
static int
xramdev_getapage(xramdev_t *xdp, pfn_t pfn, vnode_t *vp, offset_t voff,
		 page_t **ppp)
{
	page_t	*pp, *npp;
	int	error;

	if (pfn >= xdp->xd_numpages)
		return (ENXIO);

	pp = page_numtopp(XRAMDEV_PFN(xdp, pfn), SE_EXCL);
	ASSERT(pp);
	ASSERT(!PP_ISFREE(pp));

	if (pp->p_vnode != NULL) {
		/* Someone created the page after lookup. */
		page_unlock(pp);
		return (EAGAIN);
	}

	/*
	 * Unused xramdev page is always locked.
	 * Unlock here.
	 */
	ASSERT(pp->p_lckcnt > 0);
	pp->p_lckcnt--;

	/* Assign new identity. */
	if (!page_hashin(pp, vp, voff, NULL)) {
		panic("xramdev_getapage: page_hashin() failed pp=%pm vp=%p, "
		      "off=%llx", (void *)pp, (void *)vp, voff);
	}
	PP_CLRFREE(pp);
	PP_CLRAGED(pp);
	page_set_props(pp, P_REF);

	/* Acquire I/O lock. */
	page_io_lock(pp);

	*ppp = pp;

	return (0);
}

/*
 * static int
 * xramdev_putapage(xramdev_t *xdp, pfn_t pfn, page_t *pp)
 *	Release xramdev page.
 *	Page will be locked by p_lckcnt to protest against pageout scanner.
 *
 * Calling/Exit State:
 *	Currently, xramdev_putapage() always returns zero.
 */
static int
xramdev_putapage(xramdev_t *xdp, pfn_t pfn, page_t *pp)
{
	ASSERT(pfn < xdp->xd_numpages);
	ASSERT(XRAMDEV_PFN(xdp, pfn) == pp->p_pagenum);

	/* Destroy vp-off identity. */
	page_io_unlock(pp);
	page_destroy(pp, 1);
	page_clr_all_props(pp);

	/* Increment p_lckcnt so that pageout scanner can't free this page. */
	pp->p_lckcnt++;
	page_unlock(pp);

	return (0);
}

/*
 * static int
 * xramdev_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
 *	Attach xramfs physical device.
 */
static int
xramdev_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int		instance, unit;
	xramdev_t	*xdp;
	xdseg_t		*xsp;
	int		*reg_prop;
	uint_t		reg_len;
	size_t		size;
	uint64_t	prop_size, prop_nblocks;
	dev_t		dev;
	struct regspec	*regp;

	switch (cmd) {
	case DDI_RESUME:
		return (DDI_SUCCESS);
	case DDI_ATTACH:
		break;
	default:
		return (DDI_FAILURE);
	}

	/*
	 * Determine xramfs device instance number.
	 * Instance number is used as minor number for block device.
	 */
	instance = ddi_get_instance(dip);

	/* Assign software state. */
	if (ddi_soft_state_zalloc(xramdev_state, instance) != DDI_SUCCESS) {
		return (DDI_FAILURE);
	}
	xdp = XRAMDEV_LOOKUP(instance);
	ASSERT(xdp != NULL);

	/*
	 * Device information is passed via "reg" property.
	 * Unit number of xramfs device is passed via regspec->regspec_addr.
	 */
	if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
				      "reg", &reg_prop, &reg_len)
	    != DDI_PROP_SUCCESS) {
		goto err_soft;
	}
	if (reg_len * sizeof(int) != sizeof(struct regspec)) {
		ddi_prop_free((void *)reg_prop);
		goto err_soft;
	}

	regp = (struct regspec *)reg_prop;
	unit = (int)regp->regspec_addr;
	ddi_prop_free((void *)reg_prop);

	if ((xsp = xramdev_impl_getseg(unit)) == NULL) {
		goto err_soft;
	}

	/* Initialize struct xramdev. */
	mutex_init(&xdp->xd_lock, NULL, MUTEX_DRIVER, NULL);
	xdp->xd_dip = dip;
	xdp->xd_numpages = xsp->xs_count;
	xdp->xd_basepage = xsp->xs_base;
	xdp->xd_kasaddr = (caddr_t)xsp->xs_vaddr;
	xdp->xd_getapage = xramdev_getapage;
	xdp->xd_putapage = xramdev_putapage;
	xdp->xd_unit = unit;
	if (xsp->xs_attr & PROT_WRITE) {
		xdp->xd_flags = XDF_WRITABLE;
	}

	/* Create minor node. */
	if (ddi_create_minor_node(dip, (char *)xsp->xs_name, S_IFBLK, instance,
				  DDI_PSEUDO, 0) != DDI_SUCCESS) {
		goto err_mutex;
	}

#ifdef	XRAMDEV_CDEV
	{
		char		namebuf[XRAMDEV_CHR_MAX_NAMELEN];

		/*
		 * Create character special file for read/write.
		 * Block special file of xramdev never allows any I/O.
		 */
		XRAMDEV_CHR_NODENAME(namebuf, sizeof(namebuf), xsp->xs_name);
		if (ddi_create_minor_node(dip, namebuf, S_IFCHR, instance,
					  DDI_PSEUDO, 0) != DDI_SUCCESS) {
			goto err_minor_blk;
		}
	}
#endif	/* XRAMDEV_CDEV */

	/* Create size properties. */
	dev = makedevice(ddi_driver_major(dip), instance);
	size = XRAMDEV_SIZE(xdp);
	prop_size = (uint64_t)size;
	if ((ddi_prop_update_int64(dev, dip, XRAMDEV_PROP_SIZE, prop_size))
	    != DDI_PROP_SUCCESS) {
		goto err_minor_chr;
	}
	prop_nblocks = (uint64_t)(size / DEV_BSIZE);
	if ((ddi_prop_update_int64(dev, dip, XRAMDEV_PROP_NBLOCKS,
				   prop_nblocks)) != DDI_PROP_SUCCESS) {
		goto err_prop_size;
	}

	ddi_report_dev(dip);
	return (DDI_SUCCESS);

err_prop_size:
	(void)ddi_prop_remove(dev, dip, XRAMDEV_PROP_SIZE);

err_minor_chr:
#ifdef XRAMDEV_CDEV
	{
		char namebuf[XRAMDEV_CHR_MAX_NAMELEN];
		XRAMDEV_CHR_NODENAME(namebuf, sizeof(namebuf), xsp->xs_name);
		ddi_remove_minor_node(dip, namebuf);
	}
#endif /* XRAMDEV_CDEV */
err_minor_blk:
	ddi_remove_minor_node(dip, (char *)xsp->xs_name);
err_mutex:
	mutex_destroy(&xdp->xd_lock);
err_soft:
	ddi_soft_state_free(xramdev_state, instance);
	return (DDI_FAILURE);
}

/*
 * static int
 * xramdev_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
 *	Detach xramfs physical device.
 */
static int
xramdev_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	int		instance;
	xramdev_t	*xdp;

	switch (cmd) {
	case DDI_SUSPEND:
		return (DDI_SUCCESS);
	case DDI_DETACH:
		break;
	default:
		return (DDI_FAILURE);
	}

	instance = ddi_get_instance(dip);
	if ((xdp = XRAMDEV_LOOKUP(instance)) == NULL ||
	    XRAMDEV_IS_OPENED(xdp)) {
		return (DDI_FAILURE);
	}

	xramdev_free(xdp);

	return (DDI_SUCCESS);
}

/*
 * static int
 * xramdev_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg,
 *		   void **result)
 *	Get driver information.
 */
static int
xramdev_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg, void **result)
{
	int		ret = DDI_SUCCESS;
	xramdev_t	*xdp;
	minor_t		minor = getminor((dev_t)arg);

	if (cmd == DDI_INFO_DEVT2DEVINFO) {
		if ((xdp = XRAMDEV_LOOKUP(minor)) != NULL) {
			*result = xdp->xd_dip;
		}
		else {
			ret = DDI_FAILURE;
		}
	}
	else if (cmd == DDI_INFO_DEVT2INSTANCE) {
		*result = (void *)(uintptr_t)minor;
	}
	else {
		ret = DDI_FAILURE;
	}

	return (ret);
}

/*
 * static int
 * xramdev_open(dev_t *devp, int flag, int otyp, cred_t *credp)
 *	open(9E) entry point for xramdev driver.
 */
static int
xramdev_open(dev_t *devp, int flag, int otyp, cred_t *credp)
{
	minor_t		minor;
	xramdev_t	*xdp;

	if (!XRAMDEV_VALID_OTYPE(otyp)) {
		return (EINVAL);
	}

	minor = getminor(*devp);
	if ((xdp = XRAMDEV_LOOKUP(minor)) == NULL) {
		return (ENXIO);
	}

	if ((flag & FWRITE) && !XRAMDEV_WRITABLE(xdp)) {
		return (EACCES);	/* Write access is not allowed. */
	}

	XRAMDEV_LOCK(xdp);
	if (XRAMDEV_ISMOUNTED(xdp)) {
		XRAMDEV_UNLOCK(xdp);
		return (EBUSY);
	}
	XRAMDEV_REF(xdp, otyp);
	XRAMDEV_UNLOCK(xdp);

	return (0);
}

/*
 * static int
 * xramdev_close(dev_t dev, int flag, int otyp, cred_t *credp)
 *	close(9E) entry point for xramdev driver.
 */
static int
xramdev_close(dev_t dev, int flag, int otyp, cred_t *credp)
{
	minor_t		minor;
	xramdev_t	*xdp;

	minor = getminor(dev);
	xdp = XRAMDEV_LOOKUP(minor);
	ASSERT(xdp != NULL);

	XRAMDEV_LOCK(xdp);
	XRAMDEV_UNREF(xdp, otyp);
	XRAMDEV_UNLOCK(xdp);

	return (0);
}

/*
 * static int
 * xramdev_strategy(struct buf *bp)
 *	strategy(9E) entry point for xramdev driver.
 *
 * Calling/Exit State:
 *	xramdev is a special driver only for xramfs mounting.
 *	Although xramdev has a block device file, it never uses normal
 *	interface for block device. xramdev provides an interface for xramfs
 *	to obtain device page itself, and xramfs uses it as a file entity.
 *	If someone accesses xramdev device page via block device file,
 *	copy of device page will be cached in the page hash. It may break
 *	xramfs filesystem consistency.
 *
 *	That's why xramdev_strategy() always set ENXIO error into the specified
 *	buf structure.
 */
static int
xramdev_strategy(struct buf *bp)
{
	bioerror(bp, ENXIO);
	biodone(bp);

	return (0);
}

#ifdef	XRAMDEV_CDEV

/*
 * static int
 * xramdev_rdwr(dev_t dev, struct uio *uiop, cred_t *credp, enum uio_rw rw)
 *	Common read/write routine for xramdev driver.
 */
static int
xramdev_rdwr(dev_t dev, struct uio *uiop, cred_t *credp, enum uio_rw rw)
{
	xramdev_t	*xdp;
	ssize_t		nbytes;
	offset_t	maxoff;
	int		error;

	xdp = XRAMDEV_LOOKUP(getminor(dev));
	ASSERT(xdp != NULL);
	ASSERT(rw != UIO_WRITE || XRAMDEV_WRITABLE(xdp));

	nbytes = uiop->uio_resid;
	maxoff = (offset_t)XRAMDEV_SIZE(xdp);

	if (uiop->uio_loffset >= maxoff) {
		return (ENXIO);
	}

	if (uiop->uio_loffset + (offset_t)nbytes > maxoff) {
		nbytes = maxoff - uiop->uio_loffset;
	}

	error = uiomove(xdp->xd_kasaddr + uiop->uio_loffset, nbytes, rw, uiop);
	return (error);
}

/*
 * static int
 * xramdev_read(dev_t dev, struct uio *uiop, cred_t *credp)
 *	read(9E) entry point for xramdev driver.
 */
static int
xramdev_read(dev_t dev, struct uio *uiop, cred_t *credp)
{
	return (xramdev_rdwr(dev, uiop, credp, UIO_READ));
}

/*
 * static int
 * xramdev_write(dev_t dev, struct uio *uiop, cred_t *credp)
 *	write(9E) entry point for xramdev driver.
 */
static int
xramdev_write(dev_t dev, struct uio *uiop, cred_t *credp)
{
	return (xramdev_rdwr(dev, uiop, credp, UIO_WRITE));
}

#endif	/* XRAMDEV_CDEV */

/*
 * static void
 * xramdev_free(xramdev_t *xdp)
 *	Release all resources associated with the specified device.
 */
static void
xramdev_free(xramdev_t *xdp)
{
	int		instance;
	xdseg_t		*xsp;
	dev_info_t	*dip;
	dev_t		dev;

	dip = xdp->xd_dip;
	ASSERT(dip != NULL);

	/* Remove block special file. */
	if ((xsp = xramdev_impl_getseg(xdp->xd_unit)) != NULL) {

		ddi_remove_minor_node(dip, (char *)xsp->xs_name);

#ifdef	XRAMDEV_CDEV
		{
			char	namebuf[XRAMDEV_CHR_MAX_NAMELEN];

			XRAMDEV_CHR_NODENAME(namebuf, sizeof(namebuf),
					     xsp->xs_name);
			ddi_remove_minor_node(dip, namebuf);
		}
#endif	/* XRAMDEV_CDEV */
	}

	/* Remove properties. */
	instance = ddi_get_instance(dip);
	dev = makedevice(ddi_driver_major(dip), instance);
	(void)ddi_prop_remove(dev, dip, XRAMDEV_PROP_SIZE);
	(void)ddi_prop_remove(dev, dip, XRAMDEV_PROP_NBLOCKS);

#ifdef	DEBUG
	XRAMDEV_LOCK(xdp);
	ASSERT(!XRAMDEV_IS_OPENED(xdp));
	XRAMDEV_UNLOCK(xdp);
#endif	/* DEBUG */

	mutex_destroy(&xdp->xd_lock);
	ddi_soft_state_free(xramdev_state, instance);
}

/*
 * int
 * MODDRV_ENTRY_INIT(void)
 *	_init(9E) entry for xramdev driver.
 */
int
MODDRV_ENTRY_INIT(void)
{
	int	error;
	uint_t	count = xramdev_impl_segcount();

	if ((error = ddi_soft_state_init(&xramdev_state, sizeof(xramdev_t),
					 count)) != 0) {
		return (error);
	}

	mutex_init(&xramdev_bindfunc_lock, NULL, MUTEX_DRIVER, NULL);

	if ((error = mod_install(&modlinkage)) != 0)  {
		mutex_destroy(&xramdev_bindfunc_lock);
		ddi_soft_state_fini(&xramdev_state);
	}

	return (error);
}

#ifndef STATIC_DRIVER
/*
 * int
 * MODDRV_ENTRY_FINI(void)
 *	_fini(9E) entry for xramdev driver.
 */
int
MODDRV_ENTRY_FINI(void)
{
	int	error;

	XRAMDEV_BINDFUNC_LOCK();
	if (xramdev_bindfunc != NULL || xramdev_unbindfunc != NULL) {
		/* xramdisk driver is still active. */
		XRAMDEV_BINDFUNC_UNLOCK();
		return (EBUSY);
	}
	XRAMDEV_BINDFUNC_UNLOCK();

	if ((error = mod_remove(&modlinkage)) != 0)  {
		return (error);
	}

	mutex_destroy(&xramdev_bindfunc_lock);
	ddi_soft_state_fini(&xramdev_state);

	return (0);
}
#endif /* !STATIC_DRIVER */

/*
 * int
 * MODDRV_ENTRY_INFO(struct modinfo *modinfop)
 *	_info(9E) entry for xramdev driver.
 */
int
MODDRV_ENTRY_INFO(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
