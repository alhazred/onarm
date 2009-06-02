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
 * Copyright (c) 2006-2009 NEC Corporation
 */

#pragma ident	"@(#)asm_subr.s	1.26	05/06/08 SMI"

	.file	"asm_subr.s"

#include "SYS.h"
#include "../assym.h"
#include <asm/cpufunc.h>
#include <sys/regset.h>		

	/* This is where execution resumes when a thread created with */
	/* thr_create() or pthread_create() returns (see setup_context()). */
	/* We pass the (void *) return value to _thr_terminate(). */
	ENTRY(_lwp_start)
	bl	_fref_(_thr_terminate)
	SET_SIZE(_lwp_start)

	/* All we need to do now is (carefully) call lwp_exit(). */
	ENTRY(_lwp_terminate)
        SYSTRAP_RVAL1(lwp_exit)
	RET
	SET_SIZE(_lwp_terminate)

	ENTRY(set_curthread)
	WRITE_CP15(0, c13, c0, 2, r0)		
	RET
	SET_SIZE(set_curthread)

	ENTRY(__lwp_park)
	mov	r2, r1
	mov	r1, r0
	mov	r0, #0
        SYSTRAP_RVAL1(lwp_park)
	SYSLWPERR
	RET
	SET_SIZE(__lwp_park)

	ENTRY(__lwp_unpark)
	mov	r1, r0
	mov	r0, #1
        SYSTRAP_RVAL1(lwp_park)
	SYSLWPERR
	RET
	SET_SIZE(__lwp_unpark)

	ENTRY(__lwp_unpark_all)
	mov	r2, r1
	mov	r1, r0
	mov	r0, #2
        SYSTRAP_RVAL1(lwp_park)
	SYSLWPERR
	RET
	SET_SIZE(__lwp_unpark_all)

	ENTRY(lwp_yield)
	SYSTRAP_RVAL1(yield)
	RETC
	SET_SIZE(lwp_yield)

/*
 * __sighndlr(int sig, siginfo_t *si, ucontext_t *uc, void (*hndlr)())
 *
 * This is called from sigacthandler() for the entire purpose of
 * communicating the ucontext to java's stack tracing functions.
 */
	ENTRY(__sighndlr)		/* r0,r1,r2  argument          */
	.globl	__sighndlrend		/* r3 :	subroutine call adress */
	bx	r3			/* source image                */
					/* call  (*r3)(r0,r1,r2)       */
					/* return adress  =  lr        */
__sighndlrend:
	SET_SIZE(__sighndlr)

/*
 * int _sigsetjmp(sigjmp_buf env, int savemask)
 *
 * This version is faster than the old non-threaded version because we
 * don't normally have to call __getcontext() to get the signal mask.
 * (We have a copy of it in the ulwp_t structure.)
 */
	
#undef	sigsetjmp

	ENTRY2(sigsetjmp,_sigsetjmp)	
	str	lr, [sp, #-4]

	/* Call __csigsetjmp(env, savemask, r4, ..., r11, sp, lr, cpsr) */
	sub	ip, sp, #40
	stmia	ip!, {r6-r11,r13,r14}
	mrs	r2, cpsr
	str	r2, [ip], #4
	mov	r2, r4
	mov	r3, r5
	sub	sp, sp, #40
	bl	_fref_(__csigsetjmp)

	add	sp, sp, #40
	ldr	pc, [sp, #-4]		/* _sigsetjmp returns to caller */
	SET_SIZE(sigsetjmp)
	SET_SIZE(_sigsetjmp)

