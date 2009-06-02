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

#pragma ident	"@(#)significandf.c	1.10	06/01/31 SMI"

#if defined(ELFOBJ)
#pragma weak significandf = __significandf
#endif

#include "libm.h"

float
#if defined(__arm)
__significandf(float x)
#else
significandf(float x)
#endif
{
	int ix = *(int *) &x & ~0x80000000;

	if (ix == 0 || ix >= 0x7f800000)	/* 0/+-Inf/NaN */
#if defined(FPADD_TRAPS_INCOMPLETE_ON_NAN)
		return (ix > 0x7f800000 ? x * x : x);
#else
		return (x + x);
#endif
	else
		return (scalbnf(x, -ilogbf(x)));
}
