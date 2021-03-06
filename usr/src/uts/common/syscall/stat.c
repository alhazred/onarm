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

/*	Copyright (c) 1983, 1984, 1985, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*
 * Portions of this source code were derived from Berkeley 4.3 BSD
 * under license from the Regents of the University of California.
 */

/*
 * Copyright (c) 2007-2008 NEC Corporation
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * Get file attribute information through a file name or a file descriptor.
 */

#include <sys/param.h>
#include <sys/isa_defs.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/cred.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/pathname.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/mode.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <c2/audit.h>
#include <fs/fs_subr.h>

/*
 * Get the vp to be stated and the cred to be used for the call
 * to VOP_GETATTR
 */

/*
 * nmflag has the following values
 *
 * 1 - Always do lookup.  i.e. stat, lstat.
 * 2 - Name is optional i.e. fstatat
 * 0 - Don't lookup name, vp is in file_p. i.e. fstat
 *
 */
static int
cstatat_getvp(int fd, char *name, int nmflag,
    int follow, vnode_t **vp, cred_t **cred)
{
	vnode_t *startvp;
	file_t *fp;
	int error;
	cred_t *cr;
	int estale_retry = 0;

	*vp = NULL;

	/*
	 * Only return EFAULT for fstatat when fd == AT_FDCWD && name == NULL
	 */

	if (fd == AT_FDCWD) {
		if (name != NULL || nmflag != 2) {
			startvp = NULL;
			cr = CRED();
			crhold(cr);
		} else
			return (EFAULT);
	} else {
		char startchar;

		if (nmflag == 1 || (nmflag == 2 && name != NULL)) {
			if (copyin(name, &startchar, sizeof (char)))
				return (EFAULT);
		} else {
			startchar = '\0';
		}
		if (startchar != '/' || nmflag == 0) {
			if ((fp = getf(fd)) == NULL) {
				return (EBADF);
			}
			startvp = fp->f_vnode;
			cr = fp->f_cred;
			crhold(cr);
			VN_HOLD(startvp);
			releasef(fd);
		} else {
			startvp = NULL;
			cr = CRED();
			crhold(cr);
		}
	}
	*cred = cr;

	if (audit_active)
		audit_setfsat_path(1);


	if (nmflag == 1 || (nmflag == 2 && name != NULL)) {
lookup:
		if (error = lookupnameat(name, UIO_USERSPACE, follow, NULLVPP,
		    vp, startvp)) {
			if ((error == ESTALE) &&
			    fs_need_estale_retry(estale_retry++))
				goto lookup;
			if (startvp != NULL)
				VN_RELE(startvp);
			crfree(cr);
			return (error);
		}
		if (startvp != NULL)
			VN_RELE(startvp);
	} else {
		*vp = startvp;
	}

	return (0);
}

/*
 * Native syscall interfaces:
 *
 * N-bit kernel, N-bit applications, N-bit file offsets
 */

static int cstatat(int, char *, int, struct stat *, int, int);
static int cstat(vnode_t *vp, struct stat *, int, cred_t *);

int
stat(char *fname, struct stat *sb)
{
	return (cstatat(AT_FDCWD, fname, 1, sb, 0, ATTR_REAL));
}

int
lstat(char *fname, struct stat *sb)
{
	return (cstatat(AT_FDCWD, fname, 1, sb, AT_SYMLINK_NOFOLLOW, 0));
}

/*
 * fstat can and should be fast, do an inline implementation here.
 */
#define	FSTAT_BODY(fd, sb, statfn)				\
	{							\
		file_t *fp;					\
		int error;					\
								\
		if ((fp = getf(fd)) == NULL)			\
			return (set_errno(EBADF));		\
		if (audit_active)				\
			audit_setfsat_path(1);			\
		error = statfn(fp->f_vnode, sb, 0, fp->f_cred);	\
		releasef(fd);					\
		if (error)					\
			return (set_errno(error));		\
		return (0);					\
	}

int
fstat(int fd, struct stat *sb)
{
	FSTAT_BODY(fd, sb, cstat)
}

