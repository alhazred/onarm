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
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_LOADAVG_H
#define	_SYS_LOADAVG_H

#pragma ident	"@(#)loadavg.h	1.2	05/06/08 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	LOADAVG_1MIN	0
#define	LOADAVG_5MIN	1
#define	LOADAVG_15MIN	2

#define	LOADAVG_NSTATS	3

#ifdef _KERNEL

extern int getloadavg(int *, int);

#else	/* _KERNEL */

/*
 * This is the user API
 */
extern int getloadavg(double [], int);

/*
 * This is the system call that implements it.
 * Do not invoke this directly.
 */
extern int __getloadavg(int *, int);

#endif	/* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_LOADAVG_H */