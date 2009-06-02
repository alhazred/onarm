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

#pragma ident	"@(#)mp_startup.c	1.115	06/08/15 SMI"

#include <sys/types.h>
#include <sys/thread.h>
#include <sys/cpuvar.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/disp.h>
#include <sys/class.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/asm_linkage.h>
#include <sys/x_call.h>
#include <sys/systm.h>
#include <sys/var.h>
#include <sys/vtrace.h>
#include <vm/hat.h>
#include <vm/hat_armpt.h>
#include <vm/hat_machdep.h>
#include <vm/as.h>
#include <vm/seg_kmem.h>
#include <sys/kmem.h>
#include <sys/stack.h>
#include <sys/smp_impldefs.h>
#include <sys/machsystm.h>
#ifdef	TRAPTRACE
#include <sys/traptrace.h>
#endif	/* TRAPTRACE */
#include <sys/clock.h>
#include <sys/cpc_impl.h>
#include <sys/pg.h>
#include <sys/cmt.h>
#include <sys/dtrace.h>
#include <sys/archsystm.h>
#include <sys/fp.h>
#include <sys/reboot.h>
#include <sys/kdi.h>
#include <sys/memnode.h>
#include <sys/pci_cfgspace.h>
#include <sys/pte.h>
#include <sys/sysmacros.h>
#include <sys/mpcore.h>
#include <sys/prom_debug.h>
#include <asm/tlb.h>
#include <sys/mp_cpu.h>
#include <sys/mach_boot.h>
#include <sys/mach_reboot.h>
#include <sys/wdt.h>

struct cpu	*cpu[NCPU];			/* pointers to all CPUs */
cpu_core_t	cpu_core[NCPU];			/* cpu_core structures */
cpu_t		*cpu_boot;			/* struct cpu for boot CPU */

/* Space for struct cpu allocated by mlsetup(). */
caddr_t		cpu_buffer;

/*
 * Useful for disabling MP bring-up for an MP capable kernel
 * (a kernel that was built with MP defined)
 */
int use_mp = 1;

/*
 * To be set by mach_init() to indicate what CPUs are available on the system.
 */
cpuset_t mp_cpus = 0;

/*
 * This variable is used by the hat layer to decide whether or not
 * critical sections are needed to prevent race conditions.  For sun4m,
 * this variable is set once enough MP initialization has been done in
 * order to allow cross calls.
 */
int flushes_require_xcalls = 0;
cpuset_t	cpu_ready_set = 0;

static void	mp_startup(void);

/*
 * Init CPU info - get CPU type info for processor_info system call.
 */
void
init_cpu_info(struct cpu *cp)
{
	processor_info_t *pi = &cp->cpu_type_info;
	char buf[CPU_IDSTRLEN];

	/*
	 * Get clock-frequency property for the CPU.
	 */
	pi->pi_clock = cpu_freq;

	/*
	 * Current frequency in Hz.
	 */
	cp->cpu_curr_clock = cpu_freq_hz;

	/*
	 * Supported frequencies.
	 */
	cpu_set_supp_freqs(cp, NULL);

	(void) strcpy(pi->pi_processor_type, "ARM");
	if (fpu_exists)
		(void) strcpy(pi->pi_fputypes, "ARM VFP");

	(void) cpuid_getidstr(cp, buf, sizeof (buf));

	cp->cpu_idstr = kmem_alloc(strlen(buf) + 1, KM_SLEEP);
	(void) strcpy(cp->cpu_idstr, buf);

	cmn_err(CE_CONT, "?cpu%d: %s\n", cp->cpu_id, cp->cpu_idstr);

	(void) cpuid_getbrandstr(cp, buf, sizeof (buf));
	cp->cpu_brandstr = kmem_alloc(strlen(buf) + 1, KM_SLEEP);
	(void) strcpy(cp->cpu_brandstr, buf);

	cmn_err(CE_CONT, "?cpu%d: %s\n", cp->cpu_id, cp->cpu_brandstr);
}

/*
 * Multiprocessor initialization.
 *
 * Allocate and initialize the cpu structure, TRAPTRACE buffer, and the
 * startup and idle threads for the specified CPU.
 */
