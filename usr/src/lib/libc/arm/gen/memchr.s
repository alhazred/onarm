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

	.ident	"@(#)memchr.s"

#include <sys/asm_linkage.h>

/*
 * void *
 * memchr(const char *, int ,size_t)
 *    indata
 *     r0 : dist ptr
 *     r1 : find character
 *     r2 : len
 *    outdata
 *     r0 : find charcter ptr
 */

	ENTRY(memchr)
	stmdb	sp!, {r4, r5, r6, lr}	/* stack pointer is 8byte alignment */
	cmp	r0, #0
	beq	.Lend
	and	r1, r1, #0xff
	cmp	r2, #20			/* len <= 20 is byte loop */
	blt	.Lbyte
	ands	ip, r0, #CLONGMASK	/* is "s" aligned? */
	beq	.Lword
	ldrb	r3, [r0]
	cmp	r3, r1
	beq	.Lend
	add	r0, r0, #1
	sub	r2, r2, #1		/* len-- */
	cmp	ip, #3
	beq	.Lword
	ldrb	r3, [r0]
	cmp	r3, r1
	beq	.Lend
	add	r0, r0, #1
	sub	r2, r2, #1		/* len-- */
	cmp	ip, #2
	beq	.Lword
	ldrb	r3, [r0]
	cmp	r3, r1
	beq	.Lend
	add	r0, r0, #1
	sub	r2, r2, #1		/* len-- */
.Lword:
	ldr	r4, .L01
	ldr	r5, .L80
	orr	ip, r1, r1, lsl #8
	orr	r3, ip, ip, lsl #16
.Lword_loop:			/* word alignement. So find is word */
	cmp	r2, #4
	blt	.Lbyte
	ldr	ip, [r0]	/* load word */
	eor	lr, ip, r3
	sub	ip, lr, r4	/* word - 0x01010101 */
	mvn	lr, lr		/* ~word & 0x80808080 */
	and	lr, lr, r5	/* (~word & 0x80808080) & (word - 0x01010101) */
	sub	r2, r2, #4
	add	r0, r0, #4
	ands	lr, lr, ip
	beq	.Lword_loop
	sub	r0, r0, #4	/* this word include serach character */
	mov	r2, #4		/* last 4byte check */
	b	.Lbyte_loop
.Lbyte:
	cmp	r2, #0
	beq	.Lerror
.Lbyte_loop:			/* byte find loop */
	ldrb	r3, [r0]
	cmp	r3, r1
	beq	.Lend
	add	r0, r0, #1
	subs	r2, r2, #1
	bgt	.Lbyte_loop
.Lerror:
	mov	r0, #0		/* return value is NULL */
.Lend:
	ldmia	sp!, {r4, r5, r6, pc}
.L01:
	.word	0x01010101
.L80:
	.word	0x80808080
	SET_SIZE(memchr)
