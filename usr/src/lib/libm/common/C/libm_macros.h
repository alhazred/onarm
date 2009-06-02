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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2007 NEC Corporation
 */

#ifndef _LIBM_MACROS_H
#define	_LIBM_MACROS_H

#pragma ident	"@(#)libm_macros.h	1.4	06/01/23 SMI"


#if defined(__sparc)

#define	HIWORD		0
#define	LOWORD		1
#define	HIXWORD		0		/* index of int containing exponent */
#define	XSGNMSK		0x80000000	/* exponent bit mask within the int */
#define	XBIASED_EXP(x)	((((int *)&x)[HIXWORD] & ~0x80000000) >> 16)
#define	ISZEROL(x)	(((((int *)&x)[0] & ~XSGNMSK) | ((int *)&x)[1] | \
				((int *)&x)[2] | ((int *)&x)[3]) == 0)
#define	HHWORD		0
#define	HLWORD		1
#define	LHWORD		2
#define	LLWORD		3

#elif defined(__i386) || defined(__amd64) || defined(__arm)

#if defined(__arm)
#if defined(__VFP_FP__)
#define	HIWORD		1
#define	LOWORD		0
#else
#define	HIWORD		0
#define	LOWORD		1
#endif
#else
#define	HIWORD		1
#define	LOWORD		0
#endif
#if (HIWORD)
#define	HIXWORD		2
#define	HHWORD		3
#define	HLWORD		2
#define	LHWORD		1
#define	LLWORD		0
#else
#define	HIXWORD		0
#define	HHWORD		0
#define	HLWORD		1
#define	LHWORD		2
#define	LLWORD		3
#endif
#define	XSGNMSK		0x8000
#define	XBIASED_EXP(x)	(((int *)&x)[HIXWORD] & 0x7fff)
#define	ISZEROL(x)	(x == 0.0L)

#define	HANDLE_UNSUPPORTED

/*
 * "convert" the high-order 32 bits of a SPARC quad precision
 * value ("I") to the sign, exponent, and high-order bits of an
 * x86 extended double precision value ("E"); the low-order bits
 * in the 12-byte quantity are left intact
 */
#define	ITOX(I, E)       \
		E[2] = 0xffff & ((I) >> 16); \
		E[1] = (((I) & 0x7fff0000) == 0)? \
		    (E[1] & 0x7fff) | (0x7fff8000 & ((I) << 15)) :\
		    0x80000000 | (E[1] & 0x7fff) | (0x7fff8000 & ((I) << 15))

/*
 * "convert" the sign, exponent, and high-order bits of an x86
 * extended double precision value ("E") to the high-order 32 bits
 * of a SPARC quad precision value ("I")
 */
#define	XTOI(E, I)	\
		I = ((E[2]<<16) | (0xffff & (E[1]>>15)))

#else
#error Unknown architecture
#endif

/* used mvec .c */
#if (defined(__arm) && (HIWORD)) || (!defined(__arm) && defined(__LITTLE_ENDIAN))
#define HI(x)	*(1+(int*)x)
#define LO(x)	*(unsigned*)x
#define DBLWORD(x, y)   y, x
#else
#define HI(x)	*(int*)x
#define LO(x)	*(1+(unsigned*)x)
#define DBLWORD(x, y)   x, y
#endif

#ifdef __RESTRICT
#define restrict _Restrict
#else
#define restrict
#endif
/* used mvec .c */

#endif	/* !defined(_LIBM_MACROS_H) */
