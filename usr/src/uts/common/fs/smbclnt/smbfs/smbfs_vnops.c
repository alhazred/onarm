/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: smbfs_vnops.c,v 1.128.36.1 2005/05/27 02:35:28 lindak Exp $
 */

/*
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)smbfs_vnops.c	1.1	08/02/13 SMI"

#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/uio.h>
#include <sys/dirent.h>
#include <sys/errno.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/dnlc.h>
#include <sys/vfs_opreg.h>
#include <sys/policy.h>

#include <netsmb/smb_osdep.h>
#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>

#include <smbfs/smbfs.h>
#include <smbfs/smbfs_node.h>
#include <smbfs/smbfs_subr.h>

#include <fs/fs_subr.h>

/*
 * These characters are illegal in NTFS file names.
 * ref: http://support.microsoft.com/kb/147438
 */
static const char illegal_chars[] = {
	'\\',	/* back slash */
	'/',	/* slash */
	':',	/* colon */
	'*',	/* asterisk */
	'?',	/* question mark */
	'"',	/* double quote */
	'<',	/* less than sign */
	'>',	/* greater than sign */
	'|',	/* vertical bar */
	0
};

/*
 * Turning this on causes nodes to be created in the cache
 * during directory listings.  The "fast" claim is debatable,
 * and the effects on the cache can be undesirable.
 */

/* local static function defines */

static int	smbfslookup(vnode_t *dvp, char *nm, vnode_t **vpp, cred_t *cr,
			int dnlc, caller_context_t *);
static int	smbfsrename(vnode_t *odvp, char *onm, vnode_t *ndvp, char *nnm,
			cred_t *cr, caller_context_t *);
static int	smbfssetattr(vnode_t *, struct vattr *, int, cred_t *);
static int	smbfs_accessx(void *, int, cred_t *);
static int	smbfs_readvdir(vnode_t *vp, uio_t *uio, cred_t *cr, int *eofp,
			caller_context_t *);
/*
 * These are the vnode ops routines which implement the vnode interface to
 * the networked file system.  These routines just take their parameters,
 * make them look networkish by putting the right info into interface structs,
 * and then calling the appropriate remote routine(s) to do the work.
 *
 * Note on directory name lookup cacheing:  If we detect a stale fhandle,
 * we purge the directory cache relative to that vnode.  This way, the
 * user won't get burned by the cache repeatedly.  See <smbfs/smbnode.h> for
 * more details on smbnode locking.
 */

static int	smbfs_open(vnode_t **, int, cred_t *, caller_context_t *);
static int	smbfs_close(vnode_t *, int, int, offset_t, cred_t *,
			caller_context_t *);
static int	smbfs_read(vnode_t *, struct uio *, int, cred_t *,
			caller_context_t *);
static int	smbfs_write(vnode_t *, struct uio *, int, cred_t *,
			caller_context_t *);
static int	smbfs_getattr(vnode_t *, struct vattr *, int, cred_t *,
			caller_context_t *);
static int	smbfs_setattr(vnode_t *, struct vattr *, int, cred_t *,
			caller_context_t *);
static int	smbfs_access(vnode_t *, int, int, cred_t *, caller_context_t *);
static int	smbfs_fsync(vnode_t *, int, cred_t *, caller_context_t *);
static void	smbfs_inactive(vnode_t *, cred_t *, caller_context_t *);
static int	smbfs_lookup(vnode_t *, char *, vnode_t **, struct pathname *,
			int, vnode_t *, cred_t *, caller_context_t *,
			int *, pathname_t *);
static int	smbfs_create(vnode_t *, char *, struct vattr *, enum vcexcl,
			int, vnode_t **, cred_t *, int, caller_context_t *,
			vsecattr_t *);
static int	smbfs_remove(vnode_t *, char *, cred_t *, caller_context_t *,
			int);
static int	smbfs_rename(vnode_t *, char *, vnode_t *, char *, cred_t *,
			caller_context_t *, int);
static int	smbfs_mkdir(vnode_t *, char *, struct vattr *, vnode_t **,
			cred_t *, caller_context_t *, int, vsecattr_t *);
static int	smbfs_rmdir(vnode_t *, char *, vnode_t *, cred_t *,
			caller_context_t *, int);
static int	smbfs_readdir(vnode_t *, struct uio *, cred_t *, int *,
			caller_context_t *, int);
static int	smbfs_rwlock(vnode_t *, int, caller_context_t *);
static void	smbfs_rwunlock(vnode_t *, int, caller_context_t *);
static int	smbfs_seek(vnode_t *, offset_t, offset_t *, caller_context_t *);
static int	smbfs_frlock(vnode_t *, int, struct flock64 *, int, offset_t,
			struct flk_callback *, cred_t *, caller_context_t *);
static int	smbfs_space(vnode_t *, int, struct flock64 *, int, offset_t,
			cred_t *, caller_context_t *);
static int	smbfs_pathconf(vnode_t *, int, ulong_t *, cred_t *,
			caller_context_t *);
static int	smbfs_shrlock(vnode_t *, int, struct shrlock *, int, cred_t *,
			caller_context_t *);

/* Dummy function to use until correct function is ported in */
int noop_vnodeop() {
	return (0);
}

struct vnodeops *smbfs_vnodeops = NULL;

/*
 * Most unimplemented ops will return ENOSYS because of fs_nosys().
 * The only ops where that won't work are ACCESS (due to open(2)
 * failures) and GETSECATTR (due to acl(2) failures).
 */
const fs_operation_def_t smbfs_vnodeops_template[] = {
	{ VOPNAME_OPEN, { .vop_open = smbfs_open } },
	{ VOPNAME_CLOSE, { .vop_close = smbfs_close } },
	{ VOPNAME_READ, { .vop_read = smbfs_read } },
	{ VOPNAME_WRITE, { .vop_write = smbfs_write } },
	{ VOPNAME_IOCTL, { .error = fs_nosys } }, /* smbfs_ioctl, */
	{ VOPNAME_GETATTR, { .vop_getattr = smbfs_getattr } },
	{ VOPNAME_SETATTR, { .vop_setattr = smbfs_setattr } },
	{ VOPNAME_ACCESS, { .vop_access = smbfs_access } },
	{ VOPNAME_LOOKUP, { .vop_lookup = smbfs_lookup } },
	{ VOPNAME_CREATE, { .vop_create = smbfs_create } },
	{ VOPNAME_REMOVE, { .vop_remove = smbfs_remove } },
	{ VOPNAME_LINK, { .error = fs_nosys } }, /* smbfs_link, */
	{ VOPNAME_RENAME, { .vop_rename = smbfs_rename } },
	{ VOPNAME_MKDIR, { .vop_mkdir = smbfs_mkdir } },
	{ VOPNAME_RMDIR, { .vop_rmdir = smbfs_rmdir } },
	{ VOPNAME_READDIR, { .vop_readdir = smbfs_readdir } },
	{ VOPNAME_SYMLINK, { .error = fs_nosys } }, /* smbfs_symlink, */
	{ VOPNAME_READLINK, { .error = fs_nosys } }, /* smbfs_readlink, */
	{ VOPNAME_FSYNC, { .vop_fsync = smbfs_fsync } },
	{ VOPNAME_INACTIVE, { .vop_inactive = smbfs_inactive } },
	{ VOPNAME_FID, { .error = fs_nosys } }, /* smbfs_fid, */
	{ VOPNAME_RWLOCK, { .vop_rwlock = smbfs_rwlock } },
	{ VOPNAME_RWUNLOCK, { .vop_rwunlock = smbfs_rwunlock } },
	{ VOPNAME_SEEK, { .vop_seek = smbfs_seek } },
	{ VOPNAME_FRLOCK, { .vop_frlock = smbfs_frlock } },
	{ VOPNAME_SPACE, { .vop_space = smbfs_space } },
	{ VOPNAME_REALVP, { .error = fs_nosys } }, /* smbfs_realvp, */
	{ VOPNAME_GETPAGE, { .error = fs_nosys } }, /* smbfs_getpage, */
	{ VOPNAME_PUTPAGE, { .error = fs_nosys } }, /* smbfs_putpage, */
	{ VOPNAME_MAP, { .error = fs_nosys } }, /* smbfs_map, */
	{ VOPNAME_ADDMAP, { .error = fs_nosys } }, /* smbfs_addmap, */
	{ VOPNAME_DELMAP, { .error = fs_nosys } }, /* smbfs_delmap, */
	{ VOPNAME_DUMP, { .error = fs_nosys } }, /* smbfs_dump, */
	{ VOPNAME_PATHCONF, { .vop_pathconf = smbfs_pathconf } },
	{ VOPNAME_PAGEIO, { .error = fs_nosys } }, /* smbfs_pageio, */
	{ VOPNAME_SETSECATTR, { .error = fs_nosys } }, /* smbfs_setsecattr, */
	{ VOPNAME_GETSECATTR, { .error = noop_vnodeop } },
						/* smbfs_getsecattr, */
	{ VOPNAME_SHRLOCK, { .vop_shrlock = smbfs_shrlock } },
	{ NULL, NULL }
};

/*
 * XXX
 * When new and relevant functionality is enabled, we should be
 * calling vfs_set_feature() to inform callers that pieces of
 * functionality are available, per PSARC 2007/227, e.g.
 *
 * VFSFT_XVATTR            Supports xvattr for attrs
 * VFSFT_CASEINSENSITIVE   Supports case-insensitive
 * VFSFT_NOCASESENSITIVE   NOT case-sensitive
 * VFSFT_DIRENTFLAGS       Supports dirent flags
 * VFSFT_ACLONCREATE       Supports ACL on create
 * VFSFT_ACEMASKONACCESS   Can use ACEMASK for access
 */
