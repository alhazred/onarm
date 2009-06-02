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
 * Copyright (c) 2007-2008 NEC Corporation
 * All rights reserved.
 */

/*
 * NVDRAM (Non-Volatile DRAM) device driver.
 * This driver treats memory reserved by kernel as device.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/autoconf.h>
#include <sys/modctl.h>
#include <sys/open.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/vmem.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/cmn_err.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/devpolicy.h>
#include <sys/nvdram_impl.h>
#include <vm/seg_kmem.h>
#include <vm/as.h>
#include <vm/hat.h>

/*
 * Handle to keep software state.
 */
static void	*nvdram_state;

#define	NVDRAM_PROP_SIZE	"Size"

/* Lookup nvdram structure by minor number. */
#define	NVDRAM_LOOKUP(minor)					\
	((nvdram_t *)ddi_get_soft_state(nvdram_state, (minor)))

/* Acquire/Release lock for struct nvdram. */
#define	NVDRAM_LOCK(ndp)	mutex_enter(&((ndp)->nd_lock))
#define	NVDRAM_UNLOCK(ndp)	mutex_exit(&((ndp)->nd_lock))

/* Modify or test INUSE flag. */
#define	NVDRAM_SET_INUSE(ndp)	((ndp)->nd_flags |= NVDF_INUSE)
#define	NVDRAM_CLR_INUSE(ndp)	((ndp)->nd_flags &= ~NVDF_INUSE)
#define	NVDRAM_IS_INUSE(ndp)	((ndp)->nd_flags & NVDF_INUSE)

/* Internal prototypes */
static int	nvdram_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int	nvdram_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);
static int	nvdram_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg,
			       void **result);
static int	nvdram_open(dev_t *devp, int flag, int otyp, cred_t *credp);
static int	nvdram_close(dev_t dev, int flag, int otyp, cred_t *credp);
static int	nvdram_read(dev_t dev, struct uio *uiop, cred_t *credp);
static int	nvdram_write(dev_t dev, struct uio *uiop, cred_t *credp);
static int	nvdram_mmap(dev_t dev, off_t offset, int prot);

static int	nvdram_rdwr(dev_t dev, struct uio *uiop, enum uio_rw rw);
static void	nvdram_free(nvdram_t *ndp);
static void	nvdram_mperm_init(void);

/*
 * DDI definitions 
 */

static struct cb_ops nvdram_cb_ops = {
	nvdram_open,
	nvdram_close,
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	nvdram_read,
	nvdram_write,
	nodev,			/* ioctl */
	nodev,			/* devmap */
	nvdram_mmap,
	nodev,			/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,
	NULL,
	D_NEW | D_MP
	/* XXX no aread/awrite */
};

static struct dev_ops nvdram_ops = {
	DEVO_REV,
	0,
	nvdram_getinfo,
	nulldev,		/* identify */
	nulldev,		/* probe */
	nvdram_attach,
	nvdram_detach,
	nodev,			/* reset */
	&nvdram_cb_ops,
	(struct bus_ops *)0
};

#define	NVDRAM_VERSION_STR	"v1.00"

extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops,
	"nvdram driver " NVDRAM_VERSION_STR,
	&nvdram_ops
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
	0
};

/*
 * static int
 * nvdram_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
 *	Attach nvdram device.
 */
