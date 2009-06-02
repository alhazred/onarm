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
 * Copyright (c) 2006-2009 NEC Corporation
 * All rights reserved.
 */

#ident	"@(#)mdep_timers.c"

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/clock.h>
#include <sys/debug.h>
#include <sys/cpuvar.h>
#include <sys/archsystm.h>
#include <sys/platform.h>
#include <sys/time.h>
#include <sys/mpcore.h>
#include <sys/dtrace.h>
#include <sys/bitmap.h>
#include <sys/mdep_timers.h>

/*
 *	Local function prototypes
 */
void ltimer_reprogram(hrtime_t time, processorid_t cpuid);
void ltimer_enable(processorid_t cpuid);
void ltimer_disable(processorid_t cpuid);
static int ltimer_clkinit(int hertz);
void ltimer_clear_intstat(processorid_t cpuid);
void scucnt_init(void);
void scucnt_reset(void);
void scucnt_enable(void);
void scucnt_disable(void);
uint32_t scucnt_read(void);
hrtime_t scucnt_gethrtime(void);
hrtime_t scucnt_gethrtimeunscaled(void);
int scucnt_intr_handler(void);
void scucnt_intr_clear(void);

#define LTIMER_MAXCNT 0xffffffffUL
#define SCU_MAXCNT 0xffffffffUL
#define SCUCNT_PIL    15

/* minimum ticks of CPU local timer ticks to program to */
int ltimer_min_timer_cnt = 1;  

volatile int scucnt_hrtime_stamp = 0;

/* maximum counter value of CPU local timer. */
static hrtime_t ltimer_nsec_max =
	(hrtime_t)LTIMER_MAXCNT * NANOSEC / FREQ_LTIMER;
static uint_t ltimer_nsec_per_cnt =
	(uint_t)((hrtime_t)NANOSEC / FREQ_LTIMER);

#define	MEGA_HZ		1000000

/* CPU frequency. */
uint_t	cpu_freq = FREQ_CPU / MEGA_HZ;		/* MHz */ 
uint64_t cpu_freq_hz = FREQ_CPU;		/* measured (in hertz) */

/*
 * The following two variables keep the ratio of NANOSEC to CPU frequency.
 * They are used to convert SCU performance monitor counter to nanoseconds.
 */
uint64_t	scucnt_ratio_nsec;
uint64_t	scucnt_ratio_freq;

static uint64_t	scucnt_maxcnt;
static uint64_t	scucnt_maxcnt_scaled;
static uint64_t	scucnt_maxcnt_fraction;
static uint32_t	scucnt_maxcnt_shift;

/* Convert SCU counter into nanoseconds. */
#define	SCALETIME(tk)						\
	(((tk) * scucnt_ratio_nsec) / scucnt_ratio_freq)
#define	SCALETIME_MOD(tk)					\
	(((tk) * scucnt_ratio_nsec) % scucnt_ratio_freq)

#define	SCALETIME_FRAC_SIG	10000

/* Set bits in Performance Monitor Control Register. */
#define	PERFMON_CTRL_SET(bits)					\
	do {							\
		uint32_t	__r;				\
								\
		__r = readl(MPCORE_SCUREG_VADDR(PERFMON_CTRL));	\
		__r |= (bits);					\
		writel(__r, MPCORE_SCUREG_VADDR(PERFMON_CTRL));	\
	} while (0)

/* Clear bits in Performance Monitor Control Register. */
#define	PERFMON_CTRL_CLR(bits)					\
	do {							\
		uint32_t	__r;				\
								\
		__r = readl(MPCORE_SCUREG_VADDR(PERFMON_CTRL));	\
		__r &= ~(bits);					\
		writel(__r, MPCORE_SCUREG_VADDR(PERFMON_CTRL));	\
	} while (0)

/* Reset SCU performance monitor counter. */
#define	SCUCNT_RESET()					\
	PERFMON_CTRL_SET(MPCORE_SCU_PERFMON_CTRL_RST)

