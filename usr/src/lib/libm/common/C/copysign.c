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

#pragma ident	"@(#)copysign.c	1.8	06/01/31 SMI"

#if defined(ELFOBJ)
#pragma weak copysign = __copysign
#endif

#include "libm.h"

double
#if defined(__arm)
__copysign(double x, double y)
#else
copysign(double x, double y)
#endif
{
	int hx, hy;

	hx = ((int *) &x)[HIWORD];
	hy = ((int *) &y)[HIWORD];
	return (hx ^ hy) >= 0 ?  (x) : (-x);
}
