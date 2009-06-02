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

#pragma ident	"@(#)rintl.c	1.10	06/01/31 SMI"

#pragma weak rintl = __rintl

/*
 * rintl(long double x) return x rounded to integral according to
 * the prevailing rounding direction
 *
 * NOTE: aintl(x), anintl(x), ceill(x), floorl(x), and rintl(x) return result
 * with the same sign as x's, including 0.0L.
 */

#include "libm.h"

#if !defined(__arm)
extern enum fp_precision_type __swapRP(enum fp_precision_type);
#endif
#if defined(__arm)
extern double rint(double);
#endif

static const double one = 1.0;
static const long double qzero = 0.0L;

long double
#if defined(__arm)
__rintl(long double x)
#else
rintl(long double x)
#endif
{
	enum fp_precision_type rp;
	long double t, w, two112;
	int *pt = (int *) &two112;

#if defined(__arm)
	if (sizeof(long double) == sizeof(double)) {
		return((long double)rint((double)x));
	}
#endif
	if (!finitel(x))
		return (x + x);

	if (*(int *) &one != 0) {	/* set two112 = 2^112 */
		pt[0] = 0x406f0000;
		pt[1] = pt[2] = pt[3] = 0;
	} else {
		pt[3] = 0x406f0000;
		pt[0] = pt[1] = pt[2] = 0;
	}

	if (fabsl(x) >= two112)
		return (x);	/* already an integer */
	t = copysignl(two112, x);
#if !defined(__arm)
	rp = __swapRP(fp_extended);	/* make sure precision is long double */
#endif
	w = x + t;			/* x+sign(x)*2^112 rounded to integer */
#if !defined(__arm)
	(void) __swapRP(rp);		/* restore precision mode */
#endif
	if (w == t)
		return (copysignl(qzero, x));	/* x rounded to zero */
	else
		return (w - t);
}
