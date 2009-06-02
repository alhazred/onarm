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
 * Copyright (c) 2007-2008 NEC Corporation
 */

#pragma ident	"@(#)ldexp.c	1.13	06/01/31 SMI"

#if defined(ELFOBJ)
#pragma weak ldexp = __ldexp

#ifdef	__arm
/* long double must be treated as double. */
#pragma weak	ldexpl = __ldexp
#pragma weak	__ldexpl = __ldexp
#endif	/* __arm */
#endif

#include "libm.h"
#include <errno.h>

double
#if defined(__arm)
__ldexp(double x, int n)
#else
ldexp(double x, int n)
#endif
{
	int *px = (int *) &x, ix = px[HIWORD] & ~0x80000000;

	if (ix >= 0x7ff00000 || (px[LOWORD] | ix) == 0)
#if defined(FPADD_TRAPS_INCOMPLETE_ON_NAN)
		return (ix >= 0x7ff80000 ? x : x + x);
		/* assumes sparc-like QNaN */
#else
		return (x + x);
#endif
	x = scalbn(x, n);
	ix = px[HIWORD] & ~0x80000000;
	/*
	 * SVID3 requires both overflow and underflow cases to set errno
	 * XPG3/XPG4/XPG4.2/SUSv2 requires overflow to set errno
	 */
	if (ix >= 0x7ff00000 || (px[LOWORD] | ix) == 0)
		errno = ERANGE;
	return (x);
}
