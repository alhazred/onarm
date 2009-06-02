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
 * Copyright (c) 2006-2008 NEC Corporation
 */

#include <sys/asm_linkage.h>
#include <sys/asm_exception.h>
#include <sys/cpuvar_impl.h>
#include <sys/trap.h>
#include <sys/regset.h>
#include <sys/privregs.h>
#include <sys/dtrace.h>
#ifdef	TRAPTRACE
#include <sys/traptrace.h>
#endif	/* TRAPTRACE */
#include <sys/machparam.h>

#if !defined(__lint)

	/* Exception vectors */

ENTRY(exception_vectors)
	ldr	pc, .Lreset_addr
	ldr	pc, .Lundef_addr
	ldr	pc, .Lswi_addr
	ldr	pc, .Lpabt_addr
	ldr	pc, .Ldabt_addr
	ldr	pc, .Ldabt_addr	/* Old address exception vector. */
	ldr	pc, .Lirq_addr
	ldr	pc, .Lfiq_addr

.Lreset_addr:
	.word	resettrap
.Lundef_addr:
	.word	undeftrap
.Lswi_addr:
	.word	switrap
.Lpabt_addr:
	.word	pabttrap
.Ldabt_addr:
	.word	dabttrap
.Lirq_addr:
	.word	irqtrap
.Lfiq_addr:
	.word	fiqtrap

	.globl	exception_vectors_size
	.type	exception_vectors_size, %object
	.align	CLONGSHIFT
exception_vectors_size:
	.long	. - exception_vectors
	SET_SIZE(exception_vectors_size)

	/* Exception Handlers */

#if	((TRAP_INFO_SIZE % STACK_ENTRY_ALIGN) != 0)
#error	Stack allocation code must be revised.
#endif	/* ((TRAP_INFO_SIZE % STACK_ENTRY_ALIGN) != 0) */

#if	STACK_ENTRY_ALIGN == 8

/*
 * Define macros to adjust stack alignment for EABI mode.
 * On EABI mode, stack pointer must be 8-bytes aligned at function entry.
 */

/*
 * Deallocate exception stack frame.
 * "reg" must be original stack pointer saved by ARM_EABI_STACK_ALIGN(reg).
 */
#define	EXCEPTION_STACK_DEALLOC(reg, framesize)		\
	mov	sp, reg

/*
 * Set "struct regs" address to "rp".
 * "reg" must be original stack pointer saved by ARM_EABI_STACK_ALIGN(reg).
 */
#define	EXCEPTION_STACK_SET_REGS(rp, reg)		\
	mov	rp, reg

#else	/* STACK_ENTRY_ALIGN != 8 */

/*
 * Deallocate exception stack frame.
 * We can simply decrease stack frame. "reg" will be ignored.
 */
#define	EXCEPTION_STACK_DEALLOC(reg, framesize)		\
	add	sp, sp, #(framesize)

/*
 * Set "struct regs" address to "rp".
 * Original stack pointer is still available in "sp".
 */
#define	EXCEPTION_STACK_SET_REGS(rp, reg)		\
	mov	rp, sp

#endif	/* STACK_ENTRY_ALIGN == 8 */

/*
 * Undefined instruction exception
 */
ENTRY_NP(undeftrap)
	sub	lr, lr, #4		/* adjust lr to the undefined inst */
	SAVE_REGS_UNDEF

	mov	r0, sp

	/* Adjust stack alignment before C function call. */
	ARM_EABI_STACK_ALIGN(r4)

	/* Preserve room for trap_info */
	sub	sp, sp, #TRAP_INFO_SIZE
	mov	r1, sp

	mov	r2, #T_UNDEF
	bl	trap			/* trap(rp, ti, T_UNDEF); */

	/* Restore stack pointer. */
	EXCEPTION_STACK_DEALLOC(r4, TRAP_INFO_SIZE)

	b	_sys_rtt
	SET_SIZE(undeftrap)

/*
 * Prefetch abort exception
 */
ENTRY_NP(pabttrap)
	sub	lr, lr, #4		/* Adjust lr */
	SAVE_REGS_PABT

	mov	r0, sp

	/* Adjust stack alignment before C function call. */
	ARM_EABI_STACK_ALIGN(r4)

	/* Preserve room for trap_info */
	sub	sp, sp, #(TRAP_INFO_SIZE)
	mov	r1, sp

	mov	r2, #T_PABT
	bl	trap			/* trap(rp, ti, T_PABT); */

	/* Restore stack pointer. */
	EXCEPTION_STACK_DEALLOC(r4, TRAP_INFO_SIZE)

	b	_sys_rtt
	SET_SIZE(pabttrap)

/*
 * Data abort exception
 */