/* ARGSUSED */
static int
smbfs_open(vnode_t **vpp, int flag, cred_t *cr, caller_context_t *ct)
{
	struct vattr	va;
	smbnode_t	*np;
	vnode_t		*vp;
	u_int32_t	rights, rightsrcvd;
	u_int16_t	fid, oldfid;
	struct smb_cred scred;
	smbmntinfo_t	*smi;
	cred_t		*oldcr;
	int		attrcacheupdated = 0;
	int		tmperror;
	int		error = 0;

	vp = *vpp;
	np = VTOSMB(vp);
	smi = VTOSMI(vp);

	if (curproc->p_zone != smi->smi_zone)
		return (EIO);

	if (smi->smi_flags & SMI_DEAD || vp->v_vfsp->vfs_flag & VFS_UNMOUNTED)
		return (EIO);

	if (vp->v_type != VREG && vp->v_type != VDIR) { /* XXX VLNK? */
		SMBVDEBUG("open eacces vtype=%d\n", vp->v_type);
		return (EACCES);
	}

	/*
	 * Get exclusive access to n_fid and related stuff.
	 * No returns after this until out.
	 */
	if (smbfs_rw_enter_sig(&np->r_lkserlock, RW_WRITER, SMBINTR(vp)))
		return (EINTR);
	smb_credinit(&scred, curproc, cr);

	/*
	 * Directory open is easy.
	 */
	if (vp->v_type == VDIR) {
		np->n_dirrefs++;
		goto have_fid;
	}

	/*
	 * If caller specified O_TRUNC/FTRUNC, then be sure to set
	 * FWRITE (to drive successful setattr(size=0) after open)
	 */
	if (flag & FTRUNC)
		flag |= FWRITE;

	/*
	 * If we already have it open, check to see if current rights
	 * are sufficient for this open.
	 */
	if (np->n_fidrefs) {
		int upgrade = 0;

		/* BEGIN CSTYLED */
		if ((flag & FWRITE) &&
		    !(np->n_rights & (SA_RIGHT_FILE_WRITE_DATA |
				GENERIC_RIGHT_ALL_ACCESS |
				GENERIC_RIGHT_WRITE_ACCESS)))
			upgrade = 1;
		if ((flag & FREAD) &&
		    !(np->n_rights & (SA_RIGHT_FILE_READ_DATA |
				GENERIC_RIGHT_ALL_ACCESS |
				GENERIC_RIGHT_READ_ACCESS)))
			upgrade = 1;
		/* END CSTYLED */
		if (!upgrade) {
			/*
			 *  the existing open is good enough
			 */
			np->n_fidrefs++;
			goto have_fid;
		}
	}
	rights = np->n_fidrefs ? np->n_rights : 0;

	/*
	 * we always ask for READ_CONTROL so we can always get the
	 * owner/group IDs to satisfy a stat.
	 * XXX: verify that works with "drop boxes"
	 */
	rights |= STD_RIGHT_READ_CONTROL_ACCESS;
	if ((flag & FREAD))
		rights |= SA_RIGHT_FILE_READ_DATA;
	if ((flag & FWRITE))
		rights |= SA_RIGHT_FILE_APPEND_DATA | SA_RIGHT_FILE_WRITE_DATA;

	/* XXX: open gets the current size, but we don't use it. */
	error = smbfs_smb_open(np, rights, &scred, &attrcacheupdated, &fid,
	    NULL, 0, 0, NULL, &rightsrcvd);
	if (error)
		goto out;

	/*
	 * We have a new FID and access rights.
	 */
	oldfid = np->n_fid;
	np->n_fid = fid;
	np->n_rights = rightsrcvd;
	np->n_fidrefs++;
	if (np->n_fidrefs > 1) {
		/*
		 * We already had it open (presumably because
		 * it was open with insufficient rights.)
		 * Close old wire-open.
		 */
		tmperror = smbfs_smb_close(smi->smi_share,
		    oldfid, &np->n_mtime, &scred);
		if (tmperror)
			SMBVDEBUG("error %d closing %s\n",
			    tmperror, np->n_rpath);
	}

	/*
	 * This thread did the open.
	 * Save our credentials too.
	 */
	mutex_enter(&np->r_statelock);
	oldcr = np->r_cred;
	np->r_cred = cr;
	crhold(cr);
	if (oldcr)
		crfree(oldcr);
	mutex_exit(&np->r_statelock);

have_fid:
	/* Get attributes (maybe). */


	/* Darwin (derived) code. */

	va.va_mask = AT_MTIME;
	if (np->n_flag & NMODIFIED)
		smbfs_attr_cacheremove(np);

	/*
	 * Try to get attributes, but don't bail on error.
	 * We already hold r_lkserlock/reader so note:
	 * this call will recursively take r_lkserlock.
	 */
	tmperror = smbfsgetattr(vp, &va, cr);
	if (tmperror)
		SMBERROR("getattr failed, error=%d", tmperror);
	else
		np->n_mtime.tv_sec = va.va_mtime.tv_sec;

out:
	smb_credrele(&scred);
	smbfs_rw_exit(&np->r_lkserlock);
	return (error);
}

/*ARGSUSED*/
static int
smbfs_close(vnode_t *vp, int flag, int count, offset_t offset, cred_t *cr,
	caller_context_t *ct)
{
	smbnode_t	*np;
	int		error = 0;
	struct smb_cred scred;

	np = VTOSMB(vp);

	/*
	 * Don't "bail out" for VFS_UNMOUNTED here,
	 * as we want to do cleanup, etc.
	 */

	/*
	 * zone_enter(2) prevents processes from changing zones with SMBFS files
	 * open; if we happen to get here from the wrong zone we can't do
	 * anything over the wire.
	 */
	if (VTOSMI(vp)->smi_zone != curproc->p_zone) {
		/*
		 * We could attempt to clean up locks, except we're sure
		 * that the current process didn't acquire any locks on
		 * the file: any attempt to lock a file belong to another zone
		 * will fail, and one can't lock an SMBFS file and then change
		 * zones, as that fails too.
		 *
		 * Returning an error here is the sane thing to do.  A
		 * subsequent call to VN_RELE() which translates to a
		 * smbfs_inactive() will clean up state: if the zone of the
		 * vnode's origin is still alive and kicking, an async worker
		 * thread will handle the request (from the correct zone), and
		 * everything (minus the final smbfs_getattr_otw() call) should
		 * be OK. If the zone is going away smbfs_async_inactive() will
		 * throw away cached pages inline.
		 */
		return (EIO);
	}

	/*
	 * If we are using local locking for this filesystem, then
	 * release all of the SYSV style record locks.  Otherwise,
	 * we are doing network locking and we need to release all
	 * of the network locks.  All of the locks held by this
	 * process on this file are released no matter what the
	 * incoming reference count is.
	 */
	if (VTOSMI(vp)->smi_flags & SMI_LLOCK) {
		cleanlocks(vp, ttoproc(curthread)->p_pid, 0);
		cleanshares(vp, ttoproc(curthread)->p_pid);
	}

	if (count > 1)
		return (0);
	/*
	 * OK, do "last close" stuff.
	 */


	/*
	 * Do the CIFS close.
	 * Darwin code
	 */

	/*
	 * Exclusive lock for modifying n_fid stuff.
	 * Don't want this one ever interruptible.
	 */
	(void) smbfs_rw_enter_sig(&np->r_lkserlock, RW_WRITER, 0);
	smb_credinit(&scred, curproc, cr);

	error = 0;
	if (vp->v_type == VDIR) {
		struct smbfs_fctx *fctx;
		ASSERT(np->n_dirrefs > 0);
		if (--np->n_dirrefs)
			goto out;
		if ((fctx = np->n_dirseq) != NULL) {
			np->n_dirseq = NULL;
			error = smbfs_smb_findclose(fctx, &scred);
		}
	} else {
		uint16_t ofid;
		ASSERT(np->n_fidrefs > 0);
		if (--np->n_fidrefs)
			goto out;
		if ((ofid = np->n_fid) != SMB_FID_UNUSED) {
			np->n_fid = SMB_FID_UNUSED;
			error = smbfs_smb_close(np->n_mount->smi_share,
			    ofid, NULL, &scred);
		}
	}
	if (error) {
		SMBERROR("error %d closing %s\n",
		    error, np->n_rpath);
	}

	if (np->n_flag & NATTRCHANGED)
		smbfs_attr_cacheremove(np);

out:
	smb_credrele(&scred);
	smbfs_rw_exit(&np->r_lkserlock);

	/* don't return any errors */
	return (0);
}

/* ARGSUSED */
static int
smbfs_read(vnode_t *vp, struct uio *uiop, int ioflag, cred_t *cr,
	caller_context_t *ct)
{
	int		error;
	struct vattr	va;
	smbmntinfo_t	*smi;
	smbnode_t	*np;
	/* u_offset_t	off; */
	/* offset_t	diff; */

	np = VTOSMB(vp);
	smi = VTOSMI(vp);

	if (curproc->p_zone != smi->smi_zone)
		return (EIO);

	if (smi->smi_flags & SMI_DEAD || vp->v_vfsp->vfs_flag & VFS_UNMOUNTED)
		return (EIO);

	ASSERT(smbfs_rw_lock_held(&np->r_rwlock, RW_READER));

	if (vp->v_type != VREG)
		return (EISDIR);

	if (uiop->uio_resid == 0)
		return (0);

	/*
	 * Like NFS3, just check for 63-bit overflow.
	 * Our SMB layer takes care to return EFBIG
	 * when it has to fallback to a 32-bit call.
	 */
	if (uiop->uio_loffset < 0 ||
	    uiop->uio_loffset + uiop->uio_resid < 0)
		return (EINVAL);

	/* Shared lock for n_fid use in smbfs_readvnode */
	if (smbfs_rw_enter_sig(&np->r_lkserlock, RW_READER, SMBINTR(vp)))
		return (EINTR);

	/* get vnode attributes from server */
	va.va_mask = AT_SIZE | AT_MTIME;
	if (error = smbfsgetattr(vp, &va, cr))
		goto out;

	/* should probably update mtime with mtime from server here */

	/*
	 * Darwin had a loop here that handled paging stuff.
	 * Solaris does paging differently, so no loop needed.
	 */
	error = smbfs_readvnode(vp, uiop, cr, &va);

out:
	smbfs_rw_exit(&np->r_lkserlock);
	return (error);

}


