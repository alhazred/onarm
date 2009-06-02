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

/*
 *  sysarm System call
 * 
 *    The system call that can implement various ARM specific function.
 */

#pragma ident	"@(#)sysarm.s"

	.file	"sysarm.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(sysarm,function)

#include "SYS.h"

	ANSI_PRAGMA_WEAK2(_private_sysarm,_sysarm,function)

	ENTRY(_sysarm)
	SYSTRAP_RVAL1(sysarm)
	SYSCERROR
	RET
	SET_SIZE(_sysarm)