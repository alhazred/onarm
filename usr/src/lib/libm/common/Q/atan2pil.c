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

#pragma ident	"@(#)atan2pil.c	1.6	06/01/31 SMI"

#pragma weak atan2pil = __atan2pil

#include "libm.h"

/*
 * atan2pil(y,x) = atan2l(y, x) / pi
 */

static const long double invpi = 3.183098861837906715377675267450287240689e-1L;

long double
#if defined(__arm)
__atan2pil(long double y, long double x)
#else
atan2pil(long double y, long double x)
#endif
{
	return (atan2l(y, x) * invpi);
}