/* ARGSUSED */
static int
smbfs_write(vnode_t *vp, struct uio *uiop, int ioflag, cred_t *cr,
	caller_context_t *ct)
{
	int		error;
	smbmntinfo_t 	*smi;
	smbnode_t 	*np;
	int		timo = SMBWRTTIMO;

	np = VTOSMB(vp);
	smi = VTOSMI(vp);

	if (curproc->p_zone != smi->smi_zone)
		return (EIO);

	if (smi->smi_flags & SMI_DEAD || vp->v_vfsp->vfs_flag & VFS_UNMOUNTED)
		return (EIO);

	ASSERT(smbfs_rw_lock_held(&np->r_rwlock, RW_WRITER));

	if (vp->v_type != VREG)
		return (EISDIR);

	if (uiop->uio_resid == 0)
		return (0);

	/* Shared lock for n_fid use in smbfs_writevnode */
	if (smbfs_rw_enter_sig(&np->r_lkserlock, RW_READER, SMBINTR(vp)))
		return (EINTR);


	/*
	 * Darwin had a loop here that handled paging stuff.
	 * Solaris does paging differently, so no loop needed.
	 */
	error = smbfs_writevnode(vp, uiop, cr, ioflag, timo);

	smbfs_rw_exit(&np->r_lkserlock);
	return (error);

}


/*
 * Return either cached or remote attributes. If get remote attr
 * use them to check and invalidate caches, then cache the new attributes.
 *
 * XXX
 * This op should eventually support PSARC 2007/315, Extensible Attribute
 * Interfaces, for richer metadata.
 */
/* ARGSUSED */
static int
smbfs_getattr(vnode_t *vp, struct vattr *vap, int flags, cred_t *cr,
	caller_context_t *ct)
{
	smbnode_t *np;
	smbmntinfo_t *smi;

	smi = VTOSMI(vp);

	if (curproc->p_zone != smi->smi_zone)
		return (EIO);

	if (smi->smi_flags & SMI_DEAD || vp->v_vfsp->vfs_flag & VFS_UNMOUNTED)
		return (EIO);

	/*
	 * If it has been specified that the return value will
	 * just be used as a hint, and we are only being asked
	 * for size, fsid or rdevid, then return the client's
	 * notion of these values without checking to make sure
	 * that the attribute cache is up to date.
	 * The whole point is to avoid an over the wire GETATTR
	 * call.
	 */
	np = VTOSMB(vp);
	if (flags & ATTR_HINT) {
		if (vap->va_mask ==
		    (vap->va_mask & (AT_SIZE | AT_FSID | AT_RDEV))) {
			mutex_enter(&np->r_statelock);
			if (vap->va_mask | AT_SIZE)
				vap->va_size = np->r_size;
			if (vap->va_mask | AT_FSID)
				vap->va_fsid = np->r_attr.va_fsid;
			if (vap->va_mask | AT_RDEV)
				vap->va_rdev = np->r_attr.va_rdev;
			mutex_exit(&np->r_statelock);
			return (0);
		}
	}


	return (smbfsgetattr(vp, vap, cr));
}

/*
 * Mostly from Darwin smbfs_getattr()
 */
int
smbfsgetattr(vnode_t *vp, struct vattr *vap, cred_t *cr)
{
	int error;
	smbnode_t *np;
	struct smb_cred scred;
	struct smbfattr fattr;

	ASSERT(curproc->p_zone == VTOSMI(vp)->smi_zone);

	np = VTOSMB(vp);

	/*
	 * If we've got cached attributes, we're done, otherwise go
	 * to the server to get attributes, which will update the cache
	 * in the process.
	 *
	 * This section from Darwin smbfs_getattr,
	 * but then modified a lot.
	 */
	error = smbfs_attr_cachelookup(vp, vap);
	if (error != ENOENT)
		return (error);

	/* Shared lock for (possible) n_fid use. */
	if (smbfs_rw_enter_sig(&np->r_lkserlock, RW_READER, SMBINTR(vp)))
		return (EINTR);
	smb_credinit(&scred, curproc, cr);

	error = smbfs_smb_getfattr(np, &fattr, &scred);

	smb_credrele(&scred);
	smbfs_rw_exit(&np->r_lkserlock);

	if (!error) {
		smbfs_attr_cacheenter(vp, &fattr);
		error = smbfs_attr_cachelookup(vp, vap);
	}
	return (error);
}

/*
 * XXX
 * This op should eventually support PSARC 2007/315, Extensible Attribute
 * Interfaces, for richer metadata.
 */
/*ARGSUSED4*/
static int
smbfs_setattr(vnode_t *vp, struct vattr *vap, int flags, cred_t *cr,
		caller_context_t *ct)
{
	int		error;
	uint_t		mask;
	struct vattr	oldva;
	smbmntinfo_t	*smi;

	smi = VTOSMI(vp);

	if (curproc->p_zone != smi->smi_zone)
		return (EIO);

	if (smi->smi_flags & SMI_DEAD || vp->v_vfsp->vfs_flag & VFS_UNMOUNTED)
		return (EIO);

	mask = vap->va_mask;
	if (mask & AT_NOSET)
		return (EINVAL);

	oldva.va_mask = AT_TYPE | AT_MODE | AT_UID | AT_GID;
	error = smbfsgetattr(vp, &oldva, cr);
	if (error)
		return (error);

	error = secpolicy_vnode_setattr(cr, vp, vap, &oldva, flags,
	    smbfs_accessx, vp);
	if (error)
		return (error);

	return (smbfssetattr(vp, vap, flags, cr));
}

/*
 * Mostly from Darwin smbfs_setattr()
 * but then modified a lot.
 */
/* ARGSUSED */
static int
smbfssetattr(vnode_t *vp, struct vattr *vap, int flags, cred_t *cr)
{
	int		error = 0;
	smbnode_t	*np = VTOSMB(vp);
	smbmntinfo_t	*smi = VTOSMI(vp);
	uint_t		mask = vap->va_mask;
	struct timespec	*mtime, *atime;
	struct smb_cred	scred;
	int		cerror, modified = 0;
	unsigned short	fid;
	int have_fid = 0;
	uint32_t rights = 0;

	ASSERT(curproc->p_zone == smi->smi_zone);

	/*
	 * If our caller is trying to set multiple attributes, they
	 * can make no assumption about what order they are done in.
	 * Here we try to do them in order of decreasing likelihood
	 * of failure, just to minimize the chance we'll wind up
	 * with a partially complete request.
	 */

	/* Shared lock for (possible) n_fid use. */
	if (smbfs_rw_enter_sig(&np->r_lkserlock, RW_READER, SMBINTR(vp)))
		return (EINTR);
	smb_credinit(&scred, curproc, cr);

	/*
	 * Will we need an open handle for this setattr?
	 * If so, what rights will we need?
	 */
	if (mask & (AT_ATIME | AT_MTIME)) {
		rights |=
		    SA_RIGHT_FILE_WRITE_ATTRIBUTES |
		    GENERIC_RIGHT_ALL_ACCESS |
		    GENERIC_RIGHT_WRITE_ACCESS;
	}
	if (mask & AT_SIZE) {
		rights |=
		    SA_RIGHT_FILE_WRITE_DATA |
		    SA_RIGHT_FILE_APPEND_DATA;
		/*
		 * Only SIZE requires a handle.
		 * XXX May be more reliable to just
		 * always get the file handle here.
		 */
		error = smbfs_smb_tmpopen(np, rights, &scred, &fid);
		if (error) {
			SMBVDEBUG("error %d opening %s\n",
			    error, np->n_rpath);
			goto out;
		}
		have_fid = 1;
	}


	/*
	 * If the server supports the UNIX extensions, right here is where
	 * we'd support changes to uid, gid, mode, and possibly va_flags.
	 * For now we claim to have made any such changes.
	 */

	if (mask & AT_SIZE) {
		/*
		 * If the new file size is less than what the client sees as
		 * the file size, then just change the size and invalidate
		 * the pages.
		 * I am commenting this code at present because the function
		 * smbfs_putapage() is not yet implemented.
		 */

		/*
		 * Set the file size to vap->va_size.
		 */
		ASSERT(have_fid);
		error = smbfs_smb_setfsize(np, fid, vap->va_size, &scred);
		if (error) {
			SMBVDEBUG("setsize error %d file %s\n",
			    error, np->n_rpath);
		} else {
			/*
			 * Darwin had code here to zero-extend.
			 * Tests indicate the server will zero-fill,
			 * so looks like we don't need to do this.
			 * Good thing, as this could take forever.
			 */
			mutex_enter(&np->r_statelock);
			np->r_size = vap->va_size;
			mutex_exit(&np->r_statelock);
			modified = 1;
		}
	}

	/*
	 * XXX: When Solaris has create_time, set that too.
	 * Note: create_time is different from ctime.
	 */
	mtime = ((mask & AT_MTIME) ? &vap->va_mtime : 0);
	atime = ((mask & AT_ATIME) ? &vap->va_atime : 0);

	if (mtime || atime) {
		/*
		 * If file is opened with write-attributes capability,
		 * we use handle-based calls.  If not, we use path-based ones.
		 */
		if (have_fid) {
			error = smbfs_smb_setfattr(np, fid,
			    np->n_dosattr, mtime, atime, &scred);
		} else {
			error = smbfs_smb_setpattr(np,
			    np->n_dosattr, mtime, atime, &scred);
		}
		if (error) {
			SMBVDEBUG("set times error %d file %s\n",
			    error, np->n_rpath);
		} else {
			/* XXX: set np->n_mtime, etc? */
			modified = 1;
		}
	}

out:
	if (modified) {
		/*
		 * Invalidate attribute cache in case if server doesn't set
		 * required attributes.
		 */
		smbfs_attr_cacheremove(np);
		/*
		 * XXX Darwin called _getattr here to
		 * update the mtime.  Should we?
		 */
	}

	if (have_fid) {
		cerror = smbfs_smb_tmpclose(np, fid, &scred);
		if (cerror)
			SMBERROR("error %d closing %s\n",
			    cerror, np->n_rpath);
	}

	smb_credrele(&scred);
	smbfs_rw_exit(&np->r_lkserlock);

	return (error);
}

