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
 * Copyright (c) 2007 NEC Corporation
 * All rights reserved.
 */

#ident	"@(#)arm/os/toxic_range.c"

/*
 * Keep track of kernelheap allocation for toxic mapping.
 */

#include <sys/types.h>
#include <sys/mutex.h>
#include <sys/kmem.h>
#include <sys/param.h>
#include <sys/debug.h>
#include <sys/archsystm.h>
#include <sys/sysmacros.h>
#include <vm/seg_kmem.h>

struct toxic_range;
typedef struct toxic_range	toxic_range_t;

struct toxic_range {
	uintptr_t	tr_addr;	/* base address */
	size_t		tr_size;	/* size of range */
	toxic_range_t	*tr_next;
};

/*
 * List of toxic mappings.
 * struct toxic_range are sorted in address order.
 */
static toxic_range_t	*toxic_head;

static kmutex_t		toxic_lock;

/* Set true if toxic_range list is changed. */
static volatile uint8_t	toxic_changed;

/*
 * boolean_t
 * toxic_range_alloc(uintptr_t addr, size_t size, int vmflags)
 *	Mark the specified address range as "toxic mapping".
 *	The specified address range must be within heap_arena.
 *
 *	The caller must guarantee that the specified address range is
 *	not marked as "toxic mapping".
 *
 * Calling/Exit State:
 *	Upon successful completion, toxic_range_alloc() returns B_TRUE.
 *	Otherwise B_FALSE.
 *
 *	Note that toxic_range_alloc() never validate the specified address
 *	range. If invalid address range is specified on the debug kernel,
 *	it may cause assertion failure.
 */
boolean_t
toxic_range_alloc(uintptr_t addr, size_t size, int vmflags)
{
	toxic_range_t	**trpp, *new;
	int		kmflags = vmflags & VM_KMFLAGS;

	size = P2ROUNDUP(size, PAGESIZE);

	ASSERT(addr >= (uintptr_t)kernelheap &&
	       (addr + size) <= (uintptr_t)ekernelheap);

	if ((new = kmem_alloc(sizeof(toxic_range_t), kmflags)) == NULL) {
		return B_FALSE;
	}

	new->tr_addr = addr;
	new->tr_size = size;

	mutex_enter(&toxic_lock);

	/* Sort in address order. */
	for (trpp = &toxic_head; *trpp != NULL; trpp = &((*trpp)->tr_next)) {
		toxic_range_t	*trp = *trpp;

		if (trp->tr_addr > addr) {
			ASSERT(addr + size <= trp->tr_addr);
			break;
		}
		else {
			ASSERT(addr != trp->tr_addr);
			ASSERT(trp->tr_addr + trp->tr_size <= addr);
		}
	}

	new->tr_next = *trpp;
	*trpp = new;
	toxic_changed = 1;

	mutex_exit(&toxic_lock);

	return B_TRUE;
}

/*
 * void
 * toxic_range_free(uintptr_t addr, size_t size)
 *	Unmark "toxic mapping".
 *
 *	The caller must specify whole address range that was passed to
 *	toxic_range_alloc().
 */
void
toxic_range_free(uintptr_t addr, size_t size)
{
	toxic_range_t	**trpp, *freep = NULL;

	size = P2ROUNDUP(size, PAGESIZE);

	mutex_enter(&toxic_lock);
	for (trpp = &toxic_head; *trpp != NULL; trpp = &((*trpp)->tr_next)) {
		toxic_range_t	*trp = *trpp;

		if (trp->tr_addr == addr && trp->tr_size == size) {
			*trpp = trp->tr_next;
			freep = trp;
			toxic_changed = 1;
			break;
		}
		else {
			ASSERT(addr + size <= trp->tr_addr ||
			       trp->tr_addr + trp->tr_size <= addr);
		}
	}
	mutex_exit(&toxic_lock);

	if (freep) {
		kmem_free(freep, sizeof(toxic_range_t));
	}
}

/*
 * void
 * toxic_range_iterate(void (*func)(uintptr_t base, uintptr_t limit))
 *	Call the specified function for each toxic mappings registered by
 *	toxic_range_alloc().
 *
 *	If toxic_range_alloc() or toxic_range_free() is called on another
 *	context at the same time, some toxic mappings may not be passed to
 *	the function.
 */
void
toxic_range_iterate(void (*func)(uintptr_t base, uintptr_t limit))
{
	toxic_range_t	*trp;
	uintptr_t	startaddr = (uintptr_t)kernelheap;

	mutex_enter(&toxic_lock);

 tryagain:
	toxic_changed = 0;
	for (trp = toxic_head; trp != NULL; trp = trp->tr_next) {
		uintptr_t	start, end;

		if (trp->tr_addr < startaddr) {
			continue;
		}

		start = trp->tr_addr;
		end = start + trp->tr_size;
		startaddr = end;

		mutex_exit(&toxic_lock);
		(*func)(start, end);

		mutex_enter(&toxic_lock);
		if (toxic_changed) {
			goto tryagain;
		}
	}
	mutex_exit(&toxic_lock);
}
