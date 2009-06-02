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

	.ident	"@(#)powl.s	1.4	06/01/23 SMI"

        .file "powl.s"

/ Special cases:
/
/ x ** 0 is 1
/ 1 ** y is 1				(C99)
/ x ** NaN is NaN
/ NaN ** y (except 0) is NaN
/ x ** 1 is x
/ +-(|x| > 1) **  +inf is +inf
/ +-(|x| > 1) **  -inf is +0
/ +-(|x| < 1) **  +inf is +0
/ +-(|x| < 1) **  -inf is +inf
/ (-1) ** +-inf is +1			(C99)
/ +0 ** +y (except 0, NaN)              is +0
/ -0 ** +y (except 0, NaN, odd int)     is +0
/ +0 ** -y (except 0, NaN)              is +inf (z flag)
/ -0 ** -y (except 0, NaN, odd int)     is +inf (z flag)
/ -0 ** y (odd int)			is - (+0 ** x)
/ +inf ** +y (except 0, NaN)    	is +inf
/ +inf ** -y (except 0, NaN)    	is +0
/ -inf ** +-y (except 0, NaN)   	is -0 ** -+y (NO z flag)
/ x ** -1 is 1/x
/ x ** 2 is x*x
/ -x ** y (an integer) is (-1)**(y) * (+x)**(y)
/ x ** y (x negative & y not integer) is NaN (i flag)

#include "libm.h"
LIBM_ANSI_PRAGMA_WEAK(powl,function)
#include "libm_synonyms.h"
#include "xpg6.h"

#undef fabs

	.data
	.align	16
negzero:
	.float	-0.0
half:
	.float	0.5
one:
	.float	1.0
negone:
	.float	-1.0
two:
	.float	2.0
Snan:
	.4byte	0x7f800001
pinfinity:
	.4byte	0x7f800000
ninfinity:
	.4byte	0xff800000


	ENTRY(powl)
	pushq	%rbp
	movq	%rsp,%rbp
	PIC_SETUP(1)

	fldt	16(%rbp)		/ x
	fxam				/ determine class of x
	fnstsw	%ax			/ store status in %ax
	movb	%ah,%dh			/ %dh <- condition code of x

	fldt	32(%rbp)		/ y , x
	fxam				/ determine class of y
	fnstsw	%ax			/ store status in %ax
	movb	%ah,%dl			/ %dl <- condition code of y

	call	.pow_main		/// LOCAL
	PIC_WRAPUP
	leave
	ret

.pow_main:
	/ x ** 0 is 1
	movb	%dl,%cl
	andb	$0x45,%cl
	cmpb	$0x40,%cl		/ C3=1 C2=0 C1=? C0=0 when +-0
	jne	1f
	fstp	%st(0)			/ x
	fstp	%st(0)			/ stack empty
	fld1				/ 1
	ret

1:	/ y is not zero
	PIC_G_LOAD(movzwq,__xpg6,rax)
	andl	$_C99SUSv3_pow_treats_Inf_as_an_even_int,%eax
	cmpl	$0,%eax
	je	1f

	/ C99: 1 ** anything is 1
	fld1				/ 1, y, x
	fucomip	%st(2),%st		/ y, x
	jp	1f			/ so that pow(NaN1,NaN2) returns NaN2
	jne	1f
	fstp	%st(0)			/ x
	ret

1:
	/ x ** NaN is NaN
	movb	%dl,%cl
	andb	$0x45,%cl
	cmpb	$0x01,%cl		/ C3=0 C2=0 C1=? C0=1 when +-NaN
	jne	1f
	fstp	%st(1)			/ y
	ret

1:	/ y is not NaN
	/ NaN ** y (except 0) is NaN
	movb	%dh,%cl
	andb	$0x45,%cl
	cmpb	$0x01,%cl		/ C3=0 C2=0 C1=? C0=1 when +-NaN
	jne	1f
	fstp	%st(0)			/ x
	ret

1:	/ x is not NaN
	/ x ** 1 is x
	fld1				/ 1, y, x
	fcomip	%st(1),%st		/ y, x
	jne	1f
	fstp	%st(0)			/ x
	ret

