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

#pragma ident	"@(#)fenv.c	1.9	06/01/31 SMI"

#pragma weak fex_merge_flags = __fex_merge_flags

#pragma weak feholdexcept = __feholdexcept
#pragma weak feupdateenv = __feupdateenv
#pragma weak fegetenv = __fegetenv
#pragma weak fesetenv = __fesetenv

#pragma weak feholdexcept96 = __feholdexcept96
#pragma weak feupdateenv96 = __feupdateenv
#pragma weak fegetenv96 = __fegetenv
#pragma weak fesetenv96 = __fesetenv

#include "fenv_synonyms.h"
#include <fenv.h>
#include <ucontext.h>
#include <thread.h>
#include "fex_handler.h"

const fenv_t __fenv_dfl_env = {
	{
		{ FEX_NONSTOP, (void(*)())0 },
		{ FEX_NONSTOP, (void(*)())0 },
		{ FEX_NONSTOP, (void(*)())0 },
		{ FEX_NONSTOP, (void(*)())0 },
		{ FEX_NONSTOP, (void(*)())0 },
		{ FEX_NONSTOP, (void(*)())0 },
		{ FEX_NONSTOP, (void(*)())0 },
		{ FEX_NONSTOP, (void(*)())0 },
		{ FEX_NONSTOP, (void(*)())0 },
		{ FEX_NONSTOP, (void(*)())0 },
		{ FEX_NONSTOP, (void(*)())0 },
		{ FEX_NONSTOP, (void(*)())0 },
	},
#ifdef __i386
	0x13000000
#else
	0
#endif
};

int 
#if defined(__arm)
__feholdexcept(fenv_t *p)
#else
feholdexcept(fenv_t *p)
#endif
{
	(void) fegetenv(p);
	(void) feclearexcept(FE_ALL_EXCEPT);
	return !fex_set_handling(FEX_ALL, FEX_NONSTOP, NULL);
}

int 
#if defined(__arm)
__feholdexcept96(fenv_t *p)
#else
feholdexcept96(fenv_t *p)
#endif
{
	(void) fegetenv(p);
	(void) feclearexcept(FE_ALL_EXCEPT);
	return fex_set_handling(FEX_ALL, FEX_NONSTOP, NULL);
}

int 
#if defined(__arm)
__feupdateenv(const fenv_t *p)
#else
feupdateenv(const fenv_t *p)
#endif
{
	unsigned long	fsr;

	__fenv_getfsr(&fsr);
	(void) fesetenv(p);
	(void) feraiseexcept((int)__fenv_get_ex(fsr));
	return 0;
}

int 
#if defined(__arm)
__fegetenv(fenv_t *p)
#else
fegetenv(fenv_t *p)
#endif
{
	fex_getexcepthandler(&p->__handlers, FEX_ALL);
	__fenv_getfsr(&p->__fsr);
	return 0;
}

int 
#if defined(__arm)
__fesetenv(const fenv_t *p)
#else
fesetenv(const fenv_t *p)
#endif
{
	__fenv_setfsr(&p->__fsr);
	fex_setexcepthandler(&p->__handlers, FEX_ALL);
	return 0;
}

void 
#if defined(__arm)
__fex_merge_flags(const fenv_t *p)
#else
fex_merge_flags(const fenv_t *p)
#endif
{
	unsigned long	fsr;

	__fenv_getfsr(&fsr);
	__fenv_set_ex(fsr, __fenv_get_ex(fsr) | __fenv_get_ex(p->__fsr));
	__fenv_setfsr(&fsr);
	if (fex_get_log())
		__fex_update_te();
}