static void
mp_startup_init(int cpun)
{
	struct cpu *cp;
	kthread_id_t tp;
	caddr_t	sp;
	proc_t *procp;
	extern void idle();
	extern void fiq_mp_startup(struct cpu *cp, int cpun);

#ifdef TRAPTRACE
	trap_trace_ctl_t *ttc = &trap_trace_ctl[cpun];
#endif

	ASSERT(cpun < NCPU && cpu[cpun] == NULL);

	/* Allocate buffer for struct cpu. */
	MP_CPU_ALLOC(cp);

	procp = curthread->t_procp;
	cp->cpu_self = cp;

	mutex_enter(&cpu_lock);
	/*
	 * Initialize the dispatcher first.
	 */
	disp_cpu_init(cp);
	mutex_exit(&cpu_lock);

	cpu_vm_data_init(cp);

	/*
	 * Allocate and initialize the startup thread for this CPU.
	 * Interrupt and process switch stacks get allocated later
	 * when the CPU starts running.
	 */
	tp = thread_create(NULL, 0, NULL, NULL, 0, procp,
	    TS_STOPPED, maxclsyspri);

	/*
	 * Set state to TS_ONPROC since this thread will start running
	 * as soon as the CPU comes online.
	 *
	 * All the other fields of the thread structure are setup by
	 * thread_create().
	 */
	THREAD_ONPROC(tp, cp);
	tp->t_preempt = 1;
	tp->t_bound_cpu = cp;
	tp->t_affinitycnt = 1;
	tp->t_cpu = cp;
	tp->t_disp_queue = cp->cpu_disp;

	/*
	 * Setup thread to start in mp_startup.
	 */
	sp = tp->t_stk;
	tp->t_pc = (uintptr_t)mp_startup;
	tp->t_sp = (uintptr_t)(sp - MINFRAME);

	cp->cpu_id = cpun;
	cp->cpu_thread = tp;
	cp->cpu_lwp = NULL;
	cp->cpu_dispthread = tp;
	cp->cpu_dispatch_pri = DISP_PRIO(tp);

	/*
	 * Allocate stack for FIQ mode.
	 */
	fiq_mp_startup(cp, cpun);

	/*
	 * cpu_base_spl must be set explicitly here to prevent any blocking
	 * operations in mp_startup from causing the spl of the cpu to drop
	 * to 0 (allowing device interrupts before we're ready) in resume().
	 * cpu_base_spl MUST remain at LOCK_LEVEL until the cpu is CPU_READY.
	 * As an extra bit of security on DEBUG kernels, this is enforced with
	 * an assertion in mp_startup() -- before cpu_base_spl is set to its
	 * proper value.
	 */
	cp->cpu_base_spl = ipltospl(LOCK_LEVEL);

	/*
	 * Now, initialize per-CPU idle thread for this CPU.
	 */
	tp = thread_create(NULL, PAGESIZE, idle, NULL, 0, procp, TS_ONPROC, -1);

	cp->cpu_idle_thread = tp;

	tp->t_preempt = 1;
	tp->t_bound_cpu = cp;
	tp->t_affinitycnt = 1;
	tp->t_cpu = cp;
	tp->t_disp_queue = cp->cpu_disp;

	/*
	 * Bootstrap the CPU's PG data
	 */
	pg_cpu_bootstrap(cp);

	/*
	 * Perform CPC initialization on the new CPU.
	 */
	kcpc_hw_startup_cpu(cp);

	/* Should remove all entries for the current process/thread here */

#ifdef TRAPTRACE
	/*
	 * If this is a TRAPTRACE kernel, allocate TRAPTRACE buffers for this
	 * CPU.
	 */
	ttc->ttc_first = (uintptr_t)kmem_zalloc(trap_trace_bufsize, KM_SLEEP);
	ttc->ttc_next = ttc->ttc_first;
	ttc->ttc_limit = ttc->ttc_first + trap_trace_bufsize;
#endif

	/*
	 * Record that we have another CPU.
	 */
	mutex_enter(&cpu_lock);
	/*
	 * Initialize the interrupt threads for this CPU
	 */
	cpu_intr_mp_init(cp);
	/*
	 * Add CPU to list of available CPUs.  It'll be on the active list
	 * after mp_startup().
	 */
	cpu_add_unit(cp);
	mutex_exit(&cpu_lock);

	/* Setup virtual addresses for processor-local space for each CPU. */
	hat_plat_cpu_init(cp);
}

static cpuset_t procset = 0;

