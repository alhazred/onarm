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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2006-2007 NEC Corporation
 */

#pragma ident	"@(#)_div64.c"

#include <stdio.h>

#include <sys/asm_linkage.h>
#include <sys/syscall.h>
#include <sys/trap.h>
#include <sys/errno.h>

/*
 * C support for 64-bit modulo and division.
 * Hand-customized compiler output - see comments for details.
 */

/*
 * int32_t/int64_t division/manipulation
 *
 * Hand-customized compiler output: the non-GCC entry points depart from
 * the SYS V ABI by requiring their arguments to be popped, and in the
 * [u]divrem64 cases returning the remainder in %ecx:%esi. Note the
 * compiler-generated use of %r1:%r0 for the first argument of
 * internal entry points.
 *
 * Inlines for speed:
 * - counting the number of leading zeros in a word
 * - multiplying two 32-bit numbers giving a 64-bit result
 * - dividing a 64-bit number by a 32-bit number, giving both quotient
 *	and remainder
 * - subtracting two 64-bit results
 */
#define	LO(X)		((uint32_t)(X) & 0xffffffff)
#define	HI(X)		((uint32_t)((X) >> 32) & 0xffffffff)
#define	HILO(H, L)	(((uint64_t)(H) << 32) + (L))

/* give index of highest bit */
#define MSB	(unsigned int)0x80000000
#define	HIBIT(a, r)					\
	( {						\
		int	i;				\
		for (i = 0; i <=32; i++) {		\
		if ((a & (MSB >> i)) || (i == 32))	\
			break;				\
		}					\
		r = i;					\
	} )

/* multiply two uint32_ts resulting in a uint64_t */
#define       A_MUL32(a, b, lo, hi) \
     __asm__("umull %0,%1,%2,%3" \
       : "=&r"((unsigned)(lo)), "=r"((unsigned)(hi)): "r"(b), "r"(a))
 
/* subtract two uint64_ts (with borrow) */
#define A_SUB2(bl, bh, al, ah) \
    __asm__("subs %0,%0,%4\n\tsbc %1,%1,%5" \
        : "=&r"((unsigned)(al)), "=r"((unsigned)(ah)) \
        : "0"((unsigned)(al)), "1"((unsigned)(ah)), "r"(bl), \
        "r"(bh))

/* zero divide  error output */
#define       ZERO_DIV \
     __asm__("bkpt  #0x12")
 
typedef union {
	uint64_t	rtbuf;
	struct {
		uint32_t	u_d;
		uint32_t	u_r;
	} u_res;
} _udiv_t;

extern void  __zero_div(void) ;
extern uint64_t __udivrem(uint32_t, uint32_t);
extern unsigned int __udivrem_ll(uint64_t, uint64_t, uint64_t *);
extern unsigned int __udivrem_int(uint64_t, uint64_t, uint64_t *);

/*
 * Unsigned division with remainder.
 * Divide two uint64_ts, and calculate remainder.
 */
uint64_t
UDivRem(uint64_t x, uint64_t y, uint64_t * pmod)
{
	_udiv_t	result;
	uint32_t  val1,val2;
	uint32_t  q0, q1;
	uint64_t  wmod;

	if ((val1=HI(x)) == (val2=HI(y))) {
		if (val1 == 0) {
			result.rtbuf = __udivrem(LO(x),LO(y));
			*pmod = (uint64_t)result.u_res.u_r ;
			return( (uint64_t)result.u_res.u_d ) ;
		} else {
			if ((val1 = LO(x)) >= (val2 = LO(y))) {
				*pmod = (uint64_t)(val1 - val2) ;
				return((uint64_t)1) ;
			} else {
				*pmod = x;
				return ((uint64_t)0);
			}
		}
	}
	if (val1 < val2) {
		/* HI(x) < HI(y) => x < y => result is 0 */
		/* return remainder */
		*pmod = x;
 		/* return result */
 		return ((uint64_t)0);
	}
	if ((val1 > val2) && (val2 != 0)) {
		return((uint64_t)__udivrem_ll(x,y,pmod));
	}
	if (val1 < LO(y)) {
		return((uint64_t)__udivrem_int(x,y,pmod)) ;
	} else {
		result.rtbuf = __udivrem(val1,LO(y));
		wmod = HILO(result.u_res.u_r,LO(x));
		q1 = __udivrem_int(wmod,LO(y),pmod);
		return((uint64_t)(HILO(result.u_res.u_d,q1)));
	}
}

/*
 * Unsigned division without remainder.
 */
