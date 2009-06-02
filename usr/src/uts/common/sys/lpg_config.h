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
 * Copyright (c) 2008 NEC Corporation
 * All rights reserved.
 */

#ifndef	_SYS_LPG_CONFIG_H
#define	_SYS_LPG_CONFIG_H

#ident	"@(#)common/sys/lpg_config.h"

/*
 * Header file that determines large page feature should be enabled or not.
 * Kernel build tree private.
 */

#include <sys/types.h>
#include <sys/debug.h>
#include <vm/page.h>
#include <vm/seg_kmem.h>

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

#ifdef	LPG_DISABLE

/*
 * SZC_EVAL() evaluates page size code value.
 * It always returns zero that means PAGESIZE.
 */
#define	SZC_EVAL(szc)		(0)

/* Verify that page size code is zero. */
#define	SZC_ASSERT(szc)		ASSERT((szc) == 0)

/* Verify that the given condition is true when LPG_DISABLE is defined. */
#define	LPG_DISABLE_ASSERT(ex)	ASSERT(ex)

/*
 * LPG_EVAL() evaluates value which should be zero if large page is disabled.
 * It always returns zero.
 */
#define	LPG_EVAL(flag)		(0)

/*
 * Override large page macros defined in vm/seg_kmem.h.
 */
#undef	SEGKMEM_USE_LARGEPAGES
#define	SEGKMEM_USE_LARGEPAGES		(0)

#undef	IS_KMEM_VA_LARGEPAGE
#define	IS_KMEM_VA_LARGEPAGE(vaddr)	(0)

#else	/* LPG_DISABLE */

#define	SZC_EVAL(szc)		(szc)
#define	SZC_ASSERT(szc)		((void)0)
#define	LPG_DISABLE_ASSERT(ex)	((void)0)
#define	LPG_EVAL(flag)		(flag)

#endif	/* LPG_DISABLE */


#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* !_SYS_LPG_CONFIG_H */
