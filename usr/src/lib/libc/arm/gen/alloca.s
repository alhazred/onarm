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

	.ident	"@(#)alloca.s	1.8	05/06/08 SMI"

	.file	"alloca.s"

	/* This version is fp reg not use */
	/* So, This routine is not supported */
#include "SYS.h"

	ENTRY(__builtin_alloca)
	add	r0, r0, #7		/* EABI : 8byte alignment */
	and	r0, r0, #-8		/* round up to multiple of 8 */
	mov	r2, sp			
	subs	sp , sp, r0
	subs	fp , fp, r0
	mov	r0, sp	
	subs	r1, fp, sp
	bxeq	lr
	add	r1, r1, #4		/* loop count +1  [fp, #0] */
1:	
	ldr	r3, [r2], #4
	str	r3, [r0], #4
	subs	r1, r1, #4
	bne	1b
	bx	lr
	SET_SIZE(__builtin_alloca)
