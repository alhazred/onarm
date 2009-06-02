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
 * Copyright (c) 2006-2008 NEC Corporation
 */

#ident	"@(#)armpf/os/memscrub.c"

/*
 * Memory Scrubbing is not supported on ARM platform.
 * This file contains only stub routines.
 */

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <vm/page.h>

void
memscrub_init(void)
{
}

void
memscrub_disable(void)
{
}

#ifdef MEMSCRUB_DEBUG
/* ARGSUSED */
void
memscrub_printmemlist(char *title, struct memlist *listp)
{
	cmn_err(CE_CONT, "%s:\n", title);
}
#endif /* MEMSCRUB_DEBUG */

/* ARGSUSED */
void
memscrub_wakeup(void *c)
{
}

/*
 * void
 * pagescrub(page_t *pp, uint_t off, uint_t len)
 *	Platform-dependent page scrub call.
 *
 *	Only thing to do is to call pagezero() because memory scrubbing
 *	is not supported on ARM.
 */
void
pagescrub(page_t *pp, uint_t off, uint_t len)
{
	pagezero(pp, off, len);
}
