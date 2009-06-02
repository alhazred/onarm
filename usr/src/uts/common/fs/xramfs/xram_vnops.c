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
#include <sys/t_lock.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/user.h>
#include <sys/time.h>
#include <sys/vfs.h>
#include <sys/vfs_opreg.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/kmem.h>
#include <sys/uio.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/cred.h>
#include <sys/dirent.h>
#include <sys/pathname.h>
#include <sys/vmsystm.h>
#include <sys/sdt.h>
#include <sys/fs/xnode.h>
#include <fs/xramfs/xram.h>
#include <sys/xramdev.h>
#include <sys/mman.h>
#include <vm/hat.h>
#include <vm/seg_vn.h>
#include <vm/seg_map.h>
#include <vm/seg.h>
#include <vm/anon.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/pvn.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/swap.h>
#include <sys/buf.h>
#include <sys/vm.h>
#include <sys/vtrace.h>
#include <sys/policy.h>
#include <fs/fs_subr.h>

/* ARGSUSED */
static int
xram_open(vnode_t **vpp, int flag, cred_t *cred, caller_context_t *ct)
{
	/*
	 * swapon to a xramfs file is not supported so access
	 * is denied on open if VISSWAP is set
	 */
	if ((*vpp)->v_flag & VISSWAP)
		return (EINVAL);
	return (0);
}

/* ARGSUSED */
static int
xram_close(vnode_t *vp, int flag, int count, offset_t offset, cred_t *cred,
	   caller_context_t *ct)
{
	cleanlocks(vp, ttoproc(curthread)->p_pid, 0);
	cleanshares(vp, ttoproc(curthread)->p_pid);
	return (0);
}

/* rdxram does the real work of read requests for xramfs */
/* ARGSUSED */
static int
rdxram(struct xmount *xm, struct xnode *xp, uio_t *uio, caller_context_t *ct)
{
	size_t off, bytes;		/* bytes to uiomove */
	vnode_t *vp;
	int error;
	caddr_t src;
	long oresid = uio->uio_resid;

	vp = XNTOV(xp);

	TRACE_1(TR_FAC_XRAMFS, TR_XRAMFS_RWXRAM_START,
		"xram_rdxram_start:vp 0x%p", vp);

	if (MANDLOCK(vp, XNODE_MODE(xp))) {
		/* xram_getattr ends up being called by chklock */
		error = chklock(vp, FREAD, uio->uio_loffset, uio->uio_resid,
				uio->uio_fmode, ct);
		if (error != 0) {
			TRACE_2(TR_FAC_XRAMFS, TR_XRAMFS_RWXRAM_END,
				"xram_rdxram_end:vp 0x%p error %d", vp, error);
			return (error);
		}
	}

	if (uio->uio_loffset > XRAM_MAXOFFSET_T) {
		TRACE_2(TR_FAC_XRAMFS, TR_XRAMFS_RWXRAM_END,
			"xram_rdxram_end:vp 0x%p error %d (overflow)", vp, 0);
		return (0);
	}
	if (uio->uio_loffset < 0)
		return (EINVAL);
	if (uio->uio_resid == 0) {
		TRACE_2(TR_FAC_XRAMFS, TR_XRAMFS_RWXRAM_END,
			"xram_rdxram_end:vp 0x%p error %d", vp, 0);
		return (0);
	}

	if (uio->uio_loffset >= xp->xn_size) {
		/* At the end of file. */
		return (0);
	}

	/*
	 * mappable file contents is not exist or empty
	 *  but file which has content is exist, this is image file error
	 */
	if (xm->xm_map == NULL)
		return (ENXIO);

	off = (xp->xn_start << XRAM_PAGESHIFT(xm)) + uio->uio_loffset;
	bytes = MIN((xp->xn_size - uio->uio_loffset), uio->uio_resid);
	src = xm->xm_map + off;

	/* check copying region is in xramdisk/xramdev region */
	if (off > XRAM_SIZE_PTR2PTR(xm->xm_map, xm->xm_kasaddr_end)
	    || bytes > XRAM_SIZE_PTR2PTR(src, xm->xm_kasaddr_end))
		return (ENXIO);

	error = uiomove(src, bytes, UIO_READ, uio);

	/* NOTE: no atime update for ROFS. */
	/* gethrestime(&xp->xn_atime); */

	/*
	 * If we've already done a partial read, terminate
	 * the read but return no error
	 */
	if (oresid != uio->uio_resid)
		error = 0;

	TRACE_2(TR_FAC_XRAMFS, TR_XRAMFS_RWXRAM_END,
		"xram_rdxram_end:vp %x error %d", vp, error);
	return (error);
}

