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
 * Copyright (c) 2008 NEC Corporation
 * All rights reserved.
 */

#ifndef	_SYS_FP_IMPL_H
#define	_SYS_FP_IMPL_H

#ident	"@(#)arm/sys/fp_impl.h"

/*
 * Kernel build tree private definitions for floating point context.
 */

#include <sys/regset.h>
#include <sys/pcb.h>
#include <sys/thread.h>
#include <sys/disp.h>
#include <sys/fp.h>
#include <sys/archsystm.h>
#include <sys/debug.h>
#include <asm/cpufunc.h>

extern void fp_fork(klwp_t *, klwp_t *);
extern boolean_t fp_save_user(fpregset_t *);

/*
 * Initialize the floating point context of the newly created lwp.
 * This marcro is called by lwp_load().
 */
#define	FP_INIT(lwp)		((lwp)->lwp_pcb.pcb_fpu.fpu_flags = FPU_INVALID)

/*
 * Copy the floating point context of the forked thread.
 * This marcro is called by lwp_forkregs().
 */
#define	FP_FORK(lwp, clwp)						\
	do {								\
		if ((lwp)->lwp_pcb.pcb_fpu.fpu_flags == FPU_INVALID) {	\
			FP_INIT(clwp);					\
		} else {						\
			fp_fork(lwp, clwp);				\
		}							\
	} while(0)

/*
 * Disable the floating point unit.
 * This macro is called by setfpregs().
 */
#define	FP_DISABLE(lwp)							\
	do {								\
		uint32_t fpexc;						\
		ASSERT(curthread->t_preempt >= 1);			\
		fpexc = READ_FPEXC();					\
		if (fpexc & FPEXC_EN) {					\
			WRITE_FPEXC(fpexc & ~(FPEXC_EN|FPEXC_EX));	\
		}							\
	} while(0)

/*
 * Free the floating point context of the exec'ed lwp.
 * This marcro is called by lwp_freeregs().
 * If the calling thread is reaper, nothing to do.
 */
#define	FP_FREE(lwp, isexec)						\
	do {								\
		if (fpu_exists && (isexec)) {				\
			ASSERT(curthread->t_lwp == (lwp));		\
			kpreempt_disable();				\
			FP_DISABLE(lwp);				\
			FP_INIT(lwp);					\
			kpreempt_enable();				\
		}							\
	} while(0)

/*
 * Save the floating point state into fpregset_t.
 * This macro is called by getfpregs().
 */
#define	FP_SAVE_USER(fpreg)	fp_save_user(fpreg)


#endif	/* !_SYS_FP_IMPL_H */
