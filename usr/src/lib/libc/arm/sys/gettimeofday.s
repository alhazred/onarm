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

/*
 * Copyright (c) 2006 NEC Corporation
 */

#pragma ident	"@(#)gettimeofday.s	1.13	05/06/08 SMI"

	.file	"gettimeofday.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(gettimeofday,function)

#include "SYS.h"

#define OPTIMIZED_NUMBER	274877907

/*
 *  implements int gettimeofday(struct timeval *tp, void *tzp)
 *
 *	note that tzp is always ignored
 */

	ENTRY(_gettimeofday)
	cmp	r0, #0
	moveq	r0, #-1
	beq	1f
	stmfd	sp!, {r0, lr}
/*
 *	use long long gethrestime()
 */
	SYSFASTTRAP(GETHRESTIME)
/*
 *	gethrestime trap returns seconds in %r0, nsecs in %r1
 *	need to convert nsecs to usecs & store into area pointed
 *	to by struct timeval * argument.
 */
	ldmfd	sp!, {r2, lr}
	str	r0, [r2]
	ldr	r3, .L1000
	umull	r1, r0, r3, r1
	mov	r0, r0, ASR #6
	str	r0, [r2, #4]
	mov	r0, #0
1:
	RET
.L1000:
	.word	OPTIMIZED_NUMBER
	SET_SIZE(_gettimeofday)
	