int
fstatat(int fd, char *name, struct stat *sb, int flags)
{
	return (cstatat(fd, name, 2, sb,
	    flags & AT_SYMLINK_NOFOLLOW ? AT_SYMLINK_NOFOLLOW : 0,
	    flags & _AT_TRIGGER ? ATTR_TRIGGER : 0));
}

#if defined(__i386) || defined(__i386_COMPAT)

/*
 * Handle all the "extended" stat operations in the same way;
 * validate the version, then call the real handler.
 */

#define	XSTAT_BODY(ver, f, s, fn)			\
	return (ver != _STAT_VER ? set_errno(EINVAL) : fn(f, s));

#endif	/* __i386 || __i386_COMPAT */

#if defined(__i386)

/*
 * Syscalls for i386 applications that issue {,l,f}xstat() directly
 */
int
xstat(int version, char *fname, struct stat *sb)
{
	XSTAT_BODY(version, fname, sb, stat)
}

int
lxstat(int version, char *fname, struct stat *sb)
{
	XSTAT_BODY(version, fname, sb, lstat)
}

int
fxstat(int version, int fd, struct stat *sb)
{
	XSTAT_BODY(version, fd, sb, fstat)
}

#endif	/* __i386 */

/*
 * Common code for stat(), lstat(), and fstat().
 * (32-bit kernel, 32-bit applications, 32-bit files)
 * (64-bit kernel, 64-bit applications, 64-bit files)
 */
static int
cstat(vnode_t *vp, struct stat *ubp, int flag, cred_t *cr)
{
	struct vfssw *vswp;
	struct stat sb;
	vattr_t vattr;
	int error;

	vattr.va_mask = AT_STAT | AT_NBLOCKS | AT_BLKSIZE | AT_SIZE;
	if ((error = VOP_GETATTR(vp, &vattr, flag, cr, NULL)) != 0)
		return (error);
#ifdef	_ILP32
	/*
	 * (32-bit kernel, 32-bit applications, 32-bit files)
	 * NOTE: 32-bit kernel maintains a 64-bit unsigend va_size.
	 *
	 * st_size of devices (VBLK and VCHR special files) is a special case.
	 * POSIX does not define size behavior for special files, so the
	 * following Solaris specific behavior is not a violation. Solaris
	 * returns the size of the device.
	 *
	 * For compatibility with 32-bit programs which happen to do stat() on
	 * a (mknod) bigger than 2GB we suppress the large file EOVERFLOW and
	 * instead we return the value MAXOFF32_T (LONG_MAX).
	 *
	 * 32-bit applications that care about the size of devices should be
	 * built 64-bit or use a large file interface (lfcompile(5) or lf64(5)).
	 */
	if ((vattr.va_size > MAXOFF32_T) &&
	    ((vp->v_type == VBLK) || (vp->v_type == VCHR))) {
		/* OVERFLOW | UNKNOWN_SIZE */
		vattr.va_size = MAXOFF32_T;
	}
#endif	/* _ILP32 */
	if (vattr.va_size > MAXOFF_T || vattr.va_nblocks > LONG_MAX ||
	    vattr.va_nodeid > ULONG_MAX)
		return (EOVERFLOW);

	bzero(&sb, sizeof (sb));
	sb.st_dev = vattr.va_fsid;
	sb.st_ino = (ino_t)vattr.va_nodeid;
	sb.st_mode = VTTOIF(vattr.va_type) | vattr.va_mode;
	sb.st_nlink = vattr.va_nlink;
	sb.st_uid = vattr.va_uid;
	sb.st_gid = vattr.va_gid;
	sb.st_rdev = vattr.va_rdev;
	sb.st_size = (off_t)vattr.va_size;
	sb.st_atim = vattr.va_atime;
	sb.st_mtim = vattr.va_mtime;
	sb.st_ctim = vattr.va_ctime;
	sb.st_blksize = vattr.va_blksize;
	sb.st_blocks = (blkcnt_t)vattr.va_nblocks;
	if (vp->v_vfsp != NULL) {
		vswp = &vfssw[vp->v_vfsp->vfs_fstype];
		if (vswp->vsw_name && *vswp->vsw_name)
			(void) strcpy(sb.st_fstype, vswp->vsw_name);
	}
	if (copyout(&sb, ubp, sizeof (sb)))
		return (EFAULT);
	return (0);
}

