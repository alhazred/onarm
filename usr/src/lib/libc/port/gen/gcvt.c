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
 * Copyright (c) 2008 NEC Corporation
 */

#pragma ident	"@(#)gcvt.c	1.14	05/06/08 SMI"

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/


/*
 * gcvt  - Floating output conversion to a pleasant-looking string.
 */

#pragma weak gcvt = _gcvt

#ifdef	__arm
#pragma	weak qgcvt = _gcvt
#pragma	weak _qgcvt = _gcvt
#endif	/* __arm */

#include "synonyms.h"
#include <floatingpoint.h>

char *
gcvt(double number, int ndigits, char *buf)
{
	return (gconvert(number, ndigits, 0, buf));
}
