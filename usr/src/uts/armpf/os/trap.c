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

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc. */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T   */
/*		All Rights Reserved   				*/
/*								*/
/*	Copyright (c) 1987, 1988 Microsoft Corporation  	*/
/*		All Rights Reserved   				*/
/*								*/

/*
 * Copyright (c) 2006-2008 NEC Corporation
 */

#ident	"@(#)armpf/os/trap.c"

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/signal.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/disp.h>
#include <sys/class.h>
#include <sys/core.h>
#include <sys/syscall.h>
#include <sys/cpuvar.h>
#include <sys/vm.h>
#include <sys/sysinfo.h>
#include <sys/fault.h>
#include <sys/stack.h>
#include <sys/regset.h>
#include <sys/fp.h>
#include <sys/trap.h>
#include <sys/kmem.h>
#include <sys/vtrace.h>
#include <sys/cmn_err.h>
#include <sys/prsystm.h>
#include <sys/mutex_impl.h>
#include <sys/machsystm.h>
#include <sys/archsystm.h>
#include <sys/sdt.h>
#include <sys/avintr.h>
#include <sys/kobj.h>
#include <sys/uctx_ops.h>

#include <vm/hat.h>
#include <vm/hat_arm.h>
#include <vm/seg_kmem.h>
#include <vm/as.h>
#include <vm/seg.h>

#include <sys/procfs.h>

#include <sys/reboot.h>
#include <sys/debug.h>
#include <sys/modctl.h>
#include <sys/aio_impl.h>
#include <sys/tnf.h>
#include <sys/tnf_probe.h>
#include <sys/cred.h>
#include <sys/mman.h>
#include <sys/copyops.h>
#include <sys/ftrace.h>
#include <sys/panic.h>
#ifdef	TRAPTRACE
#include <sys/traptrace.h>
#endif	/* TRAPTRACE */
#include <sys/ontrap.h>
#include <sys/cpc_impl.h>

#include <sys/privregs.h>
#include <sys/mpcore.h>
#include <asm/cpufunc.h>
#include <sys/prom_debug.h>
#include <sys/dtrace.h>
#include <dis/arm/dis.h>

#define	IS_WRITE_FAULT(fsr)	MPCORE_FSR_RW(fsr)
#define	IS_READ_FAULT(fsr)	(!MPCORE_FSR_RW(fsr))
#define	IS_EXEC_FAULT(rp, addr)	((caddr_t)rp->r_pc == addr)

static const char *trap_type[] = {
	"Reset",				/* trap id 0 	*/
	"Undefined instruction",		/* trap id 1	*/
	"Prefetch abort",			/* trap id 2	*/
	"Data abort",				/* trap id 3 	*/
};

#define	TRAP_TYPES	(sizeof (trap_type) / sizeof (trap_type[0]))

int tudebug = 0;

#if defined(TRAPDEBUG) || defined(lint)
int tdebug = 0;
int lodebug = 0;
#else
#define	tdebug	0
#define	lodebug	0
#endif /* defined(TRAPDEBUG) || defined(lint) */

static int die(uint_t, struct regs *, caddr_t, uint32_t);
static int kernel_pagefault(struct regs *, caddr_t, enum fault_type,
			    enum seg_rw);
static uint_t user_pagefault(struct regs *, k_siginfo_t *, caddr_t,
			     enum fault_type, enum seg_rw);
static uint_t data_abort_handler(trap_info_t *, k_siginfo_t *, int *);
static uint_t prefetch_abort_handler(trap_info_t *, k_siginfo_t *, int *);
static boolean_t	inst_is_upriv_access(uint32_t inst);
static void		upriv_fault_catch(struct regs *rp, caddr_t faddr,
					  uint32_t dfsr, int *mstate);

#if defined(TRAPTRACE)
static void dump_ttrace(void);
#endif	/* TRAPTRACE */
static void dumpregs(struct regs *);
static void showregs(uint_t, struct regs *, caddr_t);

extern faultcode_t pagefault(caddr_t, enum fault_type, enum seg_rw, int);

/*ARGSUSED*/
static int
die(uint_t type, struct regs *rp, caddr_t addr, uint32_t fsr)
{
	trap_info_t	ti;
	const char	*trap_name;

	if (type < TRAP_TYPES) {
		trap_name = trap_type[type];
	} else {
		trap_name = "unknown trap";
	}

#ifdef TRAPTRACE
	TRAPTRACE_FREEZE;
#endif

	ti.trap_regs = rp;
	ti.trap_type = type;
	ti.trap_addr = addr;
	ti.trap_fsr = fsr;

	curthread->t_panic_trap = &ti;

	panic("CPU%d BAD TRAP: type=%x (%s) fsr=%x rp=%p addr=%p",
	      HARD_PROCESSOR_ID(), type, trap_name, fsr, (void *)rp,
	      (void *)addr);

	return (0);
}

/*
 * Handle the kernel page fault.
 *
 * Return non-zero if the fault could not be resolved and t_lofault was
 * not set. Otherwise return zero.
 */
static int
kernel_pagefault(struct regs *rp, caddr_t fault_addr, enum fault_type type,
		 enum seg_rw rw)
{
	kthread_t	*cur_thread = curthread;
	proc_t		*p = ttoproc(cur_thread);
	uintptr_t	lofault;
	faultcode_t	fc;

	/*
	 * See if we can handle as pagefault. Save lofault
	 * across this. Here we assume that an address
	 * less than kernelbase is a user fault.
	 * We can do this as copy.s routines verify that the
	 * starting address is less than kernelbase before
	 * starting and because we know that we always have
	 * kernelbase mapped as invalid to serve as a "barrier".
	 */
	lofault = cur_thread->t_lofault;
	cur_thread->t_lofault = 0;

	/* Handle pagefault */
	if (fault_addr < (caddr_t)KERNELBASE) {
		/* Handle ref/mod emulation fault. */
		if (type == F_PROT &&
		    hat_softfault(fault_addr, rw, B_FALSE, NULL) !=
		    HAT_SF_UNCLAIMED) {
			/* Resolved. */
			fc = 0;
		}
		else {
			fc = pagefault(fault_addr, type, rw, 0);
			if (fc == FC_NOMAP &&
			    fault_addr < p->p_usrstack &&
			    grow(fault_addr)) {
				fc = 0;
			}
		}
	}
	else {
		fc = pagefault(fault_addr, type, rw, 1);
	}

