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

#pragma ident	"@(#)acoshl.c	1.7	06/01/31 SMI"

#if defined(ELFOBJ)
#pragma weak acoshl = __acoshl
#endif

#include "libm.h"
#if defined(__arm)
extern double acosh(double);
#endif

static const long double
	zero	= 0.0L,
	ln2	= 6.931471805599453094172321214581765680755e-0001L,
	one	= 1.0L,
	big	= 1.e+20L;

long double
#if defined(__arm)
__acoshl(long double x)
#else
acoshl(long double x)
#endif
{
	long double t;

#if defined(__arm)
	if (sizeof(long double) == sizeof(double)) {
		return((long double)acosh((double)x));
	}
#endif
	if (isnanl(x))
		return (x + x);
	else if (x > big)
		return (logl(x) + ln2);
	else if (x > one) {
		t = sqrtl(x - one);
		return (log1pl(t * (t + sqrtl(x + one))));
	} else if (x == one)
		return (zero);
	else
		return ((x - x) / (x - x));
}
