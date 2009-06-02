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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2006 NEC Corporation
 */

#ident	"@(#)arm/os/lgrpplat.c"

#include <sys/archsystm.h>
#include <sys/cmn_err.h>
#include <sys/cpupart.h>
#include <sys/cpuvar.h>
#include <sys/lgrp.h>
#include <sys/machsystm.h>
#include <sys/memlist.h>
#include <sys/memnode.h>
#include <sys/mman.h>
#include <sys/pci_cfgspace.h>
#include <sys/pci_impl.h>
#include <sys/param.h>
#include <sys/promif.h>		/* for prom_printf() */
#include <sys/systm.h>
#include <sys/thread.h>
#include <sys/types.h>

/*
 * NUMA is not supported on ARM architecture.
 */

#define	MAX_NODES		1
#define	NLGRP			(MAX_NODES * (MAX_NODES - 1) + 1)

/*
 * Allocate lgrp and lgrp stat arrays statically.
 */
static lgrp_t	lgrp_space[NLGRP];
static int	nlgrps_alloc;

struct lgrp_stats lgrp_stats[NLGRP];

/*
 * Platform-specific initialization of lgroups
 */
void
lgrp_plat_init(void)
{
	max_mem_nodes = 1;
}

/*
 * Probe memory in each node from current CPU to determine latency topology
 */
void
lgrp_plat_probe(void)
{
	/* Do nothing because we have only one memnode. */
}


/*
 * Platform-specific initialization
 */
void
lgrp_plat_main_init(void)
{
	/* Do nothing because we have only one memnode. */
}

/*
 * Allocate additional space for an lgroup.
 */
/* ARGSUSED */
lgrp_t *
lgrp_plat_alloc(lgrp_id_t lgrpid)
{
	lgrp_t *lgrp;

	lgrp = &lgrp_space[nlgrps_alloc++];
	if (lgrpid >= NLGRP || nlgrps_alloc > NLGRP)
		return (NULL);
	return (lgrp);
}

/*
 * Platform handling for (re)configuration changes
 */
/* ARGSUSED */
void
lgrp_plat_config(lgrp_config_flag_t flag, uintptr_t arg)
{
}

/*
 * Return the platform handle for the lgroup containing the given CPU
 */
/* ARGSUSED */
lgrp_handle_t
lgrp_plat_cpu_to_hand(processorid_t id)
{
	return LGRP_DEFAULT_HANDLE;
}

/*
 * Return the platform handle of the lgroup that contains the physical memory
 * corresponding to the given page frame number
 */
/* ARGSUSED */
lgrp_handle_t
lgrp_plat_pfn_to_hand(pfn_t pfn)
{
	return LGRP_DEFAULT_HANDLE;
}

/*
 * Return the maximum number of lgrps supported by the platform.
 * Before lgrp topology is known it returns an estimate based on the number of
 * nodes. Once topology is known it returns the actual maximim number of lgrps
 * created. Since x86 doesn't support dynamic addition of new nodes, this number
 * may not grow during system lifetime.
 */
int
lgrp_plat_max_lgrps()
{
	return 1;
}

/*
 * Return the number of free pages in an lgroup.
 *
 * For query of LGRP_MEM_SIZE_FREE, return the number of base pagesize
 * pages on freelists.  For query of LGRP_MEM_SIZE_AVAIL, return the
 * number of allocatable base pagesize pages corresponding to the
 * lgroup (e.g. do not include page_t's, BOP_ALLOC()'ed memory, ..)
 * For query of LGRP_MEM_SIZE_INSTALL, return the amount of physical
 * memory installed, regardless of whether or not it's usable.
 */
pgcnt_t
lgrp_plat_mem_size(lgrp_handle_t plathand, lgrp_mem_query_t query)
{
	struct memlist *mlist;
	pgcnt_t npgs = (pgcnt_t)0;
	extern struct memlist *phys_avail;
	extern struct memlist *phys_install;

	if (plathand == LGRP_DEFAULT_HANDLE) {
		switch (query) {
		case LGRP_MEM_SIZE_FREE:
			npgs = (pgcnt_t)freemem;
			break;

		case LGRP_MEM_SIZE_AVAIL:
			memlist_read_lock();
			for (mlist = phys_avail; mlist; mlist = mlist->next) {
				npgs += btop(mlist->size);
			}
			memlist_read_unlock();
			break;

		case LGRP_MEM_SIZE_INSTALL:
			memlist_read_lock();
			for (mlist = phys_install; mlist;
			     mlist = mlist->next) {
				npgs += btop(mlist->size);
			}
			memlist_read_unlock();
			break;

		default:
			break;
		}
	}

	return npgs;
}

/*
 * Return latency between "from" and "to" lgroups
 *
 * This latency number can only be used for relative comparison
 * between lgroups on the running system, cannot be used across platforms,
 * and may not reflect the actual latency.  It is platform and implementation
 * specific, so platform gets to decide its value.  It would be nice if the
 * number was at least proportional to make comparisons more meaningful though.
 */
/* ARGSUSED */
int
lgrp_plat_latency(lgrp_handle_t from, lgrp_handle_t to)
{
	return 0;
}

/*
 * Return platform handle for root lgroup
 */
lgrp_handle_t
lgrp_plat_root_hand(void)
{
	return (LGRP_DEFAULT_HANDLE);
}
