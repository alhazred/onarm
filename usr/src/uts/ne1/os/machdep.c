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
 * Copyright (c) 2006-2009 NEC Corporation
 */

#ident	"@(#)ne1/os/machdep.c"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/mman.h>
#include <sys/vm.h>

#include <sys/disp.h>
#include <sys/class.h>

#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/kmem.h>

#include <sys/reboot.h>
#include <sys/uadmin.h>
#include <sys/callb.h>

#include <sys/cred.h>
#include <sys/vnode.h>
#include <sys/file.h>

#include <sys/procfs.h>
#include <sys/acct.h>

#include <sys/vfs.h>
#include <sys/dnlc.h>
#include <sys/var.h>
#include <sys/cmn_err.h>
#include <sys/utsname.h>
#include <sys/debug.h>
#include <sys/kdi_impl.h>

#include <sys/dumphdr.h>
#include <sys/dumpdev.h>
#include <sys/bootconf.h>
#include <sys/varargs.h>
#include <sys/promif.h>
#include <sys/modctl.h>

#include <sys/consdev.h>
#include <sys/frame.h>

#include <sys/sunddi.h>
#include <sys/ddidmareq.h>
#include <sys/regset.h>
#include <sys/privregs.h>
#include <sys/clock.h>
#include <sys/stack.h>
#include <sys/trap.h>
#include <sys/pic.h>
#include <vm/hat.h>
#include <vm/hat_arm.h>
#include <vm/anon.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>
#include <vm/seg_kp.h>
#include <sys/swap.h>
#include <sys/thread.h>
#include <sys/sysconf.h>
#include <sys/vm_machparam.h>
#include <sys/archsystm.h>
#include <sys/machsystm.h>
#include <sys/machlock.h>
#include <sys/x_call.h>
#include <sys/instance.h>

#include <sys/time.h>
#include <sys/smp_impldefs.h>
#include <sys/atomic.h>
#include <sys/panic.h>
#include <sys/cpuvar.h>
#include <sys/dtrace.h>
#include <sys/bl.h>
#include <sys/nvpair.h>
#include <sys/pool_pset.h>
#include <sys/autoconf.h>
#include <sys/kdi.h>
#include <sys/mpcore.h>
#include <sys/platform.h>
#include <sys/gic.h>
#include <sys/mach_reboot.h>
#include <sys/wdt.h>
#include <asm/cpufunc.h>
#include <c2/audit.h>


#ifdef	TRAPTRACE
#include <sys/traptrace.h>
#endif	/* TRAPTRACE */

/*
 * The panicbuf array is used to record messages and state:
 */
char panicbuf[PANICBUFSIZE];

#define	PANIC_IDLE_MAXWAIT	0x100

/* CPU number that called panic_enter_hw() */
volatile uint_t	panic_start_cpu = NCPU;

/*
 * Variables for monitoring system dump.
 * They are used only for NFS dump.
 */
static volatile int	panic_dump_start;
static volatile int	panic_dump_end;

/* CPU mask that keeps CPUs on panic idle loop */
static cpuset_t		panic_idle_enter;

/* Address of struct regs that keeps registers when panicked. */
struct regs	*panic_curframe[NCPU];

/* Preserve current hat when panicked. */
hat_t	*panic_curhat[NCPU];

/* Preserve cpu_intr_actv flags */
uint_t	panic_intr_actv[NCPU];

/*
 * maxphys - used during physio
 * klustsize - used for klustering by swapfs and specfs
 */
int maxphys = 56 * 1024;    /* XXX See vm_subr.c - max b_count in physio */
int klustsize = 56 * 1024;

/*
 * defined here, though unused on NE1,
 * to make kstat_fr.c happy.
 */
int vac;

static boolean_t	is_nfsdump(void);

void stop_other_cpus();
void debug_enter(char *);

void plat_irq_init(void);


/*
 * Machine dependent code to reboot.
 * "mdep" is interpreted as a character pointer; if non-null, it is a pointer
 * to a string to be used as the argument string when rebooting.
 *
 * "invoke_cb" is a boolean. It is set to true when mdboot() can safely
 * invoke CB_CL_MDBOOT callbacks before shutting the system down, i.e. when
 * we are in a normal shutdown sequence (interrupts are not blocked, the
 * system is not panic'ing or being suspended).
 */
