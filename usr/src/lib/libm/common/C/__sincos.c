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

#pragma ident	"@(#)__sincos.c	1.15	06/01/23 SMI"

/* INDENT OFF */
/*
 * double __k_sincos(double x, double y, double *c);
 * kernel sincos function on [-pi/4, pi/4], pi/4 ~ 0.785398164
 * Input x is assumed to be bounded by ~pi/4 in magnitude.
 * Input y is the tail of x.
 * return sin(x) with *c = cos(x)
 *
 * Accurate Table look-up algorithm by K.C. Ng, May, 1995.
 *
 * 1. Reduce x to x>0 by sin(-x)=-sin(x),cos(-x)=cos(x).
 * 2. For 0<= x < pi/4, let i = (64*x chopped)-10. Let d = x - a[i], where
 *    a[i] is a double that is close to (i+10.5)/64 and such that
 *    sin(a[i]) and cos(a[i]) is close to a double (with error less
 *    than 2**-8 ulp). Then
 *	cos(x) = cos(a[i]+d) = cos(a[i])cos(d) - sin(a[i])*sin(d)
 *	       = TBL_cos_a[i]*(1+QQ1*d^2+QQ2*d^4) -
 *			TBL_sin_a[i]*(d+PP1*d^3+PP2*d^5)
 *	       = TBL_cos_a[i] + (TBL_cos_a[i]*d^2*(QQ1+QQ2*d^2) -
 *			TBL_sin_a[i]*(d+PP1*d^3+PP2*d^5))
 *	sin(x) = sin(a[i]+d) = sin(a[i])cos(d) + cos(a[i])*sin(d)
 *	       = TBL_sin_a[i]*(1+QQ1*d^2+QQ2*d^4) +
 *			TBL_cos_a[i]*(d+PP1*d^3+PP2*d^5)
 *	       = TBL_sin_a[i] + (TBL_sin_a[i]*d^2*(QQ1+QQ2*d^2) +
 *			TBL_cos_a[i]*(d+PP1*d^3+PP2*d^5))
 *
 *    For |y| less than 10.5/64 = 0.1640625, use
 *	sin(y) = y + y^3*(p1+y^2*(p2+y^2*(p3+y^2*p4)))
 *	cos(y) = 1 + y^2*(q1+y^2*(q2+y^2*(q3+y^2*q4)))
 *
 *    For |y| less than 0.008, use
 *	sin(y) = y + y^3*(pp1+y^2*pp2)
 *	cos(y) = 1 + y^2*(qq1+y^2*qq2)
 *
 * Accuracy:
 *	TRIG(x) returns trig(x) nearly rounded (less than 1 ulp)
 */

#include "libm.h"

static const double sc[] = {
/* ONE	= */  1.0,
/* NONE	= */ -1.0,
/*
 * |sin(x) - (x+pp1*x^3+pp2*x^5)| <= 2^-58.79 for |x| < 0.008
 */
/* PP1	= */ -0.166666666666316558867252052378889521480627858683055567,
/* PP2	= */   .008333315652997472323564894248466758248475374977974017927,
/*
 * |(sin(x) - (x+p1*x^3+...+p4*x^9)|
 * |------------------------------ | <= 2^-57.63 for |x| < 0.1953125
 * |                 x             |
 */
/* P1  	= */ -1.666666666666629669805215138920301589656e-0001,
/* P2  	= */  8.333333332390951295683993455280336376663e-0003,
/* P3  	= */ -1.984126237997976692791551778230098403960e-0004,
/* P4  	= */  2.753403624854277237649987622848330351110e-0006,
/*
 * |cos(x) - (1+qq1*x^2+qq2*x^4)| <= 2^-55.99 for |x| <= 0.008 (0x3f80624d)
 */
/* QQ1	= */ -0.4999999999975492381842911981948418542742729,
/* QQ2	= */  0.041666542904352059294545209158357640398771740,
/*
 * |cos(x) - (1+q1*x^2+...+q4*x^8)| <= 2^-55.86 for |x| <= 0.1640625 (10.5/64)
 */
/* Q1  	= */ -0.5,
/* Q2  	= */  4.166666666500350703680945520860748617445e-0002,
/* Q3  	= */ -1.388888596436972210694266290577848696006e-0003,
/* Q4  	= */  2.478563078858589473679519517892953492192e-0005,
};
/* INDENT ON */

#define	ONE	sc[0]
#define	NONE	sc[1]
#define	PP1	sc[2]
#define	PP2	sc[3]
#define	P1	sc[4]
#define	P2	sc[5]
#define	P3	sc[6]
#define	P4	sc[7]
#define	QQ1	sc[8]
#define	QQ2	sc[9]
#define	Q1	sc[10]
#define	Q2	sc[11]
#define	Q3	sc[12]
#define	Q4	sc[13]

extern const double _TBL_sincos[], _TBL_sincosx[];

double
__k_sincos(double x, double y, double *c) {
	double	z, w, s, v, p, q;
	int	i, j, n, hx, ix;

	hx = ((int *)&x)[HIWORD];
	ix = hx & ~0x80000000;

	if (ix <= 0x3fc50000) {	/* |x| < 10.5/64 = 0.164062500 */
		if (ix < 0x3e400000) {	/* |x| < 2**-27 */
			if ((int)x == 0)
				*c = ONE;
			return (x + y);
		} else {
			z = x * x;
			if (ix < 0x3f800000) {	/* |x| < 0.008 */
				q = z * (QQ1 + z * QQ2);
				p = (x * z) * (PP1 + z * PP2) + y;
			} else {
				q = z * ((Q1 + z * Q2) + (z * z) * (Q3 +
				    z * Q4));
				p = (x * z) * ((P1 + z * P2) + (z * z) * (P3 +
				    z * P4)) + y;
			}
			*c = ONE + q;
			return (x + p);
		}
	} else {		/* 0.164062500 < |x| < ~pi/4 */
		n = ix >> 20;
		i = (((ix >> 12) & 0xff) | 0x100) >> (0x401 - n);
		j = i - 10;
		if (hx < 0)
			v = -y - (_TBL_sincosx[j] + x);
		else
			v = y - (_TBL_sincosx[j] - x);
		s = v * v;
		j <<= 1;
		w = _TBL_sincos[j];
		z = _TBL_sincos[j+1];
		p = s * (PP1 + s * PP2);
		q = s * (QQ1 + s * QQ2);
		p = v + v * p;
		*c = z - (w * p - z * q);
		s = w * q + z * p;
		return ((hx >= 0)? w + s : -(w + s));
	}
}
