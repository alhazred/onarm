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

	.ident	"@(#)memset.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(memset,function)

#include "SYS.h"

	ANSI_PRAGMA_WEAK2(_private_memset,memset,function)

/*
 * void *
 * memset(const char *, int, size_t)
 *    indata
 *     r0 : dist ptr
 *     r1 : set character
 *     r2 : len
 *    outdata
 *     r0 : dist ptr
 */

	ENTRY(memset)
	cmp	r0, #0
	beq	.Lend
	and	r1, r1, #0xff
	mov	ip, r0
	cmp	r2, #20			/* len <= 20 is byte loop */
	blt     .Lbyte
	ands	r3, ip, #CLONGMASK	/* is "s" aligned? */
	beq	.Lword
	strb	r1, [ip], #1
	sub	r2, r2, #1		/* len-- */
	cmp	r3, #3
	beq	.Lword
	strb	r1, [ip], #1
	sub	r2, r2, #1		/* len-- */
	cmp	r3, #2
	beq	.Lword
	strb	r1, [ip], #1
	sub	r2, r2, #1		/* len-- */
.Lword:
	orr	r3, r1, r1, lsl #8
	orr	r3, r3, r3, lsl #16	/* set charcter set word image */
.Lword_loop:				/* word alignement. So set is word */
	cmp	r2, #4
	blt	.Lbyte
	str	r3, [ip], #4
	sub	r2, r2, #4
	b	.Lword_loop
.Lbyte:
	cmp	r2, #0
	beq	.Lend
.Lbyte_loop:				/* byte set loop */
	strb	r1, [ip], #1
	subs	r2, r2, #1
	bgt	.Lbyte_loop
.Lend:
	bx      lr			/* return value(r0) is no change */
	SET_SIZE(memset)
	
