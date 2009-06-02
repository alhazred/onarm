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

#ifndef	_VM_KVLAYOUT_H
#define	_VM_KVLAYOUT_H

#ident	"@(#)arm/vm/kvlayout.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/vmparam.h>
#include <sys/cmn_err.h>
#include <sys/sysmacros.h>
#include <sys/machsystm.h>

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

/*
 * Common definitions for kerley virtual space layout for ARM architecture.
 * Kernel build tree private.
 */

/*
 * The module text region must be 32 megabytes because "bl" and "b"
 * instruction can't jump to outside 32M from the current program counter.
 */
#define	HEAPTEXT_SIZE_LIMIT	(32 * 1024 * 1024)	/* bytes */
#define	HEAPTEXT_ADDR_LIMIT	(KERNELBASE + HEAPTEXT_SIZE_LIMIT)
#define	HEAPTEXT_BASE_ADDR	KSTATIC_END

/*
 * Reserve space for module text.
 * Module text region must be within 32 megabytes from KERNELBASE.
 * because "bl" and "b" instruction can't jump to outside 32M from
 * the current program counter.
 */
#define	HEAPTEXT_RESERVE(base, size)					\
	do {								\
		if (HEAPTEXT_BASE_ADDR >= (caddr_t)HEAPTEXT_ADDR_LIMIT) { \
			panic("Can't reserve space for module text.");	\
		}							\
		(base) = (uintptr_t)HEAPTEXT_BASE_ADDR;			\
		(size) = HEAPTEXT_ADDR_LIMIT - (base);			\
	} while (0)

/*
 * Reserve segmap range from the specified base address.
 * "size" should be segmap size requested by parameter, and it will be
 * updated by this macro.
 * Currently we have no plan to support large page on segmap,
 * so we just align base address to MAXBSIZE.
 */
#define	SEGMAP_RESERVE(base, start, size)				\
	do {								\
		if ((size) < SEGMAPMIN) {				\
			(size) = SEGMAPMIN;				\
		}							\
		else if ((size) > SEGMAPMAX) {				\
			(size) = SEGMAPMAX;				\
		}							\
		(size) = P2ROUNDUP((size), MAXBSIZE);			\
		(start) = P2ROUNDUP((uintptr_t)(base), MAXBSIZE);	\
	} while (0)

/*
 * Fixup start address of kernel heap.
 * This macro reserves red zone below start address.
 */
#define	KERNELHEAP_FIXUP(addr)	(char *)((addr) + MMU_PAGESIZE)

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* !_VM_KVLAYOUT_H */
