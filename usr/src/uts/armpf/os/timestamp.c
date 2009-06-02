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
 * Copyright (c) 2006-2007 NEC Corporation
 */

#pragma ident	"@(#)timestamp.c	1.22	06/02/03 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/disp.h>
#include <sys/var.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/archsystm.h>
#include <sys/cpuvar.h>
#include <sys/clock.h>
#include <sys/atomic.h>
#include <sys/lockstat.h>
#include <sys/smp_impldefs.h>
#include <sys/time.h>

extern uint64_t cpu_freq_hz;

/*
 * The following converts nanoseconds of highres-time to ticks
 */

static uint64_t
hrtime2tick(hrtime_t ts)
{
	hrtime_t q = ts / NANOSEC;
	hrtime_t r = ts - (q * NANOSEC);

	return (q * cpu_freq_hz + ((r * cpu_freq_hz) / NANOSEC));
}

/*
 * This is used to convert scaled high-res time from nanoseconds to
 * unscaled hardware ticks.  (Read from hardware timestamp counter)
 */

uint64_t
unscalehrtime(hrtime_t ts)
{
	uint64_t unscale = 0;
	hrtime_t rescale;
	hrtime_t diff = ts;

	while (diff > (nsec_per_tick)) {
		unscale += hrtime2tick(diff);
		rescale = unscale;
		scalehrtime(&rescale);
		diff = ts - rescale;
	}

	return (unscale);
}