/*ARGSUSED*/
void
mdboot(int cmd, int fcn, char *mdep, boolean_t invoke_cb)
{
	panic_dump_end = 1;
	SYNC_BARRIER();

	if (!panicstr) {
		kpreempt_disable();
		affinity_set(CPU_CURRENT);
	}

	/*
	 * XXX - rconsvp is set to NULL to ensure that output messages
	 * are sent to the underlying "hardware" device using the
	 * monitor's printf routine since we are in the process of
	 * either rebooting or halting the machine.
	 */
	rconsvp = NULL;

	/* make sure there are no more changes to the device tree */
	devtree_freeze();

	if (invoke_cb)
		(void) callb_execute_class(CB_CL_MDBOOT, NULL);

#ifndef	PAGE_RETIRE_DISABLE
	/*
	 * Clear any unresolved UEs from memory.
	 */
	page_retire_mdboot();
#endif	/* !PAGE_RETIRE_DISABLE */

	/*
	 * stop other cpus and raise our priority.  since there is only
	 * one active cpu after this, and our priority will be too high
	 * for us to be preempted, we're essentially single threaded
	 * from here on out.
	 */
	(void) spl6();
	if (!panicstr) {
		mutex_enter(&cpu_lock);
		pause_cpus(NULL);
		mutex_exit(&cpu_lock);
	}

	/*
	 * try and reset leaf devices.  reset_leaves() should only
	 * be called when there are no other threads that could be
	 * accessing devices
	 */
	reset_leaves();

	(void) spl8();
	gic_shutdown(cmd, fcn);

	wdt_stop(CPU->cpu_id);

	if (fcn == AD_HALT || fcn == AD_POWEROFF)
		halt((char *)NULL);
	else {
		/*
		 * in case of panic, panic_stopcpus() should be called
		 */
		if (!panicstr) {
			stop_other_cpus();	/* send stop signal to other CPUs */
		}
		prom_reboot("");
        }
	/*NOTREACHED*/
}

/* mdpreboot - may be called prior to mdboot while root fs still mounted */
/*ARGSUSED*/
void
mdpreboot(int cmd, int fcn, char *mdep)
{
	gic_preshutdown(cmd, fcn);
}

void
idle_other_cpus()
{
	int cpuid = CPU->cpu_id;
	cpuset_t xcset;

	ASSERT(cpuid < NCPU);
	CPUSET_ALL_BUT(xcset, cpuid);
	xc_capture_cpus(xcset);
}

void
resume_other_cpus()
{
	ASSERT(CPU->cpu_id < NCPU);

	xc_release_cpus();
}

extern void	mp_halt(char *);

void
stop_other_cpus()
{
	int cpuid = CPU->cpu_id;
	cpuset_t xcset;

	ASSERT(cpuid < NCPU);

	/*
	 * xc_trycall will attempt to make all other CPUs execute mp_halt,
	 * and will return immediately regardless of whether or not it was
	 * able to make them do it.
	 */
	CPUSET_ALL_BUT(xcset, cpuid);
	xc_trycall(NULL, NULL, NULL, xcset, (int (*)())mp_halt);
}

/*
 *	Machine dependent abort sequence handling
 */
void
abort_sequence_enter(char *msg)
{
	if (abort_enable == 0) {
		if (audit_active)
			audit_enterprom(0);
		return;
	}
	if (audit_active)
		audit_enterprom(1);
	debug_enter(msg);
	if (audit_active)
		audit_exitprom(1);
}

/*
 * Enter debugger.  Called when the user types ctrl-alt-d or whenever
 * code wants to enter the debugger and possibly resume later.
 */
void
debug_enter(
	char	*msg)		/* message to print, possibly NULL */
{
	if (dtrace_debugger_init != NULL)
		(*dtrace_debugger_init)();

	if (msg)
		prom_printf("%s\n", msg);

	if (boothowto & RB_DEBUG)
		kmdb_enter();

	if (dtrace_debugger_fini != NULL)
		(*dtrace_debugger_fini)();
}

