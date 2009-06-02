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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*
 * Copyright (c) 2006-2008 NEC Corporation
 */

#ifndef	_SYS_MACHPARAM_H
#define	_SYS_MACHPARAM_H

#ifndef	_ASM
#include <sys/types.h>
#endif	/* !_ASM */

#include <sys/int_const.h>
#include <sys/pte.h>

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

#define	ADDRESS_C(c)	__CONCAT__(c, ul)

/*
 * Maximum number of CPU.
 * Enables to overwrite platform's Makefile.
 */
#ifndef	NCPU
#define	NCPU	4
#endif	/* !NCPU */

/* ARM architecture has 4 page sizes. */
#define	MMU_PAGE_SIZES		4

/*
 * MMU_PAGES* describes the physical page size used by the mapping hardware.
 * PAGES* describes the logical page size used by the system.
 */

/* Small page (4K) */
#define	MMU_PAGESHIFT			L2PT_VSHIFT
#define	MMU_PAGESIZE			L2PT_VSIZE
#define	MMU_PAGEOFFSET			L2PT_PAGEOFFSET
#define	MMU_PAGEMASK			L2PT_PAGEMASK

/* Large page (64K) */
#define	MMU_PAGESHIFT_LARGE		L2PT_LARGE_VSHIFT
#define	MMU_PAGESIZE_LARGE		L2PT_LARGE_VSIZE
#define	MMU_PAGEOFFSET_LARGE		L2PT_LARGE_VOFFSET
#define	MMU_PAGEMASK_LARGE		L2PT_LARGE_VMASK

/* Section (1M) */
#define	MMU_PAGESHIFT_SECTION		L1PT_SECTION_VSHIFT
#define	MMU_PAGESIZE_SECTION		L1PT_SECTION_VSIZE
#define	MMU_PAGEOFFSET_SECTION		L1PT_SECTION_VOFFSET
#define	MMU_PAGEMASK_SECTION		L1PT_SECTION_VMASK

/* Supersection (16M) */
#define	MMU_PAGESHIFT_SPSECTION		L1PT_SPSECTION_VSHIFT
#define	MMU_PAGESIZE_SPSECTION		L1PT_SPSECTION_VSIZE
#define	MMU_PAGEOFFSET_SPSECTION	L1PT_SPSECTION_VOFFSET
#define	MMU_PAGEMASK_SPSECTION		L1PT_SPSECTION_VMASK

#define	PAGESIZE	MMU_PAGESIZE
#define	PAGESHIFT	MMU_PAGESHIFT
#define	PAGEOFFSET	(PAGESIZE - 1)
#define	PAGEMASK	(~PAGEOFFSET)

/*
 * DATA_ALIGN is used to define the alignment of the Unix data segment.
 */
#define	DATA_ALIGN	PAGESIZE

/*
 * DEFAULT KERNEL THREAD stack size (in pages).
 */
#define	DEFAULTSTKSZ_NPGS	2

#define	DEFAULTSTKSZ	(DEFAULTSTKSZ_NPGS * PAGESIZE)

/*
 * KERNELBASE is the virtual address which the kernel mapping starts in
 * all contexts.
 */
#define	KERNELBASE	ADDRESS_C(0xc0000000)

/*
 * KERNELTEXTBASE is the virtual address which the kernel text starts.
 * We decide to leave 2 pages from the KERNELBASE. The first kernel page
 * (physical address 0) should not be touched because it contains
 * exception vectors set by boot monitor. The second page will be used
 * to map high vector.
 */
#define	KERNELTEXT_OFFSET	(MMU_PAGESIZE << 1)
#define	KERNELTEXTBASE		(KERNELBASE + KERNELTEXT_OFFSET)

/*
 * ARGSBASE is the base virtual address of the range which
 * the kernel uses to map the arguments for exec.
 * On ARM platform implementation, this value must NOT use.
 * Argument address will be passed from U-boot.
 */
#define	ARGSBASE	KERNELBASE	/* Dummy */

/*
 * Virtual address range available to the debugger
 */
#define	SEGDEBUGBASE	0xff900000
#define	SEGDEBUGSIZE	0x400000

/*
 * Import build tree private definitions.
 */
#if	defined(_KERNEL) && defined(_KERNEL_BUILD_TREE)
#include <sys/machparam_impl.h>
#endif	/* defined(_KERNEL) && defined(_KERNEL_BUILD_TREE) */

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* _SYS_MACHPARAM_H */
