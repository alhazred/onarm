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

#ident	"@(#)arm/os/wdt.c"

#if	WDT_INTERVAL >= WDT_TIMEOUT
#error	WDT_INTERVAL is required to set a smaller value than WDT_TIMEOUT
#endif

#include <sys/types.h>
#include <sys/cyclic.h>
#include <sys/mpcore.h>
#include <sys/smp_impldefs.h>
#include <sys/thread.h>
#include <sys/x_call.h>
#include <sys/wdt.h>

static void wdt_init_common(void);
static void wdt_refresh_online(void *arg, cpu_t *cpu, cyc_handler_t *hdlr,
				cyc_time_t *when);
static void wdt_refresh(void);
static void wdt_refresh_local(void);
static void wdt_start_local(void);
static void wdt_stop_local(void);
static void wdt_timeout(void);

/* Macros calculate the appropriate values of wdt_prescaler and wdt_counter. */
#define	WDT_PRESCALER	(((uint64_t)(FREQ_CPU/1000) * WDT_TIMEOUT) >> 33)

#define	WDT_DIVISOR	((WDT_PRESCALER + 1) << 1)
#define	WDT_DIVIDEND	(FREQ_CPU / 1000)
#define	WDT_COUNTER	(((uint64_t)WDT_DIVIDEND * WDT_TIMEOUT) / WDT_DIVISOR)

/* Interrupt priority level for wdt_timeout() */
#define	WDT_IPL	15

typedef enum {
	WDT_DISABLED,		/* Watchdog Timer service is unavailable */
	WDT_INITIALIZED,	/* Watchdog Timer service has already started */
	WDT_ACTIVE		/* All functions have already run */
} wdt_status_t;

static wdt_status_t wdt_status = WDT_DISABLED;
static cpuset_t wdt_active_cpus;
static const uint8_t wdt_prescaler = (uint8_t)WDT_PRESCALER;
static uint32_t wdt_counter = (uint32_t)WDT_COUNTER;
static hrtime_t wdt_interval = (hrtime_t)WDT_INTERVAL * 1000000;
static uint_t wdt_timeout_counter;
static ddi_softint_hdl_impl_t wdt_hdl =
	{0, NULL, NULL, NULL, 0, NULL, NULL, NULL};

#pragma weak	icedb_exists

/*
 * void
 * wdt_init(void)
 * 	initialize Watchdog Timer service of boot CPU.
 */
void
wdt_init(void)
{
	uint_t ret;
	extern int icedb_exists(void);

	if (&icedb_exists != NULL && icedb_exists() != 0) {
		/* Don't activate WDT if debugger exists. */
		wdt_status = WDT_DISABLED;
		return;
	}

	/* Add Watchdog Timer Interrupt Handler. */
	ret = add_avintr(NULL, WDT_IPL, (avfunc)wdt_timeout,
			"wdt_timeout", IRQ_LOCALWDT, 0, NULL, NULL, NULL);
	if (ret) {
		/* watchdog registers are initialized. */
		wdt_init_common();

		wdt_status = WDT_INITIALIZED;
		wdt_timeout_counter = 1;

		/* Watchdog timer service start. */
		wdt_start(CPU->cpu_id);
	} else {
		wdt_status = WDT_DISABLED;
		prom_printf("WARNING: Watchdog Timer service is unavailable.\n");
	}
}

/*
 * static void
 * wdt_init_common(void)
 * 	initialize watchdog registers (Watchdog Load Register, Watchdog Counter
 * 	Register and Watchdog Interrupt Status Register) and set Prescaler in
 * 	Watchdog Control Register. 
 */
static void
wdt_init_common(void)
{
	uint32_t r;

	/* Clear Watchdog Load Register and Watchdog Counter Register. */
	writel(0, MPCORE_TWD_PRIVATE_VADDR(WD_LOAD));

	/* Clear Event flag in Watchdog Interrupt Status Register. */
	writel(1, MPCORE_TWD_PRIVATE_VADDR(WD_INTSTAT));

	/* Set Prescaler in Watchdog Control Register. */
	r = readl(MPCORE_TWD_PRIVATE_VADDR(WD_CTRL));
	r &= ~MPCORE_TWD_WD_CTRL_PRESCALER_MASK;
	r |= MPCORE_TWD_WD_CTRL_PRESCALER(wdt_prescaler);
	writel(r, MPCORE_TWD_PRIVATE_VADDR(WD_CTRL));
}

