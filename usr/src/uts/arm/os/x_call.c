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

#pragma ident	"@(#)x_call.c	1.50	06/03/20 SMI"

/*
 * Facilities for cross-processor subroutine calls using "mailbox" interrupts.
 *
 */

#include <sys/types.h>

#include <sys/param.h>
#include <sys/t_lock.h>
#include <sys/thread.h>
#include <sys/cpuvar.h>
#include <sys/x_call.h>
#include <sys/cpu.h>
#include <sys/sunddi.h>
#include <sys/debug.h>
#include <sys/systm.h>
#include <sys/machsystm.h>
#include <sys/mutex_impl.h>
#include <sys/platform.h>
#include <sys/gic.h>
#include <asm/cpufunc.h>

static struct	xc_mbox xc_mboxes[X_CALL_LEVELS];
static kmutex_t xc_mbox_lock;
static uint_t 	xc_xlat_xcptoirq[X_CALL_LEVELS] = {
	IRQ_IPI_LO,
	IRQ_IPI_HI
};

static void xc_common(xc_func_t, xc_arg_t, xc_arg_t, xc_arg_t,
    int, cpuset_t, int);

static int	xc_initialized = 0;
extern cpuset_t	cpu_ready_set;

extern volatile int	panic_start_cpu;
extern void		panic_idle_saveregs(void);

/*
 * static inline void
 * xc_panic_check(void)
 *	Check whether the system is panicked or not.
 *	This function should be called while spin wait with disabling
 *	all interrupts.
 */
static inline void
xc_panic_check(void)
{
	if (panic_start_cpu != NCPU) {
		panic_idle_saveregs();
	}
}

void
xc_init()
{
	/*
	 * By making this mutex type MUTEX_DRIVER, the ones below
	 * XC_HI_PIL will be implemented as adaptive mutexes.
	 */
	mutex_init(&xc_mbox_lock, NULL, MUTEX_DRIVER,
	    (void *)ipltospl(XC_HI_PIL));

	xc_initialized = 1;
}

#define	CAPTURE_CPU_ARG	~0UL

/*
 * uint_t
 * xc_serv_hipri(caddr_t arg)
 *
 * X-call interrupt service routine for High-Priority Interrupt.
 * We're protected against changing CPUs by being a high-priority interrupt.
 */
uint_t
xc_serv_hipri(caddr_t arg)
{
	int	op;
	int	pri = (int)(uintptr_t)arg;
	struct cpu *cpup = CPU;

	ASSERT(pri == X_CALL_HIPRI);

	if (cpup->cpu_m.xc_pend[pri] == 0) {
		return (DDI_INTR_UNCLAIMED);
	}
	cpup->cpu_m.xc_pend[pri] = 0;
	MEMORY_BARRIER();

	op = cpup->cpu_m.xc_state[pri];

	/*
	 * Don't invoke a null function.
	 */
	if (xc_mboxes[pri].func != NULL) {
		cpup->cpu_m.xc_retval[pri] =
		    (*xc_mboxes[pri].func)(xc_mboxes[pri].arg1,
		    xc_mboxes[pri].arg2, xc_mboxes[pri].arg3);
	} else {
		cpup->cpu_m.xc_retval[pri] = 0;
	}

	/*
	 * Acknowledge that we have completed the x-call operation.
	 */
	cpup->cpu_m.xc_ack[pri] = 1;

	MEMORY_BARRIER();

	if (op == XC_CALL_OP) {
		return (DDI_INTR_CLAIMED);
	}

	/*
	 * for (op == XC_SYNC_OP)
	 * Wait for the initiator of the x-call to indicate
	 * that all CPUs involved can proceed.
	 */
	while (cpup->cpu_m.xc_wait[pri]) {
		ht_pause();
		xc_panic_check();
	}

	while (cpup->cpu_m.xc_state[pri] != XC_DONE) {
		ht_pause();
		xc_panic_check();
	}

	/*
	 * Acknowledge that we have received the directive to continue.
	 */
	ASSERT(cpup->cpu_m.xc_ack[pri] == 0);
	cpup->cpu_m.xc_ack[pri] = 1;

	MEMORY_BARRIER();

	return (DDI_INTR_CLAIMED);
}


