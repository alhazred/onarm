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
 * Copyright (c) 2008 NEC Corporation
 * All rights reserved.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/time.h>
#include <sys/pathname.h>
#include <sys/vfs.h>
#include <sys/vfs_opreg.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/statvfs.h>
#include <sys/mount.h>
#include <sys/debug.h>
#include <sys/systm.h>
#include <sys/mntent.h>
#include <fs/fs_subr.h>
#include <vm/page.h>
#include <vm/anon.h>
#include <sys/model.h>
#include <sys/policy.h>
#include <sys/modctl.h>
#include <sys/sdt.h>
#include <sys/fs/xnode.h>
#include <fs/xramfs/xram.h>
#include <sys/file.h>
#include <sys/bootconf.h>

#define XRAM_FS_MIN_NBLOCKS	(2U)

static int xramfsfstype;

static int
mountxramfs(vfs_t *vfsp, vnode_t *dvp, const char *mntpath,
	    size_t mntpathlen, cred_t *cred)
{
	struct xmount *xm = NULL;
	struct xramheader *xh;
	struct xnode *xp;
	int error;
	vnode_t *vp;

	error = VOP_OPEN(&dvp, FREAD, cred, NULL);
	if (error != 0)
		return (error);

	xm = kmem_zalloc(sizeof (struct xmount), KM_NOSLEEP);
	XDTRACE_PROBE2(xmount_alloced, struct xmount **, &xm,
		       struct xmount *, xm);
	XDTRACE_PROBE2(xmount_alloced_2, struct xmount **, &xm,
		       struct xmount *, xm);
	if (xm == NULL) {
		error = ENOMEM;
		goto memory_allocation_error;
	}

	/* xramdev bind */
	error = xramdev_bind(dvp, vfsp, VREAD, &xm->xm_disk);
	if (error != 0) {
		cmn_err(CE_WARN, "xramfs_mountfs: no xramdev %lx",
			dvp->v_rdev);
		goto xramdev_bind_error;
	}

	xm->xm_kasaddr = xm->xm_disk->xd_kasaddr;

	/* fs image check */
#define CHECK_HEADER(cond, errno, msg)			\
	if (!(cond)) {					\
		cmn_err(CE_WARN, "xramfs_mount: " msg);	\
		error = (errno);			\
		goto general_mount_error;		\
	}
	xh = XMOUNT_XRAMHEADER(xm);

	/* is this xramfs image? */
	CHECK_HEADER(xh->xh_magic == XRAM_MAGIC, ENXIO,
		     "bad filesystem image");

	/* version check is must be first */
	CHECK_HEADER(xh->xh_version == XRAMFS_IMGVERSION, ENOTSUP,
		     "incorrect filesystem image version");

	/* check headers */
	CHECK_HEADER(xh->xh_blocks >= XRAM_FS_MIN_NBLOCKS, ENXIO,
		     "image is too small");
	CHECK_HEADER(xh->xh_files != 0, ENXIO,
		     "no node data");
	CHECK_HEADER(xh->xh_off_dir != 0, ENXIO,
		     "no directory entry blocks");
	CHECK_HEADER(xh->xh_files <= XRAM_MNODE_MAX, ENXIO,
		     "too many nodes");
	CHECK_HEADER(xh->xh_blocks <= xm->xm_disk->xd_numpages, EFBIG,
		     "block count is exceeded media size");
	CHECK_HEADER(xh->xh_blocks > xh->xh_off_dir, ENXIO,
		     "offset of directory block is out of bound");
	CHECK_HEADER(xh->xh_blocks > xh->xh_off_symlink, ENXIO,
		     "offset of symbolic target block is out of bound");
	CHECK_HEADER(xh->xh_blocks > xh->xh_off_xfile, ENXIO,
		     "offset of mappable file contents is out of bound");
	CHECK_HEADER(xh->xh_dname_shift <= xh->xh_pageblk_shift, ENXIO,
		     "unit size of directory name region is greater than"
		     " block size");
	CHECK_HEADER(xh->xh_symlink_shift <= xh->xh_pageblk_shift, ENXIO,
		     "unit size of symbolic link target block is greater than"
		     " block size");
	CHECK_HEADER(xh->xh_pfile_shift <= xh->xh_pageblk_shift, ENXIO,
		     "unit size of packed file contents block is greater than"
		     " block size");
	CHECK_HEADER(((xh->xh_pageblk_shift > xh->xh_dname_shift) ?
		      ((xh->xh_blocks - xh->xh_off_dir)
		       << (xh->xh_pageblk_shift - xh->xh_dname_shift)) :
		      ((xh->xh_blocks - xh->xh_off_dir)
		       >> (xh->xh_dname_shift - xh->xh_pageblk_shift)))
		     > xh->xh_off_dnamergn, ENXIO,
		     "offset of directory region is out of bound");

	/* we cannot support if... */
	CHECK_HEADER(xh->xh_pageblk_shift == PAGESHIFT, ENOTSUP,
		     "unsupported page size");
	CHECK_HEADER(xh->xh_off_pfile == 0, ENOTSUP,
		     "packed file is not supported");
	CHECK_HEADER(xh->xh_extheader == 0, ENOTSUP,
		     "extend header is unsupported");
	CHECK_HEADER(xh->xh_dname_shift <= XRAM_SUPPORT_MAX_UNITSHIFT, ENOTSUP,
		     "unit size of directory name region is too big"
		     " to support");
	CHECK_HEADER(xh->xh_symlink_shift <= XRAM_SUPPORT_MAX_UNITSHIFT,
		     ENOTSUP,
		     "unit size of symbolic link target block is too big"
		     " to support");
	CHECK_HEADER(xh->xh_pfile_shift <= XRAM_SUPPORT_MAX_UNITSHIFT, ENOTSUP,
		     "unit size of packed file block is too big"
		     " to support");

	/* set start/end addresses */
	xm->xm_kasaddr_end = (char *)xm->xm_kasaddr +
		(xh->xh_blocks << XRAM_PAGESHIFT(xm));
#define SET_PTR_OR_NULL(name, base, off, shift)			\
	xm->xm_##name = ((xh->xh_off_##off != 0)		\
			? (((caddr_t)(xm->xm_##base))		\
			   + (xh->xh_off_##off << (shift)))	\
			: NULL)
	SET_PTR_OR_NULL(dir, kasaddr, dir, XRAM_PAGESHIFT(xm));
	SET_PTR_OR_NULL(dname, dir, dnamergn, xh->xh_dname_shift);
	SET_PTR_OR_NULL(symlink, kasaddr, symlink, XRAM_PAGESHIFT(xm));
	SET_PTR_OR_NULL(map, kasaddr, xfile, XRAM_PAGESHIFT(xm));

	/*
	 * Set but don't bother entering the mutex
	 *  (xmount not on mount list yet)
	 */
	xram_xvt_init(&xm->xm_xvt);
	xm->xm_vfsp = vfsp;
	xm->xm_devvp = dvp;

	vfsp->vfs_data = (caddr_t)xm;
	vfsp->vfs_fstype = xramfsfstype;
	vfsp->vfs_dev = dvp->v_rdev;
	vfsp->vfs_bsize = XRAM_PAGESIZE(xm);
	vfsp->vfs_flag |= VFS_NOTRUNC;
	vfs_make_fsid(&vfsp->vfs_fsid, dvp->v_rdev, xramfsfstype);

	error = xnode_lookup(xm, XRAMMNODE_XNO_ROOT, &xp);
	if (error != 0) {
		cmn_err(CE_WARN, "xramfs_mount: invalid root node");
		xram_xvt_destroy(&xm->xm_xvt);
		goto general_mount_error;
	}

	XDTRACE_PROBE1(mountfs, struct xnode *, xp);
	XNTOV(xp)->v_flag |= VROOT;
	xm->xm_rootnode = xp;

	xm->xm_mntpath = kmem_alloc(mntpathlen + 1, KM_SLEEP);
	(void) strncpy(xm->xm_mntpath, mntpath, mntpathlen + 1);

	return (0);

general_mount_error:
	xramdev_unbind(xm->xm_disk, vfsp);
xramdev_bind_error:
	kmem_free(xm, sizeof(struct xmount));
memory_allocation_error:
	VOP_CLOSE(dvp, FREAD, 1, (offset_t)0, cred, NULL);
	return (error);
}

static int
xram_mount(vfs_t *vfsp, vnode_t *mvp, struct mounta *uap, cred_t *cred)
{
	pathname_t dpn;
	int error;
	vnode_t *dvp;
	int fromspace;
	int needrele = 0;

	if ((error = secpolicy_fs_mount(cred, mvp, vfsp)) != 0)
		return (error);

	/* hsfs's way. deny if read-write mount */
	if (!(uap->flags & MS_RDONLY))
		return (EROFS);

	if (mvp->v_type != VDIR)
		return (ENOTDIR);

	mutex_enter(&mvp->v_lock);
	if ((uap->flags & MS_OVERLAY) == 0 &&
	    (mvp->v_count != 1 || (mvp->v_flag & VROOT))) {
		mutex_exit(&mvp->v_lock);
		return (EBUSY);
	}
	mutex_exit(&mvp->v_lock);

	/* now look for options we understand... */

	/* xramfs doesn't support read-write mounts */
	if (vfs_optionisset(vfsp, MNTOPT_RW, NULL) ||
	    vfs_optionisset(vfsp, MNTOPT_RQ, NULL))
		return (EINVAL);

	fromspace = uap->flags & MS_SYSSPACE ? UIO_SYSSPACE : UIO_USERSPACE;

	/* get (and check whether exist or not) properly name of mount point */
	if (error = pn_get(uap->dir, fromspace, &dpn))
		return (error);

	/* get vnode of special device */
	error = lookupname(uap->spec, fromspace, FOLLOW, NULL, &dvp);
	if (error != 0)
		goto lookup_error;

	needrele = 1;
	if (dvp->v_type != VCHR && dvp->v_type != VBLK) {
		error = EINVAL;
		goto lookup_error;
	}

	if ((error = VOP_ACCESS(dvp, VREAD, 0, cred, NULL)) != 0 ||
	    (error = secpolicy_spec_open(cred, dvp, FREAD)) != 0)
		goto lookup_error;

	error = mountxramfs(vfsp, dvp, dpn.pn_path, dpn.pn_pathlen, cred);
	if (error != 0)
		goto lookup_error;

	pn_free(&dpn);
	return (0);

lookup_error:
	if (needrele)
		VN_RELE(dvp);
	pn_free(&dpn);
	return (error);
}

static int
xram_mountroot(vfs_t *vfsp, enum whymountroot reason)
{
	static int is_root_mounted = 0;
	int error;
	dev_t rdev;
	vnode_t *dvp;

	switch (reason) {
	case ROOT_INIT:
		rdev = getrootdev();
		if (rdev == NODEV)
			return (ENODEV);

		vfsp->vfs_dev = rdev;
		vfsp->vfs_flag |= VFS_RDONLY;

		break;
	case ROOT_REMOUNT:
		cmn_err(CE_NOTE, "xram_mountroot: ignored remount request");
		return (0);
	case ROOT_UNMOUNT:
		dvp = ((struct xmount *)vfsp->vfs_data)->xm_devvp;
		VOP_CLOSE(dvp, FREAD, 1, (offset_t)0, CRED(), NULL);
		return (0);
	default:
		return (ENOTSUP);
	}

	/* NOTE: from here, reason == ROOT_INIT */

	if (is_root_mounted != 0)
		return (EBUSY);
	is_root_mounted++;

	error = vfs_lock(vfsp);
	if (error != 0) {
		cmn_err(CE_WARN, "xram_mountroot: failed to lock VFS");
		return (error);
	}
	dvp = makespecvp(rdev, VBLK);
	error = mountxramfs(vfsp, dvp, "/", 1, CRED());
	if (error != 0) {
		if (rootvp) {
			VN_RELE(rootvp);
			rootvp = NULL;
		}
		VN_RELE(dvp);
		vfs_unlock(vfsp);
		return (error);
	}

	rootvp = dvp;

	/*
	 * vfs_add() is needed only if reason == ROOT_INIT;
	 * read-write support checks vfsp->vfs_flag & VFS_RDONLY
	 * (don't set MS_RDONLY if read-write mode, use 0)
	 */
	vfs_add((vnode_t *)NULL, vfsp, MS_RDONLY);
	vfs_unlock(vfsp);
	return (0);
}


static int
xram_unmount(vfs_t *vfsp, int flag, cred_t *cred)
{
	struct xmount *xm = VFSTOXM(vfsp);
	struct xnode *xp;
	vnode_t *vp;
	int error;

	XRAMDBG(("xram_unmount: enter\n"));
	if ((error = secpolicy_fs_unmount(cred, vfsp)) != 0)
		return (error);

	/*
	 * forced unmount is not supported by this file system
	 *  and thus, ENOTSUP, is being returned
	 */
	if (flag & MS_FORCE)
		return (ENOTSUP);

	XRAMDBG(("xram_unmount: going unmount\n"));
	/* check vnode (except rootnode) is registered */
	xnode_lock_all(xm);
	if (!xnode_isalone(xm)) {
		xnode_unlock_all(xm);
		return (EBUSY);
	}

	/* check root node. */
	xp = xm->xm_rootnode;
	vp = XNTOV(xp);
	if (vp->v_count > 1) {
		xnode_unlock_all(xm);
		return (EBUSY);
	}

	xnode_unlock_all(xm);
	VN_RELE(vp);

	xram_xvt_destroy(&xm->xm_xvt);

	ASSERT(xm->xm_mntpath);
	kmem_free(xm->xm_mntpath, strlen(xm->xm_mntpath) + 1);

	xramdev_unbind(xm->xm_disk, vfsp);
	(void)VOP_CLOSE(xm->xm_devvp, FREAD, 1, (offset_t)0, cred, NULL);
	VN_RELE(xm->xm_devvp);
	XRAMDBG(("xram_unmount: xramdisk unbound\n"));

	kmem_free(xm, sizeof (struct xmount));

	return (0);
}

/*
 * return root xnode for given vnode
 */
static int
xram_root(vfs_t *vfsp, vnode_t **vpp)
{
	struct xmount *xm = VFSTOXM(vfsp);
	struct xnode *xp = xm->xm_rootnode;
	vnode_t *vp;

	ASSERT(xp);

	vp = XNTOV(xp);
	VN_HOLD(vp);
	*vpp = vp;

	return (0);
}

static int
xram_statvfs(vfs_t *vfsp, statvfs64_t *sbp)
{
	struct xmount *xm = VFSTOXM(vfsp);
	dev32_t d32;
	struct xramheader *xh;

	xh = XMOUNT_XRAMHEADER(xm);
	sbp->f_frsize = sbp->f_bsize = XRAM_PAGESIZE(xm);

	/* no free space. */
	sbp->f_bfree = 0;
	sbp->f_blocks = xh->xh_blocks;
	sbp->f_bavail = 0;

	sbp->f_ffree = 0;
	sbp->f_files = xh->xh_files;
	sbp->f_favail = 0;
	(void) cmpldev(&d32, vfsp->vfs_dev);
	sbp->f_fsid = d32;
	(void) strcpy(sbp->f_basetype, vfssw[xramfsfstype].vsw_name);
	(void) strncpy(sbp->f_fstr, xm->xm_mntpath, sizeof (sbp->f_fstr));

	/* ensure null termination */
	sbp->f_fstr[sizeof (sbp->f_fstr) - 1] = '\0';
	sbp->f_flag = vf_to_stf(vfsp->vfs_flag);
	sbp->f_namemax = MAXNAMELEN - 1;
	return (0);
}

/*
 * initialize global xramfs locks and such
 * called when loading xramfs module
 */
static int
xramfsinit(int fstype, char *name)
{
	static const fs_operation_def_t xram_vfsops_template[] = {
		VFSNAME_MOUNT, xram_mount,
		VFSNAME_MOUNTROOT, xram_mountroot,
		VFSNAME_UNMOUNT, xram_unmount,
		VFSNAME_ROOT, xram_root,
		VFSNAME_STATVFS, xram_statvfs,
		NULL, NULL
	};
	int error;

	xnode_create_cache();

	xramfsfstype = fstype;
	ASSERT(xramfsfstype != 0);

	error = vfs_setfsops(fstype, xram_vfsops_template, NULL);
	if (error != 0) {
		xnode_destroy_cache();
		cmn_err(CE_WARN, "xramfsinit: bad vfs ops template");
		return (error);
	}

	error = vn_make_ops(name, xram_vnodeops_template, &xram_vnodeops);
	if (error != 0) {
		xnode_destroy_cache();
		(void) vfs_freevfsops_by_type(fstype);
		cmn_err(CE_WARN, "xramfsinit: bad vnode ops template");
		return (error);
	}

	return (0);
}


/* Loadable module wrapper */

static vfsdef_t vfw = {
	VFSDEF_VERSION,
	"xramfs",
	xramfsinit,
	VSW_STATS,
	NULL
};

/* Module linkage information */
static struct modlfs modlfs = {
	&mod_fsops, "filesystem for xramfs", &vfw
};

static struct modlinkage modlinkage = {
	MODREV_1, &modlfs, NULL
};

int
MODDRV_ENTRY_INIT()
{
	return (mod_install(&modlinkage));
}

#ifndef STATIC_DRIVER
int
MODDRV_ENTRY_FINI()
{
	int error;

	error = mod_remove(&modlinkage);
	if (error)
		return (error);
	/* destroy objects created in xramfsinit() */
	xnode_destroy_cache();
	/* Tear down the operations vectors */
	(void) vfs_freevfsops_by_type(xramfsfstype);
	vn_freevnodeops(xram_vnodeops);

	return (0);
}
#endif /* !defined(STATIC_DRIVER) */

int
MODDRV_ENTRY_INFO(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
