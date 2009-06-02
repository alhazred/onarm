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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2007 NEC Corporation
 */

#pragma ident	"@(#)lgammaf.c	1.11	06/01/23 SMI"

#pragma weak lgammaf = __lgammaf

#include "libm.h"

extern int signgamf;

float
#if defined(__arm)
__lgammaf(float x)
#else
lgammaf(float x)
#endif
{
	float	 y;

	if (isnanf(x))
		return (x * x);
	y = (float)__k_lgamma((double)x, &signgamf);
	signgam = signgamf;	/* SUSv3 requires the setting of signgam */
	return (y);
}