/*
 * smbfs_access_rwx()
 * Common function for smbfs_access, etc.
 *
 * The security model implemented by the FS is unusual
 * due to our "single user mounts" restriction.
 *
 * All access under a given mount point uses the CIFS
 * credentials established by the owner of the mount.
 * The Unix uid/gid/mode information is not (easily)
 * provided by CIFS, and is instead fabricated using
 * settings held in the mount structure.
 *
 * Most access checking is handled by the CIFS server,
 * but we need sufficient Unix access checks here to
 * prevent other local Unix users from having access
 * to objects under this mount that the uid/gid/mode
 * settings in the mount would not allow.
 *
 * With this model, there is a case where we need the
 * ability to do an access check before we have the
 * vnode for an object.  This function takes advantage
 * of the fact that the uid/gid/mode is per mount, and
 * avoids the need for a vnode.
 *
 * We still (sort of) need a vnode when we call
 * secpolicy_vnode_access, but that only uses
 * the vtype field, so we can use a pair of fake
 * vnodes that have only v_type filled in.
 *
 * XXX: Later, add a new secpolicy_vtype_access()
 * that takes the vtype instead of a vnode, and
 * get rid of the tmpl_vxxx fake vnodes below.
 */
static int
smbfs_access_rwx(vfs_t *vfsp, int vtype, int mode, cred_t *cr)
{
	/* See the secpolicy call below. */
	static const vnode_t tmpl_vdir = { .v_type = VDIR };
	static const vnode_t tmpl_vreg = { .v_type = VREG };
	vattr_t		va;
	vnode_t		*tvp;
	struct smbmntinfo *smi = VFTOSMI(vfsp);
	int shift = 0;

	/*
	 * Build our (fabricated) vnode attributes.
	 * XXX: Could make these templates in the
	 * per-mount struct and use them here.
	 */
	bzero(&va, sizeof (va));
	va.va_mask = AT_TYPE | AT_MODE | AT_UID | AT_GID;
	va.va_type = vtype;
	va.va_mode = (vtype == VDIR) ?
	    smi->smi_args.dir_mode :
	    smi->smi_args.file_mode;
	va.va_uid = smi->smi_args.uid;
	va.va_gid = smi->smi_args.gid;

	/*
	 * Disallow write attempts on read-only file systems,
	 * unless the file is a device or fifo node.  Note:
	 * Inline vn_is_readonly and IS_DEVVP here because
	 * we may not have a vnode ptr.  Original expr. was:
	 * (mode & VWRITE) && vn_is_readonly(vp) && !IS_DEVVP(vp))
	 */
	if ((mode & VWRITE) &&
	    (vfsp->vfs_flag & VFS_RDONLY) &&
	    !(vtype == VCHR || vtype == VBLK || vtype == VFIFO))
		return (EROFS);

	/*
	 * Disallow attempts to access mandatory lock files.
	 * Similarly, expand MANDLOCK here.
	 * XXX: not sure we need this.
	 */
	if ((mode & (VWRITE | VREAD | VEXEC)) &&
	    va.va_type == VREG && MANDMODE(va.va_mode))
		return (EACCES);

	/*
	 * Access check is based on only
	 * one of owner, group, public.
	 * If not owner, then check group.
	 * If not a member of the group,
	 * then check public access.
	 */
	if (crgetuid(cr) != va.va_uid) {
		shift += 3;
		if (!groupmember(va.va_gid, cr))
			shift += 3;
	}
	mode &= ~(va.va_mode << shift);
	if (mode == 0)
		return (0);

	/*
	 * We need a vnode for secpolicy_vnode_access,
	 * but the only thing it looks at is v_type,
	 * so pass one of the templates above.
	 */
	tvp = (va.va_type == VDIR) ?
	    (vnode_t *)&tmpl_vdir :
	    (vnode_t *)&tmpl_vreg;
	return (secpolicy_vnode_access(cr, tvp, va.va_uid, mode));
}

/*
 * See smbfs_setattr
 */
static int
smbfs_accessx(void *arg, int mode, cred_t *cr)
{
	vnode_t *vp = arg;
	/*
	 * Note: The caller has checked the current zone,
	 * the SMI_DEAD and VFS_UNMOUNTED flags, etc.
	 */
	return (smbfs_access_rwx(vp->v_vfsp, vp->v_type, mode, cr));
}

/*
 * XXX
 * This op should support PSARC 2007/403, Modified Access Checks for CIFS
 */
/* ARGSUSED */
static int
smbfs_access(vnode_t *vp, int mode, int flags, cred_t *cr, caller_context_t *ct)
{
	vfs_t		*vfsp;
	smbmntinfo_t	*smi;

	vfsp = vp->v_vfsp;
	smi = VFTOSMI(vfsp);

	if (curproc->p_zone != smi->smi_zone)
		return (EIO);

	if (smi->smi_flags & SMI_DEAD || vfsp->vfs_flag & VFS_UNMOUNTED)
		return (EIO);

	return (smbfs_access_rwx(vfsp, vp->v_type, mode, cr));
}


/*
 * Flush local dirty pages to stable storage on the server.
 *
 * If FNODSYNC is specified, then there is nothing to do because
 * metadata changes are not cached on the client before being
 * sent to the server.
 *
 * Currently, this is a no-op since we don't cache data, either.
 */
/* ARGSUSED */
static int
smbfs_fsync(vnode_t *vp, int syncflag, cred_t *cr, caller_context_t *ct)
{
	int		error = 0;
	smbmntinfo_t	*smi;

	smi = VTOSMI(vp);

	if (curproc->p_zone != smi->smi_zone)
		return (EIO);

	if (smi->smi_flags & SMI_DEAD || vp->v_vfsp->vfs_flag & VFS_UNMOUNTED)
		return (EIO);

	if ((syncflag & FNODSYNC) || IS_SWAPVP(vp))
		return (0);

	return (error);
}

/*
 * Last reference to vnode went away.
 */
/* ARGSUSED */
static void
smbfs_inactive(vnode_t *vp, cred_t *cr, caller_context_t *ct)
{
	smbnode_t	*np;

	/*
	 * Don't "bail out" for VFS_UNMOUNTED here,
	 * as we want to do cleanup, etc.
	 * See also pcfs_inactive
	 */

	np = VTOSMB(vp);

	/*
	 * If this is coming from the wrong zone, we let someone in the right
	 * zone take care of it asynchronously.  We can get here due to
	 * VN_RELE() being called from pageout() or fsflush().  This call may
	 * potentially turn into an expensive no-op if, for instance, v_count
	 * gets incremented in the meantime, but it's still correct.
	 */

	/*
	 * Some paranoia from the Darwin code:
	 * Make sure the FID was closed.
	 * If we see this, it's a bug!
	 *
	 * No rw_enter here, as this should be the
	 * last ref, and we're just looking...
	 */
	if (np->n_fidrefs > 0) {
		SMBVDEBUG("opencount %d fid %d file %s\n",
		    np->n_fidrefs, np->n_fid, np->n_rpath);
	}
	if (np->n_dirrefs > 0) {
		uint_t fid = (np->n_dirseq) ?
		    np->n_dirseq->f_Sid : 0;
		SMBVDEBUG("opencount %d fid %d dir %s\n",
		    np->n_dirrefs, fid, np->n_rpath);
	}

	smb_addfree(np);
}

/*
 * Remote file system operations having to do with directory manipulation.
 */
/* ARGSUSED */
static int
smbfs_lookup(vnode_t *dvp, char *nm, vnode_t **vpp, struct pathname *pnp,
	int flags, vnode_t *rdir, cred_t *cr, caller_context_t *ct,
	int *direntflags, pathname_t *realpnp)
{
	int		error;
	smbnode_t	*dnp;
	smbmntinfo_t	*smi;

	smi = VTOSMI(dvp);

	if (curproc->p_zone != smi->smi_zone)
		return (EPERM);

	if (smi->smi_flags & SMI_DEAD || dvp->v_vfsp->vfs_flag & VFS_UNMOUNTED)
		return (EIO);

	dnp = VTOSMB(dvp);
	if (smbfs_rw_enter_sig(&dnp->r_rwlock, RW_READER, SMBINTR(dvp))) {
		error = EINTR;
		goto out;
	}

	error = smbfslookup(dvp, nm, vpp, cr, 1, ct);

	smbfs_rw_exit(&dnp->r_rwlock);

out:
	return (error);
}

