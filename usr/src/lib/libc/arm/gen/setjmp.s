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

	.ident	"@(#)setjmp.s	1.9	05/06/08 SMI"

	.file	"setjmp.s"

#include <sys/asm_linkage.h>
		
	ANSI_PRAGMA_WEAK(setjmp,function)
	ANSI_PRAGMA_WEAK(longjmp,function)
	
#include <SYS.h>
		
	ENTRY(setjmp)
	stmia	r0!, {r4-r11,r13,r14}
				/* .word	0xec808b10 */
				/* fstmiad	r0, {d8-d15} */
	mov	r0, #0
	bx	lr
	SET_SIZE(setjmp)	

	ENTRY(longjmp)
	ldmia	r0!, {r4-r11,r13,r14}
				/* .word	0xec908b10 */
				/* fldmiad	r0, {d8-d15} */
	cmp	r1, #0
	movne	r0, r1
	moveq	r0, #1
	bx	lr
	SET_SIZE(longjmp)	