	if (fc == 0 && type == F_INVAL) {
		/*
		 * Handle software ref/mod emulation fault here
		 * to reduce protection fault.
		 */
		(void)hat_softfault(fault_addr, rw, B_FALSE, NULL);
	}

	/*
	 * Restore lofault. If we resolved the fault, exit.
	 * If we didn't and lofault wasn't set, die.
	 */
	cur_thread->t_lofault = lofault;
	if (fc == 0) {
		return 0;
	}

	/*
	 * This fault is not resolvable.
	 * Check whether the fault was raised due to MPCore's dubious behaviour.
	 */
	if (hat_pteload_retry(rp, fault_addr, rw)) {
		return 0;
	}

	if (lofault == 0) {
		PRM_PRINTF("pagefault() failed and lofault wasn't set.\n");
		return -1;
	}

	/*
	 * Cannot resolve fault.  Return to lofault.
	 */
	if (lodebug) {
		showregs(T_DABT, rp, fault_addr);
		traceregs(rp);
	}
	if (FC_CODE(fc) == FC_OBJERR) {
		fc = FC_ERRNO(fc);
	} else {
		fc = EFAULT;
	}
	rp->r_r0 = fc;
	rp->r_pc = cur_thread->t_lofault;

	return 0;
}

/*
 * Handle the user page fault.
 */
static uint_t
user_pagefault(struct regs *rp, k_siginfo_t *siginfo, caddr_t fault_addr,
	       enum fault_type type, enum seg_rw rw)
{
	kthread_t	*cur_thread = curthread;
	proc_t		*p = ttoproc(cur_thread);
	klwp_t		*lwp = ttolwp(cur_thread);
	caddr_t		va;
	int		watchpage, watchcode, ta;
	size_t		sz;
	uint_t		fault = 0;
	faultcode_t	fc;

	ASSERT(!(cur_thread->t_flag & T_WATCHPT));
	watchpage = (pr_watch_active(p) && pr_is_watchpage(fault_addr, rw));

	va = fault_addr;
	if (!watchpage || (sz = instr_size(rp, &va, rw)) <= 0) {
		/* EMPTY */;
	} else if ((watchcode =
		    pr_is_watchpoint(&va, &ta, sz, NULL, rw)) != 0) {
		if (ta) {
			/*
			 * Trap after the instruction completes.
			 */
			do_watch_step(va, sz, rw, watchcode, rp->r_pc);
		} else {
			siginfo->si_signo = SIGTRAP;
			siginfo->si_code = watchcode;
			siginfo->si_addr = va;
			siginfo->si_trapafter = 0;
			siginfo->si_pc = (caddr_t)rp->r_pc;
			return FLTWATCH;
		}
	} else {
		if (pr_watch_emul(rp, va, rw)) {
			/* pr_watch_emul() never succeeds (for now) */;
		}
		do_watch_step(va, sz, rw, 0, 0);
	}

	/* Handle ref/mod emulation fault. */
	if (type == F_PROT &&
	    hat_softfault(fault_addr, rw, B_TRUE, NULL) != HAT_SF_UNCLAIMED) {
		/* Resolved. */
		fc = 0;
	}
	else {
		/* Handle pagefault */
		fc = pagefault(fault_addr, type, rw, 0);

		if (fc == 0 && type == F_INVAL) {
			/*
			 * Handle software ref/mod emulation fault here
			 * to reduce protection fault.
			 */
			(void)hat_softfault(fault_addr, rw, B_TRUE, NULL);
		}
	}

	/*
	 * If pagefault() succeeded, ok.
	 * Otherwise attempt to grow the stack.
	 */
	if (fc == 0 ||
	    (fc == FC_NOMAP &&
	     fault_addr < p->p_usrstack &&
	     grow(fault_addr))) {
		lwp->lwp_lastfault = FLTPAGE;
		lwp->lwp_lastfaddr = fault_addr;
		if (prismember(&p->p_fltmask, FLTPAGE)) {
			siginfo->si_addr = fault_addr;
			(void) stop_on_fault(FLTPAGE, siginfo);
		}
		return FLTPAGE;
	}
	else if (fc == FC_PROT && rw == S_EXEC) {
		report_stack_exec(p, fault_addr);
	}

	/*
	 * In the case where both pagefault and grow fail,
	 * set the code to the value provided by pagefault.
	 * We map all errors returned from pagefault() to 
	 * SIGSEGV.
	 */
	siginfo->si_addr = fault_addr;
	switch (FC_CODE(fc)) {
	case FC_HWERR:
	case FC_NOSUPPORT:
		siginfo->si_signo = SIGBUS;
		siginfo->si_code = BUS_ADRERR;
		fault = FLTACCESS;
		break;
	case FC_ALIGN:
		siginfo->si_signo = SIGBUS;
		siginfo->si_code = BUS_ADRALN;
		fault = FLTACCESS;
		break;
	case FC_OBJERR:
		if ((siginfo->si_errno = FC_ERRNO(fc)) != EINTR) {
			siginfo->si_signo = SIGBUS;
			siginfo->si_code = BUS_OBJERR;
			fault = FLTACCESS;
		}
		break;
	default:	/* FC_NOMAP or FC_PROT */
		siginfo->si_signo = SIGSEGV;
		siginfo->si_code = (fc == FC_NOMAP) ?
			SEGV_MAPERR : SEGV_ACCERR;
		fault = FLTBOUNDS;
		break;
	}

	return fault;
}