/* ARGSUSED */
static int
smbfslookup(vnode_t *dvp, char *nm, vnode_t **vpp, cred_t *cr, int dnlc,
	caller_context_t *ct)
{
	int		error;
	int		supplen; /* supported length */
	vnode_t		*vp;
	smbnode_t	*dnp;
	smbmntinfo_t	*smi;
	/* struct smb_vc	*vcp; */
	const char	*name = (const char *)nm;
	int 		nmlen = strlen(nm);
	int 		rplen;
	struct smb_cred scred;
	struct smbfattr fa;

	smi = VTOSMI(dvp);
	dnp = VTOSMB(dvp);

	ASSERT(curproc->p_zone == smi->smi_zone);

#ifdef NOT_YET
	vcp = SSTOVC(smi->smi_share);

	/* XXX: Should compute this once and store it in smbmntinfo_t */
	supplen = (SMB_DIALECT(vcp) >= SMB_DIALECT_LANMAN2_0) ? 255 : 12;
#else
	supplen = 255;
#endif

	/*
	 * RWlock must be held, either reader or writer.
	 * XXX: Can we check without looking directly
	 * inside the struct smbfs_rwlock_t?
	 */
	ASSERT(dnp->r_rwlock.count != 0);

	/*
	 * If lookup is for "", just return dvp.  Don't need
	 * to send it over the wire, look it up in the dnlc,
	 * or perform any access checks.
	 */
	if (nmlen == 0) {
		VN_HOLD(dvp);
		*vpp = dvp;
		return (0);
	}

	/* if the name is longer that what is supported, return an error */
	if (nmlen > supplen)
		return (ENAMETOOLONG);

	/*
	 * Avoid surprises with characters that are
	 * illegal in Windows file names.
	 * Todo: CATIA mappings  XXX
	 */
	if (strpbrk(nm, illegal_chars))
		return (EINVAL);

	/* if the dvp is not a directory, return an error */
	if (dvp->v_type != VDIR)
		return (ENOTDIR);

	/* Need search permission in the directory. */
	error = smbfs_access(dvp, VEXEC, 0, cr, ct);
	if (error)
		return (error);

	/*
	 * If lookup is for ".", just return dvp.  Don't need
	 * to send it over the wire or look it up in the dnlc,
	 * just need to check access (done above).
	 */
	if (nmlen == 1 && name[0] == '.') {
		VN_HOLD(dvp);
		*vpp = dvp;
		return (0);
	}

#ifdef NOT_YET
	if (dnlc) {
	/*
	 * NOTE: search the dnlc here
	 */
	}
#endif

	/*
	 * Handle lookup of ".." which is quite tricky,
	 * because the protocol gives us little help.
	 *
	 * We keep full pathnames (as seen on the server)
	 * so we can just trim off the last component to
	 * get the full pathname of the parent.  Note:
	 * We don't actually copy and modify, but just
	 * compute the trimmed length and pass that with
	 * the current dir path (not null terminated).
	 *
	 * We don't go over-the-wire to get attributes
	 * for ".." because we know it's a directory,
	 * and we can just leave the rest "stale"
	 * until someone does a getattr.
	 */
	if (nmlen == 2 && name[0] == '.' && name[1] == '.') {
		if (dvp->v_flag & VROOT) {
			/*
			 * Already at the root.  This can happen
			 * with directory listings at the root,
			 * which lookup "." and ".." to get the
			 * inode numbers.  Let ".." be the same
			 * as "." in the FS root.
			 */
			VN_HOLD(dvp);
			*vpp = dvp;
			return (0);
		}

		/*
		 * Find the parent path length.
		 */
		rplen = dnp->n_rplen;
		ASSERT(rplen > 0);
		while (--rplen >= 0) {
			if (dnp->n_rpath[rplen] == '\\')
				break;
		}
		if (rplen == 0) {
			/* Found our way to the root. */
			vp = SMBTOV(smi->smi_root);
			VN_HOLD(vp);
			*vpp = vp;
			return (0);
		}
		vp = smbfs_make_node(dvp->v_vfsp,
		    dnp->n_rpath, rplen,
		    NULL, 0, NULL);
		if (vp == NULL) {
			return (ENOENT);
		}
		vp->v_type = VDIR;

		/* Success! */
		*vpp = vp;
		return (0);
	}

	/*
	 * Normal lookup of a child node.
	 * Note we handled "." and ".." above.
	 *
	 * First, go over-the-wire to get the
	 * node type (and attributes).
	 */
	smb_credinit(&scred, curproc, cr);
	/* Note: this can allocate a new "name" */
	error = smbfs_smb_lookup(dnp, &name, &nmlen, &fa, &scred);
	smb_credrele(&scred);
	if (error)
		goto out;

	/*
	 * Find or create the node.
	 */
	error = smbfs_nget(dvp, name, nmlen, &fa, &vp);
	if (error)
		goto out;

	/* Success! */
	*vpp = vp;

out:
	/* smbfs_smb_lookup may have allocated name. */
	if (name != nm)
		smbfs_name_free(name, nmlen);

	return (error);
}

/*
 * XXX
 * vsecattr_t is new to build 77, and we need to eventually support
 * it in order to create an ACL when an object is created.
 *
 * This op should support the new FIGNORECASE flag for case-insensitive
 * lookups, per PSARC 2007/244.
 */
/* ARGSUSED */
static int
smbfs_create(vnode_t *dvp, char *nm, struct vattr *va, enum vcexcl exclusive,
	int mode, vnode_t **vpp, cred_t *cr, int lfaware, caller_context_t *ct,
	vsecattr_t *vsecp)
{
	int		error;
	int		cerror;
	vfs_t		*vfsp;
	vnode_t		*vp;
#ifdef NOT_YET
	smbnode_t	*np;
#endif
	smbnode_t	*dnp;
	smbmntinfo_t	*smi;
	struct vattr	vattr;
	struct smbfattr	fattr;
	struct smb_cred	scred;
	const char *name = (const char *)nm;
	int		nmlen = strlen(nm);
	uint32_t	disp;
	uint16_t	fid;

	vfsp = dvp->v_vfsp;
	smi = VFTOSMI(vfsp);
	dnp = VTOSMB(dvp);
	vp = NULL;

	if (curproc->p_zone != smi->smi_zone)
		return (EPERM);

	if (smi->smi_flags & SMI_DEAD || vfsp->vfs_flag & VFS_UNMOUNTED)
		return (EIO);

	/*
	 * Note: this may break mknod(2) calls to create a directory,
	 * but that's obscure use.  Some other filesystems do this.
	 * XXX: Later, redirect VDIR type here to _mkdir.
	 */
	if (va->va_type != VREG)
		return (EINVAL);

	/*
	 * If the pathname is "", just use dvp, no checks.
	 * Do this outside of the rwlock (like zfs).
	 */
	if (nmlen == 0) {
		VN_HOLD(dvp);
		*vpp = dvp;
		return (0);
	}

	/* Don't allow "." or ".." through here. */
	if ((nmlen == 1 && name[0] == '.') ||
	    (nmlen == 2 && name[0] == '.' && name[1] == '.'))
		return (EISDIR);

	/*
	 * We make a copy of the attributes because the caller does not
	 * expect us to change what va points to.
	 */
	vattr = *va;

	if (smbfs_rw_enter_sig(&dnp->r_rwlock, RW_WRITER, SMBINTR(dvp)))
		return (EINTR);
	smb_credinit(&scred, curproc, cr);

	/*
	 * XXX: Do we need r_lkserlock too?
	 * No use of any shared fid or fctx...
	 */

	/*
	 * NFS needs to go over the wire, just to be sure whether the
	 * file exists or not.  Using the DNLC can be dangerous in
	 * this case when making a decision regarding existence.
	 *
	 * The SMB protocol does NOT really need to go OTW here
	 * thanks to the expressive NTCREATE disposition values.
	 * Unfortunately, to do Unix access checks correctly,
	 * we need to know if the object already exists.
	 * When the object does not exist, we need VWRITE on
	 * the directory.  Note: smbfslookup() checks VEXEC.
	 */
	error = smbfslookup(dvp, nm, &vp, cr, 0, ct);
	if (error == 0) {
		/*
		 * file already exists
		 */
		if (exclusive == EXCL) {
			error = EEXIST;
			goto out;
		}
		/*
		 * Verify requested access.
		 */
		error = smbfs_access(vp, mode, 0, cr, ct);
		if (error)
			goto out;

		/*
		 * Truncate (if requested).
		 */
		if ((vattr.va_mask & AT_SIZE) && vattr.va_size == 0) {
			vattr.va_mask = AT_SIZE;
			error = smbfssetattr(vp, &vattr, 0, cr);
			if (error)
				goto out;
		}
		/* Success! */
#ifdef NOT_YET
		vnevent_create(vp, ct);
#endif
		*vpp = vp;
		goto out;
	}

	/*
	 * The file did not exist.  Need VWRITE in the directory.
	 */
	error = smbfs_access(dvp, VWRITE, 0, cr, ct);
	if (error)
		goto out;

	/*
	 * Now things get tricky.  We also need to check the
	 * requested open mode against the file we may create.
	 * See comments at smbfs_access_rwx
	 */
	error = smbfs_access_rwx(vfsp, VREG, mode, cr);
	if (error)
		goto out;

#ifdef NOT_YET
	/* remove the entry from the negative entry from the dnlc */
	dnlc_remove(dvp, name);
#endif

	/*
	 * Now the code derived from Darwin,
	 * but with greater use of NT_CREATE
	 * disposition options.  Much changed.
	 *
	 * Create (or open) a new child node.
	 * Note we handled "." and ".." above.
	 */

	if (exclusive == EXCL)
		disp = NTCREATEX_DISP_CREATE;
	else {
		/* Truncate regular files if requested. */
		if ((va->va_type == VREG) &&
		    (va->va_mask & AT_SIZE) &&
		    (va->va_size == 0))
			disp = NTCREATEX_DISP_OVERWRITE_IF;
		else
			disp = NTCREATEX_DISP_OPEN_IF;
	}
	error = smbfs_smb_create(dnp, name, nmlen, &scred, &fid, disp, 0);
	if (error)
		goto out;

	/*
	 * XXX: Missing some code here to deal with
	 * the case where we opened an existing file,
	 * it's size is larger than 32-bits, and we're
	 * setting the size from a process that's not
	 * aware of large file offsets.  i.e.
	 * from the NFS3 code:
	 */
#if NOT_YET /* XXX */
	if ((vattr.va_mask & AT_SIZE) &&
	    vp->v_type == VREG) {
		np = VTOSMB(vp);
		/*
		 * Check here for large file handled
		 * by LF-unaware process (as
		 * ufs_create() does)
		 */
		if (!(lfaware & FOFFMAX)) {
			mutex_enter(&np->r_statelock);
			if (np->r_size > MAXOFF32_T)
				error = EOVERFLOW;
			mutex_exit(&np->r_statelock);
		}
		if (!error) {
			vattr.va_mask = AT_SIZE;
			error = smbfssetattr(vp,
			    &vattr, 0, cr);
		}
	}
#endif /* XXX */
	/*
	 * Should use the fid to get/set the size
	 * while we have it opened here.  See above.
	 */

	cerror = smbfs_smb_close(smi->smi_share, fid, NULL, &scred);
	if (cerror)
		SMBERROR("error %d closing %s\\%s\n",
		    cerror, dnp->n_rpath, name);

	/*
	 * In the open case, the name may differ a little
	 * from what we passed to create (case, etc.)
	 * so call lookup to get the (opened) name.
	 *
	 * XXX: Could avoid this extra lookup if the
	 * "createact" result from NT_CREATE says we
	 * created the object.
	 */
	error = smbfs_smb_lookup(dnp, &name, &nmlen, &fattr, &scred);
	if (error)
		goto out;

	/* update attr and directory cache */
	smbfs_attr_touchdir(dnp);

	error = smbfs_nget(dvp, name, nmlen, &fattr, &vp);
	if (error)
		goto out;

#ifdef NOT_YET
	dnlc_update(dvp, name, vp);
	/* XXX invalidate pages if we truncated? */
#endif

	/* Success! */
	*vpp = vp;
	error = 0;

out:
	smb_credrele(&scred);
	if (name != nm)
		smbfs_name_free(name, nmlen);
	smbfs_rw_exit(&dnp->r_rwlock);
	return (error);
}

