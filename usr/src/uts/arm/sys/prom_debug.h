/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2006-2007 NEC Corporation
 */

#ifndef	_SYS_PROM_DEBUG_H
#define	_SYS_PROM_DEBUG_H

#ident	"@(#)arm/sys/prom_debug.h"

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

#ifdef	_KERNEL

#ifdef	DEBUG

#include <sys/types.h>
#include <sys/bootconf.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <asm/cpufunc.h>

extern int	prom_debug;
extern lock_t	prom_debug_lock;

extern const char	*prom_debug_filepath(const char *file);

#define	PRM_DEBUG_LOCK(x, cookie)			\
	do {						\
		(x) = DISABLE_IRQ_SAVE();		\
		(cookie) = 0;				\
		while (!lock_try(&prom_debug_lock)) {	\
			(cookie)++;			\
			if ((cookie) > 0x10000) {	\
				/* Give up. */		\
				(cookie) = -1;		\
				break;			\
			}				\
		}					\
	} while (0)

#define	PRM_DEBUG_UNLOCK(x, cookie)			\
	do {						\
		if ((cookie) >= 0) {			\
			lock_clear(&prom_debug_lock);	\
		}					\
		RESTORE_INTR(x);			\
	} while (0)

#define	PRM_DEBUG(q)							\
	do {								\
		const char *__fmt = (sizeof (q) >> 3)			\
			? "cpu%d:0x%08lx:%s:%d: '%s' is 0x%llx\n"	\
			: "cpu%d:0x%08lx:%s:%d: '%s' is 0x%x\n";	\
		if (prom_debug) {					\
			uint32_t __x;					\
			int	__cookie;				\
			PRM_DEBUG_LOCK(__x, __cookie);			\
			prom_printf(__fmt, HARD_PROCESSOR_ID(), lbolt,	\
				    prom_debug_filepath(__FILE__),	\
				    __LINE__, #q, q);			\
			PRM_DEBUG_UNLOCK(__x, __cookie);		\
		}							\
	} while (0)

#define	PRM_POINT(q)							\
	do {								\
		if (prom_debug) {					\
			uint32_t __x;					\
			int	__cookie;				\
			PRM_DEBUG_LOCK(__x, __cookie);			\
			prom_printf("cpu%d:0x%08lx:%s:%d: %s\n",	\
				    HARD_PROCESSOR_ID(), lbolt,		\
				    prom_debug_filepath(__FILE__),	\
				    __LINE__, q);			\
			PRM_DEBUG_UNLOCK(__x, __cookie);		\
		}							\
	} while (0)

#define	PRM_PRINTF(...)							\
	do {								\
		if (prom_debug) {					\
			uint32_t __x;					\
			int	__cookie;				\
			PRM_DEBUG_LOCK(__x, __cookie);			\
			prom_printf("cpu%d:0x%08lx:%s:%d: ",		\
				    HARD_PROCESSOR_ID(), lbolt,		\
				    prom_debug_filepath(__FILE__),	\
				    __LINE__);				\
			prom_printf(__VA_ARGS__);			\
			PRM_DEBUG_UNLOCK(__x, __cookie);		\
		}							\
	} while (0)

#else	/* !DEBUG */

#define	PRM_DEBUG(q)

#define	PRM_POINT(q)

#define	PRM_PRINTF(...)

#endif	/* DEBUG */

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* !_SYS_PROM_DEBUG_H */
