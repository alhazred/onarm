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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2007 NEC Corporation
 */

#pragma ident	"@(#)asinf.c	1.12	06/01/23 SMI"

#pragma weak asinf = __asinf

#include "libm.h"

static const float zero = 0.0f;

float
#if defined(__arm)
__asinf(float x)
#else
asinf(float x)
#endif
{
	int	ix;

	ix = *(int *)&x & ~0x80000000;
	if (ix > 0x3f800000)	/* |x| > 1 or x is nan */
		return ((x * zero) / zero);
	return ((float)asin((double)x));
}
