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
 *	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 */

/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2007 NEC Corporation
 */

#pragma ident	"@(#)fpstart.c	1.12	05/06/08 SMI"

/*
 * Establish the default settings for the floating-point state for a C language
 * program:
 *	rounding mode		-- round to nearest default by OS,
 *	exceptions enabled	-- all masked
 *	sticky bits		-- all clear by default by OS.
 *      precision control       -- double extended
 * Set _fp_hw according to what floating-point hardware is available.
 * Set _sse_hw according to what SSE hardware is available.
 * Set __flt_rounds according to the rounding mode.
 */

#pragma weak _fpstart = __fpstart

#include	"synonyms.h"
#include	<sys/types.h>
#include	<sys/fp.h>	

int	_fp_hw;			/* default: bss: 0 == no hardware */
int	_sse_hw;		/* default: bss: 0 == no sse */
int	__flt_rounds;		/* ANSI rounding mode */

void
__fpstart()
{
	_sse_hw = 0;
	_fp_hw = 0;
	__flt_rounds = 1;	/* ANSI way of saying round-to-nearest */
}
