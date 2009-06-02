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

	.ident	"@(#)scalbnl.s	1.3	06/01/23 SMI"

	.file	"scalbnl.s"

#include "libm.h"
LIBM_ANSI_PRAGMA_WEAK(scalbnl,function)
#include "libm_synonyms.h"

	ENTRY(scalbnl)
	subq	$16,%rsp
	movl	%edi,(%rsp)
	fildl	(%rsp)
	fldt	24(%rsp)
	addq	$16,%rsp
	fscale
	fstp	%st(1)
	ret
	.align	16
	SET_SIZE(scalbnl)
