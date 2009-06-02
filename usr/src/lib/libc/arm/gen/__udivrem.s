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
 * Copyright (c) 2006-2008 NEC Corporation
 * All rights reserved.
 */

/* int(32) - int(32)  divide and remainder			*/
/* result    divide is  r0					*/
/*           remainder is r1					*/

	.file	"__udivrem.s"

#include <sys/asm_linkage.h>
#if defined(__ARM_EABI__)
#include "synonyms.h"

	ANSI_PRAGMA_WEAK2(__aeabi_uidivmod,__udivrem,function)
#endif

	ENTRY(__udivrem)
	cmp	r1, #1		
	moveq	r1, #0		/* remainder = 0			*/
	bxeq	lr		/* return				*/
	clz	r2, r0		/* count of zero at bit position of	*/
				/*  most significant'1' : devidend	*/
	clz	r3, r1		/* count of zero at bit position of	*/
				/*  most significant'1' : divisor	*/
	subs	r3, r3, r2	/* cnt set				*/
	mov	ip, r0		/* devidend set. final result is remainder */
	mov	r0, #0		/* r0 is final result of divide. first is 0 */
	bmi	.Lend		/* dividend < divisor  goto end		*/
	cmp	r3, #1
	bgt	.L_arrayjmp	/* r3 <= 1 not use array jmp. */
	bne	.L_0
	cmp	ip, r1, asl #1	/* r3 == 1 */
	subhs	ip, ip, r1, asl #1
	addhs	r0, r0, #0x00000002
.L_0:				/* r3 == 0 */
	cmp	ip, r1
	subhs	ip, ip, r1
	addhs	r0, r0, #0x00000001
	mov	r1, ip		/* return value set : remainder	*/
	bx	lr		/* return */
.L_arrayjmp:
	rsb	r2, r3, #31
	add	r2, r2, r2, lsl #1	/* r2 = r2 + (r2 * 2) */
	add	r2, pc, r2, lsl #2	/* r2 = pc + (r2 * 4) */
	mov	pc, r2
.Lshift_31:
	cmp	ip, r1, asl #31
	subhs	ip, ip, r1, asl #31
	addhs	r0, r0, #0x80000000
.Lshift_30:
	cmp	ip, r1, asl #30
	subhs	ip, ip, r1, asl #30
	addhs	r0, r0, #0x40000000
.Lshift_29:
	cmp	ip, r1, asl #29
	subhs	ip, ip, r1, asl #29
	addhs	r0, r0, #0x20000000
.Lshift_28:
	cmp	ip, r1, asl #28
	subhs	ip, ip, r1, asl #28
	addhs	r0, r0, #0x10000000
.Lshift_27:
	cmp	ip, r1, asl #27
	subhs	ip, ip, r1, asl #27
	addhs	r0, r0, #0x08000000
.Lshift_26:
	cmp	ip, r1, asl #26
	subhs	ip, ip, r1, asl #26
	addhs	r0, r0, #0x04000000
.Lshift_25:
	cmp	ip, r1, asl #25
	subhs	ip, ip, r1, asl #25
	addhs	r0, r0, #0x02000000
.Lshift_24:
	cmp	ip, r1, asl #24
	subhs	ip, ip, r1, asl #24
	addhs	r0, r0, #0x01000000
.Lshift_23:
	cmp	ip, r1, asl #23
	subhs	ip, ip, r1, asl #23
	addhs	r0, r0, #0x00800000
.Lshift_22:
	cmp	ip, r1, asl #22
	subhs	ip, ip, r1, asl #22
	addhs	r0, r0, #0x00400000
.Lshift_21:
	cmp	ip, r1, asl #21
	subhs	ip, ip, r1, asl #21
	addhs	r0, r0, #0x00200000
.Lshift_20:
	cmp	ip, r1, asl #20
	subhs	ip, ip, r1, asl #20
	addhs	r0, r0, #0x00100000
.Lshift_19:
	cmp	ip, r1, asl #19
	subhs	ip, ip, r1, asl #19
	addhs	r0, r0, #0x00080000
.Lshift_18:
	cmp	ip, r1, asl #18
	subhs	ip, ip, r1, asl #18
	addhs	r0, r0, #0x00040000
.Lshift_17:
	cmp	ip, r1, asl #17
	subhs	ip, ip, r1, asl #17
	addhs	r0, r0, #0x00020000
.Lshift_16:
	cmp	ip, r1, asl #16
	subhs	ip, ip, r1, asl #16
	addhs	r0, r0, #0x00010000
.Lshift_15:
	cmp	ip, r1, asl #15
	subhs	ip, ip, r1, asl #15
	addhs	r0, r0, #0x00008000
.Lshift_14:
	cmp	ip, r1, asl #14
	subhs	ip, ip, r1, asl #14
	addhs	r0, r0, #0x00004000
.Lshift_13:
	cmp	ip, r1, asl #13
	subhs	ip, ip, r1, asl #13
	addhs	r0, r0, #0x00002000
.Lshift_12:
	cmp	ip, r1, asl #12
	subhs	ip, ip, r1, asl #12
	addhs	r0, r0, #0x00001000
.Lshift_11:
	cmp	ip, r1, asl #11
	subhs	ip, ip, r1, asl #11
	addhs	r0, r0, #0x00000800
.Lshift_10:
	cmp	ip, r1, asl #10
	subhs	ip, ip, r1, asl #10
	addhs	r0, r0, #0x00000400
.Lshift_9:
	cmp	ip, r1, asl #9
	subhs	ip, ip, r1, asl #9
	addhs	r0, r0, #0x00000200
.Lshift_8:
	cmp	ip, r1, asl #8
	subhs	ip, ip, r1, asl #8
	addhs	r0, r0, #0x00000100
.Lshift_7:
	cmp	ip, r1, asl #7
	subhs	ip, ip, r1, asl #7
	addhs	r0, r0, #0x00000080
.Lshift_6:
	cmp	ip, r1, asl #6
	subhs	ip, ip, r1, asl #6
	addhs	r0, r0, #0x00000040
.Lshift_5:
	cmp	ip, r1, asl #5
	subhs	ip, ip, r1, asl #5
	addhs	r0, r0, #0x00000020
.Lshift_4:
	cmp	ip, r1, asl #4
	subhs	ip, ip, r1, asl #4
	addhs	r0, r0, #0x00000010
.Lshift_3:
	cmp	ip, r1, asl #3
	subhs	ip, ip, r1, asl #3
	addhs	r0, r0, #0x00000008
.Lshift_2:
	cmp	ip, r1, asl #2
	subhs	ip, ip, r1, asl #2
	addhs	r0, r0, #0x00000004
.Lshift_1:
	cmp	ip, r1, asl #1
	subhs	ip, ip, r1, asl #1
	addhs	r0, r0, #0x00000002
.Lshift_0:
	cmp	ip, r1
	subhs	ip, ip, r1
	addhs	r0, r0, #0x00000001
.Lend:
	mov	r1, ip		/* return value set : remainder	*/
	bx	lr
	SET_SIZE(__udivrem)