/*
 * uint_t
 * xc_serv_lopri(caddr_t arg)
 *
 * X-call interrupt service routine for Low-Priority Interrupt.
 * Capture CPUs, while excuting this service routine.
 */
uint_t
xc_serv_lopri(caddr_t arg)
{
	int	pri = (int)(uintptr_t)arg;
	struct cpu *cpup = CPU;
	xc_arg_t *argp;
	xc_arg_t arg2val;

	ASSERT(pri == X_CALL_LOPRI);

	argp = &xc_mboxes[pri].arg2;
	arg2val = *argp;
	if (arg2val != CAPTURE_CPU_ARG &&
	    !CPU_IN_SET((cpuset_t)arg2val, cpup->cpu_id)) {
		return (DDI_INTR_UNCLAIMED);
	}
	ASSERT(arg2val == CAPTURE_CPU_ARG);
	if (cpup->cpu_m.xc_pend[pri] == 0) {
		return (DDI_INTR_UNCLAIMED);
	}
	cpup->cpu_m.xc_pend[pri] = 0;
	cpup->cpu_m.xc_ack[pri] = 1;

	MEMORY_BARRIER();

	for (;;) {
		if ((cpup->cpu_m.xc_state[pri] == XC_DONE) ||
		    (cpup->cpu_m.xc_pend[pri])) {
			break;
		}
		ht_pause();
	}
	return (DDI_INTR_CLAIMED);
}


/*
 * xc_do_call:
 */
static void
xc_do_call(
	xc_arg_t arg1,
	xc_arg_t arg2,
	xc_arg_t arg3,
	int pri,
	cpuset_t set,
	xc_func_t func,
	int sync)
{
	/* always grab highest mutex to avoid deadlock */
	ASSERT(getpil() < PIL_MAX);
	mutex_enter(&xc_mbox_lock);
	xc_common(func, arg1, arg2, arg3, pri, set, sync);
	mutex_exit(&xc_mbox_lock);
}


/*
 * xc_call: call specified function on all processors
 * remotes may continue after service
 * we wait here until everybody has completed.
 */
void
xc_call(
	xc_arg_t arg1,
	xc_arg_t arg2,
	xc_arg_t arg3,
	int pri,
	cpuset_t set,
	xc_func_t func)
{
	xc_do_call(arg1, arg2, arg3, pri, set, func, 0);
}

/*
 * xc_sync: call specified function on all processors
 * after doing work, each remote waits until we let
 * it continue; send the contiunue after everyone has
 * informed us that they are done.
 */
void
xc_sync(
	xc_arg_t arg1,
	xc_arg_t arg2,
	xc_arg_t arg3,
	int pri,
	cpuset_t set,
	xc_func_t func)
{
	xc_do_call(arg1, arg2, arg3, pri, set, func, 1);
}

/*
 * The routines xc_capture_cpus and xc_release_cpus
 * can be used in place of xc_sync in order to implement a critical
 * code section where all CPUs in the system can be controlled.
 * xc_capture_cpus is used to start the critical code section, and
 * xc_release_cpus is used to end the critical code section.
 */

/*
 * Capture the CPUs specified in order to start a x-call session,
 * and/or to begin a critical section.
 */