uint64_t
UDiv(uint64_t x, uint64_t y)
{
	_udiv_t	result;
	uint32_t  val1,val2;
	uint32_t  q0, q1;
	uint64_t  wmod,dmymod;

	if ((val1=HI(x)) == (val2=HI(y))) {
		if (val1 == 0) {
			result.rtbuf = __udivrem(LO(x),LO(y));
			return( (uint64_t)result.u_res.u_d ) ;
		} else {
			if (LO(x) >= LO(y)) {
				return((uint64_t)1) ;
			} else {
				return((uint64_t)0);
			}
		}
	}
	if (val1 < val2) {
		/* HI(x) < HI(y) => x < y => result is 0 */
 		/* return result */
 		return ((uint64_t)0);
	}
	if ((val1 > val2) && (val2 != 0)) {
		return((uint64_t)__udivrem_ll(x,y,&dmymod));
	}
	if (val1 < LO(y)) {
		return((uint64_t)__udivrem_int(x,y,&dmymod)) ;
	} else {
		result.rtbuf = __udivrem(val1,LO(y));
		wmod = HILO(result.u_res.u_r,LO(x));
		q1 = __udivrem_int(wmod,LO(y),&dmymod);
		return((uint64_t)(HILO(result.u_res.u_d,q1)));
	}
}

/*
 * __udiv64
 *
 * Perform division of two unsigned 64-bit quantities, returning the
 * quotient in %r1:%r0  __udiv64 pops the arguments on return,
 */
uint64_t
__udiv64(uint64_t x, uint64_t y)
{
	uint64_t	r;

	if (y == 0) {
		ZERO_DIV;
		return ((uint64_t)0);
	}
	r = UDiv(x, y);
	return (r);
}

/*
 * __urem64
 *
 * Perform division of two unsigned 64-bit quantities, returning the
 * remainder in %r1:%r0.  __urem64 pops the arguments on return
 */
uint64_t
__urem64(uint64_t x, uint64_t y)
{
 	uint64_t	rem;

	if (y == 0) {
		ZERO_DIV;
		return ((uint64_t)0);
	}
 	(void) UDivRem(x, y, &rem);
	return (rem);
}

/*
 * __div64
 *
 * Perform division of two signed 64-bit quantities, returning the
 * quotient in %r1:%r0.  __div64 pops the arguments on return.
 */
int64_t
__div64(int64_t x, int64_t y)
{
 	int		negative;
 	uint64_t	xt, yt, r;

	if (y == 0) {
		ZERO_DIV;
		return ((int64_t)0);
	}
 	if (x < 0) {
 		xt = -(uint64_t) x;
 		negative = 1;
 	} else {
 		xt = x;
 		negative = 0;
 	}
 	if (y < 0) {
 		yt = -(uint64_t) y;
 		negative ^= 1;
 	} else {
 		yt = y;
 	}
 	r = UDiv(xt, yt);
 	return (negative ? (int64_t) - r : r);
}

/*
 * __rem64
 *
 * Perform division of two signed 64-bit quantities, returning the
 * remainder in %r1:%r0.  __rem64 pops the arguments on return.
 */
int64_t
__rem64(int64_t x, int64_t y)
{
 	uint64_t	xt, yt, rem;

	if (y == 0) {
		ZERO_DIV;
		return ((int64_t)0);
	}
 	if (x < 0) {
 		xt = -(uint64_t) x;
 	} else {
 		xt = x;
 	}
 	if (y < 0) {
 		yt = -(uint64_t) y;
 	} else {
 		yt = y;
 	}
 	(void) UDivRem(xt, yt, &rem);
 	return (x < 0 ? (int64_t) - rem : rem);
}

/*
 * __udivrem64
 *
 * Perform division of two unsigned 64-bit quantities, returning the
 * quotient in %r1:%r0, and the remainder in %ecx:%esi.  __udivrem64
 * pops the arguments on return.
 */
uint64_t
__udivrem64(uint64_t x, uint64_t y, uint64_t *pmod)
{
	uint64_t	r;

	if (y == 0) {
		ZERO_DIV;
		*pmod = x;
		return ((uint64_t)0);
	}
	r = UDivRem(x, y, pmod);
 	return (r);
}

/*
 * Signed division with remainder.
 */
int64_t
SDivRem(int64_t x, int64_t y, int64_t * pmod)
{
 	int		negative;
 	uint64_t	xt, yt, r, rem;

 	if (x < 0) {
 		xt = -(uint64_t) x;
 		negative = 1;
 	} else {
 		xt = x;
 		negative = 0;
 	}
 	if (y < 0) {
 		yt = -(uint64_t) y;
 		negative ^= 1;
 	} else {
 		yt = y;
 	}
 	r = UDivRem(xt, yt, &rem);
 	*pmod = (x < 0 ? (int64_t) - rem : rem);
 	return (negative ? (int64_t) - r : r);
}

/*
 * __divrem64
 *
 * Perform division of two signed 64-bit quantities, returning the
 * quotient in %r1:%r0, and the remainder in %ecx:%esi.  __divrem64
 * pops the arguments on return.
 */
int64_t
__divrem64(int64_t x, int64_t y, int64_t *pmod)
{
 	int64_t	r;

	if (y == 0) {
		ZERO_DIV;
		*pmod = x;
		return ((int64_t)0);
	}
	r = SDivRem(x, y, pmod);
 	return (r);
}
