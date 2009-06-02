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
 * Copyright (c) 2006-2008 NEC Corporation
 * All rights reserved.
 */

#ifndef	_SYS_CACHE_L220_H
#define	_SYS_CACHE_L220_H

#ident	"@(#)arm/sys/cache_l220.h"

/*
 * MPCore L2 cache (L220) control.
 */

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

#ifdef	_KERNEL

#include <sys/types.h>
#include <sys/mpcore.h>
#if	defined(_KERNEL_BUILD_TREE) && defined(_MACHDEP)
#include <sys/mach_l220.h>
#endif	/* defined(_KERNEL_BUILD_TREE) && defined(_MACHDEP) */

/* L220 cache attributes */
#define	L220_LINESIZE		32	/* Line size is fixed to 32 bytes */

/*
 * Flags for cache_l220_flush().
 * Value must be a register offset in L220 registers.
 */
typedef enum {
	L220_INV = MPCORE_L220_INV_PA,		/* Invalidate */
	L220_CLEAN = MPCORE_L220_CLEAN_PA,	/* Clean */
	L220_FLUSH = MPCORE_L220_CLINV_PA	/* Clean & Invalidate */
} l220_flushop_t;

extern void	cache_l220_init(void);
extern void	cache_l220_flush(caddr_t vaddr, size_t size,
				 l220_flushop_t op);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* !_SYS_CACHE_L220_H */
