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

#ident	"@(#)arm/os/cpc_subr.c"

/*
 * ARM-specific routines used by the CPU Performance counter driver.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/atomic.h>
#include <sys/regset.h>
#include <sys/privregs.h>
#include <sys/cpuvar.h>
#include <sys/machcpuvar.h>
#include <sys/archsystm.h>
#include <sys/cpc_pcbe.h>
#include <sys/cpc_impl.h>
#include <sys/x_call.h>
#include <sys/cmn_err.h>
#include <sys/spl.h>
#include <sys/controlregs.h>

#define PMNOVF_PIL 13

/*
 * Prepare for CPC interrupts and install an idle thread CPC context.
 */
void
kcpc_hw_startup_cpu(cpu_t *cp)
{
#ifndef CPCSYS_DISABLE
	kthread_t       *t = cp->cpu_idle_thread;
#ifndef MARINE4_PMUIRQ_ERRATA
	int		pmn_irq;
	static const int pmn_irq_table[] = {
		IRQ_PMU_CPU0,
		IRQ_PMU_CPU1,
		IRQ_PMU_CPU2,
		IRQ_PMU_CPU3
	};

	if (cp->cpu_id < 0 || 
		cp->cpu_id >= (sizeof (pmn_irq_table) / sizeof (int))) {
		return;
	}
	pmn_irq = pmn_irq_table[cp->cpu_id];

	(void) add_avintr(NULL, PMNOVF_PIL, (avfunc) kcpc_hw_overflow_intr,
			"pmn_intr", pmn_irq, 0, NULL, NULL, NULL);
#endif /* !MARINE4_PMUIRQ_ERRATA */
	mutex_init(&cp->cpu_cpc_ctxlock, "cpu_cpc_ctxlock", MUTEX_DEFAULT, 0);

#ifdef KCPC_COUNTS_INCLUDE_IDLE
	return;
#else /* KCPC_COUNTS_INCLUDE_IDLE */
	installctx(t, cp, kcpc_idle_save, kcpc_idle_restore, NULL, NULL,
		NULL, NULL);
#endif /* KCPC_COUNTS_INCLUDE_IDLE */
#endif /* !CPCSYS_DISABLE */
}

/*
 * Called on the boot CPU during startup.
 */
void
kcpc_hw_init(void)
{
#ifndef CPCSYS_DISABLE
	/*
	 * Make sure the boot CPU gets set up.
	 */
	kcpc_hw_startup_cpu(CPU_GLOBAL);
#endif /* !CPCSYS_DISABLE */
}

/*
 * Examine the processor and load an appropriate PCBE.
 */
int
kcpc_hw_load_pcbe(void)
{
#ifndef CPCSYS_DISABLE
	int id;

	id = cpuid_getidcode(CPU);
	return (kcpc_pcbe_tryload((const char*)cpuid_getimplstr(id), 
				ARM_IDCODE_PARTNUM(id), ARM_IDCODE_VARIANT(id), 
				ARM_IDCODE_REVISION(id)));

#else /* !CPCSYS_DISABLE */
	return (-1);
#endif /* !CPCSYS_DISABLE */
}

static int
kcpc_remotestop_func(void)
{
#ifndef CPCSYS_DISABLE
	ASSERT(CPU->cpu_cpc_ctx != NULL);
	pcbe_ops->pcbe_allstop();
	atomic_or_uint(&CPU->cpu_cpc_ctx->kc_flags, KCPC_CTX_INVALID_STOPPED);

	return (0);
#else /* !CPCSYS_DISABLE */
	ASSERT(0);	/* should not be called */
	return(0);
#endif /* !CPCSYS_DISABLE */
}

/*
 * Ensure the counters are stopped on the given processor.
 *
 * Callers must ensure kernel preemption is disabled.
 */
void
kcpc_remote_stop(cpu_t *cp)
{
#ifndef CPCSYS_DISABLE
	cpuset_t set;

	CPUSET_ZERO(set);

	CPUSET_ADD(set, cp->cpu_id);

	xc_sync(0, 0, 0, X_CALL_HIPRI, set, (xc_func_t)kcpc_remotestop_func);
#else /* !CPCSYS_DISABLE */
	ASSERT(0);	/* should not be called */
#endif /* !CPCSYS_DISABLE */
}

/*
 * Called by the generic framework to check if it's OK to bind a set to a CPU.
 */
int
kcpc_hw_cpu_hook(processorid_t cpuid, ulong_t *kcpc_cpumap)
{
	return (0);
}

/*
 * Called by the generic framework to check if it's OK to bind a set to an LWP.
 */
int
kcpc_hw_lwp_hook(void)
{
	return (0);
}
