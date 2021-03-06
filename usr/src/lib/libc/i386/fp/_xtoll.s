/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

	.ident	"%Z%%M%	%I%	%E% SMI"

	.file	"%M%"

#include <SYS.h>

	.set	cw,0
	.set	cw_old,2
	.set	two_words,4
	.set	four_words,8

/ This function truncates the top of the 387 stack into a signed long.

	ENTRY(__xtol)	// 387-stack to signed long
	subl	$8,%esp
	fstcw	cw_old(%esp)
	movw	cw_old(%esp),%ax
	movw	%ax,%cx
	andw	$0x0c00,%cx		// save RC
	orw	$0x0c00,%ax
	movw	%ax,cw(%esp)
	fldcw	cw(%esp)
	fistpl	two_words(%esp)
					// fwait implied here
	fstcw	cw(%esp)		// fetch CW in case masks changed value
	movw	cw(%esp),%ax
	andw	$0xf3ff,%ax
	orw	%cx,%ax			// restore RC
	movw	%ax,cw(%esp)
	fldcw	cw(%esp)
	movl	two_words(%esp),%eax
	addl	$8,%esp
	ret
	SET_SIZE(__xtol)

/ This function truncates the top of the 387 stack into a signed long long.

	ENTRY(__xtoll)	// 387-stack to signed long long
	subl	$12,%esp
	fstcw	cw_old(%esp)
	movw	cw_old(%esp),%ax
	movw	%ax,%cx
	andw	$0x0c00,%cx		// save RC
	orw	$0x0c00,%ax
	movw	%ax,cw(%esp)
	fldcw	cw(%esp)
	fistpll	two_words(%esp)
					// fwait implied here
	fstcw	cw(%esp)		// fetch CW in case masks changed value
	movw	cw(%esp),%ax
	andw	$0xf3ff,%ax
	orw	%cx,%ax			// restore RC
	movw	%ax,cw(%esp)
	fldcw	cw(%esp)
	movl	two_words(%esp),%eax
	movl	four_words(%esp),%edx
	addl	$12,%esp
	ret
	SET_SIZE(__xtoll)

/ This function truncates the top of the 387 stack into a unsigned long.

	.align	4
two_to_31: .long	0x4f000000

	ENTRY(__xtoul)	// 387-stack to unsigned long
	subl	$8,%esp
	fstcw	cw_old(%esp)
	movw	cw_old(%esp),%ax
	movw	%ax,%cx
	andw	$0x0c00,%cx		// save RC
	orw	$0x0c00,%ax
	movw	%ax,cw(%esp)
	fldcw	cw(%esp)
	_prologue_
	fcoms	_sref_(two_to_31)	// compare st to 2**31
	_epilogue_
	fstsw	%ax			// store status in %ax
					// use fstsw for correct trap handling
	sahf				// load AH into flags
	jb	.donotsub		// jump if st < 2**31 or is NaN
	_prologue_
	fsubs	_sref_(two_to_31)	// subtract 2**31
	_epilogue_
.donotsub:
	fistpl	two_words(%esp)
	fwait				// in case fistpl causes exception
	movl	two_words(%esp),%eax
	jb	.donotadd		// flags did not change
	add	$0x80000000,%eax	// add back 2**31
.donotadd:
	fstcw	cw(%esp)		// fetch CW in case masks changed value
	movw	cw(%esp),%dx
	andw	$0xf3ff,%dx
	orw	%cx,%dx			// restore RC
	movw	%dx,cw(%esp)
	fldcw	cw(%esp)
	addl	$8,%esp
	ret
	SET_SIZE(__xtoul)
