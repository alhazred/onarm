/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2007 NEC Corporation
 */

#pragma ident	"@(#)_X_cplx_lr_div_rx.c	1.4	05/06/08 SMI"

/*
 * _X_cplx_lr_div_rx(a, w) returns a / w computed by the textbook
 * formula without regard to exceptions or special cases.
 *
 * This code is intended to be used only when CX_LIMITED_RANGE is
 * ON; otherwise use _X_cplx_div_rx.
 */

#if !defined(__arm)
#error This code is for ARM only
#endif
#if 0
#if !defined(i386) && !defined(__i386) && !defined(__amd64) 
#error This code is for x86 only
#endif
#endif

long double _Complex
_X_cplx_lr_div_rx(long double a, long double _Complex w)
{
	long double _Complex	v;
	long double		c, d, r;

	c = ((long double *)&w)[0];
	d = ((long double *)&w)[1];
	r = a / (c * c + d * d);
	((long double *)&v)[0] = r * c;
	((long double *)&v)[1] = r * -d;
	return (v);
}
