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
 * Copyright (c) 2006-2008 NEC Corporation
 */

#pragma ident	"@(#)syscall.c"

#include <sys/param.h>
#include <sys/vmparam.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/stack.h>
#include <sys/cred.h>
#include <sys/cmn_err.h>
#include <sys/user.h>
#include <sys/privregs.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/modctl.h>
#include <sys/var.h>
#include <sys/inline.h>
#include <sys/syscall.h>
#include <sys/ucontext.h>
#include <sys/cpuvar.h>
#include <sys/siginfo.h>
#include <sys/trap.h>
#include <sys/vtrace.h>
#include <sys/sysinfo.h>
#include <sys/procfs.h>
#include <c2/audit.h>
#include <sys/modctl.h>
#include <sys/aio_impl.h>
#include <sys/tnf.h>
#include <sys/tnf_probe.h>
#include <sys/copyops.h>
#include <sys/priv.h>
#include <sys/msacct.h>
#include <sys/uctx_ops.h>
#include <sys/lwp_private.h>

#include <asm/cpufunc.h>

#ifdef SYSCALLTRACE
int syscalltrace = 0;
static kmutex_t systrace_lock;		/* syscall tracing lock */
#else
#define	syscalltrace 0
#endif /* SYSCALLTRACE */

typedef	int64_t (*llfcn_t)();	/* function returning long long */

int pre_syscall(void);
void post_syscall(long rval1, long rval2);
static krwlock_t *lock_syscall(struct sysent *, uint_t);

#ifdef _SYSCALL32_IMPL
#define	LWP_GETSYSENT(lwp)	\
	(lwp_getdatamodel(lwp) == DATAMODEL_NATIVE ? sysent : sysent32)
#else
#define	LWP_GETSYSENT(lwp)	(sysent)
#endif

/*
 * Arrange for the real time profiling signal to be dispatched.
 */
void
realsigprof(int sysnum, int error)
{
	proc_t *p;
	klwp_t *lwp;

	if (curthread->t_rprof->rp_anystate == 0)
		return;
	p = ttoproc(curthread);
	lwp = ttolwp(curthread);
	mutex_enter(&p->p_lock);
	if (sigismember(&p->p_ignore, SIGPROF) ||
	    signal_is_blocked(curthread, SIGPROF)) {
		mutex_exit(&p->p_lock);
		return;
	}
	lwp->lwp_siginfo.si_signo = SIGPROF;
	lwp->lwp_siginfo.si_code = PROF_SIG;
	lwp->lwp_siginfo.si_errno = error;
	hrt2ts(gethrtime(), &lwp->lwp_siginfo.si_tstamp);
	lwp->lwp_siginfo.si_syscall = sysnum;
	lwp->lwp_siginfo.si_nsysarg = (sysnum > 0 && sysnum < NSYSCALL) ?
		LWP_GETSYSENT(lwp)[sysnum].sy_narg : 0;
	lwp->lwp_siginfo.si_fault = lwp->lwp_lastfault;
	lwp->lwp_siginfo.si_faddr = lwp->lwp_lastfaddr;
	lwp->lwp_lastfault = 0;
	lwp->lwp_lastfaddr = NULL;
	sigtoproc(p, curthread, SIGPROF);
	mutex_exit(&p->p_lock);
	ASSERT(lwp->lwp_cursig == 0);
	if (issig(FORREAL)) {
		psig();
	}
	mutex_enter(&p->p_lock);
	lwp->lwp_siginfo.si_signo = 0;
	bzero(curthread->t_rprof, sizeof (*curthread->t_rprof));
	mutex_exit(&p->p_lock);
}

/*
 * If watchpoints are active, don't make copying in of
 * system call arguments take a read watchpoint trap.
 */
static int
copyin_args(struct regs *rp, long *ap, uint_t nargs)
{
	greg_t *sp = (greg_t *)rp->r_sp;

	ASSERT(nargs <= MAXSYSARGS);

	return (copyin_nowatch(sp, ap, nargs * sizeof (*sp)));
}

