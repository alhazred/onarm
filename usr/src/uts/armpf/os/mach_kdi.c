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
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2006-2008 NEC Corporation
 */

#ident	"@(#)armpf/os/mach_kdi.c"

#include <sys/systm.h>
#include <sys/kdi_impl.h>
#include <sys/cpuvar.h>
#include <sys/cachectl.h>

/* Stub entries */

static void
kdi_system_claim(void)
{
	/* nothing to do */
}

static void
kdi_system_release(void)
{
	/* nothing to do */
}

void
kdi_flush_caches(void)
{
	PCACHE_FLUSH_ALL();
}

void
mach_kdi_init(kdi_t *kdi)
{
#ifdef	notyet
#endif	/* notyet */
}

/*ARGSUSED*/
void
mach_kdi_fini(kdi_t *kdi)
{
#ifdef	notyet
	hat_kdi_fini();
#endif	/* notyet */
}

void
plat_kdi_init(kdi_t *kdi)
{
#ifdef	notyet
	kdi->pkdi_system_claim = kdi_system_claim;
	kdi->pkdi_system_release = kdi_system_release;
#endif	/* notyet */
}
