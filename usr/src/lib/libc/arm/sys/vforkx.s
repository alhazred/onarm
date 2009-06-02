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
 * Copyright (c) 2006-2008 NEC Corporation
 */

#pragma ident	"@(#)vforkx.s	1.22	06/11/26 SMI"

	.file	"vforkx.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(vforkx,function)
	ANSI_PRAGMA_WEAK(vfork,function)

#include "SYS.h"
#include "assym.h"
#include <asm/cpufunc.h>

/*
 * pid = vforkx(flags);
 * syscall trap: forksys(2, flags)
 *
 * pid = vfork();
 * syscall trap: forksys(2, 0)
 *
 * From the syscall:
 * r1 == 0 in parent process, r1 = 1 in child process.
 * r0 == pid of child in parent, r0 == pid of parent in child.
 *
 * The child gets a zero return value.
 * The parent gets the pid of the child.
 */

/*
 * The child of vfork() will execute in the parent's address space,
 * thereby changing the stack before the parent runs again.
 * Therefore we have to be careful how we return from vfork().
 * Pity the poor debugger developer who has to deal with this kludge.
 *
 * We block all blockable signals while performing the vfork() system call
 * trap.  This enables us to set curthread->ul_vfork safely, so that we
 * don't end up in a signal handler with curthread->ul_vfork set wrong.
 */
ENTRY(vforkx)
	str	r0, [sp, #-4]!		/* save flags into stack */
	b	0f
ENTRY(vfork)
	mov	r3, #0			/* save flags = 0 into stack */
	str	r3, [sp, #-4]!		/* save flags into stack */
0:
	adr	ip, .L0_MASKSET
	mov	r0, $SIG_SETMASK	/* block all signals */
	ldmia	ip, {r1, r2}
	SYSTRAP_2RVALS(lwp_sigmask)

	ldr	r1, [sp], #4		/* flags */
	mov	r0, #2
	SYSTRAP_2RVALS(forksys)		/* vforkx(flags) */
	bcc	.L1_SUCCESS		/* goto it when vfork() succeeds */

	/* vfork() failed. retrieve the signal masks */
	str	r0, [sp, #-4]!		/* save the vfork() error number */

	READ_CP15(0, c13, c0, 2, r3)	/* r3: curthread */
	mov	r0, $SIG_SETMASK
	ldr	r1, [r3, #UL_SIGMASK]
	ldr	r2, [r3, #UL_SIGMASK+4]
	SYSTRAP_2RVALS(lwp_sigmask)

	ldr	r0, [sp], #4		/* restore the vfork() error number */
	b	__cerror
	/* NOTREACHED */

.L1_SUCCESS:
	/*
	 * To determine if we are (still) a child of vfork(), the child
	 * increments curthread->ul_vfork by one and the parent decrements
	 * it by one.  If the result is zero, then we are not a child of
	 * vfork(), else we are.  We do this to deal with the case of
	 * a vfork() child calling vfork().
	 */
	READ_CP15(0, c13, c0, 2, r3)	/* r3: curthread */
	cmp	r1, #0
	bne	.child

	/* parent process */
	ldr	r1, [r3, #UL_VFORK]
	cmp	r1, #0			/* don't let it go negative */
	subne	r1, r1, #1		/* curthread->ul_vfork--; */
	b	.update_ul_vfork

.child:
	mov	r0, #0			/* zero the return value in the child */
	ldr	r1, [r3, #UL_VFORK]
	add	r1, r1, #1		/* curthread->ul_vfork++; */
	/* FALLTHRU */

.update_ul_vfork:
	str	r1, [r3, #UL_VFORK]	/* curthread->ul_vfork updated */

	/*
	 * Clear the schedctl interface in both parent and child.
	 * (The child might have modified the parent.)
	 */
	mov	r1, #0
	str	r1, [r3, #UL_SCHEDCTL]
	str	r1, [r3, #UL_SCHEDCTL_CALLED]
	str	r0, [sp, #-4]!		/* save the vfork() return value */

	mov	r0, $SIG_SETMASK	/* retrieve the signal masks */
	ldr	r1, [r3, #UL_SIGMASK]
	ldr	r2, [r3, #UL_SIGMASK+4]
	SYSTRAP_2RVALS(lwp_sigmask)

	ldr	r0, [sp], #4		/* restore the vfork() return value */
	bx	lr

	/*
	 * As the following is the "read only data", we don't need to
	 * use lock facility.
	 */
.L0_MASKSET:
	.word	MASKSET0
	.word	MASKSET1
	SET_SIZE(vfork)
	SET_SIZE(vforkx)