/* Enable SCU performance monitor counter. */
#ifdef	MARINE4_PMUIRQ_ERRATA
#define	SCUCNT_ENABLE()						\
	PERFMON_CTRL_SET(MPCORE_SCU_PERFMON_CTRL_EN)
#else	/* !MARINE4_PMUIRQ_ERRATA */
#define	SCUCNT_ENABLE()						\
	PERFMON_CTRL_SET(MPCORE_SCU_PERFMON_CTRL_EN |		\
			 MPCORE_SCU_PERFMON_CTRL_INTEN0)
#endif	/* MARINE4_PMUIRQ_ERRATA */

/* Disable SCU performance monitor counter. */
#define	SCUCNT_DISABLE()				\
	PERFMON_CTRL_CLR(MPCORE_SCU_PERFMON_CTRL_EN)

/* Clear MN0 overflow interrupt. */
#define	SCUCNT_INTR_CLR()					\
	PERFMON_CTRL_SET(MPCORE_SCU_PERFMON_CTRL_INTCLR0)

/* Reset and enable SCU performance monitor counter. */
#ifdef	MARINE4_PMUIRQ_ERRATA
#define	SCUCNT_RESET_ENABLE()					\
	PERFMON_CTRL_SET(MPCORE_SCU_PERFMON_CTRL_RST|		\
			 MPCORE_SCU_PERFMON_CTRL_EN)
#else	/* !MARINE4_PMUIRQ_ERRATA */
#define	SCUCNT_RESET_ENABLE()					\
	PERFMON_CTRL_SET(MPCORE_SCU_PERFMON_CTRL_RST|		\
			 MPCORE_SCU_PERFMON_CTRL_EN|		\
			 MPCORE_SCU_PERFMON_CTRL_INTEN0)
#endif	/* MARINE4_PMUIRQ_ERRATA */

/* Disable and reset SCU performance monitor counter. */
#define	SCUCNT_RESET_DISABLE()					\
	do {							\
		uint32_t	__r;				\
								\
		__r = readl(MPCORE_SCUREG_VADDR(PERFMON_CTRL));	\
		__r &= ~MPCORE_SCU_PERFMON_CTRL_EN;		\
		__r |= (MPCORE_SCU_PERFMON_CTRL_INTCLR0|	\
			MPCORE_SCU_PERFMON_CTRL_RST);		\
		writel(__r, MPCORE_SCUREG_VADDR(PERFMON_CTRL));	\
	} while (0)

/* local timer clock frequency (MHz).*/
uint64_t ltimer_freq = FREQ_LTIMER;

/* called from armpf/io/cbe.c: cbe_init() */
int
ltimer_init()
{
	int resolution;

	/* 
	 * SCU Counter has already initialized in startup_init().
	 */

#ifndef	MARINE4_PMUIRQ_ERRATA
	/* Regist SCU Monitor Counter Interrupt Handler. */
	(void) add_avintr(NULL, SCUCNT_PIL, (avfunc)scucnt_intr_handler,
	    "scucnt_intr",IRQ_PMU_SCU0, 0, NULL, NULL, NULL);
#endif	/* !MARINE4_PMUIRQ_ERRATA */

	/* Initialize CPU local timer.  */
	resolution = ltimer_clkinit(0);

	return (resolution);
}

/* 
 * Reprogram the timer(oneshot mode).  
 *
 * When in oneshot mode the argument is the absolute time in future to
 * generate the interrupt at.
 */