#if defined(_SYSCALL32_IMPL)
static int
copyin_args32(struct regs *rp, long *ap, uint_t nargs)
{
	greg32_t *sp = (greg32_t *)rp->r_sp;
	uint32_t a32[MAXSYSARGS];
	int rc;

	ASSERT(nargs <= MAXSYSARGS);

	if ((rc = copyin_nowatch(sp, a32, nargs * sizeof (*sp))) == 0) {
		uint32_t *a32p = &a32[0];

		while (nargs--)
			*ap++ = (ulong_t)*a32p++;
	}
	return (rc);
}
#endif

/*
 * Perform pre-system-call processing, including stopping for tracing,
 * auditing, etc.
 *
 * This routine is called only if the t_pre_sys flag is set. Any condition
 * requiring pre-syscall handling must set the t_pre_sys flag. If the
 * condition is persistent, this routine will repost t_pre_sys.
 */
int
pre_syscall()
{
	kthread_t *t = curthread;
	unsigned code = t->t_sysnum;
	klwp_t *lwp = ttolwp(t);
	proc_t *p = ttoproc(t);
	int	repost;

	t->t_pre_sys = repost = 0;	/* clear pre-syscall processing flag */

	ASSERT(t->t_schedflag & TS_DONT_SWAP);

	/*
	 * In this time, lwp_ap should point the syscall arguments in
	 * the r0..r3 registers. If the args need to be copied so that
	 * those registers can be changed without losing
	 * the ability to get the args for /proc, they can be saved by
	 * save_syscall_args(), and lwp_ap will be restored by post_syscall().
	 */
	ASSERT(lwp->lwp_ap == (long *)&lwptoregs(lwp)->r_r0);

	/*
	 * Make sure the thread is holding the latest credentials for the
	 * process.  The credentials in the process right now apply to this
	 * thread for the entire system call.
	 */
	if (t->t_cred != p->p_cred) {
		cred_t *oldcred = t->t_cred;
		/*
		 * DTrace accesses t_cred in probe context.  t_cred must
		 * always be either NULL, or point to a valid, allocated cred
		 * structure.
		 */
		t->t_cred = crgetcred();
		crfree(oldcred);
	}

	/*
	 * Undo special arrangements to single-step the lwp
	 * so that a debugger will see valid register contents.
	 */
	if (lwp->lwp_pcb.pcb_step != STEP_NONE) {
		(void) prundostep();
		repost = 1;
	}

	/*
	 * From the proc(4) manual page:
	 * When entry to a system call is being traced, the traced process
	 * stops after having begun the call to the system but before the
	 * system call arguments have been fetched from the process.
	 */
	if (PTOU(p)->u_systrap) {
		if (prismember(&PTOU(p)->u_entrymask, code)) {
			mutex_enter(&p->p_lock);
			/*
			 * Recheck stop condition, now that lock is held.
			 */
			if (PTOU(p)->u_systrap &&
			    prismember(&PTOU(p)->u_entrymask, code)) {
				stop(PR_SYSENTRY, code);
				/*
				 * Must refetch args since they were
				 * possibly modified by /proc.  Indicate
				 * that the valid copy is in the
				 * registers and user stack.
				 */
				lwp->lwp_argsaved = 0;
				lwp->lwp_ap = (long *)&lwptoregs(lwp)->r_r0;
			}
			mutex_exit(&p->p_lock);
		}
		repost = 1;
	}

	if (lwp->lwp_sysabort) {
		/*
		 * lwp_sysabort may have been set via /proc while the process
		 * was stopped on PR_SYSENTRY.  If so, abort the system call.
		 * Override any error from the copyin() of the arguments.
		 */
		lwp->lwp_sysabort = 0;
		(void) set_errno(EINTR);	/* forces post_sys */
		t->t_pre_sys = 1;	/* repost anyway */
		return (1);		/* don't do system call, return EINTR */
	}

	if (audit_active) {	/* begin auditing for this syscall */
		int error;
		if (error = audit_start(T_SYSCALL, code, 0, lwp)) {
			t->t_pre_sys = 1;	/* repost anyway */
			(void) set_errno(error);
			return (1);
		}
		repost = 1;
	}

#ifndef NPROBE
	/* Kernel probe */
	if (tnf_tracing_active) {
		TNF_PROBE_1(syscall_start, "syscall thread", /* CSTYLED */,
			tnf_sysnum,	sysnum,		t->t_sysnum);
		t->t_post_sys = 1;	/* make sure post_syscall runs */
		repost = 1;
	}
#endif /* NPROBE */

#ifdef SYSCALLTRACE
	if (syscalltrace) {
		int i;
		long *ap;
		char *cp;
		char *sysname;
		struct sysent *callp;

		if (code >= NSYSCALL)
			callp = &nosys_ent;	/* nosys has no args */
		else
			callp = LWP_GETSYSENT(lwp) + code;
		(void) save_syscall_args();
		mutex_enter(&systrace_lock);
		printf("%d: ", p->p_pid);
		if (code >= NSYSCALL)
			printf("0x%x", code);
		else {
			sysname = mod_getsysname(code);
			printf("%s[0x%x/0x%p]", sysname == NULL ? "NULL" :
			    sysname, code, callp->sy_callc);
		}
		cp = "(";
		for (i = 0, ap = lwp->lwp_ap; i < callp->sy_narg; i++, ap++) {
			printf("%s%lx", cp, *ap);
			cp = ", ";
		}
		if (i)
			printf(")");
		printf(" %s id=0x%p\n", PTOU(p)->u_comm, curthread);
		mutex_exit(&systrace_lock);
	}
#endif /* SYSCALLTRACE */

	/*
	 * If there was a continuing reason for pre-syscall processing,
	 * set the t_pre_sys flag for the next system call.
	 */
	if (repost)
		t->t_pre_sys = 1;
	lwp->lwp_error = 0;	/* for old drivers */
	lwp->lwp_badpriv = PRIV_NONE;	/* for privilege tracing */
	return (0);
}