static int
nvdram_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int		instance, unit;
	nvdram_t	*ndp;
	nvdseg_t	*nsp;
	int		*reg_prop, privflag;
	uint_t		reg_len;
	size_t		size;
	uint64_t	prop_size;
	dev_t		dev;
	struct regspec	*regp;

	if (cmd == DDI_RESUME) {
		return DDI_SUCCESS;
	}
	else if (cmd != DDI_ATTACH) {
		return DDI_FAILURE;
	}

	/*
	 * Determine nvdram device instance number.
	 * Instance number is used as minor number for device file.
	 */
	instance = ddi_get_instance(dip);

	/* Assign software state. */
	if (ddi_soft_state_zalloc(nvdram_state, instance) != DDI_SUCCESS) {
		return DDI_FAILURE;
	}
	ndp = NVDRAM_LOOKUP(instance);
	ASSERT(ndp != NULL);

	/*
	 * Device information is passed via "reg" property.
	 * Unit number of nvdram device is passed via regspec->regspec_addr.
	 */
	if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
				      "reg", &reg_prop, &reg_len)
	    != DDI_PROP_SUCCESS) {
		goto errout;
	}
	if (reg_len * sizeof(int) != sizeof(struct regspec)) {
		ddi_prop_free((void *)reg_prop);
		goto errout;
	}

	regp = (struct regspec *)reg_prop;
	unit = (int)regp->regspec_addr;
	ddi_prop_free((void *)reg_prop);

	if ((nsp = nvdram_impl_getseg(unit)) == NULL) {
		goto errout;
	}

	/* Initialize struct nvdram. */
	mutex_init(&ndp->nd_lock, NULL, MUTEX_DRIVER, NULL);
	ndp->nd_dip = dip;
	ndp->nd_flags = 0;
	ndp->nd_unit = unit;
	ndp->nd_seg = (const nvdseg_t *)nsp;
	ndp->nd_vaddr = vmem_alloc(heap_arena, PAGESIZE, VM_SLEEP);

	privflag = (nsp->ns_flags & NVDSEG_PRIVONLY) ? PRIVONLY_DEV : 0;

	/* Create minor node. */
	if (ddi_create_priv_minor_node(dip, (char *)nsp->ns_name, S_IFCHR,
				       instance, DDI_PSEUDO, privflag,
				       nsp->ns_rpriv, nsp->ns_wpriv,
				       nsp->ns_mode) != DDI_SUCCESS) {
		goto errout;
	}

	/* Create size property. */
	dev = makedevice(ddi_driver_major(dip), instance);
	size = mmu_ptob(nsp->ns_count);
	prop_size = (uint64_t)size;
	if ((ddi_prop_update_int64(dev, dip, NVDRAM_PROP_SIZE, prop_size))
	    != DDI_PROP_SUCCESS) {
		nvdram_free(ndp);
		return DDI_FAILURE;
	}

	ddi_report_dev(dip);
	return DDI_SUCCESS;

 errout:
	ddi_soft_state_free(nvdram_state, instance);
	return DDI_FAILURE;
}

/*
 * static int
 * xramdev_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
 *	Detach nvdram device.
 */
static int
nvdram_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	int		instance;
	nvdram_t	*ndp;

	if (cmd == DDI_SUSPEND) {
		return DDI_SUCCESS;
	}
	else if (cmd != DDI_DETACH) {
		return DDI_FAILURE;
	}

	instance = ddi_get_instance(dip);
	if ((ndp = NVDRAM_LOOKUP(instance)) == NULL) {
		return DDI_FAILURE;
	}

	NVDRAM_LOCK(ndp);
	if (NVDRAM_IS_INUSE(ndp)) {
		/* Another thread uses this device. */
		NVDRAM_UNLOCK(ndp);
		return DDI_FAILURE;
	}
	NVDRAM_UNLOCK(ndp);

	nvdram_free(ndp);

	return DDI_SUCCESS;
}

/*
 * static int
 * nvdram_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg,
 *		   void **result)
 *	Get driver information.
 */
