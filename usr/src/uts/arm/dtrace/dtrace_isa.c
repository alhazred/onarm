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
 * Copyright (c) 2007-2008 NEC Corporation
 */

#ident	"@(#)dtrace_isa.c"

#include <sys/dtrace_impl.h>
#include <sys/stack.h>
#include <sys/frame.h>
#include <sys/cmn_err.h>
#include <sys/privregs.h>
#include <sys/sysmacros.h>
#include <sys/regset.h>
#include <sys/modctl.h>
#include <sys/atomic.h>
#include <asm/cpufunc.h>

#define	DTRACE_INVALID_CODE	0xffffffff

#define	DTRACE_IS_BACKINST_CACHE_AREA(pc)	\
	(dtrace_text_start <= (char *)(pc) && (char *)(pc) < dtrace_text_end)

#define	DTRACE_BACKINST_CACHE_ENTRY	10

static char *dtrace_text_start = NULL;
static char *dtrace_text_end = NULL;

static struct {
	uint32_t	*tpc;
	uint32_t	*pc;
	int		spdiff;
	int		lroff;
} dtrace_backinst_cache[DTRACE_BACKINST_CACHE_ENTRY];

static int dtrace_backinst_cache_idx	= -1;

static void
dtrace_gettextinfo(void)
{
	struct modctl *mod;

	mod = &modules;
	do {
		if (strcmp(mod->mod_modname, "dtrace") == 0) {
			dtrace_text_start = mod->mod_text;
			MEMORY_BARRIER();
			dtrace_text_end = mod->mod_text + mod->mod_text_size;
			MEMORY_BARRIER();
			break;
		}
	} while ((mod = mod->mod_next) != &modules);
}

static uint32_t *
dtrace_backinst_cachesearch(uint32_t *pc, int *spdiffp, int *lroffp)
{
	int i, endi;

	endi = dtrace_backinst_cache_idx;
	if (endi > (DTRACE_BACKINST_CACHE_ENTRY - 1))
		endi = DTRACE_BACKINST_CACHE_ENTRY - 1;

	for (i = 0; i <= endi; i ++) {
		if (dtrace_backinst_cache[i].pc == pc) {
			*spdiffp = dtrace_backinst_cache[i].spdiff;
			*lroffp = dtrace_backinst_cache[i].lroff;
			return dtrace_backinst_cache[i].tpc;
		}
	}
	return NULL;
}

static void
dtrace_backinst_cacheset(uint32_t *tpc, uint32_t *pc, int spdiff, int lroff)
{
	uint_t i;
	int dspdiff, dlroff;

	if (dtrace_backinst_cachesearch(pc, &dspdiff, &dlroff) != NULL)
		return;

	i = atomic_inc_uint_nv((uint_t *)&dtrace_backinst_cache_idx);

	if (i >= DTRACE_BACKINST_CACHE_ENTRY)
		return;

	dtrace_backinst_cache[i].tpc = tpc;
	dtrace_backinst_cache[i].spdiff = spdiff;
	dtrace_backinst_cache[i].lroff = lroff;
	MEMORY_BARRIER();
	dtrace_backinst_cache[i].pc = pc;
	MEMORY_BARRIER();
}

uint32_t
dtrace_readinst(uint32_t *pc)
{
	uint32_t inst;
	DTRACE_CPUFLAG_SET(CPU_DTRACE_NOFAULT);
	inst = *pc;
	if (DTRACE_CPUFLAG_ISSET(CPU_DTRACE_BADADDR)) {
		inst = DTRACE_INVALID_CODE;
	}
	DTRACE_CPUFLAG_CLEAR(CPU_DTRACE_NOFAULT | CPU_DTRACE_BADADDR);
	return (inst);
}

static uint32_t *
dtrace_backinst(uint32_t *pc, int *spdiffp, int *lroffp)
{
	uint32_t *tpc;
	uint32_t inst;

	if (DTRACE_IS_BACKINST_CACHE_AREA(pc)) {
		tpc = dtrace_backinst_cachesearch(pc, spdiffp, lroffp);
		if (tpc != NULL)
			return tpc;
	}

	tpc = pc;
	inst = dtrace_readinst(tpc);
	while (inst != DTRACE_INVALID_CODE) {
		if (dtrace_dis_stacktrace(inst, spdiffp, lroffp))
			break;
		tpc--;
		inst = dtrace_readinst(tpc);
	}

	if (inst == DTRACE_INVALID_CODE)
		return NULL;

	if (DTRACE_IS_BACKINST_CACHE_AREA(pc)
	    && (dtrace_backinst_cache_idx < (DTRACE_BACKINST_CACHE_ENTRY - 1)))
		dtrace_backinst_cacheset(tpc, pc, *spdiffp, *lroffp);

	return tpc;
}

