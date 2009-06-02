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
 * Copyright (c) 2006-2008 NEC Corporation
 * All rights reserved.
 */

#pragma ident	"@(#)door.s"

	.file	"door.s"

#include <sys/asm_linkage.h>

	/*
	 * weak aliases for public interfaces
	 */
	ANSI_PRAGMA_WEAK(_door_bind,function)
	ANSI_PRAGMA_WEAK(_door_getparam,function)
	ANSI_PRAGMA_WEAK(_door_info,function)
	ANSI_PRAGMA_WEAK(_door_revoke,function)
	ANSI_PRAGMA_WEAK(_door_setparam,function)
	ANSI_PRAGMA_WEAK(_door_unbind,function)

	ANSI_PRAGMA_WEAK(door_bind,function)
	ANSI_PRAGMA_WEAK(door_getparam,function)
	ANSI_PRAGMA_WEAK(door_info,function)
	ANSI_PRAGMA_WEAK(door_revoke,function)
	ANSI_PRAGMA_WEAK(door_setparam,function)
	ANSI_PRAGMA_WEAK(door_unbind,function)

#include <sys/door.h>
#include "SYS.h"

#if defined(PIC)

/*
 * Setup an address of global symbol.
 *	label - correspondence between GSYM_REF and GSYM_GOT.
 *	name  - global symbol name.
 *	reg0  - out parameter. address of name symbol.
 *	reg1  - temporary register.
 *
 * Note that 'reg0' must be even register number.
 */
#define	GSYM_REF(label, name, reg0, reg1)				\
	ldrd	reg0, .L##label##_##name##_got;				\
.L##label##_##name##_ref:;						\
	add	reg1, pc, reg1;						\
	ldr	reg0, [reg1, reg0]
/*
 * Hold a reference to got.
 */
#define	GSYM_GOT(label, name)						\
	.align	3;							\
.L##label##_##name##_got:;						\
	.word	_GLOBAL_OFFSET_TABLE_-(.L##label##_##name##_ref+8);	\
	.word	name(GOT)

#else /* PIC */

#define	GSYM_REF(label, name, reg0, reg1)	ldr	reg0, =name
#define	GSYM_GOT(label, name)

#endif /* PIC */

/*
 * Offsets within struct door_results
 */
#define	DOOR_COOKIE	(0 * CLONGSIZE)
#define	DOOR_DATA_PTR	(1 * CLONGSIZE)
#define	DOOR_DATA_SIZE	(2 * CLONGSIZE)
#define	DOOR_DESC_PTR	(3 * CLONGSIZE)
#define	DOOR_DESC_SIZE	(4 * CLONGSIZE)
#define	DOOR_PC		(5 * CLONGSIZE)
#define	DOOR_SERVERS	(6 * CLONGSIZE)
#define	DOOR_INFO_PTR	(7 * CLONGSIZE)

/*
 * All of the syscalls except door_return() follow the same pattern.
 * The subcode goes in argument 6th, after all of the other arguments.
 * Max argument number of door syscalls is 3.
 * So we extend the stack for argument 5 and 6, then store the subcode.
 */
#define	DOOR_SYSCALL(name, code)					\
	ENTRY(name);							\
	sub	sp, sp, #8;						\
	mov	ip, #(code);						\
	str	ip, [sp, #4];	/* syscall subcode, arg 6 */		\
	SYSTRAP_RVAL1(door);						\
	add	sp, sp, #8;						\
	SYSCERROR;							\
	RET;								\
	SET_SIZE(name)

	DOOR_SYSCALL(__door_bind,	DOOR_BIND)
	DOOR_SYSCALL(__door_call,	DOOR_CALL)
	DOOR_SYSCALL(__door_create,	DOOR_CREATE)
	DOOR_SYSCALL(__door_getparam,	DOOR_GETPARAM)
	DOOR_SYSCALL(__door_info,	DOOR_INFO)
	DOOR_SYSCALL(__door_revoke,	DOOR_REVOKE)
	DOOR_SYSCALL(__door_setparam,	DOOR_SETPARAM)
	DOOR_SYSCALL(__door_ucred,	DOOR_UCRED)
	DOOR_SYSCALL(__door_unbind,	DOOR_UNBIND)
	DOOR_SYSCALL(__door_unref,	DOOR_UNREFSYS)

/*
 * int
 * __door_return(
 *	void 			*data_ptr,
 *	size_t			data_size,	(in bytes)
 *	door_return_desc_t	*door_ptr,	(holds returned desc info)
 *	caddr_t			stack_base,
 *	size_t			stack_size)
 */
	ENTRY(__door_return)
	sub	sp, sp, #8
	ldr	ip, [sp, #8]
	str	ip, [sp]		/* copy stack_size (arg5) */
	mov	ip, #DOOR_RETURN
	str	ip, [sp, #4]		/* set subcode (arg6) */

.Ldoor_restart:
	SYSTRAP_RVAL1(door)
	bcs	2f			/* errno is set */
	/*
	 * On return, we're serving a door_call.  Our stack looks like this:
	 *
	 *		descriptors (if any)
	 *		data (if any)
	 *	 sp->	struct door_results
	 *
	 * struct door_results has the arguments in place for the server proc,
	 * so we just call it directly.
	 */
	ldr	r2, [sp, #DOOR_SERVERS]	/* load nservers */
	teq	r2, #0			/* test nservers */
	bne	1f			/* everything looks o.k. */
	/*
	 * this is the last server thread - call creation func for more
	 */
	ldr	r0, [sp, #DOOR_INFO_PTR]	/* load door_info ptr */
	GSYM_REF(label1, door_server_func, r2, r3)
	ldr	r1, [r2]
	blx	r1			/* call create function */
1:
	/* Call the door server function now */
	mov	r3, sp
	ldr	r2, [sp, #DOOR_DESC_SIZE]
	str	r2, [sp, #-8]!		/* EABI : 8-byte alignment */
	ldr	ip, [r3, #DOOR_PC]
	ldr	r0, [r3, #DOOR_COOKIE]
	ldr	r1, [r3, #DOOR_DATA_PTR]
	ldr	r2, [r3, #DOOR_DATA_SIZE]
	ldr	r3, [r3, #DOOR_DESC_PTR]
	blx	ip

	/* Exit the thread if we return here */
	mov	r0, #0
	bl	_thr_terminate
	/* NOTREACHED */
2:
	/*
	 * Error during door_return call.  Repark the thread in the kernel if
	 * the error code is EINTR (or ERESTART) and this lwp is still part
	 * of the same process.
	 */
	mov	r0, ip
	cmp	r0, #ERESTART		/* ERESTART is same as EINTR */
	moveq	r0, #EINTR
	cmp	r0, #EINTR		/* interrupted while waiting? */
	addne	sp, sp, #8
	bne	__cerror		/* if not, return the error */

	stmdb	sp!, {r3, lr}
	bl	_private_getpid		/* get current process id */
	GSYM_REF(label2, door_create_pid, r2, r3)
	ldr	r1, [r2]
	ldmia	sp!, {r3, lr}

	cmp	r1, r0			/* same process? */
	movne	r0, #EINTR		/* if no, return EINTR */
	addne	sp, sp, #8		/*        (child of forkall) */
	bne	__cerror

	mov	r0, #0			/* clear arguments and restart */
	mov	r1, #0
	mov	r2, #0
	b	.Ldoor_restart
	/* NOTREACHED */
	/* Don't place any instruction from here. */
	GSYM_GOT(label1, door_server_func)
	GSYM_GOT(label2, door_create_pid)
	SET_SIZE(__door_return)