void
reset(void)
{
	prom_printf("Ready for start-up.\n");

	rbt_enter_reboot(RBT_CALLER_RESET);
	/*NOTREACHED*/
}

/*
 * Halt the machine and return to the monitor
 */
void
halt(char *s)
{
	stop_other_cpus();	/* send stop signal to other CPUs */
	if (s)
		prom_printf("(%s) \n", s);
	prom_exit_to_mon();
	/*NOTREACHED*/
}

/*
 * Enter monitor.  Called via cross-call from stop_other_cpus().
 */
void
mp_halt(char *msg)
{
	if (msg)
		prom_printf("%s\n", msg);

	wdt_stop(CPU->cpu_id);

	rbt_enter_reboot(RBT_CALLER_HALT);
	/*NOTREACHED*/
}

/*
 * Initiate interrupt redistribution.
 */
void
i_ddi_intr_redist_all_cpus()
{
}

/*
 * XXX These probably ought to live somewhere else
 * XXX They are called from mem.c
 */

/*
 * Convert page frame number to an OBMEM page frame number
 * (i.e. put in the type bits -- zero for this implementation)
 */
pfn_t
impl_obmem_pfnum(pfn_t pf)
{
	return (pf);
}

#ifdef	NM_DEBUG
int nmi_test = 0;	/* checked in intentry.s during clock int */
int nmtest = -1;
nmfunc1(arg, rp)
int	arg;
struct regs *rp;
{
	printf("nmi called with arg = %x, regs = %x\n", arg, rp);
	nmtest += 50;
	if (arg == nmtest) {
		printf("ip = %x\n", rp->r_pc);
		return (1);
	}
	return (0);
}

#endif

int
goany(void)
{
	prom_printf("Type any key to continue ");
	(void) prom_getchar();
	prom_printf("\n");
	return (1);
}

void
kadb_uses_kernel()
{
	/* only used on intel */
}

/*
 *	the interface to the outside world
 */

/*
 * poll_port -- wait for a register to achieve a
 *		specific state.  Arguments are a mask of bits we care about,
 *		and two sub-masks.  To return normally, all the bits in the
 *		first sub-mask must be ON, all the bits in the second sub-
 *		mask must be OFF.  If about seconds pass without the register
 *		achieving the desired bit configuration, we return 1, else
 *		0.
 */
int
poll_port(ushort_t port, ushort_t mask, ushort_t onbits, ushort_t offbits)
{
	int i;
	ushort_t maskval;

	for (i = 500000; i; i--) {
		maskval = inb(port) & mask;
		if (((maskval & onbits) == onbits) &&
			((maskval & offbits) == 0))
			return (0);
		drv_usecwait(10);
	}
	return (1);
}

/*
 * set_idle_cpu is called from idle() when a CPU becomes idle.
 */
/*LINTED: static unused */
static uint_t last_idle_cpu;

/*ARGSUSED*/
void
set_idle_cpu(int cpun)
{
	last_idle_cpu = cpun;
#ifdef notyet
	apic_set_idlecpu(cpun);		/* was psm_set_idle_cpuf */
#endif
}

/*
 * unset_idle_cpu is called from idle() when a CPU is no longer idle.
 */
/*ARGSUSED*/
void
unset_idle_cpu(int cpun)
{
#ifdef notyet
	apic_unset_idlecpu(cpun);	/* was psm_unset_idle_cpuf */
#endif
}

/*
 * This routine is almost correct now, but not quite.  It still needs the
 * equivalent concept of "hres_last_tick", just like on the sparc side.
 * The idea is to take a snapshot of the hi-res timer while doing the
 * hrestime_adj updates under hres_lock in locore, so that the small
 * interval between interrupt assertion and interrupt processing is
 * accounted for correctly.  Once we have this, the code below should
 * be modified to subtract off hres_last_tick rather than hrtime_base.
 *
 * I'd have done this myself, but I don't have source to all of the
 * vendor-specific hi-res timer routines (grrr...).  The generic hook I
 * need is something like "gethrtime_unlocked()", which would be just like
 * gethrtime() but would assume that you're already holding CLOCK_LOCK().
 * This is what the GET_HRTIME() macro is for on sparc (although it also
 * serves the function of making time available without a function call
 * so you don't take a register window overflow while traps are disabled).
 */
