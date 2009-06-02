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

#ifndef _SYS_PG_IMPL_H
#define	_SYS_PG_IMPL_H

#pragma ident	"@(#)pg_impl.h"

/*
 * pg_impl.h: Kernel build tree private definitions for Processor Groups.
 */
#ifndef	_PG_H
#error	Do NOT include pg_impl.h directly.
#endif	/* !_PG_H */

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	CMT_SCHED_DISABLE

/*
 * PG CPU reconfiguration hooks
 */
#define	pg_cpu0_reinit()
#define	pg_cpu_init(cp)
#define	pg_cpu_fini(cp)
#define	pg_cpu_active(cp)
#define	pg_cpu_inactive(cp)
#define	pg_cpu_bootstrap(cp)

/*
 * PG cpupart service hooks
 */
#define	pg_cpupart_in(cp, pp)
#define	pg_cpupart_out(cp, pp)
#define	pg_cpupart_move(cp, oldpp, newpp)

#undef	PG_PART_BITSET_INIT
#undef	PG_PART_BITSET_FINI
#undef	PG_PART_BITSET_IS_NULL
#define	PG_PART_BITSET_INIT(b)
#define	PG_PART_BITSET_FINI(b)
#define	PG_PART_BITSET_IS_NULL(b)	(1)

#endif	/* CMT_SCHED_DISABLE */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PG_IMPL_H */
