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
 * Copyright (c) 2007-2008 NEC Corporation
 * All rights reserved.
 */

#ident	"@(#)arm/ml/sys_trap.s"

#include <sys/asm_linkage.h>
#include <sys/asm_exception.h>

#if defined(__lint)

#include <sys/types.h>

#else   /* __lint */

#include <sys/privregs.h>
#include <asm/thread.h>
#include "assym.h"

#endif	/* __lint */

#if defined(__lint)

void
switrap(void)
{}

#else	/* __lint */

#if defined(DEBUG)

.Lassert_lwptoregs_msg:
	.asciz	"lwptoregs(lwp) == rp"
.Lassert_schedflag_msg:
	.asciz	"curthread->t_schedflag & TS_DONT_SWAP"
.Lmy_filename:
	.asciz	"sys_trap.s"

#define	ASSERT_LWPTOREGS(lwp, rp)		\
	ldr	r3, [lwp, #LWP_REGS];		\
	cmp	r3, rp;				\
	adrne	r0, .Lassert_lwptoregs_msg;	\
	adrne	r1, .Lmy_filename;		\
	movne	r2, #__LINE__;			\
	blne	assfail;

/* ASSERT(curthread->t_schedflag & TS_DONT_SWAP); */
#define	ASSERT_SCHEDFLAG(thread)		\
	ldrh	r3, [thread, #T_SCHEDFLAG];		\
	tst	r3, #2;				\
	adreq	r0, .Lassert_schedflag_msg;	\
	adreq	r1, .Lmy_filename;		\
	moveq	r2, #__LINE__;		\
	bleq	assfail;

#else	/* DEBUG */
#define	ASSERT_LWPTOREGS(lwp, rp)
#define	ASSERT_SCHEDFLAG(thread)
#endif	/* DEBUG */

/*
 * Software interrupt exception handler for the exception vector.
 */
ENTRY_NP(switrap)
	SAVE_REGS_SWI_ENTER
	tst	ip, #T_SWI_FASTCALL	/* check whether fast syscall */
	bne	fastswitrap		/* fastswitrap in fast_trap.s */

	/*
	 * normal system call handler
	 */
	SAVE_REGS_SWI_SYSCALL

	/*
	 * In this version of Solaris, syscall number is passed by r12.
	 * The immedisate value of SWI instruction is not used.
	 */
	/*
	 * Register usage:
	 *   r4: curthread
	 *   r5: CPU / lwp
	 *   r6: syscall number (code) / sysent address (callp)
	 *   sp: struct regs *rp
	 */
	THREADP(r4)
	ldr	r5, [r4, #T_CPU]
	mov	r6, ip

#ifdef  TRAPTRACE
	/* REVISIT */
#endif  /* TRAPTRACE */

	/*
	 * syscall_mstate(LMS_USER, LMS_SYSTEM);
	 */
	mov	r0, #LMS_USER
	mov	r1, #LMS_SYSTEM
	bl	syscall_mstate

	/*
	 * CPU_STATS_ADDQ(CPU, sys, syscall, 1);
	 * This code must be placed before interrupts enabled.
	 */
	add	ip, r5, #CPU_STATS_SYS_SYSCALL
	ldrd	r0, [ip]
	mov	r2, #1
	mov	r3, #0
	adds	r0, r0, r2
	adc	r1, r1, r3
	strd	r0, [ip]

	cpsie	i			/* enable irq */

	ldr	r5, [r4, #T_LWP]	/* r5 = ttolwp(curthread) */

	ASSERT_LWPTOREGS(r5, sp)
	ASSERT_SCHEDFLAG(r4)

	/* lwp->lwp_state = LWP_SYS */
	mov	r3, #LWP_SYS
	strb	r3, [r5, #LWP_STATE]
	/* lwp->lwp_ru.sysc++ */
	add	ip, r5, #LWP_RU_SYSC
	ldrd	r0, [ip]
	mov	r2, #1
	mov	r3, #0
	adds	r0, r0, r2
	adc	r1, r1, r3
	strd	r0, [ip]
	/* lwp->lwp_eosys = NORMALRETURN */
	mov	r3, #0
	strb	r3, [r5, #LWP_EOSYS]	/* assume this will be normal */

	/*
	 * Set lwp_ap to point to the args, even if none are needed for this
	 * system call.  This is for the loadable-syscall case where the
	 * number of args won't be known until the system call is loaded, and
	 * also maintains a non-NULL lwp_ap setup for get_syscall_args(). Note
	 * that lwp_ap MUST be set to a non-NULL value _BEFORE_ t_sysnum is
	 * set to non-zero; otherwise get_syscall_args(), seeing a non-zero
	 * t_sysnum for this thread, will charge ahead and dereference lwp_ap.
	 */
	/* lwp->lwp_ap = &rp->r_r0 */
	str	sp, [r5, #LWP_AP]	/* for get_syscall_args */

	/* t->t_sysnum = code */
	mov	ip, #T_SYSNUM
	strh	r6, [r4, ip]
	/* callp = code >= NSYSCALL ? &nosys_ent : sysent + code; */
	cmp	r6, #NSYSCALL
	ldrhs	r6, .Lnosys_ent
	ldrlo	r3, .Lsysent
	addlo	r6, r3, r6, lsl #4	/* r6 = callp = &sysent[r6] */

	/* if ((t->t_pre_sys | syscalltrace) != 0) */
	ldrb	r2, [r4, #T_PRE_SYS]
#ifdef SYSCALLTRACE
	ldr	r3, .Lsyscalltrace
	ldr	r3, [r3]
	orrs	r2, r2, r3
#else
	teq	r2, #0
#endif /* SYSCALLTRACE */
	bne	.Lpre_syscall

	/*
	 * Setup the system call arguments and call the handler.
	 * If sy_arg <= 4, use r0-r3 of struct regs on kernel stack.
	 * If sy_narg > 4, save the arguments to lwp_args[].
	 *
	 * Note: for loadable system calls the number of arguments required
	 * may not be known at this point, and will be zero if the system call
	 * was never loaded.  Once the system call has been loaded, the number
	 * of args is not allowed to be changed.
	 */
.Lcall_handler:
	ldrb	r3, [r6, #SY_NARG]
	/* sy_callc = callp->sy_callc */
	ldr	r8, [r6, #SY_CALLC]	/* r8 = sy_callc */
	/* if (callp->sy_narg <= 4) */
	cmp	r3, #4
	bhi	.Lcall_handler8

	ldmia	sp, {r0-r3}
	blx	r8

.Lsyscall_exit:
	/*
	 * Handle signals and other post-call events if necessary.
	 */
	/* if ((t->t_post_sys_ast | syscalltrace) == 0) */
	ldr	r2, [r4, #T_POST_SYS_AST]
#ifdef SYSCALLTRACE
	ldr	r3, .Lsyscalltrace
	ldr	r3, [r3]
	orrs	r2, r2, r3
#else
	teq	r2, #0
#endif /* SYSCALLTRACE */
	bne	.Lpost_syscall

	/*
	 * Normal return.
	 * Clear error indication and set return values.
	 */
	ldr	r3, [sp, #ROFF_CPSR]
	/* rp->r_r0 = rval.r_val1; */
	/* rp->r_r1 = rval.r_val2; */
	stmia	sp, {r0, r1}
	bic	r3, r3, #PSR_C_BIT	/* reset carry bit (PSR_C_BIT) */
	mov	r2, #LWP_USER
	str	r3, [sp, #ROFF_CPSR]	/* rp->r_cpsr &= ~PSR_C_BIT; */
	strb	r2, [r5, #LWP_STATE]	/* lwp->lwp_state = LWP_USER; */
	/* t->t_sysnum = 0; */
	mov	r1, #0
	mov	ip, #T_SYSNUM
	strh	r1, [r4, ip]		/* invalidate args */
	/* syscall_mstate(LMS_SYSTEM, LMS_USER); */
	mov	r0, #LMS_SYSTEM
	mov	r1, #LMS_USER
	bl	syscall_mstate
	b	user_rtt		/* return to user_rtt */

.Lcall_handler8:
	bl	save_syscall_args
	cmp	r0, #0
	bne	.Lsyscall_exit_efault
	ldr	r7, [r5, #LWP_AP]	/* r7 = lwp_ap */
	add	ip, r7, #0x10
	ldmia	ip, {r0-r3}
	stmdb	sp!, {r0-r3}		/* expand stack */
	ldmia	r7, {r0-r3}
	blx	r8
	add	sp, sp, #0x10
	b	.Lsyscall_exit

.Lsyscall_exit_efault:
	mov	r0, #14			/* EFAULT */
	bl	set_errno
.Lsyscall_exit_zero:
	mov	r0, #0
	mov	r1, #0
	b	.Lsyscall_exit

.Lpre_syscall:
	/* pre_syscall(); */
	bl	pre_syscall
	cmp	r0, #0
	bne	.Lsyscall_exit_zero
	b	.Lcall_handler

.Lpost_syscall:
	/* post_syscall(rval.r_val1, rval.r_val2); */
	bl	post_syscall
	b	user_rtt		/* return to user_rtt */

#ifdef SYSCALLTRACE
.Lsyscalltrace:
	.word   syscalltrace
#endif
.Lnosys_ent:
	.word	nosys_ent
.Lsysent:
	.word	sysent

	SET_SIZE(switrap)

#endif	/* __lint */
