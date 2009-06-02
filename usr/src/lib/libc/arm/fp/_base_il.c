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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2007 NEC Corporation
 */

#pragma ident	"@(#)_base_il.c	1.9	05/06/08 SMI"

#include "synonyms.h"
#include "base_conversion.h"

#if defined(__arm)
#define   EX_MASK    0xffff
#define   RD_MASK    0xc00000
#define   RDD_MASK   0x000000    
#else
#define   EX_MASK    0x3f
#define   RD_MASK    0xf00
#define   RDD_MASK   0x200
#endif

/* The following should be coded as inline expansion templates.	 */

/*
 * Multiplies two normal or subnormal doubles, returns result and exceptions.
 */
double
__mul_set(double x, double y, int *pe) {
	extern void _putsw(), _getsw();
	int sw;
	double z;

	_putsw(0);
	z = x * y;
#if defined(__arm)
	/* z=x*y compute before getsw() */
	_getsw(&sw,z);
#else
	_getsw(&sw);
#endif
	if ((sw & EX_MASK) == 0) {
		*pe = 0;
	} else {
		/* Result may not be exact. */
		*pe = 1;
	}
	return (z);
}

/*
 * Divides two normal or subnormal doubles x/y, returns result and exceptions.
 */
double
__div_set(double x, double y, int *pe) {
	extern void _putsw(), _getsw();
	int sw;
	double z;

	_putsw(0);
	z = x / y;
#if defined(__arm)
	/* z=x/y compute before getsw() */
	_getsw(&sw,z);
#else
	_getsw(&sw);
#endif
	if ((sw & EX_MASK) == 0) {
		*pe = 0;
	} else {
		*pe = 1;
	}
	return (z);
}

double
__dabs(double *d)
{
	/* should use hardware fabs instruction */
	return ((*d < 0.0) ? -*d : *d);
}

/*
 * Returns IEEE mode/status and
 * sets up standard environment for base conversion.
 */
void
__get_ieee_flags(__ieee_flags_type *b) {
	extern void _getcw(), _getsw(), _putcw();
	int cw;

	_getcw(&cw);
	b->mode = cw;
	_getsw(&b->status);
	/*
	 * set CW to...
	 * RC (bits 23:22)	0 == round to nearest zero
	 */
	cw = ((cw & ~(RD_MASK)) | (RDD_MASK)) ;
	_putcw(cw);
}

/*
 * Restores previous IEEE mode/status
 */
void
__set_ieee_flags(__ieee_flags_type *b) {
	extern void _putcw(), _putsw();

	_putcw(b->mode);
	_putsw(b->status);
}
