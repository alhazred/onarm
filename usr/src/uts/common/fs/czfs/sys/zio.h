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

#ifndef _CZFS_ZIO_H
#define	_CZFS_ZIO_H

#pragma ident	"@(#)czfs:zio.h"

#include <../zfs/sys/zio.h>

#undef	KMEM_ZIO_CACHE
#undef	KMEM_ZIO_BUF_SIZE
#undef	KMEM_ZIO_DATA_BUF_SIZE
#undef	ZIO_FAILURE_MODE_DEFAULT

#define	KMEM_ZIO_CACHE			"czfs_zio_cache"
#define	KMEM_ZIO_BUF_SIZE		"czfs_zio_buf_%lu"
#define	KMEM_ZIO_DATA_BUF_SIZE		"czfs_zio_data_buf_%lu"
#define	ZIO_FAILURE_MODE_DEFAULT	ZIO_FAILURE_MODE_ABORT

#endif	/* _CZFS_ZIO_H */
