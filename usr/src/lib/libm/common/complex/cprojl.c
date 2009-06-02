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

#pragma ident	"@(#)cprojl.c	1.3	06/01/31 SMI"

#pragma weak cprojl = __cprojl

#include "libm.h"		/* fabsl */
#include "complex_wrapper.h"
/* INDENT OFF */
static const long double zero = 0.0L;
/* INDENT ON */

ldcomplex
#if defined(__arm)
__cprojl(ldcomplex z)
#else
cprojl(ldcomplex z)
#endif
{
	long double x, y;
	int hy;

	x = LD_RE(z);
	y = LD_IM(z);
#if defined(__i386) || (defined(__arm) && (HIWORD))
	hy = ((int *) &y)[2] << 16;
#else
	hy = ((int *) &y)[0];
#endif
	if (isinfl(y)) {
		LD_RE(z) = fabsl(y);
		LD_IM(z) = hy >= 0 ? zero : -zero;
	} else if (isinfl(x)) {
		LD_RE(z) = fabsl(x);
		LD_IM(z) = hy >= 0 ? zero : -zero;
	}
	return (z);
}