static int
cstatat(int fd, char *name, int nmflag, struct stat *sb, int follow, int flags)
{
	vnode_t *vp;
	int error;
	cred_t *cred;
	int link_follow;
	int estale_retry = 0;

	link_follow = (follow == AT_SYMLINK_NOFOLLOW) ? NO_FOLLOW : FOLLOW;
lookup:
	if (error = cstatat_getvp(fd, name, nmflag, link_follow, &vp, &cred))
		return (set_errno(error));
	error = cstat(vp, sb, flags, cred);
	crfree(cred);
	VN_RELE(vp);
out:
	if (error != 0) {
		if (error == ESTALE &&
		    fs_need_estale_retry(estale_retry++) &&
		    (nmflag == 1 || (nmflag == 2 && name != NULL)))
			goto lookup;
		return (set_errno(error));
	}
	return (0);
}

#if defined(_SYSCALL32_IMPL)

/*
 * 64-bit kernel, 32-bit applications, 32-bit file offsets
 */
static int cstatat32(int, char *, int, struct stat32 *, int, int);
static int cstat32(vnode_t *, struct stat32 *, int, cred_t *);
int
stat32(char *fname, struct stat32 *sb)
{
	return (cstatat32(AT_FDCWD, fname, 1, sb, 0, ATTR_REAL));
}

int
lstat32(char *fname, struct stat32 *sb)
{
	return (cstatat32(AT_FDCWD, fname, 1, sb, AT_SYMLINK_NOFOLLOW, 0));
}

int
fstat32(int fd, struct stat32 *sb)
{
	FSTAT_BODY(fd, sb, cstat32)
}

int
fstatat32(int fd, char *name, struct stat32 *sb, int flags)
{
	return (cstatat32(fd, name, 2, sb,
	    flags & AT_SYMLINK_NOFOLLOW ? AT_SYMLINK_NOFOLLOW : 0,
	    flags & _AT_TRIGGER ? ATTR_TRIGGER : 0));
}

#if defined(__i386_COMPAT)

/*
 * Syscalls for i386 applications that issue {,l,f}xstat() directly
 */
int
xstat32(int version, char *fname, struct stat32 *sb)
{
	XSTAT_BODY(version, fname, sb, stat32)
}

int
lxstat32(int version, char *fname, struct stat32 *sb)
{
	XSTAT_BODY(version, fname, sb, lstat32)
}

int
fxstat32(int version, int fd, struct stat32 *sb)
{
	XSTAT_BODY(version, fd, sb, fstat32)
}

#endif	/* __i386_COMPAT */