extern void lockstat_wrapper();
extern void lockstat_wrapper_arg();

void
dtrace_getpcstack(pc_t *pcstack, int pcstack_limit, int aframes,
    uint32_t *argpc)
{
	uint32_t *pc;
	caddr_t sp, minsp, stacktop;
	int depth = 0;
	int on_intr;

	pc = (uint32_t *)dtrace_getpc();
	sp = (caddr_t)dtrace_getsp();

	if (dtrace_text_end == NULL)
		dtrace_gettextinfo();

	if ((on_intr = CPU_ON_INTR(CPU)) != 0)
		stacktop = CPU->cpu_intr_stack + SA(MINFRAME);
	else
		stacktop = curthread->t_stk;

	aframes++;

	if (argpc != NULL && depth < pcstack_limit) {
		pcstack[depth++] = (pc_t)argpc;
		aframes++;
	}

	pc -= 2;
	minsp = sp;

	while (depth < pcstack_limit) {
		uint32_t *tpc;
		int lroff, spdiff;
		caddr_t fp;

		lroff = -1;
		spdiff = 0;
		tpc = dtrace_backinst(pc, &spdiff, &lroff);

		/* Special adjustment */
		if (tpc == (uint32_t *)lockstat_wrapper) {
			/* spdiff += 4; */	/* use "bic sp, sp, #7" */
			/* lroff += 4; */	/* use "bic sp, sp, #7" */
			aframes ++;		/* function depth +1 */
		} else if (tpc == (uint32_t *)lockstat_wrapper_arg) {
			spdiff += 4;		/* use "bic sp, sp, #7" */
			lroff += 4;		/* use "bic sp, sp, #7" */
			aframes ++;		/* function depth +1 */
		}

		if (tpc != NULL && lroff != -1
		    && (fp = sp + lroff) < stacktop) {
			DTRACE_CPUFLAG_SET(CPU_DTRACE_NOFAULT);
			pc = (uint32_t *)*(uint32_t *)fp;
			DTRACE_CPUFLAG_CLEAR(CPU_DTRACE_NOFAULT
			    | CPU_DTRACE_BADADDR);
			if (aframes > 0) {
				aframes--;
			} else {
				pcstack[depth++] = (pc_t)pc;
			}
			sp = sp + spdiff;
		} else {
			sp = stacktop;	/* end */
		}
	
		if (sp <= minsp || sp >= stacktop) {
			if (on_intr) {
				/*
				 * Hop from interrupt stack to thread stack.
				 */
				/* not support */
				on_intr = 0;
				/* continue; */
			}
			while (depth < pcstack_limit)
				pcstack[depth++] = NULL;
			return;
		}

		pc -= 2;
		minsp = sp;
	}
}

static int
dtrace_getustack_common(uint64_t *pcstack, int pcstack_limit, uintptr_t pc,
    uintptr_t sp)
{
	int ret = 0;

	ASSERT(pcstack == NULL || pcstack_limit > 0);

	/* This is not supported, because fp register does not exist 
	   on ARM architecture. */

	return (ret);
}

void
dtrace_getupcstack(uint64_t *pcstack, int pcstack_limit)
{
	klwp_t *lwp = ttolwp(curthread);
	proc_t *p = curproc;
	struct regs *rp;
	uintptr_t pc, sp;
	int n;

	ASSERT(DTRACE_CPUFLAG_ISSET(CPU_DTRACE_NOFAULT));

	if (pcstack_limit <= 0)
		return;

	/*
	 * If there's no user context we still need to zero the stack.
	 */
	if (lwp == NULL || p == NULL || (rp = lwp->lwp_regs) == NULL)
		goto zero;

	*pcstack++ = (uint64_t)p->p_pid;
	pcstack_limit--;

	if (pcstack_limit <= 0)
		return;

	pc = rp->r_pc;

	if (DTRACE_CPUFLAG_ISSET(CPU_DTRACE_ENTRY)) {
		*pcstack++ = (uint64_t)pc;
		pcstack_limit--;
		if (pcstack_limit <= 0)
			return;
	}

	sp = rp->r_sp;

	n = dtrace_getustack_common(pcstack, pcstack_limit, pc, sp);
	ASSERT(n >= 0);
	ASSERT(n <= pcstack_limit);

	pcstack += n;
	pcstack_limit -= n;

zero:
	while (pcstack_limit-- > 0)
		*pcstack++ = NULL;
}

