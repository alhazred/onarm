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

	.ident	"@(#)atanl.s	1.3	06/01/23 SMI"

	.file	"atanl.s"

#include "libm.h"
LIBM_ANSI_PRAGMA_WEAK(atanl,function)
#include "libm_synonyms.h"

	ENTRY(atanl)
	fldt	8(%rsp)			/ push arg
	fld1				/ push 1.0
	fpatan				/ atan(arg/1.0)
	ret
	.align	16
	SET_SIZE(atanl)
