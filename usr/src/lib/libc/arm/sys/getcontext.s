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

#pragma ident	"@(#)getcontext.s	1.9	05/06/08 SMI"

	.file	"getcontext.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(getcontext,function)
	ANSI_PRAGMA_WEAK(swapcontext,function)

#include "SYS.h"
#include "assym.h"
#include <asm/cpufunc.h>
#include <sys/regset.h>

/*
 * getcontext() and swapcontext() are written in assembler since it has to
 * capture the correct machine state of the caller, including
 * the registers: %lr,%sp,%pc
 *
 * As swapcontext() is actually equivalent to getcontext() + setcontext(),
 * swapcontext() shares the most code with getcontext().
 */

#define GETCONTEXT_IMPL		/* reg save is refer */			\
	add	r1, r1, #UC_MCONTEXT_GREGS;   /* $UC_MCONTEXT_GREGS; */	\
				/* %r1 <-- &ucp->uc_mcontext.gregs */	\
	str	lr, [r1, #LR_OFF];	/* return PC set */		\
	str	sp, [r1, #SP_OFF];					\
	str	lr, [r1, #PC_OFF];					\
	mov	r0, #0;							\
	str	r0, [r1, #R0_OFF]; 					
				/* getcontext returns 0 after a setcontext */

/*
 * getcontext(ucontext_t *ucp)
 */
	ANSI_PRAGMA_WEAK2(_private_getcontext,getcontext,function)

	ENTRY(getcontext)
	stmfd	sp!, {r0, lr}			/* save first arg: ucp */
	bl	_fref_(__getcontext_syscall)	/* call getcontext: syscall */
	ldmfd	sp!, {r1, lr}			/* restore arg: r0->r1 */	
	cmp	r0, #0
	beq	1f
	mov	r0, #-1
	b	2f
1:
	GETCONTEXT_IMPL
	mov	r0, #0
2:
	bx	lr
	SET_SIZE(getcontext)


/*
 * swapcontext(ucontext_t *oucp, const ucontext_t *ucp)
 */
	ENTRY(swapcontext)
	stmfd	sp!, {r0, r1, lr}		/* save first arg: ucp */
	sub	sp, sp, #4			/* EABI : 8-byte alignment */
	bl	_fref_(__getcontext_syscall)	/* call getcontext: syscall */
	add	sp, sp, #4
	ldmfd	sp!, {r1, r2, lr};		/*  restore arg: r0->r1  r1->r2 */
	cmp	r0, #0
	beq	1f
	mov	r0, #-1
	b	2f
1:
	GETCONTEXT_IMPL
	/* call setcontext */
	str	lr, [sp, #-8]!
	mov	r0, r2				/* %r0 <-- second arg: ucp */
	bl	_fref_(_private_setcontext)	/* call setcontext */
	ldr	lr, [sp], #8	
2:
	bx	lr
	SET_SIZE(swapcontext)