void
xc_capture_cpus(cpuset_t set)
{
	int cix;
	int lcx;
	struct cpu *cpup;
	int	i;
	cpuset_t *cpus;
	cpuset_t c;
	cpuset_t cpuset = NULL;
	uint_t	nsent = 0;

	/*
	 * Prevent deadlocks where we take an interrupt and are waiting
	 * for a mutex owned by one of the CPUs that is captured for
	 * the x-call, while that CPU is waiting for some x-call signal
	 * to be set by us.
	 *
	 * This mutex also prevents preemption, since it raises SPL above
	 * LOCK_LEVEL (it is a spin-type driver mutex).
	 */
	/* always grab highest mutex to avoid deadlock */
	ASSERT(getpil() < PIL_MAX);
	mutex_enter(&xc_mbox_lock);
	lcx = CPU->cpu_id;	/* now we're safe */

	ASSERT(CPU->cpu_flags & CPU_READY);

	/*
	 * Wait for all cpus
	 */
	cpus = (cpuset_t *)&xc_mboxes[X_CALL_LOPRI].arg2;
	if (CPU_IN_SET(*cpus, CPU->cpu_id))
		CPUSET_ATOMIC_DEL(*cpus, CPU->cpu_id);
	for (;;) {
		c = *(volatile cpuset_t *)cpus;
		CPUSET_AND(c, cpu_ready_set);
		if (CPUSET_ISNULL(c))
			break;
		ht_pause();
		xc_panic_check();
	}

	/*
	 * Store the set of CPUs involved in the x-call session, so that
	 * xc_release_cpus will know what CPUs to act upon.
	 */
	xc_mboxes[X_CALL_LOPRI].set = set;
	xc_mboxes[X_CALL_LOPRI].arg2 = CAPTURE_CPU_ARG;

	/*
	 * Now capture each CPU in the set and cause it to go into a
	 * holding pattern.
	 */
	i = 0;
	for (cix = 0; cix < NCPU; cix++) {
		if ((cpup = cpu[cix]) == NULL ||
		    (cpup->cpu_flags & CPU_READY) == 0) {
			/*
			 * In case CPU wasn't ready, but becomes ready later,
			 * take the CPU out of the set now.
			 */
			CPUSET_DEL(set, cix);
			continue;
		}
		if (cix != lcx && CPU_IN_SET(set, cix)) {
			cpup->cpu_m.xc_ack[X_CALL_LOPRI] = 0;
			cpup->cpu_m.xc_state[X_CALL_LOPRI] = XC_HOLD;
			cpup->cpu_m.xc_pend[X_CALL_LOPRI] = 1;
			CPUSET_ADD(cpuset, cix);
			nsent++;
		}
		i++;
		if (i >= ncpus)
			break;
	}

	MEMORY_BARRIER();

	/*
	 * Send IPI to requested cpu sets.
	 */
	if (cpuset) {
		CPU_STATS_ADDQ(CPU, sys, xcalls, nsent);
		gic_send_ipi(cpuset, IRQ_IPI_LO);
	}

	/*
	 * Wait here until all remote calls to acknowledge.
	 */
	i = 0;
	for (cix = 0; cix < NCPU; cix++) {
		if (lcx != cix && CPU_IN_SET(set, cix)) {
			cpup = cpu[cix];
			while (cpup->cpu_m.xc_ack[X_CALL_LOPRI] == 0) {
				ht_pause();
				xc_panic_check();
			}
			cpup->cpu_m.xc_ack[X_CALL_LOPRI] = 0;
		}
		i++;
		if (i >= ncpus)
			break;
	}

	MEMORY_BARRIER();
}

/*
 * Release the CPUs captured by xc_capture_cpus, thus terminating the
 * x-call session and exiting the critical section.
 */
void
xc_release_cpus(void)
{
	int cix;
	int lcx = (int)(CPU->cpu_id);
	cpuset_t set = xc_mboxes[X_CALL_LOPRI].set;
	struct cpu *cpup;
	int	i;

	ASSERT(MUTEX_HELD(&xc_mbox_lock));

	/*
	 * Allow each CPU to exit its holding pattern.
	 */
	i = 0;
	for (cix = 0; cix < NCPU; cix++) {
		if ((cpup = cpu[cix]) == NULL)
			continue;
		if ((cpup->cpu_flags & CPU_READY) &&
		    (cix != lcx) && CPU_IN_SET(set, cix)) {
			/*
			 * Clear xc_ack since we will be waiting for it
			 * to be set again after we set XC_DONE.
			 */
			cpup->cpu_m.xc_state[X_CALL_LOPRI] = XC_DONE;
		}
		i++;
		if (i >= ncpus)
			break;
	}

	xc_mboxes[X_CALL_LOPRI].arg2 = 0;
	mutex_exit(&xc_mbox_lock);
}

