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

	.file	"__udivrem_ll.s"

#include <sys/asm_linkage.h>

/* input data */
/*  r0,r1 - dividend */
/*  r2,r3 - divisor  */
/* [sp]   - result remainder */
		
	ENTRY(__udivrem_ll)
	stmdb	sp!, {r4, r5, r6, r7, r8, lr}
	clz	r4, r1		/* count of zero at bit position of	*/
				/*  most significant'1' : devidend	*/
	clz	r5, r3		/* count of zero at bit position of	*/
				/*  most significant'1' : divisor	*/
	subs	r4, r5, r4	/* loopcnt set				*/
	movmi	r5, #0		/* dividend < divisor  divide is 0	*/
	bmi	.L_end		/* dividend < divisor  goto end		*/
	mov	lr, #0		/* lr is final result of divide. first is 0 */
	rsb	ip, r4, #32
.L_loop:
				/* lcnt = clz(dividend)-clz(divisor)	*/
	mov	r7, r3, asl r4	/* for( (lcnt > 0 ) {			*/
	orr	r7, r7, r2, lsr ip
	mov	r6, r2, asl r4	/*   devide <<= 1			*/
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
	add	ip, ip, #1
	cmn	r4, #1
	bne	.L_loop
	mov	r5, lr
.L_end:
	mov	r2, r5
	ldmia	sp!, {r4, r5, r6, r7, r8, lr}
	ldr	r3, [sp]
	stmia	r3, {r0, r1}
	mov	r0, r2		/* return value set : divide		*/
	bx	lr
	SET_SIZE(__udivrem_ll)
