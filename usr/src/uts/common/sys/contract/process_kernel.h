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
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Copyright (c) 2008 NEC Corporation
 */

#ifndef	_SYS_CONTRACT_PROCESS_KERNEL_H
#define	_SYS_CONTRACT_PROCESS_KERNEL_H

#pragma ident	"@(#)process_kernel.h"

/*
 * process_kernel.h: Kernel build tree private definitions for brand.
 */
#ifndef _SYS_CONTRACT_PROCESS_IMPL_H
#error  Do NOT include process_kernel.h directly.
#endif  /* !_SYS_CONTRACT_PROCESS_IMPL_H */

#include <sys/contract/process.h>
#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	CONTRACT_DISABLE

#define	CONT_PROCESS_DUMMY	(cont_process_t *)0xbabecafe

#undef	PRCTID
#define	PRCTID(pp)		(0)

#define	process_type		(NULL)

#define	contract_process_fork(rtmpl, cp, pp, canfail)	\
	((cp)->p_ct_process = CONT_PROCESS_DUMMY)
#define	contract_process_exit(ctp, p, exitstatus)	\
	((p)->p_ct_process = NULL)
#define	contract_process_core(ctp, p, sig, process, global, zone)
#define	contract_process_hwerr(ctp, p)
#define	contract_process_sig(ctp, p, sig, pid, ctid, zoneid)

#endif	/* CONTRACT_DISABLE */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CONTRACT_PROCESS_KERNEL_H */
