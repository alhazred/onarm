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

	.ident	"@(#)_getsp.s	1.9	05/06/08 SMI"

	.file	"_getsp.s"

#include "SYS.h"

	ENTRY(_getsp)
	mov	r0, sp			/* mov r0, r13 */
	bx	lr
	SET_SIZE(_getsp)

	ENTRY(_getfp)
	mov	r0, fp			/* mov r0, r11 */
	bx	lr
	SET_SIZE(_getfp)

	ENTRY(_setsp)
	and	sp, r0, #-8		/* EABI : 8byte alignment */
					/* mov r13, r0 */
	bx	lr
	SET_SIZE(_setsp)

	ENTRY(_setfp)
	and	fp, r0, #-8		/* EABI : 8byte alignment */	
					/* mov r11, r0 */
	bx	lr
	SET_SIZE(_setfp)
