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

	.ident	"@(#)strlen.s"

#include <sys/asm_linkage.h>

/*
 * size_t
 * strlen(const char *)
 *    indata
 *     r0 : dist
 *    outdata
 *     r0 : len
 */

	ENTRY(strlen)
	str	lr, [sp, #-8]!		/* stack pointer is 8byte alignment */
	mov	r1, #0
	cmp	r0, #0			/* is "s" null? */
	beq	.Lend			
	ands	ip, r0, #CLONGMASK	/* is "s" aligned? */
	beq	.Lword
					/* work byte-wise until aligned */
	ldrb	r2, [r0, r1]
	cmp	r2, #0			/* is *s == 0 ? */
	beq	.Lend
	add	r1, r1, #1		/* increment length */
	cmp	ip, #3			/* check of 3rd byte */
	beq	.Lword
	ldrb	r2, [r0, r1]
	cmp	r2, #0			/* is *s == 0 ? */
	beq	.Lend
	add	r1, r1, #1		/* increment length */
	cmp	ip, #2			/* check of 2nd byte */
	beq	.Lword
	ldrb	r2, [r0, r1]
	cmp	r2, #0			/* is *s == 0 ? */
	beq	.Lend
	add	r1, r1, #1		/* increment length */
.Lword:	
	ldr	ip, .L01
	ldr	lr, .L80
.Lword_loop:
	ldr	r2, [r0, r1]	/* load word */
	sub	r3, r2, ip	/* word - 0x01010101 */
	mvn	r2, r2
	and	r2, r2, lr	/* ~word & 0x80808080 */
	ands	r2, r2, r3	/* (~word & 0x80808080) & (word - 0x01010101) */
	addeq	r1, r1, #4	/* increment length by 4 */
	beq	.Lword_loop	/* if zero, no null byte found */
	ldrb	r2, [r0, r1]
	cmp	r2, #0		/* is *s == 0 ? */
	beq	.Lend
	add	r1, r1, #1	/* increment length */
	ldrb	r2, [r0, r1]
	cmp	r2, #0		/* is *s == 0 ? */
	beq	.Lend
	add	r1, r1, #1	/* increment length */
	ldrb	r2, [r0, r1]
	cmp	r2, #0		/* is *s == 0 ? */
	addne	r1, r1, #1	/* increment length */
.Lend:
	mov	r0, r1
	ldr	pc, [sp], #8
.L01:
	.word	0x01010101
.L80:
	.word	0x80808080
	SET_SIZE(strlen)
