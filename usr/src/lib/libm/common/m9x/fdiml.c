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

#pragma ident	"@(#)fdiml.c	1.5	06/01/31 SMI"

#if defined(ELFOBJ)
#pragma weak fdiml = __fdiml
#endif

#include "libm.h"	/* for islessequal macro */

long double
__fdiml(long double x, long double y) {
#if defined(COMPARISON_MACRO_BUG)
	if (x == x && y == y && x <= y) {
#else
	if (islessequal(x, y)) {
#endif
		x = 0.0l;
		y = -x;
	}
	return (x - y);
}
