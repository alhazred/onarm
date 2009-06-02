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

#pragma ident	"@(#)_xregs_clrptr.c	1.5	05/06/08 SMI"

#include "synonyms.h"
#include <ucontext.h>
#include <sys/types.h>
#include "libc.h"

/*
 * clear the struct ucontext extra register state pointer
 */
void
_xregs_clrptr(ucontext_t *uc)
{
#ifndef __LP64
	uc->uc_mcontext.xrs.xrs_id = 0;
	uc->uc_mcontext.xrs.xrs_ptr = NULL;
#endif
}
