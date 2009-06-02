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
 * Copyright (c) 2007 NEC Corporation
 * All rights reserved.
 */

#ifndef	_SYS_PTE_IMPL_H
#define	_SYS_PTE_IMPL_H

#ident	"@(#)arm/sys/pte_impl.h"

/*
 * pte_impl.h: Kernel build tree private definitions for PTE.
 */

#ifndef	_SYS_PTE_H
#error	Do NOT include pte_impl.h directly.
#endif	/* !_SYS_PTE_H */

#include <asm/tlb.h>

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

/*
 * Define level 1 page table parameters for user process.
 */
#define	L1PT_USER_SHIFT		L1PT_SHIFT
#define	L1PT_USER_SIZE		L1PT_SIZE
#define	L1PT_USER_NPTES		L1PT_NPTES

#ifdef	_ASM

/*
 * Initialize Translation Table Base Register for boot CPU.
 */
#define	TTB_BOOT_INIT(ttb, zero, reg)					\
	TTB_SET(0, ttb);		/* Initialize TTB(0) */		\
	TTBCTRL_SET(zero)		/* Set zero into TTBCTRL */

/*
 * Initialize Translation Table Base Register for secondary CPUs.
 */
#define	TTB_SECONDARY_INIT(ttb, zero, reg)				\
	TTB_BOOT_INIT(ttb, zero, reg)

#else	/* !_ASM */

/* Install kernel official L1PT into Translation Table Base Register. */
#define	TTB_KERN_INIT(ttb)	TTB_SET(0, ttb)

#endif	/* _ASM */

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* !_SYS_PTE_IMPL_H */
