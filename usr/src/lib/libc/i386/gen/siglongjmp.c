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

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#pragma weak siglongjmp = _siglongjmp

#include "synonyms.h"
#include <sys/types.h>
#include <sys/ucontext.h>
#include <setjmp.h>
#include <ucontext.h>

extern int _setcontext(const ucontext_t *);

void
_siglongjmp(sigjmp_buf env, int val)
{
	ucontext_t *ucp = (ucontext_t *)env;

	if (val)
		ucp->uc_mcontext.gregs[EAX] = val;
	else
		ucp->uc_mcontext.gregs[EAX] = 1;

	(void) _setcontext(ucp);
}
