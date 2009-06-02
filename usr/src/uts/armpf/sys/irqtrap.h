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

#ifndef	_SYS_IRQTRAP_H
#define	_SYS_IRQTRAP_H

#ident	"@(#)armpf/sys/irqtrap.h"

/*
 * Definitions for ARM platform low level IRQ handler.
 * Kernel build tree private.
 */

#ifdef	_KERNEL

#ifdef	_ASM

#include <sys/stack.h>
#include <asm/cpufunc.h>

#if	STACK_ENTRY_ALIGN == 8

/*
 * Define macros to adjust stack alignment for EABI mode.
 * On EABI mode, stack pointer must be 8-bytes aligned at function entry.
 */

/*
 * Adjust stack alignment.
 * Original stack pointer must be preserved by caller.
 */
#define	IRQTRAP_EABI_STACK_ALIGN()			\
	bic	sp, sp, #(STACK_ENTRY_ALIGN - 1)

/*
 * Push original stack pointer into the bottom of new stack.
 * On EABI mode, new stack must be 8-bytes aligned.
 * "cond" must be condition mnemonic for ARM instruction.
 */
#define	IRQTRAP_PUSH_OLDSP(cond, oldsp, newsp)		\
	ARM_INST(str, cond)	oldsp, [newsp, #-4]!;	\
	ARM_INST(sub, cond)	newsp, newsp, #4

#else	/* STACK_ENTRY_ALIGN != 8 */

/*
 * Adjust stack alignment.
 * Nothing to do here because stack pointer must be already 4 bytes aligned.
 */
#define	IRQTRAP_EABI_STACK_ALIGN()

/*
 * Push original stack pointer into the bottom of new stack.
 * "cond" must be condition mnemonic for ARM instruction.
 */
#define	IRQTRAP_PUSH_OLDSP(cond, oldsp, newsp)		\
	ARM_INST(str, cond)	oldsp, [newsp, #-4]!

#endif	/* STACK_ENTRY_ALIGN == 8 */


#else	/* !_ASM */

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/privregs.h>

extern volatile uint_t  panic_start_cpu; 
extern void	panic_idle(struct regs *rp);

/*
 * IRQTRAP_PANIC_CHECK(rp)
 *	Call panic_idle() if panicsys() is already called.
 */
#define	IRQTRAP_PANIC_CHECK(rp)					      \
	do {							      \
		if (panic_start_cpu != NCPU) {			      \
			/*					      \
			 * panicsys() was called. All CPUs except for \
			 * panicked CPU should spin here.	      \
			 */					      \
			panic_idle(rp);				      \
		}						      \
	} while (0)

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* _ASM */

#endif	/* _KERNEL */
#endif	/* !_SYS_IRQTRAP_H */
