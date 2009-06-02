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

#ifndef _SYS_SYSARM_H
#define	_SYS_SYSARM_H

#pragma ident	"@(#)sysarm.h"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Commands for sysarm system call (1-?)
 */

#define SARM_IC_INVALL         1     /* Invalidate all I cache line */
#define SARM_IC_INVVADDR       2     /* Invalidate apointed I cache line */
#define SARM_DC_FLUSHALL       3     /* Flush all D cache line */
#define SARM_DC_FLUSHVADDR     4     /* Flush apointed D cache line */
#define SARM_CYCLECOUNT_LOG    5     /* Cycle Counter Register log routine */
#define SARM_SYSINIT_DONE      6     /* init(1M) reports system has
					already initialized. */

/*
 * Subcommands for SARM_CYCLECOUNT_LOG
 */
#define	SARM_CYCLECOUNT_LOG_START	1	/* Start log */
#define	SARM_CYCLECOUNT_LOG_STOP	2	/* Stop log */
#define	SARM_CYCLECOUNT_LOG_DATA	3	/* Log data force */
#define	SARM_CYCLECOUNT_LOG_DATAW	4	/* Log data while active */


#ifndef _KERNEL
#ifdef __STDC__
extern	int	sysarm(int, ...);
#else
extern	int	sysarm();
#endif	/* __STDC__ */
#endif	/* !_KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SYSARM_H */
