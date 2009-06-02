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

#ifndef	_SYS_MACH_L220_H
#define	_SYS_MACH_L220_H

#ident	"@(#)armpf/sys/mach_l220.h"

/*
 * ARMPF-specific L220 definitions.
 * Kernel build tree private.
 */

#include <sys/platform.h>

/* Wrapper macros for L220 interfaces. */

#if	ARMPF_L220_EXIST == 0

/*
 * We know this platform doesn't have L220 level 2 cache.
 * So we can omit L220 I/F call.
 */

/* Define dummy L220 attributes */
#include <sys/machparam.h>
#define	L220_WAYSIZE	MMU_PAGESIZE

#define	CACHE_L220_INIT()
#define	CACHE_L220_FLUSH(vaddr, size, op)

#else	/* ARMPF_L220_EXIST != 0 */

/* Machine-dependant L220 attributes */
#define	L220_ASSOCIATIVITY	ARMPF_L220_ASSOCIATIVITY
#define	L220_WAY_BITS		((1U << L220_ASSOCIATIVITY) - 1)
#define	L220_WAY_PARAM		ARMPF_L220_WAY_PARAM
#define	L220_WAYSIZE		(1U << (13 + L220_WAY_PARAM))
#define	L220_LAT_DATA_READ	ARMPF_L220_LAT_DATA_READ
#define	L220_LAT_DATA_WRITE	ARMPF_L220_LAT_DATA_WRITE
#define	L220_LAT_TAG		ARMPF_L220_LAT_TAG
#define	L220_LAT_DIRTY		ARMPF_L220_LAT_DIRTY

#define	CACHE_L220_INIT()			\
	cache_l220_init()

#define	CACHE_L220_FLUSH(vaddr, size, op)	\
	cache_l220_flush(vaddr, size, op)

#endif	/* ARMPF_L220_EXIST == 0 */

#endif	/* !_SYS_MACH_L220_H */