int
dtrace_getustackdepth(void)
{
	klwp_t *lwp = ttolwp(curthread);
	proc_t *p = curproc;
	struct regs *rp;
	uintptr_t pc, sp;
	int n = 0;

	if (lwp == NULL || p == NULL || (rp = lwp->lwp_regs) == NULL)
		return (0);

	if (DTRACE_CPUFLAG_ISSET(CPU_DTRACE_FAULT))
		return (-1);

	pc = rp->r_pc;
	sp = rp->r_sp;

	if (DTRACE_CPUFLAG_ISSET(CPU_DTRACE_ENTRY)) {
		n++;
	}

	n += dtrace_getustack_common(NULL, 0, pc, sp);

	return (n);
}

void
dtrace_getufpstack(uint64_t *pcstack, uint64_t *fpstack, int pcstack_limit)
{
	klwp_t *lwp = ttolwp(curthread);
	proc_t *p = ttoproc(curthread);
	struct regs *rp;
	uintptr_t pc, sp;

	if (pcstack_limit <= 0)
		return;

	/*
	 * If there's no user context we still need to zero the stack.
	 */
	if (lwp == NULL || p == NULL || (rp = lwp->lwp_regs) == NULL)
		goto zero;

	*pcstack++ = (uint64_t)p->p_pid;
	pcstack_limit--;

	if (pcstack_limit <= 0)
		return;

	pc = rp->r_pc;
	sp = rp->r_sp;

	if (DTRACE_CPUFLAG_ISSET(CPU_DTRACE_ENTRY)) {
		*pcstack++ = (uint64_t)pc;
		*fpstack++ = 0;
		pcstack_limit--;
		if (pcstack_limit <= 0)
			return;
	}

	/* This is not supported, because fp register does not exist 
	   on ARM architecture. */

zero:
	while (pcstack_limit-- > 0)
		*pcstack++ = NULL;
}

/*ARGSUSED*/
uint64_t
dtrace_getarg(int arg, int aframes)
{
	/* ARM does not make stack frame. So does not get argument. */
	return (0);
}

int
dtrace_getstackdepth(int aframes)
{
	/* This is not supported, because fp register does not exist 
	   on ARM architecture. */
	return (0);
}

ulong_t
dtrace_getreg(struct regs *rp, uint_t reg)
{
	ulong_t value;

	switch (reg) {
	case REG_R0:
		return (rp->r_r0);
	case REG_R1:
		return (rp->r_r1);
	case REG_R2:
		return (rp->r_r2);
	case REG_R3:
		return (rp->r_r3);
	case REG_R4:
		return (rp->r_r4);
	case REG_R5:
		return (rp->r_r5);
	case REG_R6:
		return (rp->r_r6);
	case REG_R7:
		return (rp->r_r7);
	case REG_R8:
		return (rp->r_r8);
	case REG_R9:
		return (rp->r_r9);
	case REG_R10:
		return (rp->r_r10);
	case REG_R11:
		return (rp->r_r11);
	case REG_R12:
		return (rp->r_r12);
	case REG_SP:
		return (rp->r_sp);
	case REG_LR:
		return (rp->r_lr);
	case REG_PC:
		return (rp->r_pc);
	case REG_CPSR:
		return (rp->r_cpsr);
	}

	DTRACE_CPUFLAG_SET(CPU_DTRACE_ILLOP);
	return (0);
}

/*ARGSUSED*/
void
dtrace_probe_error(dtrace_state_t *arg0, dtrace_epid_t arg1, int arg2, int arg3,
    int arg4, uintptr_t arg5)
{
	extern dtrace_id_t dtrace_probeid_error;
	dtrace_probe(dtrace_probeid_error, (uintptr_t)arg0, (uintptr_t)arg1,
	    (uintptr_t)arg2, (uintptr_t)arg3, (uintptr_t)arg4);
}
