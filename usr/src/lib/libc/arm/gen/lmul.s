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

	.ident	"@(#)lmul.s	1.5	05/06/08 SMI"

	.file	"lmul.s"

/*
 *
 *   function __mul64(A,B:Longint):Longint;
 *	{Overflow is not checked}
 *
 * We essentially do multiply by longhand, using base 2**32 digits.
 *               a       b	parameter A
 *           x 	 c       d	parameter B
 *		----------
 *              ad      bd
 *       ac	bc         
 *       -----------------
 *       ac	ad+bc	bd
 *       
 *       We can ignore ac and top 32 bits of ad+bc: if <> 0, overflow happened.
 *       
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(lmul,function)

#include "synonyms.h"

	ENTRY(lmul)
	str	lr, [sp, #-8]!		/* stack pointer is 8byte alignment */
	mov	ip, r1
	ldr	lr, [sp, #8]
	mul	r2, r3, r2
	mla	r2, ip, lr, r2
	mov	r1, #0
	umlal	r1, r2, ip, r3
	stmia	r0, {r1, r2}	/* store result */
	ldr	pc, [sp], #8	
	SET_SIZE(lmul)
