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

#ifndef	_VM_HAT_MACHDEP_H
#define	_VM_HAT_MACHDEP_H

#ident	"@(#)armpf/vm/hat_machdep.h"

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

/*
 * ARMPF-specific HAT management.
 */

#include <sys/types.h>
#include <sys/cpuvar.h>
#include <sys/xramdev_impl.h>

#ifdef	_KERNEL

extern caddr_t	hat_plat_reserve_space(caddr_t);
extern void	hat_plat_cpu_init(cpu_t *cp);
extern void	hat_plat_mpstart_init(void);
extern void	hat_plat_mpstart_fini(void);
extern void	hat_plat_dump(void);

/*
 * void
 * hat_plat_memattr(pfn_t pfn, uint_t *attrp)
 *	Determine mapping attribute to map the specified memory.
 *	"pfn" must be a page frame number corresponding to normal memory.
 *
 *	Typically, normal memory should be mapped using cache.
 *	But we may need to use special attribute for some part of memory.
 *
 *	hat_plat_memattr() is implemented as macro because all we have to
 *	do here is to check xramfs device page.
 *
 * Calling/Exit State:
 *	The caller of hat_plat_memattr() must set HAT attribute in *attrp.
 *	If we must use the special mapping attripute to map the specified
 *	pfn, hat_plat_memattr() updates attributes in *attrp.
 */
#define	hat_plat_memattr(pfn, attrp)		\
	(void)xramdev_impl_mapattr(pfn, attrp)

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* !_VM_HAT_MACHDEP_H */
