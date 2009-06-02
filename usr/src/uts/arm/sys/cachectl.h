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
 * Copyright (c) 2006-2007 NEC Corporation
 * All rights reserved.
 */

#ifndef	_SYS_CACHECTL_H
#define	_SYS_CACHECTL_H

#ident	"@(#)arm/sys/cachectl.h"

/*
 * ARM cache control utilities.
 */

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

#include <sys/feature_tests.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <asm/cpufunc.h>

#ifdef	_KERNEL
#include <vm/as.h>

/*
 * The following macros are valid because cache linesize must be a power of 2.
 */
#define	ICACHE_ROUNDUP(a)	P2ROUNDUP((uint32_t)a, arm_picache_linesize)
#define	ICACHE_ROUNDDOWN(a)	P2ALIGN((uint32_t)a, arm_picache_linesize)
#define	DCACHE_ROUNDUP(a)	P2ROUNDUP((uint32_t)a, arm_pdcache_linesize)
#define	DCACHE_ROUNDDOWN(a)	P2ALIGN((uint32_t)a, arm_pdcache_linesize)

/* Primary cache attributes */
extern size_t	arm_picache_linesize;		/* I-cache linesize */
extern size_t	arm_picache_size;		/* I-cache size */
extern size_t	arm_pdcache_linesize;		/* D-cache linesize */
extern size_t	arm_pdcache_size;		/* D-cache size */

extern void	cache_init(void);
extern int      sync_user_icache(struct as *as, caddr_t addr, uint_t size);
extern int      sync_user_dcache(struct as *as, caddr_t addr, size_t len);

/* Cross call handlers. */
extern int	local_sync_icache(uintptr_t vaddr, uint_t size, void *notused);
extern int	local_sync_user_icache(struct as *as, uintptr_t vaddr, uint_t size);
extern int	local_sync_data_memory(uintptr_t vaddr, size_t size,
				       void *notused);
extern int	local_sync_user_dcache(struct as *as, uintptr_t vaddr, size_t size);
extern int	local_dcache_flushall(void *a1, void *a2, void *a3);

/* Flush all primary cache lines */
#define	PCACHE_FLUSH_ALL()			\
	do {					\
		DCACHE_FLUSH_ALL();		\
		ICACHE_INV_ALL();		\
		SYNC_BARRIER();			\
	} while (0)

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* !_SYS_CACHECTL_H */
