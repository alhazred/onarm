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

#pragma ident	"@(#)mp_machdep.c	1.69	06/08/15 SMI"

#include <sys/smp_impldefs.h>
#include <sys/pit.h>
#include <sys/cmn_err.h>
#include <sys/strlog.h>
#include <sys/clock.h>
#include <sys/debug.h>
#include <sys/cpupart.h>
#include <sys/cpuvar.h>
#include <sys/pghw.h>
#include <sys/disp.h>
#include <sys/cpu.h>
#include <sys/archsystm.h>
#include <sys/mach_intr.h>
#include <sys/platform.h>
#include <sys/machsystm.h>
#include <sys/time.h>
#include <sys/mpcore.h>
#include <asm/cpufunc.h>
#include <sys/gic.h>
#include <sys/cyclic.h>
#include <sys/cyclic_impl.h>
#include <sys/dtrace.h>
#include <sys/prom_debug.h>
#include <sys/wdt.h>
#include <sys/sdt.h>

#define	MP_CPUS_INIT()					\
	{						\
		int	__i;				\
							\
		for (__i = 0; __i < max_ncpus; __i++) {	\
			CPUSET_ADD(mp_cpus, __i);	\
		}					\
	}


/*
 *	Local function prototypes
 */
static int mach_softlvl_to_vect(int ipl);
static void cpu_halt(void);
static void cpu_wakeup(cpu_t *, int);

/*
 *	External reference functions
 */
extern void return_instr();
extern void pc_gethrestime(timestruc_t *);

extern int gic_addspl(int irq, int ipl, int min_ipl, int max_ipl);
extern int gic_delspl(int irq, int ipl, int min_ipl, int max_ipl);
extern void secondary_start(void);
extern timestruc_t (*todgetf)(void);
extern void (*todsetf)(timestruc_t);

/*
 *	PSM functions initialization
 */
int (*addspl)(int, int, int, int) = (int (*)(int, int, int, int))return_instr;
int (*delspl)(int, int, int, int) = (int (*)(int, int, int, int))return_instr;
void (*kdisetsoftint)(int, struct av_softinfo *)=
	(void (*)(int, struct av_softinfo *))return_instr;
void (*setsoftint)(int, struct av_softinfo *)=
	(void (*)(int, struct av_softinfo *))return_instr;
int (*slvltovect)(int)		= (int (*)(int))return_instr;
void (*gethrestimef)(timestruc_t *) = pc_gethrestime;

typedef struct _gtol_irq {
	unsigned short irq;
	unsigned short cpuid;
} gtol_irq_t;

/*
 * Defines for the idle_state_transition DTrace probe
 *
 * The probe fires when the CPU undergoes an idle state change (e.g. halting)
 * The agument passed is the state to which the CPU is transitioning.
 *
 * The states are defined here.
 */
#define	IDLE_STATE_NORMAL 0
#define	IDLE_STATE_HALTED 1

#if	ARMPF_L220_EXIST == 1
#define	PGHW_IS_CACHE(hw)	((hw) == PGHW_CACHE)
#else	/* ARMPF_L220_EXIST != 1 */
#define	PGHW_IS_CACHE(hw)	(0)
#endif	/* ARMPF_L220_EXIST == 1 */

#ifndef	CMT_SCHED_DISABLE

/*
 * int
 * pg_plat_hw_shared(cpu_t *cp, pghw_type_t hw)
 *	Determine whether the specified hardware component is shared by a group
 *	of processors.
 *
 * Remarks:
 *	This function assumes that only one MPCore chip exists.
 */
/*ARGSUSED*/
int
pg_plat_hw_shared(cpu_t *cp, pghw_type_t hw)
{
	if (PGHW_IS_CACHE(hw)) {
		return ARMPF_L220_EXIST;
	}
	if (hw == PGHW_IPIPE || hw == PGHW_CHIP) {
		return 1;
	}

	return (0);
}

/*
 * int
 * pg_plat_cpus_share(cpu_t *cpu_a, cpu_t *cpu_b, pghw_type_t hw)
 *	Compare two CPUs and see if they have a pghw_type_t sharing
 *	relationship. If pghw_type_t is an unsupported hardware type,
 *	then return -1
 */
int
pg_plat_cpus_share(cpu_t *cpu_a, cpu_t *cpu_b, pghw_type_t hw)
{
	id_t	pgp_a, pgp_b;

	pgp_a = pg_plat_hw_instance_id(cpu_a, hw);
	pgp_b = pg_plat_hw_instance_id(cpu_b, hw);

	if (pgp_a == -1 || pgp_b == -1) {
		return (-1);
	}

	return (pgp_a == pgp_b);
}

