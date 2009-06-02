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

#ifndef	_VM_HMENT_H
#define	_VM_HMENT_H

#ident	"@(#)arm/vm/hment.h"

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

#include <sys/types.h>

struct hat;

#if	defined(_KERNEL) || defined(_KMEMUSER)
/*
 * When pages are shared by more than one mapping, a list of these
 * structs hangs off of the page_t connected by the hm_next and hm_prev
 * fields.  Every hment is also indexed by a system-wide hash table, using
 * hm_hashnext to connect it to the chain of hments in a single hash
 * bucket.
 */
typedef struct hment {
	struct hment	*hm_hashnext;	/* next mapping on hash chain */
	struct hment	*hm_next;	/* next mapping of same page */
	struct hment	*hm_prev;	/* previous mapping of same page */
	pfn_t		hm_pfn;		/* mapping page frame number */
	void		*hm_htable;	/* corresponding hat or hat_l2pt */
	uint16_t	hm_entry;	/* index of pte in htable */
	uint16_t	hm_ptszc;	/* szc of mapping */
} hment_t;
#endif	/* defined(_KERNEL) || defined(_KMEMUSER) */

#if defined(_KERNEL)

/*
 * Remove a page mapping, finds the matching mapping and unlinks it from
 * the page_t. If it returns a non-NULL pointer, the pointer must be
 * freed via hment_free() after doing hment_exit().
 */
extern hment_t *hment_remove(page_t *pp, void *ht, uint16_t ptlevel,
			     uint_t entry);
extern void hment_free(hment_t *hm);

/*
 * Iterator to walk through all mappings of a page.
 */
extern hment_t *hment_walk(page_t *, void **, uint16_t *, uint_t *, hment_t *);

/*
 * Prepare a page for a new mapping
 */
extern hment_t *hment_prepare(struct hat *hat, void *ht, uint16_t ptlevel,
			      uint_t entry, page_t *pp);

/*
 * Add a mapping to a page's mapping list
 */
extern void	hment_assign(void *htable, uint16_t ptlevel, uint_t entry,
			     page_t *pp, hment_t *hm);

/*
 * initialize hment data structures
 */
extern void	hment_boot_alloc(void);
extern void	hment_init(void);

/*
 * lock/unlock a page_t's mapping list/pte entry
 */
extern void	hment_enter(page_t *pp);
extern boolean_t	hment_tryenter(page_t *pp);
extern void	hment_exit(page_t *pp);
extern int	hment_owned(page_t *pp);

/*
 * Used to readjust the hment reserve after the reserve list has been used.
 * Also called after boot to release left over boot reserves.
 */
extern void hment_adjust_reserve(void);

/*
 * Return the number of mappings of a page_t
 */
extern uint_t hment_mapcnt(page_t *);

#endif	/* _KERNEL */


#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* _VM_HMENT_H */
