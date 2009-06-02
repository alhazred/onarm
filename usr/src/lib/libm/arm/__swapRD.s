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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2006-2008 NEC Corporation
 */

#pragma ident	"@(#)__swapRD.s"

	.file	"__swapRD.s"

#include <sys/asm_linkage.h>

	ENTRY(__swapRD)
	and	r0, r0, #3		/* r0 &= 3 */
	mov	r0, r0, LSL #22		/* r0 <<= 22 */
	.word	0xeef11a10		/* fmrx	  r1, fpscr */
	ldr	r2, .LRMODE_MASK	/* r2 = fpscr RMODE_MASK */
	and	r2, r1, r2		/* r2 = r1 & r2 */
	orr	r2, r2, r0		/* r2 = r2 | r0 */
	.word   0xeee12a10              /* fmxr   fpscr, r2 */
	mov	r0, r1, LSR #22		/* r0 = r1 >> 22 */
	and	r0, r0, #3		/* r0 &= 3 */
	bx	lr
.LRMODE_MASK:
	.long	0xff3fffff
	SET_SIZE(__swapRD)

