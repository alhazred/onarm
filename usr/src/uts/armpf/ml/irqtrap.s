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

	.ident	"@(#)armpf/ml/irqtrap.s"
	.file	"irqtrap.s"

/*
 * IRQ handler for ARM platform.
 *
 * REVISIT: traptrace is not yet supported.
 */

#include <sys/asm_linkage.h>
#include <sys/asm_exception.h>
#include <sys/cpuvar_impl.h>
#include <sys/regset.h>
#include <sys/privregs.h>
#include <sys/mpcore.h>
#include <sys/machlock.h>
#ifdef	__lint
#include <sys/types.h>
#include <sys/thread.h>
#include <sys/systm.h>
#else   /* __lint */
#include <sys/irqtrap.h>
#endif	/* __lint */

#ifdef	__lint

void
irqtrap(void)
{}

#else	/* __lint */

/*
 * void
 * irqtrap(void)
 *	Entry function for IRQ trap.
 *
 * Register Usage:
 *	r4: struct regs address (original stack pointer)
 *	r5: struct cpu address
 *	r6: current IPL for interrupted context
 *	r7: IRQ number
 *	r8: IPL for this interrupt
 *
 *	Other registers are free to use.
 */
ENTRY_NP(irqtrap)
	sub	lr, lr, #4
	SAVE_REGS

	mov	r4, sp			/* pointer to struct regs */
	LOADCPU(r5)			/* r5 = current struct cpu. */
	ldr	r6, [r5, #CPU_PRI]	/* r6 = current PIL */

	/* get irq num */
	ldr	r0, =MPCORE_CPUIF_INTACK_VADDR
	ldr	r0, [r0, #0]

	/* Do nothing for invalid IRQ. */
	mov	r2, #0x400
	sub	r2, r2, #1		/* r2 = 1024 - 1 = 0x3ff */
	and	r0, r0, r2
	cmp	r0, r2
	beq	_sys_rtt

	/*
	 * Use IRQ num as autovect[] index.
	 */
	mov	r7, r0

	/*
	 * Adjust stack alignment before C function call.
	 * Old stack pointer is saved in r4.
	 */
	IRQTRAP_EABI_STACK_ALIGN()

	/*
	 * Mask and ack irq. This irq will be unmasked in hilevel_intr_epilog()
	 * or intr_thread_epilog().
	 */
	bl	gic_mask_irq		/* mask irq */
	mov	r0, r7			/* r0 = irq */
	bl	gic_ack_irq		/* eoi */

	/*
	 * Raise PIL according to this IRQ.
	 * New level is stored into r8.
	 */
	mov	r0, r7			/* r0 = irq */
	bl	setlvl
	mov	r8, r0

	/* If ipl == 0, nothing to do for this irq */
	cmp	r8, #0
	bne	0f
	mov	r0, r7			/* r0 = irq */
	bl	gic_unmask_irq		/* unmask irq */
	b	.Lcheck_softint

0:
	str	r8, [r5, #CPU_PRI]	/* update ipl */

	/*
	 * Check priority level.
	 * If new level is greater than LOCK_LEVEL, we invoke interrupt
	 * handler on current thread. Otherwise, switch to interrupt
	 * thread and dispatch interrupt handler on it.
	 */
	cmp	r8, #LOCK_LEVEL
	ble	intr_thread

	/*
	 * Handle this IRQ as high level interrupt.
	 */
	mov	r0, r5		/* arg0 = struct cpu */
	mov	r1, r8		/* arg1 = new PIL */
	mov	r2, r6		/* arg2 = old PIL */
	mov	r3, r4		/* arg3 = struct regs */
	bl	hilevel_intr_prolog

	/* hilevel_intr_prolog() returns 0 if we need to change stack. */
	cmp	r0, #0

	/*
	 * Save the thread stack and get on the cpu's interrupt stack
	 */
	moveq	r9, sp
	ldreq	sp, [r5, #CPU_INTR_STACK]

	/*
	 * Push old sp into the bottom of new stack.
	 * It's useful for debugging.
	 */
	IRQTRAP_PUSH_OLDSP(eq, r4, sp)

	cpsie	i		/* Enable irq */

	/*
	 * Dispatch interrupt handler corresponding to this IRQ.
	 */
	mov	r0, r7
	bl	av_dispatch_autovect

	cpsid	i

	mov	r0, r5		/* arg0 = struct cpu */
	mov	r1, r8		/* arg1 = new PIL */
	mov	r2, r6		/* arg2 = old PIL */
	mov	r3, r7		/* arg3 = IRQ */
	bl	hilevel_intr_epilog

	/*
	 * hilevel_intr_epilog() returns 0 if we need to restore
	 * stack address.
	 */
	cmp	r0, #0
	moveq	sp, r9

.Lcheck_softint:
	/*
	 * Dispatch software interrupts if pending.
	 *
	 * Assumptions:
	 * - IRQ has been unmasked.
	 * - r5 keeps struct cpu address.
	 * - r6 keeps old PIL
	 */
	ldr	r2, [r5, #CPU_SOFTINFO]
	cmp	r2, #0
	bne	dosoftint

	ARM_EABI_STACK_RESTORE(, r4)	/* Restore stack pointer. */
	b	_sys_rtt		/* return from trap */

	SET_SIZE(irqtrap)
#endif	/* __lint */
