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
 * Copyright (c) 2006-2009 NEC Corporation
 */

#pragma ident	"@(#)machdep.c	1.20	05/06/08 SMI"

#include "thr_uberdata.h"
#include <procfs.h>
#include <ucontext.h>
#include <setjmp.h>

extern int getlwpstatus(thread_t, lwpstatus_t *);
extern int putlwpregs(thread_t, prgregset_t);

int
setup_context(ucontext_t *ucp, void *(*func)(ulwp_t *),
	ulwp_t *ulwp, caddr_t stk, size_t stksize)
{
	/*
	 * Top-of-stack must be rounded down to STACK_ALIGN and
	 * there must be a minimum frame for the register window.
	 */
	uintptr_t stack = (((uintptr_t)stk + stksize) & ~(STACK_ALIGN - 1));

	/*
	 * BIAS for syscall() and systemcall().
	 * See syscall.s in libc.
	 */
	stack = (uintptr_t)((char *)stack - (6 * sizeof(long)));

	/* clear the context */
	(void) _memset(ucp, 0, sizeof (*ucp));

	/* fill in registers of interest */
	ucp->uc_flags |= UC_CPU;
	ucp->uc_mcontext.gregs[REG_PC] = (greg_t)func;
	ucp->uc_mcontext.gregs[REG_R0] = (greg_t)ulwp;
	ucp->uc_mcontext.gregs[REG_SP] = (greg_t)stack;
	ucp->uc_mcontext.gregs[REG_LR] = (greg_t)_lwp_start;
	ucp->uc_mcontext.gregs[REG_TP] = (greg_t)ulwp;

	return (0);
}

/*
 * Machine-dependent startup code for a newly-created thread.
 */
void *
_thr_setup(ulwp_t *self)
{
	self->ul_ustack.ss_sp = (void *)(self->ul_stktop - self->ul_stksiz);
	self->ul_ustack.ss_size = self->ul_stksiz;
	self->ul_ustack.ss_flags = 0;
	(void) _private_setustack(&self->ul_ustack);
	tls_setup();

	/* signals have been deferred until now */
	sigon(self);

	return (self->ul_startpc(self->ul_startarg));
}

void
_fpinherit(ulwp_t *ulwp)
{
	ulwp->ul_fpuenv.ftag = 0xffffffff;
}

void
getgregs(ulwp_t *ulwp, gregset_t rs)
{
	lwpstatus_t status;

	if (getlwpstatus(ulwp->ul_lwpid, &status) == 0) {
		rs[REG_PC] = status.pr_reg[REG_PC];
		rs[REG_SP] = status.pr_reg[REG_SP];
		rs[REG_LR] = status.pr_reg[REG_LR];
	} else {
		rs[REG_PC] = 0;
		rs[REG_SP] = 0;
		rs[REG_LR] = 0;
	}
}

void
setgregs(ulwp_t *ulwp, gregset_t rs)
{
	lwpstatus_t status;

	if (getlwpstatus(ulwp->ul_lwpid, &status) == 0) {
		status.pr_reg[REG_PC] = rs[REG_PC];
		status.pr_reg[REG_SP] = rs[REG_SP];
		status.pr_reg[REG_LR] = rs[REG_LR];
		(void) putlwpregs(ulwp->ul_lwpid, status.pr_reg);
	}
}

int
__csigsetjmp(sigjmp_buf env, int savemask,
	     greg_t wr4,  greg_t wr5,  greg_t wr6,  greg_t wr7,  greg_t wr8,
	     greg_t wr9,  greg_t wr10, greg_t wr11, greg_t wr13, greg_t wr14,
	     greg_t wrcp)
{
	ucontext_t *ucp = (ucontext_t *)env;
	ulwp_t *self = curthread;

	/* float register save is getcontext() and uc_flag |= UC_FPU */
	getcontext(ucp) ;

	ucp->uc_link = self->ul_siglink;
	if (self->ul_ustack.ss_flags & SS_ONSTACK)
		ucp->uc_stack = self->ul_ustack;
	else {
		ucp->uc_stack.ss_sp =
			(void *)(self->ul_stktop - self->ul_stksiz);
		ucp->uc_stack.ss_size = self->ul_stksiz;
		ucp->uc_stack.ss_flags = 0;
	}
	ucp->uc_flags = UC_STACK | UC_CPU | UC_FPU;
	if (savemask) {
		ucp->uc_flags |= UC_SIGMASK;
		enter_critical(self);
		ucp->uc_sigmask = self->ul_sigmask;
		exit_critical(self);
	}
        ucp->uc_mcontext.gregs[REG_R4] = wr4;
        ucp->uc_mcontext.gregs[REG_R5] = wr5;
        ucp->uc_mcontext.gregs[REG_R6] = wr6;
        ucp->uc_mcontext.gregs[REG_R7] = wr7;
        ucp->uc_mcontext.gregs[REG_R8] = wr8;
        ucp->uc_mcontext.gregs[REG_R9] = wr9;
        ucp->uc_mcontext.gregs[REG_R10] = wr10;
        ucp->uc_mcontext.gregs[REG_R11] = wr11;
        ucp->uc_mcontext.gregs[REG_R13] = wr13;
        ucp->uc_mcontext.gregs[REG_R14] = wr14;
        ucp->uc_mcontext.gregs[REG_R15] = wr14;
        ucp->uc_mcontext.gregs[REG_CPSR] = wrcp;

	return (0);
}
