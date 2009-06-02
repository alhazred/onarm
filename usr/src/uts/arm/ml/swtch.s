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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2006-2008 NEC Corporation
 */

	.ident	"@(#)arm/ml/swtch.s"

/*
 * Process switching routines.
 */

#if defined(__lint)
#include <sys/thread.h>
#include <sys/systm.h>
#include <sys/time.h>
#else	/* __lint */
#include "assym.h"
#endif	/* __lint */

#include <sys/asm_linkage.h>
#include <sys/cpuvar_impl.h>
#include <sys/pcb.h>
#include <sys/stack.h>
#include <asm/cpufunc.h>

/*
 * resume(thread_id_t t);
 *
 * a thread can only run on one processor at a time. there
 * exists a window on MPs where the current thread on one
 * processor is capable of being dispatched by another processor.
 * some overlap between outgoing and incoming threads can happen
 * when they are the same thread. in this case where the threads
 * are the same, resume() on one processor will spin on the incoming
 * thread until resume() on the other processor has finished with
 * the outgoing thread.
 *
 * The MMU context changes when the resuming thread resides in a different
 * process.  Kernel threads are known by resume to reside in process 0.
 * The MMU context, therefore, only changes when resuming a thread in
 * a process different from curproc.
 *
 * resume_from_intr() is called when the thread being resumed was not 
 * passivated by resume (e.g. was interrupted).  This means that the
 * resume lock is already held and that a restore context is not needed.
 * Also, the MMU context is not changed on the resume in this case.
 *
 * resume_from_zombie() is the same as resume except the calling thread
 * is a zombie and must be put on the deathrow list after the CPU is
 * off the stack.
 */

#if !defined(__lint)

#if LWP_PCB != 0
#error LWP_PCB MUST be defined as 0 for code in swtch.s to work
#endif	/* LWP_PCB != 0 */

#endif	/* !__lint */

#if defined(__lint)

/* ARGSUSED */
void
resume(kthread_t *t)
{}

#else	/* __lint */

/*
 * struct member offset used for ldrd/strd.
 * Those instructions can take only 8 bits immediate as register offset.
 */
