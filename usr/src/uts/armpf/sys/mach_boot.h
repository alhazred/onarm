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
 * Copyright (c) 2008-2009 NEC Corporation
 * All rights reserved.
 */

#ifndef	_SYS_MACH_BOOT_H
#define	_SYS_MACH_BOOT_H

#ident	"@(#)armpf/sys/mach_boot.h"

/*
 * ARM platform implementation specific definitions only for bootstrap code.
 * Kernel build tree private.
 */

#include <sys/param.h>
#include <sys/pte.h>
#include <sys/platform.h>
#include <sys/int_const.h>

/*
 * The following constants are used to pass L1PT physical address to
 *  secondary_start().
 * Boot CPU writes L1PT physical address into memory just below kernel text
 * before it kicks secondary CPUs.
 */
#define	MP_STARTUP_L1PT_OFFSET	4
#define	MP_STARTUP_L1PT_VADDR	(KERNELTEXTBASE - MP_STARTUP_L1PT_OFFSET)
#define	MP_STARTUP_L1PT_PADDR	(KERNELPHYSBASE + KERNELTEXT_OFFSET -	\
				 MP_STARTUP_L1PT_OFFSET)

#ifdef	_ASM

/*
 * L1PT attributes used to map memory in locore.s.
 */
#if	NCPU == 1
#define	STARTUP_L1PTATTR_MEM						\
	(L1PT_BUFFERABLE|L1PT_CACHED|L1PT_AP(PTE_AP_KRW)|		\
	 L1PT_TEX(PTE_TEX_WALLOC))
#define	STARTUP_LARGE_ATTR						\
	(L2PT_BUFFERABLE|L2PT_CACHED|L2PT_AP(PTE_AP_KRW)|		\
	 L2PT_LARGE_TEX(PTE_TEX_WALLOC))
#define	STARTUP_SMALL_ATTR						\
	(L2PT_BUFFERABLE|L2PT_CACHED|L2PT_AP(PTE_AP_KRW)|		\
	 L2PT_SMALL_TEX(PTE_TEX_WALLOC))
#else	/* NCPU != 1 */
#define	STARTUP_L1PTATTR_MEM						\
	(L1PT_BUFFERABLE|L1PT_CACHED|L1PT_SHARED|L1PT_AP(PTE_AP_KRW)|	\
	 L1PT_TEX(PTE_TEX_WALLOC))
#define	STARTUP_LARGE_ATTR						\
	(L2PT_BUFFERABLE|L2PT_CACHED|L2PT_SHARED|L2PT_AP(PTE_AP_KRW)|	\
	 L2PT_LARGE_TEX(PTE_TEX_WALLOC))
#define	STARTUP_SMALL_ATTR						\
	(L2PT_BUFFERABLE|L2PT_CACHED|L2PT_SHARED|L2PT_AP(PTE_AP_KRW)|	\
	 L2PT_SMALL_TEX(PTE_TEX_WALLOC))
#endif	/* NCPU == 1 */

/*
 * L1PT attribute used to map device in locore.s.
 * SHARED bit is not require for device page because TEX(0) and uncached
 * mapping will be treated as shared device regardless of shared bit.
 */
#define	STARTUP_L1PTATTR_DEV						\
	(L1PT_BUFFERABLE|L1PT_AP(PTE_AP_KRW)|L1PT_TEX(PTE_TEX_NOALLOC))

/*
 * L1PT attribute for non-executable device mapping.
 * STARTUP_L1PTATTR_DEV_XN is same as STARTUP_L1PTATTR_DEV, except that
 * STARTUP_L1PTATTR_DEV_XN revokes exec permission.
 */
#define	STARTUP_L1PTATTR_DEV_XN		(STARTUP_L1PTATTR_DEV|L1PT_XN)

/* Flags for early_mapinit(). */
#define	EMAP_READONLY		0x1	/* read-only mapping */
#define	EMAP_NOEXEC		0x2	/* disable exec permission */
#define	EMAP_DEVICE		0x4	/* device page */

/*
 * STARTUP_EARLYMAP_DECL(uint32_t va, uint32_t pa, size_t size, uint32_t flags)
 *	Declare initial data to create initial mapping.
 *		va:	Base virtual address for new mapping.
 *		pa:	Base physical address to be mapped.
 *		size:	Size of mapping.
 *		flags:	Flags for early_mapinit()
 */
#define	STARTUP_EARLYMAP_DECL(va, pa, size, flags)			\
	.word	(va);		/* virtual address */			\
	.word	(pa);		/* physical address */			\
	.word	(size);		/* size */				\
	.word	(flags)		/* flags for early_mapinit() */

/* Terminator of initial mapping data array. */
#define	STARTUP_EARLYMAP_END()			\
	STARTUP_EARLYMAP_DECL(0, 0, 0, 0)


/*
 * STARTUP_KERNEL_MAP_DECL()
 *	Declare the kernel mapping to be created at bootstrap.
 */
#ifdef	XRAMDEV_CONFIG
/*
 * If XRAMDEV_CONFIG is defined, STARTUP_KERNEL_MAP_DECL() does nothing
 * because kernel text and data are not contiguous. locore.s will map
 * each section without initial data set.
 */
#define	STARTUP_KERNEL_MAP_DECL()
#else	/* !XRAMDEV_CONFIG */
#define	STARTUP_KERNEL_MAP_DECL()					\
	STARTUP_EARLYMAP_DECL(KERNELBASE, KERNELPHYSBASE,		\
			      STARTUP_RAM_MAPSIZE, 0)
#endif	/* XRAMDEV_CONFIG */

#endif	/* _ASM */

/* Import platform-specific definitions. */
#include <sys/mach_boot_impl.h>

#ifndef	STARTUP_RAM_MAPSIZE
/*
 * Size of initial mapping of SDRAM area.
 */
#define	STARTUP_RAM_MAPSIZE		UINT32_C(0x4000000)	/* 64MB */
#endif	/* !STARTUP_RAM_MAPSIZE */

/* Highest address boundary for memory that BOP_ALLOC() will return. */
#define	BOP_ALLOC_LIMIT							\
	(KERNELBASE +							\
	 MIN(mmu_ptob(PLATFORM_MIN_PHYSMEM), STARTUP_RAM_MAPSIZE))

#endif	/* !_SYS_MACH_BOOT_H */
