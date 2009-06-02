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
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Copyright (c) 2008 NEC Corporation
 */

#ifndef _SYS_BRAND_IMPL_H
#define	_SYS_BRAND_IMPL_H

#pragma ident	"@(#)brand_impl.h"

/*
 * brand_impl.h: Kernel build tree private definitions for brand.
 */
#ifndef	_SYS_BRAND_H
#error	Do NOT include brand_impl.h directly.
#endif	/* !_SYS_BRAND_H */

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	BRAND_DISABLE

#undef	PROC_IS_BRANDED
#undef	ZONE_IS_BRANDED
#undef	BROP
#undef	ZBROP
#undef	ZBRINIT
#undef	ZBRNAME

#define	PROC_IS_BRANDED(p)	(0)
#define	ZONE_IS_BRANDED(z)	(0)
#define	BROP(p)			((struct brand_ops *)NULL)
#define	ZBROP(z)		((struct brand_ops *)NULL)
#define	ZBRINIT(z)		((z)->zone_brand = (brand_t *)0xbabecafe)
#define	ZBRNAME(z)		"native"

#define	brand_init()			(p0.p_brand = (brand_t *)0xbabecafe)
#define	brand_register(brand)		(EINVAL)
#define	brand_unregister(brand)		(EINVAL)
#define	brand_register_zone(attr)	(NULL)
#define	brand_unregister_zone(brand)
#define	brand_setbrand(p)

#endif	/* BRAND_DISABLE */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_BRAND_IMPL_H */
