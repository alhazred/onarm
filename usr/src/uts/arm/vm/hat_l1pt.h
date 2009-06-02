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

#ifndef	_VM_HAT_L1PT_H
#define	_VM_HAT_L1PT_H

#ident	"@(#)arm/vm/hat_l1pt.h"

/*
 * Kernel build tree private header for level 1 page table management.
 */

#include <sys/types.h>
#include <sys/pte.h>

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

/*
 * Kernel L1 PTE synchronization
 */

/*
 * HATPT_KAS_SYNC(vaddr, len, cond)
 *	Call hatpt_kas_sync(vaddr, len) if (cond) is true.
 *	Kernel hat lock will be acquired and released in this macro.
 */
#define	HATPT_KAS_SYNC(vaddr, len, cond)				\
	do {								\
		if (cond) {						\
			HAT_KAS_LOCK();					\
			hatpt_kas_sync((uintptr_t)(vaddr), (size_t)(len)); \
			HAT_KAS_UNLOCK();				\
		}							\
	} while (0)

/*
 * HATPT_KAS_SYNC_L(vaddr, len, cond)
 *	Call hatpt_kas_sync(vaddr, len) if (cond) is true.
 *	The caller must hold kernel hat lock.
 */
#define	HATPT_KAS_SYNC_L(vaddr, len, cond)				\
	do {								\
		if (cond) {						\
			hatpt_kas_sync((uintptr_t)(vaddr), (size_t)(len)); \
		}							\
	} while (0)

/*
 * Reclaim function for hatpt_l2ptable_cache.
 * We set hatpt_l1pt_reap() as reclaim function.
 */
#define	HAT_L2PT_RECLAIM_FUNC		hatpt_l1pt_reap

/* Prototypes */
extern void		hatpt_l1pt_init(void);
extern l1pte_t		*hatpt_l1pt_alloc(int cansleep, uintptr_t *paddrp);
extern void		hatpt_l1pt_free(l1pte_t *l1pt, uintptr_t paddr);
extern void		hatpt_l1pt_reap(void *notused);
extern void		hatpt_kas_sync(uintptr_t vaddr, size_t len);

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* !_VM_HAT_L1PT_H */
