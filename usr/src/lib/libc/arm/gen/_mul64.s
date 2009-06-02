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

	.ident	"@(#)_mul64.s	1.7	05/06/08 SMI"

	.file	"_mul64.s"
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

#if defined(__ARM_EABI__)
#include <sys/asm_linkage.h>
#include "synonyms.h"

	ANSI_PRAGMA_WEAK2(__aeabi_lmul,__mul64,function)
#endif

#include "SYS.h"

	ENTRY(__mul64)
	mov	ip, r0
	mul	r1, r2, r1
	mla	r1, r0, r3, r1
	mov	r0, #0
	umlal	r0, r1, ip, r2
	bx	lr
	SET_SIZE(__mul64)
