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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

	.ident	"@(#)log10f.s	1.9	06/01/23 SMI"

        .file "log10f.s"

#include "libm.h"
LIBM_ANSI_PRAGMA_WEAK(log10f,function)
#include "libm_synonyms.h"
#include "libm_protos.h"

	ENTRY(log10f)
	fldlg2	
	flds	4(%esp)			/ st = arg, st(1) = log10(2)
	fyl2x				/ st = log10(arg) = log10(2)*log2(arg)
	ret
	.align	4
	SET_SIZE(log10f)
