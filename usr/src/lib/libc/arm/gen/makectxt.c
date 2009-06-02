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
 * Copyright (c) 2006 NEC Corporation
 */

#pragma ident	"@(#)makectxt.c	1.10	05/06/08 SMI"

#pragma weak makecontext = _makecontext

#include "synonyms.h"
#include <stdarg.h>
#include <ucontext.h>
#include <sys/stack.h>

/*
 * The ucontext_t that the user passes in must have been primed with a
 * call to getcontext(2), have the uc_stack member set to reflect the
 * stack which this context will use, and have the uc_link member set
 * to the context which should be resumed when this context returns.
 * When makecontext() returns, the ucontext_t will be set to run the
 * given function with the given parameters on the stack specified by
 * uc_stack, and which will return to the ucontext_t specified by uc_link.
 */

static void resumecontext(void);

void
makecontext(ucontext_t *ucp, void (*func)(), int argc, ...)
{
	long	*sp;
	int	no;
	va_list	ap;
	size_t	size;

	ucp->uc_mcontext.gregs[REG_PC] = (greg_t)func;

	size = (argc > 4 ? argc - 4 : 0) * sizeof (long);

	sp = (long *)(((uintptr_t)ucp->uc_stack.ss_sp +
	    ucp->uc_stack.ss_size - size) & ~(STACK_ALIGN - 1));

	ucp->uc_mcontext.gregs[REG_SP] = (greg_t)sp;

	va_start(ap, argc);

	for (no = 0; no < argc; no++) {
		switch (no) {
		case 0:
			ucp->uc_mcontext.gregs[REG_R0] = va_arg(ap, long);
			break;
		case 1:
			ucp->uc_mcontext.gregs[REG_R1] = va_arg(ap, long);
			break;
		case 2:
			ucp->uc_mcontext.gregs[REG_R2] = va_arg(ap, long);
			break;
		case 3:
			ucp->uc_mcontext.gregs[REG_R3] = va_arg(ap, long);
			break;
		default:
			*sp++ = va_arg(ap, long);
			break;
		}
	}

	va_end(ap);

	ucp->uc_mcontext.gregs[REG_LR] = (greg_t)resumecontext;	/* return address */
}


static void
resumecontext(void)
{
	ucontext_t uc;

	(void) getcontext(&uc);
	if (uc.uc_link == 0) {
		_exit(1);
	}
	(void) setcontext(uc.uc_link);
}
