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

#ifndef _CZFS_ZIO_CHECKSUM_H
#define	_CZFS_ZIO_CHECKSUM_H

#pragma ident	"@(#)czfs:zio_checksum.h"

#include <../zfs/sys/zio_checksum.h>

#define	fletcher_2_byteswap			fletcher_2_native
#define	fletcher_4_byteswap			fletcher_4_native
#define	fletcher_4_incremental_byteswap		fletcher_4_incremental_native

#endif	/* _CZFS_ZIO_CHECKSUM_H */
