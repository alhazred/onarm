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

#ifndef _CZFS_ZIL_H
#define	_CZFS_ZIL_H

#pragma ident	"@(#)czfs:zil.h"

#include <../zfs/sys/zil.h>

#undef	KMEM_ZIL_LWB_CACHE
#undef	zfs_replay_create_attr

#define	KMEM_ZIL_LWB_CACHE	"czfs_zil_lwb_cache"
#define	zfs_replay_create_acl	zfs_replay_error
#define	zfs_replay_acl_v0	zfs_replay_error
#define	zfs_replay_acl		zfs_replay_error
#define	zfs_replay_create_attr	zfs_replay_error

#endif	/* _CZFS_ZIL_H */