/* ARGSUSED */
static int
xram_read(vnode_t *vp, uio_t *uiop, int ioflag, cred_t *cred,
	  caller_context_t *ct)
{
	struct xnode *xp = VTOXN(vp);
	struct xmount *xm = VTOXM(vp);
	int error;

	XRAMDBG(("xram_read: vp:off=0x%p:%lld, len=%" PRIdMAX "\n", vp,
		 uiop->uio_loffset, (intmax_t)uiop->uio_resid));

	/* We don't currently support reading non-regular files */
	if (vp->v_type == VDIR)
		return (EISDIR);
	if (vp->v_type != VREG)
		return (EINVAL);
	error = rdxram(xm, xp, uiop, ct);
	XRAMDBG(("xram_read: resid=%" PRIdMAX ", error=%d\n",
		 (intmax_t)uiop->uio_resid, error));

	return (error);
}

/* ARGSUSED */
static int
xram_ioctl(vnode_t *vp, int cmd, intptr_t arg, int flag, cred_t *cred,
	   int *rvalp, caller_context_t *ct)
{
	return (ENOTTY);
}

/* ARGSUSED */
static int
xram_getattr(vnode_t *vp, vattr_t *vap, int flags, cred_t *cred,
	     caller_context_t *ct)
{
	struct xmount *xm = VFSTOXM(vp->v_vfsp);
	struct xnode *xp = VTOXN(vp);
	int pagesize;

	if (vap->va_mask == AT_SIZE) {
		/*
		 *  like ufs:
		 *  caller requests only size.
		 */
		vap->va_size = xp->xn_size;
		return (0);
	}

	pagesize = XRAM_PAGESIZE(xm);

	vap->va_type = vp->v_type;
	vap->va_mode = XNODE_MODE(xp);
	vap->va_uid = xp->xn_uid;
	vap->va_gid = xp->xn_gid;
	vap->va_fsid = xm->xm_vfsp->vfs_dev;
	vap->va_nodeid = XNODE_NODEID(xm, xp);
	vap->va_nlink = (xp->xn_nlink & XRAM_LINK_MASK);
	vap->va_size = xp->xn_size;
	vap->va_atime.tv_sec = xp->xn_time_sec;
	vap->va_atime.tv_nsec = xp->xn_time_nsec;
	vap->va_mtime.tv_sec = xp->xn_time_sec;
	vap->va_mtime.tv_nsec = xp->xn_time_nsec;
	vap->va_ctime.tv_sec = xp->xn_time_sec;
	vap->va_ctime.tv_nsec = xp->xn_time_nsec;
	vap->va_blksize = pagesize;
	if (vp->v_type == VCHR || vp->v_type == VBLK)
		vap->va_rdev = makedevice(xp->xn_rdev_major,
					  xp->xn_rdev_minor);
	else
		vap->va_rdev = NODEV;
	vap->va_seq = 0;
	vap->va_nblocks = (fsblkcnt64_t)lbtodb(
		(fsblkcnt64_t)XRAM_BYTE2UNIT_ROUNDUP(
			xp->xn_size, XRAM_PAGESHIFT(xm))
		<< XRAM_PAGESHIFT(xm));
	return (0);
}

/* ARGSUSED */
static int
xram_setattr(vnode_t *vp, vattr_t *vap, int flags, cred_t *cred,
	     caller_context_t *ct)
{
	/*
	 * if node is FIFO or a device node, and requests
	 * truncation (size -> 0), we ignore it
	 */
	if ((vp->v_type == VFIFO || vp->v_type == VBLK || vp->v_type == VCHR)
	    && vap->va_mask == AT_SIZE && vap->va_size == 0 &&
	    flags == 0)
		return (0);
	return (EROFS);
}


