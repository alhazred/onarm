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

#pragma ident	"@(#)lgrp_single.c"

/*
 * UMA optimization of locality groups
 */

#include <sys/lgrp.h>
#include <sys/lgrp_user.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/thread.h>
#include <sys/cpuvar.h>
#include <sys/cpupart.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/mutex.h>

static lgrp_t	lroot = {
	0, 			/* lgrp_id */
	0,			/* lgrp_latency */
	LGRP_DEFAULT_HANDLE,	/* lgrp_plathand */
	NULL,			/* lgrp_parent */
	0,			/* lgrp_reserved1 */
	0,			/* lgrp_childcnt */
	(klgrpset_t)0,		/* lgrp_children */
	(klgrpset_t)1,		/* lgrp_leaves */
	{
		(klgrpset_t)1,
		(klgrpset_t)1
	},			/* lgrp_set[LGRP_RSRC_COUNT] */
	(mnodeset_t)1,		/* lgrp_mnodes */
	1,			/* lgrp_nmnodes */
	0,			/* lgrp_reserved2 */
	NULL,			/* lgrp_cpu */
	0,			/* lgrp_cpucnt */
	NULL			/* lgrp_kstat */
};

/*
 * The root lgroup. Represents the set of resources at the system wide
 * level of locality.
 */
lgrp_t		*lgrp_root = &lroot;

/*
 * Default lpl for cp_default.
 * On UMA machine, cp_default needs 1-element lpl list.
 */
static lpl_t		lpl_default;

/*
 * Set the default memory allocation policy.  For most platforms,
 * next touch is sufficient, but some platforms may wish to override
 * this.
 */
lgrp_mem_policy_t	lgrp_mem_default_policy = LGRP_MEM_POLICY_NEXT;

/*
 * lgroup CPU partition event handlers
 */
static void	lgrp_part_add_cpu(struct cpu *);
static void	lgrp_part_del_cpu(struct cpu *);

/*
 * Create the root and cpu0's lgroup, and set t0's home.
 */
void
lgrp_setup(void)
{
	/*
	 * Setup the root lgroup
	 */
	t0.t_lpl = &lpl_default;
	cp_default.cp_nlgrploads = 1;
	cp_default.cp_lgrploads = &lpl_default;

	/*
	 * Add cpu0 to an lgroup
	 */
	/* lgrp_config(LGRP_CONFIG_CPU_ADD, (uintptr_t)CPU_GLOBAL, 0); */
	lgrp_config(LGRP_CONFIG_CPU_ONLINE, (uintptr_t)CPU_GLOBAL, 0);
}

/*
 * true when lgrp initialization has been completed.
 */
int	lgrp_initialized = 0;

/*
 * Init routine called after startup(), /etc/system has been processed,
 * and cpu0 has been added to an lgroup.
 */
void
lgrp_main_init(void)
{
	lgrp_initialized = 1;
}

/*
 * Finish lgrp initialization after all CPUS are brought on-line.
 * This routine is called after start_other_cpus().
 */
void
lgrp_main_mp_init(void)
{
}

/*
 * Handle lgroup (re)configuration events (eg. addition of CPU, etc.)
 */
void
lgrp_config(lgrp_config_flag_t event, uintptr_t resource, uintptr_t where)
{
	cpu_t		*cp;

	switch (event) {
	case LGRP_CONFIG_CPU_ADD:
		cp = (cpu_t *)resource;
		cp->cpu_next_lpl = cp;
		cp->cpu_prev_lpl = cp;
		cp->cpu_lpl = &lpl_default;
		break;

	case LGRP_CONFIG_CPU_DEL:
		/* nothing to do */
		break;

	case LGRP_CONFIG_CPUPART_ADD:
		ASSERT((lgrp_id_t)where == LGRP_ROOTID);
	case LGRP_CONFIG_CPU_ONLINE:
		lgrp_part_add_cpu((cpu_t *)resource);
		break;

	case LGRP_CONFIG_CPU_OFFLINE:
	case LGRP_CONFIG_CPUPART_DEL:
		lgrp_part_del_cpu((cpu_t *)resource);
		break;

	case LGRP_CONFIG_MEM_ADD:
		ASSERT(where == LGRP_DEFAULT_HANDLE);
		ASSERT(resource == 0);
		/* nothing to do */
		break;

	case LGRP_CONFIG_MEM_DEL:
	case LGRP_CONFIG_MEM_RENAME:
	case LGRP_CONFIG_GEN_UPDATE:
	case LGRP_CONFIG_FLATTEN:
	case LGRP_CONFIG_LAT_CHANGE_ALL:
	case LGRP_CONFIG_LAT_CHANGE:
		ASSERT(0);	/* should not be called */
		break;

	case LGRP_CONFIG_NOP:
	default:
		break;
	}
}

