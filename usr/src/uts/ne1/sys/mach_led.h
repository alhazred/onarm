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

#ifndef	_SYS_MACH_LED_H
#define	_SYS_MACH_LED_H

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

#ident	"@(#)ne1/sys/mach_led.h"

#include <sys/types.h>
#include <sys/platform.h>

/*
 * NE1 on-board LED bit assignment.
 */

/* Status LED bits */
#define	NE1_STATUS_LED_TICK		0	/* Blink per 16 tick */
#define	NE1_STATUS_LED_SECOND		1	/* Blink per second */

#define	NE1_STATUS_LED_BLINK(bit)					\
	do {								\
		uint16_t	__val = readw(NE1_STATUS_LED_VADDR);	\
		__val ^= (1U << (bit));					\
		writew(__val, NE1_STATUS_LED_VADDR);			\
	} while (0)

#define	NE1_STATUS_LED_SET(bit)						\
	do {								\
		writew(1U << (bit), NE1_STATUS_LED_VADDR);		\
	} while (0)

#define	NE1_STATUS_LED_CLEAR(bit)					\
	do {								\
		uint16_t	__val = readw(NE1_STATUS_LED_VADDR);	\
		__val &= ~(1U << (bit));				\
		writew(__val, NE1_STATUS_LED_VADDR);			\
	} while (0)

/* Blink LED bit which indicates one second passed. */
#define	ARMPF_UPDATE_SECOND_LED()		\
	NE1_STATUS_LED_BLINK(NE1_STATUS_LED_SECOND);

/* Blink LED bit which indicates 16 ticks passed. */
#define	ARMPF_UPDATE_TICK_LED(ticks)					\
	do {								\
		if (((ticks) & (16 - 1)) == 0) {			\
			NE1_STATUS_LED_BLINK(NE1_STATUS_LED_TICK);	\
		}							\
	} while (0)

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* !_SYS_MACH_LED_H */
