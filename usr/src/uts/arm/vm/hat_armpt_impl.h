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
 * Copyright (c) 2007-2008 NEC Corporation
 * All rights reserved.
 */

#ifndef	_VM_HAT_ARMPT_IMPL_H
#define	_VM_HAT_ARMPT_IMPL_H

#ident	"@(#)arm/vm/hat_armpt_impl.h"

/*
 * hat_armpt_impl.h:
 *	Kernel build tree private definitions for HAT page table management.
 */

#ifndef	_VM_HAT_ARMPT_H
#error	Do NOT include hat_armpt_impl.h directly.
#endif	/* !_VM_HAT_ARMPT_H */

#include <sys/sysmacros.h>
#include <sys/archsystm.h>
#include <vm/hat_l1pt.h>

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

/*
 * Number of bundles to manage whole user space
 */
#define	L2BD_USER_NBSIZE	(L2BD_NBSIZE - L2BD_KERN_NBSIZE)

/* Zero out hat_l2bundle array. */
#define	HAT_L2BUNDLE_USER_INIT(hat)					\
	do {								\
		FAST_BZERO_ALIGNED(hat->hat_l2bundle,			\
				   sizeof(hat_l2bundle_t *) *		\
				   L2BD_USER_NBSIZE);			\
	} while (0)

/*
 * Initialize user L1PT.
 */
#define	HAT_USERPT_INIT(hat, l1pt)					\
	do {								\
		int	__l1index;					\
		l1pte_t	*__ksrc, *__kdst;				\
		size_t	__uvsize;					\
									\
		/*							\
		 * Use single L1PT pointed by TTB(0).			\
		 * So we must copy L1PT entries for kernel space into	\
		 * new L1PT.						\
		 */							\
		__l1index = L1PT_INDEX(KERNELBASE);			\
		__ksrc = hat_kas.hat_l1vaddr + __l1index;		\
		__kdst = l1pt + __l1index;				\
		__uvsize = __l1index * L1PT_PTE_SIZE;			\
									\
		HAT_KAS_LOCK();						\
		mutex_enter(&hat_list_lock);				\
									\
		/*							\
		 * Preemption is disabled because hat_list_lock is	\
		 * a spinlock and it raises spl to DISP_LEVEL.		\
		 */							\
									\
		/*							\
		 * Link new hat to the global list of all		\
		 * hat structures.					\
		 */							\
		HAT_LIST_INSERT(hat, &hat_kas);				\
									\
		/* Zeros out all L1PT entries for the user mapping. */	\
		fast_bzero(l1pt, __uvsize);				\
									\
		/* Copy all L1PT entries for the kernel mapping. */	\
		fast_bcopy(__ksrc, __kdst, L1PT_USER_SIZE - __uvsize);	\
									\
		/* Sync whole L1PT changes. */				\
		PTE_SYNC_RANGE(l1pt, L1PT_PTE_SIZE, L1PT_USER_NPTES);	\
									\
		mutex_exit(&hat_list_lock);				\
		HAT_KAS_UNLOCK();					\
	} while (0)

/* Return current L1PT for kernel space. */
#define	HAT_KERN_L1PT(curhat)		((curhat)->hat_l1vaddr)

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* !_VM_HAT_ARMPT_IMPL_H */
