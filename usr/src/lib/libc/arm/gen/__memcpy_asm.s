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

	.ident	"@(#)memcpy_asm.s"
	
#include <sys/asm_linkage.h>
	
/*
 * void
 * __memcpy_asm(char *, const char *, size_t)
 *    indata
 *     r0 : dist ptr
 *     r1 : src ptr
 *     r2 : len
 *    outdata
 *     none:	return value set is C  
 */

	ENTRY(__memcpy_asm)
	ands	ip, r1, #CLONGMASK	/* is "s" aligned? */
	beq	.Lword_loop
	ldrb	r3, [r1], #1
	sub	r2, r2, #1		/* len -- */
	strb	r3, [r0], #1
	cmp	ip, #3
	beq	.Lword_loop
	ldrb	r3, [r1], #1
	sub	r2, r2, #1		/* len -- */
	strb	r3, [r0], #1
	cmp	ip, #2
	beq	.Lword_loop
	ldrb	r3, [r1], #1
	sub	r2, r2, #1		/* len -- */
	strb	r3, [r0], #1
.Lword_loop:
	cmp	r2, #4
	blt	.Lbyte
	ldr	ip, [r1], #4
	sub	r2, r2, #4
	str	ip, [r0], #4
	b	.Lword_loop
.Lbyte:
	cmp	r2, #0
	beq	.Lend
.Lbyte_loop:
	ldrb	ip, [r1], #1
	subs	r2, r2, #1
	strb	ip, [r0], #1
	bgt	.Lbyte_loop
.Lend:
	bx	lr
	SET_SIZE(__memcpy_asm)	
