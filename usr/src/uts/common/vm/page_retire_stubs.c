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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Copyright (c) 2008 NEC Corporation
 */

#pragma ident	"@(#)page_retire_stubs.c"

/*
 * Stub routines for Page Retire.
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <vm/page.h>

/*
 * Note that multiple bits may be set in a single settoxic operation.
 * May be called without the page locked.
 */
void
page_settoxic(page_t *pp, uchar_t bits)
{
	ASSERT(0);	/* should not be called */
}

/*
 * Note that multiple bits may cleared in a single clrtoxic operation.
 * Must be called with the page exclusively locked to prevent races which
 * may attempt to retire a page without any toxic bits set.
 */
void
page_clrtoxic(page_t *pp, uchar_t bits)
{
	ASSERT(0);	/* should not be called */
}

/*
 * Initialize the page retire mechanism:
 */
void
page_retire_init(void)
{
}

/*
 * page_retire() - the front door in to retire a page.
 */
int
page_retire(uint64_t pa, uchar_t reason)
{
	return EINVAL;	/* PA is not a relocatable page */
}

/*
 * Return a page to service by moving it from the retired_pages vnode
 * onto the freelist.
 */
int
page_unretire(uint64_t pa)
{
	return EIO;	/* Page is not retired */
}

/*
 * Test to see if the page_t for a given PA is retired, and return the
 * hardware errors we have seen on the page if requested.
 */
int
page_retire_check(uint64_t pa, uint64_t *errors)
{
	if (errors) {
		*errors = 0;
	}
	return EINVAL;	/* PA is not a relocatable page */
}

/*
 * Page retire self-test. For now, it always returns 0.
 */
int
page_retire_test(void)
{
	return (0);
}
