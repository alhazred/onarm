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

#ifndef	_SYS_PGHW_IMPL_H
#define	_SYS_PGHW_IMPL_H

#pragma ident	"@(#)pghw_impl.h"

/*
 * pghw_impl.h: Kernel build tree private definitions for Processor Groups.
 */
#ifndef	_PGHW_H
#error	Do NOT include pghw_impl.h directly.
#endif	/* !_PGHW_H */

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	CMT_SCHED_DISABLE

/*
 * Physical ID cache creation / destruction
 */
#define	pghw_physid_create(cp)
#define	pghw_physid_destroy(cp)

#endif	/* CMT_SCHED_DISABLE */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_PGHW_IMPL_H */
