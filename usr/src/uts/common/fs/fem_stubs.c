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
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2008 NEC Corporation
 */

#pragma ident	"@(#)fem_stubs.c"

#include <sys/types.h>
#include <sys/atomic.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>

#include <sys/fem.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/vfs_opreg.h>

/* ARGSUSED */
void
fem_free(fem_t *p)
{
}

/* ARGSUSED */
void
fsem_free(fsem_t *p)
{
}

/* ARGSUSED */
int
vnext_open(femarg_t *vf, int mode, cred_t *cr, caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_close(femarg_t *vf, int flag, int count, offset_t offset, cred_t *cr,
	caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_read(femarg_t *vf, uio_t *uiop, int ioflag, cred_t *cr,
	caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_write(femarg_t *vf, uio_t *uiop, int ioflag, cred_t *cr,
	caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_ioctl(femarg_t *vf, int cmd, intptr_t arg, int flag, cred_t *cr,
	int *rvalp, caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_setfl(femarg_t *vf, int oflags, int nflags, cred_t *cr,
	caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_getattr(femarg_t *vf, vattr_t *vap, int flags, cred_t *cr,
	caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_setattr(femarg_t *vf, vattr_t *vap, int flags, cred_t *cr,
	caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_access(femarg_t *vf, int mode, int flags, cred_t *cr,
	caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_lookup(femarg_t *vf, char *nm, vnode_t **vpp, pathname_t *pnp,
	int flags, vnode_t *rdir, cred_t *cr, caller_context_t *ct,
	int *direntflags, pathname_t *realpnp)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_create(femarg_t *vf, char *name, vattr_t *vap, vcexcl_t excl,
	int mode, vnode_t **vpp, cred_t *cr, int flag, caller_context_t *ct,
	vsecattr_t *vsecp)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_remove(femarg_t *vf, char *nm, cred_t *cr, caller_context_t *ct,
	int flags)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_link(femarg_t *vf, vnode_t *svp, char *tnm, cred_t *cr,
	caller_context_t *ct, int flags)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_rename(femarg_t *vf, char *snm, vnode_t *tdvp, char *tnm, cred_t *cr,
	caller_context_t *ct, int flags)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_mkdir(femarg_t *vf, char *dirname, vattr_t *vap, vnode_t **vpp,
	cred_t *cr, caller_context_t *ct, int flags, vsecattr_t *vsecp)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_rmdir(femarg_t *vf, char *nm, vnode_t *cdir, cred_t *cr,
	caller_context_t *ct, int flags)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_readdir(femarg_t *vf, uio_t *uiop, cred_t *cr, int *eofp,
	caller_context_t *ct, int flags)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_symlink(femarg_t *vf, char *linkname, vattr_t *vap, char *target,
	cred_t *cr, caller_context_t *ct, int flags)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_readlink(femarg_t *vf, uio_t *uiop, cred_t *cr, caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_fsync(femarg_t *vf, int syncflag, cred_t *cr, caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
void
vnext_inactive(femarg_t *vf, cred_t *cr, caller_context_t *ct)
{
}

/* ARGSUSED */
int
vnext_fid(femarg_t *vf, fid_t *fidp, caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_rwlock(femarg_t *vf, int write_lock, caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
void
vnext_rwunlock(femarg_t *vf, int write_lock, caller_context_t *ct)
{
}

/* ARGSUSED */
int
vnext_seek(femarg_t *vf, offset_t ooff, offset_t *noffp, caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_cmp(femarg_t *vf, vnode_t *vp2, caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_frlock(femarg_t *vf, int cmd, struct flock64 *bfp, int flag,
	offset_t offset, struct flk_callback *flk_cbp, cred_t *cr,
	caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_space(femarg_t *vf, int cmd, struct flock64 *bfp, int flag,
	offset_t offset, cred_t *cr, caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_realvp(femarg_t *vf, vnode_t **vpp, caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_getpage(femarg_t *vf, offset_t off, size_t len, uint_t *protp,
	struct page **plarr, size_t plsz, struct seg *seg, caddr_t addr,
	enum seg_rw rw, cred_t *cr, caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_putpage(femarg_t *vf, offset_t off, size_t len, int flags,
	cred_t *cr, caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_map(femarg_t *vf, offset_t off, struct as *as, caddr_t *addrp,
	size_t len, uchar_t prot, uchar_t maxprot, uint_t flags,
	cred_t *cr, caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_addmap(femarg_t *vf, offset_t off, struct as *as, caddr_t addr,
	size_t len, uchar_t prot, uchar_t maxprot, uint_t flags,
	cred_t *cr, caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_delmap(femarg_t *vf, offset_t off, struct as *as, caddr_t addr,
	size_t len, uint_t prot, uint_t maxprot, uint_t flags,
	cred_t *cr, caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_poll(femarg_t *vf, short events, int anyyet, short *reventsp,
	struct pollhead **phpp, caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_dump(femarg_t *vf, caddr_t addr, int lbdn, int dblks,
	caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_pathconf(femarg_t *vf, int cmd, ulong_t *valp, cred_t *cr,
	caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_pageio(femarg_t *vf, struct page *pp, u_offset_t io_off,
	size_t io_len, int flags, cred_t *cr, caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_dumpctl(femarg_t *vf, int action, int *blkp, caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
void
vnext_dispose(femarg_t *vf, struct page *pp, int flag, int dn, cred_t *cr,
	caller_context_t *ct)
{
}

/* ARGSUSED */
int
vnext_setsecattr(femarg_t *vf, vsecattr_t *vsap, int flag, cred_t *cr,
	caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_getsecattr(femarg_t *vf, vsecattr_t *vsap, int flag, cred_t *cr,
	caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_shrlock(femarg_t *vf, int cmd, struct shrlock *shr, int flag,
	cred_t *cr, caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vnext_vnevent(femarg_t *vf, vnevent_t vnevent, vnode_t *dvp, char *cname,
    caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vfsnext_mount(fsemarg_t *vf, vnode_t *mvp, struct mounta *uap, cred_t *cr)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vfsnext_unmount(fsemarg_t *vf, int flag, cred_t *cr)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vfsnext_root(fsemarg_t *vf, vnode_t **vpp)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vfsnext_statvfs(fsemarg_t *vf, statvfs64_t *sp)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vfsnext_sync(fsemarg_t *vf, short flag, cred_t *cr)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vfsnext_vget(fsemarg_t *vf, vnode_t **vpp, fid_t *fidp)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
vfsnext_mountroot(fsemarg_t *vf, enum whymountroot reason)
{
	return (ENOTSUP);
}

/* ARGSUSED */
void
vfsnext_freevfs(fsemarg_t *vf)
{
}

/* ARGSUSED */
int
vfsnext_vnstate(fsemarg_t *vf, vnode_t *vp, vntrans_t nstate)
{
	return (ENOTSUP);
}


/*
 * VNODE interposition.
 */

/* ARGSUSED */
int
fem_create(char *name, const struct fs_operation_def *templ,
    fem_t **actual)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
fem_install(
	vnode_t *vp,		/* Vnode on which monitor is being installed */
	fem_t *mon,		/* Monitor operations being installed */
	void *arg,		/* Opaque data used by monitor */
	femhow_t how,		/* Installation control */
	void (*arg_hold)(void *),	/* Hold routine for "arg" */
	void (*arg_rele)(void *))	/* Release routine for "arg" */
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
fem_is_installed(vnode_t *v, fem_t *mon, void *arg)
{
	return (0);
}

/* ARGSUSED */
int
fem_uninstall(vnode_t *v, fem_t *mon, void *arg)
{
	return (ENOTSUP);
}

/* ARGSUSED */
void
fem_setvnops(vnode_t *v, vnodeops_t *newops)
{
}

vnodeops_t *
fem_getvnops(vnode_t *v)
{
	return (v->v_op);
}


/*
 * VFS interposition
 */
/* ARGSUSED */
int
fsem_create(char *name, const struct fs_operation_def *templ,
    fsem_t **actual)
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
fsem_is_installed(struct vfs *v, fsem_t *mon, void *arg)
{
	return (0);
}

/* ARGSUSED */
int
fsem_install(
	struct vfs *vfsp,	/* VFS on which monitor is being installed */
	fsem_t *mon,		/* Monitor operations being installed */
	void *arg,		/* Opaque data used by monitor */
	femhow_t how,		/* Installation control */
	void (*arg_hold)(void *),	/* Hold routine for "arg" */
	void (*arg_rele)(void *))	/* Release routine for "arg" */
{
	return (ENOTSUP);
}

/* ARGSUSED */
int
fsem_uninstall(struct vfs *v, fsem_t *mon, void *arg)
{
	return (ENOTSUP);
}

/* ARGSUSED */
void
fsem_setvfsops(vfs_t *v, vfsops_t *newops)
{
}

vfsops_t *
fsem_getvfsops(vfs_t *v)
{
	return (v->vfs_op);
}

/*
 * Setup FEM.
 */
void
fem_init()
{
}
