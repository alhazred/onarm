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
 * Copyright (c) 2007-2009 NEC Corporation
 * All rights reserved.
 */

#ifndef	_SYS_MACHPARAM_IMPL_H
#define	_SYS_MACHPARAM_IMPL_H

#ident	"@(#)armpf/sys/machparam_impl.h"

/*
 * machparam_impl.h:
 *	Kernel build tree private definitions for machine-dependant parameters.
 */

#ifndef	_SYS_MACHPARAM_H
#error	Do NOT include machparam_impl.h directly.
#endif	/* !_SYS_MACHPARAM_H */

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

#ifndef	_ASM
#include <sys/sysmacros.h>
#endif	/* !_ASM */
#include <sys/pte.h>
#include <sys/platform.h>

/*
 * Define upper limit on user address space.
 */
#define	USERLIMIT	KERNELBASE
#define	USERLIMIT32	USERLIMIT

/*
 * KERNELPHYSBASE is the physical address which the kernel image is loaded.
 */
#ifdef	ARMPF_SDRAM0_SPLIT

/*
 * The kernel is loaded in the middle of SDRAM0.
 */
#ifdef	XRAMDEV_CONFIG
#define	UNIX_CTF_RESV_SIZE	UINT32_C(0)
#else	/* !XRAMDEV_CONFIG */
/*
 * In this case, we reserve 256 kilobytes memory just above the kernel
 * for unix CTF data.
 */
#define	UNIX_CTF_RESV_SIZE	UINT32_C(0x40000)	/* 256KB */
#endif	/* XRAMDEV_CONFIG */

#ifndef	_ASM
#define	KERNELPHYSBASE							\
	P2ROUNDUP(ARMPF_SDRAM0_PADDR + UNIX_CTF_RESV_SIZE, L1PT_SECTION_VSIZE)
#endif	/* !_ASM */

#else	/* !ARMPF_SDRAM0_SPLIT */

/* The kernel is loaded at the start of SDRAM0 */
#ifndef	_ASM
#define	KERNELPHYSBASE		ARMPF_SDRAM0_PADDR
#endif	/* !_ASM */

#endif	/* ARMPF_SDRAM0_SPLIT */

/*
 * Physical address of vector page.
 * This page will be mapped to high vector address.
 */
#define	VECTOR_PAGE_PADDR	(KERNELPHYSBASE + MMU_PAGESIZE)

/* Define FIQ stack size */
#define	FIQ_MODE_STACKSZ	0x20		/* Size of FIQ mode stack */
#define	FIQ_STACKSZ		0x200		/* Size of FIQ stack */

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* !_SYS_MACHPARAM_IMPL_H */
