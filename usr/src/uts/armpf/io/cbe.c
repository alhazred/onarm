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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2006-2008 NEC Corporation
 */

#pragma ident	"@(#)cbe.c	1.14	06/02/03 SMI"

#include <sys/systm.h>
#include <sys/cyclic.h>
#include <sys/cyclic_impl.h>
#include <sys/spl.h>
#include <sys/x_call.h>
#include <sys/kmem.h>
#include <sys/machsystm.h>
#include <sys/smp_impldefs.h>
#include <sys/atomic.h>
#include <sys/clock.h>
#include <sys/ddi_impldefs.h>
#include <sys/ddi_intr.h>
#include <sys/avintr.h>
#include <sys/platform.h>
#include <sys/mach_led.h>
#include <sys/gic.h>
#include <sys/wdt.h>

static int cbe_ticks = 0;

static cyc_func_t volatile cbe_xcall_func;
static cpu_t *volatile cbe_xcall_cpu;
static void *cbe_xcall_farg;
static cpuset_t cbe_enabled;

static ddi_softint_hdl_impl_t cbe_low_hdl =
	{0, NULL, NULL, NULL, 0, NULL, NULL, NULL};
static ddi_softint_hdl_impl_t cbe_clock_hdl =
	{0, NULL, NULL, NULL, 0, NULL, NULL, NULL};

static cyclic_id_t cbe_hres_cyclic;

extern void ltimer_clear_intstat(processorid_t);
extern void ltimer_reprogram(hrtime_t, processorid_t);
extern void ltimer_enable(processorid_t);
extern void ltimer_disable(processorid_t);
extern int ltimer_init(void);

static int
cbe_softclock(void)
{
	cyclic_softint(CPU, CY_LOCK_LEVEL);
	return (1);
}

static int
cbe_low_level(void)
{
	cyclic_softint(CPU, CY_LOW_LEVEL);
	return (1);
}

/*
 * The timer firing function.
 */
static int
cbe_fire(void)
{
	cpu_t *cpu = CPU;
	processorid_t me = cpu->cpu_id;

	/* Intr flag clear. */
	ltimer_clear_intstat(me);

	cyclic_fire(cpu);

	return (1);
}

/*
 * cyclic-induced cross call handler.
 */
static int
cbe_xcallint(void)
{
	ASSERT(cbe_xcall_func != NULL && cbe_xcall_cpu == CPU_GLOBAL);

	(*cbe_xcall_func)(cbe_xcall_farg);
	cbe_xcall_func = NULL;
	cbe_xcall_cpu = NULL;

	MEMORY_BARRIER();

	return (1);
}

/*ARGSUSED*/
static void
cbe_softint(void *arg, cyc_level_t level)
{
	switch (level) {
	case CY_LOW_LEVEL:
		(*setsoftint)(CBE_LOW_PIL, cbe_low_hdl.ih_pending);
		break;
	case CY_LOCK_LEVEL:
		(*setsoftint)(CBE_LOCK_PIL, cbe_clock_hdl.ih_pending);
		break;
	default:
		panic("cbe_softint: unexpected soft level %d", level);
	}
}

/*ARGSUSED*/
static void
cbe_reprogram(void *arg, hrtime_t time)
{
	cpu_t *cpu = CPU;
	processorid_t cpuid = cpu->cpu_id;

	ltimer_reprogram(time, cpuid);
}

/*ARGSUSED*/
static cyc_cookie_t
cbe_set_level(void *arg, cyc_level_t level)
{
	int ipl;

	switch (level) {
	case CY_LOW_LEVEL:
		ipl = CBE_LOW_PIL;
		break;
	case CY_LOCK_LEVEL:
		ipl = CBE_LOCK_PIL;
		break;
	case CY_HIGH_LEVEL:
		ipl = CBE_HIGH_PIL;
		break;
	default:
		panic("cbe_set_level: unexpected level %d", level);
	}

	return (splr(ipltospl(ipl)));
}

/*ARGSUSED*/
static void
cbe_restore_level(void *arg, cyc_cookie_t cookie)
{
	splx(cookie);
}