static uint_t
data_abort_handler(trap_info_t *ti, k_siginfo_t *siginfo, int *mstate)
{
	kthread_t	*cur_thread = curthread;
	caddr_t		fault_addr;
	uint32_t	dfsr, fault_status;
	uint_t		fault = 0;
	int		fatal_err = 0;
	enum seg_rw	rw;
	enum fault_type	type = F_INVAL;
	struct regs	*rp;
	boolean_t	usermode;

	rp = ti->trap_regs;
	fault_addr = ti->trap_addr;
	dfsr = ti->trap_fsr;
	fault_status = MPCORE_FSR_STATUS(dfsr);
	usermode = USERMODE(rp->r_cpsr);

	if (IS_WRITE_FAULT(dfsr)) {
		rw = S_WRITE;
	}
	else {
		rw = S_READ;
	}

	/*
	 * If we're under on_trap() protection (see <sys/ontrap.h>),
	 * set ot_trap and longjmp back to the on_trap() call site.
	 */
	if (!usermode) {
		if (CPU_ON_INTR(CPU) || getpil() > LOCK_LEVEL) {
			/*
			 * Data abort was occured on high level interrupt
			 * handler. This should be a false fault raised by
			 * MPCore, or a software bug.
			 */
			if (hat_pteload_retry(rp, fault_addr, rw)) {
				/* This is a false fault. */
				return 0;
			}

			panic("Data abort on high level interrupt: "
			      "vaddr=0x%p, pc=0x%lx",
			      fault_addr, rp->r_pc);
			/* NOTREACHED */
		}

		if (fault_status == MPCORE_FSR_PERM_S ||
		    fault_status == MPCORE_FSR_PERM_P ||
		    fault_status == MPCORE_FSR_ACCESS_S ||
		    fault_status == MPCORE_FSR_ACCESS_P) {
			int		sf;
			boolean_t	userpte;

			/* Handle ref/mod emulation fault. */
			*mstate = new_mstate(cur_thread, LMS_KFAULT);
			sf = hat_softfault(fault_addr, rw, B_FALSE, &userpte);
			if (sf != HAT_SF_UNCLAIMED) {
				if (sf == HAT_SF_HASPERM && !userpte) {
					upriv_fault_catch(rp, fault_addr, dfsr,
							  mstate);
				}

				/* Resolved. */
				(void)new_mstate(cur_thread, *mstate);
				return 0;
			}
			(void)new_mstate(cur_thread, *mstate);
		}

		if ((cur_thread->t_ontrap != NULL) &&
		    (cur_thread->t_ontrap->ot_prot & OT_DATA_ACCESS)) {
			cur_thread->t_ontrap->ot_trap |= OT_DATA_ACCESS;
			longjmp(&cur_thread->t_ontrap->ot_jmpbuf);
		}
	}

	switch (fault_status) {

	default:		/* Unknown trap */

		PRM_PRINTF("Unknown data abort.\n");
		fatal_err = 1;

		siginfo->si_signo = SIGILL;
		siginfo->si_code = ILL_ILLTRP;
		siginfo->si_addr = fault_addr;

		fault = FLTILL;
		break;

	case MPCORE_FSR_ALIGN:	/* Aignment fault */

		PRM_PRINTF("Alignment fault.\n");
		fatal_err = 1;

		siginfo->si_signo = SIGBUS;
		siginfo->si_code = BUS_ADRALN;
		siginfo->si_addr = fault_addr;

		fault = FLTACCESS;
		break;

	case MPCORE_FSR_ICMAIN:	/* I-cache maintenance operation fault */

		PRM_PRINTF("I-cache maintenance operation fault.\n");
		fatal_err = 1;

		/* Cannot happen in user mode. */
		ASSERT(!usermode);
		break;

	case MPCORE_FSR_DOMAIN_S:	/* Domain fault (Section) */
	case MPCORE_FSR_DOMAIN_P:	/* Domain fault (Page) */

		PRM_PRINTF("Domain fault.\n");
		fatal_err = 1;

		siginfo->si_signo = SIGSEGV;
		siginfo->si_code = SEGV_ACCERR;
		siginfo->si_addr = fault_addr;

		fault = FLTBOUNDS;
		break;

	case MPCORE_FSR_IMPRECISE:	/* Imprecise external abort */

		/* FAR is invalid for imprecise exceptions */
		fault_addr = 0;

		/*FALLTHROUGH*/

	case MPCORE_FSR_PRECISE:	/* Precise external abort */

	case MPCORE_FSR_BUSTRNL1:	/* External abort on translation(L1) */
	case MPCORE_FSR_BUSTRNL2:	/* External abort on translation(L2) */

		PRM_PRINTF("External abort.\n");
		fatal_err = 1;

		siginfo->si_signo = SIGBUS;
		siginfo->si_code = BUS_ADRERR;
		siginfo->si_addr = fault_addr;

		fault = FLTACCESS;
		break;

	case MPCORE_FSR_ACCESS_S:	/* Access bit fault (Section) */
	case MPCORE_FSR_ACCESS_P:	/* Access bit fault (Page) */

		/*
		 * Access bit fault is used to detect references or
		 * modifications to mapping.
		 */

		/*FALLTHROUGH*/

	case MPCORE_FSR_PERM_S:		/* Permission fault (Section) */
	case MPCORE_FSR_PERM_P:		/* Permission fault (Page) */

		type = F_PROT;
		/*FALLTHROUGH*/

	case MPCORE_FSR_TRANS_S:	/* Translation fault (Section) */
	case MPCORE_FSR_TRANS_P:	/* Translation fault (Page) */

		if (usermode) {
			*mstate = new_mstate(cur_thread, LMS_DFAULT);
			fault = user_pagefault(rp, siginfo, fault_addr,
					       type, rw);
		} else {
			*mstate = new_mstate(cur_thread, LMS_KFAULT);
			if (kernel_pagefault(rp, fault_addr, type, rw)) {
				(void) die(T_DABT, rp, fault_addr, dfsr);
			}
			(void) new_mstate(cur_thread, *mstate);
		}
		break;

	case MPCORE_FSR_DBGEVNT:	/* Debug event */

		/*
		 * Supported debug event is BKPT instruction only.
		 * Other debug event is not yet.
		 */
		fatal_err = 1;
		siginfo->si_signo = SIGILL;
		siginfo->si_code = ILL_ILLTRP;
		siginfo->si_addr = fault_addr;

		fault = FLTILL;
		break;

	}

	if (fatal_err) {
		if (!usermode) {
			die(T_DABT, rp, fault_addr, dfsr);
			/*NOTREACHED*/
		}
		*mstate = new_mstate(cur_thread, LMS_TRAP);

		if (tudebug) {
			showregs(T_DABT, rp, (caddr_t)0);
		}
	}

	return fault;
}

