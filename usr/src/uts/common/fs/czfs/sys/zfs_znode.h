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

#ifndef _CZFS_ZFS_ZNODE_H
#define	_CZFS_ZFS_ZNODE_H

#pragma ident	"@(#)czfs:zfs_znode.h"

#include <sys/czfs_conf.h>
#include <../zfs/sys/zfs_znode.h>

#undef	KMEM_ZFS_ZNODE_CACHE
#undef	USE_FUIDS
#undef	ZFS_READONLY
#undef	ZFS_HIDDEN
#undef	ZFS_SYSTEM
#undef	ZFS_ARCHIVE
#undef	ZFS_IMMUTABLE
#undef	ZFS_NOUNLINK
#undef	ZFS_APPENDONLY
#undef	ZFS_NODUMP
#undef	ZFS_OPAQUE
#undef	ZFS_AV_QUARANTINED
#undef	ZFS_AV_MODIFIED

#define	KMEM_ZFS_ZNODE_CACHE		"czfs_znode_cache"
#define	USE_FUIDS(version, os)		B_FALSE
#define	ZFS_READONLY			0x0000000000000000
#define	ZFS_HIDDEN			0x0000000000000000
#define	ZFS_SYSTEM			0x0000000000000000
#define	ZFS_ARCHIVE			0x0000000000000000
#define	ZFS_IMMUTABLE			0x0000000000000000
#define	ZFS_NOUNLINK			0x0000000000000000
#define	ZFS_APPENDONLY			0x0000000000000000
#define	ZFS_NODUMP			0x0000000000000000
#define	ZFS_OPAQUE			0x0000000000000000
#define	ZFS_AV_QUARANTINED		0x0000000000000000
#define	ZFS_AV_MODIFIED			0x0000000000000000

/*
 * Data may pad out any remaining bytes in the znode buffer, eg:
 *
 * |<------------------ dnode_phys (256) -------------------->|
 * |<-- dnode (96) -->|<-------- "bonus" buffer (160) ------->|
 *		      |<--- znode (144) --->|<-- data (16) -->|
 *
 * At present, we only use this space to store symbolic links.
 */

#endif	/* _CZFS_ZFS_ZNODE_H */