void
pc_gethrestime(timestruc_t *tp)
{
	int lock_prev;
	timestruc_t now;
	int nslt;		/* nsec since last tick */
	int adj;		/* amount of adjustment to apply */
	hrtime_t hr;

loop:
	lock_prev = hres_lock;
	now = hrestime;
	hr = gethrtime();
	if (hr < hres_last_tick) {
		/*
		 * nslt < 0 means a tick came between sampling
		 * gethrtime() and hres_last_tick; restart the loop
		 */

		goto loop;
	}
	nslt = (int)(hr - hres_last_tick);
	now.tv_nsec += nslt;
	if (hrestime_adj != 0) {
		if (hrestime_adj > 0) {
			adj = (nslt >> ADJ_SHIFT);
			if (adj > hrestime_adj)
				adj = (int)hrestime_adj;
		} else {
			adj = -(nslt >> ADJ_SHIFT);
			if (adj < hrestime_adj)
				adj = (int)hrestime_adj;
		}
		now.tv_nsec += adj;
	}
	while ((unsigned long)now.tv_nsec >= NANOSEC) {

		/*
		 * We might have a large adjustment or have been in the
		 * debugger for a long time; take care of (at most) four
		 * of those missed seconds (tv_nsec is 32 bits, so
		 * anything >4s will be wrapping around).  However,
		 * anything more than 2 seconds out of sync will trigger
		 * timedelta from clock() to go correct the time anyway,
		 * so do what we can, and let the big crowbar do the
		 * rest.  A similar correction while loop exists inside
		 * hres_tick(); in all cases we'd like tv_nsec to
		 * satisfy 0 <= tv_nsec < NANOSEC to avoid confusing
		 * user processes, but if tv_sec's a little behind for a
		 * little while, that's OK; time still monotonically
		 * increases.
		 */

		now.tv_nsec -= NANOSEC;
		now.tv_sec++;
	}
	if ((hres_lock & ~1) != lock_prev)
		goto loop;

	*tp = now;
}

void
gethrestime_lasttick(timespec_t *tp)
{
	int s;

	s = hr_clock_lock();
	*tp = hrestime;
	hr_clock_unlock(s);
}

time_t
gethrestime_sec(void)
{
	timestruc_t now;

	gethrestime(&now);
	return (now.tv_sec);
}

/*
 * Initialize a kernel thread's stack
 */

caddr_t
thread_stk_init(caddr_t stk)
{
#if	STACK_ENTRY_ALIGN == 8
	uintptr_t	addr = (uintptr_t)stk;

	ASSERT(((uintptr_t)stk & (STACK_ALIGN - 1)) == 0);

	addr -= SA(MINFRAME);
	addr = P2ALIGN(addr, STACK_ENTRY_ALIGN);
	return (caddr_t)addr;

#else	/* !STACK_ENTRY_ALIGN == 8 */
	ASSERT(((uintptr_t)stk & (STACK_ALIGN - 1)) == 0);
	return (stk - SA(MINFRAME));
#endif	/* STACK_ENTRY_ALIGN == 8 */
}

/*
 * Initialize lwp's kernel stack.
 */

#if	STACK_ENTRY_ALIGN == 8
#define	LWP_STACK_ALIGN		STACK_ENTRY_ALIGN
#else	/* STACK_ENTRY_ALIGN != 8 */
#define	LWP_STACK_ALIGN		STACK_ALIGN
#endif	/* STACK_ENTRY_ALIGN == 8 */

caddr_t
lwp_stk_init(klwp_t *lwp, caddr_t stk)
{
	caddr_t oldstk;

	oldstk = stk;
	stk -= SA(sizeof (struct regs) + SA(MINFRAME));
	stk = (caddr_t)((uintptr_t)stk & ~(LWP_STACK_ALIGN - 1ul));
	bzero(stk, oldstk - stk);
	lwp->lwp_regs = (void *)(stk + SA(MINFRAME));

	return (stk);
}