static uint_t
bkpt_handler(trap_info_t *ti, k_siginfo_t *siginfo, caddr_t fault_pc,
	boolean_t usermode)
{
	uint32_t	bp_instr;
	klwp_id_t	lwp = ttolwp(curthread);

	if (!usermode) {
		die(T_PABT, ti->trap_regs, fault_pc, ti->trap_fsr);
		/*NOTREACHED*/
	}

	/*
	 * read instruction to read immediate field.
	 */
	if (fuword32_nowatch(fault_pc, &bp_instr)) {
		goto bkpt_default;
	}

	if (bp_instr == (ARM_BKPT_INSTR|ARM_BKPT_CALC|ARM_BKPT_CALC_ZDIV)) {
		/* 
		 * send SIGFPE
		 * integer divide by zero
		 */
		siginfo->si_signo = SIGFPE;
		siginfo->si_addr = fault_pc;
		siginfo->si_code = FPE_INTDIV;
		return FLTIZDIV;
	}
	if (bp_instr == (ARM_BKPT_INSTR|ARM_BKPT_PROC|ARM_BKPT_PROC_STEP)) {
		/* proc(4) */
		/*
		 * prundostep() return
		 *    1 : expect BKPT
		 *    0 : not expect BKPT. (treat default BKPT)
		 */
		if (prundostep() == 1) {
			if (lwp->lwp_pcb.pcb_flags & NORMAL_STEP) {
				/* hitting Breakpoint. (single-step) */
				siginfo->si_signo = SIGTRAP;
				siginfo->si_code = TRAP_TRACE;
				siginfo->si_addr = fault_pc;
				if (lwp->lwp_pcb.pcb_flags & WATCH_STEP) {
					(void) undo_watch_step(NULL);
				}
				lwp->lwp_pcb.pcb_flags &= 
				    ~(NORMAL_STEP|WATCH_STEP);
				return FLTTRACE;
			} else {
				/* pcb->pcb_flags == WATCH_STEP.
				 * watchpoint flag is WA_TRAPAFTER.
				 * trap after instruction completes.
				 */
				lwp->lwp_pcb.pcb_flags &= 
				    ~(NORMAL_STEP|WATCH_STEP);
				return undo_watch_step(siginfo);
			}
		}
		goto bkpt_default;
	}

bkpt_default:
	/* default BKPT instruction */
	siginfo->si_signo = SIGTRAP;
	siginfo->si_addr = fault_pc;
	siginfo->si_code = TRAP_BRKPT;
	return FLTBPT;
}

static uint_t
prefetch_abort_handler(trap_info_t *ti, k_siginfo_t *siginfo, int *mstate)
{
	caddr_t		fault_pc;
	uint32_t	ifsr, dscr, fault_status;
	enum fault_type	type;
	struct regs	*rp;
	boolean_t	usermode;

	rp = ti->trap_regs;
	fault_pc = ti->trap_addr;
	ifsr = ti->trap_fsr;
	fault_status = MPCORE_FSR_STATUS(ifsr);
	usermode = USERMODE(rp->r_cpsr);

	switch (fault_status) {
	default:			/* Illegal trap */

		if (!usermode) {
			die(T_PABT, rp, fault_pc, ifsr);
			/*NOTREACHED*/
		}

		*mstate = new_mstate(curthread, LMS_TRAP);

		siginfo->si_signo = SIGILL;
		siginfo->si_code = ILL_ILLTRP;
		siginfo->si_addr = fault_pc;

		return FLTILL;

	case MPCORE_FSR_TRANS_S:	/* Translation fault (Section) */
	case MPCORE_FSR_TRANS_P:	/* Translation fault (Page) */

		type = F_INVAL;
		break;

	case MPCORE_FSR_ACCESS_S:	/* Access bit fault (Section) */
	case MPCORE_FSR_ACCESS_P:	/* Access bit fault (Page) */

		/*
		 * Access bit fault is used to detect references or
		 * modifications to mapping.
		 */

		/*FALLTHROUGH*/

	case MPCORE_FSR_PERM_S:		/* Permission fault (Section) */
	case MPCORE_FSR_PERM_P:		/* Permission fault (Page) */

		if (!usermode) {
			/* Handle ref/mod emulation fault. */
			*mstate = new_mstate(curthread, LMS_KFAULT);
			if (hat_softfault(fault_pc, S_EXEC, B_FALSE, NULL) !=
			    HAT_SF_UNCLAIMED) {
				/* Resolved. */
				(void)new_mstate(curthread, *mstate);
				return 0;
			}
			(void)new_mstate(curthread, *mstate);
		}

		type = F_PROT;
		break;

	case MPCORE_FSR_DBGEVNT:	/* Debug event */

		*mstate = new_mstate(curthread, LMS_TRAP);

		/*
		 * Read debug status from DSCR.
		 */
		dscr = (uint32_t)READ_CP14(0, c0, c1, 0);

		switch (MPCORE_DSCR_ENTRY(dscr)) {
		default:
			if (!usermode) {
				die(T_PABT, rp, fault_pc, ifsr);
				/*NOTREACHED*/
			}
			/*
			 * Supported debug event is BKPT instruction only.
			 * Other debug event is not yet.
			 */
			siginfo->si_signo = SIGILL;
			siginfo->si_code = ILL_ILLTRP;
			siginfo->si_addr = fault_pc;

			return FLTILL;

		case MPCORE_DSCR_BKPT:	/* BKPT instruction occurs */
			return bkpt_handler(ti, siginfo, fault_pc, usermode);
		}
	}

	/*
	 * This code assumes that kernel text is always mapped on valid pages.
	 * Prefetch abort pagefault cannot happen in kernel mode.
	 */
	if (!usermode) {
		die(T_PABT, rp, fault_pc, ifsr);
		/*NOTREACHED*/
	}

	if (fault_pc >= (caddr_t)_userlimit) {
		*mstate = new_mstate(curthread, LMS_TRAP);
		siginfo->si_signo = SIGSEGV;
		siginfo->si_code = SEGV_ACCERR;
		siginfo->si_addr = fault_pc;
		return FLTBOUNDS;
	}

	*mstate = new_mstate(curthread, LMS_TFAULT);

	return (user_pagefault(rp, siginfo, fault_pc, type, S_EXEC));
}

/*
 * Undefined instruction handler
 */
