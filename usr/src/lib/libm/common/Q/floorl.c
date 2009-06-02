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
 * Copyright (c) 2007 NEC Corporation
 */

#pragma ident	"@(#)floorl.c	1.6	06/01/31 SMI"

/*
 * ceill(x)	return the biggest integral value below x
 * floorl(x)	return the least integral value above x
 *
 * NOTE: aintl(x), anintl(x), ceill(x), floorl(x), and rintl(x) return result
 * with the same sign as x's,  including 0.0.
 */

#pragma weak ceill = __ceill
#pragma weak floorl = __floorl

#include "libm.h"
#if defined(__arm)
extern double ceil(double);
extern double floor(double);
#endif

static const long double qone = 1.0L;

long double
#if defined(__arm)
__ceill(long double x)
#else
ceill(long double x)
#endif
{
	long double t;

#if defined(__arm)
	if (sizeof(long double) == sizeof(double)) {
		return((long double)ceil((double)x));
	}
#endif
	if (!finitel(x))
		return (x + x);
	t = rintl(x);
	if (t >= x) 			/* already ceil(x) */
		return (t);
	else				/* t < x case: return t+1  */
		return (copysignl(t + qone, x));
}

long double
#if defined(__arm)
__floorl(long double x)
#else
floorl(long double x)
#endif
{
	long double t;

#if defined(__arm)
	if (sizeof(long double) == sizeof(double)) {
		return((long double)floor((double)x));
	}
#endif
	if (!finitel(x))
		return (x + x);
	t = rintl(x);
	if (t <= x)
		return (t);		/* already floor(x) */
	else 				/* x < t case: return t-1  */
	    return (copysignl(t - qone, x));
}
