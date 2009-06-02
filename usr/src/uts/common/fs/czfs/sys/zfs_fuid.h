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

#ifndef _CZFS_ZFS_FUID_H
#define	_CZFS_ZFS_FUID_H

#pragma ident	"@(#)czfs:zfs_fuid.h"

#include <../zfs/sys/zfs_fuid.h>

#ifdef _KERNEL
#define	zfs_fuid_map_id(zfsvfs, fuid, cr, type)	(uid_t)fuid
#define	zfs_fuid_destroy(zfsvfs)
#define	zfs_fuid_create_cred(zfsvfs, type, tx, cr, fuidp)	\
	(uint64_t)((type == ZFS_OWNER) ? crgetuid(cr) : crgetgid(cr))
#define	zfs_fuid_create(zfsvfs, id, cr, type, tx, fuidpp)	(uint64_t)id
#define	zfs_fuid_map_ids(zp, cr, uidp, gidp) {	\
	*uidp = (uid_t)zp->z_phys->zp_uid;	\
	*gidp = (uid_t)zp->z_phys->zp_gid;	\
}
#define	zfs_fuid_info_alloc()			NULL
#define	zfs_fuid_info_free(fuidp)
#define	zfs_groupmember(zfsvfs, id, cr)		groupmember(id, cr)
#endif

#define	zfs_fuid_idx_domain(idx_tree, idx)	NULL
#define	zfs_fuid_table_load(os, fuid_obj, idx_tree, domain_tree)	0ULL
#define	zfs_fuid_table_destroy(idx_tree, domain_tree)

#endif	/* _CZFS_ZFS_FUID_H */
