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

#ifndef	_SYS_WDT_H
#define	_SYS_WDT_H

#ident	"@(#)arm/sys/wdt.h"

/*
 * Prototypes only for WDT_ENABLE.
 */
#ifdef	WDT_ENABLE

extern void wdt_init(void);
extern void wdt_mp_init(void);
extern void wdt_refresh_init(void);
extern void wdt_start(processorid_t cpun);
extern void wdt_stop(processorid_t cpun);

#else	/* !WDT_ENABLE */

#define	wdt_init()			/* nop */
#define	wdt_mp_init()			/* nop */
#define	wdt_refresh_init()		/* nop */
#define	wdt_start(cpun)			/* nop */
#define	wdt_stop(cpun)			/* nop */

#endif	/* WDT_ENABLE */

#endif	/* !_SYS_WDT_H */
