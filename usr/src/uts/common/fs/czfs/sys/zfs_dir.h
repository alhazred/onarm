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

#ifndef _CZFS_ZFS_DIR_H
#define	_CZFS_ZFS_DIR_H

#pragma ident	"@(#)czfs:zfs_dir.h"

#include <../zfs/sys/zfs_dir.h>
#include <sys/u8_textprep.h>

#define	u8_strcmp(s1, s2, n, flag, version, errnum)	\
	(n == 0 ? strcmp(s1, s2) : strncmp(s1, s2, n))
#define	u8_validate(u8str, n, list, flag, errnum)	0

#endif	/* _CZFS_ZFS_DIR_H */
