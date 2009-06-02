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

	.ident	"@(#)memccpy_asm.s"

#include <sys/asm_linkage.h>

/*
 * unsigned char *
 * __memccpy_asm(unsigned char *, const unsigned char *, int, size_t)
 *    indata
 *     r0 : dist ptr
 *     r1 : src ptr
 *     r2 : find character
 *     r3 : len
 *    outdata
 *     r0 : after found character ptr
 */

	ENTRY(__memccpy_asm)
	stmdb	sp!, {r4, r5, r6, r7, r8, lr}
	and	r2, r2, #0xff
	ands	ip, r0, #CLONGMASK	/* is "s" aligned? */
	beq	.Lword
	ldrb	lr, [r1], #1
	strb	lr, [r0], #1
	cmp	lr, r2
	beq	.Lend
	sub	r3, r3, #1		/* len-- */
	cmp	ip, #3
	beq	.Lword
	ldrb	lr, [r1], #1
	strb	lr, [r0], #1
	cmp	lr, r2
	beq	.Lend
	sub	r3, r3, #1		/* len-- */
	cmp	ip, #2
	beq	.Lword
	ldrb	lr, [r1], #1
	strb	lr, [r0], #1
	cmp	lr, r2
	beq	.Lend
	sub	r3, r3, #1		/* len-- */
.Lword:
	ldr	r4, .L01
	ldr	r5, .L80
	orr	ip, r2, r2, lsl #8
	orr	r6, ip, ip, lsl #16
.Lword_loop:				/* word alignement. So find is word */
	cmp	r3, #4
	blt	.Lbyte
	ldr	ip, [r1]	/* load word */
	eor	lr, ip, r6
	sub	ip, lr, r4	/* word - 0x01010101 */
	mvn	lr, lr		
	and	lr, lr, r5	/* ~word & 0x80808080 */
	ands	lr, lr, ip	/* (~word & 0x80808080) & (word - 0x01010101) */
	bne	.Lbyte_loop	/* this word is  includes check char */
	sub	r3, r3, #4
	ldr	r7, [r1], #4
	str	r7, [r0], #4
	b	.Lword_loop
.Lbyte:
	cmp	r3, #0
	beq	.Lerror
.Lbyte_loop:				/* byte check loop */
	ldrb	r7, [r1], #1
	strb	r7, [r0], #1
	cmp	r7, r2
	beq	.Lend
	subs	r3, r3, #1
	bgt	.Lbyte_loop
.Lerror:
	mov	r0, #0			/* return value is NULL */
.Lend:
	ldmia	sp!, {r4, r5, r6, r7, r8, pc}
.L01:
	.word	0x01010101
.L80:
	.word	0x80808080
	SET_SIZE(__memccpy_asm)