/*
 * Post-syscall processing.  Perform abnormal system call completion
 * actions such as /proc tracing, profiling, signals, preemption, etc.
 *
 * This routine is called only if t_post_sys, t_sig_check, or t_astflag is set.
 * Any condition requiring pre-syscall handling must set one of these.
 * If the condition is persistent, this routine will repost t_post_sys.
 */
void
post_syscall(long rval1, long rval2)
{
	kthread_t *t = curthread;
	klwp_t *lwp = ttolwp(t);
	proc_t *p = ttoproc(t);
	struct regs *rp = lwptoregs(lwp);
	uint_t	error;
	int	code = t->t_sysnum;
	int	repost = 0;
	int	proc_stop = 0;		/* non-zero if stopping */
	int	sigprof = 0;		/* non-zero if sending SIGPROF */

	t->t_post_sys = 0;

	error = lwp->lwp_errno;

	/*
	 * Code can be zero if this is a new LWP returning after a forkall(),
	 * other than the one which matches the one in the parent which called
	 * forkall().  In these LWPs, skip most of post-syscall activity.
	 */
	if (code == 0)
		goto sig_check;

	if (audit_active) {	/* put out audit record for this syscall */
		rval_t	rval;	/* fix audit_finish() someday */

		/* XX64 -- truncation of 64-bit return values? */
		rval.r_val1 = (int)rval1;
		rval.r_val2 = (int)rval2;
		audit_finish(T_SYSCALL, code, error, &rval);
		repost = 1;
	}

	if (curthread->t_pdmsg != NULL) {
		char *m = curthread->t_pdmsg;

		uprintf("%s", m);
		kmem_free(m, strlen(m) + 1);
		curthread->t_pdmsg = NULL;
	}

	/*
	 * If we're going to stop for /proc tracing, set the flag and
	 * save the arguments so that the return values don't smash them.
	 */
	if (PTOU(p)->u_systrap) {
		if (prismember(&PTOU(p)->u_exitmask, code)) {
			(void) save_syscall_args();
			proc_stop = 1;
		}
		repost = 1;
	}

	/*
	 * Similarly check to see if SIGPROF might be sent.
	 */
	if (curthread->t_rprof != NULL &&
	    curthread->t_rprof->rp_anystate != 0) {
		(void) save_syscall_args();
		sigprof = 1;
	}

	if (lwp->lwp_eosys == NORMALRETURN) {
		if (error == 0) {
#ifdef SYSCALLTRACE
			if (syscalltrace) {
				mutex_enter(&systrace_lock);
				printf(
				    "%d: r_val1=0x%lx, r_val2=0x%lx, id 0x%p\n",
				    p->p_pid, rval1, rval2, curthread);
				mutex_exit(&systrace_lock);
			}
#endif /* SYSCALLTRACE */
			rp->r_cpsr &= ~PSR_C_BIT;
			rp->r_r0 = rval1;
			rp->r_r1 = rval2;
		} else {
			int sig;
#ifdef SYSCALLTRACE
			if (syscalltrace) {
				mutex_enter(&systrace_lock);
				printf("%d: error=%d, id 0x%p\n",
				    p->p_pid, error, curthread);
				mutex_exit(&systrace_lock);
			}
#endif /* SYSCALLTRACE */
			if (error == EINTR && t->t_activefd.a_stale)
				error = EBADF;
			if (error == EINTR &&
			    (sig = lwp->lwp_cursig) != 0 &&
			    sigismember(&PTOU(p)->u_sigrestart, sig) &&
			    PTOU(p)->u_signal[sig - 1] != SIG_DFL &&
			    PTOU(p)->u_signal[sig - 1] != SIG_IGN)
				error = ERESTART;
			/*
			 * Set error indication and error code.
			 * The error code is returned by r12.
			 * r0 is not used because of keeping arg0
			 * for restartable syscall.
			 */
			rp->r_r12 = error;
			rp->r_cpsr |= PSR_C_BIT;
		}
	}

	/*
	 * From the proc(4) manual page:
	 * When exit from a system call is being traced, the traced process
	 * stops on completion of the system call just prior to checking for
	 * signals and returning to user level.  At this point all return
	 * values have been stored into the traced process's saved registers.
	 */
	if (proc_stop) {
		mutex_enter(&p->p_lock);
		if (PTOU(p)->u_systrap &&
		    prismember(&PTOU(p)->u_exitmask, code))
			stop(PR_SYSEXIT, code);
		mutex_exit(&p->p_lock);
	}

	/*
	 * If we are the parent returning from a successful
	 * vfork, wait for the child to exec or exit.
	 * This code must be here and not in the bowels of the system
	 * so that /proc can intercept exit from vfork in a timely way.
	 */
	if (t->t_flag & T_VFPARENT) {
		ASSERT(rp->r_r1 == 0 && error == 0);
		vfwait((pid_t)rval1);
		t->t_flag &= ~T_VFPARENT;
	}

	/*
	 * If profiling is active, bill the current PC in user-land
	 * and keep reposting until profiling is disabled.
	 */
	if (p->p_prof.pr_scale) {
		if (lwp->lwp_oweupc)
			profil_tick(rp->r_pc);
		repost = 1;
	}

sig_check:
	/*
	 * Reset flag for next time.
	 * We must do this after stopping on PR_SYSEXIT
	 * because /proc uses the information in lwp_eosys.
	 */
	lwp->lwp_eosys = NORMALRETURN;
	clear_stale_fd();
	t->t_flag &= ~T_FORKALL;

	if (t->t_astflag | t->t_sig_check) {
		/*
		 * Turn off the AST flag before checking all the conditions that
		 * may have caused an AST.  This flag is on whenever a signal or
		 * unusual condition should be handled after the next trap or
		 * syscall.
		 */
		astoff(t);
		t->t_sig_check = 0;

		/*
		 * The following check is legal for the following reasons:
		 *	1) The thread we are checking, is ourselves, so there is
		 *	   no way the proc can go away.
		 *	2) The only time we need to be protected by the
		 *	   lock is if the binding is changed.
		 *
		 *	Note we will still take the lock and check the binding
		 *	if the condition was true without the lock held.  This
		 *	prevents lock contention among threads owned by the
		 *	same proc.
		 */

		if (curthread->t_proc_flag & TP_CHANGEBIND) {
			mutex_enter(&p->p_lock);
			if (curthread->t_proc_flag & TP_CHANGEBIND) {
				timer_lwpbind();
				curthread->t_proc_flag &= ~TP_CHANGEBIND;
			}
			mutex_exit(&p->p_lock);
		}

		/*
		 * for kaio requests on the special kaio poll queue,
		 * copyout their results to user memory.
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
		if (ISHOLD(p) || (t->t_proc_flag & TP_EXITLWP))
			holdlwp();

		/*
		 * All code that sets signals and makes ISSIG_PENDING
		 * evaluate true must set t_sig_check afterwards.
		 */
		if (ISSIG_PENDING(t, lwp, p)) {
			if (issig(FORREAL))
				psig();
			t->t_sig_check = 1;	/* recheck next time */
		}

		if (sigprof) {
			realsigprof(code, error);
			t->t_sig_check = 1;	/* recheck next time */
		}

		/*
		 * If a performance counter overflow interrupt was
		 * delivered *during* the syscall, then re-enable the
		 * AST so that we take a trip through trap() to cause
		 * the SIGEMT to be delivered.
		 */
		if (lwp->lwp_pcb.pcb_flags & CPC_OVERFLOW)
			aston(t);
	}

	lwp->lwp_errno = 0;		/* clear error for next time */

#ifndef NPROBE
	/* Kernel probe */
	if (tnf_tracing_active) {
		TNF_PROBE_3(syscall_end, "syscall thread", /* CSTYLED */,
			tnf_long,	rval1,		rval1,
			tnf_long,	rval2,		rval2,
			tnf_long,	errno,		(long)error);
		repost = 1;
	}
#endif /* NPROBE */

	UCTX_OPS_TRAPRET(t);
	/*
	 * Set state to LWP_USER here so preempt won't give us a kernel
	 * priority if it occurs after this point.  Call CL_TRAPRET() to
	 * restore the user-level priority.
	 *
	 * It is important that no locks (other than spinlocks) be entered
	 * after this point before returning to user mode (unless lwp_state
	 * is set back to LWP_SYS).
	 *
	 * XXX Sampled times past this point are charged to the user.
	 */
	lwp->lwp_state = LWP_USER;

	if (t->t_trapret) {
		t->t_trapret = 0;
		thread_lock(t);
		CL_TRAPRET(t);
		thread_unlock(t);
	}
	if (CPU->cpu_runrun || t->t_schedflag & TS_ANYWAITQ)
		preempt();

	/*
	 * t_post_sys will be set if pcb_step is active.
	 */
	if (lwp->lwp_pcb.pcb_step != STEP_NONE) {
		prdostep();
		repost = 1;
	}


	/*
	 * The thread lock must be held in order to clear sysnum and reset
	 * lwp_ap atomically with respect to other threads in the system that
	 * may be looking at the args via lwp_ap from get_syscall_args().
	 */

	thread_lock(t);
	t->t_sysnum = 0;	/* no longer in a system call */

	/*
	 * In case the args were copied to the lwp, reset the
	 * pointer so the next syscall will have the right lwp_ap pointer.
	 */
	lwp->lwp_ap = (long *)&rp->r_r0;
	thread_unlock(t);

	lwp->lwp_argsaved = 0;

	/*
	 * If there was a continuing reason for post-syscall processing,
	 * set the t_post_sys flag for the next system call.
	 */
	if (repost)
		t->t_post_sys = 1;

	/*
	 * If there is a ustack registered for this lwp, and the stack rlimit
	 * has been altered, read in the ustack. If the saved stack rlimit
	 * matches the bounds of the ustack, update the ustack to reflect
	 * the new rlimit. If the new stack rlimit is RLIM_INFINITY, disable
	 * stack checking by setting the size to 0.
	 */
	if (lwp->lwp_ustack != 0 && lwp->lwp_old_stk_ctl != 0) {
		rlim64_t new_size;
		caddr_t top;
		stack_t stk;
		struct rlimit64 rl;

		mutex_enter(&p->p_lock);
		new_size = p->p_stk_ctl;
		top = p->p_usrstack;
		(void) rctl_rlimit_get(rctlproc_legacy[RLIMIT_STACK], p, &rl);
		mutex_exit(&p->p_lock);

		if (rl.rlim_cur == RLIM64_INFINITY)
			new_size = 0;

		if (copyin((stack_t *)lwp->lwp_ustack, &stk,
		    sizeof (stack_t)) == 0 &&
		    (stk.ss_size == lwp->lwp_old_stk_ctl ||
			stk.ss_size == 0) &&
		    stk.ss_sp == top - stk.ss_size) {
			stk.ss_sp = (void *)((uintptr_t)stk.ss_sp +
			    stk.ss_size - (uintptr_t)new_size);
			stk.ss_size = new_size;

			(void) copyout(&stk, (stack_t *)lwp->lwp_ustack,
			    sizeof (stack_t));
		}

		lwp->lwp_old_stk_ctl = 0;
	}

	syscall_mstate(LMS_SYSTEM, LMS_USER);
}

