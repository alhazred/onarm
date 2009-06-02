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

#ifndef	_SYS_ASM_EXCEPTION_H
#define	_SYS_ASM_EXCEPTION_H

#ident	"@(#)arm/sys/asm_exception.h"

/*
 * Common definitions for ARM exception handler.
 * Kernel build tree private.
 */

#ifdef	_ASM

#include "assym.h"

/*
 * Prologue of FIQ handler.
 * Currently, nothing to do.
 */
#define	FIQ_PROLOG()

/*
 * Prologue of svc_enter.
 * struct cpu address will be set into "cpu".
 */
#define	SVC_RTT_PROLOG(thread, cpu)					\
	cpsid	i;			/* Disable interrupt. */	\
	ldr	cpu, [thread, #T_CPU]

/*
 * Macros for saving/restoring registers.
 */

/* Save registers in exception entry except for swi */
#define	SAVE_REGS							\
	/* save lr_xxx, SPSR_xxx on svc stack */			\
	srsdb	#PSR_MODE_SVC!;						\
	cps	#PSR_MODE_SVC;		/* enter svc mode */		\
	stmdb	sp!, {sp,lr};		/* save sp_svc, lr_svc */	\
	sub	sp, sp, #(4*15);	/* adjust the stack pointer */	\
	stmia	sp, {r0-r14}^;		/* save user mode registers */	\
	mov	r0, r0			/* can't access banked regs */

/* Restore registers for returning to svc mode */
#define	RESTORE_REGS_SVC						\
	ldmia	sp, {r0-r12};		/* restore r0 - r12 */		\
	add	sp, sp, #(4*16);	/* adjust the stack pointer */	\
	ldr	lr, [sp], #4;		/* restore lr_svc */		\
	rfefd	sp!			/* restore pc, CPSR */

/* Save registers in swi entry */
#define	SAVE_REGS_SWI_ENTER						\
	srsdb	#PSR_MODE_SVC!;		/* save r_pc, r_cpsr */

/* Save registers in swi entry before calling syscall handler */
#define	SAVE_REGS_SWI_SYSCALL						\
	sub	sp, sp, #(4*17);					\
	stmia	sp, {r0-r14}^;		/* save r_r0 - r_r14 */		\
	mov	r0, r0;			/* can't access banked regs */

/* Restore registers for returning to user mode */
#define	RESTORE_REGS_USER		\
	ldmia	sp, {r0-r14}^;		/* restore r_r0 - r_r14 */	\
	mov	r0, r0;			/* can't access banked regs */	\
	add	sp, sp, #(4*17);					\
	rfeia	sp!;			/* restore r_pc, r_cpsr */

/* Restore registers for returning from fasttrap to user mode */
#define	RESTORE_REGS_FASTUSER		\
	rfeia	sp!;			/* restore r_pc, r_cpsr */

/* Save regs */
#define	SAVE_REGS_DABT	SAVE_REGS
#define	SAVE_REGS_UNDEF	SAVE_REGS
#define	SAVE_REGS_PABT	SAVE_REGS
#define	SAVE_REGS_FIQ	SAVE_REGS

#endif	/* _ASM */

#endif	/* !_SYS_ASM_EXCEPTION_H */