/*
 * add a cpu to a partition in terms of lgrp load avg bookeeping
 */
void
lgrp_part_add_cpu(cpu_t *cp)
{
	cpupart_t	*cpupart;
	lpl_t		*lpl_leaf;

	/* called sometimes w/ cpus paused - grab no locks */
	ASSERT(MUTEX_HELD(&cpu_lock) || !lgrp_initialized);

	cpupart = cp->cpu_part;

	lpl_leaf = &cpupart->cp_lgrploads[LGRP_ROOTID];
	cp->cpu_lpl = lpl_leaf;

	/* link cpu into list of cpus in lpl */

	if (lpl_leaf->lpl_ncpu++ == 0) {
		lpl_leaf->lpl_lgrpid = LGRP_ROOTID;
		lpl_leaf->lpl_loadavg = 0;
		lpl_leaf->lpl_ncpu = 1;
		lpl_leaf->lpl_nrset = 1;
		lpl_leaf->lpl_rset[0] = lpl_leaf;
		lpl_leaf->lpl_lgrp = lgrp_root;
		lpl_leaf->lpl_parent = NULL;
		lpl_leaf->lpl_cpus = cp->cpu_next_lpl = cp->cpu_prev_lpl = cp;
	} else {
		ASSERT(lpl_leaf->lpl_cpus);
		cp->cpu_next_lpl = lpl_leaf->lpl_cpus;
		cp->cpu_prev_lpl = lpl_leaf->lpl_cpus->cpu_prev_lpl;
		lpl_leaf->lpl_cpus->cpu_prev_lpl->cpu_next_lpl = cp;
		lpl_leaf->lpl_cpus->cpu_prev_lpl = cp;
	}
}

/*
 * remove a cpu from a partition in terms of lgrp load avg bookeeping
 */
void
lgrp_part_del_cpu(cpu_t *cp)
{
	lpl_t		*lpl;

	/* called sometimes w/ cpus paused - grab no locks */

	ASSERT(MUTEX_HELD(&cpu_lock) || !lgrp_initialized);

	lpl = cp->cpu_lpl;

	/* don't delete a leaf that isn't there */
	ASSERT(LGRP_EXISTS(lpl->lpl_lgrp));

	/* no double-deletes */
	ASSERT(lpl->lpl_ncpu);
	if (--lpl->lpl_ncpu == 0) {
		/* eliminate remaning lpl link pointers in cpu, lpl */
		lpl->lpl_cpus = cp->cpu_next_lpl = cp->cpu_prev_lpl = NULL;

		ASSERT(lpl->lpl_lgrpid == LGRP_ROOTID);
		bzero(lpl, sizeof (lpl_t));
		lpl->lpl_lgrpid = LGRP_ROOTID;
	} else {
		/* unlink cpu from lists of cpus in lpl */
		cp->cpu_prev_lpl->cpu_next_lpl = cp->cpu_next_lpl;
		cp->cpu_next_lpl->cpu_prev_lpl = cp->cpu_prev_lpl;
		if (lpl->lpl_cpus == cp) {
			lpl->lpl_cpus = cp->cpu_next_lpl;
		}
	}
	/* clear cpu's lpl ptr when we're all done */
	cp->cpu_lpl = NULL;
}