/*
 * nonexistent system call-- signal lwp (may want to handle it)
 * flag error if lwp won't see signal immediately
 * This works for old or new calling sequence.
 */
int64_t
nosys()
{
	tsignal(curthread, SIGSYS);
	return (set_errno(ENOSYS));
}

/*
 * Get the arguments to the current system call. See comment atop
 * save_syscall_args() regarding lwp_ap usage.
 */
uint_t
get_syscall_args(klwp_t *lwp, long *argp, int *nargsp)
{
	kthread_t	*t = lwptot(lwp);
	ulong_t	mask = 0xfffffffful;
	uint_t	code;
	long	*ap;
	int	nargs;

#if defined(_LP64)
	if (lwp_getdatamodel(lwp) == DATAMODEL_LP64)
		mask = 0xfffffffffffffffful;
#endif

	/*
	 * The thread lock must be held while looking at the arguments to ensure
	 * they don't go away via post_syscall().
	 * get_syscall_args() is the only routine to read them which is callable
	 * outside the LWP in question and hence the only one that must be
	 * synchronized in this manner.
	 */
	thread_lock(t);

	code = t->t_sysnum;
	ap = lwp->lwp_ap;

	thread_unlock(t);

	if (code != 0 && code < NSYSCALL) {
		nargs = LWP_GETSYSENT(lwp)[code].sy_narg;

		ASSERT(nargs <= MAXSYSARGS);

		*nargsp = nargs;
		while (nargs-- > 0)
			*argp++ = *ap++ & mask;
	} else {
		*nargsp = 0;
	}

	return (code);
}

