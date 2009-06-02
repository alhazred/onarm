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

#ifndef	_SYS_DISP_IMPL_H
#define	_SYS_DISP_IMPL_H

#pragma	ident	"@(#)disp_impl.h"

/*
 * disp_impl.h: Kernel build tree private definitions for dispatcher.
 */
#ifndef	_SYS_DISP_H
#error	Do NOT include disp_impl.h directly.
#endif	/* !_SYS_DISP_H */

#ifdef	__cplusplus
extern "C" {
#endif

#define	SETDQ_FLAG_SET(t, flag)
#define	SETDQ_FLAG_CLR(t)

#define	DISP_STROLL_END(lpl)		(0)
#define	DISP_STEAL_IMMEDIATELY(cp)	(0)
#define	DISP_CHOOSE_LAST_CPU(t)		(0)

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DISP_IMPL_H */