static uint_t
undefined_inst_handler(trap_info_t *ti, k_siginfo_t *sip, int *mstate)
{
	uint_t		fault = 0;
	struct regs	*rp;
	uint32_t	fault_inst = 0;
	uint_t		opcode;
	int		sicode;

	rp = ti->trap_regs;

	if (!USERMODE(rp->r_cpsr)) {
		/* VFP is not used in kernel */
		(void) die(T_UNDEF, rp, (caddr_t)rp->r_pc, 0);
		/*NOTREACHED*/
	}

	TNF_PROBE_1(thread_state, "thread", /* CSTYLED */,
		    tnf_microstate, state, LMS_TRAP);
	*mstate = new_mstate(curthread, LMS_TRAP);

	if (fuword32_nowatch((caddr_t)rp->r_pc, &fault_inst)) {
		/* Failed to fetch instruction */
		sip->si_code = ILL_ILLTRP;
		goto illegal;
	}

	/* Check fault instruction */
	opcode = (fault_inst >> 24) & 0xf;
	if (opcode >= 0xc && opcode <= 0xe) {
		/* coprocessor instruction */
		uint_t copnum = (fault_inst >> 8) & 0xf;
		if (copnum == 10 || copnum == 11) {
			/*
			 * VFP instruction
			 */
			return fpu_exception(rp, sip);
		} else {
			/* coprocessor instruction except for VFP. */
			sip->si_code = ILL_COPROC;
		}
	} else {
		/* undefined instruction except for coprocessor instruction.*/
		sip->si_code = ILL_ILLOPC;
	}

	/*
	 * Send SIGILL in the following cases.
	 *  - undefined instruction
	 *  - coprocessor instruction except for VFP
	 */
illegal:
	if (tudebug) {
		showregs(T_UNDEF, rp, (caddr_t)0);
	}
	sip->si_signo = SIGILL;
	sip->si_addr  = (caddr_t)rp->r_pc;
	sip->si_trapno = T_UNDEF;
	return FLTILL;
}

/*
 * Reset exception handler
 */
void
resettrap(void)
{
	/*
	 * Remarks: 
	 *	According to the MPCore Technical Reference Manual, 
	 *	"It is strongly recommended that if one MP11 CPU has
	 *	to be reset, the entire MPCore Multiprocessor is
	 *	reset. That is, all present MP11 CPUs, SCUs, and
	 *	Interrupt Controllers must be reset to prevent any
	 *	potential system deadlock caused by an MP11 CPU being
	 *	reset while it was performing a transaction on the
	 *	memory system."
	 *	Because of our specification, we should not do
	 *	entire reset from this system. So we only call die().
	 */ 
	(void) die(T_RESET, (struct regs *)0, (caddr_t)0, 0);
	/*NOTREACHED*/
}

/*
 * Common handler of below exceptions.
 *   - Undefined instruction
 *   - Prefetch abort
 *   - Data abort
 */
