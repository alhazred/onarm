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

#pragma ident	"@(#)casinh.c	1.3	06/01/31 SMI"

#pragma weak casinh = __casinh

/* INDENT OFF */
/*
 * dcomplex casinh(dcomplex z);
 * casinh z = -i casin iz .
 */
/* INDENT ON */

#include "libm.h"
#include "complex_wrapper.h"

dcomplex
#if defined(_arm)
__casinh(dcomplex z)
#else
casinh(dcomplex z)
#endif
{
	dcomplex w, r, ans;

	D_RE(w) = -D_IM(z);
	D_IM(w) = D_RE(z);
	r = casin(w);
	D_RE(ans) = D_IM(r);
	D_IM(ans) = -D_RE(r);
	return (ans);
}
