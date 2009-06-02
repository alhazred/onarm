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

	.ident	"@(#)cerror64.s	1.8	05/06/08 SMI"

	.file	"cerror64.s"

/* C return sequence which sets errno, returns -1. */

#include <SYS.h>

	ENTRY(__cerror64)
	cmp	r0, #ERESTART
	moveq	r0, #EINTR
	stmdb	sp!, {r0, lr}
	bl	_fref_(_private___errno)
	ldmia	sp!, {r3, lr}
	str	r3, [r0, #0]
	mov	r0, #-1
	mov	r1, #-1
	bx	lr
	SET_SIZE(__cerror64)
