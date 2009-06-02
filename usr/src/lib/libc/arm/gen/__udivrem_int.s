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

/* int(64) - int(64)  divide and remainder			*/
/* result    divide is  r0					*/
/*           remainder is [sp]  by 8byte memory			*/

	.file	"__udivrem_int.s"

#include <sys/asm_linkage.h>

/* input data */
/*  r0,r1 - dividend */
/*  r2,r3 - divisor( 32bit so r3=0  */
/*  r1 < r2 */
/* [sp]   - result remainder */
/* divide <= 32bit */
		
	ENTRY(__udivrem_int)
	stmdb	sp!, {r4, r5, r6, r7, r8, lr}
	clz	ip, r1		/* count of zero at bit position of	*/
				/*  most significant'1' : devidend	*/
	rsb	r4, ip, #32	/* hiword loopcnt			*/
	clz	r8, r2		/* count of zero at bit position of	*/
				/*  most significant'1' : divisor	*/
	sub	r5, ip, r8
	rsb	ip, r5, #32
	mov	lr, #0		/* lr is final result of divide. first is 0 */
.L_loop:
				/* lcnt = clz(dividend)-clz(divisor)	*/
	mov	r7, #0		/* for( (lcnt > 0 ) {			*/
	orr	r7, r7, r2, lsr r5	
	mov	r6, r2, asl ip	/*   devide <<= 1			*/
	mov	lr, lr, asl #1	/*   if (dividend >= (divisor*(2**lcnt))) */
	cmp	r1, r7		/*   {					*/
	bcc	.L_notdivide	/*	 dividend -= (divisor*(2**lcnt)) */
	bne	.L_divide	/*	 divide += 1			*/
	cmp	r0, r6		/*   }					*/
	bcc	.L_notdivide	/*   lcnt -= 1				*/
.L_divide:			/* }					*/
	subs	r0, r0, r6
	sbc	r1, r1, r7
	add	lr, lr, #1
.L_notdivide:
	sub	r4, r4, #1
	add	r5, r5, #1
	sub	ip, ip, #1
	cmn	r4, #1
	bne	.L_loop
	cmp	r8, #0
	beq	.Lend
	sub	r8, r8, #1
.Llow_loop:
				/* lcnt = clz(dividend)-clz(divisor)	*/
	mov	r6, r2, asl r8	/* for( (lcnt > 0 )			*/
	mov	lr, lr, asl #1	/* {					*/
	cmp	r6, r0		/*    devide <<= 1			*/
	sub	r8, r8, #1	/*    if ( dividend >= (divisor*(2**lcnt)) ) */
	rsbls	r0, r6, r0	/*    {					*/
	addls	lr, lr, #1	/*	  dividend -= (divisor*(2**lcnt)) */
	cmn	r8, #1		/*	  divide += 1			*/
	bne	.Llow_loop	/*    }					*/
				/*    lcnt -= 1				*/
				/* }					*/
.Lend:	
	mov	r2, lr		/* return value set : divide		*/
	ldmia	sp!, {r4, r5, r6, r7, r8, lr}
	ldr	r3, [sp]
	stmia	r3, {r0, r1}
	mov	r0, r2		/* return value set : divide		*/
	bx	lr
	SET_SIZE(__udivrem_int)
