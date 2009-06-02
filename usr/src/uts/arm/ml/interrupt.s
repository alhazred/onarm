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

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved					*/

/*	Copyright (c) 1987, 1988 Microsoft Corporation		*/
/*	  All Rights Reserved					*/

/*
 * Copyright (c) 2006-2007 NEC Corporation
 */

	.ident	"@(#)arm/ml/interrupt.s"
	.file	"interrupt.s"

#ifndef	_MACHDEP
#error	interrupt.s must be linked to platform-specific module.
#endif	/* !_MACHDEP */

/*
 * Common part of IRQ handler.
 *
 * REVISIT: traptrace is not yet supported.
 */

#include <sys/asm_linkage.h>
#include <sys/cpuvar_impl.h>
#include <sys/regset.h>
#include <sys/mpcore.h>
#ifdef	__lint
#include <sys/types.h>
#include <sys/thread.h>
#include <sys/systm.h>
#else   /* __lint */
#include <sys/pcb.h>
#include <sys/trap.h>
#include <sys/ftrace.h>
#include <sys/irqtrap.h>
#endif	/* lint */

#ifndef	__lint
/*
 * intr_thread() is a part of irqtrap().
 * irqtrap() branches here without link.
 *
 * Calling and Exit State:
 *	irqtrap() must guarantee that:
 *	  - Interrupt is disabled by I bit in CPSR.
 *	  - r4 keeps struct regs address in stack, that is original
 *	    stack pointer.
 *	  - r5 keeps struct cpu address.
 *	  - r6 keeps current PIL for interrupted context.
 *	  - r7 keeps IRQ number.
 *	  - r8 keeps new PIL for this IRQ handler.
 */
ENTRY_NP(intr_thread)
	mov	r0, r5		/* arg0 = struct cpu */
	mov	r1, r4		/* arg1 = original stack pointer */
	mov	r2, r8		/* arg2 = new PIL */
	bl	intr_thread_prolog
	mov	r9, sp		/* save interrupted thread sp */
	mov	sp, r0		/* Install new stack pointer */
	mov	r1, r0

	/*
	 * Push old sp into the bottom of new stack.
	 * It's useful for debugging.
	 */
	IRQTRAP_PUSH_OLDSP(, r4, sp)

	cpsie	i		/* Enable iRQ */

	/*
	 * Fast event tracing.
	 */
	ldr	r2, [r5, #CPU_FTRACE_STATE]
	adds	r2, r2, #FTRACE_ENABLED
	adreq	r0, .Lftrace_fmt
	moveq	r2, r7
	moveq	r3, r8
	bleq	ftrace_3_notick

	/* Dispatch interrupt handler. */
	mov	r0, r7
	bl	av_dispatch_autovect

	/*
	 * We must disable interrupt again to protect interrupt context,
	 * such as pool for interrupt thread.
	 */
	cpsid	i

	mov	r0, r5		/* arg0 = struct cpu */
	mov	r1, r7		/* arg1 = IRQ */
	mov	r2, r6		/* arg2 = old PIL */
	bl	intr_thread_epilog

	/* IRQ (in r7) will be unmasked in intr_thread_epilog() */
	mov	sp, r9		/* restore stack pointer */

	/* cpu->cpu_m.mcpu_softinfo.st_pending into r2 */
	ldr	r2, [r5, #CPU_SOFTINFO]
	cmp	r2, #0
	bne	dosoftint

	ARM_EABI_STACK_RESTORE(, r4)	/* Restore stack pointer. */
	b	_sys_rtt		/* return from trap */
.Lftrace_fmt:
	.string	"intr_thread(): regs=0x%lx, int=0x%x, pil=0x%x"
	SET_SIZE(intr_thread)

/*
 * dosoftint() is a part of irqtrap().
 * irqtrap() branches here without link.
 *
 * Calling and Exit State:
 *	irqtrap() must guarantee that:
 *	  - Interrupt is disabled by I bit in CPSR.
 *	  - r2 keeps value of cpu->cpu_m.mcpu_softinfo.st_pending.
 *	  - r4 keeps struct regs address in stack, that is original
 *	    stack pointer.
 *	  - r5 keeps struct cpu address.
 *	  - r6 keeps current PIL for interrupted context.
 *	  - r7 keeps IRQ number.
 */
ENTRY_NP(dosoftint)
	mov	r0, r5	/* arg0 = struct cpu */
	mov	r1, r4	/* arg1 = struct regs */
			/* arg2 = software pending status */
	mov	r3, r6	/* arg3 = old PIL */
	bl	dosoftint_prolog

	/*
	 * If dosoftint_prolog() returns NULL, it means that the current PIL
	 * is out of suitable range for software interrupt. So we must
	 * resume interrupt if it returns NULL.
	 */
	cmp	r0, #0
	ARM_EABI_STACK_RESTORE(eq, r4)	/* Restore stack pointer */
	beq	_sys_rtt	/* return from trap */

	mov	r9, sp		/* save interrupted thread sp */
	mov	sp, r0		/* t_stk from interrupt thread */

	/*
	 * Push old sp into the bottom of new stack.
	 * It's useful for debugging.
	 */
	IRQTRAP_PUSH_OLDSP(, r4, sp)

	/*
	 * Enable interrupts before calling handler, so that higher level
	 * interrupt can run.
	 */
	cpsie	i
	THREADP(r0)
	ldrb	r0, [r0, #T_PIL]
	bl	av_dispatch_softvect

	cpsid	i		/* Disable IRQ */

	mov	r0, r5		/* arg0 = struct cpu */
	mov	r1, r6		/* arg1 = old PIL */
	bl	dosoftint_epilog

	mov	sp, r9		/* Restore stack pointer. */

	/*
	 * Check whether anoter software interrupt is pending or not.
	 * Note that we must load softinfo into r2.
	 */
	ldr	r2, [r5, #CPU_SOFTINFO]
	cmp	r2, #0
	bne	dosoftint

	ARM_EABI_STACK_RESTORE(, r4)	/* Restore stack pointer */
	b	_sys_rtt	/* return from trap */
	SET_SIZE(dosoftint)
#endif	/* __lint */

#ifndef	_LITTLE_ENDIAN
#error	"set_base_spl() assumes little endian system."
#endif	/* !_LITTLE_ENDIAN */

#ifdef	__lint

void
set_base_spl(void)
{}

#else	/* __lint */

/*
 * void
 * set_base_spl(void)
 *	Update base SPL for the current CPU, according to active interrupt
 *	level mask in struct cpu.
 *
 * Remarks:
 *	The caller must guarantee that preemption is disabled.
 */
ENTRY_NP(set_base_spl)
	/* load active interrupts mask */
	LOADCPU(r3)

	/*
	 * We use only lower 16 bits as active interrupt mask.
	 */
	ldrh	r0, [r3, #CPU_INTR_ACTV]

	cmp	r0, #0
	clzne	r1, r0
	rsbne	r0, r1, #31

	str	r0, [r3, #CPU_BASE_SPL]	/* store base priority */
	bx	lr

	SET_SIZE(set_base_spl)
#endif	/* __lint */