void
trap(struct regs *rp, trap_info_t *ti, uint32_t type)
{
	kthread_t	*cur_thread = curthread;
	proc_t		*p = ttoproc(cur_thread);
	klwp_t		*lwp = ttolwp(cur_thread);
	k_siginfo_t	siginfo;
	uint_t		fault = 0;
	int		old_mstate = -1;
	uint32_t	fault_status = (uint32_t)-1;

	ASSERT_STACK_ALIGNED();

	/*
	 * Save fault address and fault status before enabling interrupts.
	 */
	if (type == T_PABT) {
		/*
		 * Get fault address from lr_abt (saved in rp->r_pc).
		 * Note that FAR is NOT updated for prefetch abort.
		 */
		ti->trap_addr = (caddr_t)rp->r_pc;

		/*
		 * Get fault status from IFSR.
		 */
		ti->trap_fsr = (uint32_t)READ_CP15(0, c5, c0, 1);

		fault_status = MPCORE_FSR_STATUS(ti->trap_fsr);
	} else if (type == T_DABT) {
		/*
		 * Get fault address and fault status from FAR/DFSR
		 */
		ti->trap_addr = (caddr_t)READ_CP15(0, c6, c0, 0);
		ti->trap_fsr = (uint32_t)READ_CP15(0, c5, c0, 0);

		fault_status = MPCORE_FSR_STATUS(ti->trap_fsr);
	}
	if (ti != NULL) {
		ti->trap_regs = rp;
		ti->trap_type = type;
	}

	/*
	 * Enable interrupts if they were enabled when abort occur
	 */
	if (!(rp->r_cpsr & PSR_I_BIT)) {
		ENABLE_IRQ();
	}

	CPU_STATS_ADDQ(CPU, sys, trap, 1);

	ASSERT(cur_thread->t_schedflag & TS_DONT_SWAP);

	if (tdebug) {
		showregs(type, rp, (caddr_t)rp->r_pc);
	}

	if (USERMODE(rp->r_cpsr)) {
		/*
		 * Set lwp_state before trying to acquire any
		 * adaptive lock
		 */
		ASSERT(lwp != NULL);
		lwp->lwp_state = LWP_SYS;
		/*
		 * Set up the current cred to use during this trap. u_cred
		 * no longer exists.  t_cred is used instead.
		 * The current process credential applies to the thread for
		 * the entire trap.  If trapping from the kernel, this
		 * should already be set up.
		 */
		if (cur_thread->t_cred != p->p_cred) {
			cred_t *oldcred = cur_thread->t_cred;
			/*
			 * DTrace accesses t_cred in probe context.  t_cred
			 * must always be either NULL, or point to a valid,
			 * allocated cred structure.
			 */
			cur_thread->t_cred = crgetcred();
			crfree(oldcred);
		}
		ASSERT(lwptoregs(lwp) == rp);

		bzero(&siginfo, sizeof (siginfo));
	}

	FTRACE_2("trap(): type=0x%lx, regs=0x%lx", (ulong_t)type, (ulong_t)rp);

	switch (type) {
	default:
	case T_UNDEF:	/* undefined instruction */

		fault = undefined_inst_handler(ti, &siginfo, &old_mstate);
		break;

	case T_PABT:	/* prefetch abort */

		fault = prefetch_abort_handler(ti, &siginfo, &old_mstate);
		if (fault == FLTPAGE) {
			goto out;
		}

		break;

	case T_DABT:	/* data abort */

		fault = data_abort_handler(ti, &siginfo, &old_mstate);
		if (fault == FLTPAGE) {
			goto out;
		}

		break;

	case T_AST:	/* profiling or resched pseudo trap */

		TNF_PROBE_1(thread_state, "thread", /* CSTYLED */,
		    tnf_microstate, state, LMS_TRAP);
		old_mstate = new_mstate(cur_thread, LMS_TRAP);

		if (lwp->lwp_pcb.pcb_flags & CPC_OVERFLOW) {
			lwp->lwp_pcb.pcb_flags &= ~CPC_OVERFLOW;
			if (kcpc_overflow_ast()) {
				/*
				 * Signal performance counter overflow
				 */
				if (tudebug) {
					showregs(type, rp, (caddr_t)0);
				}
				bzero(&siginfo, sizeof (siginfo));
				siginfo.si_signo = SIGEMT;
				siginfo.si_code = EMT_CPCOVF;
				siginfo.si_addr = (caddr_t)rp->r_pc;
				fault = FLTCPCOVF;
			}
		}
		break;
	}

	/*
	 * No work remains in system trap.
	 */
	if (!USERMODE(rp->r_cpsr)) {
		return;
	}

	if (fault) {
		/* We took a fault so abort single step. */
		lwp->lwp_pcb.pcb_flags &= ~(NORMAL_STEP|WATCH_STEP);
		/*
		 * Remember the fault and fault adddress
		 * for real-time (SIGPROF) profiling.
		 */
		lwp->lwp_lastfault = fault;
		lwp->lwp_lastfaddr = siginfo.si_addr;

		DTRACE_PROC2(fault, int, fault, ksiginfo_t *, &siginfo);

		/*
		 * If a debugger has declared this fault to be an
		 * event of interest, stop the lwp.  Otherwise just
		 * deliver the associated signal.
		 */
		if (siginfo.si_signo != SIGKILL &&
		    prismember(&p->p_fltmask, fault) &&
		    stop_on_fault(fault, &siginfo) == 0)
			siginfo.si_signo = 0;
	}

	if (siginfo.si_signo)
		trapsig(&siginfo, (fault == FLTCPCOVF)? 0 : 1);

	if (lwp->lwp_oweupc)
		profil_tick(rp->r_pc);

	if (cur_thread->t_astflag | cur_thread->t_sig_check) {
		/*
		 * Turn off the AST flag before checking all the conditions that
		 * may have caused an AST.  This flag is on whenever a signal or
		 * unusual condition should be handled after the next trap or
		 * syscall.
		 */
		astoff(cur_thread);
		cur_thread->t_sig_check = 0;

		mutex_enter(&p->p_lock);
		if (curthread->t_proc_flag & TP_CHANGEBIND) {
			timer_lwpbind();
			curthread->t_proc_flag &= ~TP_CHANGEBIND;
		}
		mutex_exit(&p->p_lock);

		/*
		 * for kaio requests that are on the per-process poll queue,
		 * aiop->aio_pollq, they're AIO_POLL bit is set, the kernel
		 * should copyout their result_t to user memory. by copying
		 * out the result_t, the user can poll on memory waiting
		 * for the kaio request to complete.
		 */
		if (p->p_aio)
			aio_cleanup(0);
		/*
		 * If this LWP was asked to hold, call holdlwp(), which will
		 * stop.  holdlwps() sets this up and calls pokelwps() which
		 * sets the AST flag.
		 *
		 * Also check TP_EXITLWP, since this is used by fresh new LWPs
		 * through lwp_rtt().  That flag is set if the lwp_create(2)
		 * syscall failed after creating the LWP.
		 */
		if (ISHOLD(p))
			holdlwp();

		/*
		 * All code that sets signals and makes ISSIG evaluate true must
		 * set t_astflag afterwards.
		 */
		if (ISSIG_PENDING(cur_thread, lwp, p)) {
			if (issig(FORREAL))
				psig();
			cur_thread->t_sig_check = 1;
		}

		if (cur_thread->t_rprof != NULL) {
			realsigprof(0, 0);
			cur_thread->t_sig_check = 1;
		}
	}

out:	/* We can't get here from a system trap */
	ASSERT(USERMODE(rp->r_cpsr));

	if (ISHOLD(p))
		holdlwp();

	UCTX_OPS_TRAPRET(cur_thread);
	/*
	 * Set state to LWP_USER here so preempt won't give us a kernel
	 * priority if it occurs after this point.  Call CL_TRAPRET() to
	 * restore the user-level priority.
	 *
	 * It is important that no locks (other than spinlocks) be entered
	 * after this point before returning to user mode (unless lwp_state
	 * is set back to LWP_SYS).
	 */
	lwp->lwp_state = LWP_USER;

	if (cur_thread->t_trapret) {
		cur_thread->t_trapret = 0;
		thread_lock(cur_thread);
		CL_TRAPRET(cur_thread);
		thread_unlock(cur_thread);
	}
	if (CPU->cpu_runrun || curthread->t_schedflag & TS_ANYWAITQ)
		preempt();

	if (lwp->lwp_pcb.pcb_step == STEP_REQUESTED) {
		prdostep();
	} else if (lwp->lwp_pcb.pcb_step == STEP_WASACTIVE) {
		lwp->lwp_pcb.pcb_step = STEP_NONE;
	}

	(void) new_mstate(cur_thread, old_mstate);

	/* Kernel probe */
	TNF_PROBE_1(thread_state, "thread", /* CSTYLED */,
	    tnf_microstate, state, LMS_USER);

	return;
}

/*
 * Patch non-zero to disable preemption of threads in the kernel.
 */
int IGNORE_KERNEL_PREEMPTION = 0;	/* XXX - delete this someday */

struct kpreempt_cnts {		/* kernel preemption statistics */
	int	kpc_idle;	/* executing idle thread */
	int	kpc_intr;	/* executing interrupt thread */
	int	kpc_clock;	/* executing clock thread */
	int	kpc_blocked;	/* thread has blocked preemption (t_preempt) */
	int	kpc_notonproc;	/* thread is surrendering processor */
	int	kpc_inswtch;	/* thread has ratified scheduling decision */
	int	kpc_prilevel;	/* processor interrupt level is too high */
	int	kpc_apreempt;	/* asynchronous preemption */
	int	kpc_spreempt;	/* synchronous preemption */
} kpreempt_cnts;

/*
 * kernel preemption: forced rescheduling, preempt the running kernel thread.
 *	the argument is old PIL for an interrupt,
 *	or the distingished value KPREEMPT_SYNC.
 */
