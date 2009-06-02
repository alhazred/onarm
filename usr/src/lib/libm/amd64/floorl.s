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

	.ident	"@(#)floorl.s	1.3	06/01/23 SMI"

	.file	"floorl.s"

#include "libm.h"
LIBM_ANSI_PRAGMA_WEAK(ceill,function)
LIBM_ANSI_PRAGMA_WEAK(floorl,function)
#include "libm_synonyms.h"

	ENTRY(ceill)
	subq	$16,%rsp
	fstcw	(%rsp)
	fldt	24(%rsp)
	movw	(%rsp),%cx
	orw	$0x0c00,%cx
	xorw	$0x0400,%cx
	movw	%cx,4(%rsp)
	fldcw	4(%rsp)			/ set RD = up
	frndint
	fstcw	4(%rsp)			/ restore RD
	movw	4(%rsp),%dx
	andw	$0xf3ff,%dx
	movw	(%rsp),%cx
	andw	$0x0c00,%cx
	orw	%dx,%cx
	movw	%cx,(%rsp)
	fldcw	(%rsp)			/ restore RD
	addq	$16,%rsp
	ret
	.align	16
	SET_SIZE(ceill)


	ENTRY(floorl)
	subq	$16,%rsp
	fstcw	(%rsp)
	fldt	24(%rsp)
	movw	(%rsp),%cx
	orw	$0x0c00,%cx
	xorw	$0x0800,%cx
	movw	%cx,4(%rsp)
	fldcw	4(%rsp)			/ set RD = down
	frndint
	fstcw	4(%rsp)			/ restore RD
	movw	4(%rsp),%dx
	andw	$0xf3ff,%dx
	movw	(%rsp),%cx
	andw	$0x0c00,%cx
	orw	%dx,%cx
	movw	%cx,(%rsp)
	fldcw	(%rsp)			/ restore RD
	addq	$16,%rsp
	ret
	.align	16
	SET_SIZE(floorl)