/*ARGSUSED*/
void
lwp_stk_fini(klwp_t *lwp)
{}

void
lwp_stk_cache_init(void)
{
	/* Nop */
}

/*
 * If we're not the panic CPU, we wait in panic_idle for reboot.
 */
void
panic_idle(struct regs *rp)
{
	cpu_t		*cp = CPU;
	processorid_t	cpuid = cp->cpu_id;
	boolean_t	nfsdump;
	int		result, i;

	wdt_stop(cpuid);

	nfsdump = is_nfsdump();

	CPUSET_ATOMIC_XADD(panic_idle_enter, cpuid, result);

	if (cpuid == panic_start_cpu || (nfsdump && result != 0)) {
		/* We must handle interrupt for system dump over NFS. */
		return;
	}

	/* Save interrupt frame for debugging. */
	atomic_cas_ptr(&panic_curframe[cpuid], NULL, rp);

	/* Save current hat. */
	atomic_cas_ptr(&panic_curhat[cpuid], NULL, cp->cpu_current_hat);

	/* Save cpu_intr_actv bits. */
	atomic_cas_uint(&panic_intr_actv[cpuid], 0, cp->cpu_intr_actv);

	splx(ipltospl(CLOCK_LEVEL));
	(void) setjmp(&curthread->t_pcb);

	SYNC_BARRIER();

	if (nfsdump) {
		int	base;

		/*
		 * If dump device is on NFS, CPUs other than panic CPU should
		 * switch to master kernel L1PT.
		 */
		hat_switch(kas.a_hat);

		/* Enable interrupt for system dump over NFS. */
		while (panic_dump_start == 0);

		kpreempt_disable();
		base = cp->cpu_base_spl;
		cp->cpu_base_spl = 0;
		(void)spl0();
		SYNC_BARRIER();

		/* Wait for the end of system dump. */
		while (panic_dump_end == 0);

		cp->cpu_base_spl = base;
		splx(ipltospl(CLOCK_LEVEL));
		kpreempt_enable();
	}

	cp->cpu_flags |= CPU_QUIESCED;

	rbt_enter_reboot(RBT_CALLER_PANIC);
	/*NOTREACHED*/
}

/*
 * Stop the other CPUs by sending IPI to them and forcing them to enter
 * the panic_idle() loop above.
 */
/*ARGSUSED*/
void
panic_stopcpus(cpu_t *cp, kthread_t *t, int spl)
{
	processorid_t	i;
	cpuset_t	xcset;

	wdt_stop(CPU->cpu_id);

	(void) splzs();

	/*
	 * Send the highest level interrupt to all CPUs but me.
	 * hilevel_intr_prolog() will call panic_idle().
	 * We don't want to use xc_trycall() because it may not send interrupt.
	 */
	CPUSET_ALL_BUT(xcset, cp->cpu_id);
	gic_send_ipi(xcset, IRQ_IPI_HI);
	SYNC_BARRIER();

	for (i = 0; i < NCPU; i++) {
		if (i != cp->cpu_id && cpu[i] != NULL &&
		    (cpu[i]->cpu_flags & CPU_EXISTS)) {
			int	loop;

			if (!(cpu[i]->cpu_flags & CPU_ENABLE)) {
				continue;
			}

			/* Wait for CPU to enter panic idle loop. */
			loop = 0;
			while (!CPU_IN_SET((volatile cpuset_t)panic_idle_enter,
					   i)) {
				gic_send_ipi(CPUSET(i), IRQ_IPI_HI);
				SYNC_BARRIER();
				drv_usecwait(10);
				loop++;
				if (loop >= PANIC_IDLE_MAXWAIT) {
					prom_printf("cpu%d: panic_idle() may "
						    "not be called.\n", i);
					break;
				}
			}
		}
	}
}

/*
 * Platform callback following each entry to panicsys().
 */
