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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2008 NEC Corporation
 */

#ifndef _CZFS_DMU_TRAVERSE_H
#define	_CZFS_DMU_TRAVERSE_H

#pragma ident	"@(#)czfs:dmu_traverse.h"

#include <../zfs/sys/dmu_traverse.h>

#undef	ZB_MAXOBJSET
#undef	ZB_MAXOBJECT

#define	ZB_MAXOBJSET		(1U << 30)
#define	ZB_MAXOBJECT		(1U << 30)

#endif	/* _CZFS_DMU_TRAVERSE_H */
