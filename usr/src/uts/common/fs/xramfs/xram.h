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
 * Copyright 1989-1991, 1996-1999, 2001-2003 Sun Microsystems, Inc.
 * All rights reserved.  Use is subject to license terms.
 */

/*
 * Copyright (c) 2008 NEC Corporation
 * All rights reserved.
 */

#ifndef	_XRAMFS_XRAM_H
#define	_XRAMFS_XRAM_H

#ifdef _KERNEL

#include <sys/xramdev.h>
#include <sys/vfs_opreg.h>

/* parameters for xramfs */
#define XRAM_SUPPORT_MAX_UNITSHIFT	(31U)

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * xnvnode table
 */
#if defined(XRAMFS_XVT_MUTEX_SIZE) && defined(XRAMFS_XVT_HASH_SIZE)
#define XVT_LIST_MUTEX_SIZE 	XRAMFS_XVT_MUTEX_SIZE
#define XVT_HASH_SIZE		XRAMFS_XVT_HASH_SIZE
#else
#define XVT_LIST_MUTEX_SIZE_SHIFT (5)		/* 2^5 == 32 */
#define XVT_HASH_SIZE_SHIFT (5) 		/* 2^5 == 32 */

#define XVT_LIST_MUTEX_SIZE	(1U << (XVT_LIST_MUTEX_SIZE_SHIFT))
#define XVT_HASH_SIZE		(1U << (XVT_HASH_SIZE_SHIFT))
#endif /* defined(XRAMFS_XVT_MUTEX_SIZE) && defined(XRAMFS_XVT_HASH_SIZE) */

#if XVT_LIST_MUTEX_SIZE > XVT_HASH_SIZE
#error XRAMFS_XVT_MUTEX_SIZE_must_be_equal_or smaller_than_XRAMFS_XVT_HASH_SIZE
#endif

#define XVT_LIST_MUTEX_MASK	(XVT_LIST_MUTEX_SIZE - 1)
#define XVT_HASH_MASK		(XVT_HASH_SIZE - 1)

#define XRAM_XVT_HASH(xno)	(int)((xno) & XVT_HASH_MASK)

struct xram_xnvntable {
	/* to protect xnode list on hash table */
	kmutex_t	xvt_list_mutex[XVT_LIST_MUTEX_SIZE];
	/* first entries of specified hash number */
	struct xnode	*xvt_hash[XVT_HASH_SIZE];
};
extern	void	xram_xvt_init(struct xram_xnvntable *);
extern	void	xram_xvt_destroy(struct xram_xnvntable *);

/*
 * xramfs per-mount data structure.
 *
 * All fields are protected by xm_contents.
 */
struct xmount {
	vfs_t		*xm_vfsp;	/* filesystem's vfs struct */
	struct xnode	*xm_rootnode;	/* root xnode */
	char 		*xm_mntpath;	/* name of xramfs mount point */

	/* disk volume info. */
	vnode_t		*xm_devvp;	/* device vnode */
	xramdev_t	*xm_disk;	/* ref for xramdev mounting. */
	void		*xm_kasaddr;	/* kas addr for xramdev top */
	void		*xm_kasaddr_end; /* kas addr for xramdev bottom */

	/* xnovnode table */
	struct xram_xnvntable	xm_xvt;		/* xnovnode hash table */

	/* addr for blocks and regions */
	caddr_t		xm_dir;
	caddr_t		xm_dname;
	caddr_t		xm_symlink;
	caddr_t		xm_map;
};
#define XMOUNT_XRAMHEADER(xm) ((struct xramheader *)(xm)->xm_kasaddr)

#define XNODE_MAPOFFS_TO_PFN(xm, xp, offbytes)			\
	((pfn_t)((xp)->xn_start) + ((offbytes) >> PAGESHIFT)	\
		+ XMOUNT_XRAMHEADER(xm)->xh_off_xfile)

/*
 * File system independent to xramfs conversion macros
 */
#define	VFSTOXM(vfsp)		((struct xmount *)(vfsp)->vfs_data)
#define	VTOXM(vp)		((struct xmount *)(vp)->v_vfsp->vfs_data)
#define	VTOXN(vp)		((struct xnode *)(vp)->v_data)
#define	XNTOV(xp)		((xp)->xn_vp)