static int
cstat32(vnode_t *vp, struct stat32 *ubp, int flag, struct cred *cr)
{
	struct vfssw *vswp;
	struct stat32 sb;
	vattr_t vattr;
	int error;
	dev32_t st_dev, st_rdev;

	vattr.va_mask = AT_STAT | AT_NBLOCKS | AT_BLKSIZE | AT_SIZE;
	if (error = VOP_GETATTR(vp, &vattr, flag, cr, NULL))
		return (error);

	/* devices are a special case, see comments in cstat */
	if ((vattr.va_size > MAXOFF32_T) &&
	    ((vp->v_type == VBLK) || (vp->v_type == VCHR))) {
		/* OVERFLOW | UNKNOWN_SIZE */
		vattr.va_size = MAXOFF32_T;
	}

	/* check for large values */
	if (!cmpldev(&st_dev, vattr.va_fsid) ||
	    !cmpldev(&st_rdev, vattr.va_rdev) ||
	    vattr.va_size > MAXOFF32_T ||
	    vattr.va_nblocks > INT32_MAX ||
	    vattr.va_nodeid > UINT32_MAX ||
	    TIMESPEC_OVERFLOW(&(vattr.va_atime)) ||
	    TIMESPEC_OVERFLOW(&(vattr.va_mtime)) ||
	    TIMESPEC_OVERFLOW(&(vattr.va_ctime)))
		return (EOVERFLOW);

	bzero(&sb, sizeof (sb));
	sb.st_dev = st_dev;
	sb.st_ino = (ino32_t)vattr.va_nodeid;
	sb.st_mode = VTTOIF(vattr.va_type) | vattr.va_mode;
	sb.st_nlink = vattr.va_nlink;
	sb.st_uid = vattr.va_uid;
	sb.st_gid = vattr.va_gid;
	sb.st_rdev = st_rdev;
	sb.st_size = (off32_t)vattr.va_size;
	TIMESPEC_TO_TIMESPEC32(&(sb.st_atim), &(vattr.va_atime));
	TIMESPEC_TO_TIMESPEC32(&(sb.st_mtim), &(vattr.va_mtime));
	TIMESPEC_TO_TIMESPEC32(&(sb.st_ctim), &(vattr.va_ctime));
	sb.st_blksize = vattr.va_blksize;
	sb.st_blocks = (blkcnt32_t)vattr.va_nblocks;
	if (vp->v_vfsp != NULL) {
		vswp = &vfssw[vp->v_vfsp->vfs_fstype];
		if (vswp->vsw_name && *vswp->vsw_name)
			(void) strcpy(sb.st_fstype, vswp->vsw_name);
	}
	if (copyout(&sb, ubp, sizeof (sb)))
		return (EFAULT);
	return (0);
}

static int
cstatat32(int fd, char *name, int nmflag, struct stat32 *sb,
    int follow, int flags)
{
	vnode_t *vp;
	int error;
	cred_t *cred;
	int link_follow;
	int estale_retry = 0;

	link_follow = (follow == AT_SYMLINK_NOFOLLOW) ? NO_FOLLOW : FOLLOW;
lookup:
	if (error = cstatat_getvp(fd, name, nmflag, link_follow, &vp, &cred))
		return (set_errno(error));
	error = cstat32(vp, sb, flags, cred);
	crfree(cred);
	VN_RELE(vp);
out:
	if (error != 0) {
		if (error == ESTALE &&
		    fs_need_estale_retry(estale_retry++) &&
		    (nmflag == 1 || (nmflag == 2 && name != NULL)))
			goto lookup;
		return (set_errno(error));
	}
	return (0);
}

#endif	/* _SYSCALL32_IMPL */

#if defined(_ILP32)

#if	!defined(__arm) || !defined(__ARM_EABI__)
#undef	ARM_OABI_USER
#endif	/* !defined(__arm) || !defined(__ARM_EABI__) */

#ifdef	ARM_OABI_USER

struct  stat64_oabi {
	dev_t		st_dev;
	long		st_pad1[3];	/* reserve for dev expansion, */
				/* sysid definition */
	ino64_t		st_ino;
	mode_t		st_mode;
	nlink_t		st_nlink;
	uid_t		st_uid;
	gid_t		st_gid;
	dev_t		st_rdev;
	long		st_pad2[2];
	uint32_t	st_size_lo;
	uint32_t	st_size_hi;
	timestruc_t	st_atim;
	timestruc_t	st_mtim;
	timestruc_t	st_ctim;
	blksize_t	st_blksize;
	uint32_t	st_blocks_lo;
	uint32_t	st_blocks_hi;
	char		st_fstype[_ST_FSTYPSZ];
	long		st_pad4[8];	/* expansion area */
};

#define	STAT64_SET64(sbp, member, size)					\
	do {								\
		(sbp)->member##_lo = (uint32_t)((size) & 0xffffffff);	\
		(sbp)->member##_hi = (uint32_t)(((size) >> 32) & 0xffffffff); \
	} while (0)

#else	/* !ARM_OABI_USER */

#define	STAT64_SET64(sbp, member, size)					\
	do {								\
		(sbp)->member = (size);					\
	} while (0)

#endif	/* ARM_OABI_USER */

/*
 * 32-bit kernel, 32-bit applications, 64-bit file offsets.
 *
 * These routines are implemented differently on 64-bit kernels.
 */
