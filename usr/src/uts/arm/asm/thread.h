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
 * Copyright (c) 2006 NEC Corporation
 * All rights reserved.
 */

#ifndef _ASM_THREAD_H
#define	_ASM_THREAD_H

#ifndef	_ASM

#ident	"@(#)asm/thread.h"

#include <sys/types.h>
#include <asm/cpufunc.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if	!defined(__lint) && defined(__GNUC__)

struct _kthread;

/*
 * struct _kthread *
 * threadp(void)
 *	Return kernel thread address of the current thread.
 */
static __inline__ struct _kthread *
threadp(void)
{
	/*
	 * Current kernel thread pointer is stored to Thread ID register
	 * (privileged R/W accessible only).
	 */
	return (struct _kthread *)READ_CP15(0, c13, c0, 4);
}

/*
 * void
 * threadp_set(struct _kthread *tp)
 *	Set current kernel thread pointer.
 */
static __inline__ void
threadp_set(struct _kthread *tp)
{
	WRITE_CP15(0, c13, c0, 4, tp);
}

/*
 * caddr_t
 * caller(void)
 *	Returns the caller address.
 *	If a() calls b() and b() calls caller(), caller() returns address
 *	in a().
 *
 * Remarks:
 *	This function simply returns lr.
 */
static __inline__ caddr_t
caller(void)
{
	caddr_t	ret;

	__asm__ __volatile__
		("mov	%0, lr"
		 : "=r" (ret));
	return ret;
}

/*
 * caddr_t
 * callee(void)
 *	Returns the callee address.
 *	If a() calls callee(), callee() returns the return address in a();
 */
static __inline__ caddr_t
callee(void)
{
	caddr_t	ret;

	__asm__ __volatile__
		("1:	adr	%0, 1b"
		 : "=r" (ret));
	return ret;
}

#endif	/* !__lint && __GNUC__ */

#ifdef	__cplusplus
}
#endif

#else	/* _ASM */

#include <asm/cpufunc.h>

 /* Set current thread pointer to the specified register. */
#define	THREADP(reg)		READ_CP15(0, c13, c0, 4, reg)

/* Update current thread pointer */
#define	THREADP_SET(reg)	WRITE_CP15(0, c13, c0, 4, reg)

#endif	/* !_ASM */

#endif	/* _ASM_THREAD_H */