/*
 * Common code to call a specified function on a set of processors.
 * sync specifies what kind of waiting is done.
 *	-1 - no waiting, don't release remotes
 *	0 - no waiting, release remotes immediately
 *	1 - run service locally w/o waiting for remotes.
 */
static void
xc_common(
	xc_func_t func,
	xc_arg_t arg1,
	xc_arg_t arg2,
	xc_arg_t arg3,
	int pri,
	cpuset_t set,
	int sync)
{
	int cix;
	int lcx = (int)(CPU->cpu_id);
	struct cpu *cpup;
	cpuset_t cpuset;

	ASSERT(panicstr == NULL);

	ASSERT(MUTEX_HELD(&xc_mbox_lock));
	ASSERT(CPU->cpu_flags & CPU_READY);

	CPUSET_ZERO(cpuset);

	/*
	 * Set up the service definition mailbox.
	 */
	xc_mboxes[pri].func = func;
	xc_mboxes[pri].arg1 = arg1;
	xc_mboxes[pri].arg2 = arg2;
	xc_mboxes[pri].arg3 = arg3;

	/*
	 * Request service on all remote processors.
	 */
	for (cix = 0; cix < NCPU; cix++) {
		if ((cpup = cpu[cix]) == NULL ||
		    (cpup->cpu_flags & CPU_READY) == 0) {
			/*
			 * In case the non-local CPU is not ready but becomes
			 * ready later, take it out of the set now. The local
			 * CPU needs to remain in the set to complete the
			 * requested function.
			 */
			if (cix != lcx)
				CPUSET_DEL(set, cix);
		} else if (cix != lcx && CPU_IN_SET(set, cix)) {
			CPU_STATS_ADDQ(CPU, sys, xcalls, 1);
			cpup->cpu_m.xc_ack[pri] = 0;
			cpup->cpu_m.xc_wait[pri] = sync;
			if (sync > 0)
				cpup->cpu_m.xc_state[pri] = XC_SYNC_OP;
			else
				cpup->cpu_m.xc_state[pri] = XC_CALL_OP;
			cpup->cpu_m.xc_pend[pri] = 1;
			CPUSET_ADD(cpuset, cix);
		}
	}

	MEMORY_BARRIER();

	/*
	 * Send IPI to requested cpu sets.
	 */
	if (cpuset) {
		gic_send_ipi(cpuset, xc_xlat_xcptoirq[pri]);
	}

	/*
	 * Run service locally
	 */
	if (CPU_IN_SET(set, lcx) && func != NULL)
		CPU->cpu_m.xc_retval[pri] = (*func)(arg1, arg2, arg3);

	if (sync == -1)
		return;

	/*
	 * Wait here until all remote calls acknowledge.
	 */
	for (cix = 0; cix < NCPU; cix++) {
		if (lcx != cix && CPU_IN_SET(set, cix)) {
			cpup = cpu[cix];
			while (cpup->cpu_m.xc_ack[pri] == 0) {
				ht_pause();
				xc_panic_check();
			}
			cpup->cpu_m.xc_ack[pri] = 0;
		}
	}
	MEMORY_BARRIER();

	if (sync == 0)
		return;

	/*
	 * Release any waiting CPUs
	 */
	for (cix = 0; cix < NCPU; cix++) {
		if (lcx != cix && CPU_IN_SET(set, cix)) {
			cpup = cpu[cix];
			if (cpup != NULL && (cpup->cpu_flags & CPU_READY)) {
				cpup->cpu_m.xc_wait[pri] = 0;
				cpup->cpu_m.xc_state[pri] = XC_DONE;
			}
		}
	}

	MEMORY_BARRIER();

	/*
	 * Wait for all CPUs to acknowledge completion before we continue.
	 * Without this check it's possible (on a VM or hyper-threaded CPUs
	 * or in the presence of Service Management Interrupts which can all
	 * cause delays) for the remote processor to still be waiting by
	 * the time xc_common() is next invoked with the sync flag set
	 * resulting in a deadlock.
	 */
	for (cix = 0; cix < NCPU; cix++) {
		if (lcx != cix && CPU_IN_SET(set, cix)) {
			cpup = cpu[cix];
			if (cpup != NULL && (cpup->cpu_flags & CPU_READY)) {
				while (cpup->cpu_m.xc_ack[pri] == 0) {
					ht_pause();
					xc_panic_check();
				}
				cpup->cpu_m.xc_ack[pri] = 0;
			}
		}
	}
}

