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
 * Copyright (c) 2007 NEC Corporation
 * All rights reserved.
 */

#ifndef	_ASM_BSWAP_H
#define	_ASM_BSWAP_H

#ident	"@(#)arm/asm/bswap.h"

#include <sys/types.h>

/*
 * ARMv6 specific byte swap operations.
 */

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

#if	!defined(__lint) && defined(__GNUC__)

/* Byte swapping function for 16bit data. */
static __inline__ uint16_t
arm_bswap16(uint16_t value)
{
	uint16_t	ret;

	__asm__ __volatile__
		("rev16	%0, %1" : "=r" (ret) : "r" (value));
	return ret;
}

/* Byte swapping function for 32bit data. */
static __inline__ uint32_t
arm_bswap32(uint32_t value)
{
	uint32_t	ret;

	__asm__ __volatile__
		("rev	%0, %1" : "=r" (ret) : "r" (value));
	return ret;
}

/* Byte swapping function for 64bit data. */
static __inline__ uint64_t
arm_bswap64(uint64_t value)
{
	return ((uint64_t)arm_bswap32((uint32_t)(value & 0xffffffff)) << 32) |
		(uint64_t)arm_bswap32((uint32_t)(value >> 32));
}

#endif	/* !__lint && __GNUC__ */

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* !_ASM_BSWAP_H */
