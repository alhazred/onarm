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

#ifndef _CZFS_DSL_DELEG_H
#define	_CZFS_DSL_DELEG_H

#pragma ident	 "@(#)czfs:dsl_deleg.h"

#include <../zfs/sys/dsl_deleg.h>

#define	dsl_deleg_get(ddname, nvp)		ENOTSUP
#define	dsl_deleg_set(ddname, nvp, unset)	ENOTSUP
#define	dsl_deleg_access(ddname, perm, cr)	EPERM
#define	dsl_deleg_set_create_perms(dd, tx, cr)
#define	dsl_deleg_can_allow(ddname, nvp, cr)	ENOTSUP
#define	dsl_deleg_can_unallow(ddname, nvp, cr)	ENOTSUP
#define	dsl_deleg_destroy(os, zapobj, tx)	0
#define	dsl_delegation_on(os)			B_FALSE


#endif	/* _CZFS_DSL_DELEG_H */