static int cstatat64(int, char *, int, struct stat64 *, int, int);
static int cstat64(vnode_t *, struct stat64 *, int, cred_t *);

int
stat64(char *fname, struct stat64 *sb)
{
	return (cstatat64(AT_FDCWD, fname, 1, sb, 0, ATTR_REAL));
}

int
lstat64(char *fname, struct stat64 *sb)
{
	return (cstatat64(AT_FDCWD, fname, 1, sb, AT_SYMLINK_NOFOLLOW, 0));
}

int
fstat64(int fd, struct stat64 *sb)
{
	FSTAT_BODY(fd, sb, cstat64)
}

int
fstatat64(int fd, char *name, struct stat64 *sb, int flags)
{
	return (cstatat64(fd, name, 2, sb,
	    flags & AT_SYMLINK_NOFOLLOW ? AT_SYMLINK_NOFOLLOW : 0,
	    flags & _AT_TRIGGER ? ATTR_TRIGGER : 0));
}

static int
cstat64(vnode_t *vp, struct stat64 *ubp, int flag, cred_t *cr)
{
	struct vfssw *vswp;
#ifdef	ARM_OABI_USER
	struct stat64_oabi	lsb;
#else	/* !ARM_OABI_USER */
	struct stat64 lsb;
#endif	/* ARM_OABI_USER */
	vattr_t vattr;
	int error;

	vattr.va_mask = AT_STAT | AT_NBLOCKS | AT_BLKSIZE | AT_SIZE;
	if (error = VOP_GETATTR(vp, &vattr, flag, cr, NULL))
		return (error);

	bzero(&lsb, sizeof (lsb));
	lsb.st_dev = vattr.va_fsid;
	lsb.st_ino = vattr.va_nodeid;
	lsb.st_mode = VTTOIF(vattr.va_type) | vattr.va_mode;
	lsb.st_nlink = vattr.va_nlink;
	lsb.st_uid = vattr.va_uid;
	lsb.st_gid = vattr.va_gid;
	lsb.st_rdev = vattr.va_rdev;
	STAT64_SET64(&lsb, st_size, vattr.va_size);
	lsb.st_atim = vattr.va_atime;
	lsb.st_mtim = vattr.va_mtime;
	lsb.st_ctim = vattr.va_ctime;
	lsb.st_blksize = vattr.va_blksize;
	STAT64_SET64(&lsb, st_blocks, vattr.va_nblocks);
	if (vp->v_vfsp != NULL) {
		vswp = &vfssw[vp->v_vfsp->vfs_fstype];
		if (vswp->vsw_name && *vswp->vsw_name)
			(void) strcpy(lsb.st_fstype, vswp->vsw_name);
	}
	if (copyout(&lsb, ubp, sizeof (lsb)))
		return (EFAULT);
	return (0);
}

static int
cstatat64(int fd, char *name, int nmflag, struct stat64 *sb,
    int follow, int flags)
{
	vnode_t *vp;
	int error;
	cred_t *cred;
	int link_follow;
	int estale_retry = 0;

	link_follow = (follow == AT_SYMLINK_NOFOLLOW) ? NO_FOLLOW : FOLLOW;
lookup:
	if (error = cstatat_getvp(fd, name, nmflag, link_follow, &vp, &cred))
		return (set_errno(error));
	error = cstat64(vp, sb, flags, cred);
	crfree(cred);
	VN_RELE(vp);
out:
	if (error != 0) {
		if (error == ESTALE &&
		    fs_need_estale_retry(estale_retry++) &&
		    (nmflag == 1 || (nmflag == 2 && name != NULL)))
			goto lookup;
		return (set_errno(error));
	}
	return (0);
}

#endif	/* _ILP32 */

#if defined(_SYSCALL32_IMPL)

/*
 * 64-bit kernel, 32-bit applications, 64-bit file offsets.
 *
 * We'd really like to call the "native" stat calls for these ones,
 * but the problem is that the 64-bit ABI defines the 'stat64' structure
 * differently from the way the 32-bit ABI defines it.
 */

static int cstatat64_32(int, char *, int, struct stat64_32 *, int, int);
static int cstat64_32(vnode_t *, struct stat64_32 *, int, cred_t *);