/*
 * XXX
 * This op should support the new FIGNORECASE flag for case-insensitive
 * lookups, per PSARC 2007/244.
 */
/* ARGSUSED */
static int
smbfs_remove(vnode_t *dvp, char *nm, cred_t *cr, caller_context_t *ct,
	int flags)
{
	int		error;
	vnode_t		*vp;
	smbnode_t	*np;
	smbnode_t	*dnp;
	struct smb_cred	scred;
	/* enum smbfsstat status; */
	smbmntinfo_t	*smi;

	smi = VTOSMI(dvp);

	if (curproc->p_zone != smi->smi_zone)
		return (EPERM);

	if (smi->smi_flags & SMI_DEAD || dvp->v_vfsp->vfs_flag & VFS_UNMOUNTED)
		return (EIO);

	dnp = VTOSMB(dvp);
	if (smbfs_rw_enter_sig(&dnp->r_rwlock, RW_WRITER, SMBINTR(dvp)))
		return (EINTR);

	/*
	 * Verify access to the dirctory.
	 */
	error = smbfs_access(dvp, VWRITE|VEXEC, 0, cr, ct);
	if (error)
		goto out;

	/*
	 * NOTE:  the darwin code gets the "vp" passed in so it looks
	 * like the "vp" has probably been "lookup"ed by the VFS layer.
	 * It looks like we will need to lookup the vp to check the
	 * caches and check if the object being deleted is a directory.
	 */
	error = smbfslookup(dvp, nm, &vp, cr, 0, ct);
	if (error)
		goto out;

	/* Never allow link/unlink directories on CIFS. */
	if (vp->v_type == VDIR) {
		VN_RELE(vp);
		error = EPERM;
		goto out;
	}

#ifdef NOT_YET
	/*
	 * First just remove the entry from the name cache, as it
	 * is most likely the only entry for this vp.
	 */
	dnlc_remove(dvp, nm);

	/*
	 * If the file has a v_count > 1 then there may be more than one
	 * entry in the name cache due multiple links or an open file,
	 * but we don't have the real reference count so flush all
	 * possible entries.
	 */
	if (vp->v_count > 1)
		dnlc_purge_vp(vp);
#endif /* NOT_YET */

	/*
	 * Now we have the real reference count on the vnode
	 */
	np = VTOSMB(vp);
	mutex_enter(&np->r_statelock);
	if (vp->v_count > 1) {
		/*
		 * NFS does a rename on remove here.
		 * Probably not applicable for SMB.
		 * Like Darwin, just return EBUSY.
		 *
		 * XXX: Todo - Ask the server to set the
		 * set the delete-on-close flag.
		 */
		mutex_exit(&np->r_statelock);
		error = EBUSY;
		goto out;
	} else {
		mutex_exit(&np->r_statelock);

		smb_credinit(&scred, curproc, cr);
		error = smbfs_smb_delete(np, &scred, NULL, 0, 0);
		smb_credrele(&scred);

	}

	VN_RELE(vp);

out:
	smbfs_rw_exit(&dnp->r_rwlock);

	return (error);
}


/*
 * XXX
 * This op should support the new FIGNORECASE flag for case-insensitive
 * lookups, per PSARC 2007/244.
 */
/* ARGSUSED */
static int
smbfs_rename(vnode_t *odvp, char *onm, vnode_t *ndvp, char *nnm, cred_t *cr,
	caller_context_t *ct, int flags)
{
	/* vnode_t		*realvp; */

	if (curproc->p_zone != VTOSMI(odvp)->smi_zone ||
	    curproc->p_zone != VTOSMI(ndvp)->smi_zone)
		return (EPERM);

	if (VTOSMI(odvp)->smi_flags & SMI_DEAD ||
	    VTOSMI(ndvp)->smi_flags & SMI_DEAD ||
	    odvp->v_vfsp->vfs_flag & VFS_UNMOUNTED ||
	    ndvp->v_vfsp->vfs_flag & VFS_UNMOUNTED)
		return (EIO);

	return (smbfsrename(odvp, onm, ndvp, nnm, cr, ct));
}

/*
 * smbfsrename does the real work of renaming in SMBFS
 */
/* ARGSUSED */
static int
smbfsrename(vnode_t *odvp, char *onm, vnode_t *ndvp, char *nnm, cred_t *cr,
	caller_context_t *ct)
{
	int		error;
	int		nvp_locked = 0;
	vnode_t		*nvp = NULL;
	vnode_t		*ovp = NULL;
	smbnode_t	*onp;
	smbnode_t	*odnp;
	smbnode_t	*ndnp;
	struct smb_cred	scred;
	/* enum smbfsstat	status; */

	ASSERT(curproc->p_zone == VTOSMI(odvp)->smi_zone);

	if (strcmp(onm, ".") == 0 || strcmp(onm, "..") == 0 ||
	    strcmp(nnm, ".") == 0 || strcmp(nnm, "..") == 0)
		return (EINVAL);

	/*
	 * Check that everything is on the same filesystem.
	 * vn_rename checks the fsid's, but in case we don't
	 * fill those in correctly, check here too.
	 */
	if (odvp->v_vfsp != ndvp->v_vfsp)
		return (EXDEV);

	odnp = VTOSMB(odvp);
	ndnp = VTOSMB(ndvp);

	/*
	 * Avoid deadlock here on old vs new directory nodes
	 * by always taking the locks in order of address.
	 * The order is arbitrary, but must be consistent.
	 */
	if (odnp < ndnp) {
		if (smbfs_rw_enter_sig(&odnp->r_rwlock, RW_WRITER,
		    SMBINTR(odvp)))
			return (EINTR);
		if (smbfs_rw_enter_sig(&ndnp->r_rwlock, RW_WRITER,
		    SMBINTR(ndvp))) {
			smbfs_rw_exit(&odnp->r_rwlock);
			return (EINTR);
		}
	} else {
		if (smbfs_rw_enter_sig(&ndnp->r_rwlock, RW_WRITER,
		    SMBINTR(ndvp)))
			return (EINTR);
		if (smbfs_rw_enter_sig(&odnp->r_rwlock, RW_WRITER,
		    SMBINTR(odvp))) {
			smbfs_rw_exit(&ndnp->r_rwlock);
			return (EINTR);
		}
	}
	/*
	 * No returns after this point (goto out)
	 */

	/*
	 * Need write access on source and target.
	 * Server takes care of most checks.
	 */
	error = smbfs_access(odvp, VWRITE|VEXEC, 0, cr, ct);
	if (error)
		goto out;
	if (odvp != ndvp) {
		error = smbfs_access(ndvp, VWRITE, 0, cr, ct);
		if (error)
			goto out;
	}

	/*
	 * Lookup the source name.  Must already exist.
	 */
	error = smbfslookup(odvp, onm, &ovp, cr, 0, ct);
	if (error)
		goto out;

	/*
	 * Lookup the target file.  If it exists, it needs to be
	 * checked to see whether it is a mount point and whether
	 * it is active (open).
	 */
	error = smbfslookup(ndvp, nnm, &nvp, cr, 0, ct);
	if (!error) {
		/*
		 * Target (nvp) already exists.  Check that it
		 * has the same type as the source.  The server
		 * will check this also, (and more reliably) but
		 * this lets us return the correct error codes.
		 */
		if (ovp->v_type == VDIR) {
			if (nvp->v_type != VDIR) {
				error = ENOTDIR;
				goto out;
			}
		} else {
			if (nvp->v_type == VDIR) {
				error = EISDIR;
				goto out;
			}
		}

		/*
		 * POSIX dictates that when the source and target
		 * entries refer to the same file object, rename
		 * must do nothing and exit without error.
		 */
		if (ovp == nvp) {
			error = 0;
			goto out;
		}

		/*
		 * Also must ensure the target is not a mount point,
		 * and keep mount/umount away until we're done.
		 */
		if (vn_vfsrlock(nvp)) {
			error = EBUSY;
			goto out;
		}
		nvp_locked = 1;
		if (vn_mountedvfs(nvp) != NULL) {
			error = EBUSY;
			goto out;
		}

#ifdef NOT_YET
		/*
		 * Purge the name cache of all references to this vnode
		 * so that we can check the reference count to infer
		 * whether it is active or not.
		 */
		/*
		 * First just remove the entry from the name cache, as it
		 * is most likely the only entry for this vp.
		 */
		dnlc_remove(ndvp, nnm);
		/*
		 * If the file has a v_count > 1 then there may be more
		 * than one entry in the name cache due multiple links
		 * or an open file, but we don't have the real reference
		 * count so flush all possible entries.
		 */
		if (nvp->v_count > 1)
			dnlc_purge_vp(nvp);
#endif

		if (nvp->v_count > 1 && nvp->v_type != VDIR) {
			/*
			 * The target file exists, is not the same as
			 * the source file, and is active.  Other FS
			 * implementations unlink the target here.
			 * For SMB, we don't assume we can remove an
			 * open file.  Return an error instead.
			 * Darwin returned an error here too.
			 */
			error = EEXIST;
			goto out;
		}
	} /* nvp */

#ifdef NOT_YET
	dnlc_remove(odvp, onm);
	dnlc_remove(ndvp, nnm);
#endif

	onp = VTOSMB(ovp);
	smb_credinit(&scred, curproc, cr);
	error = smbfs_smb_rename(onp, ndnp, nnm, strlen(nnm), &scred);
	smb_credrele(&scred);


out:
	if (nvp) {
		if (nvp_locked)
			vn_vfsunlock(nvp);
		VN_RELE(nvp);
	}
	if (ovp)
		VN_RELE(ovp);

	smbfs_rw_exit(&odnp->r_rwlock);
	smbfs_rw_exit(&ndnp->r_rwlock);

	return (error);
}