void
ltimer_reprogram(hrtime_t time, processorid_t cpuid)
{
	hrtime_t now;
	uint_t ticks;

	/* oneshot mode  */
	now = scucnt_gethrtime();
	if (time <= now) {
		/*
		 * requested to generate an interrupt in the past
		 * generate an interrupt as soon as possible.
		 */
		ticks = ltimer_min_timer_cnt;
	} else if ((time - now) > ltimer_nsec_max) {
		/*
		 * requested to generate an interrupt at a time
		 * further than what we are capable of. Set to max
		 * the hardware can handle.
		 */
		ticks = LTIMER_MAXCNT;
#ifdef DEBUG
		cmn_err(CE_CONT, "ltimer_reprogram, request at"
			"  %lld  too far in future, current time"
			"  %lld \n", time, now);
#endif	/* DEBUG */
	} else {
		uint64_t	nsec = (uint64_t)time - (uint64_t)now;

		ticks = (uint_t)(nsec * FREQ_LTIMER / NANOSEC);
	}
	/* Check if ticks < 1. */
	if (ticks < ltimer_min_timer_cnt) {
		ticks = ltimer_min_timer_cnt;
	}
	/* Set ticks to counter register. */
	ASSERT(cpuid == CPU->cpu_id);
	writel(ticks, MPCORE_TWD_PRIVATE_VADDR(TIMER_COUNTER));
}

/*
 * This function will enable timer interrupts(only oneshot mode).
 */
void
ltimer_enable(processorid_t cpuid)
{
	uint32_t r;

	ASSERT(cpuid == CPU->cpu_id);

	/* Clear load and counter Register. */
	writel(0, MPCORE_TWD_PRIVATE_VADDR(TIMER_LOAD));

	/* Clear interrupt flag. */
	ltimer_clear_intstat(cpuid);

	/* Set timer enable, oneshot mode and Interrupt enable. */
	r = readl(MPCORE_TWD_PRIVATE_VADDR(TIMER_CTRL));
	r |= (MPCORE_TWD_TIMER_CTRL_ENABLE | MPCORE_TWD_TIMER_CTRL_INTR);

	/* Set prescaler value. */
	r &= ~MPCORE_TWD_TIMER_CTRL_PRESCALER_MASK;
	r |= MPCORE_TWD_TIMER_CTRL_PRESCALER(LTIMER_PRESCALER);

	writel(r, MPCORE_TWD_PRIVATE_VADDR(TIMER_CTRL));
}

/*
 * This function will disable timer interrupts(only oneshot mode).
 */
void
ltimer_disable(processorid_t cpuid)
{
	uint32_t r;

	ASSERT(cpuid == CPU->cpu_id);

	r = readl(MPCORE_TWD_PRIVATE_VADDR(TIMER_CTRL));
	r &= ~(MPCORE_TWD_TIMER_CTRL_ENABLE | MPCORE_TWD_TIMER_CTRL_INTR); 
	writel(r, MPCORE_TWD_PRIVATE_VADDR(TIMER_CTRL));
}

/*
 * Initialize the MPCore local timer of booted CPU.  Note at this stage in
 * the boot sequence, the boot processor is the only active processor.
 * hertz value of 0 indicates a one-shot mode request.  In this case
 * the function returns the resolution (in nanoseconds) for the
 * hardware timer interrupt.
 */
/* ARGSUSED */
static int
ltimer_clkinit(int hertz)
{
	cpu_t *cpu = CPU;
	processorid_t me = cpu->cpu_id;

	/* Adjust nanoseconds per one local timer count. */
	if (ltimer_nsec_per_cnt == 0){
		ltimer_nsec_per_cnt = 1;
	}

	/* Set the register of local timer. */
	ltimer_enable(me);
	return((int)ltimer_nsec_per_cnt);
}

/* Clear intrrupt flag. */
void
ltimer_clear_intstat(processorid_t cpuid)
{
	ASSERT(cpuid == CPU->cpu_id);

	/* Clear interrupt flag. */
	writel(1, MPCORE_TWD_PRIVATE_VADDR(TIMER_INTSTAT));
}


/*
 * static inline uint32_t
 * scucnt_read_impl(void)
 *	Inline function to read SCU counter value.
 */
