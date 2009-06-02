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

	.ident	"@(#)llrintf.s	1.4	06/01/23 SMI"

	.file	"llrintf.s"

#include "libm.h"
LIBM_ANSI_PRAGMA_WEAK(llrintf,function)
#include "libm_synonyms.h"

	ENTRY(llrintf)
	movl	%esp,%ecx
	subl	$8,%esp
	flds	4(%ecx)			/ load x
	fistpll	-8(%ecx)		/ [x]
	fwait
	movl	-8(%ecx),%eax
	movl	-4(%ecx),%edx
	addl	$8,%esp
	ret
	.align	4
	SET_SIZE(llrintf)
