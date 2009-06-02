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
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*
 * Copyright (c) 2008 NEC Corporation
 */

#pragma ident	"@(#)hardclk.c	1.35	07/09/18 SMI"

#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/archsystm.h>
#include <sys/lockstat.h>

#include <sys/clock.h>
#include <sys/debug.h>
#include <sys/smp_impldefs.h>
#include <sys/rtc.h>

/*
 * This file contains all generic part of clock and timer handling.
 * Specifics are now in separate files and may be overridden by TOD
 * modules.
 */

#if	ARMPF_RTC_EXIST == 0

/*
 * RTC is not supported on the target board.
 * Use dummy RTC feature.
 */

#define SECONDS_PER_DAY	86400

static void		armpf_tod_set(timestruc_t ts);
static  timestruc_t	armpf_tod_get(void);
static void		mach_set_hrestime(void);

/*
 * void
 * armpf_rtc_init(void)
 *	Setup dummy RTC feature.
 */
void
armpf_rtc_init(void)
{
	extern int dosynctodr;

	/* 
	 * Set base time of hrestime. (2007/1/1 0:00:00)
	 */
	mach_set_hrestime();

	/*
	 * If dosynctodr is 0, then the tod chip is independent of the
	 * software clock and should not be adjusted, but allowed to
	 * free run. This allows NTP to sync. hrestime without any
	 * interference from the tod chip.
	 */
	dosynctodr = 0;
}

static timestruc_t
armpf_tod_get(void)
{
	timestruc_t	ts;
	/* 
	 * This function is dummy. We assume that the current time is
	 * "2007/1/1 0:00:00 + elapsed time from this boot".  We have
	 * already set the number of seconds from 1970/1/1 0:00:00 to
	 * 2007/1/1 0:00:00 to hrestime.tv_sec in mach_set_hrestime(),
	 * so we only call gethrestime() in this function.
	 */
	gethrestime(&ts);
	return (ts);
}

static void
armpf_tod_set(timestruc_t ts)
{
	/*
	 * This function is dummy. Do nothing.
	 */
}

static void
mach_set_hrestime(void)
{
	/* dummy_sec: Number of seconds from 1970/1/1 to 2008/1/1 */
	time_t dummy_sec = ((2008 - 1970) * 365 + 9) * SECONDS_PER_DAY;
	timestruc_t t;

	t.tv_sec  = dummy_sec;
	t.tv_nsec = 0;
	set_hrestime(&t);
}

#else	/* ARMPF_RTC_EXIST != 0 */

/*
 * You must implement the following functions to support RTC.
 *    - void armpf_rtc_init(void)
 *    - void armpf_tod_set(timestruc_t ts)
 *    - timestruc_t armpf_tod_get(void)
 */
extern void		armpf_tod_set(timestruc_t ts);
extern timestruc_t	armpf_tod_get(void);

#endif	/* ARMPF_RTC_EXIST == 0 */

/*
 * Write the specified time into the clock chip.
 * Must be called with tod_lock held.
 */
void
tod_set(timestruc_t ts)
{
	ASSERT(MUTEX_HELD(&tod_lock));

	/*
	 * Prevent false alarm in tod_validate() due to tod value change.
	 */
	tod_fault_reset();
	armpf_tod_set(ts);
}

/*
 * Read the current time from the clock chip and convert to UNIX form.
 * Assumes that the year in the clock chip is valid.
 * Must be called with tod_lock held.
 */
timestruc_t
tod_get(void)
{
	timestruc_t ts;

	ASSERT(MUTEX_HELD(&tod_lock));

	ts = armpf_tod_get();
	ts.tv_sec = tod_validate(ts.tv_sec);
	return (ts);
}

/*
 * The following wrappers have been added so that locking
 * can be exported to platform-independent clock routines
 * (ie adjtime(), clock_setttime()), via a functional interface.
 */
int
hr_clock_lock(void)
{
	ushort_t s;

	CLOCK_LOCK(&s);
	return (s);
}

void
hr_clock_unlock(int s)
{
	CLOCK_UNLOCK(s);
}