/* ARGSUSED */
static int
xram_access(vnode_t *vp, int mode, int flags, cred_t *cred,
	    caller_context_t *ct)
{
	struct xmount *xm = VTOXM(vp);
	struct xnode *xp = VTOXN(vp);
	int error;

	XRAMDBG(("xram_access: for xno %" PRIuMAX " (type/mode 0%06" PRIo16
		 ") reqmode 0%06o flag 0x%x\n",
		 (uintmax_t)XNODE_XNO(xm, xp), xp->xn_typemode, mode, flags));
	/*
	 * this is read only file system, so we cannot accept writing for
	 * regular file, directory, symbolic link.
	 * (pipe, device nodes are OK even if read-only file system)
	 */
	if ((mode & VWRITE) &&
	    (vp->v_type == VREG || vp->v_type == VDIR
	     || vp->v_type  == VLNK)) {
		XRAMDBG(("xram_access: for xno %" PRIuMAX ": writing request "
			 "for dir / link / regular file, blocked\n",
			 (uintmax_t)XNODE_XNO(xm, xp)));
		return (EROFS);
	}
	error = xram_xaccess(xm, xp, mode, cred);
	return (error);
}

/* ARGSUSED */
static int
xram_lookup(vnode_t *dvp, char *nm, vnode_t **vpp, pathname_t *pnp,
	    int flags, vnode_t *rdir, cred_t *cred, caller_context_t *ct,
	    int *direntflags, pathname_t *realpnp)
{
	struct xmount *xm = VTOXM(dvp);
	struct xnode *dxp = VTOXN(dvp);
	struct xnode *nxp = NULL;
	int error;

	XRAMDBG(("xram_lookup: dir=0x%p, nm=\"%s\"\n", dvp, nm));

	/* Null component name is a synonym for directory being searched */
	if (*nm == '\0') {
		XDTRACE_PROBE1(directory_self, struct xnode *, dxp);

		VN_HOLD(dvp);
		*vpp = dvp;
		return (0);
	}
	ASSERT(dxp);

	error = xram_dirlookup(xm, dxp, nm, &nxp, cred);

	if (error != 0)
		return (error);

	ASSERT(nxp);
	*vpp = XNTOV(nxp);

	/* If vnode is a device return special vnode instead */
	if (IS_DEVVP(*vpp)) {
		vnode_t *newvp;

		XDTRACE_PROBE1(sepcvp, struct xnode *, *vpp);

		newvp = specvp(*vpp, (*vpp)->v_rdev, (*vpp)->v_type,
			       cred);
		VN_RELE(*vpp);
		if (newvp == NULL)
			return (ENOSYS);
		*vpp = newvp;
	}

	XRAMDBG(("xram_lookup: 0x%p/%s returns *vpp=0x%p\n", dvp, nm, *vpp));

	return (0);
}


/* ARGSUSED */
static int
xram_create(vnode_t *dvp, char *nm, vattr_t *vap, enum vcexcl exclusive,
	    int mode, vnode_t **vpp, cred_t *cred, int flag,
	    caller_context_t *ct, vsecattr_t *vsecp)
{
	struct xmount *xm = VTOXM(dvp);
	struct xnode *xp = VTOXN(dvp);
	struct xnode *nxp = NULL;
	int error;

	XRAMDBG(("xram_create: dir=0x%p, nm=\"%s\"\n", dvp, nm));

	/* Null component name is a synonym for directory being searched */
	if (*nm == '\0') {
		XDTRACE_PROBE1(directory_self, struct xnode *, xp);
		VN_HOLD(dvp);
		*vpp = dvp;
	} else {
		ASSERT(xp);

		error = xram_dirlookup(xm, xp, nm, &nxp, cred);
		if (error == ENOENT)
			return (EROFS);
		else if (error != 0)
			return (error);
		ASSERT(nxp);
		*vpp = XNTOV(nxp);
	}

	if (exclusive == EXCL)
		error = EEXIST;
	else
		error = xram_xaccess(xm, VTOXN(*vpp), mode, cred);
	/*
	 * check for truncation for regular files
	 * (FIFO and devices nodes are size == 0, so ignore truncation)
	 */
	if (error == 0 &&
	    (*vpp)->v_type == VREG && (vap->va_mask & AT_SIZE) &&
	    vap->va_size == 0 && nxp->xn_size != 0) {
		/* Cannot truncate file for ReadOnly FS. */
		error = EROFS;
	}

	/* any error until here, 'create' failed */
	if (error != 0) {
		VN_RELE(*vpp);
		return (error);
	}

	/* If vnode is a device return special vnode instead */
	if (IS_DEVVP(*vpp)) {
		vnode_t *newvp;

		XDTRACE_PROBE1(sepcvp, struct xnode *, *vpp);

		newvp = specvp(*vpp, (*vpp)->v_rdev, (*vpp)->v_type,
			       cred);
		VN_RELE(*vpp);
		if (newvp == NULL)
			return (ENOSYS);
		*vpp = newvp;
	}

	/* Returns no-error with the vnode found. */
	XRAMDBG(("xram_create: 0x%p/%s returns *vpp=0x%p\n", dvp, nm, *vpp));
	return (0);
}