1:	/ y is not 1
	/ +-(|x| > 1) **  +inf is +inf
	/ +-(|x| > 1) **  -inf is +0
	/ +-(|x| < 1) **  +inf is +0
	/ +-(|x| < 1) **  -inf is +inf
	/ +-(|x| = 1) ** +-inf is NaN
	movb	%dl,%cl
	andb	$0x47,%cl
	cmpb	$0x05,%cl		/ C3=0 C2=1 C1=0 C0=1 when +inf
	je	.yispinf
	cmpb	$0x07,%cl		/ C3=0 C2=1 C1=1 C0=1 when -inf
	je	.yisninf

	/ +0 ** +y (except 0, NaN)		is +0
	/ -0 ** +y (except 0, NaN, odd int)	is +0
	/ +0 ** -y (except 0, NaN)		is +inf (z flag)
	/ -0 ** -y (except 0, NaN, odd int)	is +inf (z flag)
	/ -0 ** y (odd int)			is - (+0 ** x)
	movb	%dh,%cl
	andb	$0x47,%cl
	cmpb	$0x40,%cl		/ C3=1 C2=0 C1=0 C0=0 when +0
	je	.xispzero
	cmpb	$0x42,%cl		/ C3=1 C2=0 C1=1 C0=0 when -0
	je	.xisnzero

	/ +inf ** +y (except 0, NaN)	is +inf
	/ +inf ** -y (except 0, NaN)	is +0
	/ -inf ** +-y (except 0, NaN)	is -0 ** -+y (NO z flag)
	movb	%dh,%cl
	andb	$0x47,%cl
	cmpb	$0x05,%cl		/ C3=0 C2=1 C1=0 C0=1 when +inf
	je	.xispinf
	cmpb	$0x07,%cl		/ C3=0 C2=1 C1=1 C0=1 when -inf
	je	.xisninf

	/ x ** -1 is 1/x
	flds	PIC_L(negone)		/ -1, y, x
	fcomip	%st(1),%st		/ y, x
	jne	1f
	fld	%st(1)			/ x , y , x
	fdivrs	PIC_L(one)		/ 1/x , y , x
	jmp	.signok			/ check for over/underflow

1:	/ y is not -1
	/ x ** 2 is x*x
	flds	PIC_L(two)		/ 2, y , x
	fcomip	%st(1),%st		/ y, x
	jne	1f
	fld	%st(1)			/ x , y , x
	fld	%st(0)			/ x , x , y , x
	fmul				/ x^2 , y , x
	jmp	.signok			/ check for over/underflow

1:	/ y is not 2
	/ x ** 1/2 is sqrt(x)
	flds	PIC_L(half)		/ 1/2, y , x
	fcomip	%st(1),%st		/ y, x
	jne	1f
	fld	%st(1)			/ x , y , x
	fsqrt				/ sqrt(x) , y , x
	jmp	.signok			/ check for over/underflow

1:	/ y is not 1/2
	/ make copies of x & y
	fld	%st(1)			/ x , y , x
	fld	%st(1)			/ y , x , y , x

	/ -x ** y (an integer) is (-1)**(y) * (+x)**(y)
	/ x ** y (x negative & y not integer) is  NaN
	movl	$0,%ecx			/ track whether to flip sign of result
	fldz				/ 0 , y , x , y , x
	fcomip	%st(2),%st		/ compare 0 with %st(2)
	jb	.merge			/ 0 < x
	/ x < 0
	call	.y_is_int
	cmpl	$0,%ecx
	jne	1f
	/ x < 0 & y != int so x**y = NaN (i flag)
	fstp	%st(0)			/ x , y , x
	fstp	%st(0)			/ y , x
	fstp	%st(0)			/ x
	fstp	%st(0)			/ stack empty
	fldz
	fdiv	%st,%st(0)		/ 0/0
	ret

1:	/ x < 0 & y = int
	fxch				/ x , y , y , x
	fchs				/ px = -x , y , y , x
	fxch				/ y , px , y , x
.merge:
	/ px > 0
	fxch				/ px , y , y , x

	/ x**y   =   exp(y*ln(x))
	fyl2x				/ t=y*log2(px) , y , x
	fld	%st(0)			/ t , t , y , x
	frndint				/ [t] , t , y , x
	fxch				/ t , [t] , y , x
	fucomi	%st(1),%st
	je	1f			/ t is integral
	fsub    %st(1),%st		/ t-[t] , [t] , y , x
	f2xm1				/ 2**(t-[t])-1 , [t] , y , x
	fadds	PIC_L(one)		/ 2**(t-[t]) , [t] , y , x
	fscale				/ 2**t = px**y , [t] , y , x
	jmp	2f
1:
	fstp    %st(0)                  / t=[t] , y , x
	fld1                            / 1 , t , y , x
	fscale                          / 1*2**t = x**y , t , y , x
2:
	fstp	%st(1)			/ x**y , y , x
	cmpl	$1,%ecx
	jne	.signok
	fchs				/ change sign since x<0 & y=-int
.signok:
	fstp	%st(2)			/ y , x**y
	fstp	%st(0)			/ x**y
	ret

/ ------------------------------------------------------------------------

.xispinf:
	fldz
	fcomip	%st(1),%st		/ compare 0 with %st(1)
	jb	.retpinf		/ 0 < y
	jmp	.retpzero		/ y < 0

.xisninf:
	/ -inf ** +-y is -0 ** -+y
	fchs				/ -y , x
	flds	PIC_L(negzero)		/ -0 , -y , x
	fstp	%st(2)			/ -y , -0
	jmp	.xisnzero

