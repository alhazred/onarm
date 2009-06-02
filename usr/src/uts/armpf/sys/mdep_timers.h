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

#ifndef	_SYS_MDEP_TIMERS_H
#define	_SYS_MDEP_TIMERS_H

#ident	"@(#)armpf/sys/mdep_timers.h"

/*
 * Definitions used for timer functions in mdep_timers.c
 * Kernel build tree private.
 */

/*
 * MPCore local timer frequency.
 * It is determined by CPU frequency and prescaler value.
 */
#define	LTIMER_DIVISOR		((LTIMER_PRESCALER + 1) << 1)
#define	FREQ_LTIMER		(FREQ_CPU / LTIMER_DIVISOR)

#endif	/* !_SYS_MDEP_TIMERS_H */
