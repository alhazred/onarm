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
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2006-2008 NEC Corporation
 */

#ident	"@(#)arm/ml/fast_trap.s"

#include <sys/asm_linkage.h>
#include <sys/asm_exception.h>
#include <sys/cpuvar_impl.h>

#if defined(__lint)

#include <sys/types.h>
#include <sys/thread.h>
#include <sys/klwp.h>
#include <sys/systm.h>
#include <sys/lgrp.h>
#include <sys/cpuvar.h>
#include <sys/time.h>
#include <sys/archsystm.h>

#else   /* __lint */

#include <sys/privregs.h>
#include <sys/stack.h>
#include <asm/thread.h>
#include "assym.h"

#endif	/* __lint */

/*
 * Define macros to adjust stack alignment for EABI mode.
 * On EABI mode, stack pointer must be 8-bytes aligned at function entry.
 */
#if	STACK_ENTRY_ALIGN == 8

/*
 * Adjust stack alignment.
 * We know that stack pointer is not 8 bytes aligned here, so we can adjust
 * stack pointer without register save.
 */
#define	FASTTRAP_EABI_STACK_ALIGN()					\
	sub	sp, sp, #4

#define	FASTTRAP_EABI_STACK_RESTORE()					\
	add	sp, sp, #4

/* Adjust stack pointer, and allocate stack frame. */
#define	FASTTRAP_EABI_STACK_ALLOC(size)					\
	sub	sp, sp, #(size + 4)

#else	/* STACK_ENTRY_ALIGN != 8 */

/*
 * Do nothing on OABI mode.
 */
#define	FASTTRAP_EABI_STACK_ALIGN()
#define	FASTTRAP_EABI_STACK_RESTORE()

/* No need to align stack pointer. */
#define	FASTTRAP_EABI_STACK_ALLOC(size)					\
	sub	sp, sp, #(size)

#endif	/* STACK_ENTRY_ALIGN == 8 */

/*
 * Fast system calls
 */

#if defined(__lint)

void
fastswitrap(void)
{}

#else	/* __lint */

/*
 * Fast trap entry called from switrap.
 *
 * Don't return to caller. Fast trap handlers invoked by this entry
 * must return to user mode using RESTORE_REGS_FASTUSER macro.
 *
 * Caling status:
 *	ip:     trap type passed from libc.
 *	r0-r3:  scratch register. fast syscalls have no argument.
 *	[sp]:   pc on user mode.
 *	[sp+4]: cpsr on user mode.
 */
ENTRY(fastswitrap)
	bic	ip, #T_SWI_FASTCALL	/* ip &= ~T_SWI_FASTCALL */
	cmp	ip, #T_LASTFAST		/* if (ip > T_LASTFAST) */
	movhi	ip, #0			/*    ip = 0 */
	adr	r0, fasttable
	add	r0, r0, ip, lsl #2	/* r0 = &fasttable[ip] */
	ldr	pc, [r0]
	SET_SIZE(fastswitrap)

	.globl	fasttable
	.type	fasttable, %object
	.align	CPTRSHIFT
fasttable:
	.word	fast_null		/* T_FNULL */
	.word	fast_null		/* T_FGETFP */
	.word	fast_null		/* T_FSETFP */
	.word	get_hrtime		/* T_GETHRTIME */
	.word	gethrvtime		/* T_GETHRVTIME */
	.word	get_hrestime		/* T_GETHRESTIME */
	.word	getlgrp			/* T_GETLGRP */
	SET_SIZE(fasttable)

#endif	/* __lint */


#if defined(__lint)

hrtime_t
get_hrtime(void)
{ return (0); }

hrtime_t
get_hrestime(void)
{
	hrtime_t ts;

	gethrestime((timespec_t *)&ts);
	return (ts);
}

hrtime_t
gethrvtime(void)
{
	klwp_t *lwp = ttolwp(curthread);
	struct mstate *ms = &lwp->lwp_mstate;

	return (gethrtime() - ms->ms_state_start + ms->ms_acct[LMS_USER]);
}

uint64_t
getlgrp(void)
{
	return (((uint64_t)(curthread->t_lpl->lpl_lgrpid) << 32) |
			curthread->t_cpu->cpu_id);
}

#else	/* __lint */

/*
 * Fast trap handler for T_GETHRTIME.
 * Returns a 64-bit nanosecond timestamp in r0 and r1.
 */
ENTRY_NP(get_hrtime)
	FASTTRAP_EABI_STACK_ALIGN()
	bl	gethrtime		/* call gethrtime() */
	FASTTRAP_EABI_STACK_RESTORE()
	RESTORE_REGS_FASTUSER
	SET_SIZE(get_hrtime)

/*
 * Fast trap handler for T_GETHRESTIME.
 * Returns a timestruc_t in r0 and r1.
 */
ENTRY_NP(get_hrestime)
#if TIMESPEC_SIZE == 8
	FASTTRAP_EABI_STACK_ALLOC(TIMESPEC_SIZE)
	mov	r0, sp
	bl	gethrestime		/* call gethrestime(&tp) */
	ldmia	sp!, {r0, r1}
	FASTTRAP_EABI_STACK_RESTORE()
	RESTORE_REGS_FASTUSER
#else
#error port me
#endif
	SET_SIZE(get_hrestime)

/*
 * Fast trap handler for T_GETHRVTIME.
 * Returns a 64-bit lwp virtual time in r0 and r1, which is the number
 * of nanoseconds consumed.
 */
ENTRY_NP(gethrvtime)
	FASTTRAP_EABI_STACK_ALIGN()
	bl	gethrtime_unscaled		/* get time since boot */
	THREADP(r2)
	ldr	ip, [r2, #T_LWP]		/* ip = current lwp */
	ldr	r2, [ip, #LWP_MS_STATE_START]	/* r3_r2 = lwp_mstate */
	ldr	r3, [ip, #LWP_MS_STATE_START+4]	/*         .ms_state_start */
	subs	r0, r0, r2			/* time -= r3_r2 */
	sbc	r1, r1, r3
	ldr	r2, [ip, #LWP_ACCT_USER]	/* r3_r2 = lwp_mstate */
	ldr	r3, [ip, #LWP_ACCT_USER+4]	/*         .ms_acct[LWP_USER]*/
	adds	r0, r0, r2			/* time += r3_r2 */
	adc	r1, r1, r3
	stmfd	sp!, {r0-r1}
	mov	r0, sp
	bl	scalehrtime			/* scalehrtime(&hrt) */
	ldmfd	sp!, {r0-r1}
	FASTTRAP_EABI_STACK_RESTORE()
	RESTORE_REGS_FASTUSER
	SET_SIZE(gethrvtime)

/*
 * Fast trap handler for T_GETLGRP.
 */
ENTRY_NP(getlgrp)
	THREADP(r2)			/* r2 = curthread */
	ldr	r3, [r2, #T_LPL]
	ldr	r1, [r3, #LPL_LGRPID]	/* r1 = t->t_lpl->lpl_lgrpid */
	ldr	r3, [r2, #T_CPU]
	ldr	r0, [r3, #CPU_ID]	/* r0 = t->t_cpu->cpu_id */
	RESTORE_REGS_FASTUSER
	SET_SIZE(getlgrp)

ENTRY_NP(fast_null)
	ldr	r1, [sp, #4]		/* set carry bit in user flags */
	orr	r1, r1, #PSR_C_BIT
	str	r1, [sp, #4]
	RESTORE_REGS_FASTUSER
	SET_SIZE(fast_null)

#endif	/* __lint */
