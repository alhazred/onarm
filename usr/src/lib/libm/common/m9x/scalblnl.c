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

#pragma ident	"@(#)scalblnl.c	1.3	06/01/31 SMI"

#if defined(ELFOBJ)
#pragma weak scalblnl = __scalblnl
#endif

#include "libm.h"
#include <float.h>		/* LDBL_MAX, LDBL_MIN */

#if defined(__sparc)
#define	XSET_EXP(k, x)	((int *) &x)[0] = (((int *) &x)[0] & ~0x7fff0000) | \
				(k << 16)
#define	ISINFNANL(k, x)	(k == 0x7fff)
#define	XTWOT_OFFSET	113
static const long double xtwot = 10384593717069655257060992658440192.0L,
								/* 2^113 */
	twomtm1 = 4.814824860968089632639944856462318296E-35L;	/* 2^-114 */
#elif defined(__i386) || defined(__arm)
#define	XSET_EXP(k, x)	((int *) &x)[2] = (((int *) &x)[2] & ~0x7fff) | k
#if defined(HANDLE_UNSUPPORTED)
#define	ISINFNANL(k, x)	(k == 0x7fff || k != 0 && \
				(((int *) &x)[1] & 0x80000000) == 0)
#else
#define	ISINFNANL(k, x)	(k == 0x7fff)
#endif
#define	XTWOT_OFFSET	64
static const long double xtwot = 18446744073709551616.0L,	/* 2^64 */
	twomtm1 = 2.7105054312137610850186E-20L;		/* 2^-65 */
#endif

long double
#if defined(__arm)
__scalblnl(long double x, long n)
#else
scalblnl(long double x, long n)
#endif
{
	int k = XBIASED_EXP(x);

	if (ISINFNANL(k, x))
		return (x + x);
	if (ISZEROL(x) || n == 0)
		return (x);
	if (k == 0) {
		x *= xtwot;
		k = XBIASED_EXP(x) - XTWOT_OFFSET;
	}
	k += (int) n;
	if (n > 50000 || k > 0x7ffe)
		return (LDBL_MAX * copysignl(LDBL_MAX, x));
	if (n < -50000 || k <= -XTWOT_OFFSET - 1)
		return (LDBL_MIN * copysignl(LDBL_MIN, x));
	if (k > 0) {
		XSET_EXP(k, x);
		return (x);
	}
	k += XTWOT_OFFSET + 1;
	XSET_EXP(k, x);
	return (x * twomtm1);
}
