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

	.ident	"@(#)strchr.s"

#include <sys/asm_linkage.h>

/*
 * char *
 * strrchr(const char *, int)
 *    indata
 *     r0 : dist ptr
 *     r1 : search character
 *    outdata
 *     r0 : find charcter ptr
 */

	ENTRY(strrchr)
	stmdb	sp!, {r4, r5, r6, r7, r8, lr}	
					/* stack pointer is 8byte alignment */
	mov	r6, #0			/* return value initialize */
	cmp	r0, #0
	beq	.Lend
	and	r1, r1, #0xff
	ands	ip, r0, #CLONGMASK	/* is "s" aligned? */
	beq	.Lword
	ldrb	r3, [r0]
	cmp	r3, r1
	moveq	r6, r0
	cmp	r3, #0
	beq	.Lend
	add	r0, r0, #1
	cmp	ip, #3
	beq	.Lword
	ldrb	r3, [r0]
	cmp	r3, r1	
	moveq	r6, r0	
	cmp	r3, #0
	beq	.Lend
	add	r0, r0, #1
	cmp	ip, #2
	beq	.Lword
	ldrb	r3, [r0]
	cmp	r3, r1
	moveq	r6, r0
	cmp	r3, #0
	beq	.Lend
	add	r0, r0, #1
.Lword:
	ldr	r4, .L01
	ldr	r5, .L80
	orr	ip, r1, r1, lsl #8
	orr	r3, ip, ip, lsl #16
.Lword_loop:			/* word alignement. So find is word */
	ldr	lr, [r0]	/* load word */
	mov	r2, lr
	sub	ip, lr, r4	/* word - 0x01010101 */
	mvn	lr, lr
	and	lr, lr, r5	/* ~word & 0x80808080 */
	ands	lr, lr, ip	/* (~word & 0x80808080) & (word - 0x01010101) */
	bne	.L_has_zero	/* this word include '\0' */
	eor	lr, r2, r3
	sub	ip, lr, r4
	mvn	lr, lr
	and	lr, lr, r5
	add	r0, r0, #4
	ands	lr, lr, ip
	beq	.Lword_loop	/* not find char or '\0' */
	sub	r0, r0, #4
	ldrb	r8, [r0]
	cmp	r8, r1
	moveq	r6, r0
	add	r0, r0, #1
	ldrb	r8, [r0]
	cmp	r8, r1
	moveq	r6, r0
	add	r0, r0, #1
	ldrb	r8, [r0]
	cmp	r8, r1
	moveq	r6, r0
	add	r0, r0, #1
	ldrb	r8, [r0]
	cmp	r8, r1
	moveq	r6, r0
	add	r0, r0, #1
	b	.Lword_loop
.L_has_zero:			/* include null byte */
	ldrb	r2, [r0]
	cmp	r2, r1
	moveq	r6, r0
	cmp	r2, #0
	beq	.Lend
	add	r0, r0, #1
	ldrb	r2, [r0]
	cmp	r2, r1
	moveq	r6, r0
	cmp	r2, #0
	beq	.Lend
	add	r0, r0, #1
	ldrb	r2, [r0]
	cmp	r2, r1
	moveq	r6, r0
	cmp	r2, #0
	beq	.Lend
	add	r0, r0, #1
	ldrb	r2, [r0]
	cmp	r2, r1
	moveq	r6, r0
.Lend:
	mov	r0, r6
	ldmia	sp!, {r4, r5, r6, r7, r8, pc}
.L01:
	.word	0x01010101
.L80:
	.word	0x80808080
	SET_SIZE(strrchr)