/*ARGSUSED*/
static void
cbe_xcall(void *arg, cpu_t *dest, cyc_func_t func, void *farg)
{
	cpuset_t	cpuset;

	kpreempt_disable();

	if (dest == CPU_GLOBAL) {
		(*func)(farg);
		kpreempt_enable();
		return;
	}

	CPUSET_ZERO(cpuset);

	ASSERT(cbe_xcall_func == NULL);

	cbe_xcall_farg = farg;
	cbe_xcall_cpu = CPU_SELF(dest);
	cbe_xcall_func = func;

	CPUSET_ADD(cpuset, dest->cpu_id);
	MEMORY_BARRIER();
	gic_send_ipi(cpuset, IRQ_IPI_CBE);

	while (cbe_xcall_func != NULL || cbe_xcall_cpu != NULL)
		continue;

	kpreempt_enable();

	ASSERT(cbe_xcall_func == NULL && cbe_xcall_cpu == NULL);
}

static void *
cbe_configure(cpu_t *cpu)
{
	return (cpu);
}

static void
cbe_enable(void *arg)
{
	processorid_t me = ((cpu_t *)arg)->cpu_id;

	ASSERT(!CPU_IN_SET(cbe_enabled, me));
	CPUSET_ADD(cbe_enabled, me);

	ltimer_enable(me);
}

static void
cbe_disable(void *arg)
{
	processorid_t me = ((cpu_t *)arg)->cpu_id;

	ASSERT(CPU_IN_SET(cbe_enabled, me));
	CPUSET_DEL(cbe_enabled, me);

	ltimer_disable(me);
}

/*
 * Unbound cyclic, called once per clock tick (every nsec_per_tick ns).
 */
static void
cbe_hres_tick(void)
{
	int s;

	dtrace_hres_tick();

	/*
	 * Because hres_tick effectively locks hres_lock, we must be at the
	 * same PIL as that used for CLOCK_LOCK.
	 */
	s = splr(ipltospl(XC_HI_PIL));
	hres_tick();
	splx(s);

	if ((cbe_ticks % 128) == 0) {
		/* Blink LED per 128 ticks */
		ARMPF_UPDATE_SECOND_LED();
	}

	/* Blink LED per 16 ticks */
	ARMPF_UPDATE_TICK_LED(cbe_ticks);

	cbe_ticks++;
}

void
cbe_init(void)
{

	cyc_backend_t cbe = {
		cbe_configure,		/* cyb_configure */
		NULL,			/* cyb_unconfigure */
		cbe_enable,		/* cyb_enable */
		cbe_disable,		/* cyb_disable */
		cbe_reprogram,		/* cyb_reprogram */
		cbe_softint,		/* cyb_softint */
		cbe_set_level,		/* cyb_set_level */
		cbe_restore_level,	/* cyb_restore_level */
		cbe_xcall,		/* cyb_xcall */
		NULL,			/* cyb_suspend */
		NULL			/* cyb_resume */
	};
	hrtime_t resolution;
	cyc_handler_t hdlr;
	cyc_time_t when;

	/* cbe_enabled Flags, 0 clear. */
	CPUSET_ZERO(cbe_enabled);

	/* Initialize CPU local timer and SCU counter. */
	resolution = ltimer_init();

	mutex_enter(&cpu_lock);
	cyclic_init(&cbe, resolution);
	mutex_exit(&cpu_lock);

	/* Add Local Timer Handler. */
	(void) add_avintr(NULL, CBE_HIGH_PIL, (avfunc)cbe_fire,
	    "cbe_fire", IRQ_LOCALTIMER, 0, NULL, NULL, NULL);

	/* Add IPI Handler. */
	(void) add_avintr(NULL, CBE_HIGH_PIL, (avfunc)cbe_xcallint,
		  "cbe_xcallint", IRQ_IPI_CBE, 0, NULL, NULL, NULL);

	/* Add softclock handler. */
	(void) add_avsoftintr((void *)&cbe_clock_hdl, CBE_LOCK_PIL,
	    (avfunc)cbe_softclock, "softclock", NULL, NULL);

	/* Add low level softintr handler. */
	(void) add_avsoftintr((void *)&cbe_low_hdl, CBE_LOW_PIL,
	    (avfunc)cbe_low_level, "low level", NULL, NULL);

	mutex_enter(&cpu_lock);

	/* Set parameters for high resolution tick process.*/
	hdlr.cyh_level = CY_HIGH_LEVEL;
	hdlr.cyh_func = (cyc_func_t)cbe_hres_tick;
	hdlr.cyh_arg = NULL;

	when.cyt_when = 0;
	when.cyt_interval = nsec_per_tick;/* NANOSEC/hz. */

	/* Add highres tick prosess to cyclic subsystem. */
	cbe_hres_cyclic = cyclic_add(&hdlr, &when);

	mutex_exit(&cpu_lock);

	wdt_refresh_init();
}
