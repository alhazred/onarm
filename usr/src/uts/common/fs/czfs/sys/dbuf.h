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

#ifndef _CZFS_DBUF_H
#define	_CZFS_DBUF_H

#pragma ident	"@(#)czfs:dbuf.h"

#include <sys/czfs_conf.h>
#include <../zfs/sys/dbuf.h>

#undef	KMEM_DMU_BUF_IMPL_T
#undef	KMEM_DN_BONUS_BUF

#define	KMEM_DMU_BUF_IMPL_T	"czfs_dmu_buf_impl_t"
#define	KMEM_DN_BONUS_BUF	"czfs_dn_bonus_buf"

#endif	/* _CZFS_DBUF_H */
