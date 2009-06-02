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

	.ident	"@(#)lock.s	1.9	05/06/08 SMI"

	.file	"lock.s"

#include "SYS.h"
#include "assym.h"
#include <asm/cpufunc.h>

/*
 * _lock_try(lp)
 *	- returns non-zero on success.
 */
	ENTRY(_lock_try)
	mov	r3, #LOCKSET
1:	ldrexb	r1, [r0]
	strexb	r2, r3, [r0]
	cmp	r2, #0
	bne	1b
	eor	r0, r1, r3	/* return non-zero if success */
	MEMORY_BARRIER(r2)	/* Issue memory barrier */
	bx	lr
	SET_SIZE(_lock_try)

/*
 * _lock_clear(lp)
 *	- clear lock and force it to appear unlocked in memory.
 */
	ENTRY(_lock_clear)
	mov	r3, #0
	MEMORY_BARRIER(r3)	/* Memory barrier */
1:	ldrexb	r1, [r0]
	strexb	r2, r3, [r0]
	cmp	r2, #0
	bne	1b
	bx	lr
	SET_SIZE(_lock_clear)
