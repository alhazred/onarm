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
 * Copyright (c) 2006 NEC Corporation
 * All rights reserved.
 */

	.ident	"@(#)arm_string.s"
	.file	"arm_string.s"

#include <sys/asm_linkage.h>
#include <sys/feature_tests.h>
#include "assym.h"

/*
 * String related subroutines
 */

#if	CLONGSIZE != 4
#error	"CLONGSIZE must be 4."
#endif	/* CLONGSIZE != 4 */

#if	defined(_BIG_ENDIAN)
#define	CHAR0_MASK	0xff000000
#define	CHAR1_MASK	0x00ff0000
#define	CHAR2_MASK	0x0000ff00
#define	CHAR3_MASK	0x000000ff
#elif defined(_LITTLE_ENDIAN)
#define	CHAR0_MASK	0x000000ff
#define	CHAR1_MASK	0x0000ff00
#define	CHAR2_MASK	0x00ff0000
#define	CHAR3_MASK	0xff000000
#else	/* !defined(_BIG_ENDIAN) && !defined(_LITTLE_ENDIAN) */
#error	"Endianess is not defined."
#endif	/* defined(_BIG_ENDIAN) */

/*
 * size_t
 * strlen(const char *str)
 *	Returns the number of non-NULL bytes in string argument.
 */
ENTRY(strlen)
	mov	r3, r0
	tst	r0, #CLONGMASK		/* if str is not word-aligned */
	bne	.Lnot_aligned		/*   goto .Lnot_aligned */

.Laligned:
	/*
	 * The following routine assumes that we can access at least 1 word
	 * even if the current word exceeds buffer area.
	 */
	ldr	r1, [r3]		/* r1 = *((long *)r3) */
	tst	r1, #CHAR0_MASK		/* if ((r1 & CHAR0_MASK) == 0) */
	beq	.Lfound			/*   goto .Lfound */
	tst	r1, #CHAR1_MASK		/* if ((r1 & CHAR1_MASK) == 0) */
	addeq	r3, r3, #1		/*   r3 += 1 */
	beq	.Lfound			/*   goto .Lfound */
	tst	r1, #CHAR2_MASK		/* if ((r1 & CHAR2_MASK) == 0) */
	addeq	r3, r3, #2		/*   r3 += 2 */
	beq	.Lfound			/*   goto .Lfound */
	tst	r1, #CHAR3_MASK		/* if ((r1 & CHAR3_MASK) == 0) */
	addeq	r3, r3, #3		/*   r3 += 3 */
	beq	.Lfound			/*   goto .Lfound */
	add	r3, r3, #CLONGSIZE	/* r3 += CLONGSIZE */
	b	.Laligned		/* loop over */
.Lnot_aligned:
	ldrb	r1, [r3]		/* r1 = *r3 */
	teq	r1, #0			/* if (r1 == 0) */
	beq	.Lfound			/*   goto .Lfound */
	add	r3, r3, #1		/* r3++ */
	tst	r3, #CLONGMASK		/* if r3 is word-aligned */
	beq	.Laligned		/*   goto .Laligned */
	b	.Lnot_aligned		/* goto .Lnot_aligned */
.Lfound:
	sub	r0, r3, r0		/* return r3 - r0 */
	mov	pc, lr
	SET_SIZE(strlen)