#define	CPU_STATS_SYS_CPUMIGRATE_HI	(CPU_STATS_SYS_CPUMIGRATE & 0xff00)
#define	CPU_STATS_SYS_CPUMIGRATE_LO	(CPU_STATS_SYS_CPUMIGRATE & 0x00ff)

	ENTRY(resume)
	THREADP(r1)			/* r1 = curthread */
	add	r2, r1, #T_LABEL	/* r2 = &curthread->t_pcb */
	stmia	r2, {r4-r14}		/* save registers */
	mov	r4, r0			/* save arg0 */
	bl	__dtrace_probe___sched_off__cpu

	THREADP(r6)			/* r6 = old(curthread) */
	ldr	r5, [r6, #T_CPU]	/* r5 = CPU */

	/*
	 *	r4 = new thread
	 *	r5 = cpu pointer
	 *	r6 = old thread
	 *	r7 = new proc
	 */

	/*
	 * Call savectx if thread has installed context ops.
	 *
	 * Note that we don't call savepctx here, because context ops
	 * for process is not used in ARM.
	 */
	ldr	r1, [r6, #T_CTX]	/* r1 = old->t_ctx */
	cmp	r1, #0			/* should current thread savectx? */
	movne	r0, r6
	blne	savectx			/* call savecvx(curthread) */

	/*
	 * Save user thread pointer to pcb.
	 * Save floating point context to pcb.
	 */
	ldr	r0, [r6, #T_LWP]	/* r0 = old->t_lwp */
	cmp	r0, #0			/* if (old->t_lwp != NULL) */
	mrcne	p15, 0, r2, c13, c0, 2	/*    r2 = Thread_ID_2 */
	READ_FPEXC(ne, r1)		/*    r1 = fpexc */
	strne	r2, [r0, #PCB_USERTP]	/*    lwp_pcb.pcb_usertp = r2 */
	tstne	r1, #FPEXC_EN		/*    if (fpexc & FPEXC_EN) */
	bne	.Lfp_save		/*       goto fp_save */
.Lfp_save_ret:

	/* 
	 * Temporarily switch to the idle thread's stack
	 */
	ldr	r0, [r5, #CPU_IDLE_THREAD]	/* r0 = idle thread pointer */

	/* 
	 * Set the idle thread as the current thread
	 */
	THREADP_SET(r0)			/* Thread_ID_4 = idle */
	str	r0, [r5, #CPU_THREAD]	/* CPU->cpu_thread = idle */
	ldr	sp, [r0, #T_SP]		/* sp = idle->t_sp */

	/*
	 * switch in the hat context for the new thread
	 */
	ldr	r7, [r4, #T_PROCP]	/* r7 = new->t_procp */
	ldr	r2, [r7, #P_AS]
	ldr	r0, [r2, #A_HAT]	/* r0 = new->t_procp->p_as->a_hat */
	bl	hat_switch		/* call hat_switch(a_hat) */

	/* 
	 * Clear and unlock previous thread's t_lock
	 * to allow it to be dispatched by another processor.
	 */
	mov	r3, #0
	mcr	p15, 0, r3, c7, c10, 5  /* Memory barrier */
	mov	r0, #0
	strb	r0, [r6, #T_LOCK]	/* old->t_lock = 0 */

	/*
	 * IMPORTANT: Registers at this point must be:
	 *	r4 = new thread
	 *	r5 = cpu pointer
	 *	r7 = new proc
	 *
	 * Current stack pointer must be aligned by STACK_ENTRY_ALIGN.
	 *
	 * Here we are in the idle thread, have dropped the old thread.
	 */
	ALTENTRY(_resume_from_idle)

	/* Clear exclusive monitor */
	clrex

	/*
	 * spin until dispatched thread's mutex has
	 * been unlocked. this mutex is unlocked when
	 * it becomes safe for the thread to run.
	 */
.Llocktry:
	add	r0, r4, #T_LOCK
	bl	lock_spin_try		/* lock_spin_try(&new->t_lock) */
	cmp	r0, #0
	beq	.Llockloop

	/*
	 * Fix CPU structure to indicate new running thread.
	 * Set pointer in new thread to the CPU structure.
	 */
	ldr	r0, [r4, #T_CPU]	/* r0 = new->t_cpu */
	cmp	r5, r0			/* if (new->t_cpu != CPU) */
	bne	.Lcpumigrate		/*     goto .Lcpumigrate   */

.Lcpumigrate_ret:
	THREADP_SET(r4)			/* Thread_ID_4 = new */
	str	r4, [r5, #CPU_THREAD]	/* CPU->cpu_thread = new */
	ldr	r0, [r4, #T_LWP]
	str	r0, [r5, #CPU_LWP]	/* CPU->cpu_lwp = new->t_lwp */

	cmp	r0, #0			/* if (new->t_lwp != NULL) */
	ldrne	r2, [r0, #PCB_USERTP]	/*     r2 = t_lwp->lwp_pcb.pcb_usertp */
	mcrne	p15, 0, r2, c13, c0, 2	/*     Thread_ID_2 = r2 */

	/*
	 * Switch to new thread's stack
	 */
	ldr	sp, [r4, #T_SP]		/* sp = new->t_sp */

	/*
	 * Call restorectx if context ops have been installed.
	 *
	 * Note that we don't call restorepctx here, because context ops
	 * for process is not used in ARM.
	 */
	ldr	r1, [r4, #T_CTX]	/* r1 = new->t_ctx */
	cmp	r1, #0			/* should resumed thread restorectx? */
	movne	r0, r4
	blne	restorectx		/* call restorectx(new thread) */

	/*
	 * If we are resuming an interrupt thread, store a timestamp 
	 * in the thread structure.
	 */
	ldr	r0, [r4, #T_FLAG]	/* r0 = new->t_flag */
	tst	r0, #T_INTR_THREAD	/* if (t_flag & T_INTR_THREAD) */
	beq     1f                      /*     goto 1f */
2:
	bl	arm_gettick		/* get total SCU ticks since boot */
	strd	r0, [r4, #T_INTR_START]	/* new->t_intr_start = arm_gettick()*/
1:		
	/*
	 * Set the priority as low as possible, blocking all interrupt threads
	 * that may be active.
	 */
	bl	__dtrace_probe___sched_on__cpu
	mov	r0, r4			/* r0 = new */
	add	r1, r0, #T_LABEL	/* r1 = &new->t_pcb */
	ldmia	r1, {r4-r14}		/* restore registers */
	b	spl0			/* return to the restored lr */
	/*NOTREACHED*/
		
.Llockloop:
	ldrb	r0, [r4, #T_LOCK]
	cmp	r0, #0
	beq	.Llocktry
	b	.Llockloop

.Lcpumigrate:
	/* cp->cpu_stats.sys.cpumigrate++ */
	add	r0, r5, #CPU_STATS_SYS_CPUMIGRATE_HI
	ldrd	r2, [r0, #CPU_STATS_SYS_CPUMIGRATE_LO]
	adds	r2, r2, #1
	adc	r3, r3, #0
	strd	r2, [r0, #CPU_STATS_SYS_CPUMIGRATE_LO]
	str	r5, [r4, #T_CPU]		/* new->t_cpu = CPU */
	b	.Lcpumigrate_ret

.Lfp_save:
	/*
	 * Save the floating point state.
	 *
	 *	r0 :    old klwp (set by caller)
	 *	r1 :    fpexc register (set by caller)
	 *	r2,r3 : temporary
	 *	r8 :    fpexc & ~(FPEXC_EN|FPEXC_EX)
	 *	r9 :    lwp_pcb.pcb_fpu.fpu_flags
	 *	r4~r7 : preserve
	 */
	ldr	r9, [r0, #PCB_FPU_FLAGS]
	bic	r8, r1, #(FPEXC_EN|FPEXC_EX)
	tst	r9, #FPU_VALID			/* if (FPU_VALID) */
	WRITE_FPEXC(ne, r8)			/*    disable fpu */
	bne	.Lfp_save_ret			/*    goto fp_save_ret */
	add	r3, r0, #PCB_FPU_FPREGS
	READ_FPSCR(r2)
	STORE_CP11(r3, c0, 16)			/* store fpu regs */
	str	r1, [r0, #PCB_FPU_FPEXC]
	str	r2, [r0, #PCB_FPU_FPSCR]
	orr	r9, r9, #FPU_VALID
	tst	r1, #FPEXC_EX
	READ_FPINST(ne, r2)
	READ_FPINST2(ne, r3)
	strne	r2, [r0, #PCB_FPU_FPINST]
	strne	r3, [r0, #PCB_FPU_FPINST2]
	str	r9, [r0, #PCB_FPU_FLAGS]	/* set FPU_VALID */
	WRITE_FPEXC(, r8)			/* disable fpu */
	b	.Lfp_save_ret

	SET_SIZE(_resume_from_idle)
	SET_SIZE(resume)

#endif	/* __lint */

#if defined(__lint)

/* ARGSUSED */
void
resume_from_zombie(kthread_t *t)
{}

#else	/* __lint */

	ENTRY(resume_from_zombie)
	THREADP(r1)			/* r1 = curthread */
	add	r2, r1, #T_LABEL	/* r2 = &curthread->t_pcb */
	stmia	r2, {r4-r14}		/* save registers */
	mov	r4, r0			/* save arg0 */
	bl	__dtrace_probe___sched_off__cpu

	THREADP(r6)			/* r6 = old(curthread) */
	ldr	r5, [r6, #T_CPU]	/* r5 = CPU */

	/*
	 *	r4 = new thread
	 *	r5 = cpu pointer
	 *	r6 = old thread
	 */

	/*
	 * Disable fpu.
	 */
	READ_FPEXC(, r0)
	tst	r0, #FPEXC_EN
	bicne	r0, r0, #(FPEXC_EN|FPEXC_EX)
	WRITE_FPEXC(ne, r0)

	/* 
	 * Temporarily switch to the idle thread's stack so that the zombie
	 * thread's stack can be reclaimed by the reaper.
	 */
	ldr	r0, [r5, #CPU_IDLE_THREAD]	/* r0 = idle thread pointer */
	ldr	sp, [r0, #T_SP]			/* sp = idle->t_sp */

	/* 
	 * Set the idle thread as the current thread.
	 */
	THREADP_SET(r0)			/* Thread_ID_4 = idle */
	str	r0, [r5, #CPU_THREAD]	/* CPU->cpu_thread = idle */

	/* switch in the hat context for the new thread */
	ldr	r7, [r4, #T_PROCP]	/* r7 = new->t_procp */
	ldr	r2, [r7, #P_AS]
	ldr	r0, [r2, #A_HAT]	/* r0 = new->t_procp->p_as->a_hat */
	bl	hat_switch		/* call hat_switch(a_hat) */

	/* 
	 * Put the zombie on death-row.
	 */
	mov	r0, r6
	bl	reapq_add		/* call reapq_add(old) */

	b	_resume_from_idle	/* finish job of resume */
	SET_SIZE(resume_from_zombie)

#endif	/* __lint */

#if defined(__lint)

/* ARGSUSED */
void
resume_from_intr(kthread_t *t)
{}

#else	/* __lint */

	ENTRY(resume_from_intr)
	THREADP(r1)			/* r1 = curthread */
	add	r2, r1, #T_LABEL	/* r2 = &curthread->t_pcb */
	stmia	r2, {r4-r14}		/* save registers */
	mov	r4, r0			/* save arg0 */
	bl	__dtrace_probe___sched_off__cpu

	THREADP(r6)			/* r6 = old(curthread) */
	ldr	r5, [r6, #T_CPU]	/* r5 = CPU */

	/*
	 *	r4 = new thread
	 *	r5 = cpu pointer
	 *	r6 = old thread
	 */

	THREADP_SET(r4)			/* Thread_ID_4 = new */
	str	r4, [r5, #CPU_THREAD]	/* CPU->cpu_thread = new */

	ldr	sp, [r4, #T_SP]		/* sp = new->t_sp */

#if	STACK_ENTRY_ALIGN == 8
	/*
	 * Adjust stack alignment before C function call.
	 * No need to save original sp because sp will be restored
	 * from label_t.
	 */
	bic	sp, sp, #(STACK_ENTRY_ALIGN - 1)
#endif	/* STACK_ENTRY_ALIGN == 8 */

	/* 
	 * Unlock outgoing thread's mutex dispatched by another processor.
	 */
	mov	r3, #0
	mcr	p15, 0, r3, c7, c10, 5  /* Memory barrier */
	mov	r0, #0
	strb	r0, [r6, #T_LOCK]	/* old->t_lock = 0 */

	/* Clear exclusive monitor */
	clrex

	/*
	 * If we are resuming an interrupt thread, store a timestamp in
	 * the thread structure.
	 */
	ldr	r0, [r4, #T_FLAG]	/* r0 = new->t_flag */
	tst	r0, #T_INTR_THREAD	/* if (!(t_flag & T_INTR_THREAD)) */
	beq	1f			/*     goto 1f */

2:
	bl	arm_gettick		/* get total SCU ticks since boot */
	strd	r0, [r4, #T_INTR_START]	/* new->t_intr_start = arm_gettick()*/
1:
	/*
	 * Restore non-volatile registers, then have spl0 return to the
	 * resuming thread's PC after first setting the priority as low as
	 * possible and blocking all interrupt threads that may be active.
	 */
	bl	__dtrace_probe___sched_on__cpu
	mov	r0, r4			/* r0 = new */
	add	r1, r0, #T_LABEL	/* r1 = &new->t_pcb */
	ldmia	r1, {r4-r14}		/* restore registers */
	b	spl0			/* return to the restored lr */
	/*NOTREACHED*/
	SET_SIZE(resume_from_intr)

#endif /* __lint */

#if defined(__lint)

void
thread_start(void)
{}

#else   /* __lint */

	ENTRY(thread_start)
	ldmia	sp!, {r0, r1, r2}
#if	STACK_ENTRY_ALIGN == 8
	add	sp, sp, #4
#endif	/* STACK_ENTRY_ALIGN == 8 */
	blx	r2		/* (*start)(arg, len); */

	bl	thread_exit	/* destroy thread if it returns. */
	/*NOTREACHED*/
	SET_SIZE(thread_start)

#endif  /* __lint */