/*
 * int
 * pg_plat_hw_level(pghw_type_t hw)
 *	Return a sequential level identifier for the specified
 *	hardware sharing relationship.
 */
int
pg_plat_hw_level(pghw_type_t hw)
{
	int i;
	static pghw_type_t hw_hier[] = {
		PGHW_IPIPE,
#if	ARMPF_L220_EXIST == 1
		PGHW_CACHE,
#endif	/* ARMPF_L220_EXIST == 1 */
		PGHW_CHIP,
		PGHW_NUM_COMPONENTS
	};

	for (i = 0; hw_hier[i] != PGHW_NUM_COMPONENTS; i++) {
		if (hw_hier[i] == hw) {
			return i;
		}
	}

	return -1;
}

/*
 * int
 * pg_plat_cmt_load_bal_hw(pghw_type_t hw)
 *	Return 1 if CMT load balancing policies should be implemented
 *	across instances of the specified hardware sharing relationship.
 */
int
pg_plat_cmt_load_bal_hw(pghw_type_t hw)
{
	if (PGHW_IS_CACHE(hw)) {
		return 1;
	}

	return (hw == PGHW_CHIP || PGHW_IPIPE) ? 1 : 0;
}

/*
 * int
 * pg_plat_cmt_affinity_hw(pghw_type_t hw)
 *	Return 1 if thread affinity polices should be implemented
 *	for instances of the specifed hardware sharing relationship.
 */
int
pg_plat_cmt_affinity_hw(pghw_type_t hw)
{
	int	ret;

	return (PGHW_IS_CACHE(hw)) ? 1 : 0;
}

#endif	/* !CMT_SCHED_DISABLE */

/*
 * id_t
 * pg_plat_get_core_id(cpu_t *cpu)
 *	Return cpu core ID of the given cpu.
 */
id_t
pg_plat_get_core_id(cpu_t *cpu)
{
	return ARM_CPUID_ID(cpu->cpu_m.mcpu_cpuid);
}

/*
 * This variable is required to advertise chipid_t via CTF data.
 */
static chipid_t	mpcore_chipid = 0;

/*
 * id_t
 * pg_plat_hw_instance_id(cpu_t *cpu, pghw_type_t hw)
 *	Return a physical instance identifier for known hardware sharing
 *	relationships
 *
 * Remarks:
 *	This function assumes that only one MPCore chip exists.
 */
id_t
pg_plat_hw_instance_id(cpu_t *cpu, pghw_type_t hw)
{
	if (PGHW_IS_CACHE(hw)) {
		/* This function assumes that only one L220 cache exists. */
		return 0;
	}
	if (hw == PGHW_CHIP) {
		/* Return zero as instance ID for only one MPCore chip. */
		return mpcore_chipid;
	}
	if (hw == PGHW_IPIPE) {
		return ARM_CPUID_ID(cpu->cpu_m.mcpu_cpuid);
	}

	return -1;
}

#ifdef	CMT_SCHED_DISABLE
/*
 * Return non-zero if thread can migrate between "from" and "to"
 * without a performance penalty
 */
int
pg_cmt_can_migrate(cpu_t *from, cpu_t *to)
{
#if	ARMPF_L220_EXIST == 1
	/* This function assumes that only one L220 cache exists. */
	return (1);
#else
	return (0);
#endif
}
#endif	/* CMT_SCHED_DISABLE */

/*
 * void
 * cmp_set_nosteal_interval(void)
 *	Initialize nosteal_nsec, which is used by process dispatcher.
 */
void
cmp_set_nosteal_interval(void)
{
	/* Set the nosteal interval (used by disp_getbest()) to 100us */
	nosteal_nsec = 100000UL;
}

/*
 * Halt the present CPU until awoken via an interrupt
 */