/* ARGSUSED */
static int
xram_readdir(vnode_t *vp, uio_t *uiop, cred_t *cred, int *eofp,
	     caller_context_t *ct, int flags)
{
	struct xmount *xm = VTOXM(vp);
	struct xnode *dir = VTOXN(vp);

	if (uiop->uio_loffset >= XRAM_MAXOFFSET_T) {
		if (eofp)
			*eofp = 1;
		return (0);
	}

	if (uiop->uio_iovcnt != 1)
		return (EINVAL);

	if (vp->v_type != VDIR)
		return (ENOTDIR);

	return (xram_listdir(xm, dir, uiop, eofp));
}

/* ARGSUSED */
static int
xram_readlink(vnode_t *vp, uio_t *uiop, cred_t *cred, caller_context_t *ct)
{
	struct xnode *xp = VTOXN(vp);
	struct xmount *xm = VTOXM(vp);
	size_t off, bytes;
	caddr_t src;

	if (vp->v_type != VLNK)
		return (EINVAL);

	if (xp->xn_size == 0) {
		/* This is symlink, but no content */
		return (0);
	}
	/*
	 * symlink target blocks is not exist or empty
	 *  but symlink which has content is exist
	 */
	if (xm->xm_symlink == NULL)
		return (ENXIO);

	bytes = MIN(xp->xn_size, uiop->uio_resid);
	off = (xp->xn_start << XMOUNT_XRAMHEADER(xm)->xh_symlink_shift);
	src = xm->xm_symlink + off;

	/* check start and end point is in xramdisk/xramdev region */
	if (off > XRAM_SIZE_PTR2PTR(xm->xm_symlink, xm->xm_kasaddr_end)
	    || bytes > XRAM_SIZE_PTR2PTR(src, xm->xm_kasaddr_end))
		return (ENXIO);

	return (uiomove(src, bytes, UIO_READ, uiop));
}

/* ARGSUSED */
static int
xram_fsync(vnode_t *vp, int syncflag, cred_t *cred, caller_context_t *ct)
{
	return (0);
}

/* ARGSUSED */
static void
xram_inactive(vnode_t *vp, cred_t *cred, caller_context_t *ct)
{
	struct xnode *xp = VTOXN(vp);
	struct xmount *xm = VFSTOXM(vp->v_vfsp);
	kmutex_t *kmtx = xnode_getmutex(xm, XNODE_XNO(xm, xp));

	XRAMDBG(("xram_inactive: vp 0x%p\n", vp));

	while (1) {
		/*
		 * check specified vnode is not used, and
		 * dispose all pages if its vnode has them
		 */

		mutex_enter(kmtx);
		mutex_enter(&vp->v_lock);
		ASSERT(vp->v_count >= 1);

		/* If we don't have the last hold, just drop our hold */
		if (vp->v_count > 1) {
			vp->v_count--;
			mutex_exit(&vp->v_lock);
			mutex_exit(kmtx);
			return;
		}

		/* OK, all pages are released and nothing referred */
		if (vp->v_pages == NULL) {
			mutex_exit(&vp->v_lock);
			break;
		}
		/* Release all pages bound to this vnode */
		mutex_exit(&vp->v_lock);
		mutex_exit(kmtx);
		free_vp_pages(vp, (u_offset_t)0, (size_t)xp->xn_size);
	}
	/* remove from xnode list */
	xnode_inactive(xm, xp);
	mutex_exit(kmtx);
	xnode_free(xm, xp);

	XRAMDBG(("xram_inactive: vp 0x%p (xp==0x%p) released\n", vp, xp));
}