#ifdef _SYSCALL32_IMPL
/*
 * Get the arguments to the current 32-bit system call.
 */
uint_t
get_syscall32_args(klwp_t *lwp, int *argp, int *nargsp)
{
	long args[MAXSYSARGS];
	uint_t i, code;

	code = get_syscall_args(lwp, args, nargsp);

	for (i = 0; i != *nargsp; i++)
		*argp++ = (int)args[i];
	return (code);
}
#endif

/*
 * Save the system call arguments in a safe place.
 *
 *	In case of nargs <= 4, lwp->lwp_ap normally points to r0..r3 in the
 *	regs structure. If the user is going to change the argument registers,
 *	or the stack, and might want to get the args (for /proc tracing),
 *	it must copy the args elsewhere via save_syscall_args().
 *
 *	In case of nargs > 4, lwp->lwp_ap should points to a copy of
 *	the system call arguments on the lwp_arg[] made from the registers
 *	and the user stack via this routine.
 *	Copy the args prior to change the stack or stack pointer.
 *	This is so /proc will be able to get a valid copy of the args
 *	from the user stack even after that stack has been changed.
 *
 *	This may be called from stop() even when we're not in a system call.
 *	Since there's no easy way to tell, this must be safe (not panic).
 *	If the copyins get data faults, return non-zero.
 */