static void
cpu_halt(void)
{
	cpu_t		*cpup = CPU;
	processorid_t	cpun = cpup->cpu_id;
	cpupart_t	*cp = cpup->cpu_part;
	int		hset_update = 1;

	/*
	 * If this CPU is online, and there's multiple CPUs
	 * in the system, then we should notate our halting
	 * by adding ourselves to the partition's halted CPU
	 * bitmap. This allows other CPUs to find/awaken us when
	 * work becomes available.
	 */
	if (cpup->cpu_flags & CPU_OFFLINE || ncpus == 1)
		hset_update = 0;

	/*
	 * Add ourselves to the partition's halted CPUs bitmask
	 * and set our HALTED flag, if necessary.
	 *
	 * When a thread becomes runnable, it is placed on the queue
	 * and then the halted cpuset is checked to determine who
	 * (if anyone) should be awoken. We therefore need to first
	 * add ourselves to the halted cpuset, and then check if there
	 * is any work available.
	 */
	if (hset_update) {
		cpup->cpu_disp_flags |= CPU_DISP_HALTED;
		MEMORY_BARRIER();
		CPUSET_ATOMIC_ADD(cp->cp_mach->mc_haltset, cpun);
	}

	/*
	 * Check to make sure there's really nothing to do.
	 * Work destined for this CPU may become available after
	 * this check. We'll be notified through the clearing of our
	 * bit in the halted CPU bitmask, and a poke.
	 */
	if (disp_anywork()) {
		if (hset_update) {
			cpup->cpu_disp_flags &= ~CPU_DISP_HALTED;
			MEMORY_BARRIER();
			CPUSET_ATOMIC_DEL(cp->cp_mach->mc_haltset, cpun);
		}
		return;
	}

	/*
	 * We're on our way to being halted.
	 *
	 * Disable interrupts now, so that we'll awaken immediately
	 * after halting if someone tries to poke us between now and
	 * the time we actually halt.
	 *
	 * We check for the presence of our bit after disabling interrupts.
	 * If it's cleared, we'll return. If the bit is cleared after
	 * we check then the poke will pop us out of the halted state.
	 *
	 * This means that the ordering of the poke and the clearing
	 * of the bit by cpu_wakeup is important.
	 * cpu_wakeup() must clear, then poke.
	 * cpu_halt() must disable interrupts, then check for the bit.
	 */
	DISABLE_IRQ();

	if (hset_update && !CPU_IN_SET(cp->cp_mach->mc_haltset, cpun)) {
		cpup->cpu_disp_flags &= ~CPU_DISP_HALTED;
		MEMORY_BARRIER();
		ENABLE_IRQ();
		return;
	}

	/*
	 * The check for anything locally runnable is here for performance
	 * and isn't needed for correctness. disp_nrunnable ought to be
	 * in our cache still, so it's inexpensive to check, and if there
	 * is anything runnable we won't have to wait for the poke.
	 */
	if (cpup->cpu_disp->disp_nrunnable != 0) {
		if (hset_update) {
			cpup->cpu_disp_flags &= ~CPU_DISP_HALTED;
			MEMORY_BARRIER();
			CPUSET_ATOMIC_DEL(cp->cp_mach->mc_haltset, cpun);
		}
		ENABLE_IRQ();
		return;
	}

	DTRACE_PROBE1(idle__state__transition, uint_t, IDLE_STATE_HALTED);

	/*
	 * Call the halt sequence:
	 *	SYNC_BARRIER()
	 *	WFI
	 */
	SYNC_BARRIER();
	ARM_WFI();
	/*
	 * enable interrupts
	 */
	ENABLE_IRQ();

	DTRACE_PROBE1(idle__state__transition, uint_t, IDLE_STATE_NORMAL);

	/*
	 * We're no longer halted
	 */
	if (hset_update) {
		cpup->cpu_disp_flags &= ~CPU_DISP_HALTED;
		MEMORY_BARRIER();
		CPUSET_ATOMIC_DEL(cp->cp_mach->mc_haltset, cpun);
	}
}


/*
 * If "cpu" is halted, then wake it up clearing its halted bit in advance.
 * Otherwise, see if other CPUs in the cpu partition are halted and need to
 * be woken up so that they can steal the thread we placed on this CPU.
 * This function is only used on MP systems.
 */