static int
nvdram_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg, void **result)
{
	int		ret = DDI_SUCCESS;
	nvdram_t	*ndp;
	minor_t		minor = getminor((dev_t)arg);

	if (cmd == DDI_INFO_DEVT2DEVINFO) {
		if ((ndp = NVDRAM_LOOKUP(minor)) != NULL) {
			*result = ndp->nd_dip;
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

	return ret;
}

/*
 * static int
 * nvdram_open(dev_t *devp, int flag, int otyp, cred_t *credp)
 *	open(9E) entry point for nvdram driver.
 */
static int
nvdram_open(dev_t *devp, int flag, int otyp, cred_t *credp)
{
	minor_t		minor;
	nvdram_t	*ndp;
	dev_info_t	*dip;
	devplcy_t	*plcy;
	struct ddi_minor_data	*dmdp;
	extern int	secpolicy_dev_open(const cred_t *cr, devplcy_t *plcy,
					   int oflag);

	if (otyp != OTYP_CHR) {
		return EINVAL;
	}

	minor = getminor(*devp);
	if ((ndp = NVDRAM_LOOKUP(minor)) == NULL) {
		return ENXIO;	/* Unconfigured device. */
	}

	/*
	 * Check for privilege descripbed in configuration file.
	 * We can use the first minor node in ddi_minor_data because
	 * nvdram node has only one minor node.
	 */
	dip = ndp->nd_dip;
	dmdp = DEVI(dip)->devi_minor;
	ASSERT(dmdp != NULL);
	if ((plcy = dmdp->ddm_node_priv) != NULL) {
		int	err;

		if ((err = secpolicy_dev_open(credp, plcy, flag)) != 0) {
			return err;
		}
	}

	NVDRAM_LOCK(ndp);
	NVDRAM_SET_INUSE(ndp);
	NVDRAM_UNLOCK(ndp);

	return 0;
}

/*
 * static int
 * nvdram_close(dev_t dev, int flag, int otyp, cred_t *credp)
 *	close(9E) entry point for nvdram driver.
 */
static int
nvdram_close(dev_t dev, int flag, int otyp, cred_t *credp)
{
	minor_t		minor;
	nvdram_t	*ndp;

	minor = getminor(dev);
	if ((ndp = NVDRAM_LOOKUP(minor)) == NULL) {
		return EINVAL;
	}

	NVDRAM_LOCK(ndp);
	NVDRAM_CLR_INUSE(ndp);
	NVDRAM_UNLOCK(ndp);

	return 0;
}

/*
 * static int
 * nvdram_read(dev_t dev, struct uio *uiop, cred_t *credp)
 *	read(9E) entry point for nvdram driver.
 */
static int
nvdram_read(dev_t dev, struct uio *uiop, cred_t *credp)
{
	return nvdram_rdwr(dev, uiop, UIO_READ);
}

/*
 * static int
 * nvdram_write(dev_t dev, struct uio *uiop, cred_t *credp)
 *	write(9E) entry point for nvdram driver.
 */
static int
nvdram_write(dev_t dev, struct uio *uiop, cred_t *credp)
{
	return nvdram_rdwr(dev, uiop, UIO_WRITE);
}

/*
 * static int
 * nvdram_rdwr(dev_t dev, struct uio *uiop, enum uio_rw rw)
 *	Common read/write routine for nvdram driver.
 */
static int
nvdram_rdwr(dev_t dev, struct uio *uiop, enum uio_rw rw)
{
	nvdram_t	*ndp;
	const nvdseg_t	*nsp;
	minor_t		minor;
	offset_t	maxoff;
	ssize_t		oresid;
	int		error = 0;
	uint_t		attr;
	caddr_t		vaddr;
	struct hat	*khat;

	minor = getminor(dev);
	ndp = NVDRAM_LOOKUP(minor);
	ASSERT(ndp != NULL);

	nsp = ndp->nd_seg;
	maxoff = (offset_t)mmu_ptob(nsp->ns_count);
	oresid = uiop->uio_resid;
	vaddr = (caddr_t)ndp->nd_vaddr;
	khat = kas.a_hat;
	attr = (rw == UIO_READ) ? PROT_READ : (PROT_READ|PROT_WRITE);

	while (uiop->uio_resid > 0 && error == 0) {
		struct iovec	*iov = uiop->uio_iov;
		size_t		pageoff, nbytes, maxio;
		pfn_t		pfn;

		if (iov->iov_len == 0) {
			ASSERT(uiop->uio_iovcnt > 0);
			uiop->uio_iov++;
			uiop->uio_iovcnt--;
			continue;
		}

		if (uiop->uio_loffset >= maxoff) {
			if (rw == UIO_READ) {
				/* Return zero to stop read(2) at EOF. */
				error = 0;
			}
			else {
				error = ENOSPC;
			}
			break;
		}

		pageoff = (size_t)(uiop->uio_loffset & PAGEOFFSET);
		pfn = nsp->ns_base + (pfn_t)mmu_btop(uiop->uio_loffset);
		ASSERT(pfn < nsp->ns_base + nsp->ns_count);

		nbytes = iov->iov_len;
		maxio = PAGESIZE - pageoff;
		if (nbytes > maxio) {
			nbytes = maxio;
		}

		/* Create temporary mapping. */
		NVDRAM_LOCK(ndp);
		hat_devload(khat, vaddr, PAGESIZE, pfn, attr,
			    HAT_LOAD_NOCONSIST|HAT_LOAD_LOCK);

		error = uiomove(vaddr + pageoff, nbytes, rw, uiop);

		hat_unload(khat, vaddr, PAGESIZE, HAT_UNLOAD_UNLOCK);

		NVDRAM_UNLOCK(ndp);
	}

	return (uiop->uio_resid == oresid) ? error : 0;
}

/*
 * static int
 * nvdram_mmap(dev_t dev, off_t offset, int prot)
 *	mmap(9E) entry point for nvdram driver.
 */
static int
nvdram_mmap(dev_t dev, off_t offset, int prot)
{
	minor_t		minor;
	nvdram_t	*ndp;
	const nvdseg_t	*nsp;
	pgcnt_t		doff;

	if (offset < 0) {
		return -1;
	}

	minor = getminor(dev);
	ndp = NVDRAM_LOOKUP(minor);
	ASSERT(ndp != NULL);

	nsp = ndp->nd_seg;
	doff = (pgcnt_t)mmu_btop(offset);
	if (doff >= nsp->ns_count) {
		return -1;
	}

	return nsp->ns_base + doff;
}

/*
 * static void
 * nvdram_free(nvdram_t *ndp)
 *	Release all resources associated with the specified device.
 */
static void
nvdram_free(nvdram_t *ndp)
{
	int		instance;
	const nvdseg_t	*nsp;
	dev_info_t	*dip;
	dev_t		dev;

	dip = ndp->nd_dip;
	nsp = ndp->nd_seg;
	ASSERT(dip != NULL);
	ASSERT(nsp != NULL);

	/* Remove special file. */
	ddi_remove_minor_node(dip, (char *)nsp->ns_name);

	/* Remove property. */
	instance = ddi_get_instance(dip);
	dev = makedevice(ddi_driver_major(dip), instance);
	(void)ddi_prop_remove(dev, dip, NVDRAM_PROP_SIZE);

	/* Free virtual space. */
	vmem_free(heap_arena, ndp->nd_vaddr, PAGESIZE);

#ifdef	DEBUG
	NVDRAM_LOCK(ndp);
	ASSERT(!NVDRAM_IS_INUSE(ndp));
	NVDRAM_UNLOCK(ndp);
#endif	/* DEBUG */

	mutex_destroy(&ndp->nd_lock);
	ddi_soft_state_free(nvdram_state, instance);
}

/*
 * static void
 * nvdram_mperm_init(void)
 *	Construct minor perm entry list for nvdram driver.
 *
 *	The purpose of this function is to control file owner and group
 *	using xramdev.conf. This function is called from _init() entry,
 *	and initialize minor perm entry list only if it doesn't exist.
 *	So the definition in /etc/minor_perm is prior to xramdev.conf.
 */
static void
nvdram_mperm_init(void)
{
	major_t		major = ddi_name_to_major((char *)NVDRAM_DRIVER_NAME);
	int		unit;
	nvdseg_t	*nsp;
	mperm_t		*mplist, **mpnext;
	struct devnames	*dnp;

	if (major == (major_t)-1) {
		/* No major number is assigned. */
		return;
	}

	dnp = &devnamesp[major];

	/* At first, do a quick check without locking. */
	if (dnp->dn_mperm != NULL || dnp->dn_mperm_wild != NULL) {
		/*
		 * Minor perm entries are already loaded by this function or
		 * /etc/minor_perm.
		 */
		return;
	}

	mpnext = &mplist;
	for (unit = 0; (nsp = nvdram_impl_getseg(unit)) != NULL; unit++) {
		mperm_t	*mp;

		if (nsp->ns_uid == NVDSEG_UID_DEFAULT &&
		    nsp->ns_gid == NVDSEG_GID_DEFAULT &&
		    nsp->ns_mode == NVDSEG_MODE_DEFAULT) {
			/*
			 * No minor perm entry is required because requested
			 * uid and gid are the same as devfs default.
			 */
			continue;
		}
		if (nsp->ns_flags & NVDSEG_PRIVONLY) {
			/* File permissions are always ignored. */
			continue;
		}

		/* Allocate minor perm entry. */
		mp = (mperm_t *)kmem_alloc(sizeof(*mp), KM_SLEEP);
		mp->mp_minorname = i_ddi_strdup((char *)nsp->ns_name,
						KM_SLEEP);
		mp->mp_uid = nsp->ns_uid;
		mp->mp_gid = nsp->ns_gid;
		mp->mp_mode = nsp->ns_mode;
		*mpnext = mp;
		mpnext = &(mp->mp_next);
	}

	if (mpnext == &mplist) {
		/* No minor perm entry is required. */
		return;
	}
	*mpnext = NULL;
	ASSERT(mplist != NULL);

	LOCK_DEV_OPS(&dnp->dn_lock);

	if (dnp->dn_mperm != NULL || dnp->dn_mperm_wild != NULL) {
		mperm_t	*mp, *next;

		/* Lost the race with /etc/minor_perm loading. */
		UNLOCK_DEV_OPS(&dnp->dn_lock);
		for (mp = mplist; mp != NULL; mp = next) {
			size_t	len;

			next = mp->mp_next;
			len = strlen(mp->mp_minorname) + 1;
			kmem_free(mp->mp_minorname, len);
			kmem_free(mp, sizeof(*mp));
		}
		return;
	}

	/* Install minor perm entries. */
	ASSERT(dnp->dn_mperm_clone == NULL);
	dnp->dn_mperm = mplist;
	UNLOCK_DEV_OPS(&dnp->dn_lock);
}

/*
 * int
 * MODDRV_ENTRY_INIT(void)
 *	_init(9E) entry for nvdram driver.
 */
int
MODDRV_ENTRY_INIT(void)
{
	int	error;
	uint_t	count = nvdram_impl_segcount();

	if ((error = ddi_soft_state_init(&nvdram_state, sizeof(nvdram_t),
					 count)) != 0) {
		return error;
	}

	if ((error = mod_install(&modlinkage)) != 0)  {
		ddi_soft_state_fini(&nvdram_state);
	}
	else {
		nvdram_mperm_init();
	}

	return error;
}

#ifndef STATIC_DRIVER
/*
 * int
 * MODDRV_ENTRY_FINI(void)
 *	_fini(9E) entry for nvdram driver.
 */
int
MODDRV_ENTRY_FINI(void)
{
	int	error;

	if ((error = mod_remove(&modlinkage)) != 0)  {
		return error;
	}

	ddi_soft_state_fini(&nvdram_state);

	return 0;
}
#endif /* !STATIC_DRIVER */

/*
 * int
 * MODDRV_ENTRY_INFO(struct modinfo *modinfop)
 *	_info(9E) entry for nvdram driver.
 */
int
MODDRV_ENTRY_INFO(struct modinfo *modinfop)
{
	return mod_info(&modlinkage, modinfop);
}
