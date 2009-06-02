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
 *  Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.
 *  Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T
 *    All Rights Reserved
 */

/*
 * Copyright (c) 2006-2008 NEC Corporation
 */

	.ident	"@(#)arm/ml/arm_spl.s"
	.file	"arm_spl.s"

#ifndef	_MACHDEP
#error	arm_spl.s must be linked to platform-specific module.
#endif	/* !_MACHDEP */

#include <sys/asm_linkage.h>
#include <sys/controlregs.h>
#include <sys/machlock.h>
#include <sys/cpuvar_impl.h>
#include <sys/gic.h>
#include "assym.h"

/*
 * Berkley 4.3 introduced symbolically named interrupt levels
 * as a way deal with priority in a machine independent fashion.
 * Numbered priorities are machine specific, and should be
 * discouraged where possible.
 *
 * Note, for the machine specific priorities there are
 * examples listed for devices that use a particular priority.
 * It should not be construed that all devices of that
 * type should be at that priority.  It is currently were
 * the current devices fit into the priority scheme based
 * upon time criticalness.
 *
 * The underlying assumption of these assignments is that
 * IPL 10 is the highest level from which a device
 * routine can call wakeup.  Devices that interrupt from higher
 * levels are restricted in what they can do.  If they need
 * kernels services they should schedule a routine at a lower
 * level (via software interrupt) to do the required
 * processing.
 *
 * Examples of this higher usage:
 *	Level	Usage
 *	14	Profiling clock (and PROM uart polling clock)
 *	12	Serial ports
 *
 * The serial ports request lower level processing on level 6.
 *
 * Also, almost all splN routines (where N is a number or a
 * mnemonic) will do a RAISE(), on the assumption that they are
 * never used to lower our priority.
 * The exceptions are:
 *	spl8()		Because you can't be above 15 to begin with!
 *	splzs()		Because this is used at boot time to lower our
 *			priority, to allow the PROM to poll the uart.
 *	spl0()		Used to lower priority to 0.
 */

/* locks out all interrupts, including memory errors */
ENTRY(spl8)
	GIC_RAISE_PIL_CONST(15, r1, r2, r3, ip)
	mov	pc, lr
	SET_SIZE(spl8)

/* just below the level that profiling runs */
ENTRY(spl7)
	GIC_RAISE_PIL_CONST(13, r1, r2, r3, ip)
	mov	pc, lr
	SET_SIZE(spl7)

/* sun specific - highest priority onboard serial i/o asy ports */
ENTRY(splzs)
	GIC_SET_PIL_CONST(12, r1, r2, r3, ip)
	mov	pc, lr
	SET_SIZE(splzs)

/*
 * should lock out clocks and all interrupts,
 * as you can see, there are exceptions
 */
ENTRY(splhi)
ALTENTRY(splhigh)
ALTENTRY(spl6)
ALTENTRY(i_ddi_splhigh)
	GIC_RAISE_PIL_CONST(DISP_LEVEL, r1, r2, r3, ip)
	mov	pc, lr
	SET_SIZE(i_ddi_splhigh)
	SET_SIZE(spl6)
	SET_SIZE(splhigh)
	SET_SIZE(splhi)

/* Allow all interrupts */
ENTRY(spl0)
	GIC_SET_PIL_CONST(0, r1, r2, r3, ip)
	mov	pc, lr
	SET_SIZE(spl0)

/*
 * void
 * splx(int level)
 *	Set PIL back to that indicated by the old PIL passed as an argument,
 *	or to the CPU's base priority, whichever is higher.
 */
ENTRY(splx)
ALTENTRY(i_ddi_splx)
	GIC_SET_PIL(r1, r2, r3, ip)
	mov	pc, lr
	SET_SIZE(i_ddi_splx)
	SET_SIZE(splx)

/*
 * int
 * splr(int level)
 *	splr() is like splx() but will only raise the priority and never
 *	drop it. Be careful not to set priority lower than CPU->cpu_base_pri,
 *	even though it seems we are raising the priority, it could be set
 *	higher at any time by an interrupt routine, so we must block
 *	inuterrupts and look at CPU->cpu_base_pri.
 */
ENTRY(splr)
	GIC_RAISE_PIL(r1, r2, r3, ip)
	mov	pc, lr
	SET_SIZE(splr)

/*
 * int
 * getpil(void)
 *      Get current processor interrupt level
 */
ENTRY(getpil)
	CPU_PIN(r2)			/* Pin this thread to the cpu */
	LOADCPU(r1)			/* r1 = CPU */
	ldr	r0, [r1, #CPU_PRI]	/* r0 = current PIL */
	CPU_UNPIN(r2)			/* Unpin this thread */
	mov	pc, lr
	SET_SIZE(getpil)
