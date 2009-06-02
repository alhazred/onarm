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
 * Copyright (c) 2006-2008 NEC Corporation
 */

	.ident	"@(#)lshiftl.s	1.5	05/06/08 SMI"

	.file	"lshiftl.s"

/* Shift a double long value. */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(lshiftl,function)

#include "synonyms.h"

	ENTRY(lshiftl)
	tst	r3, r3
	beq	.lshiftld
	bpl	.lshiftlp

	/*
	 * We are doing a negative (right) shift
	 */

	adds	ip, r3, #32
	ble	1f
	rsb	r3, r3, #0
	mov	r1, r1, LSR r3
	orr	r1, r1, r2, LSL ip
	mov	r2, r2, ASR r3
	b	.lshiftld
1:	
	rsb	ip, ip, #0			
	mov	r1, r2, ASR ip
	mov	r2, r2, ASR #31	
	b	.lshiftld	
	/*
	 * We are doing a positive (left) shift
	 */

.lshiftlp:
	subs	ip, r3, #32
	bge	2f
	rsb	ip, ip, #0
	mov	r2, r2, LSL r3
	orr	r2, r2, r1, LSR ip
	mov	r1, r1, LSL r3
	b	.lshiftld	
2:			
	mov	r2, r1, LSL ip
	mov	r1, #0
	/*
	 * We are done.
	 */

.lshiftld:
	stmia	r0, {r1, r2}	/* store result */
	bx	lr
	SET_SIZE(lshiftl)