int
stat64_32(char *fname, struct stat64_32 *sb)
{
	return (cstatat64_32(AT_FDCWD, fname, 1, sb, 0, ATTR_REAL));
}

int
lstat64_32(char *fname, struct stat64_32 *sb)
{
	return (cstatat64_32(AT_FDCWD, fname, 1, sb, AT_SYMLINK_NOFOLLOW, 0));
}

int
fstat64_32(int fd, struct stat64_32 *sb)
{
	FSTAT_BODY(fd, sb, cstat64_32)
}

int
fstatat64_32(int fd, char *name, struct stat64_32 *sb, int flags)
{
	return (cstatat64_32(fd, name, 2, sb,
	    flags & AT_SYMLINK_NOFOLLOW ? AT_SYMLINK_NOFOLLOW : 0,
	    flags & _AT_TRIGGER ? ATTR_TRIGGER : 0));
}

static int
cstat64_32(vnode_t *vp, struct stat64_32 *ubp, int flag, cred_t *cr)
{
	struct vfssw *vswp;
	struct stat64_32 lsb;
	vattr_t vattr;
	int error;
	dev32_t st_dev, st_rdev;

	vattr.va_mask = AT_STAT | AT_NBLOCKS | AT_BLKSIZE | AT_SIZE;
	if (error = VOP_GETATTR(vp, &vattr, flag, cr, NULL))
		return (error);

	if (!cmpldev(&st_dev, vattr.va_fsid) ||
	    !cmpldev(&st_rdev, vattr.va_rdev) ||
	    TIMESPEC_OVERFLOW(&(vattr.va_atime)) ||
	    TIMESPEC_OVERFLOW(&(vattr.va_mtime)) ||
	    TIMESPEC_OVERFLOW(&(vattr.va_ctime)))
		return (EOVERFLOW);

	bzero(&lsb, sizeof (lsb));
	lsb.st_dev = st_dev;
	lsb.st_ino = vattr.va_nodeid;
	lsb.st_mode = VTTOIF(vattr.va_type) | vattr.va_mode;
	lsb.st_nlink = vattr.va_nlink;
	lsb.st_uid = vattr.va_uid;
	lsb.st_gid = vattr.va_gid;
	lsb.st_rdev = st_rdev;
	lsb.st_size = vattr.va_size;
	TIMESPEC_TO_TIMESPEC32(&(lsb.st_atim), &(vattr.va_atime));
	TIMESPEC_TO_TIMESPEC32(&(lsb.st_mtim), &(vattr.va_mtime));
	TIMESPEC_TO_TIMESPEC32(&(lsb.st_ctim), &(vattr.va_ctime));
	lsb.st_blksize = vattr.va_blksize;
	lsb.st_blocks = vattr.va_nblocks;
	if (vp->v_vfsp != NULL) {
		vswp = &vfssw[vp->v_vfsp->vfs_fstype];
		if (vswp->vsw_name && *vswp->vsw_name)
			(void) strcpy(lsb.st_fstype, vswp->vsw_name);
	}
	if (copyout(&lsb, ubp, sizeof (lsb)))
		return (EFAULT);
	return (0);
}

static int
cstatat64_32(int fd, char *name, int nmflag, struct stat64_32 *sb,
    int follow, int flags)
{
	vnode_t  *vp;
	int error;
	cred_t *cred;
	int link_follow;
	int estale_retry = 0;

	link_follow = (follow == AT_SYMLINK_NOFOLLOW) ? NO_FOLLOW : FOLLOW;
lookup:
	if (error = cstatat_getvp(fd, name, nmflag, link_follow, &vp, &cred))
		return (set_errno(error));
	error = cstat64_32(vp, sb, flags, cred);
	crfree(cred);
	VN_RELE(vp);
out:
	if (error != 0) {
		if (error == ESTALE &&
		    fs_need_estale_retry(estale_retry++) &&
		    (nmflag == 1 || (nmflag == 2 && name != NULL)))
			goto lookup;
		return (set_errno(error));
	}
	return (0);
}

#endif /* _SYSCALL32_IMPL */