void
kpreempt(int asyncspl)
{
	kthread_t *cur_thread = curthread;

	if (IGNORE_KERNEL_PREEMPTION) {
		aston(CPU->cpu_dispthread);
		return;
	}

	/*
	 * Check that conditions are right for kernel preemption
	 */
	do {
		if (cur_thread->t_preempt) {
			/*
			 * either a privileged thread (idle, panic, interrupt)
			 *	or will check when t_preempt is lowered
			 */
			if (cur_thread->t_pri < 0)
				kpreempt_cnts.kpc_idle++;
			else if (cur_thread->t_flag & T_INTR_THREAD) {
				kpreempt_cnts.kpc_intr++;
				if (cur_thread->t_pil == CLOCK_LEVEL)
					kpreempt_cnts.kpc_clock++;
			} else
				kpreempt_cnts.kpc_blocked++;
			aston(CPU->cpu_dispthread);
			return;
		}
		if (cur_thread->t_state != TS_ONPROC ||
		    cur_thread->t_disp_queue != CPU->cpu_disp) {
			/* this thread will be calling swtch() shortly */
			kpreempt_cnts.kpc_notonproc++;
			if (CPU->cpu_thread != CPU->cpu_dispthread) {
				/* already in swtch(), force another */
				kpreempt_cnts.kpc_inswtch++;
				siron();
			}
			return;
		}
		if (getpil() >= DISP_LEVEL) {
			/*
			 * We can't preempt this thread if it is at
			 * a PIL >= DISP_LEVEL since it may be holding
			 * a spin lock (like sched_lock).
			 */
			siron();	/* check back later */
			kpreempt_cnts.kpc_prilevel++;
			return;
		}
		if (READ_CPSR() & PSR_I_BIT) {
			/*
			 * Can't preempt while running with ints disabled
			 */
			kpreempt_cnts.kpc_prilevel++;
			return;
		}
		if (asyncspl != KPREEMPT_SYNC)
			kpreempt_cnts.kpc_apreempt++;
		else
			kpreempt_cnts.kpc_spreempt++;

		cur_thread->t_preempt++;
		preempt();
		cur_thread->t_preempt--;
	} while (CPU->cpu_kprunrun);
}

/*
 * Print out debugging info.
 */
static void
showregs(uint_t type, struct regs *rp, caddr_t addr)
{
	int s;
	user_t	*u = PTOU(curproc);

	s = spl7();
	if (u->u_comm[0]) {
		printf("%s: ", u->u_comm);
	}
	if (type < TRAP_TYPES) {
		printf("%s\n", trap_type[type]);
	} else {
		switch (type) {
		case T_SYSCALL:
			printf("Syscall Trap:\n");
			break;
		case T_AST:
			printf("AST\n");
			break;
		default:
			printf("Bad Trap = %d\n", type);
			break;
		}
	}

	printf("addr=0x%lx\n", (uintptr_t)addr);
	printf("pid=%d, pc=0x%lx, sp=0x%lx, cpsr=0x%lx\n",
	       (ttoproc(curthread) && ttoproc(curthread)->p_pidp) ?
	       ttoproc(curthread)->p_pid : 0, rp->r_pc, 
	       USERMODE(rp->r_cpsr) ? rp->r_sp : rp->r_sp_svc, rp->r_cpsr);

	dumpregs(rp);
	splx(s);
}

static void
dumpregs(struct regs *rp)
{
	const char fmt[] = "\t%6s: %08lx %6s: %08lx %6s: %08lx\n";

	printf(fmt, "r0", rp->r_r0, "r1", rp->r_r1, "r2", rp->r_r2);
	printf(fmt, "r3", rp->r_r3, "r4", rp->r_r4, "r5", rp->r_r5);
	printf(fmt, "r6", rp->r_r6, "r7", rp->r_r7, "r8", rp->r_r8);
	printf(fmt, "r9", rp->r_r9, "r10", rp->r_r10, "r11", rp->r_r11);
	printf(fmt, "r12", rp->r_r12, " sp", rp->r_sp, "lr", rp->r_lr);
	printf(fmt, "sp_svc", rp->r_sp_svc, "lr_svc", rp->r_lr_svc,
	       "pc", rp->r_pc);
}

#if defined(TRAPTRACE)

int ttrace_nrec = 0;		/* number of records to dump out */
int ttrace_dump_nregs = 5;	/* dump out this many records with regs too */

/*
 * Dump out the last ttrace_nrec traptrace records on each CPU
 */
static void
dump_ttrace(void)
{
	trap_trace_ctl_t *ttc;
	trap_trace_rec_t *rec;
	uintptr_t current;
	int i, j, k;
	int n = NCPU;
	const char fmt2[] = "%4s %3x ";
	const char fmt3[] = "%8s ";

	if (ttrace_nrec == 0)
		return;

	printf(banner);

	for (i = 0; i < n; i++) {
		ttc = &trap_trace_ctl[i];
		if (ttc->ttc_first == NULL)
			continue;

		current = ttc->ttc_next - sizeof (trap_trace_rec_t);
		for (j = 0; j < ttrace_nrec; j++) {
			struct sysent	*sys;
			struct autovec	*vec;
			extern struct av_head autovect[];
			int type;
			ulong_t	off;
			char *sym, *stype;

			if (current < ttc->ttc_first)
				current =
				    ttc->ttc_limit - sizeof (trap_trace_rec_t);

			if (current == NULL)
				continue;

			rec = (trap_trace_rec_t *)current;

			if (rec->ttr_stamp == 0)
				break;

			printf(fmt1, i, (uintptr_t)rec, rec->ttr_stamp);

			switch (rec->ttr_marker) {
			case TT_SYSCALL:
			case TT_SYSENTER:
			case TT_SYSC:
			case TT_SYSC64:
				case TT_SYSC:
					stype = "sysc";	/* syscall */
					break;
				case TT_SYSCALL:
					stype = "lcal";	/* lcall */
					break;
				case TT_SYSENTER:
					stype = "syse";	/* sysenter */
					break;
				default:
					break;
				}
				printf(fmt2, "sysc", rec->ttr_sysnum);
				if (sys != NULL) {
					sym = kobj_getsymname(
					    (uintptr_t)sys->sy_callc,
					    &off);
					if (sym != NULL)
						printf("%s ", sym);
					else
						printf("%p ", sys->sy_callc);
				} else {
					printf("unknown ");
				}
				break;

			case TT_INTERRUPT:
				printf(fmt2, "intr", rec->ttr_vector);
				vec = (&autovect[rec->ttr_vector])->avh_link;
				if (vec != NULL) {
					sym = kobj_getsymname(
					    (uintptr_t)vec->av_vector, &off);
					if (sym != NULL)
						printf("%s ", sym);
					else
						printf("%p ", vec->av_vector);
				} else {
					printf("unknown ");
				}
				break;

			case TT_TRAP:
				type = rec->ttr_regs.r_trapno;
				printf(fmt2, "trap", type);
				printf("#%s ", type < TRAP_TYPES ?
				    trap_type_mnemonic[type] : "trap");
				break;

			default:
				break;
			}

			sym = kobj_getsymname(rec->ttr_regs.r_pc, &off);
			if (sym != NULL)
				printf("%s+%lx\n", sym, off);
			else
				printf("%lx\n", rec->ttr_regs.r_pc);

			if (ttrace_dump_nregs-- > 0) {
				int s;

				if (rec->ttr_marker == TT_INTERRUPT)
					printf(
					    "\t\tipl %x spl %x pri %x\n",
					    rec->ttr_ipl,
					    rec->ttr_spl,
					    rec->ttr_pri);

				dumpregs(&rec->ttr_regs);

				printf("\t%3s: %p\n\n", " ct",
				    (void *)rec->ttr_curthread);

				/*
				 * print out the pc stack that we recorded
				 * at trap time (if any)
				 */
				for (s = 0; s < rec->ttr_sdepth; s++) {
					uintptr_t fullpc;

					if (s >= TTR_STACK_DEPTH) {
						printf("ttr_sdepth corrupt\n");
						break;
					}

					fullpc = (uintptr_t)rec->ttr_stack[s];

					sym = kobj_getsymname(fullpc, &off);
					if (sym != NULL)
						printf("-> %s+0x%lx()\n",
						    sym, off);
					else
						printf("-> 0x%lx()\n", fullpc);
				}
				printf("\n");
			}
			current -= sizeof (trap_trace_rec_t);
		}
	}
}

