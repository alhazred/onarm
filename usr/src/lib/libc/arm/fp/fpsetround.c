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
/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2007 NEC Corporation
 */

#pragma ident	"@(#)fpsetround.c	1.7	05/06/08 SMI"

#pragma weak fpsetround = _fpsetround

#include "synonyms.h"
#include <ieeefp.h>
#include "fp.h"

#define   RD_MASK    0xc00000
#define   RD_SHIFT   22

extern void	_getmxcsr(int *), _putmxcsr(int);

fp_rnd
fpsetround(fp_rnd newrnd)
{
	struct _cw87 cw;
	fp_rnd oldrnd;
	extern int __flt_rounds;  /* ANSI value for rounding */

	newrnd &= 0x3;	/* mask off all ubt last 2 bits */
	switch (newrnd) {	/* set ANSI rounding mode */
	case FP_RN:	__flt_rounds = 1;
			break;
	case FP_RM:	__flt_rounds = 3;
			break;
	case FP_RP:	__flt_rounds = 2;
			break;
	case FP_RZ:	__flt_rounds = 0;
			break;
	};
	_getcw(&cw);
	oldrnd = (fp_rnd)cw.rnd;
	cw.rnd = newrnd;
	_putcw(cw);
	return (oldrnd);
}
