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
 * Copyright (c) 2006-2007 NEC Corporation
 */

#ifndef _SYS_STACK_H
#define	_SYS_STACK_H

#if !defined(_ASM)

#include <sys/types.h>

#endif

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * A stack frame looks like:
 *
 *		|--------------------------|
 * 4n+4(fp)   ->| stacked argument word n  |
 *		| ...			   |	(Previous frame)
 *    4(fp)   ->| stacked argument word 0  |
 *		|--------------------------|--------------------
 *    0(fp)   ->| PC			   |
 *		|--------------------------|
 *   -4(fp)   ->| previous LR		   |
 *		|--------------------------|
 *   -8(fp)   ->| previous SP		   |
 *		|--------------------------|
 *  -12(fp)   ->| previous FP		   |
 *		|--------------------------|
 *    4(sp)   ->| unspecified		   |	(Current frame)
 *		| ...			   |
 *    0(sp)   ->| variable size		   |
 * 		|--------------------------|
 */

/*
 * Stack alignment macros.
 */
#define	STACK_ALIGN32		4
#ifdef	__ARM_EABI__
#define	STACK_ENTRY_ALIGN32	8
#else	/* !__ARM_EABI__ */
#define	STACK_ENTRY_ALIGN32	4
#endif	/* __ARM_EABI__ */
#define	STACK_BIAS32		0
#define	SA32(x)			(((x)+(STACK_ALIGN32-1)) & ~(STACK_ALIGN32-1))
#define	STACK_RESERVE32		0
#define	MINFRAME32		0

#define	STACK_ALIGN		STACK_ALIGN32
#define	STACK_ENTRY_ALIGN	STACK_ENTRY_ALIGN32
#define	STACK_BIAS		STACK_BIAS32
#define	SA(x)			SA32(x)
#define	STACK_RESERVE		STACK_RESERVE32
#define	MINFRAME		MINFRAME32

#if defined(_KERNEL) && !defined(_ASM)

#if defined(DEBUG)

/* Use assert function in order to avoid unexpected optimization. */
extern void	assert_stack_aligned(uintptr_t addr, size_t align);

#if	defined(__ARM_EABI__)
#define	ASSERT_STACK_ALIGNED()						\
	{								\
		long double __tmp;					\
		uintptr_t __ptr = (uintptr_t)&__tmp;			\
		assert_stack_aligned(__ptr, STACK_ENTRY_ALIGN);		\
	}
#elif	STACK_ALIGN == 4
#define	ASSERT_STACK_ALIGNED()						\
	{								\
		uint32_t __tmp;						\
		uintptr_t __ptr = (uintptr_t)&__tmp;			\
		assert_stack_aligned(__ptr, STACK_ALIGN);		\
	}
#endif	/* defined(__ARM_EABI__) */
#else	/* DEBUG */
#define	ASSERT_STACK_ALIGNED()
#endif	/* DEBUG */

struct regs;

void traceregs(struct regs *);
void traceback(caddr_t, uintptr_t, uintptr_t);

#endif /* defined(_KERNEL) && !defined(_ASM) */

#define	STACK_GROWTH_DOWN /* stacks grow from high to low addresses */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_STACK_H */