static void
cpu_wakeup(cpu_t *cpu, int bound)
{
	uint_t		cpu_found;
	int		result;
	cpupart_t	*cp;

	cp = cpu->cpu_part;
	if (CPU_IN_SET(cp->cp_mach->mc_haltset, cpu->cpu_id)) {
		/*
		 * Clear the halted bit for that CPU since it will be
		 * poked in a moment.
		 */
		CPUSET_ATOMIC_DEL(cp->cp_mach->mc_haltset, cpu->cpu_id);
		/*
		 * We may find the current CPU present in the halted cpuset
		 * if we're in the context of an interrupt that occurred
		 * before we had a chance to clear our bit in cpu_halt().
		 * Poking ourself is obviously unnecessary, since if
		 * we're here, we're not halted.
		 */
		if (cpu != CPU_GLOBAL)
			poke_cpu(cpu->cpu_id);
		return;
	} else {
		/*
		 * This cpu isn't halted, but it's idle or undergoing a
		 * context switch. No need to awaken anyone else.
		 */
		if (cpu->cpu_thread == cpu->cpu_idle_thread ||
		    cpu->cpu_disp_flags & CPU_DISP_DONTSTEAL)
			return;
	}

	/*
	 * No need to wake up other CPUs if the thread we just enqueued
	 * is bound.
	 */
	if (bound)
		return;


	/*
	 * See if there's any other halted CPUs. If there are, then
	 * select one, and awaken it.
	 * It's possible that after we find a CPU, somebody else
	 * will awaken it before we get the chance.
	 * In that case, look again.
	 */
	do {
		CPUSET_FIND(cp->cp_mach->mc_haltset, cpu_found);
		if (cpu_found == CPUSET_NOTINSET)
			return;

		ASSERT(cpu_found >= 0 && cpu_found < NCPU);
		CPUSET_ATOMIC_XDEL(cp->cp_mach->mc_haltset, cpu_found, result);
	} while (result < 0);

	if (cpu_found != CPU->cpu_id)
		poke_cpu(cpu_found);
}

void
mach_init()
{
	/* register the interrupt setup code */
	slvltovect = mach_softlvl_to_vect;
	addspl = gic_addspl;
	delspl = gic_delspl;

	/*
	 * Initialize the dispatcher's function hooks
	 * to enable CPU halting when idle
	 */
	/*
	 * Set the dispatcher hook to enable cpu "wake up"
	 * when a thread becomes runnable.
	 */
#ifdef CPU_HALT_ENABLE
	idle_cpu = cpu_halt;
	disp_enq_thread = cpu_wakeup;
#endif /* CPU_HALT_ENABLE */

	MP_CPUS_INIT();

	/*
	 *  Resister IPI that x_call uses.
	 */
	(void) add_avintr((void *)NULL, XC_HI_PIL, (avfunc)xc_serv_hipri, "xc_hi_intr",
			  IRQ_IPI_HI, (caddr_t)X_CALL_HIPRI, NULL, NULL, NULL);
	(void) add_avintr((void *)NULL, XC_LO_PIL, (avfunc)xc_serv_lopri, "xc_lo_intr",
			  IRQ_IPI_LO, (caddr_t)X_CALL_LOPRI, NULL, NULL, NULL);

	wdt_init();

	(*addspl)(IRQ_IPI_CPUPOKE, XC_CPUPOKE_PIL, 0, 0);
}

/*
 * static void
 * mach_set_softintr(int ipl, struct av_softinfo *pending)
 *	Trigger software interrupt.
 *
 *	mach_set_softintr() sets softint pending bit for the current CPU,
 *	and then send IPI to kick the softint handler.
 */
static void
mach_set_softintr(int ipl, struct av_softinfo *pending)
{
	cpu_t		*curcpu;
	uint32_t	x;

	x = DISABLE_IRQ_SAVE();
	av_set_softint_pending(ipl, pending);
	curcpu = CPU;
	if (!CPU_ON_INTR(curcpu) && curthread->t_intr == NULL) {
		poke_cpu(curcpu->cpu_id);
	}
	RESTORE_INTR(x);
}

static int
mach_softlvl_to_vect(register int ipl)
{
	setsoftint = mach_set_softintr;
	kdisetsoftint = kdi_av_set_softint_pending;
	return -1;
}

void
mach_cpu_start(register int cpun)
{
	poke_cpu(cpun);
}

void
mach_cpu_pause(volatile char *safe)
{
	/*
	 * This cpu is now safe.
	 */
	*safe = PAUSE_WAIT;
	SYNC_BARRIER();		/* make sure stores are flushed */

	/*
	 * Now we wait.  When we are allowed to continue, safe
	 * will be set to PAUSE_IDLE.
	 */
	while (*safe != PAUSE_IDLE) {
		SMT_PAUSE();
	}
}

processorid_t
irq_to_bound_cpuid(int irq)
{
#ifndef MARINE4_PMUIRQ_ERRATA
	int i, entry_num;
	static const gtol_irq_t gtol_irqs[] = {
		{IRQ_PMU_CPU0, 0},
		{IRQ_PMU_CPU1, 1},
		{IRQ_PMU_CPU2, 2},
		{IRQ_PMU_CPU3, 3}
	};
	
	entry_num = sizeof (gtol_irqs) / sizeof (gtol_irq_t);
	for (i = 0; i < entry_num; i++) {
		if (irq == gtol_irqs[i].irq) {
			return (gtol_irqs[i].cpuid);
		}
	}
#endif /* !MARINE4_PMUIRQ_ERRATA */
	return (-1);
}