/*
 * void
 * wdt_mp_init(void)
 * 	initialize Watchdog Timer service of secondary CPUs.
 */
void
wdt_mp_init(void)
{
	if (wdt_status != WDT_ACTIVE) {
		return;
	}

	(*addspl)(IRQ_LOCALWDT, WDT_IPL, 0, 0);

	/* watchdog registers are initialized. */
	wdt_init_common();

	/* Watchdog timer service start. */
	wdt_start(CPU->cpu_id);
}

/*
 * void
 * wdt_refresh_init(void)
 * 	add a software interrupt handler and create an omnipresent cyclic to
 * 	refresh Watchdog Counter Register.
 */
void
wdt_refresh_init(void)
{
	uint_t ret;
	cyc_omni_handler_t hdlr;

	if (wdt_status != WDT_INITIALIZED) {
		return;
	}

	ret = add_avsoftintr((void *)&wdt_hdl, WDT_REFRESH_LEVEL,
			     (avfunc)wdt_refresh_local, "wdt_refresh_local",
			     NULL, NULL);
	if (ret) {
		wdt_refresh_local();

		hdlr.cyo_online = wdt_refresh_online;
		hdlr.cyo_offline = NULL;
		hdlr.cyo_arg = NULL;

		mutex_enter(&cpu_lock);
		(void) cyclic_add_omni(&hdlr);
		mutex_exit(&cpu_lock);

		wdt_status = WDT_ACTIVE;
	} else {
		wdt_status = WDT_DISABLED;
		(void) rem_avintr(NULL, WDT_IPL, (avfunc)wdt_timeout,
				  IRQ_LOCALWDT);
		prom_printf("WARNING: Watchdog Timer service "
			    "is unavailable.\n");
	}
}

/*
 * static void
 * wdt_refresh_online(void *arg, cpu_t *cpu, cyc_handler_t *hdlr,
 * 						 cyc_time_t *when)
 * 	set the properties of wdt_refresh() handler.
 */
static void
wdt_refresh_online(void *arg, cpu_t *cpu, cyc_handler_t *hdlr, cyc_time_t *when)
{
	hdlr->cyh_level = CY_LOCK_LEVEL;
	hdlr->cyh_func = (cyc_func_t)wdt_refresh;
	hdlr->cyh_arg = NULL;
	when->cyt_when = 0;
	when->cyt_interval = wdt_interval;
}

/*
 * static void
 * wdt_refresh(void)
 * 	kick the software interrupt handler, wdt_refresh_local(), to refresh
 * 	Watchdog Counter Register.
 */
static void
wdt_refresh(void)
{
	if (CPU_IN_SET(wdt_active_cpus, CPU->cpu_id)) {
		(*setsoftint)(WDT_REFRESH_LEVEL, wdt_hdl.ih_pending);
	}
}

/*
 * static void
 * wdt_refresh_local(void)
 * 	reload the starting value of Watchdog Counter Register.
 */
static void
wdt_refresh_local(void)
{
	uint32_t cpsr;
	
	cpsr = DISABLE_IRQ_SAVE();
	writel(wdt_counter, MPCORE_TWD_PRIVATE_VADDR(WD_LOAD));
	RESTORE_INTR(cpsr);
}

/*
 * void
 * wdt_start(processorid_t cpun)
 * 	direct Watchdog Timer service of a specified cpu to start.
 *
 * Remarks:
 *	the caller must specify the cpuid of cpu which is available and
 *	stopped service.
 */