.yispinf:
	fld	%st(1)			/ x , y , x
	fabs				/ |x| , y , x
	flds	PIC_L(one)		/ 1 , |x| , y , x
	fcomip	%st(1),%st		/ |x| , y , x
	fstp	%st(0)			/ y , x
	je	.retponeorinvalid	/ x == -1	C99
	jb	.retpinf		/ 1 < |x|
	jmp	.retpzero		/ |x| < 1

.yisninf:
	fld	%st(1)			/ x , y , x
	fabs				/ |x| , y , x
	flds	PIC_L(one)		/ 1 , |x| , y , x
	fcomip	%st(1),%st		/ |x| , y , x
	fstp	%st(0)			/ y , x
	je	.retponeorinvalid	/ x == -1	C99
	jb	.retpzero		/ 1 < |x|
	jmp	.retpinf		/ |x| < 1

.xispzero:
	/ y cannot be 0 or NaN ; stack has	y , x
	fldz				/ 0 , y , x
	fcomip	%st(1),%st		/ compare 0 with %st(1)
	jb	.retpzero		/ 0 < y
	/ x = +0 & y < 0 so x**y = +inf
	jmp	.retpinfzflag		/ ret +inf & z flag

.xisnzero:
	/ y cannot be 0 or NaN ; stack has	y , x
	call	.y_is_int
	cmpl	$1,%ecx
	jne	1f			/ y is not an odd integer
	/ y is an odd integer
	fldz
	fcomip	%st(1),%st		/ compare 0 with %st(1)
	jb	.retnzero		/ 0 < y
	/ x = -0 & y < 0 (odd int)	return -inf (z flag)
	/ x = -inf & y != 0 or NaN	return -inf (NO z flag)
	movb	%dh,%cl
	andb	$0x45,%cl
	cmpb	$0x05,%cl		/ C3=0 C2=1 C1=? C0=1 when +-inf
	je	2f
	fdiv	%st,%st(1)		/ y / x, x (raise z flag)
2:
	fstp	%st(0)			/ x
	fstp	%st(0)			/ stack empty
	flds	PIC_L(ninfinity)	/ -inf
	ret

1:	/ y is not an odd integer
	fldz
	fcomip	%st(1),%st		/ compare 0 with %st(1)
	jb	.retpzero		/ 0 < y
	/ x = -0 & y < 0 (not odd int)	return +inf (z flag)
	/ x = -inf & y not 0 or NaN 	return +inf (NO z flag)
	movb	%dh,%cl
	andb	$0x45,%cl
	cmpb	$0x05,%cl		/ C3=0 C2=1 C1=? C0=1 when +-inf
	jne	.retpinfzflag		/ ret +inf & divide-by-0 flag
	jmp	.retpinf		/ return +inf (NO z flag)

.retpzero:
	fstp	%st(0)			/ x
	fstp	%st(0)			/ stack empty
	fldz				/ +0
	ret

.retnzero:
	fstp	%st(0)			/ x
	fstp	%st(0)			/ stack empty
	flds	PIC_L(negzero)		/ -0
	ret

.retponeorinvalid:
	PIC_G_LOAD(movzwq,__xpg6,rax)
	andl	$_C99SUSv3_pow_treats_Inf_as_an_even_int,%eax
	cmpl	$0,%eax
	je	1f
	fstp	%st(0)			/ x
	fstp	%st(0)			/ stack empty
	fld1				/ 1
	ret

1:
	fstp	%st(0)			/ x
	fstp	%st(0)			/ stack empty
	flds	PIC_L(Snan)		/ Q NaN (i flag)
	fwait
	ret

.retpinf:
	fstp	%st(0)			/ x
	fstp	%st(0)			/ stack empty
	flds	PIC_L(pinfinity)	/ +inf
	ret

.retpinfzflag:
	fstp	%st(0)			/ x
	fstp	%st(0)			/ stack empty
	fldz
	fdivrs	PIC_L(one)		/ 1/0
	ret

/ Set %ecx to 2 if y is an even integer, 1 if y is an odd integer,
/ 0 otherwise.  Assume y is not zero.  Do not raise inexact or modify
/ %edx.
.y_is_int:
	movl	40(%rbp),%eax
	andl	$0x7fff,%eax		/ exponent of y
	cmpl	$0x403f,%eax
	jae	1f			/ |y| >= 2^64, an even int
	cmpl	$0x3fff,%eax
	jb	2f			/ |y| < 1, can't be an int
	movl	%eax,%ecx
	subl	$0x403e,%ecx
	negl	%ecx			/ 63 - unbiased exponent of y
	movq	32(%rbp),%rax
	bsfq	%rax,%rax		/ index of least sig. 1 bit
	cmpl	%ecx,%eax
	jb	2f
	ja	1f
	movl	$1,%ecx
	ret
1:
	movl	$2,%ecx
	ret
2:
	xorl	%ecx,%ecx
	ret
	.align	16
	SET_SIZE(powl)
