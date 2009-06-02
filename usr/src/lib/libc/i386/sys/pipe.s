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

#pragma ident	"@(#)pipe.s	1.12	05/06/08 SMI"

	.file	"pipe.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(pipe,function)

#include "SYS.h"

	ENTRY(pipe)
	SYSTRAP_2RVALS(pipe)
	SYSCERROR
	movl	4(%esp), %ecx
	movl	%eax, (%ecx)
	movl	%edx, 4(%ecx)
	RETC
	SET_SIZE(pipe)