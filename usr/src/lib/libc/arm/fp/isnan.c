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

/*
 * Copyright (c) 2007 NEC Corporation
 */

#pragma ident	"@(#)isnan.c	1.10	06/01/25 SMI"

#pragma weak isnan = __isnan
#pragma weak _isnan = __isnan
#pragma weak _isnand = __isnan
#pragma weak isnand = __isnan

#include "base_conversion.h"

int
__isnan(double x) {
	long int	*px = (long int *)&x;
	long int	lx;

	lx = px[HIWORD] & ~0x80000000ul;
	if ((lx == 0x7ff00000) && (px[LOWORD] != 0)) return(1);
	return((unsigned long)(0x7ff00000 - lx) >> 31);
}