ENTRY_NP(dabttrap)
	sub	lr, lr, #8		/* Adjust lr */
	SAVE_REGS_DABT

	mov	r0, sp

	/* Adjust stack alignment before C function call. */
	ARM_EABI_STACK_ALIGN(r4)

	bl	dtrace_check_dabttrap	/* dtrace_check_dabttrap(rp); */
	cmp	r0, #0
	ARM_EABI_STACK_RESTORE(ne, r4)
	bne	svc_rtt			/* return 1 only kernel mode */

	/* Set strucrt regs address to r0 again. */
	EXCEPTION_STACK_SET_REGS(r0, r4)

	/* Preserve room for trap_info */
	sub	sp, sp, #TRAP_INFO_SIZE
	mov	r1, sp

	mov	r2, #T_DABT
	bl	trap			/* trap(rp, ti, T_DABT); */

	/* Restore stack pointer. */
	EXCEPTION_STACK_DEALLOC(r4, TRAP_INFO_SIZE)

	b	_sys_rtt
	SET_SIZE(dabttrap)

/*
 * FIQ handler
 *
 * Remarks:
 *	fiqtrap() switches SVC stack pointer to its own stack so that
 *	FIQ handler can run even if SVC stack is corrupted.
 */
	.globl	fiq_handler
ENTRY_NP(fiqtrap)
	/*
	 * r8-r14 are banked registers.
	 */

	FIQ_PROLOG()

	sub	lr, lr, #4		/* Adjust lr */
	ldr	r12, [sp]		/* r12 = FIQ nest counter */
	add	r8, r12, #1
	str	r8, [sp]		/* Increment nest counter */

	/*
	 * Save r4-r5 into FIQ mode stack.
	 * They will be used to pass values between FIQ and SVC mode.
	 */
	stmdb	sp!, {r4-r5}

	/* Check whether we are already on FIQ stack. */
	teq	r12, #0
	movne	r4, sp
	movne	r5, #0			/* r5 = NULL if already on FIQ stack */
	bne	10f

	/*
	 * Install FIQ stack into SVC stack pointer.
	 */

	/* Install FIQ stack. */
	LOADCPU(r8)
	ldr	r4, [r8, #CPU_FIQ_STACK]
	add	r4, r4, #FIQ_STACKSZ	/* r4 = new SVC sp */
	cps	#PSR_MODE_SVC
	mov	r5, sp			/* Save current SVC sp into r5 */
	mov	sp, r4			/* Install new SVC sp */
	cps	#PSR_MODE_FIQ
	mov	r4, sp

10:
	SAVE_REGS_FIQ

	/*
	 * r4 = current FIQ sp
	 * r5 = origial SVC sp (NULL if sp is not changed)
	 */

	/* Save real r4-r5 into struct regs. */
	ldmia	r4, {r8-r9}
	str	r8, [sp, #ROFF_R4]
	str	r9, [sp, #ROFF_R5]

	/* Save real SVC sp into struct regs if sp has been changed. */
	teq	r5, #0
	subne	r5, r5, #8		/* Emulate srsdb! */
	strne	r5, [sp, #ROFF_SP_SVC]

	/* Deallocate FIQ stack frame. */
	cps	#PSR_MODE_FIQ
	add	sp, sp, #8
	cps	#PSR_MODE_SVC

	mov	r0, sp

	/* Adjust stack alignment before C function call. */
	ARM_EABI_STACK_ALIGN(r4)

	/* Preserve room for trap_info */
	sub	sp, sp, #(TRAP_INFO_SIZE)
	mov	r1, sp

	bl	fiq_handler		/* fiq_handler(rp, tip) */

	/* Restore stack pointer. */
	EXCEPTION_STACK_DEALLOC(r4, TRAP_INFO_SIZE)

	/*
	 * Decrement FIQ nest counter.
	 *
	 * Remarks:
	 *	It seems that GNU ld v2.17 dumps wrong code for
	 *	"cps<effect> <iflags>, #<mode>".
	 */
	cpsid	if
	cps	#PSR_MODE_FIQ
	ldr	r4, [sp]
	sub	r4, r4, #1
	str	r4, [sp]
	cps	#PSR_MODE_SVC

	/* Jump to _sys_rtt() if no need to change stack. */
	teq	r4, #0
	bne	_sys_rtt

	/*
	 * Restore SVC stack. We must copy struct regs from FIQ stack.
	 * We should restore sp after struct regs copy in order to
	 * detect SVC stack overflow.
	 */
	ldr	r10, [sp, #ROFF_SP_SVC]
	sub	r10, r10, #(REGSIZE - 8)
	mov	r12, r10
	ldmia	sp!, {r0-r9}		/* r0-r9 */
	stmia	r10!, {r0-r9}
	ldmia	sp!, {r0-r8}		/* r10-cpsr */
	stmia	r10!, {r0-r8}
	mov	sp, r12

	b	_sys_rtt
	SET_SIZE(fiqtrap)

#endif	/* !__lint */

/*
 * _sys_rtt - return from system trap handler.
 */
#if defined(__lint)

void
_sys_rtt(void)
{}

#else	/* __lint */

ENTRY_NP(_sys_rtt)
	/*
	 * Register Usage:
	 *	r4 = spsr
	 *	r5 = curthread
	 *	r6 = CPU
	 */
	ldr	r4, [sp, #ROFF_CPSR]
	and	r0, r4, #PSR_MODE
	cmp	r0, #PSR_MODE_USER
	bne	svc_rtt
	/* b user_rtt */

/*
 * We don't need to adjust stack alignment because we are at the bottom of
 * kernel stack that is aligned properly.
 */
ALTENTRY(user_rtt)
	THREADP(r5)
	cpsid	i			/* disable interrupt */
	ldrb	r0, [r5, #T_ASTFLAG]	/* r1 = curthread->t_astflag */
	cmp	r0, #0
	bne	.Luser_rtt_ast
	RESTORE_REGS_USER		/* exit */
	/*NOTREACHED*/
.Luser_rtt_ast:
	cpsie	i			/* enable interrupt */
	mov	r0, sp
	mov	r1, #0
	mov	r2, #T_AST
	adr	lr, user_rtt		/* return to user_rtt */
	b	trap			/* trap(rp, NULL, T_AST) */

ALTENTRY(svc_rtt)
	THREADP(r5)
	SVC_RTT_PROLOG(r5, r6)
	ldrb	r0, [r6, #CPU_KPRUNRUN]
	cmp	r0, #0
	bne	.Lsvc_rtt_preempt

.Lsvc_rtt_preempt_ret:
	/* Load range of critical sections. */
	adr	r10, .Lcritical_section
	ldmia	r10, {r2-r5}

	/*
	 * If we interrupted the mutex_exit() critical region we must
	 * reset the PC back to the beginning to prevent missed wakeups.
	 * See the comments in mutex_exit() for details.
	 */
	ldr	r1, [sp, #ROFF_PC]
	ldr	r3, [r3]
	sub	r0, r1, r2
	cmp	r0, r3
	strlo	r2, [sp, #ROFF_PC]	/* restart mutex_exit() */
	blo	10f

	/*
	 * If we interrupted the mutex_owner_running() critical region we
	 * must reset the PC back to the beginning to prevent dereferencing
	 * of a freed thread pointer. See the comments in mutex_owner_running
	 * for details.
	 */
	ldr	r5, [r5]
	sub	r0, r1, r4
	cmp	r0, r5
	strlo	r4, [sp, #ROFF_PC]	/* restart mutex_owner_running() */

10:
	RESTORE_REGS_SVC		/* exit */
	/*NOTREACHED*/

.Lsvc_rtt_preempt:
	ldrb	r1, [r5, #T_PREEMPT_LK]
	cmp	r1, #0			/* Already in kpreempt? */
	bne	.Lsvc_rtt_preempt_ret	/* if so, get out of here */
	mov	r0, #1
	strb	r0, [r5, #T_PREEMPT_LK]	/* otherwise, set t_preempt_lk */

	/* Adjust stack alignment before C function call. */
	ARM_EABI_STACK_ALIGN(r4)

	cpsie	i			/* enable interrupt */
	bl	kpreempt		/* kpreempt(1) */
	cpsid	i			/* disable interrupt */
	ARM_EABI_STACK_RESTORE(, r4)	/* Restore stack pointer. */

	mov	r0, #0
	strb	r0, [r5, #T_PREEMPT_LK]	/* clear t_preempt_lk */
	b	.Lsvc_rtt_preempt_ret

	.align	4
.Lcritical_section:
	/* Critical section in mutex_exit() */
	.word	mutex_exit_critical_start
	.word	mutex_exit_critical_size

	/* Critical section in mutex_owner_running() */
	.word	mutex_owner_running_critical_start
	.word	mutex_owner_running_critical_size

	SET_SIZE(svc_rtt)
	SET_SIZE(user_rtt)
	SET_SIZE(_sys_rtt)

#endif	/* __lint */

/*
 * lwp_rtt - start execution in newly created LWP.
 */
#if defined(__lint)

void
lwp_rtt_initial(void)
{}

void
lwp_rtt(void)
{}

#else	/* __lint */

ENTRY_NP(lwp_rtt_initial)
	THREADP(r0)
	ldr	sp, [r0, #T_STACK]	/* switch to the thread stack */
	bl	__dtrace_probe___proc_start
	b	0f

ENTRY_NP(lwp_rtt)
	THREADP(r0)
	ldr	sp, [r0, #T_STACK]	/* switch to the thread stack */
0:
	bl	__dtrace_probe___proc_lwp__start
	bl	dtrace_systrace_rtt
	ldr	r0, [sp, #ROFF_R0]
	ldr	r1, [sp, #ROFF_R1]
	adr	lr, user_rtt		/* return to user_rtt */
	b	post_syscall		/* post_syscall(rval1, rval2) */
	SET_SIZE(lwp_rtt)
	SET_SIZE(lwp_rtt_initial)

#endif	/* __lint */