int
save_syscall_args()
{
	kthread_t	*t = curthread;
	klwp_t		*lwp = ttolwp(t);
	uint_t		code = t->t_sysnum;
	uint_t		nargs;
	struct regs	*rp;

	if (lwp->lwp_argsaved || code == 0)
		return (0);		/* args already saved or not needed */

	if (code >= NSYSCALL) {
		nargs = 0;		/* illegal syscall */
	} else {
		struct sysent *se = LWP_GETSYSENT(lwp);
		struct sysent *callp = se + code;

		nargs = callp->sy_narg;
		if (LOADABLE_SYSCALL(callp) && nargs == 0) {
			krwlock_t	*module_lock;

			/*
			 * Find out how many arguments the system
			 * call uses.
			 *
			 * We have the property that loaded syscalls
			 * never change the number of arguments they
			 * use after they've been loaded once.  This
			 * allows us to stop for /proc tracing without
			 * holding the module lock.
			 * /proc is assured that sy_narg is valid.
			 */
			module_lock = lock_syscall(se, code);
			nargs = callp->sy_narg;
			rw_exit(module_lock);
		}
	}

	/*
	 * Fetch the system call arguments.
	 */
	if (nargs == 0)
		goto out;

	ASSERT(nargs <= MAXSYSARGS);

	rp = lwptoregs(lwp);

	lwp->lwp_arg[0] = rp->r_r0;
	lwp->lwp_arg[1] = rp->r_r1;
	lwp->lwp_arg[2] = rp->r_r2;
	lwp->lwp_arg[3] = rp->r_r3;
	if (nargs > 4 && copyin_args(rp, &lwp->lwp_arg[4], nargs - 4)) {
		return (-1);
	}

out:
	lwp->lwp_ap = lwp->lwp_arg;
	lwp->lwp_argsaved = 1;
	t->t_post_sys = 1;	/* so lwp_ap will be reset */
	return (0);
}