/*ARGSUSED*/
void
panic_enter_hw(int spl)
{
	struct regs	*frame;
	extern char	panic_stack[];
	cpu_t		*cp = CPU;

	/* We can use CPU macro safely because preemption is disabled here. */
	atomic_cas_uint(&panic_start_cpu, NCPU, cp->cpu_id);

	/*
	 * Set register save area in panic_stack into panic_curframe.
	 */
	frame = (struct regs *)(panic_stack + PANICSTKSIZE -
				sizeof(struct regs));
	atomic_cas_ptr(&panic_curframe[panic_start_cpu], NULL, frame);
	atomic_cas_ptr(&panic_curhat[panic_start_cpu], NULL,
		       cp->cpu_current_hat);
	atomic_cas_uint(&panic_intr_actv[panic_start_cpu], 0,
			cp->cpu_intr_actv);

	SYNC_BARRIER();
}

/*
 * Platform-specific code to execute after panicstr is set.
 */
/*ARGSUSED*/
void
panic_quiesce_hw(panic_data_t *pdp)
{
	/* Issue sync barrier so that panicstr is visible to other CPUs. */
	SYNC_BARRIER();

#ifdef	TRAPTRACE
	/*
	 * Turn off TRAPTRACE
	 */
	TRAPTRACE_FREEZE;
#endif	/* TRAPTRACE */
}

/*
 * Platform callback prior to writing crash dump.
 */
/*ARGSUSED*/
void
panic_dump_hw(int spl)
{
	cpu_t	*cp = CPU;
	extern int	mtc_off;

	/* Turn off high level interrupt bits explicitly. */
	cp->cpu_intr_actv &= ((1 << (LOCK_LEVEL + 1)) - 1);

	/* Mask console interrupt. */
	writel(0, NE1_CONSOLE_IER);

	/* Turn off multi-threaded device configuration. */
	mtc_off = 1;


	/*
	 * Switch to master kernel L1PT so that we can access kernel address
	 * space even if user L1PT is corrupted.
	 */
	hat_switch(kas.a_hat);

	panic_dump_start = 1;
	SYNC_BARRIER();
}

/*ARGSUSED*/
void
plat_tod_fault(enum tod_fault_type tod_bad)
{
}

/*ARGSUSED*/
int
blacklist(int cmd, const char *scheme, nvlist_t *fmri, const char *class)
{
	return (ENOTSUP);
}

/*
 * The underlying console output routines are protected by raising IPL in case
 * we are still calling into the early boot services.  Once we start calling
 * the kernel console emulator, it will disable interrupts completely during
 * character rendering (see sysp_putchar, for example).  Refer to the comments
 * and code in common/os/console.c for more information on these callbacks.
 */
/*ARGSUSED*/
int
console_enter(int busy)
{
	return (splzs());
}

/*ARGSUSED*/
void
console_exit(int busy, int spl)
{
	splx(spl);
}

/*
 * Allocate a region of virtual address space, unmapped.
 * Stubbed out except on sparc, at least for now.
 */
/*ARGSUSED*/
void *
boot_virt_alloc(void *addr, size_t size)
{
	return (addr);
}

void
tenmicrosec(void)
{
	int		i;
	hrtime_t start, end;

	start = end = gethrtime();
	while ((end - start) < (10 * (NANOSEC / MICROSEC))) {
#ifdef notyet
		SMT_PAUSE();
#endif
		end = gethrtime();
	}
}

/*
 * get_cpu_mstate() is passed an array of timestamps, NCMSTATES
 * long, and it fills in the array with the time spent on cpu in
 * each of the mstates, where time is returned in nsec.
 *
 * No guarantee is made that the returned values in times[] will
 * monotonically increase on sequential calls, although this will
 * be true in the long run. Any such guarantee must be handled by
 * the caller, if needed. This can happen if we fail to account
 * for elapsed time due to a generation counter conflict, yet we
 * did account for it on a prior call (see below).
 *
 * The complication is that the cpu in question may be updating
 * its microstate at the same time that we are reading it.
 * Because the microstate is only updated when the CPU's state
 * changes, the values in cpu_intracct[] can be indefinitely out
 * of date. To determine true current values, it is necessary to
 * compare the current time with cpu_mstate_start, and add the
 * difference to times[cpu_mstate].
 *
 * This can be a problem if those values are changing out from
 * under us. Because the code path in new_cpu_mstate() is
 * performance critical, we have not added a lock to it. Instead,
 * we have added a generation counter. Before beginning
 * modifications, the counter is set to 0. After modifications,
 * it is set to the old value plus one.
 *
 * get_cpu_mstate() will not consider the values of cpu_mstate
 * and cpu_mstate_start to be usable unless the value of
 * cpu_mstate_gen is both non-zero and unchanged, both before and
 * after reading the mstate information. Note that we must
 * protect against out-of-order loads around accesses to the
 * generation counter. Also, this is a best effort approach in
 * that we do not retry should the counter be found to have
 * changed.
 *
 * cpu_intracct[] is used to identify time spent in each CPU
 * mstate while handling interrupts. Such time should be reported
 * against system time, and so is subtracted out from its
 * corresponding cpu_acct[] time and added to
 * cpu_acct[CMS_SYSTEM].
 */

