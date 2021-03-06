/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

	.ident	"@(#)log2l.s	1.6	06/01/23 SMI"

	.file	"log2l.s"

#include "libm.h"
LIBM_ANSI_PRAGMA_WEAK(log2l,function)
#include "libm_synonyms.h"

	ENTRY(log2l)
	fld1			/ push 1.0
	fldt	4(%esp)		/ push x
	fyl2x			/ st = 1.0*log2(arg)
	ret
	.align	4
	SET_SIZE(log2l)
