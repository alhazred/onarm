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

#ifndef _CZFS_DNODE_H
#define	_CZFS_DNODE_H

#pragma ident	 "@(#)czfs:dnode.h"

#ifdef	__cplusplus
extern "C" {
#endif

#define	DNODE_SHIFT		8	/* 256 bytes */
#define	DN_MIN_INDBLKSHIFT	10	/* 1k */
#define	DN_MAX_INDBLKSHIFT	13	/* 8k */
#define	DNODE_CORE_SIZE		32	/* 32 bytes for dnode sans blkptrs */
#define	DN_MAX_OBJECT_SHIFT	24	/* 16 million (zfs_fid_t limit) */

#include <sys/czfs_conf.h>
#include <../zfs/sys/dnode.h>

#undef	KMEM_DNODE_T

#define	KMEM_DNODE_T	"czfs_dnode_t"

void byteswap_none(void *, size_t);
#define	dnode_byteswap(dnp)	byteswap_none(dnp, 0)

#ifdef	__cplusplus
}
#endif

#endif	/* _CZFS_DNODE_H */
