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

#ifndef _CZFS_ARC_H
#define	_CZFS_ARC_H

#pragma ident	"@(#)czfs:arc.h"

#include <sys/czfs_conf.h>
#include <../zfs/sys/arc.h>

#undef	KMEM_ARC_BUF_HDR_T
#undef	KMEM_ARC_BUF_T

#define	KMEM_ARC_BUF_HDR_T	"czfs_arc_buf_hdr_t"
#define	KMEM_ARC_BUF_T		"czfs_arc_buf_t"

#define	ARC_LOWER_LIMIT		(1<<20)		/* 1MB */

#define	BUF_EMPTY(buf)						\
	((buf)->b_dva.dva_word[0] == 0 &&			\
	(buf)->b_birth == 0)

#define	BUF_EQUAL(spa, dva, birth, buf)				\
	((buf)->b_dva.dva_word[0] == (dva)->dva_word[0]) &&	\
	((buf)->b_birth == birth) && ((buf)->b_spa == spa)

typedef struct arc_stats {
	uint64_t	arcstat_size;
	uint64_t	arcstat_p;
	uint64_t	arcstat_c;
	uint64_t	arcstat_c_min;
	uint64_t	arcstat_c_max;
} arc_stats_t;

#define	ARCSTAT(stat)	(arc_stats.stat)

#define	ARCSTAT_INCR(stat, val)

#define	ARCSTAT_BUMP(stat)
#define	ARCSTAT_BUMPDOWN(stat)

#define	ARCSTAT_MAX(stat, val)
#define	ARCSTAT_MAXSTAT(stat)

#define	ARCSTAT_CONDSTAT(cond1, stat1, notstat1, cond2, stat2, notstat2, stat)

#define	kstat_create(module, instance, name, class, type, ndata, flag)  NULL
#define	kstat_install(ksp)
#define	kstat_delete(ksp)

#endif	/* _CZFS_ARC_H */
