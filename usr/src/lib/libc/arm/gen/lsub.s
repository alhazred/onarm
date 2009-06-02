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

	.ident	"@(#)lsub.s	1.5	05/06/08 SMI"

	.file	"lsub.s"

/* Double long subtraction routine. */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(lsub,function)

#include "synonyms.h"

	ENTRY(lsub)
	subs	r1, r1, r3
	ldr	r3, [sp, #0]
	sbc	r2, r2, r3
	stmia	r0, {r1, r2}	/* store result */
	bx	lr
	SET_SIZE(lsub)
