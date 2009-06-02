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
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2006-2008 NEC Corporation
 */

#pragma ident	"@(#)_lwp_mutex_unlock.s	1.18	07/06/17 SMI"

	.file	"_lwp_mutex_unlock.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(_lwp_mutex_unlock,function)

#include "SYS.h"
#include "assym.h"
#include <asm/cpufunc.h>

	ANSI_PRAGMA_WEAK2(_private_lwp_mutex_unlock,_lwp_mutex_unlock,function)

	ENTRY(_lwp_mutex_unlock)
	add	r0, r0, $MUTEX_LOCK_WORD
	mov	r1, #0
	MEMORY_BARRIER(r1)		/* Memory barrier */
0:	ldrex	r3, [r0]
	strex	r2, r1, [r0]
	cmp	r2, #0
	bne	0b			/* retry when fails */
	tst	r3, $WAITER_MASK
	beq	1f
	sub	r0, r0, $MUTEX_LOCK_WORD

	/* Remarks: r1 must be zero. */
	SYSTRAP_RVAL1(lwp_mutex_wakeup)	/* lwp_mutex_wakeup(mp, 0) */
	SYSLWPERR
	RET
1:
	RETC
	SET_SIZE(_lwp_mutex_unlock)