/*
 * xc_trycall: attempt to call specified function on all processors
 * remotes may wait for a long time
 * we continue immediately
 */
void
xc_trycall(
	xc_arg_t arg1,
	xc_arg_t arg2,
	xc_arg_t arg3,
	cpuset_t set,
	xc_func_t func)
{
	int		save_kernel_preemption;
	extern int	IGNORE_KERNEL_PREEMPTION;

	/*
	 * If we can grab the mutex, we'll do the cross-call.  If not -- if
	 * someone else is already doing a cross-call -- we won't.
	 */

	save_kernel_preemption = IGNORE_KERNEL_PREEMPTION;
	IGNORE_KERNEL_PREEMPTION = 1;
	if (mutex_tryenter(&xc_mbox_lock)) {
		xc_common(func, arg1, arg2, arg3, X_CALL_HIPRI, set, -1);
		mutex_exit(&xc_mbox_lock);
	}
	IGNORE_KERNEL_PREEMPTION = save_kernel_preemption;
}

/*
 * Used by the debugger to cross-call the other CPUs, thus causing them to
 * enter the debugger.  We can't hold locks, so we spin on the cross-call
 * lock until we get it.  When we get it, we send the cross-call, and assume
 * that we successfully stopped the other CPUs.
 */
void
kdi_xc_others(int this_cpu, void (*func)(void))
{
	extern int	IGNORE_KERNEL_PREEMPTION;
	int save_kernel_preemption;
	mutex_impl_t *lp;
	cpuset_t set;
	int x;

	if (!xc_initialized)
		return;

	CPUSET_ALL_BUT(set, this_cpu);

	save_kernel_preemption = IGNORE_KERNEL_PREEMPTION;
	IGNORE_KERNEL_PREEMPTION = 1;

	lp = (mutex_impl_t *)&xc_mbox_lock;
	for (x = 0; x < 0x400000; x++) {
		if (lock_spin_try(&lp->m_spin.m_spinlock)) {
			xc_common((xc_func_t)func, 0, 0, 0, X_CALL_HIPRI,
			    set, -1);
			lp->m_spin.m_spinlock = 0; /* XXX */
			break;
		}
		(void) xc_serv_lopri((caddr_t)X_CALL_LOPRI);
	}
	IGNORE_KERNEL_PREEMPTION = save_kernel_preemption;
}

/*
 * void
 * xc_lock(void)
 *	Acquire xcall's lock.
 *
 *	This function is used to block xcall on other CPUs.
 *
 * Remarks:
 *	- xc_lock() raises PIL to PIL_MAX.
 *	- The caller must NOT call xc_lock() on PIL_MAX.
 */
void
xc_lock(void)
{
	ASSERT(getpil() < PIL_MAX);
	mutex_enter(&xc_mbox_lock);
}

/*
 * void
 * xc_unlock(void)
 *	Release xcall's lock.
 */
void
xc_unlock(void)
{
	mutex_exit(&xc_mbox_lock);
}
