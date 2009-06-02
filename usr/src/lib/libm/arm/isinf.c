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
 * Copyright (c) 2007-2009 NEC Corporation
 */

#pragma ident	"@(#)isinf.c"

#if defined(ELFOBJ)
#pragma weak isinf = __isinf
/* long double must be treated as double. */
#pragma weak isinfl = __isinf
#pragma weak __isinfl = __isinf
#endif  /* __arm */

#include "libm.h"

int
__isinf(double x)
{
	int *px = (int *) &x;
	return (((px[HIWORD] & 0x7ff00000) == 0x7ff00000) &&
		(px[LOWORD] == 0));
}
