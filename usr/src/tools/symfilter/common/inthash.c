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
 * Copyright (c) 2008-2009 NEC Corporation
 * All rights reserved.
 */

#ident	"@(#)tools/symfilter/common/inthash.c"

/*
 * Simple integer hash table implementation.
 * inthash keeps integer key only, not key and value pair.
 */

#include <sys/types.h>
#include <string.h>
#include "symfilter.h"

/* Default hash table size */
#define	INTHASH_DEFAULT_SIZE	1024

/* Hash entry */
struct ihashent {
	uint_t		ihe_value;	/* integer value */
	ihashent_t	*ihe_next;	/* next link */
};

/*
 * void
 * inthash_init(inthash_t *hash, size_t size)
 *	Initialize inthash instance.
 *	If zero is passed as size, INTHASH_DEFAULT_SIZE is used as
 *	hash table size.
 */
void
inthash_init(inthash_t *hash, size_t size)
{
	size_t	tsize;
	if (size == 0) {
		size = INTHASH_DEFAULT_SIZE;
	}

	tsize = sizeof(ihashent_t *) * size;
	hash->ih_table = (ihashent_t **)xmalloc(tsize);
	(void)memset(hash->ih_table, 0, tsize);
	hash->ih_size = size;
}

/*
 * boolean_t
 * inthash_put(inthash_t *hash, uint_t value)
 *	Put an integer value into hash table.
 *
 * Calling/Exit State:
 *	B_TRUE is returned if the specified value is put into the table.
 *	B_FALSE is returned if the specified value already exists in the table.
 */
boolean_t
inthash_put(inthash_t *hash, uint_t value)
{
	uint_t		index;
	ihashent_t	**ihpp, *ihp;

	index = value % hash->ih_size;

	for (ihpp = hash->ih_table + index; *ihpp != NULL;
	     ihpp = &((*ihpp)->ihe_next)) {
		ihp = *ihpp;
		if (ihp->ihe_value == value) {
			return B_FALSE;
		}
	}

	/* Allocate a new hash entry. */
	ihp = (ihashent_t *)xmalloc(sizeof(*ihp));
	ihp->ihe_value = value;
	*ihpp = ihp;
	ihp->ihe_next = NULL;

	return B_TRUE;
}

/*
 * boolean_t
 * inthash_exists(inthash_t *hash, uint_t value) {
 *	Determine whether the specified value exists in the hash table.
 *
 * Calling/Exit State:
 *	B_TRUE is returned if the value exists. Otherwise B_FALSE is returned.
 */
boolean_t
inthash_exists(inthash_t *hash, uint_t value)
{
	uint_t		index;
	ihashent_t	*ihp;

	index = value % hash->ih_size;

	for (ihp = *(hash->ih_table + index); ihp != NULL;
	     ihp = ihp->ihe_next) {
		if (ihp->ihe_value == value) {
			return B_TRUE;
		}
	}

	return B_FALSE;
}
