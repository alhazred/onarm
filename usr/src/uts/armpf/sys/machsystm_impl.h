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

#ifndef	_SYS_MACHSYSTM_IMPL_H
#define	_SYS_MACHSYSTM_IMPL_H

#ident	"@(#)armpf/sys/machsystm_impl.h"

/*
 * machsystm_impl.h:
 *	Kernel build tree private definitions for machine-dependant interfaces.
 */

#ifndef	_SYS_MACHSYSTM_H
#error	Do NOT include machsystm_impl.h directly.
#endif	/* !_SYS_MACHSYSTM_H */

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

/*
 * Convert kernel virtual/physical address.
 * This macro is only available for the kernel space [KERNELBASE, econtig).
 */
#define	KVTOP(va)	((uintptr_t)(va) - KERNELBASE + KERNELPHYSBASE)
#define	PTOKV(pa)	((uintptr_t)(pa) - KERNELPHYSBASE + KERNELBASE)

/*
 * Convert kernel virtual/physical address.
 * This macro can be used only for kernel static data.
 */

#ifdef	XRAMDEV_CONFIG

/*
 * Kernel text and other sections are located in separated area that are
 * not physically contiguous.
 */
extern const uintptr_t	data_paddr;
extern const uintptr_t	data_paddr_base;

#define	KVTOP_DATA(va)		(data_paddr + (KVTOP(va) - data_paddr_base))
#define	PTOKV_DATA(pa)		((uintptr_t)s_data + ((pa) - data_paddr))

/* End virtual address of kernel static data. */
extern caddr_t		e_kstatic;
#define	KSTATIC_END	e_kstatic

#else	/* !XRAMDEV_CONFIG */

#define	KVTOP_DATA(va)		KVTOP(va)
#define	PTOKV_DATA(pa)		PTOKV(pa)

extern caddr_t		econtig;
#define	KSTATIC_END	econtig

#endif	/* XRAMDEV_CONFIG */

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* !_SYS_MACHSYSTM_IMPL_H */
