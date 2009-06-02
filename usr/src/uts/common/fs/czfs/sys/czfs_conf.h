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

/*
 * Configuration for CZFS (Compactified ZFS)
 * Always defined ZFS_COMPACT.
 */

#ifndef _CZFS_CONF_H
#define	_CZFS_CONF_H

#pragma ident	"@(#)czfs:czfs_conf.h"

#ifdef CZFS_ARC_MAX
#define	ZFS_ARC_MAX			CZFS_ARC_MAX
#endif	/* CZFS_ARC_MAX */

#ifdef CZFS_ARC_MIN
#define	ZFS_ARC_MIN			CZFS_ARC_MIN
#endif	/* CZFS_ARC_MIN */

#ifdef CZFS_BUF_LOCKS
#define	ZFS_BUF_LOCKS			CZFS_BUF_LOCKS
#endif	/* CZFS_BUF_LOCKS */

#ifdef CZFS_DBUF_MUTEXES
#define	ZFS_DBUF_MUTEXES		CZFS_DBUF_MUTEXES
#endif	/* CZFS_DBUF_MUTEXES */

#ifdef CZFS_IOCTL_MINIMUMSET
#define	ZFS_IOCTL_MINIMUMSET
#endif	/* CZFS_IOCTL_MINIMUMSET */

#ifdef CZFS_NO_L2ARC
#define	ZFS_NO_L2ARC
#endif	/* CZFS_NO_L2ARC */

#ifdef CZFS_NO_MIRROR
#define	ZFS_NO_MIRROR
#endif	/* CZFS_NO_MIRROR */

#ifdef CZFS_NO_PREFETCH
#define	ZFS_NO_PREFETCH
#endif	/* CZFS_NO_PREFETCH */

#ifdef CZFS_NO_RAIDZ
#define	ZFS_NO_RAIDZ
#endif	/* CZFS_NO_RAIDZ */

#ifdef CZFS_NO_UFSFILE
#define	ZFS_NO_UFSFILE
#endif	/* CZFS_NO_UFSFILE */

#ifdef CZFS_NO_VDEVCACHE
#define	ZFS_NO_VDEVCACHE
#endif	/* CZFS_NO_VDEVCACHE */

#ifdef CZFS_NO_ZVOL
#define	ZFS_NO_ZVOL
#endif	/* CZFS_NO_ZVOL */

#ifdef CZFS_OBJ_MTX_SZ
#define	ZFS_OBJ_MTX_SZ			CZFS_OBJ_MTX_SZ
#endif	/* CZFS_OBJ_MTX_SZ */

#ifdef CZFS_ROOTFS_RW
#define	ZFS_ROOTFS_RW
#endif	/* CZFS_ROOTFS_RW */

#ifdef CZFS_TASKQ_MINALLOC
#define	ZFS_TASKQ_MINALLOC		CZFS_TASKQ_MINALLOC
#endif	/* CZFS_TASKQ_MINALLOC */

#ifdef CZFS_TASKQ_THREADS
#define	ZFS_TASKQ_THREADS		CZFS_TASKQ_THREADS
#endif	/* CZFS_TASKQ_THREADS */

#ifdef CZFS_ZIOTASKQ_CREATE_ASNEED
#define	ZFS_ZIOTASKQ_CREATE_ASNEED
#endif	/* CZFS_ZIOTASKQ_CREATE_ASNEED */

#endif	/* _CZFS_CONF_H */