#endif	/* TRAPTRACE */

void
panic_showtrap(trap_info_t *tip)
{
	showregs(tip->trap_type, tip->trap_regs, tip->trap_addr);

#if defined(TRAPTRACE)
	dump_ttrace();
#endif	/* TRAPTRACE */
}

void
panic_savetrap(panic_data_t *pdp, trap_info_t *tip)
{
	panic_saveregs(pdp, tip->trap_regs);
}

#pragma	weak	plat_fiq_handler

/*
 * void
 * fiq_handler(struct regs *rp, trap_info_t *tip)
 *	FIQ handler.
 *	Currently, it always causes panic.
 */
void
fiq_handler(struct regs *rp, trap_info_t *tip)
{
	extern volatile int	panic_start_cpu;
	extern boolean_t	plat_fiq_handler(struct regs *rp,
						 trap_info_t *tip);
	extern void	panic_idle_saveregs(void);

	/*
	 * Although fiq_handler() doesn't require trap_info, we construct it
	 * just for debugging purpose.
	 */
	tip->trap_regs = rp;
	tip->trap_type = T_FIQ;
	tip->trap_addr = (caddr_t)rp->r_pc;
	tip->trap_fsr = 0;

	/* Call platform-specific FIQ handler. */
	if (&plat_fiq_handler != NULL && plat_fiq_handler(rp, tip)) {
		/* FIQ trap has been resolved. */
		return;
	}

	if (panic_start_cpu != NCPU) {
		panic_idle_saveregs();
	}
	else {
		panic("Unknown FIQ: rp=0x%p", rp);
	}

	/* NOTREACHED */
}

#define	ARM_INST_USERPRIV_MASK	0x01200000
#define	ARM_INST_USERPRIV_VALUE	0x00200000

/*
 * static boolean_t
 * inst_is_upriv_access(uint32_t inst)
 *	Determine whether the given ARM instruction is data access instruction
 *	with user mode privilege.
 *
 * Calling/Exit State:
 *	B_TRUE is returned if the given instruction is data access instruction
 *	with user mode privilege. Currently, this function returns B_TRUE
 *	if the given instruction is one of the follows:
 *
 *	- ldrt
 *	- ldrbt
 *	- strt
 *	- strbt
 */
static boolean_t
inst_is_upriv_access(uint32_t inst)
{
	uint32_t	type = INSTR_TYPE(inst);

	if (type != 2 &&
	    !(type == 3 && (inst & 0x10) == 0)) {
		return B_FALSE;
	}

	return ((inst & ARM_INST_USERPRIV_MASK) == ARM_INST_USERPRIV_VALUE)
		? B_TRUE : B_FALSE;
}

/*
 * static void
 * upriv_fault_catch(struct regs *rp, caddr_t faddr, uint32_t dfsr,
 *		     int *mstate)
 *	Handle protection fault caused by kernel data access using
 *	user privilege instruction.
 *	If the kernel accesses kernel data using user privilege instruction,
 *	such as ldrt and strt, ARM processor raises protection fault.
 *	But fault handler can't handle the fault because valid PTE is
 *	installed for the faulted address.
 *
 *	We assume that:
 *	- the only caller is kernel mode data abort handler.
 *	- hat_softfault() has been previously called, and it has returned
 *	  HAT_SF_HASPERM.
 *	- The given address is mapped as kernel mode.
 */
static void
upriv_fault_catch(struct regs *rp, caddr_t faddr, uint32_t dfsr, int *mstate)
{
	uint32_t	inst;
	kthread_t	*t;
	uintptr_t	lofault;
	struct on_trap_data	*otdp;

	ASSERT(!USERMODE(rp->r_cpsr));

	/*
	 * Fetch faulted instruction.
	 * We can access text directly because the fault has been raised from
	 * kernel mode.
	 */
	ASSERT(rp->r_pc >= KERNELBASE);
	inst = *((uint32_t *)rp->r_pc);
	if (!inst_is_upriv_access(inst)) {
		/* The fault has been raised with normal instruction. */
		return;
	}

	/*
	 * This fault should fail because it has been raised due to 
	 * user privilege access to the kernel space. 
	 */
	t = curthread;
	otdp = t->t_ontrap;
	if (otdp != NULL && (otdp->ot_prot & OT_DATA_ACCESS)) {
		/* Jump to the on_trap() context. */
		(void)new_mstate(t, *mstate);
		otdp->ot_trap |= OT_DATA_ACCESS;
		longjmp(&otdp->ot_jmpbuf);
		/* NOTREACHED */
	}

	if ((lofault = t->t_lofault) == 0) {
		(void)die(T_DABT, rp, faddr, dfsr);
		/* NOTREACHED */
	}

	/* Return to lofault. */
	if (lodebug) {
		showregs(T_DABT, rp, faddr);
		traceregs(rp);
	}
	rp->r_r0 = EFAULT;
	rp->r_pc = lofault;

	return;
}
