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

#pragma ident	"@(#)syscall.s	1.21	05/06/08 SMI"

	.file	"syscall.s"

#include <sys/asm_linkage.h>
#include <sys/trap.h>

	ANSI_PRAGMA_WEAK(syscall,function)

#include "SYS.h"

#undef _syscall		/* override "synonyms.h" */
#undef __systemcall

/*
 * C library -- int syscall(int sysnum, arg0, ..., arg7);
 * C library -- int __systemcall(sysret_t *, int sysnum, arg0, ..., arg7);
 *
 * Interpret a given system call
 *
 * This version handles up to 8 'long' arguments to a system call.
 * See MAXSYSARGS in sys/klwp.h
 *
 * See sparc/sys/syscall.s to understand why __syscall6() exists.
 * On ARM, the implementation of __syscall6() is the same as _syscall(),
 * the only difference being that __syscall6 is not an exported symbol.
 * It seems better that __syscall6() is combined with _syscall()
 * for the reduction of the code.
 *
 * WARNING WARNING WARNING:
 * As this function loads the 5 'long' variables from the original
 * stack pointer always, you should guarantee that they can be accessed.
 */

	ENTRY2(_syscall, _syscall6)
	add	ip, sp, #4
	stmdb	sp!, {r0, r1, r2, r3}	/* r0, r1, r2, r3: saved to stack */
	sub	sp, sp, #16
	ldmia	ip, {r0, r1, r2, r3}	/* arg4-arg7 */
	stmia	sp, {r0, r1, r2, r3}
	sub	ip, ip, #16		/* arg0-arg3 */
	ldmia	ip, {r0, r1, r2, r3}
	ldr	ip, [ip, #-4]		/* sysnum */
	swi	0			/* syscall */
	add	sp, sp, #32		/* not change C flag */
	SYSCERROR
	RET
	SET_SIZE(_syscall)
	SET_SIZE(_syscall6)


/*
 * C library -- int __systemcall(sysret_t *, int sysnum, arg0, ..., arg7);
 *
 * Interpret a given system call
 *
 * This version handles up to 8 'long' arguments to a system call.
 * See MAXSYSARGS in sys/klwp.h
 *
 * See sparc/sys/syscall.s to understand why __systemcall6() exists.
 * On ARM, the implementation of __systemcall6() is the same as __systemcall(),
 * the only difference being that __systemcall6 is not an exported symbol.
 * It seems better that __systemcall6() is combined with __systemcall()
 * for the reduction of the code.
 *
 * WARNING WARNING WARNING:
 * As this function loads the 6 'long' variables from the original
 * stack pointer always, you should guarantee that they can be accessed.
 */

	ENTRY2(__systemcall, __systemcall6)
	add	ip, sp, #8
	stmdb	sp!, {r0, r1, r2, r3}	/* r0, r1, r2, r3: saved to stack */
	sub	sp, sp, #16
	ldmia	ip, {r0, r1, r2, r3}	/* arg4-arg7 */
	stmia	sp, {r0, r1, r2, r3}
	sub	ip, ip, #16		/* arg0-arg3 */
	ldmia	ip, {r0, r1, r2, r3}
	ldr	ip, [ip, #-4]		/* sysnum */
	swi	0			/* syscall */

	bcs	.L_error

	ldr	r2, [sp, #16]
	stmia	r2, {r0, r1}
	mov	r0, #0
	add	sp, sp, #32
	bx	lr			/* successfully */

.L_error:
	ldr	r2, [sp, #16]
	mov	r0, #-1
	mov	r1, #-1
	stmia	r2, {r0, r1}
	add	sp, sp, #32
	mov	r0, ip
	bx	lr
	SET_SIZE(__systemcall)
	SET_SIZE(__systemcall6)
