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
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2006 NEC Corporation
 */

#ifndef _ASM_CLOCK_H
#define	_ASM_CLOCK_H

#pragma ident	"@(#)arm/asm/clock.h"

#include <sys/types.h>
#include <sys/time.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if !defined(__lint) && defined(__GNUC__)

#include <sys/machlock.h>
#include <asm/cpufunc.h>

#if	LOCK_HELD_VALUE != 0xff
#error	"LOCK_HELD_VALUE must be 0xff."
#endif	/* LOCK_HELD_VALUE != 0xff */

extern __inline__ void
unlock_hres_lock(void)
{
	uint32_t	tmp1, tmp2;
	uint32_t	*lockp = (uint32_t *)&hres_lock;

	MEMORY_BARRIER();
	__asm__ __volatile__
		("1:\n"
		 "ldrex		%0, [%2]\n"
		 "add		%0, %0, #1\n"
		 "strex		%1, %0, [%2]\n"
		 "teq		%1, #0\n"
		 "bne		1b\n"
		 : "=&r" (tmp1), "=&r" (tmp2)
		 : "r" (lockp)
		 : "cc");
}

#endif	/* !__lint && __GNUC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _ASM_CLOCK_H */
