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

#pragma ident	"@(#)lrintf.c	1.3	06/01/31 SMI"

#if defined(ELFOBJ)
#pragma weak lrintf = __lrintf
#endif

#include <sys/isa_defs.h>	/* _ILP32 */
#include "libm.h"

#if defined(_ILP32)
long
#if defined(__arm)
__lrintf(float x)
#else
lrintf(float x)
#endif
{
	/*
	 * Note: The following code works on x86 (in the default rounding
	 * precision mode), but one should just use the fistpl instruction
	 * instead.
	 */
	union {
		unsigned i;
		float f;
	} xx, yy;
	unsigned hx;

	xx.f = x;
	hx = xx.i & ~0x80000000;
	if (hx < 0x4b000000) {	/* |x| < 2^23 */
		/* add and subtract a power of two to round x to an integer */
#if defined(__sparc) || defined(__arm)
		yy.i = (xx.i & 0x80000000) | 0x4b000000;
#elif defined(__i386)
		/* assume 64-bit precision */
		yy.i = (xx.i & 0x80000000) | 0x5f000000;
#else
#error Unknown architecture
#endif
		x = (x + yy.f) - yy.f;
		return ((long) x);
	}

	/* now x is nan, inf, or integral */
	return ((long) x);
}
#else
#error Unsupported architecture
#endif	/* defined(_ILP32) */