static inline uint32_t
scucnt_read_impl(void)
{
	uint32_t r;

	/* Read MN0 counter value.*/
	r = readl(MPCORE_SCUREG_VADDR(MONCNT0));
	return r;
}

/*
 * Read Monitor Counter0 and return value.
 *	Note: We assume that we read monitor counter0 only. 
 */
uint32_t
scucnt_read(void)
{
	return scucnt_read_impl();
}

volatile uint64_t scucnt_lasttick = 0;

/*
 * Return the number of counts since booting.
 */
hrtime_t
scucnt_gethrtimeunscaled(void)
{
	uint32_t cpsr, count;
	uint64_t ret, last;
	int old_hrtime_stamp;
#ifdef	MARINE4_PMUIRQ_ERRATA
	uint32_t ovf = 0;
#endif	/* MARINE4_PMUIRQ_ERRATA */

retry:
	last = scucnt_lasttick;

	cpsr = DISABLE_IRQ_SAVE();	/* prevent migration */
	old_hrtime_stamp = scucnt_hrtime_stamp;
	count = scucnt_read_impl();
	RESTORE_INTR(cpsr);

	ret = (uint64_t)count + ((uint64_t)old_hrtime_stamp << 32);

	if (scucnt_hrtime_stamp != old_hrtime_stamp) { /* got an interrupt */
		goto retry;
	}

	while (((ret - last) & 0x8000000000000000ULL) != 0) {
		/*
		 * scucnt_intr_handler() may not yet be called when
		 * SCU performance counter is overflowed.
		 * We must adjust return value as appropriate.
		 */
		ret += 0x100000000ULL;
#ifdef	MARINE4_PMUIRQ_ERRATA
		ovf++;
#endif	/* MARINE4_PMUIRQ_ERRATA */
	}

#ifdef	MARINE4_PMUIRQ_ERRATA
	if (ovf != 0) {
		int new = old_hrtime_stamp + ovf;
		if (atomic_cas_32((volatile uint32_t *)&scucnt_hrtime_stamp,
				  old_hrtime_stamp, new) != old_hrtime_stamp) {
			/* Lost the race. */
			ovf = 0;
			goto retry;
		}
	}
#endif	/* MARINE4_PMUIRQ_ERRATA */

	if (atomic_cas_64((volatile uint64_t *)&scucnt_lasttick, last, ret)
	    != last) {
		/* Lost the race. */
		goto retry;
	}

	return (ret);
}

/*
 * Convert a timestamp to nanoseconds.
 */
hrtime_t
scucnt_scaletime(hrtime_t mtime)
{
	ASSERT(scucnt_ratio_nsec != 0 && scucnt_ratio_freq != 0);

	if (mtime > scucnt_maxcnt) {
		uint64_t	quot, mod, a, b, c;

		/* Need to avoid overflow. */
		quot = mtime >> scucnt_maxcnt_shift;
		mod = mtime & (scucnt_maxcnt - 1);
		a = scucnt_maxcnt_scaled * quot;
		b = SCALETIME(mod);
		c = quot * scucnt_maxcnt_fraction / SCALETIME_FRAC_SIG;

		return  a + b + c;
	}

	return SCALETIME(mtime);
}

/*
 * Return the number of nanoseconds since booting.
 */
hrtime_t
scucnt_gethrtime(void)
{
	hrtime_t mtime = scucnt_gethrtimeunscaled();
	return scucnt_scaletime(mtime);
}

/*
 * Get timestamp for DTrace.
 */
hrtime_t
dtrace_gethrtime(void)
{
	hrtime_t mtime = scucnt_gethrtimeunscaled();
	return scucnt_scaletime(mtime);
}

/*
 * Convert a timestamp to nanoseconds.
 */
void
scucnt_scalehrtime(hrtime_t *mtp)
{
	hrtime_t mtime;

	if (mtp == NULL) return;

	mtime = scucnt_scaletime(*mtp);
	*mtp = mtime;
}

