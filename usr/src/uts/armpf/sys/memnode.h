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
 * Copyright (c) 2006-2008 NEC Corporation
 */

#ifndef _SYS_MEMNODE_H
#define	_SYS_MEMNODE_H

#pragma ident	"@(#)memnode.h	1.9	05/08/24 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	_KERNEL

#include <sys/lgrp.h>


/*
 * This file defines the mappings between physical addresses and memory
 * nodes. Memory nodes are defined so that the low-order bits are the
 * memory slice ID and the high-order bits are the SSM nodeid.
 */

/* ARM platform implementation has only one memory node. */
#ifndef	MAX_MEM_NODES
#define	MAX_MEM_NODES	1
#endif	/* MAX_MEM_NODES */

#define	PFN_2_MEM_NODE(pfn)			0
#define	MEM_NODE_2_LGRPHAND(mnode)		LGRP_DEFAULT_HANDLE

/*
 * Platmod hooks
 */

extern void plat_assign_lgrphand_to_mem_node(lgrp_handle_t, int);

struct	mem_node_conf {
	int	exists;		/* only try if set, list may still be empty */
	pfn_t	physbase;	/* lowest PFN in this memnode */
	pfn_t	physmax;	/* highest PFN in this memnode */
};

struct memlist;

extern void startup_build_mem_nodes(struct memlist *);
extern void mem_node_add_slice(pfn_t, pfn_t);
extern void mem_node_pre_del_slice(pfn_t, pfn_t);
extern void mem_node_post_del_slice(pfn_t, pfn_t, int);
extern int mem_node_alloc(void);
extern pgcnt_t mem_node_memlist_pages(int, struct memlist *);


extern struct mem_node_conf	mem_node_config[];
extern uint64_t			mem_node_physalign;
extern int			mem_node_pfn_shift;
extern int			max_mem_nodes;

extern uint_t			lgrp_plat_node_cnt;

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MEMNODE_H */
