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

	.ident	"@(#)fpcw.s	1.10	05/06/08 SMI"

	.file	"fpcw.s"

#include <SYS.h>

	ENTRY(_getcw)
	.word	0xeef11a10		/* fmrx		r1, fpscr */
	str	r1, [r0]
        bx      lr
	SET_SIZE(_getcw)

	ENTRY(_putcw)
	.word	0xeee10a10		/* fmxr		fpscr, r0 */
	bx	lr
	SET_SIZE(_putcw)

	ENTRY(_getsw)
	.word	0xeef11a10		/* fmrx		r1, fpscr */
	str	r1, [r0]
	bx	lr
	SET_SIZE(_getsw)

	ENTRY(_putsw)
	.word	0xeee10a10		/* fmxr		fpscr, r0 */
	bx	lr
	SET_SIZE(_putsw)

	ENTRY(_getmxcsr)
	.word	0xeef11a10		/* fmrx		r1, fpscr */
	str	r1, [r0]
	bx	lr
	SET_SIZE(_getmxcsr)

	ENTRY(_putmxcsr)
	.word	0xeee10a10		/* fmxr		fpscr, r0 */
	bx	lr
	SET_SIZE(_putmxcsr)
