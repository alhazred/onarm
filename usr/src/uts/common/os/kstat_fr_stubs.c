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
 * Copyright (c) 2008 NEC Corporation
 */

#pragma ident	"@(#)kstat_fr_stubs.c"

/*
 * Stub routines for Kernel statistics framework
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/kstat.h>
#include <sys/debug.h>

#if 0
int
kstat_zone_find(kstat_t *k, zoneid_t zoneid)
{
	return 0;
}
#endif

void
kstat_zone_remove(kstat_t *k, zoneid_t zoneid)
{
}

void
kstat_zone_add(kstat_t *k, zoneid_t zoneid)
{
}

void
kstat_rele(kstat_t *ksp)
{
}

#if 0
kstat_t *
kstat_hold_bykid(kid_t kid, zoneid_t zoneid)
{
	return NULL;
}
#endif

kstat_t *
kstat_hold_byname(const char *ks_module, int ks_instance, const char *ks_name,
    zoneid_t ks_zoneid)
{
	return NULL;
}

void
kstat_init(void)
{
}

/*
 * kstat_named_setstr(9F)
 */
void
kstat_named_setstr(kstat_named_t *knp, const char *src)
{
	if (knp->data_type != KSTAT_DATA_STRING)
		panic("kstat_named_setstr('%p', '%p'): "
		    "named kstat is not of type KSTAT_DATA_STRING", knp, src);

	KSTAT_NAMED_STR_PTR(knp) = (char *)src;
	if (src != NULL)
		KSTAT_NAMED_STR_BUFLEN(knp) = strlen(src) + 1;
	else
		KSTAT_NAMED_STR_BUFLEN(knp) = 0;
}

void
kstat_set_string(char *dst, const char *src)
{
	bzero(dst, KSTAT_STRLEN);
	(void) strncpy(dst, src, KSTAT_STRLEN - 1);
}

/*
 * kstat_named_init(9F)
 */
void
kstat_named_init(kstat_named_t *knp, const char *name, uchar_t data_type)
{
	kstat_set_string(knp->name, name);
	knp->data_type = data_type;

	if (data_type == KSTAT_DATA_STRING)
		kstat_named_setstr(knp, NULL);
}

/*
 * kstat_create(9F)
 */
kstat_t *
kstat_create(const char *ks_module, int ks_instance, const char *ks_name,
    const char *ks_class, uchar_t ks_type, uint_t ks_ndata, uchar_t ks_flags)
{
	return NULL;
}

kstat_t *
kstat_create_zone(const char *ks_module, int ks_instance, const char *ks_name,
    const char *ks_class, uchar_t ks_type, uint_t ks_ndata, uchar_t ks_flags,
    zoneid_t ks_zoneid)
{
	return NULL;
}

/*
 * kstat_install(9F)
 */
void
kstat_install(kstat_t *ksp)
{
	ASSERT(0);	/* should not be called */
}

/*
 * kstat_delete(9F)
 */
void
kstat_delete(kstat_t *ksp)
{
	ASSERT(ksp == NULL);
}

void
kstat_delete_byname_zone(const char *ks_module, int ks_instance,
    const char *ks_name, zoneid_t ks_zoneid)
{
}

void
kstat_delete_byname(const char *ks_module, int ks_instance, const char *ks_name)
{
}

#if !defined(__sparc) || defined(lint) || defined(__lint)

/*
 * kstat_waitq_enter(9F)
 */
void
kstat_waitq_enter(kstat_io_t *kiop)
{
	ASSERT(0);	/* should not be called */
}

void
kstat_waitq_exit(kstat_io_t *kiop)
{
	ASSERT(0);	/* should not be called */
}

void
kstat_runq_enter(kstat_io_t *kiop)
{
	ASSERT(0);	/* should not be called */
}

void
kstat_runq_exit(kstat_io_t *kiop)
{
	ASSERT(0);	/* should not be called */
}

void
kstat_waitq_to_runq(kstat_io_t *kiop)
{
	ASSERT(0);	/* should not be called */
}

void
kstat_runq_back_to_waitq(kstat_io_t *kiop)
{
	ASSERT(0);	/* should not be called */
}

#endif