void
reset_syscall_args(void)
{
	ttolwp(curthread)->lwp_argsaved = 0;
}

/*
 * Call a system call which takes a pointer to the user args struct and
 * a pointer to the return values.  This is a bit slower than the standard
 * C arg-passing method in some cases.
 */
int64_t
syscall_ap(void)
{
	uint_t	error;
	struct sysent *callp;
	rval_t	rval;
	kthread_t *t = curthread;
	klwp_t	*lwp = ttolwp(t);
	struct regs *rp = lwptoregs(lwp);

	callp = LWP_GETSYSENT(lwp) + t->t_sysnum;

	/*
	 * If the arguments don't fit in registers r0-r3, make sure they
	 * have been copied to the lwp_arg array.
	 */
	if (callp->sy_narg > 4 && save_syscall_args())
		return ((int64_t)set_errno(EFAULT));

	rval.r_val1 = 0;
	rval.r_val2 = rp->r_r1;
	lwp->lwp_error = 0;	/* for old drivers */
	error = (*(callp->sy_call))(lwp->lwp_ap, &rval);
	if (error)
		return ((int64_t)set_errno(error));
	return (rval.r_vals);
}

/*
 * Load system call module.
 *	Returns with pointer to held read lock for module.
 */
static krwlock_t *
lock_syscall(struct sysent *table, uint_t code)
{
	krwlock_t	*module_lock;
	struct modctl	*modp;
	int		id;
	struct sysent   *callp;

	callp = table + code;
	module_lock = callp->sy_lock;

	/*
	 * Optimization to only call modload if we don't have a loaded
	 * syscall.
	 */
	rw_enter(module_lock, RW_READER);
	if (LOADED_SYSCALL(callp))
		return (module_lock);
	rw_exit(module_lock);

	for (;;) {
		if ((id = modload("sys", syscallnames[code])) == -1)
			break;

		/*
		 * If we loaded successfully at least once, the modctl
		 * will still be valid, so we try to grab it by filename.
		 * If this call fails, it's because the mod_filename
		 * was changed after the call to modload() (mod_hold_by_name()
		 * is the likely culprit).  We can safely just take
		 * another lap if this is the case;  the modload() will
		 * change the mod_filename back to one by which we can
		 * find the modctl.
		 */
		modp = mod_find_by_filename("sys", syscallnames[code]);

		if (modp == NULL)
			continue;

		mutex_enter(&mod_lock);

		if (!modp->mod_installed) {
			mutex_exit(&mod_lock);
			continue;
		}
		break;
	}
	rw_enter(module_lock, RW_READER);

	if (id != -1)
		mutex_exit(&mod_lock);

	return (module_lock);
}

/*
 * Loadable syscall support.
 *	If needed, load the module, then reserve it by holding a read
 *	lock for the duration of the call.
 *	Later, if the syscall is not unloadable, it could patch the vector.
 */
