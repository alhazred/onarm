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

#pragma ident	"@(#)gammal.c	1.5	06/01/31 SMI"

#pragma weak gammal = __gammal

/*
 * long double gammal(long double x);
 */

#include "libm.h"
#include "longdouble.h"
#if defined(__arm)
extern double __k_lgamma(double, int *);
#endif

extern int signgam;
extern int signgaml;

long double
#if defined(__arm)
__gammal(long double x)
#else
gammal(long double x)
#endif
{
#if defined(__arm)
	long double y;
	if (sizeof(long double) == sizeof(double)) {
		double w;
		w = __k_lgamma((double)x, &signgaml);
		signgam = signgaml; /* SUSv3 requires the setting of signgam */
		return((long double)w);
	}
	y = __k_lgammal(x, &signgaml);
#else
	long double y = __k_lgammal(x, &signgaml);
#endif
	signgam = signgaml;	/* SUSv3 requires the setting of signgam */
	return (y);
}
