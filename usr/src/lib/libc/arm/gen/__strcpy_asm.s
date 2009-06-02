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

	.ident	"@(#)strcpy_asm.s"

#include <sys/asm_linkage.h>

/*
 * char *
 * __strcpy_asm(char *, const char *)
 *    indata
 *     r0 : dist ptr
 *     r1 : src ptr
 *    outdata
 *     r0 : dist ptr 
 */

	ENTRY(__strcpy_asm)
	stmdb	sp!, {r0, r4, r5, lr}	/* r5 register saved for alignment of sp */
	ands	ip, r1, #CLONGMASK	/* is "dist" aligned? */
	beq	.Lword
					/* work byte-wise until aligned */
	ldrb	r2, [r1], #1
	cmp	r2, #0			/* is *dist == 0 ? */
	strb	r2, [r0], #1
	beq	.Lend
	cmp	ip, #3			/* check of 3rd byte */
	beq	.Lword
	ldrb	r2, [r1], #1
	cmp	r2, #0			/* is *dist == 0 ? */
	strb	r2, [r0], #1
	beq	.Lend
	cmp	ip, #2			/* check of 2nd byte */
	beq	.Lword
	ldrb	r2, [r1], #1
	cmp	r2, #0			/* is *dist == 0 ? */
	strb	r2, [r0], #1
	beq	.Lend
.Lword:	
	ldr	r4, .L01
	ldr	lr, .L80
.Lword_loop:
	ldr	r2, [r1], #4	/* load word */
	sub	r3, r2, r4	/* word - 0x01010101 */
	mov	ip, r2
	mvn	r2, r2
	and	r2, r2, lr	/* ~word & 0x80808080 */
	ands	r2, r2, r3	/* (~word & 0x80808080) & (word - 0x01010101) */
	streq	ip, [r0], #4	/* if zero, stored word */
	beq	.Lword_loop	/* if zero, no null byte found */
				/* "r2" has the data of 3 bytes from 0 bytes */
	sub	r1, r1, #4	/* word has null char. so store is byte */
.Lbyte_loop:
	ldrb	r2, [r1], #1
	cmp	r2, #0		/* is *dist == 0 ? */
	strb	r2, [r0], #1 
	bne	.Lbyte_loop
.Lend:
	ldmia	sp!, {r0, r4, r5, pc}
.L01:
	.word	0x01010101
.L80:
	.word	0x80808080
	SET_SIZE(__strcpy_asm)
