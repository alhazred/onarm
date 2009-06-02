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

#ifndef	_ASM_TLB_H
#define	_ASM_TLB_H

#ident	"@(#)arm/asm/tlb.h"

#include <asm/cpufunc.h>

/*
 * ARM specific TLB operations.
 * This header is build environment private.
 */

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

#ifdef	_ASM

/*
 * Flush all TLB entries.
 * This requires zero as register value.
 */
#define	TLB_FLUSH(reg)		WRITE_CP15(0, c8, c7, 0, reg)

/* Set (reg) to TTB #id */
#define	TTB_SET(id, reg)	WRITE_CP15(0, c2, c0, id, reg)

/* Set Translation Table Base Control Register */
#define	TTBCTRL_SET(reg)	WRITE_CP15(0, c2, c0, 2, reg)

/* Set DACR */
#define	DACR_SET(reg)		WRITE_CP15(0, c3, c0, 0, reg)

/* Set Context ID register */
#define	CONTEXT_ID_SET(reg)	WRITE_CP15(0, c13, c0, 1, reg)

#else	/* !_ASM */

/* Flush all TLB entries */
#define	TLB_FLUSH()		WRITE_CP15(0, c8, c7, 0, 0)

/*
 * Flush TLB single entry.
 * Note that ASID must be set in vaddr.
 */
#define	TLB_FLUSH_VADDR(vaddr)	WRITE_CP15(0, c8, c7, 1, vaddr)

/* Flush all TLB entries corresponding to the specified ASID. */
#define	TLB_FLUSH_ASID(asid)	WRITE_CP15(0, c8, c7, 2, asid)

/* Set (ttb) to TTB #id */
#define	TTB_SET(id, ttb)	WRITE_CP15(0, c2, c0, id, ttb)

/* Set Translation Table Base Control Register */
#define	TTBCTRL_SET(ctrl)	WRITE_CP15(0, c2, c0, 2, ctrl)

/* Set DACR */
#define	DACR_SET(dacr)		WRITE_CP15(0, c3, c0, 0, dacr)

/* Set Context ID register */
#define	CONTEXT_ID_SET(ctxt)	WRITE_CP15(0, c13, c0, 1, ctxt)

#endif	/* !_ASM */

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* !_ASM_TLB_H */