/*
 * Called from pvn_getpages or xram_getpage to get a particular page.
 */
/* ARGSUSED */
static int
xram_getapage(vnode_t *vp, u_offset_t off, size_t len, uint_t *protp,
	      page_t *pl[], size_t plsz, struct seg *seg, caddr_t addr,
	      enum seg_rw rw, cred_t *cred)
{
	struct xnode *xp = VTOXN(vp);
	struct xmount *xm = VTOXM(vp);
	xramdev_t *xrdp = xm->xm_disk;
	pfn_t pgno;
	page_t *pp;
	int error;

	XRAMDBG(("xram_getapage: vp:off=0x%p:%" PRIuMAX " (create=%s), "
		 "pl=0x%p\n", vp, (uintmax_t)off,
		 rw == S_CREATE ? "yes" : "no", pl));

	if (protp != NULL)
		*protp = PROT_READ|PROT_EXEC|PROT_USER;

	/* fetch a page or return a pointer (if already fetched) */
	do {
		if (pp = page_lookup(vp, off,
				     rw == S_CREATE ? SE_EXCL : SE_SHARED)) {
			/* page was already fetched. */
			XRAMDBG(("xram_getapage: FOUND page 0x%p for pfn 0x%"
				 PRIxMAX "\n", pp,
				 (uintmax_t)(XNODE_MAPOFFS_TO_PFN(
						     xm, xp, off))));
			if (pl != NULL) {
				pl[0] = pp;
				pl[1] = NULL;
			} else
				page_unlock(pp);
			error = 0;
			break;
		}

		/* Fetch a page in vnode from xramdev. */
		pgno = XNODE_MAPOFFS_TO_PFN(xm, xp, off);
		error = XRAMDEV_GETAPAGE(xrdp, pgno, vp, off, &pp);
		XRAMDBG(("xram_getapage: got pfn=0x%" PRIxMAX ", error=%d, "
			 "pp=0x%p\n", (uintmax_t)pgno, error, pp));
		/*
		 * xramdev_getapage may be failed with error code EAGAIN
		 * when another threads already fetched the specified page.
		 * so, we retry page_lookup to get a address (in KAS)
		 * for target page.
		 */
		if (error == 0) {
			if (pl != NULL)
				pvn_plist_init(
					pp, pl, plsz, off, PAGESIZE, rw);
			else
				pvn_io_done(pp);
			break;
		}
	} while (error == EAGAIN);

	return (error);
}

/*
 * Return all the pages from [off..off+len] in given file
 */
static int
xram_getpage(vnode_t *vp, offset_t off, size_t len, uint_t *protp,
	     page_t *pl[], size_t plsz, struct seg *seg, caddr_t addr,
	     enum seg_rw rw, cred_t *cred, caller_context_t *ct)
{
	int error = 0;
	struct xnode *xp = VTOXN(vp);

	if (off + len > xp->xn_size + PAGEOFFSET)
		return (EFAULT);

	if (len <= PAGESIZE)
		error = xram_getapage(vp, (u_offset_t)off, len, protp, pl,
				      plsz, seg, addr, rw, cred);
	else
		error = pvn_getpages(xram_getapage, vp, (u_offset_t)off, len,
				     protp, pl, plsz, seg, addr, rw, cred);

	return (error);
}

/* ARGSUSED */
int
xram_putpage(register vnode_t *vp, offset_t off, size_t len, int flags,
	     cred_t *cred, caller_context_t *ct)
{
	/* nothing to be done */
	return (0);
}

/* ARGSUSED */
static int
xram_dispose(vnode_t *vp, page_t *pp, int flag, int dn, cred_t *cred,
	     caller_context_t *ct)
{
	struct xnode *xp = VTOXN(vp);
	struct xmount *xm = VTOXM(vp);
	xramdev_t *xrdp = xm->xm_disk;
	pfn_t pfn;

	pfn = XNODE_MAPOFFS_TO_PFN(xm, xp, pp->p_offset);
	XRAMDBG(("xram_dispose: disposing pfn %u\n", (unsigned int)pfn));
	XRAMDEV_PUTAPAGE(xrdp, pfn, pp);
	return (0);
}