/*
 * void
 * scucnt_mlsetup(void)
 *	SCU performance monitor counter initialization that should be done
 *	at early system startup. mlsetup() must call scucnt_mlsetup()
 *	before it initializes t0.
 */
void
scucnt_mlsetup(void)
{
	uint64_t	m, n, gcd, mod, quot;

	/*
	 * Calculate the ratio of NANOSEC to CPU frequency.
	 * At first, calculate the greatest common divisor of them.
	 */
	m = NANOSEC;
	n = FREQ_CPU;
	if (m < n) {
		uint64_t	tmp = n;

		n = m;
		m = tmp;
	}

	ASSERT(n != 0);
	while ((mod = m % n) != 0) {
		m = n;
		n = mod;
	}
	gcd = n;

	/* Initialize the ratio of NANOSEC to CPU frequency. */
	scucnt_ratio_nsec = NANOSEC / gcd;
	scucnt_ratio_freq = FREQ_CPU / gcd;

	/*
	 * Calculate maximum SCU counter value which is a power of 2 and
	 * doesn't cause overflow while nanosec conversion.
	 */
	quot = (uint64_t)-1 / scucnt_ratio_nsec;
	if (quot & 0xffffffff00000000ULL) {
		int	n = highbit((ulong_t)(quot >> 32));

		scucnt_maxcnt_shift = n - 1 + 32;
	}
	else {
		int	n = highbit((ulong_t)(quot & 0xffffffffULL));

		scucnt_maxcnt_shift = n - 1;
	}
	scucnt_maxcnt = (1ULL << scucnt_maxcnt_shift);

	/*
	 * Convert scucnt_maxcnt into nanoseconds with fraction part.
	 */
	scucnt_maxcnt_scaled = SCALETIME(scucnt_maxcnt);
	mod = SCALETIME_MOD(scucnt_maxcnt);
	scucnt_maxcnt_fraction =
		(mod * SCALETIME_FRAC_SIG) / scucnt_ratio_freq;

}

/* 
 * Initialize SCU performance monitor counter. 
 *	Note: We assume that we use monitor counter0 only. 
 */
void
scucnt_init(void)
{
	uint32_t r;

	/* disable counter.*/
	SCUCNT_DISABLE();

	/* Set cycle event to Perf Monitor Event 0 Register. */
	/* Use MN0 counter only. */
	r = readl(MPCORE_SCUREG_VADDR(MONCNT_EV0));
	r &= 0xffffff00; /* clear bits 0-7 */
	r |= MPCORE_SCU_MONCNT_EVN31; /* Set event32(cycle count).*/
	writel(r, MPCORE_SCUREG_VADDR(MONCNT_EV0));

	/* Reset and enable SCU Monitor Counter. */
	SCUCNT_RESET_ENABLE();

	scucnt_lasttick = 0;	/* scucnt_gethrtimeunscaled() may be called before */
}

/*
 * Reset All Monitor Counters .
 */
void
scucnt_reset(void)
{
	SCUCNT_RESET();
}

/*
 * Enable All Monitor Counters.
 */
void
scucnt_enable(void)
{
	SCUCNT_ENABLE();
}

/*
 * Disable All Monitor Counters.
 */
void
scucnt_disable(void)
{
	SCUCNT_DISABLE();
}

#ifndef	MARINE4_PMUIRQ_ERRATA
/*
 * SCU overflow interrupt handler.
 */
int
scucnt_intr_handler(void)
{
	/* Clear Intr Flag.*/
	SCUCNT_INTR_CLR();

	/* Update nanosec since boot. */
	scucnt_hrtime_stamp++;
	MEMORY_BARRIER();

	return DDI_INTR_CLAIMED;
}
#endif	/* !MARINE4_PMUIRQ_ERRATA */

/* Clear MN0 overflow interrupt flag. */
void
scucnt_intr_clear(void)
{
	SCUCNT_INTR_CLR();
}