void
wdt_start(processorid_t cpun)
{
	cpuset_t cpuset;
	uint_t cpuid;
	uint32_t cpsr;

	if (wdt_status == WDT_DISABLED) {
		return;
	}

	ASSERT(!CPU_IN_SET(wdt_active_cpus, cpun));
	ASSERT(CPU_IN_SET(mp_cpus, cpun));

	kpreempt_disable();
	cpuid = CPU->cpu_id;

	/* 
	 * Invoke wdt_start_local() if the current cpu's id and the
	 * specified cpu's id are the same. If not, invoke xc_call()
	 * to execute wdt_start_local() on the specified cpu.
	 */
	if (cpun == cpuid) {
		cpsr = DISABLE_IRQ_SAVE();
		wdt_start_local();
		RESTORE_INTR(cpsr);
	} else {
		CPUSET_ONLY(cpuset, cpun);
		xc_call(NULL, NULL, NULL, X_CALL_HIPRI,
			cpuset, (xc_func_t)wdt_start_local);
	}
	kpreempt_enable();
}

/*
 * static void
 * wdt_start_local(void)
 * 	set the values in watchdog registers (Watchdog Load Register, Watchdog
 * 	Counter Register and Watchdog Control Register) to start Watchdog
 * 	Timer service. 
 */
static void
wdt_start_local(void)
{
	uint32_t r;

	writel(wdt_counter, MPCORE_TWD_PRIVATE_VADDR(WD_LOAD));
	r = readl(MPCORE_TWD_PRIVATE_VADDR(WD_CTRL));
	r |= (MPCORE_TWD_WD_CTRL_ENABLE | MPCORE_TWD_WD_CTRL_INTR);
	writel(r, MPCORE_TWD_PRIVATE_VADDR(WD_CTRL));

	/* Mark Watchdog Timer service of the cpu as start. */
	CPUSET_ATOMIC_ADD(wdt_active_cpus, CPU->cpu_id);
}

/*
 * void
 * wdt_stop(processorid_t cpun)
 * 	direct Watchdog Timer service of a specified cpu to stop.
 */
void
wdt_stop(processorid_t cpun){

	cpuset_t cpuset;
	uint_t cpuid;
	uint32_t cpsr;

	if (!(CPU_IN_SET(wdt_active_cpus, cpun))) {
		return;
	}

	kpreempt_disable();
	cpuid = CPU->cpu_id;

	/* 
	 * Invoke wdt_stop_local() if the current cpu's id and the
	 * specified cpu's id are the same. If not, invoke xc_call()
	 * to execute wdt_stop_local() on the specified cpu.
	 */
	if (cpun == cpuid) {
		cpsr = DISABLE_IRQ_SAVE();
		wdt_stop_local();
		RESTORE_INTR(cpsr);
	} else {
		CPUSET_ONLY(cpuset,cpun);
		xc_call(NULL, NULL, NULL, X_CALL_HIPRI,
			cpuset, (xc_func_t)wdt_stop_local);
	}
	kpreempt_enable();
}

/*
 * static void
 * wdt_stop_local(void)
 * 	set the value in Watchdog Control Register to stop Watchdog Timer
 * 	service. 
 */
static void
wdt_stop_local(void)
{
	uint32_t r;

	r = readl(MPCORE_TWD_PRIVATE_VADDR(WD_CTRL));
	r &= ~(MPCORE_TWD_WD_CTRL_ENABLE | MPCORE_TWD_WD_CTRL_INTR);
	writel(r, MPCORE_TWD_PRIVATE_VADDR(WD_CTRL));

	/* Mark Watchdog Timer service of the cpu as stop. */
	CPUSET_ATOMIC_DEL(wdt_active_cpus, CPU->cpu_id);
}

/*
 * static void
 * wdt_timeout(void)
 * 	invoke panic() as the value of Watchdog Counter Register reaches zero.
 */
static void
wdt_timeout(void)
{
	/* Clear Event flag in Watchdog Interrupt Status Register. */
	writel(1, MPCORE_TWD_PRIVATE_VADDR(WD_INTSTAT));

	switch (wdt_status) {
	case WDT_INITIALIZED:
		prom_printf("WARNING: %llu milliseconds have passed since "
			    "Watchdog Timer service was started\n", 
			    (uint64_t)WDT_TIMEOUT * wdt_timeout_counter);
		wdt_timeout_counter++;
		wdt_refresh_local();
		break;
	case WDT_DISABLED:
		wdt_stop_local();
		break;
	case WDT_ACTIVE:
		if(!panicstr){
			panic("Watchdog timer expired.");
		}
		break;
	}
}
