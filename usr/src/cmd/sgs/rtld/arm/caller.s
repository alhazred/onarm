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
 *
 * Return the pc of the calling routine.
 */

/*
 * Copyright (c) 2007-2008 NEC Corporation
 */

	.ident	"@(#)caller.s	1.7	05/06/08 SMI"

#if	defined(lint)

#include	<sys/types.h>

caddr_t
caller()
{
	return (0);
}

#else
	.file	"caller.s"

#include <sys/asm_linkage.h>

	/*
	 * Outside of caller(), we have already made r0 to
	 * contain the return address of the object calling
	 * the dlXXX functions.
	 * So we do not need anything in this function.
	 */
	ENTRY(caller)
	mov	pc, lr
	SET_SIZE(caller)
#endif
