/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)quad.h	8.1 (Berkeley) 6/4/93
 * $FreeBSD: src/sys/libkern/quad.h,v 1.18 2004/05/05 15:12:55 bde Exp $
 */

#ifndef _LIBKERN_QUAD_H_
#define	_LIBKERN_QUAD_H_

/*
 * Quad arithmetic.
 *
 * This library makes the following assumptions:
 *
 *  - The type long long (aka int64_t) exists.
 *
 *  - A quad variable is exactly twice as long as `long'.
 *
 *  - The machine's arithmetic is two's complement.
 *
 * This library can provide 128-bit arithmetic on a machine with 128-bit
 * quads and 64-bit longs, for instance, or 96-bit arithmetic on machines
 * with 48-bit longs.
 */

#include <sys/types.h>

/*
 * Depending on the desired operation, we view a `long long' (aka int64_t) in
 * one or more of the following formats.
 */
union uu {
	int64_t	q;		/* as a (signed) quad */
	int64_t	uq;		/* as an unsigned quad */
	long	sl[2];		/* as two signed longs */
	u_long	ul[2];		/* as two unsigned longs */
};

#if	defined(_LITTLE_ENDIAN)
#define	_QUAD_HIGHWORD	1
#define	_QUAD_LOWWORD	0
#elif defined(_BIG_ENDIAN)
#define	_QUAD_HIGHWORD	0
#define	_QUAD_LOWWORD	1
#else	/* !_LITTLE_ENDIAN && !_BIG_ENDIAN */
#error	"Endianess is not defined."
#endif	/* defined(_LITTLE_ENDIAN) */

/*
 * Define high and low longwords.
 */
#define	H		_QUAD_HIGHWORD
#define	L		_QUAD_LOWWORD

/*
 * Total number of bits in a int64_t and in the pieces that make it up.
 * These are used for shifting, and also below for halfword extraction
 * and assembly.
 */
#define	CHAR_BIT	8
#define	QUAD_BITS	(sizeof(int64_t) * CHAR_BIT)
#define	LONG_BITS	(sizeof(long) * CHAR_BIT)
#define	HALF_BITS	(sizeof(long) * CHAR_BIT / 2)

/*
 * Extract high and low shortwords from longword, and move low shortword of
 * longword to upper half of long, i.e., produce the upper longword of
 * ((int64_t)(x) << (number_of_bits_in_long/2)).  (`x' must actually be u_long.)
 *
 * These are used in the multiply code, to split a longword into upper
 * and lower halves, and to reassemble a product as a int64_t, shifted left
 * (sizeof(long)*CHAR_BIT/2).
 */
#define	HHALF(x)	((x) >> HALF_BITS)
#define	LHALF(x)	((x) & ((1 << HALF_BITS) - 1))
#define	LHUP(x)		((x) << HALF_BITS)

typedef unsigned int	qshift_t;

int64_t		__ashldi3(int64_t, qshift_t);
int64_t		__ashrdi3(int64_t, qshift_t);
int64_t		__divdi3(int64_t a, int64_t b);
int64_t		__lshrdi3(int64_t, qshift_t);
int64_t		__moddi3(int64_t a, int64_t b);
uint64_t	__qdivrem(uint64_t u, uint64_t v, uint64_t *rem);
uint64_t	__udivdi3(uint64_t a, uint64_t b);
uint64_t	__umoddi3(uint64_t a, uint64_t b);
int		__ucmpdi2(uint64_t a, uint64_t b);

#endif /* !_LIBKERN_QUAD_H_ */
