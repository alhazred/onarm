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

#ifndef	_LGRP_IMPL_H
#define	_LGRP_IMPL_H

#pragma ident	"@(#)lgrp_impl.h"

/*
 * lgrp_impl.h: Kernel build tree private definitions for locality group.
 */
#ifndef	_LGRP_H
#error	Do NOT include lgrp_impl.h directly.
#endif	/* !_LGRP_H */

#ifdef	__cplusplus
extern "C" {
#endif

#if	defined(LGROUP_SINGLE)

#undef	LGRP_MNODE_COOKIE_INIT
#undef	LGRP_MNODE_COOKIE_UPGRADE
#define	LGRP_MNODE_COOKIE_INIT(c, lgrp, scope)	\
	((&(c))->lmc_nodes = (mnodeset_t)1)
#define	LGRP_MNODE_COOKIE_UPGRADE(c)

#undef	LGRP_DEFAULT_LGRPLOADS_ALLOC
#define	LGRP_DEFAULT_LGRPLOADS_ALLOC(n)

#undef	LGRP_ID_TO_LGRP
#define	LGRP_ID_TO_LGRP(lgrpid)	(((lgrpid) == LGRP_ROOTID) ? lgrp_root : NULL)

#define	lgrp_alloc_max		(0)	/* max lgroup ID allocated so far */
#define	nlgrps			(1)	/* number of lgroups in machine */

/*
 * lgroup management
 */
#define	lgrp_optimizations()		(0)
#define	lgrp_init()
#define	lgrp_hand_to_lgrp(hand)		\
	(((hand) == LGRP_DEFAULT_HANDLE) ? lgrp_root : NULL)

/*
 * lgroup stats
 */
#define	lgrp_stat_add(lgrpid, stat, val)

/*
 * lgroup memory
 */
#define	lgrp_madv_to_policy(advice, size, type)	(LGRP_MEM_POLICY_NEXT)
#define	lgrp_mem_choose(seg, vaddr, pgsz)	(lgrp_root)
#define	lgrp_memnode_choose(c)		\
	(((c)->lmc_nodes == (mnodeset_t)0)	\
	 ? (-1) : ((c)->lmc_nodes = (mnodeset_t)0))
#define	lgrp_mem_policy_default(size, type)	(LGRP_MEM_POLICY_NEXT)
#define	lgrp_pfn_to_lgrp(pfn)			(lgrp_root)
#define	lgrp_privm_policy_set(policy, policy_info, size)	\
	((policy_info)->mem_policy = LGRP_MEM_POLICY_NEXT,	\
	 (policy_info)->mem_lgrpid = LGRP_NONE, 1)
#define	lgrp_shm_policy_init(amp, vp)
#define	lgrp_shm_policy_fini(amp, vp)
#define	lgrp_shm_policy_get(amp, anon_idex, vp, vn_off)		\
	((lgrp_mem_policy_info_t *)NULL)
#define	lgrp_shm_policy_set(policy, amp, anon_index, vp, vn_off, len)	(1)

/*
 * lgroup thread placement
 */
#define	lgrp_affinity_best(t, cpupart, start, prefer_start)	((lpl_t *)NULL)
#define	lgrp_affinity_init(buf)		(*(buf) = NULL)
#define	lgrp_affinity_free(buf)
#define	lgrp_choose(t, cp)		(&(cp)->cp_lgrploads[LGRP_ROOTID])
#define	lgrp_home_lgrp()		(lgrp_root)
#define	lgrp_home_id(t)			(LGRP_ROOTID)
#define	lgrp_loadavg(lpl, nrcpus, ageflag)
#define	lgrp_move_thread(t, newlpl, do_lgrpset_delete)	\
	do {							\
		if ((t)->t_lpl != (newlpl) && (newlpl) != NULL)	\
			(t)->t_lpl = (newlpl);			\
	} while (0)
#define	lgrp_get_trthr_migrations()	((uint64_t)0)
#define	lgrp_update_trthr_migrations(i)

/*
 * lpl topology
 */
#define	lpl_topo_bootstrap(target, size)

/* platform interfaces */
#define	lgrp_plat_cpu_to_hand(id)	(LGRP_DEFAULT_HANDLE)
#define	lgrp_plat_max_lgrps()		(1)
#define	lgrp_plat_probe()

#endif	/* LGROUP_SINGLE */

#if	defined(LGROUP_SINGLE) || defined(KSTAT_DISABLE)
#define	lgrp_kstat_create(cp)
#define	lgrp_kstat_destroy(cp)
#endif	/* LGROUP_SINGLE || KSTAT_DISABLE */

#ifdef	__cplusplus
}
#endif

#endif /* _LGRP_H */
