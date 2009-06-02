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

#ifndef	_SYS_X_CALL_H
#define	_SYS_X_CALL_H

#pragma ident	"@(#)x_call.h	1.14	05/06/08 SMI"

#ifndef	_MACHDEP
#error	"sys/x_call.h is only for platform dependant code."
#endif	/* !_MACHDEP */

/*
 * For ARM MPCore, we only have two cross call levels:
 * a low and high. (see xc_levels.h)
 */
#include <sys/xc_levels.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * States of a cross-call session. (stored in xc_state field of the
 * per-CPU area).
 */
#define	XC_DONE		0	/* x-call session done */
#define	XC_HOLD		1	/* spin doing nothing */
#define	XC_SYNC_OP	2	/* perform a synchronous operation */
#define	XC_CALL_OP	3	/* perform a call operation */
#define	XC_WAIT		4	/* capture/release. callee has seen wait */

#ifndef _ASM

#include <sys/cpuvar.h>

typedef intptr_t xc_arg_t;
typedef int (*xc_func_t)(xc_arg_t, xc_arg_t, xc_arg_t);

struct	xc_mbox {
	xc_func_t	func;
	xc_arg_t	arg1;
	xc_arg_t	arg2;
	xc_arg_t	arg3;
	cpuset_t	set;
	int		saved_pri;
};

/*
 * Cross-call routines.
 */
#if defined(_KERNEL)

extern void	xc_init(void);
extern uint_t	xc_serv_hipri(caddr_t);
extern uint_t	xc_serv_lopri(caddr_t);
extern void	xc_call(xc_arg_t, xc_arg_t, xc_arg_t, int, cpuset_t, xc_func_t);
extern void	xc_trycall(xc_arg_t, xc_arg_t, xc_arg_t, cpuset_t, xc_func_t);
extern void	xc_sync(xc_arg_t, xc_arg_t, xc_arg_t, int, cpuset_t, xc_func_t);
extern void	xc_capture_cpus(cpuset_t);
extern void	xc_release_cpus(void);
extern void	xc_lock(void);
extern void	xc_unlock(void);

#endif	/* _KERNEL */

#endif	/* !_ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_X_CALL_H */
