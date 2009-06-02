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
 * Copyright (c) 2007 NEC Corporation
 */

/*
 * XXX This stuff should be in usr/src/common, to be shared by boot
 * code, kernel DR, and busra stuff.
 *
 * NOTE: We are only using the next-> link. The prev-> link is
 *	not used in the implementation.
 */
#include <sys/types.h>
#include <sys/memlist.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/pci_impl_arm.h>
#include <sys/debug.h>

extern int pci_boot_debug;
#define	dprintf if (pci_boot_debug) printf

void
memlist_dump(struct pci_memlist *listp)
{
	dprintf("memlist 0x%p content", (void *)listp);
	while (listp) {
		dprintf("(0x%x, 0x%x)",
		    (int)listp->address,
		    (int)listp->size);
		listp = listp->next;
	}
}

struct pci_memlist *
memlist_alloc()
{
	return ((struct pci_memlist *)kmem_zalloc(sizeof (struct pci_memlist),
	    KM_SLEEP));
}

void
memlist_free(struct pci_memlist *buf)
{
	kmem_free(buf, sizeof (struct pci_memlist));
}

void
memlist_free_all(struct pci_memlist **list)
{
	struct pci_memlist  *next, *buf;

	next = *list;
	while (next) {
		buf = next;
		next = buf->next;
		kmem_free(buf, sizeof (struct pci_memlist));
	}
	*list = 0;
}

/* insert in the order of addresses */
void
memlist_insert(struct pci_memlist **listp, uint32_t addr, uint32_t size)
{
	int merge_left, merge_right;
	struct pci_memlist *entry;
	struct pci_memlist *prev = 0, *next;

	/* find the location in list */
	next = *listp;
	while (next && next->address < addr) {
		prev = next;
		next = prev->next;
	}

	merge_left = (prev && addr == prev->address + prev->size);
	merge_right = (next && addr + size == next->address);
	if (merge_left && merge_right) {
		prev->size += size + next->size;
		prev->next = next->next;
		memlist_free(next);
		return;
	}

	if (merge_left) {
		prev->size += size;
		return;
	}

	if (merge_right) {
		next->address = addr;
		next->size += size;
		return;
	}

	entry = memlist_alloc();
	if (!entry) {
		cmn_err(CE_WARN, "pci: cannot allocate pci memlist.\n");
	}
	entry->address = addr;
	entry->size = size;
	if (prev == 0) {
		entry->next = *listp;
		*listp = entry;
	} else {
		entry->next = next;
		prev->next = entry;
	}
}

/*
 * Delete memory chunks, assuming list sorted by address
 */
int
memlist_remove(struct pci_memlist **listp, uint32_t addr, uint32_t size)
{
	struct pci_memlist *entry;
	struct pci_memlist *prev = 0, *next;

	/* a remove can't be done on an empty list */
	ASSERT(*listp);

	/* find the location in list */
	next = *listp;
	while (next && (next->address + next->size < addr)) {
		prev = next;
		next = prev->next;
	}

	if (next == 0 || (addr < next->address)) {
		dprintf("memlist_remove: addr 0x%x, size 0x%x"
		    " not contained in list\n",
		    (int)addr, (int)size);
		memlist_dump(*listp);
		return (-1);
	}

	if (addr > next->address) {
		uint32_t oldsize = next->size;
		next->size = addr - next->address;
		if ((next->address + oldsize) > (addr + size)) {
			entry = memlist_alloc();
			entry->address = addr + size;
			entry->size = next->address + oldsize - addr - size;
			entry->next = next->next;
			next->next = entry;
		}
	} else if ((next->address + next->size) > (addr + size)) {
		/* addr == next->address */
		next->address = addr + size;
		next->size -= size;
	} else {
		/* the entire chunk is deleted */
		if (prev == 0) {
			*listp = next->next;
		} else {
			prev->next = next->next;
		}
		memlist_free(next);
	}
	return (0);
}

/*
 * find and claim a memory chunk of given size, first fit
 */
uint32_t
memlist_find(struct pci_memlist **listp, uint32_t size, int align)
{
	uint32_t delta, total_size;
	uint32_t paddr;
	struct pci_memlist *prev = 0, *next;

	/* find the chunk with sufficient size */
	next = *listp;
	while (next) {
		delta = next->address & ((align != 0) ? (align - 1) : 0);
		if (delta != 0)
			total_size = size + align - delta;
		else
			total_size = size; /* the addr is already aligned */
		if (next->size >= total_size)
			break;
		prev = next;
		next = prev->next;
	}

	if (next == 0)
		return (0);	/* Not found */

	paddr = next->address;
	if (delta)
		paddr += align - delta;
	(void) memlist_remove(listp, paddr, size);
	return (paddr);
}

/*
 * find and claim a memory chunk of given size, starting
 * at a specified address
 */
uint32_t
memlist_find_with_startaddr(struct pci_memlist **listp, uint32_t address,
    uint32_t size, int align)
{
	uint32_t delta, total_size;
	uint32_t paddr;
	struct pci_memlist *next;

	/* find the chunk starting at 'address' */
	next = *listp;
	while (next && (next->address != address)) {
		next = next->next;
	}
	if (next == 0)
		return (0);	/* Not found */

	delta = next->address & ((align != 0) ? (align - 1) : 0);
	if (delta != 0)
		total_size = size + align - delta;
	else
		total_size = size;	/* the addr is already aligned */
	if (next->size < total_size)
		return (0);	/* unsufficient size */

	paddr = next->address;
	if (delta)
		paddr += align - delta;
	(void) memlist_remove(listp, paddr, size);

	return (paddr);
}

/*
 * Merge memlist src into memlist dest
 */
void
memlist_merge(struct pci_memlist **src, struct pci_memlist **dest)
{
	struct pci_memlist *head, *prev;

	head = *src;
	while (head) {
		memlist_insert(dest, head->address, head->size);
		prev = head;
		head = head->next;
		memlist_free(prev);
	}
	*src = 0;
}

/*
 * Make a copy of memlist
 */
struct pci_memlist *
memlist_dup(struct pci_memlist *listp)
{
	struct pci_memlist *head = 0, *prev = 0;

	while (listp) {
		struct pci_memlist *entry = memlist_alloc();
		entry->address = listp->address;
		entry->size = listp->size;
		entry->next = 0;
		if (prev)
			prev->next = entry;
		else
			head = entry;
		prev = entry;
		listp = listp->next;
	}

	return (head);
}

int
memlist_count(struct pci_memlist *listp)
{
	int count = 0;
	while (listp) {
		count++;
		listp = listp->next;
	}

	return (count);
}
