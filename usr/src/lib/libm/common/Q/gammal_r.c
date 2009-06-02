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

#pragma ident	"@(#)gammal_r.c	1.5	06/01/31 SMI"

/*
 * long double gammal_r(long double x, int *signgamlp);
 */

#pragma weak gammal_r = __gammal_r

#include "libm.h"
#include "longdouble.h"
#if defined(__arm)
extern double __k_lgamma(double, int *);
#endif

long double
#if defined(__arm)
__gammal_r(long double x, int *signgamlp)
#else
gammal_r(long double x, int *signgamlp)
#endif
{
#if defined(__arm)
	if (sizeof(long double) == sizeof(double)) {
		double y;
		y = __k_lgamma((double)x, signgamlp);
		return((long double)y);
	}
#endif
	return (__k_lgammal(x, signgamlp));
}