/*ARGSUSED*/
void
start_other_cpus(int cprboot)
{
	unsigned int who;
	int skipped = 0;
	int cpuid = HARD_PROCESSOR_ID();
	int delays = 0;
	int started_cpu;


	/*
	 * Set current cpu in procset and cpu_ready_set.
	 */
	CPUSET_ADD(procset, cpuid);
	CPUSET_ADD(cpu_ready_set, cpuid);

	/*
	 * Initialize our own cpu_info.
	 */
	init_cpu_info(CPU);

	/*
	 * if only 1 cpu or not using MP, skip the rest of this
	 */
	if (CPUSET_ISEQUAL(mp_cpus, cpu_ready_set) || use_mp == 0) {
		if (use_mp == 0)
			cmn_err(CE_CONT, "?***** Not in MP mode\n");
		goto done;
	}

	/* Initialize HAT layer for secondary CPU startup. */
	hat_plat_mpstart_init();

	/*
	 * perform such initialization as is needed
	 * to be able to take CPUs on- and off-line.
	 */
	cpu_pause_init();

	xc_init();		/* initialize processor crosscalls */

	/*
	 * Prepare for booting secondary CPUs.
	 */
	PLAT_SECONDARY_CPU_ENTRY_INIT();

	flushes_require_xcalls = 1;

	ASSERT(CPU_IN_SET(procset, cpuid));
	ASSERT(CPU_IN_SET(cpu_ready_set, cpuid));

	affinity_set(CPU_CURRENT);

	for (who = 0; who < NCPU; who++) {
		if (who == cpuid)
			continue;

		delays = 0;

		if (!CPU_IN_SET(mp_cpus, who))
			continue;

		if (ncpus >= max_ncpus) {
			skipped = who;
			continue;
		}

		mp_startup_init(who);
		started_cpu = 1;

		/*
		 * Writeback all data cache lines in order to make all changes
		 * visible to other CPUs.
		 */
		DCACHE_CLEAN_ALL();
		SYNC_BARRIER();
		mach_cpu_start(who);

		while (!CPU_IN_SET(procset, who)) {
			delay(1);
			if (++delays > (20 * hz)) {

				cmn_err(CE_WARN,
				    "cpu%d failed to start", who);

				mutex_enter(&cpu_lock);
				cpu[who]->cpu_flags = 0;
				cpu_vm_data_destroy(cpu[who]);
				cpu_del_unit(who);
				mutex_exit(&cpu_lock);

				started_cpu = 0;
				break;
			}
		}
		if (!started_cpu)
			continue;
	}

	/*
	 * Wait for all CPUs that booted (have presence in procset)
	 * to come online (have presence in cpu_ready_set).  Note
	 * that the start CPU already satisfies both of these, so no
	 * special case is needed.
	 */
	for (who = 0; who < NCPU; who++) {
		if (!CPU_IN_SET(procset, who))
			continue;

		while (!CPU_IN_SET(cpu_ready_set, who))
			delay(1);
	}

	if (skipped) {
		cmn_err(CE_NOTE,
		    "System detected %d CPU(s), but "
		    "only %d CPU(s) were enabled during boot.",
		    skipped + 1, ncpus);
		cmn_err(CE_NOTE,
		    "Use \"boot-ncpus\" parameter to enable more CPU(s). "
		    "See eeprom(1M).");
	}

	/* Finalize secondary CPU startup. */
	hat_plat_mpstart_fini();
	affinity_clear();

done:

	/*
	 * Add a software interrupt for shutting down by external
	 * request.
	 */
	rbt_add_softintr();
}

/*
 * Dummy functions - no ARM platforms support dynamic cpu allocation.
 */
/*ARGSUSED*/
int
mp_cpu_configure(int cpuid)
{
	return (ENOTSUP);		/* not supported */
}

/*ARGSUSED*/
int
mp_cpu_unconfigure(int cpuid)
{
	return (ENOTSUP);		/* not supported */
}

#pragma weak	plat_mp_startup

/*
 * Startup function for 'other' CPUs (besides boot cpu).
 * Called from secondary_start.
 *
 * WARNING: until CPU_READY is set, mp_startup and routines called by
 * mp_startup should not call routines (e.g. kmem_free) that could call
 * hat_unload which requires CPU_READY to be set.
 */
