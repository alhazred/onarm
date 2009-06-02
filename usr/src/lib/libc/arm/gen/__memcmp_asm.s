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

	.ident	"@(#)memcmp_asm.s"
	
#include <sys/asm_linkage.h>
	
/*
 * void *
 * __memcmp_asm(const unsigned char *, const unsigned char *, size_t)
 *    indata
 *     r0 : search ptr 1
 *     r1 : search ptr 2
 *     r2 : len
 *    outdata
 *     r0 : cmp value
 */

	ENTRY(__memcmp_asm)
	stmdb	sp!, {r4, lr}
	ands	ip, r1, #CLONGMASK	/* is "s" aligned? */
	beq	.Lword_loop
	ldrb	r3, [r0], #1
	ldrb	r4, [r1], #1
	sub	r2, r2, #1		/* len -- */
	subs	lr, r3, r4
	bne	.Lend
	cmp	ip, #3
	beq	.Lword_loop
	ldrb	r3, [r0], #1
	ldrb	r4, [r1], #1
	sub	r2, r2, #1		/* len -- */
	subs	lr, r3, r4
	bne	.Lend
	cmp	ip, #2
	beq	.Lword_loop
	ldrb	r3, [r0], #1
	ldrb	r4, [r1], #1
	sub	r2, r2, #1		/* len -- */
	subs	lr, r3, r4
	bne	.Lend
.Lword_loop:
	cmp	r2, #4
	blt	.Lbyte
	ldr	r3, [r0]
	ldr	r4, [r1]
	rev	r3, r3			/* cmp check is left char first */
	rev	r4, r4			/* cmp check is left char first */
	subs	lr, r3, r4
	bne	.Lbyte_loop
	add	r0, r0, #4
	add	r1, r1, #4
	subs	r2, r2, #4		/* len = len - 4 */
	b	.Lword_loop
.Lbyte:
	cmp	r2, #0
	beq	.Lend
.Lbyte_loop:
	ldrb	r3, [r0], #1
	ldrb	r4, [r1], #1
	subs	lr, r3, r4
	bne	.Lend
	subs	r2, r2, #1		/* len -- */
	bgt	.Lbyte_loop
.Lend:
	mov	r0, lr
	ldmia	sp!, {r4, pc}
	SET_SIZE(__memcmp_asm)
