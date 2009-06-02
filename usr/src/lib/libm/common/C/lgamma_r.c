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

#pragma ident	"@(#)lgamma_r.c	1.6	06/01/23 SMI"

#pragma weak lgamma_r = __lgamma_r

#include "libm.h"

double
#if defined(__arm)
__lgamma_r(double x, int *signgamp)
#else
lgamma_r(double x, int *signgamp)
#endif
{
	double	g;

	if (isnan(x))
		return (x * x);

	g = rint(x);
	if (x == g && x <= 0.0) {
		*signgamp = 1;
		return (_SVID_libm_err(x, x, 15));
	}

	g = __k_lgamma(x, signgamp);
	if (!finite(g))
	    g = _SVID_libm_err(x, x, 14);
	return (g);
}