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
 * Copyright (c) 2007-2008 NEC Corporation
 * All rights reserved.
 */

#pragma ident	"@(#)_ext_misc.c"

#if defined(__ARM_EABI__)

#include "thr_uberdata.h"

#pragma weak __aeabi_memset8 = __aeabi_memset
#pragma weak __aeabi_memset4 = __aeabi_memset

void
__aeabi_memset(void *dest, size_t n, int c)
{
	(void)_private_memset(dest, c, n);
}

#pragma weak __aeabi_memclr8 = __aeabi_memclr
#pragma weak __aeabi_memclr4 = __aeabi_memclr

void
__aeabi_memclr(void *dest, size_t n)
{
	(void)_private_memset(dest, 0, n);
}

/* helper functions */
int
__aeabi_idiv0(int return_value)
{
	return (return_value);
}

long long
__aeabi_ldiv0(long long return_value)
{
	return (return_value);
}


union uu {
	longlong_t	q;		/* as a (signed) quad */
	u_longlong_t	uq;		/* as an unsigned quad */
	long		sl[2];		/* as two signed longs */
	ulong_t		ul[2];		/* as two unsigned longs */
};

/*
 * Define high and low longwords.
 */
#define	H		1
#define	L		0
#define	HALF_BITS	(sizeof (long) * CHAR_BIT / 2)

#pragma weak __aeabi_ul2d = __float_ul2d

double
__float_ul2d(u_longlong_t a)
{
	union uu aa;
	double d;

	aa.uq = a;
	d = aa.ul[H];
	d *= (1 << HALF_BITS);
	d *= (1 << HALF_BITS);
	d += aa.ul[L];

	return (d);
}

#pragma weak __aeabi_ul2f = __float_ul2f

float
__float_ul2f(u_longlong_t a)
{
	union uu aa;
	double d;

	aa.uq = a;
	d = aa.ul[H];
	d *= (1 << HALF_BITS);
	d *= (1 << HALF_BITS);
	d += aa.ul[L];

	return ((float)d);
}

#endif

