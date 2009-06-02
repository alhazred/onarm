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

#pragma ident	"@(#)sinhf.c	1.12	06/01/23 SMI"

#pragma weak sinhf = __sinhf

#include "libm.h"

float
#if defined(__arm)
__sinhf(float x)
#else
sinhf(float x)
#endif
{
	double	s;
	float	w;
	int	hx, ix;

	hx = *(int *)&x;
	ix = hx & ~0x80000000;
	if (ix >= 0x7f800000) {
		/* sinhf(x) is x if x is +-Inf or NaN */
		return (x * 1.0f);
	}
	if (ix >= 0x43000000)	/* sinhf(x) trivially overflows */
		s = (hx < 0)? -1.0e100 : 1.0e100;
	else
		s = sinh((double)x);
	w = (float)s;
	return (w);
}