/*
 * some functions and variables
 */
extern	int	xram_dirlookup(struct xmount *, struct xnode *, char *,
	struct xnode **, cred_t *);
extern	int	xram_listdir(struct xmount *, struct xnode *, uio_t *,
	int *);
extern	int	xram_xaccess(struct xmount *, struct xnode *, int,
	cred_t *);

extern struct vnodeops *xram_vnodeops;
extern const struct fs_operation_def xram_vnodeops_template[];


/* macros to probe by dtrace */
#define XRAMFS_ENABLE_DTRACE

#ifdef XRAMFS_ENABLE_DTRACE

#define XDTRACE_PROBE(n)			\
	DTRACE_PROBE(n)
#define XDTRACE_PROBE1(n,t1,a1)			\
	DTRACE_PROBE1(n,t1,a1)
#define XDTRACE_PROBE2(n,t1,a1,t2,a2)		\
	DTRACE_PROBE2(n,t1,a1,t2,a2)
#define XDTRACE_PROBE3(n,t1,a1,t2,a2,t3,a3)	\
	DTRACE_PROBE3(n,t1,a1,t2,a2,t3,a3)
#define XDTRACE_PROBE4(n,t1,a1,t2,a2,t3,a3,t4,a4)	\
	DTRACE_PROBE4(n,t1,a1,t2,a2,t3,a3,t4,a4)

#else

#define XDTRACE_PROBE(n)
#define XDTRACE_PROBE1(n,t1,a1)
#define XDTRACE_PROBE2(n,t1,a1,t2,a2)
#define XDTRACE_PROBE3(n,t1,a1,t2,a2,t3,a3)
#define XDTRACE_PROBE4(n,t1,a1,t2,a2,t3,a3,t4,a4)

#endif

#ifdef XRAMFS_ENABLE_DEBUG_PRINT
#define XRAMDBG(x)	do { (void)printf x ; } while(0)
#else
#define XRAMDBG(x)
#endif

/*
 * calculate size between specified pointers (addresses) in bytes.
 * note:
 * 1. we cannot use ptrdiff_t, because ptrdiff_t is signed so that
 *    cannot expression over 2G (in 32bit env.)
 *    e.g. size of 0x0000000 to 0xffffffff is 4294967295 (= 0xffffffff);
 *         but ptrdiff_t cannot expression 4294967295 (instead -1).
 *    to resolve this problem, we use size_t to expression them.
 * 2. we cast pointer to 'char *' for calculating size in _bytes_.
 *    in ANSI C, size of 'char' type is equal to 1.
 * 3. even if $to is smaller than $from (i.e. $to is before $from),
 *    do not raise an error. this may be overflowed
 *    (e.g. mapping 256 MiB sized media from 0xf0000000
 *     -> from 0xf00000000 to 0x00000000(!)).
 */
#define XRAM_SIZE_PTR2PTR(from, to)	\
	((size_t)((char *)(to) - (char *)(from)))
/*
 * calculate required blocks (each block has (1 << $shift) bytes)
 * to store data which size is $byte byte(s). note that we DO NOT
 * consider integer overflow.
 *
 * $byte should be unsigned value even if defined as unsigned -
 * if not specified, $byte may be changed to 'signed' int.
 * (example: uint8_t(8bit), uint16_t(16bit) -> int(31bit + signed bit))
 * to understand, try this code:
 *
 * uint16_t val = 0x8000U;
 * uint64_t sres, ures;
 * sres = val << 16;
 * ures = (unsigned)val << 16;
 * printf("%llx\n%llx\n", sres, ures);
 *
 * you will get:
 * ffffffff80000000
 * 80000000
 */
#define XRAM_BYTE2UNIT_ROUNDUP(byte, shift) \
	(((uintmax_t)(byte) + (1ULL << (shift)) - 1ULL) >> (shift))


#ifdef	__cplusplus
}
#endif

#endif  /* _KERNEL */

#endif	/* _XRAMFS_XRAM_H */
