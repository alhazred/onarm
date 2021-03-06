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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2006 NEC Corporation
 */

#pragma ident	"@(#)_divdi3.s	1.4	05/11/16 SMI"

	.file	"_divdi3.s"

#include <SYS.h>

/*
 * C support for 64-bit modulo and division.
 * GNU routines callable from C (though generated by the compiler). 
 * Hand-customized compiler output - see comments for details.
 */

#if defined(__lint)

/*ARGSUSED*/
uint64_t
__udivdi3(uint64_t a, uint64_t b)
{ return (0); }

/*ARGSUSED*/
uint64_t
__umoddi3(uint64_t a, uint64_t b)
{ return (0); }

/*ARGSUSED*/
int64_t
__divdi3(int64_t a, int64_t b)
{ return (0); }

/*ARGSUSED*/
int64_t
__moddi3(int64_t a, int64_t b)
{ return (0); }

#else

/*
 * __udivdi3
 *
 * Perform division of two unsigned 64-bit quantities, returning the
 * quotient in %r1:%r0.
 */
	ENTRY(__udivdi3)
	str	lr, [sp, #-8]!
	bl	_fref_(__udiv64)
	ldr	pc, [sp], #8
	SET_SIZE(__udivdi3)

/*
 * __umoddi3
 *
 * Perform division of two unsigned 64-bit quantities, returning the
 * remainder in %r1:%r0.
 */
	ENTRY(__umoddi3)
	str	lr, [sp, #-8]!
	bl	_fref_(__urem64)
	ldr	pc, [sp], #8
	SET_SIZE(__umoddi3)

/*
 * __divdi3
 *
 * Perform division of two signed 64-bit quantities, returning the
 * quotient in %r1:%r0.
 */
	ENTRY(__divdi3)
	str	lr, [sp, #-8]!
	bl	_fref_(__div64)
	ldr	pc, [sp], #8
	SET_SIZE(__divdi3)

/*
 * __moddi3
 *
 * Perform division of two signed 64-bit quantities, returning the
 * quotient in %r1:%r0.
 */
	ENTRY(__moddi3)
	str	lr, [sp, #-8]!
	bl	_fref_(__rem64)
	ldr	pc, [sp], #8
	SET_SIZE(__moddi3)

#endif	/* __lint */
