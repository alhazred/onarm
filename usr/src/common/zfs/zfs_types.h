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

#ifndef	_ZFS_TYPES_H
#define	_ZFS_TYPES_H

#pragma ident	"@(#)zfs_types.h"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * For index types (e.g. compression and checksum), we want the numeric value
 * in the kernel, but the string value in userland.
 */
typedef uint64_t			objid_t;
typedef uint64_t			txg_t;
typedef uint64_t			numchildren_t;
#define	PRIuOBJID			PRIu64
#define	PRIuTXG				PRIu64
#define	byteswap_objid_array		byteswap_uint64_array
#define	DATA_TYPE_OBJID			DATA_TYPE_UINT64
#define	TXG_MAX				UINT64_MAX
#define	nvlist_lookup_objid(x,y,z)	nvlist_lookup_uint64(x,y,z)
#define	nvlist_add_objid(x,y,z)		nvlist_add_uint64(x,y,z)
#define	nvlist_lookup_txg(x,y,z)	nvlist_lookup_uint64(x,y,z)
#define	nvlist_add_txg(x,y,z)		nvlist_add_uint64(x,y,z)
#define	BSWAP_OBJID(obj)		BSWAP_64(obj)

#ifdef	__cplusplus
}
#endif

#endif	/* _ZFS_TYPES_H */

