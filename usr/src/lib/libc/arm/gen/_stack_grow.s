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
 * Copyright (c) 2006 NEC Corporation
 */

#pragma ident	"@(#)_stack_grow.s	1.7	05/06/08 SMI"

        .file   "_stack_grow.s"

#include "SYS.h"
#include "assym.h"
#include <asm/cpufunc.h>

/*
 * void *
 * _stack_grow(void *addr)
 */
	ENTRY(_stack_grow)
	str	r0, [sp, #-8]!
	READ_CP15(0, c13, c0, 2, r3)	/* r3: _curthread */
	mov	r2, #UL_USTACK
	add	r1, r2, #SS_SP		/* ul_ustack.ss_sp */
	ldr	r1, [r3, r1]		/* curthread->ul_ustack.ss_sp */
	add	r2, r2, #SS_SIZE	/* ul_ustack.ss_size */
	ldr	r2, [r3, r2]		/* curthread->ul_ustack.ss_size */
	sub	r3, r0, r2		/* addr - base */
	cmp	r2, r3			/* if (size > (addr - base)) */
	bgt	.Lend
	cmp	r2, #0			/* if (size == 0) */
	beq	.Lend
	sub	r3, sp, r1		/* sp - base */
	cmp	r2, r3			/* if (size > (sp - base)) */
	subgt	sp, r1, #STACK_ALIGN	/* base - STACK_ALIGN */

	/*
	 * Dereference an address in the guard page.
	 */
	strb	r3, [r1, #-1]

	SYSTRAP_RVAL1(lwp_self)
	mov	r1, #SIGSEGV
	SYSTRAP_RVAL1(lwp_kill)
.Lend:
	ldr	r0, [sp], #8
	bx	lr
        SET_SIZE(_stack_grow)