void
get_cpu_mstate(cpu_t *cpu, hrtime_t *times)
{
	int i;
	hrtime_t now, start;
	uint16_t gen;
	uint16_t state;
	hrtime_t intracct[NCMSTATES];

	/*
	 * Load all volatile state under the protection of membar.
	 * cpu_acct[cpu_mstate] must be loaded to avoid double counting
	 * of (now - cpu_mstate_start) by a change in CPU mstate that
	 * arrives after we make our last check of cpu_mstate_gen.
	 */

	now = gethrtime_unscaled();
	gen = cpu->cpu_mstate_gen;

	membar_consumer();	/* guarantee load ordering */
	start = cpu->cpu_mstate_start;
	state = cpu->cpu_mstate;
	for (i = 0; i < NCMSTATES; i++) {
		intracct[i] = cpu->cpu_intracct[i];
		times[i] = cpu->cpu_acct[i];
	}
	membar_consumer();	/* guarantee load ordering */

	if (gen != 0 && gen == cpu->cpu_mstate_gen && now > start)
		times[state] += now - start;

	for (i = 0; i < NCMSTATES; i++) {
		if (i == CMS_SYSTEM)
			continue;
		times[i] -= intracct[i];
		if (times[i] < 0) {
			intracct[i] += times[i];
			times[i] = 0;
		}
		times[CMS_SYSTEM] += intracct[i];
		scalehrtime(&times[i]);
	}
	scalehrtime(&times[CMS_SYSTEM]);
}

/*
 * Initialize NE1 EB IRQ and local IRQ for CPU0.
 */
void
plat_irq_init(void)
{
	int val;

	/*
	 * The following codes should be done by boot loader.
	 */

	/*
	 * Initialize SCU.
	 *	- Enable SCU
	 *	- Can NOT access aliased CPU interface registers.
	 *	- Can NOT access aliased CPU timer and WDT registers.
	 */
	val = readl(MPCORE_SCUREG_VADDR(CTRL));
	val |= MPCORE_SCU_CTRL_EN;
	val &= ~(MPCORE_SCU_CTRL_CPUIF_MASK|MPCORE_SCU_CTRL_PERI_MASK);
	writel(val, MPCORE_SCUREG_VADDR(CTRL));

	/*
	 * We should sync up with register changes before GIC
	 * initialization.
	 */
	SYNC_BARRIER();

	/* Initialize Interrupt distributor. */
	gic_dist_init();

	/* Initialize local IRQ for CPU0. */
	gic_cpuif_init();
}

/*
 * uint64_t
 * arm_gettick(void)
 *	Return 64bit system tick value.
 */
uint64_t
arm_gettick(void)
{
	ASSERT_STACK_ALIGNED();
	return (scucnt_gethrtimeunscaled());
}

/*
 * static boolean_t
 * is_nfsdump(void)
 *	Determine whether an NFS file is configured as dump device.
 */
static boolean_t
is_nfsdump(void)
{
	extern dumpdev_t	*dump_dumpdev;

	if (dump_dumpdev == NULL && dumpvp != NULL) {
		vnodeops_t	*vnop = vn_getops(dumpvp);

		if (vnop != NULL && strncmp(vnop->vnop_name, "nfs", 3) == 0) {
			return B_TRUE;
		}
	}

	return B_FALSE;
}
