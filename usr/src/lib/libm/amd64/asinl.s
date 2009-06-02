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

	.ident	"@(#)asinl.s	1.3	06/01/23 SMI"

	.file	"asinl.s"

#include "libm.h"
LIBM_ANSI_PRAGMA_WEAK(asinl,function)
#include "libm_synonyms.h"

#undef fabs

	ENTRY(asinl)
	fldt	8(%rsp)			/ push x
	fld1				/ push 1
	fld	%st(1)			/ x , 1 , x
	fabs				/ |x| , 1 , x
	fucomip	%st(1),%st
	ja	9f
	fadd	%st(1),%st		/ 1+x,x
	fld1				/ 1,1+x,x
	fsub	%st(2),%st		/ 1-x,1+x,x
	fmulp	%st,%st(1)		/ (1-x)*(1+x),x
	fsqrt				/ sqrt((1-x)*(1+x)),x
	fpatan				/ atan(x/sqrt((1-x)*(1+x)))
	ret
9:
	/ |x| > 1
	fstp	%st(0)			/ x
	fsub	%st,%st(0)		/ +/-0 or NaN+invalid
	fdiv	%st,%st(0)		/ NaN+invalid or NaN
	ret
	.align	16
	SET_SIZE(asinl)
