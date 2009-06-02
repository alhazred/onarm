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
 * Copyright (c) 2007-2008 NEC Corporation
 * All rights reserved.
 */

#ifndef _SYS_LWP_PRIVATE_H
#define	_SYS_LWP_PRIVATE_H

#pragma ident	"@(#)lwp_private.h"

#include <sys/proc.h>
#include <sys/thread.h>
 
#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	_KERNEL

/* Start function of child thread. */
#define	LWP_RTT(t, cp)					\
	((cp)->p_lwpcnt == 0 ? lwp_rtt_initial: lwp_rtt)

/* Copy parent's struct regs to child. */
#define	LWP_FORKREGS(t, lwp, clwp)			\
	lwp_forkregs(lwp, clwp);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_LWP_PRIVATE_H */
