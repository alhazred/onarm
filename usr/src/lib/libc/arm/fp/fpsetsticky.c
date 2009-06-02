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
 * Copyright (c) 2007 NEC Corporation
 */

#pragma ident	"@(#)fpsetsticky.c	1.2	05/06/08 SMI"

#include <ieeefp.h>
#include "synonyms.h"
#include "fp.h"

#pragma weak fpsetsticky = _fpsetsticky

extern void	_getsw(int *), _putsw(int), _getmxcsr(int *), _putmxcsr(int);

fp_except
fpsetsticky(fp_except s)
{
	int		sw;

	_getsw(&sw);
	s = (sw & ~EXCPMASK) | (s & EXCPMASK);
	_putsw((int)s);
	return ((fp_except)(sw & EXCPMASK));
}