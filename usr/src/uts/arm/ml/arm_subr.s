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
 *  Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.
 *  Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T
 *    All Rights Reserved
 */

/*
 * Copyright (c) 2006-2008 NEC Corporation
 */

	.ident	"@(#)arm_subr.s"
	.file	"arm_subr.s"

/*
 * General assembly language routines.
 * It is the intent of this file to contain routines that are
 * independent of the specific kernel architecture, and those that are
 * common across kernel architectures.
 * As architectures diverge, and implementations of specific
 * architecture-dependent routines change, the routines should be moved
 * from this file into the respective ../`arch -k`/subr.s file.
 */

#include <sys/asm_linkage.h>
#include <sys/panic.h>
#include <sys/ontrap.h>
#include <sys/regset.h>
#include <sys/privregs.h>
#include <sys/reboot.h>
#include <sys/stack.h>
#include <asm/thread.h>
#include "assym.h"

/*
 * int
 * on_fault(label_t *lp)
 *	Catch lofault faults. Like setjmp except it returns one
 *	if code following causes uncorrectable fault. Turned off by
 *	calling no_fault().
 */
ENTRY(on_fault)
	mov	r2, #0
	mcr	p15, 0, r2, c7, c10, 4	/* Memory synchronization barrier */
	THREADP(r1)			/* r1 = curthread() */
	str	r0, [r1, #T_ONFAULT]	/* Put jmpbuf address in t_onfault */
	adr	r2, catch_fault
	str	r2, [r1, #T_LOFAULT]	/* put catch_fault in t_lofault */
	b	setjmp			/* Let setjmp() do the rest of work */

catch_fault:
	THREADP(r1)			/* r1 = curthread() */
	ldr	r0, [r1, #T_ONFAULT]	/* r0 = jmpbuf address */
	mov	r2, #0
	mcr	p15, 0, r2, c7, c10, 4	/* Memory synchronization barrier */
	str	r2, [r1, #T_ONFAULT]	/* Turn off t_onfault */
	str	r2, [r1, #T_LOFAULT]	/* Turn off t_lofault */
	b	longjmp			/* Let longjmp() do the rest of work */
	SET_SIZE(on_fault)

/*
 * void
 * no_fault(void)
 *	Turn off fault catching.
 */
ENTRY(no_fault)
	THREADP(r1)			/* r1 = curthread() */
	mov	r2, #0
	mcr	p15, 0, r2, c7, c10, 4	/* Memory synchronization barrier */
	str	r2, [r1, #T_ONFAULT]	/* Turn off t_onfault */
	str	r2, [r1, #T_LOFAULT]	/* Turn off t_lofault */
	mov	pc, lr
	SET_SIZE(no_fault)

/*
 * void
 * on_trap_trampoline(void)
 *	Default trampoline code for on_trap() (see <sys/ontrap.h>).
 *	We just do a longjmp(&curthread->t_ontrap->ot_jmpbuf) if this is
 *	ever called.
 */
ENTRY(on_trap_trampoline)
	THREADP(r2)			/* r2 = curthread() */
	ldr	r0, [r2, #T_ONTRAP]	/* r0 = curthread->t_ontrap */
	add	r0, r0, #OT_JMPBUF	/* r0 = &t_ontrap->ot_jmpbuf */
	b	longjmp			/* Let longjmp() do the rest of work */
	SET_SIZE(on_trap_trampoline)

/*
 * int
 * on_trap(on_trap_data_t *otp, uint_t prot)
 *	Push a new element on to the t_ontrap stack.
 *	Refer to <sys/ontrap.h> for more information about the on_trap()
 *	mechanism.  If the on_trap_data is the same as the topmost stack
 *	element, we just modify that element.
 *	On ARMv6, we need to issue memory synchronization barrier before
 *	modifying t_ontrap. The issue barrier is defined to force all deferred
 *	errors to complete before we go any further. We want these errors to
 *	be processed before we modify our current error protection.
 */
ENTRY(on_trap)
	mov	r3, #0
	mcr	p15, 0, r3, c7, c10, 4	/* Memory synchronization barrier */
	strh	r1, [r0, #OT_PROT]	/* otp->ot_prot = prot */
	strh	r3, [r0, #OT_TRAP]	/* otp->ot_trap = 0 */
	adr	r1, on_trap_trampoline	/* r1 = &on_trap_trampoline */
	str	r1, [r0, #OT_TRAMPOLINE] /* otp->ot_trampoline = r1 */
	str	r3, [r0, #OT_HANDLE]	/* otp->ot_handle = NULL */
	str	r3, [r0, #OT_PAD1]	/* otp->ot_pad1 = NULL */
	THREADP(r1)			/* r1 = curthread() */
	ldr	r2, [r1, #T_ONTRAP]
	teq	r0, r2			/* if (otp != r1->t_ontrap) */
	strne	r2, [r0, #OT_PREV]	/*   otp->ot_prev = r1->t_ontrap */
	strne	r0, [r1, #T_ONTRAP]	/*   r1->t_ontrap = otp */
	mcrne	p15, 0, r3, c7, c10, 4	/*   Memory synchronization barrier */

	add	r0, r0, #OT_JMPBUF	/* r0 = &otp->ot_jmpbuf */
	b	setjmp			/* Let setjmp() do the rest of work */
	SET_SIZE(on_trap)

/*
 * int
 * setjmp(label_t *lp)
 *	Save general registers for succeeding longjmp().
 */
ENTRY(setjmp)
	stmia	r0, {r4-r14}
	mov	r0, #0
	mov	pc, lr
	SET_SIZE(setjmp)

/*
 * void
 * longjmp(label_t *lp)
 *	Restore environment saved by setjmp().
 */
ENTRY(longjmp)
	ldmia	r0, {r4-r14}
	mov	r0, #1
	mov	pc, lr
	SET_SIZE(longjmp)

/*
 * caddr_t
 * caller(void)
 *	Returns the caller address.
 *	If a() calls b() and b() calls caller(), caller() returns address
 *	in a().
 *
 * Remarks:
 *	This function doesn't work correctly.
 *	Use inline function in asm/thread.h.
 */
#if	0
ENTRY(caller)
	mov	pc, lr
	SET_SIZE(caller)
#endif	/* 0 */

/*
 * caddr_t
 * callee(void)
 *	Returns the callee address.
 *	If a() calls callee(), callee() returns the return address in a();
 */
ENTRY(callee)
	mov	r0, lr
	mov	pc, lr
	SET_SIZE(callee)

/*
 * greg_t
 * getfp(void)
 *	Return frame pointer address.
 *
 * Remarks:
 *	ARM architecture has no frame pointer strictly defined by HW.
 *	So getfp() returns stack pointer.
 */
ENTRY(getfp)
	mov	r0, sp
	mov	pc, lr
	SET_SIZE(getfp)

/*
 * void
 * _insque(caddr_t entryp, caddr_t predp)
 *	Insert entryp after predp in a doubly linked list.
 */
ENTRY(_insque)
	ldr	r2, [r1]		/* r2 = predp->forw */
	str	r1, [r0, #CPTRSIZE]	/* entryp->back = predp */
	str	r2, [r0]		/* entryp->forw = predp->forw */
	str	r0, [r1]		/* predp->forw = entryp */
	str	r0, [r2, #CPTRSIZE]	/* predp->forw->back = entryp */
	mov	pc, lr
	SET_SIZE(_insque)

/*
 * void
 * _remque(caddr_t entryp)
 *	Remove entryp from a doubly linked list
 */
ENTRY(_remque)
	ldr	r1, [r0]		/* r1 = entryp->forw */
	ldr	r2, [r0, #CPTRSIZE]	/* r2 = entryp->back */
	str	r1, [r2]		/* entryp->back->forw = entryp->forw */
	str	r2, [r1, #CPTRSIZE]	/* entryp->forw->back = entryp->back */
	mov	pc, lr
	SET_SIZE(_remque)

/*
 * int
 * scanc(size_t length, uchar_t *string, uchar_t *table, uchar_t mask)
 *	VAX scanc instruction.
 */
ENTRY(scanc)
	stmfd	sp!, {r4-r5}
	add	r5, r1, r0		/* r5 = endp = string + length */
.Lscanc_loop:
	cmp	r1, r5			/* if (string >= endp) */
	bhs	.Lscanc_done		/*   break */
	ldrb	r4, [r1], #1		/* r4 = *string, string++ */
	ldrb	r4, [r2, r4]		/* r4 = *(table + r4) */
	tst	r4, r3			/* if ((r4 & mask) == 0) */
	beq	.Lscanc_loop		/*   goto .Lscanc_loop */
	sub	r1, r1, #1		/* string-- */
.Lscanc_done:
	sub	r0, r5, r1		/* return r1 - string */
	ldmfd	sp!, {r4-r5}
	mov	pc, lr
	SET_SIZE(scanc)

/*
 * dtrace_icookie_t
 * dtrace_interrupt_disable(void)
 *	Disable interruption for dtrace.
 *
 * Calling/Exit State:
 *	This function returns current CPSR value.
 */
ENTRY(dtrace_interrupt_disable)
	mrs	r0, cpsr
	cpsid	i
	mov	pc, lr
	SET_SIZE(dtrace_interrupt_disable)

/*
 * void
 * dtrace_interrupt_enable(dtrace_icookie_t cookie)
 *	Enable interrption disabled by dtrace_interrupt_disable().
 */
ENTRY(dtrace_interrupt_enable)
	msr	cpsr_c, r0
	mov	pc, lr
	SET_SIZE(dtrace_interrupt_enable)

/*
 * Memory barrier functions for dtrace.
 */
ENTRY(dtrace_membar_producer)
ALTENTRY(dtrace_membar_consumer)
	mov	r0, #0
	mcr	p15, 0, r0, c7, c10, 5
	mov	pc, lr
	SET_SIZE(dtrace_membar_consumer)
	SET_SIZE(dtrace_membar_producer)

#define	PANIC_MAGIC	0xDEFACEDD

.Lpanic_magic:
	.word	PANIC_MAGIC

/*
 * int
 * panic_trigger(int *tp)
 * int
 * dtrace_panic_trigger(int *tp)
 *	A panic trigger is a word which is updated atomically and can only
 *	be set once.  We atomically store 0xDEFACEDD and load the old value.
 *	If the previous value was 0, we succeed and return 1; otherwise
 *	return 0.
 *	This allows a partially corrupt trigger to still trigger correctly.
 *	DTrace has its own version of this function to allow it to panic
 *	correctly from probe context.
 */

#define	PANIC_TRIGGER()							\
	ldr	r3, .Lpanic_magic;	/* r3 = PANIC_MAGIC */		\
1:									\
	ldrex	r2, [r0];						\
	teq	r2, #0;			/* if (*tp != 0) */		\
	movne	r0, #0;							\
	bne	10f;			/*   return 0 */		\
	strex	r2, r3, [r0];		/* Install magic */		\
	teq	r2, #0;							\
	bne	1b;			/* Retry when fails */		\
	mov	r0, #1;			/* return 1 */			\
10:									\
	mov	pc, lr
	
ENTRY_NP(panic_trigger)
	PANIC_TRIGGER()
	SET_SIZE(panic_trigger)

ENTRY_NP(dtrace_panic_trigger)
	PANIC_TRIGGER()
	SET_SIZE(dtrace_panic_trigger)

/*
 * void
 * panic(const char *format, ...)
 *
 *	va_list alist;
 *	va_start(alist, format);
 *	vpanic(format, alist);
 *	va_end(alist);
 */
ENTRY_NP(panic)
	stmdb	sp!, {r0-r3}
	str	lr, [sp, #-4]!
	add	r1, sp, #8
#if	STACK_ENTRY_ALIGN == 8
	sub	sp, sp, #4
#endif	/* STACK_ENTRY_ALIGN == 8 */
	bl	vpanic
10:
	b	10b			/* NOTREACHED */
	SET_SIZE(panic)

/*
 * void
 * vpanic(const char *format, va_list alist)
 * void
 * dtrace_vpanic(const char *format, va_list alist)
 *	The panic() and cmn_err() functions invoke vpanic() as a common
 *	entry point into the panic code implemented in panicsys().
 *	vpanic() is responsible for passing through the format string and
 *	arguments, and constructing a regs structure on the stack into which
 *	it saves the current register values.  If we are not dying due to
 *	a fatal trap, these registers will then be preserved in panicbuf
 *	as the current processor state.  Before invoking panicsys(),
 *	vpanic() activates the first panic trigger (see common/os/panic.c)
 *	and switches to the panic_stack if successful.  Note that DTrace
 *	takes a slightly different panic path if it must panic from probe
 *	context.  Instead of calling panic, it calls into dtrace_vpanic(),
 *	which sets up the initial stack as vpanic does, calls
 *	dtrace_panic_trigger(), and branches back into vpanic().
 */
.Lpanic_quiesce:
	.word	panic_quiesce
.Lpanic_stack:
	.word	panic_stack

	.globl	panic_quiesce
	.globl	panicsys
ENTRY_NP(vpanic)
	stmfd	sp!, {r0-r3, lr}		/* Save registers */

	/* Call panic_trigger(&panic_quiesce) */

#if	STACK_ENTRY_ALIGN == 8
	/*
	 * We don't need to adjust stack alignment because we know
	 * panic_trigger() never touches stack.
	 */
#endif	/* STACK_ENTRY_ALIGN == 8 */
	ldr	r0, .Lpanic_quiesce
	bl	panic_trigger

vpanic_common:
	teq	r0, #0				/* if (r0 == 0) */
	moveq	r1, sp				/*   r1 = sp */
	beq	0f				/*   goto 0f */

	/*
	 * If panic_trigger() was successful, we are the first to initiate a
	 * panic.
	 */
	ldr	r1, .Lpanic_stack		/* r1 = panic_stack */
	ldr	r2, =PANICSTKSIZE
	add	r1, r1, r2			/* r1 += PANICSTKSIZE */

0:
	sub	r1, r1, #REGSIZE		/* Allocate struct regs */

	/*
	 * Store the register values as they were when we entered vpanic()
	 * to the designated location in the regs structure we allocated on
	 * the panic stack. 
	 */
	add	r2, r1, #ROFF_R4
	stmia	r2, {r4-r12}			/* Save r4-r12 at once */

	/* Save stack pointer */
	add	r2, sp, #20			/* 4 * count(r0-r3, lr) = 20 */
	str	r2, [r1, #ROFF_SP]

	/* Save r0-r3, lr */
	ldr	r2, [sp, #0]
	str	r2, [r1, #ROFF_R0]
	ldr	r2, [sp, #4]
	str	r2, [r1, #ROFF_R1]
	ldr	r2, [sp, #8]
	str	r2, [r1, #ROFF_R2]
	ldr	r2, [sp, #12]
	str	r2, [r1, #ROFF_R3]
	ldr	r2, [sp, #16]
	str	r2, [r1, #ROFF_LR]

	/* Save vpanic() address as PC. */
	adr	r2, vpanic
	str	r2, [r1, #ROFF_PC]

	/* Save CPSR */
	mrs	r2, cpsr
	str	r2, [r1, #ROFF_CPSR]

	/* Now we switch to the new stack. */
	mov	sp, r1

	/* Call panicsys(). */
	mov	r3, r0				/* r3 = on_panic_stack */
	mov	r2, sp				/* r2 = &regs */
	ldr	r1, [sp, #ROFF_R1]		/* r1 = alist */
	ldr	r0, [sp, #ROFF_R0]		/* r0 = format */
#if	STACK_ENTRY_ALIGN == 8
	/* Adjust stack alignment */
	bic	sp, sp, #(STACK_ENTRY_ALIGN - 1)
#endif	/* STACK_ENTRY_ALIGN == 8 */
	bl	panicsys
10:
	b	10b				/* NOTREACHED */
	SET_SIZE(vpanic)

ENTRY_NP(dtrace_vpanic)
	stmfd	sp!, {r0-r3, lr}		/* Save registers */

	/* Call panic_trigger(&panic_quiesce) */
	ldr	r0, .Lpanic_quiesce
#if	STACK_ENTRY_ALIGN == 8
	/*
	 * We don't need to adjust stack alignment because we know
	 * dtrace_panic_trigger() is a leaf function, and it never touches
	 * stack.
	 */
#endif	/* STACK_ENTRY_ALIGN == 8 */
	bl	dtrace_panic_trigger
	b	vpanic_common
	SET_SIZE(dtrace_vpanic)

/*
 * void
 * hres_tick(void)
 *	Tick process for high resolution timer, called once per clock tick.
 */
ENTRY_NP(hres_tick)

	stmfd	sp!, {r4-r7, lr}	/* Save registers */
#if	STACK_ENTRY_ALIGN == 8
	sub	sp, sp, #4
#endif	/* STACK_ENTRY_ALIGN == 8 */

	/*
	 * We need to call *gethrtimef before picking up CLOCK_LOCK (obviously,
	 * hres_last_tick can only be modified while holding CLOCK_LOCK).
	 * At worst, performing this now instead of under CLOCK_LOCK may
	 * introduce some jitter in pc_gethrestime().
	 */

	bl	gethrtime	/* call gethrtime() and return nsec_since_boot(64bit).*/

	mov	r6, r0		/* load nsec_since_boot, lower 32bit. */
	mov	r7, r1		/* upper 32bit. */
				/* r6: lower 32bit, r7: upper 32bit. */

	/* Try to get hres_lock. */
	ldr	r4, =hres_lock	/* load hres_lock addr to r4 */
.Lhres_lock_try:
	mov	r0, r4
	bl	lock_spin_try	/* lock_spin_try(&hres_lock) */
	cmp	r0, #0
	beq	.Lhres_lock_loop
	b	.Lhres_lock_got

.Lhres_lock_loop:
	ldrb	r0, [r4]
	cmp	r0, #0
	beq	.Lhres_lock_try
	b	.Lhres_lock_loop

.Lhres_lock_got:
	/*
	 * compute the interval since last time hres_tick was called
	 * and adjust hrtime_base and hrestime accordingly
	 * hrtime_base is an 8 byte value (in nsec), hrestime is
	 * a timestruc_t (sec, nsec)
	 */

	/* diff = nsec_since_boot - hres_last_tick */
	ldr	r5, =hres_last_tick	/* load hres_last_tick addr. */
	ldrd	r0, [r5]		/* get hres_last_tick(64bit)  */
					/* r0: lower 32bit, r1: upper 32bit  */
	subs	r2, r6, r0		/* calc lower 32bit. */
	sbc	r3, r7, r1		/* calc upper 32bit. */

	/* hrtime_base += diff */
	ldr	r5, =hrtime_base	/* load hrtime_base addr. */
	ldrd	r0, [r5]		/* get 64bit value. (hrtime_base)  */
					/* r0: lower 32bit, r1: upper 32bit  */
	adds	r0, r0,	r2		/* calc lower 32bit. */
	adc	r1, r1, r3		/* calc upper 32bit. */
	strd	r0, [r5]		/* update hrtime_base.*/

	/* hrestime.tv_nsec += diff */
	ldr	r5, =hrestime		/* get hrestime addr. */
	ldr	r0, [r5, #4]		/* get hrestime.tv_nsec value. */
	add	r0, r0, r2		/* add interval to hrestime.tv_nsec. */
	str	r0, [r5, #4]		/* update hrestime.tv_nsec. */

	/* update hres_last_tick. */
	ldr	r5, =hres_last_tick	/* load hres_last_tick addr. */
	strd	r6, [r5]		/* update hres_last_tick. */

	/* call __adj_hrestime(). Adjustment hres timers' value.*/
	bl	__adj_hrestime

	/*
	 * release the hres_lock
	 */
	mov	r0, r4			/* hres_lock addr */
	mov	r1, #0
	mcr	p15, 0, r1, c7, c10, 5	/* Memory barrier */
1:
	ldrex	r1, [r0]		/* hres_lock + 1 */
	add	r1, r1, #1
	strex	r2, r1, [r0]
	teq	r2, #0
	bne	1b

#if	STACK_ENTRY_ALIGN == 8
	add	sp, sp, #4
#endif	/* STACK_ENTRY_ALIGN == 8 */
	ldmfd	sp!, {r4-r7, pc}        /* return */
	SET_SIZE(hres_tick)

/*
 * void
 * prefetch_smap_w(void *)
 * void
 * prefetch_page_r(page_t *)
 *	Prefetch ahead within a linear list of smap/page structures.
 *	Not implemented for ARM.  Stub for compatibility.
 */
ENTRY(prefetch_smap_w)
ALTENTRY(prefetch_page_r)
	mov	pc, lr
	SET_SIZE(prefetch_smap_w)
	SET_SIZE(prefetch_page_r)

/*
 * void
 * return_instr(void)
 *	Returns immediately.
 *	This is used entry point for stub functions.
 */
ENTRY_NP(return_instr)
	mov	pc, lr
	SET_SIZE(return_instr)

/*
 * Timer related variables.
 */
DGDEF3(hrestime, CLONGSIZE * 2, 5)	/* 32byte align */
	.long	0, 0

DGDEF3(hrestime_adj, CLONGLONGSIZE, CLONGLONGSHIFT)
	.long	0, 0

DGDEF3(hres_last_tick, CLONGLONGSIZE, CLONGLONGSHIFT)
	.long	0, 0

DGDEF3(timedelta, CLONGLONGSIZE, CLONGLONGSHIFT)
	.long	0, 0

DGDEF3(hres_lock, CLONGSIZE, CLONGSHIFT)
	.long	0

DGDEF3(hrtime_base, CLONGLONGSIZE, CLONGLONGSHIFT)
	.long	0, 0

DGDEF(adj_shift)
	.long	ADJ_SHIFT

/*
 * int
 * highbit(ulong_t i)
 *	Find highest one bit set.
 *	Returns bit number + 1 of highest bit that is set, otherwise returns 0.
 *	High order bit is 31 (or 63 in _LP64 kernel).
 */
ENTRY(highbit)
	clz	r1, r0
	rsb	r0, r1, #32
	mov	pc, lr
	SET_SIZE(highbit)

/*
 * void
 * panic_idle_saveregs(void)
 *	Save registers into stack, and pass them to panic_idle().
 *
 * Calling/Exit State:
 *	This function does not return if panic_idle() has not called yet.
 *
 * Remarks:
 *	This function must be called with preemtion disabled.
 */
ENTRY_NP(panic_idle_saveregs)
	str	lr, [sp, #-4]!

	/* Allocate struct regs */
	sub	sp, sp, #REGSIZE
	stmia	sp, {r0-r12}			/* Save general registers */

	/* Save original sp. */
	add	r0, sp, #(REGSIZE + 4)
	str	r0, [sp, #ROFF_SP]

	/* Save lr. */
	str	lr, [sp, #ROFF_LR]

	/* sp_svc and lr_svc is not used. */
	mov	r0, #0
	str	r0, [sp, #(ROFF_LR + 4)]
	str	r0, [sp, #(ROFF_LR + 8)]

	/* Save panic_idle_saveregs() as PC. */
	adr	r0, panic_idle_saveregs
	str	r0, [sp, #ROFF_PC]

	/* Save CPSR. */
	READ_CPSR(r0)
	str	r0, [sp, #ROFF_CPSR]

	/* Call panic_idle(). */
	mov	r0, sp
	bl	panic_idle

	/*
	 * Although panic_idle() should not return, we provide
	 * function epilogue.
 	 */
	add	sp, sp, #REGSIZE
	ldmia	sp!, {pc}
	SET_SIZE(panic_idle_saveregs)

/*
 * void
 * arm_set_modestack(uint_t mode, void *sp)
 *	Set stack pointer for the specified CPU mode.
 *	"mode" must be a value for PSR mode field. (e.g. PSR_MODE_SVC)
 *
 * Remarks:
 *	This function must be called while any interruption is disabled.
 */
ENTRY_NP(arm_set_modestack)
	mrs	r2, cpsr		/* Preserve current CPU mode */
	bic	r3, r2, #PSR_MODE
	orr	r3, r3, r0
	msr	cpsr_all, r3		/* Switch to the specified CPU mode */
	mov	sp, r1			/* Set stack pointer */
	msr	cpsr_all, r2		/* Back to original CPU mode */
	mov	pc, lr
	SET_SIZE(arm_set_modestack)

/*
 * ftrace_icookie_t
 * ftrace_interrupt_disable(void)
 *	FTRACE utility to disable interrupt.
 */
ENTRY_NP(ftrace_interrupt_disable)
	DISABLE_INTR_SAVE(r0)
	mov	pc, lr

/*
 * void
 * ftrace_interrupt_enable(ftrace_icookie_t cookie)
 *	FTRACE utility to enable interrupt.
 *	The caller must specify interrupt cookie returned by
 *	ftrace_interrupt_disable().
 */
ENTRY_NP(ftrace_interrupt_enable)
	RESTORE_INTR(r0)
	mov	pc, lr
