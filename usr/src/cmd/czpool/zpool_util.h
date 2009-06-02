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

#ifndef _CZFS_ZPOOL_UTIL_H_
#define	_CZFS_ZPOOL_UTIL_H_

#pragma ident	"@(#)zpool_util.h"

#include <../zpool/zpool_util.h>

#ifdef SET_DEFAULT_GUID
#include "sys/czfs_poolcfg.h"
#endif	/* SET_DEFAULT_GUID */
#include "zfs_subr.h"

#undef	ZPOOL_CMD_NAME
#define	ZPOOL_CMD_NAME		"czpool"

#endif	/* _CZFS_ZPOOL_UTIL_H_ */
