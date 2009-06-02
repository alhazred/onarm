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

#ifndef _CZFS_ZFS_H
#define	_CZFS_ZFS_H

#pragma ident	"@(#)czfs:zfs.h"

#include <../../sys/fs/zfs.h>

#undef	ZFS_MODNAME
#undef	ZFS_MODULE
#undef	ZFS_BOOTFS
#undef	ZPOOL_CACHE_DIR
#undef	ZPOOL_CACHE_FILE
#undef	ZPOOL_CACHE_TMP
#undef	ZFS_DRIVER
#undef	ZFS_DEV
#undef	ZVOL_DEV_DIR
#undef	ZVOL_RDEV_DIR
#undef	ZVOL_PSEUDO_DEV

#define	ZFS_MODNAME		"CZFS"
#define	ZFS_MODULE		"czfs"
#define	ZFS_BOOTFS		"czfs-bootfs"
#define	ZPOOL_CACHE_DIR		"/etc/czfs"
#define	ZPOOL_CACHE_FILE	"czpool.cache"
#define	ZPOOL_CACHE_TMP		".czpool.cache"
#define	ZFS_DRIVER		"czfs"
#define	ZFS_DEV			"/dev/czfs"
#define	ZVOL_DEV_DIR		"czvol/dsk"
#define	ZVOL_RDEV_DIR		"czvol/rdsk"
#define	ZVOL_PSEUDO_DEV		"/devices/pseudo/czvol@0:"

#endif	/* _CZFS_ZFS_H */