void
mp_startup(void)
{
	struct cpu	*cp = curthread->t_cpu;
	uint32_t	irq;
	extern void	fiq_mp_init(struct cpu *cp);
	extern void	plat_mp_startup(void);

	MP_CPU_BOOTSTRAP(cp);

	/* Install FIQ mode stack. */
	fiq_mp_init(cp);

	/*
	 * Initialize CPU Interface for this CPU.
	 */
	gic_cpuif_init();

	/*
	 * Set priority level for local irqs.
	 */
	(*addspl)(IRQ_IPI_LO, XC_LO_PIL, 0, 0);
	(*addspl)(IRQ_IPI_HI, XC_HI_PIL, 0, 0);
	(*addspl)(IRQ_IPI_CPUPOKE, XC_CPUPOKE_PIL, 0, 0);
	(*addspl)(IRQ_IPI_CBE, CBE_HIGH_PIL, 0, 0);
	(*addspl)(IRQ_LOCALTIMER, CBE_HIGH_PIL, 0, 0);

	wdt_mp_init();

	/*
	 * Ack for IPI that woke me up.
	 */
	irq = readl(MPCORE_CPUIF_VADDR(INTACK)) & 0x3ff;
	if (irq != 0x3ff) {
		gic_ack_irq(irq);
	}

	(void) cpuid_pass1(cp);

	/* Initialize VFP */
	fpu_probe();

	if (&plat_mp_startup != NULL) {
		/* Call platform specific startup code. */
		plat_mp_startup();
	}

	/*
	 * Enable interrupts with spl set to LOCK_LEVEL. LOCK_LEVEL is the
	 * highest level at which a routine is permitted to block on
	 * an adaptive mutex (allows for cpu poke interrupt in case
	 * the cpu is blocked on a mutex and halts). Although cross call
	 * for TLB maintenance is enabled, this CPU should never become
	 * a target of cross call because the kernel hat is still offline.
	 */
	(void) splx(ipltospl(LOCK_LEVEL));
	ENABLE_INTR();

	cpuid_pass2(cp);
	cpuid_pass3(cp);
	(void) cpuid_pass4(cp);

	init_cpu_info(cp);

	CPUSET_ATOMIC_ADD(procset, cp->cpu_id);

	mutex_enter(&cpu_lock);

	/*
	 * Processor group initialization for this CPU is dependent on the
	 * cpuid probing, which must be done in the context of the current
	 * CPU.
	 */
	pghw_physid_create(cp);
	pg_cpu_init(cp);
	pg_cmt_cpu_startup(cp);

	cp->cpu_flags |= CPU_RUNNING | CPU_READY | CPU_ENABLE | CPU_EXISTS;
	cpu_add_active(cp);

	if (dtrace_cpu_init != NULL) {
		(*dtrace_cpu_init)(cp->cpu_id);
	}

	mutex_exit(&cpu_lock);

	/*
	 * Enable preemption here so that contention for any locks acquired
	 * later in mp_startup may be preempted if the thread owning those
	 * locks is continously executing on other CPUs (for example, this
	 * CPU must be preemptible to allow other CPUs to pause it during their
	 * startup phases).  It's safe to enable preemption here because the
	 * CPU state is pretty-much fully constructed.
	 */
	curthread->t_preempt = 0;

	add_cpunode2devtree(cp);

	/* The base spl should still be at LOCK LEVEL here */
	ASSERT(cp->cpu_base_spl == ipltospl(LOCK_LEVEL));
	set_base_spl();		/* Restore the spl to its proper value */

	(void) spl0();				/* enable interrupts */

	/* Make the kernel hat online for this CPU. */
	hat_cpu_online(cp);

	/*
	 * Setting the bit in cpu_ready_set must be the last operation in
	 * processor initialization; the boot CPU will continue to boot once
	 * it sees this bit set for all active CPUs.
	 */
	CPUSET_ATOMIC_ADD(cpu_ready_set, cp->cpu_id);

	/*
	 * Because mp_startup() gets fired off after init() starts, we
	 * can't use the '?' trick to do 'boot -v' printing - so we
	 * always direct the 'cpu .. online' messages to the log.
	 */
	cmn_err(CE_CONT, "!cpu%d initialization complete - online\n",
	    cp->cpu_id);

	/*
	 * Now we are done with the startup thread, so free it up.
	 */
	thread_exit();
	panic("mp_startup: cannot return");
	/*NOTREACHED*/
}


/*
 * Start CPU on user request.
 */
/* ARGSUSED */
int
mp_cpu_start(struct cpu *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));
	return (0);
}

/*
 * Stop CPU on user request.
 */
/* ARGSUSED */
int
mp_cpu_stop(struct cpu *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));
	return (0);
}

/*
 * Power on CPU.
 */
/* ARGSUSED */
int
mp_cpu_poweron(struct cpu *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));
	return (ENOTSUP);		/* not supported */
}

/*
 * Power off CPU.
 */
/* ARGSUSED */
int
mp_cpu_poweroff(struct cpu *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));
	return (ENOTSUP);		/* not supported */
}


/*
 * Take the specified CPU out of participation in interrupts.
 */
int
cpu_disable_intr(struct cpu *cp)
{
	wdt_stop(cp->cpu_id);

	ASSERT(MUTEX_HELD(&cpu_lock));
	if (gic_disable_intr(cp->cpu_id) != DDI_SUCCESS) {
		return (EBUSY);
	}

	cp->cpu_flags &= ~CPU_ENABLE;
	return (0);
}

/*
 * Allow the specified CPU to participate in interrupts.
 */
void
cpu_enable_intr(struct cpu *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));
	cp->cpu_flags |= CPU_ENABLE;
	gic_enable_intr(cp->cpu_id);

	wdt_start(cp->cpu_id);
}

void
mp_cpu_faulted_enter(struct cpu *cp)
{
}

void
mp_cpu_faulted_exit(struct cpu *cp)
{
}
