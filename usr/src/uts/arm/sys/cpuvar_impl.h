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
 * Copyright (c) 2007 NEC Corporation
 * All rights reserved.
 */

#ifndef	_SYS_CPUVAR_IMPL_H
#define	_SYS_CPUVAR_IMPL_H

#ident	"@(#)arm/sys/cpuvar_impl.h"

/*
 * cpuvar_impl.h:
 *	ARM specific struct cpu definitions.
 *	Kernel build tree private.
 */

#ifdef	_ASM

#include <asm/thread.h>
#include <asm/cpufunc.h>
#include "assym.h"

/* Load reg with pointer to per-CPU structure */
#define	LOADCPU(reg)							\
	THREADP(reg);							\
	ldr	reg, [reg, #T_CPU]

/* Load cpu_thread_lock address */
#define	LOADCPU_THREAD_LOCK(lock, cpu)					\
	add	lock, cpu, #CPU_THREAD_LOCK

/*
 * Disable preemption while accessing to struct cpu.
 * reg should be preserved by the caller.
 */
#define	CPU_PIN(reg)							\
	READ_CPSR(reg);							\
	cpsid	i			/* Disable IRQ */

/*
 * Restore preemption state changed by CPU_PIN().
 */
#define	CPU_UNPIN(reg)		RESTORE_INTR(reg)

#else	/* !_ASM */

#ifndef	_SYS_CPUVAR_H
#error	Do NOT include cpuvar_impl.h directly.
#endif	/* !_SYS_CPUVAR_H */

/*
 * Redefine cpu related macros.
 */
#undef	CPU
#undef	CPU_SELF

#define	CPU			(curthread->t_cpu)
#define	CPU_SELF(cp)		(cp)

/*
 * Redefine CPU_ON_INTR
 */
#undef	CPU_ON_INTR

#define	CPU_ON_INTR(cpup) ((cpup)->cpu_intr_actv >> (LOCK_LEVEL + 1))

#endif	/* _ASM */

#endif	/* !_SYS_CPUVAR_IMPL_H */
