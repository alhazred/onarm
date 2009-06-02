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

#pragma ident	"@(#)forkx.s	1.12	06/11/26 SMI"

	.file	"forkx.s"

#include "SYS.h"

/*
 * pid = __forkx(flags);
 *
 * syscall trap: forksys(0, flags)
 *
 * From the syscall:
 * r1 == 0 in parent process, r1 = 1 in child process.
 * r0 == pid of child in parent, r0 == pid of parent in child.
 *
 * The child gets a zero return value.
 * The parent gets the pid of the child.
 */

ENTRY(__forkx)
	mov	r1, r0
	mov	r0, #0
	SYSTRAP_2RVALS(forksys)
	SYSCERROR
	cmp	r1, #0
	movne	r0, #0
	bx	lr
	SET_SIZE(__forkx)