static int
xram_map(vnode_t *vp, offset_t off, struct as *as, caddr_t *addrp,
	 size_t len, uchar_t prot, uchar_t maxprot, uint_t flags, cred_t *cred,
	caller_context_t *ct)
{
	segvn_crargs_t vn_a;
	struct xnode *xp = VTOXN(vp);
	int error;

	if (vp->v_flag & VNOMAP)
		return (ENOSYS);

	if (off < 0 || len > XRAM_MAXOFFSET_T - off)
		return (ENXIO);

	if (vp->v_type != VREG)
		return (ENODEV);

	/* Don't allow mapping to locked file */
	if (vn_has_mandatory_locks(vp, XNODE_MODE(xp))) {
		return (EAGAIN);
	}

	as_rangelock(as);
	if ((flags & MAP_FIXED) == 0) {
		map_addr(addrp, len, (offset_t)off, 1, flags);
		if (*addrp == NULL) {
			as_rangeunlock(as);
			return (ENOMEM);
		}
	} else {
		/* User specified address - blow away any previous mappings */
		(void) as_unmap(as, *addrp, len);
	}

	vn_a.vp = vp;
	vn_a.offset = (u_offset_t)off;
	vn_a.type = flags & MAP_TYPE;
	vn_a.prot = prot;
	vn_a.maxprot = maxprot;
	vn_a.flags = flags & ~MAP_TYPE;
	vn_a.cred = cred;
	vn_a.amp = NULL;
	vn_a.szc = 0;
	vn_a.lgrp_mem_policy_flags = 0;

	error = as_map(as, *addrp, len, segvn_create, &vn_a);
	as_rangeunlock(as);
	return (error);
}

/*
 * xram_addmap and xram_delmap can't be called since the vp
 * maintained in the segvn mapping is NULL.
 */
/* ARGSUSED */
static int
xram_addmap(vnode_t *vp, offset_t off, struct as *as, caddr_t addr, size_t len,
	    uchar_t prot, uchar_t maxprot, uint_t flags, cred_t *cred,
	    caller_context_t *ct)
{
	return (0);
}

/* ARGSUSED */
static int
xram_delmap(vnode_t *vp, offset_t off, struct as *as, caddr_t addr, size_t len,
	    uint_t prot, uint_t maxprot, uint_t flags, cred_t *cred,
	    caller_context_t *ct)
{
	return (0);
}

/* ARGSUSED */
static int
xram_seek(vnode_t *vp, offset_t ooff, offset_t *noffp, caller_context_t *ct)
{
	return ((*noffp < 0 || *noffp > XRAM_MAXOFFSET_T) ? EINVAL : 0);
}

vnodeops_t *xram_vnodeops;

const fs_operation_def_t xram_vnodeops_template[] = {
	VOPNAME_OPEN, xram_open,
	VOPNAME_CLOSE, xram_close,
	VOPNAME_READ, xram_read,
	VOPNAME_IOCTL, xram_ioctl,
	VOPNAME_GETATTR, xram_getattr,
	VOPNAME_SETATTR, xram_setattr,
	VOPNAME_ACCESS, xram_access,
	VOPNAME_LOOKUP, xram_lookup,
	VOPNAME_CREATE, xram_create,
	VOPNAME_READDIR, xram_readdir,
	VOPNAME_READLINK, xram_readlink,
	VOPNAME_FSYNC, xram_fsync,
	VOPNAME_INACTIVE, (fs_generic_func_p) xram_inactive,
	VOPNAME_GETPAGE, xram_getpage,
	VOPNAME_PUTPAGE, xram_putpage,
	VOPNAME_DISPOSE, xram_dispose,
	VOPNAME_MAP, (fs_generic_func_p) xram_map,
	VOPNAME_ADDMAP, (fs_generic_func_p) xram_addmap,
	VOPNAME_DELMAP, xram_delmap,
	VOPNAME_SEEK, xram_seek,
	VOPNAME_VNEVENT, fs_vnevent_support,
	NULL, NULL
};
