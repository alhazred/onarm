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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

	.ident	"%Z%%M%	%I%	%E% SMI"

	.file	"%M%"

/	/* Assembler program to implement the following C program */
/	int
/	abs(arg)
/	int	arg;
/	{
/		return((arg < 0)? -arg: arg);
/	}

#include "SYS.h"

	ENTRY(abs)
	movl	4(%esp),%eax	/ arg < 0?
	testl	%eax,%eax
	jns	1f
	negl	%eax		/ yes, return -arg
1:
	ret
	SET_SIZE(abs)

	ENTRY(labs)
	movl	4(%esp),%eax	/ arg < 0?
	testl	%eax,%eax
	jns	1f
	negl	%eax		/ yes, return -arg
1:
	ret
	SET_SIZE(labs)