/*
 * XXX
 * vsecattr_t is new to build 77, and we need to eventually support
 * it in order to create an ACL when an object is created.
 *
 * This op should support the new FIGNORECASE flag for case-insensitive
 * lookups, per PSARC 2007/244.
 */
/* ARGSUSED */
static int
smbfs_mkdir(vnode_t *dvp, char *nm, struct vattr *va, vnode_t **vpp,
	cred_t *cr, caller_context_t *ct, int flags, vsecattr_t *vsecp)
{
	vnode_t		*vp;
	struct smbnode	*dnp = VTOSMB(dvp);
	struct smbmntinfo *smi = VTOSMI(dvp);
	struct smb_cred	scred;
	struct smbfattr	fattr;
	const char		*name = (const char *) nm;
	int		nmlen = strlen(name);
	int		error, hiderr;

	if (curproc->p_zone != smi->smi_zone)
		return (EPERM);

	if (smi->smi_flags & SMI_DEAD || dvp->v_vfsp->vfs_flag & VFS_UNMOUNTED)
		return (EIO);

	if ((nmlen == 1 && name[0] == '.') ||
	    (nmlen == 2 && name[0] == '.' && name[1] == '.'))
		return (EEXIST);

	if (smbfs_rw_enter_sig(&dnp->r_rwlock, RW_WRITER, SMBINTR(dvp)))
		return (EINTR);
	smb_credinit(&scred, curproc, cr);

	/*
	 * XXX: Do we need r_lkserlock too?
	 * No use of any shared fid or fctx...
	 */

	/*
	 * Require write access in the containing directory.
	 */
	error = smbfs_access(dvp, VWRITE, 0, cr, ct);
	if (error)
		goto out;

	error = smbfs_smb_mkdir(dnp, name, nmlen, &scred);
	if (error)
		goto out;

	error = smbfs_smb_lookup(dnp, &name, &nmlen, &fattr, &scred);
	if (error)
		goto out;

	smbfs_attr_touchdir(dnp);

	error = smbfs_nget(dvp, name, nmlen, &fattr, &vp);
	if (error)
		goto out;

#ifdef NOT_YET
	dnlc_update(dvp, name, vp);
#endif

	if (name[0] == '.')
		if ((hiderr = smbfs_smb_hideit(VTOSMB(vp), NULL, 0, &scred)))
			SMBVDEBUG("hide failure %d\n", hiderr);

	/* Success! */
	*vpp = vp;
	error = 0;
out:
	smb_credrele(&scred);
	smbfs_rw_exit(&dnp->r_rwlock);

	if (name != nm)
		smbfs_name_free(name, nmlen);

	return (error);
}

/*
 * XXX
 * This op should support the new FIGNORECASE flag for case-insensitive
 * lookups, per PSARC 2007/244.
 */
/* ARGSUSED */
static int
smbfs_rmdir(vnode_t *dvp, char *nm, vnode_t *cdir, cred_t *cr,
	caller_context_t *ct, int flags)
{
	vnode_t		*vp = NULL;
	int		vp_locked = 0;
	struct smbmntinfo *smi = VTOSMI(dvp);
	struct smbnode	*dnp = VTOSMB(dvp);
	struct smbnode	*np;
	struct smb_cred	scred;
	int		error;

	if (curproc->p_zone != smi->smi_zone)
		return (EPERM);

	if (smi->smi_flags & SMI_DEAD || dvp->v_vfsp->vfs_flag & VFS_UNMOUNTED)
		return (EIO);

	if (smbfs_rw_enter_sig(&dnp->r_rwlock, RW_WRITER, SMBINTR(dvp)))
		return (EINTR);
	smb_credinit(&scred, curproc, cr);

	/*
	 * Require w/x access in the containing directory.
	 * Server handles all other access checks.
	 */
	error = smbfs_access(dvp, VEXEC|VWRITE, 0, cr, ct);
	if (error)
		goto out;

	/*
	 * First lookup the entry to be removed.
	 */
	error = smbfslookup(dvp, nm, &vp, cr, 0, ct);
	if (error)
		goto out;
	np = VTOSMB(vp);

	/*
	 * Disallow rmdir of "." or current dir, or the FS root.
	 * Also make sure it's a directory, not a mount point,
	 * and lock to keep mount/umount away until we're done.
	 */
	if ((vp == dvp) || (vp == cdir) || (vp->v_flag & VROOT)) {
		error = EINVAL;
		goto out;
	}
	if (vp->v_type != VDIR) {
		error = ENOTDIR;
		goto out;
	}
	if (vn_vfsrlock(vp)) {
		error = EBUSY;
		goto out;
	}
	vp_locked = 1;
	if (vn_mountedvfs(vp) != NULL) {
		error = EBUSY;
		goto out;
	}

	error = smbfs_smb_rmdir(np, &scred);
	if (error)
		goto out;

	mutex_enter(&np->r_statelock);
	dnp->n_flag |= NMODIFIED;
	mutex_exit(&np->r_statelock);
	smbfs_attr_touchdir(dnp);
#ifdef NOT_YET
	dnlc_remove(dvp, nm);
	dnlc_purge_vp(vp);
#endif
	smb_rmhash(np);

out:
	if (vp) {
		if (vp_locked)
			vn_vfsunlock(vp);
		VN_RELE(vp);
	}
	smb_credrele(&scred);
	smbfs_rw_exit(&dnp->r_rwlock);

	return (error);
}


/* ARGSUSED */
static int
smbfs_readdir(vnode_t *vp, struct uio *uiop, cred_t *cr, int *eofp,
	caller_context_t *ct, int flags)
{
	struct smbnode	*np = VTOSMB(vp);
	int		error = 0;
	smbmntinfo_t	*smi;

	smi = VTOSMI(vp);

	if (curproc->p_zone != smi->smi_zone)
		return (EIO);

	if (smi->smi_flags & SMI_DEAD || vp->v_vfsp->vfs_flag & VFS_UNMOUNTED)
		return (EIO);

	/*
	 * Require read access in the directory.
	 */
	error = smbfs_access(vp, VREAD, 0, cr, ct);
	if (error)
		return (error);

	ASSERT(smbfs_rw_lock_held(&np->r_rwlock, RW_READER));

	/*
	 * XXX: Todo readdir cache here
	 * Note: NFS code is just below this.
	 *
	 * I am serializing the entire readdir opreation
	 * now since we have not yet implemented readdir
	 * cache. This fix needs to be revisited once
	 * we implement readdir cache.
	 */
	if (smbfs_rw_enter_sig(&np->r_lkserlock, RW_WRITER, SMBINTR(vp)))
		return (EINTR);

	error = smbfs_readvdir(vp, uiop, cr, eofp, ct);

	smbfs_rw_exit(&np->r_lkserlock);

	return (error);
}

