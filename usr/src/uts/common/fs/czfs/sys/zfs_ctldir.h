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
 * Copyright (c) 2008 NEC Corporation
 * All rights reserved.
 */

#ifndef _CZFS_ZFS_CTLDIR_H
#define	_CZFS_ZFS_CTLDIR_H

#pragma ident	"@(#)czfs:zfs_ctldir.h"

#include <../zfs/sys/zfs_ctldir.h>

#define	zfsctl_create(zfsvfs)
#define	zfsctl_destroy(zfsvfs)
#define	zfsctl_root(zp)	NULL
#define	zfsctl_init()
#define	zfsctl_fini()

#define	zfsctl_root_lookup(dvp, nm, vpp, pnp, flags, \
	rdir, cr, ct, direntflags, realpnp)		ENOTSUP
#define	zfsctl_lookup_objset(vfsp, objsetid, zfsvfsp)	ENOTSUP
#define	zfsctl_umount_snapshots(vfsp, fflags, cr)	ENOTSUP

#endif	/* _CZFS_ZFS_CTLDIR_H */
