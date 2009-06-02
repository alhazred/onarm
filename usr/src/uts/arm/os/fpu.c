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
 * Copyright (c) 2006-2008 NEC Corporation
 */

#pragma ident	"@(#)arm/os/fpu.c"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/signal.h>
#include <sys/regset.h>
#include <sys/privregs.h>
#include <sys/fault.h>
#include <sys/systm.h>
#include <sys/pcb.h>
#include <sys/cpuvar.h>
#include <sys/thread.h>
#include <sys/disp.h>
#include <sys/fp.h>
#include <sys/siginfo.h>
#include <sys/debug.h>
#include <sys/fp_impl.h>
#include <asm/cpufunc.h>

#include <sys/prom_debug.h>

int fpu_exists = 1;		/* VFP always exists for MPCore */
int fp_kind = FP_VFP;

/*
 * void
 * fpu_probe(void)
 *	Check for existance of VFP and initialize it.
 *	This routine called by each cpu at startup time.
 */
void
fpu_probe(void)
{
	/* enable VFP full access */
	WRITE_CP15(0, c1, c0, 2, CPCTL_CP10_FULL|CPCTL_CP11_FULL);
	MEMORY_BARRIER();

	if (READ_CP15(0, c1, c0, 2) == 0) {
		/* VFP not present */
		fpu_exists = 0;
		/* Don't support full software emulation */
		fp_kind = FP_NO;
		return;
	}

	/* disable VFP */
	WRITE_FPEXC(READ_FPEXC() & ~(FPEXC_EN|FPEXC_EX));

	PRM_PRINTF("VFP(%d): fpsid = 0x%x, fpexc = 0x%x\n",
		CPU->cpu_id, READ_FPSID(), READ_FPEXC());
}

/*
 * Copy the floating point context of the forked thread.
 * Called in the following case:
 * 1) from forklwp -> lwp_forkregs -> fp_fork
 *	copy fpu context from parent thread to child thread,
 *	and install thread context ops to the new thread.
 */
void
fp_fork(klwp_t *lwp, klwp_t *clwp)
{
	struct fpu_ctx *fp;		/* parent fpu context */
	struct fpu_ctx *cfp;		/* new fpu context */

	ASSERT(fp_kind != FP_NO);

	fp = &lwp->lwp_pcb.pcb_fpu;
	cfp = &clwp->lwp_pcb.pcb_fpu;

	ASSERT(fp->fpu_flags != FPU_INVALID);	/* check by caller */

	kpreempt_disable();
	if (fp->fpu_flags & FPU_EN) {
		cfp->fpu_flags = FPU_EN;

		if (ttolwp(curthread) == lwp && !(fp->fpu_flags & FPU_VALID)) {
			/* save the current FPU state into new context */
			uint32_t fpexc = READ_FPEXC();
			if (fpexc & FPEXC_EN) {
				kfpu_t *kfpu = &cfp->fpu_regs;
				STORE_CP11(kfpu->fp_regs, c0, 16);
				kfpu->fp_fpscr = READ_FPSCR();
				kfpu->fp_fpexc = fpexc;
				if (fpexc & FPEXC_EX) {
					kfpu->fp_fpinst = READ_FPINST();
					kfpu->fp_fpinst2 = READ_FPINST2();
				}
				cfp->fpu_flags |= FPU_VALID;
			}
			ASSERT(cfp->fpu_flags & FPU_VALID);
			kpreempt_enable();
			return;
		}
		ASSERT(fp->fpu_flags & FPU_VALID);
	}

	if (fp->fpu_flags & FPU_VALID) {
		bcopy(&fp->fpu_regs, &cfp->fpu_regs, sizeof(kfpu_t));
		cfp->fpu_flags |= FPU_VALID;
	}
	kpreempt_enable();
}

/*
 * Store the floating point state.
 * Called in the following cases:
 * 1) from savecontext -> getfpregs -> fp_save_user
 */
boolean_t
fp_save_user(fpregset_t *fpreg)
{
	ASSERT(fp_kind != FP_NO);
	ASSERT(curthread->t_preempt >= 1);

	if (READ_FPEXC() & FPEXC_EN) {
		STORE_CP11(fpreg->fp_u.fp_vfp.fp_regs, c0, 16);
		fpreg->fp_u.fp_vfp.fp_fpscr = READ_FPSCR();
		return B_TRUE;
	}
	return B_FALSE;
}

/*
 * Seeds the initial state for the current thread.  The possibilities are:
 *      1. Another process has modified the FPU state before we have done any
 *         initialization: Load the FPU state from the LWP state.
 *      2. The FPU state has not been externally modified:  Load a clean state.
 */
static void
fp_seed(struct fpu_ctx *fp)
{
	kfpu_t *kfpu = &fp->fpu_regs;

	ASSERT(curthread->t_preempt >= 1);

	/*
	 * If FPU_VALID is set, it means someone has modified registers via
	 * /proc.  In this case, restore it to the current lwp's state.
	 * Otherwise (case 2), initialize pcb_fpu.
	 */
	if (!(fp->fpu_flags & FPU_VALID)) {
		bzero(kfpu, sizeof(kfpu_t));
		kfpu->fp_fpexc = FPEXC_EN;
		kfpu->fp_fpscr = FPSCR_INIT;
	}

	/*
	 * initialize fpu hardware.
	 */
	WRITE_FPEXC(kfpu->fp_fpexc);
	LOAD_CP11(kfpu->fp_regs, c0, 16);
	WRITE_FPSCR(kfpu->fp_fpscr);
	if (kfpu->fp_fpexc & FPEXC_EX) {
		WRITE_FPINST(kfpu->fp_fpinst);
		WRITE_FPINST2(kfpu->fp_fpinst2);
	}

	fp->fpu_flags = FPU_EN;
}

/*
 * This routine is called from trap() when User thread takes VFP exception.
 * The possiblities are:
 *	1. User thread has executed a FP instruction for the first time.
 *	   Save current FPU context if any. Initialize FPU, setup FPU
 *	   context for the thread and enable FP hw.
 *	2. Thread's pcb has a valid FPU state: Restore the FPU state and
 *	   enable FP hw.
 *	3. VFP support code.
 */
int
fpu_exception(struct regs *rp, k_siginfo_t *sip)
{
	struct fpu_ctx *fp = &ttolwp(curthread)->lwp_pcb.pcb_fpu;
	uint32_t	fpexc, fpscr;

	kpreempt_disable();

	if (!fpu_exists || fp_kind != FP_VFP) {	/* check for VFP exists */
		/*
		 * If we don't have VFP, kill the process OR panic the kernel.
		 */
		sip->si_signo = SIGILL;
		sip->si_code  = ILL_ILLOPC;
		sip->si_addr  = (caddr_t)rp->r_pc;
		kpreempt_enable();
		return FLTILL;
	}

	fpexc = READ_FPEXC();
	if (!(fpexc & FPEXC_EN)) {
		/*
		 * Enable VFP
		 */
		fp_seed(fp);
		kpreempt_enable();
		return 0;
	}

	/*
	 * case 3
	 * no support code.
	 */
	WRITE_FPEXC(fpexc & ~FPEXC_EX);
	fpscr = READ_FPSCR();

	PRM_PRINTF("VFP exception: "
		"curthread=0x%x, fpscr=0x%x, fpexc=0x%x, pc=0x%x\n",
		curthread, fpscr, fpexc, rp->r_pc);

	sip->si_signo = SIGFPE;
	sip->si_code  = FPE_FLTINV;
	sip->si_addr  = (caddr_t)rp->r_pc;

	kpreempt_enable();
	return FLTFPE;
}