/* ARGSUSED */
static int
smbfs_readvdir(vnode_t *vp, uio_t *uio, cred_t *cr, int *eofp,
	caller_context_t *ct)
{
	size_t		dbufsiz;
	struct dirent64 *dp;
	struct smb_cred scred;
	vnode_t		*newvp;
	struct smbnode	*np = VTOSMB(vp);
	int		nmlen, reclen, error = 0;
	long		offset, limit;
	struct smbfs_fctx *ctx;

	ASSERT(curproc->p_zone == VTOSMI(vp)->smi_zone);

	/* Make sure we serialize for n_dirseq use. */
	ASSERT(smbfs_rw_lock_held(&np->r_lkserlock, RW_WRITER));

	/* Min size is DIRENT64_RECLEN(256) rounded up. */
	if (uio->uio_resid < 512 || uio->uio_offset < 0)
		return (EINVAL);

	/*
	 * This dnlc_purge_vp ensures that name cache for this dir will be
	 * current - it'll only have the items for which the smbfs_nget
	 * MAKEENTRY happened.
	 */
#ifdef NOT_YET
	if (smbfs_fastlookup)
		dnlc_purge_vp(vp);
#endif
	SMBVDEBUG("dirname='%s'\n", np->n_rpath);
	smb_credinit(&scred, curproc, cr);
	dbufsiz = DIRENT64_RECLEN(MAXNAMELEN);
	dp = kmem_alloc(dbufsiz, KM_SLEEP);

	offset = uio->uio_offset; /* NB: "cookie" */
	limit = uio->uio_resid / DIRENT64_RECLEN(1);
	SMBVDEBUG("offset=0x%ld, limit=0x%ld\n", offset, limit);

	if (offset == 0) {
		/* Don't know EOF until findclose */
		np->n_direof = -1;
	} else if (offset == np->n_direof) {
		/* Arrived at end of directory. */
		goto out;
	}

	/*
	 * Generate the "." and ".." entries here so we can
	 * (1) make sure they appear (but only once), and
	 * (2) deal with getting their I numbers which the
	 * findnext below does only for normal names.
	 */
	while (limit && offset < 2) {
		limit--;
		reclen = DIRENT64_RECLEN(offset + 1);
		bzero(dp, reclen);
		/*LINTED*/
		dp->d_reclen = reclen;
		/* Tricky: offset 0 is ".", offset 1 is ".." */
		dp->d_name[0] = '.';
		dp->d_name[1] = '.';
		dp->d_name[offset + 1] = '\0';
		/*
		 * Want the real I-numbers for the "." and ".."
		 * entries.  For these two names, we know that
		 * smbfslookup can do this all locally.
		 */
		error = smbfslookup(vp, dp->d_name, &newvp, cr, 1, ct);
		if (error) {
			dp->d_ino = np->n_ino + offset; /* fiction */
		} else {
			dp->d_ino = VTOSMB(newvp)->n_ino;
			VN_RELE(newvp);
		}
		dp->d_off = offset + 1;  /* see d_off below */
		error = uiomove(dp, dp->d_reclen, UIO_READ, uio);
		if (error)
			goto out;
		uio->uio_offset = ++offset;
	}
	if (limit == 0)
		goto out;
	if (offset != np->n_dirofs || np->n_dirseq == NULL) {
		SMBVDEBUG("Reopening search %ld:%ld\n", offset, np->n_dirofs);
		if (np->n_dirseq) {
			(void) smbfs_smb_findclose(np->n_dirseq, &scred);
			np->n_dirseq = NULL;
		}
		np->n_dirofs = 2;
		error = smbfs_smb_findopen(np, "*", 1,
		    SMB_FA_SYSTEM | SMB_FA_HIDDEN | SMB_FA_DIR,
		    &scred, &ctx);
		if (error) {
			SMBVDEBUG("can not open search, error = %d", error);
			goto out;
		}
		np->n_dirseq = ctx;
	} else
		ctx = np->n_dirseq;
	while (np->n_dirofs < offset) {
		if (smbfs_smb_findnext(ctx, offset - np->n_dirofs++,
		    &scred) != 0) {
			(void) smbfs_smb_findclose(np->n_dirseq, &scred);
			np->n_dirseq = NULL;
			np->n_direof = np->n_dirofs;
			np->n_dirofs = 0;
			*eofp = 1;
			error = 0;
			goto out;
		}
	}
	error = 0;
	for (; limit; limit--) {
		error = smbfs_smb_findnext(ctx, limit, &scred);
		if (error) {
			if (error == EBADRPC)
				error = ENOENT;
			(void) smbfs_smb_findclose(np->n_dirseq, &scred);
			np->n_dirseq = NULL;
			np->n_direof = np->n_dirofs;
			np->n_dirofs = 0;
			*eofp = 1;
			error = 0;
			break;
		}
		np->n_dirofs++;
		/* Sanity check the name length. */
		nmlen = ctx->f_nmlen;
		if (nmlen > (MAXNAMELEN - 1)) {
			nmlen = MAXNAMELEN - 1;
			SMBVDEBUG("Truncating name: %s\n", ctx->f_name);
		}
		reclen = DIRENT64_RECLEN(nmlen);
		if (uio->uio_resid < reclen)
			break;
		bzero(dp, reclen);
		/*LINTED*/
		dp->d_reclen = reclen;
		dp->d_ino = ctx->f_attr.fa_ino;
		/*
		 * Note: d_off is the offset that a user-level program
		 * should seek to for reading the _next_ directory entry.
		 * See libc: readdir, telldir, seekdir
		 */
		dp->d_off = offset + 1;
		bcopy(ctx->f_name, dp->d_name, nmlen);
		dp->d_name[nmlen] = '\0';
#ifdef NOT_YET
		if (smbfs_fastlookup) {
			if (smbfs_nget(vp, ctx->f_name,
			    ctx->f_nmlen, &ctx->f_attr, &newvp) == 0)
				VN_RELE(newvp);
		}
#endif /* NOT_YET */
		error = uiomove(dp, dp->d_reclen, UIO_READ, uio);
		if (error)
			break;
		uio->uio_offset = ++offset;
	}
	if (error == ENOENT)
		error = 0;
out:
	kmem_free(dp, dbufsiz);
	smb_credrele(&scred);
	return (error);
}


/*
 * The pair of functions VOP_RWLOCK, VOP_RWUNLOCK
 * are optional functions that are called by:
 *    getdents, before/after VOP_READDIR
 *    pread, before/after ... VOP_READ
 *    pwrite, before/after ... VOP_WRITE
 *    (other places)
 *
 * Careful here: None of the above check for any
 * error returns from VOP_RWLOCK / VOP_RWUNLOCK!
 * In fact, the return value from _rwlock is NOT
 * an error code, but V_WRITELOCK_TRUE / _FALSE.
 *
 * Therefore, it's up to _this_ code to make sure
 * the lock state remains balanced, which means
 * we can't "bail out" on interrupts, etc.
 */

/* ARGSUSED2 */
static int
smbfs_rwlock(vnode_t *vp, int write_lock, caller_context_t *ctp)
{
	smbnode_t	*np = VTOSMB(vp);

	if (!write_lock) {
		(void) smbfs_rw_enter_sig(&np->r_rwlock, RW_READER, FALSE);
		return (V_WRITELOCK_FALSE);
	}


	(void) smbfs_rw_enter_sig(&np->r_rwlock, RW_WRITER, FALSE);
	return (V_WRITELOCK_TRUE);
}

/* ARGSUSED */
static void
smbfs_rwunlock(vnode_t *vp, int write_lock, caller_context_t *ctp)
{
	smbnode_t	*np = VTOSMB(vp);

	smbfs_rw_exit(&np->r_rwlock);
}


/* ARGSUSED */
static int
smbfs_seek(vnode_t *vp, offset_t ooff, offset_t *noffp, caller_context_t *ct)
{
	smbmntinfo_t	*smi;

	smi = VTOSMI(vp);

	if (curproc->p_zone != smi->smi_zone)
		return (EPERM);

	if (smi->smi_flags & SMI_DEAD || vp->v_vfsp->vfs_flag & VFS_UNMOUNTED)
		return (EIO);

	/*
	 * Because we stuff the readdir cookie into the offset field
	 * someone may attempt to do an lseek with the cookie which
	 * we want to succeed.
	 */
	if (vp->v_type == VDIR)
		return (0);

	/* Like NFS3, just check for 63-bit overflow. */
	if (*noffp < 0)
		return (EINVAL);

	return (0);
}


/*
 * XXX
 * This op may need to support PSARC 2007/440, nbmand changes for CIFS Service.
 */
static int
smbfs_frlock(vnode_t *vp, int cmd, struct flock64 *bfp, int flag,
	offset_t offset, struct flk_callback *flk_cbp, cred_t *cr,
	caller_context_t *ct)
{
	if (curproc->p_zone != VTOSMI(vp)->smi_zone)
		return (EIO);

	if (VTOSMI(vp)->smi_flags & SMI_LLOCK)
		return (fs_frlock(vp, cmd, bfp, flag, offset, flk_cbp, cr, ct));
	else
		return (ENOSYS);
}

/*
 * Free storage space associated with the specified vnode.  The portion
 * to be freed is specified by bfp->l_start and bfp->l_len (already
 * normalized to a "whence" of 0).
 *
 * Called by fcntl(fd, F_FREESP, lkp) for libc:ftruncate, etc.
 */
/* ARGSUSED */
static int
smbfs_space(vnode_t *vp, int cmd, struct flock64 *bfp, int flag,
	offset_t offset, cred_t *cr, caller_context_t *ct)
{
	int		error;
	smbmntinfo_t	*smi;

	smi = VTOSMI(vp);

	if (curproc->p_zone != smi->smi_zone)
		return (EIO);

	if (smi->smi_flags & SMI_DEAD || vp->v_vfsp->vfs_flag & VFS_UNMOUNTED)
		return (EIO);

	ASSERT(vp->v_type == VREG);
	if (cmd != F_FREESP)
		return (EINVAL);

	/*
	 * Like NFS3, no 32-bit offset checks here.
	 * Our SMB layer takes care to return EFBIG
	 * when it has to fallback to a 32-bit call.
	 */

	error = convoff(vp, bfp, 0, offset);
	if (!error) {
		ASSERT(bfp->l_start >= 0);
		if (bfp->l_len == 0) {
			struct vattr va;

			/*
			 * ftruncate should not change the ctime and
			 * mtime if we truncate the file to its
			 * previous size.
			 */
			va.va_mask = AT_SIZE;
			error = smbfsgetattr(vp, &va, cr);
			if (error || va.va_size == bfp->l_start)
				return (error);
			va.va_mask = AT_SIZE;
			va.va_size = bfp->l_start;
			error = smbfssetattr(vp, &va, 0, cr);
		} else
			error = EINVAL;
	}

	return (error);
}

/* ARGSUSED */
static int
smbfs_pathconf(vnode_t *vp, int cmd, ulong_t *valp, cred_t *cr,
	caller_context_t *ct)
{
	smbmntinfo_t *smi;
	struct smb_share *ssp;

	smi = VTOSMI(vp);

	if (curproc->p_zone != smi->smi_zone)
		return (EIO);

	if (smi->smi_flags & SMI_DEAD || vp->v_vfsp->vfs_flag & VFS_UNMOUNTED)
		return (EIO);

	switch (cmd) {
	case _PC_FILESIZEBITS:
		ssp = smi->smi_share;
		if (SSTOVC(ssp)->vc_sopt.sv_caps & SMB_CAP_LARGE_FILES)
			*valp = 64;
		else
			*valp = 32;
		break;

	case _PC_LINK_MAX:
		/* We only ever report one link to an object */
		*valp = 1;
		break;

	case _PC_SYMLINK_MAX:	/* No symlinks until we do Unix extensions */
	case _PC_ACL_ENABLED:	/* No ACLs yet - see FILE_PERSISTENT_ACLS bit */
	case _PC_XATTR_EXISTS:	/* No xattrs yet */
		*valp = 0;
		break;

	default:
		return (fs_pathconf(vp, cmd, valp, cr, ct));
	}
	return (0);
}



/*
 * XXX
 * This op should eventually support PSARC 2007/268.
 */
static int
smbfs_shrlock(vnode_t *vp, int cmd, struct shrlock *shr, int flag, cred_t *cr,
	caller_context_t *ct)
{
	if (curproc->p_zone != VTOSMI(vp)->smi_zone)
		return (EIO);

	if (VTOSMI(vp)->smi_flags & SMI_LLOCK)
		return (fs_shrlock(vp, cmd, shr, flag, cr, ct));
	else
		return (ENOSYS);
}