/*ARGSUSED*/
int64_t
loadable_syscall(
    long a0, long a1, long a2, long a3,
    long a4, long a5, long a6, long a7)
{
	klwp_t		*lwp = ttolwp(curthread);
	int64_t		rval;
	struct sysent	*callp;
	struct sysent	*se = LWP_GETSYSENT(lwp);
	krwlock_t	*module_lock;
	int		code, error = 0;
	int64_t		(*sy_call)();

	code = curthread->t_sysnum;
	callp = se + code;

	/*
	 * Try to autoload the system call if necessary.
	 */
	module_lock = lock_syscall(se, code);
	THREAD_KPRI_RELEASE();	/* drop priority given by rw_enter */

	/*
	 * we've locked either the loaded syscall or nosys
	 */

	/*
	 * Now that it's loaded, make sure enough args were copied.
	 */
	if (callp->sy_narg > 4 && save_syscall_args()) {
		error = EFAULT;
	}
	if (error) {
		rval = set_errno(error);
	} else if (callp->sy_flags & SE_ARGC) {
		sy_call = (int64_t (*)())callp->sy_call;
		rval = (*sy_call)(lwp->lwp_ap[0], lwp->lwp_ap[1],
			lwp->lwp_ap[2], lwp->lwp_ap[3], lwp->lwp_ap[4],
			lwp->lwp_ap[5]);
	} else {
		rval = syscall_ap();
	}

	THREAD_KPRI_REQUEST();	/* regain priority from read lock */
	rw_exit(module_lock);
	return (rval);
}

/*
 * Indirect syscall handled in libc on ARM architectures
 */
int64_t
indir()
{
	return (nosys());
}

/*
 * set_errno - set an error return from the current system call.
 *	This could be a macro.
 *	This returns the value it is passed, so that the caller can
 *	use tail-recursion-elimination and do return (set_errno(ERRNO));
 */
uint_t
set_errno(uint_t error)
{
	ASSERT(error != 0);		/* must not be used to clear errno */

	curthread->t_post_sys = 1;	/* have post_syscall do error return */
	return (ttolwp(curthread)->lwp_errno = error);
}

/*
 * set_proc_pre_sys - Set pre-syscall processing for entire process.
 */
void
set_proc_pre_sys(proc_t *p)
{
	kthread_t	*t;
	kthread_t	*first;

	ASSERT(MUTEX_HELD(&p->p_lock));

	t = first = p->p_tlist;
	do {
		t->t_pre_sys = 1;
	} while ((t = t->t_forw) != first);
}

/*
 * set_proc_post_sys - Set post-syscall processing for entire process.
 */
void
set_proc_post_sys(proc_t *p)
{
	kthread_t	*t;
	kthread_t	*first;

	ASSERT(MUTEX_HELD(&p->p_lock));

	t = first = p->p_tlist;
	do {
		t->t_post_sys = 1;
	} while ((t = t->t_forw) != first);
}

/*
 * set_proc_sys - Set pre- and post-syscall processing for entire process.
 */
void
set_proc_sys(proc_t *p)
{
	kthread_t	*t;
	kthread_t	*first;

	ASSERT(MUTEX_HELD(&p->p_lock));

	t = first = p->p_tlist;
	do {
		t->t_pre_sys = 1;
		t->t_post_sys = 1;
	} while ((t = t->t_forw) != first);
}

/*
 * set_all_proc_sys - set pre- and post-syscall processing flags for all
 * user processes.
 *
 * This is needed when auditing, tracing, or other facilities which affect
 * all processes are turned on.
 */
void
set_all_proc_sys()
{
	kthread_t	*t;
	kthread_t	*first;

	mutex_enter(&pidlock);
	t = first = curthread;
	do {
		t->t_pre_sys = 1;
		t->t_post_sys = 1;
	} while ((t = t->t_next) != first);
	mutex_exit(&pidlock);
}

/*
 * set_proc_ast - Set asynchronous service trap (AST) flag for all
 * threads in process.
 */
void
set_proc_ast(proc_t *p)
{
	kthread_t	*t;
	kthread_t	*first;

	ASSERT(MUTEX_HELD(&p->p_lock));

	t = first = p->p_tlist;
	do {
		aston(t);
	} while ((t = t->t_forw) != first);
}
