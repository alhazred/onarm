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
 * Copyright (c) 2007 NEC Corporation
 */

#pragma ident	"@(#)errorq_stub.c"

#include <sys/errorq_impl.h>
#include <sys/sysmacros.h>
#include <sys/machlock.h>
#include <sys/cmn_err.h>
#include <sys/atomic.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/bootconf.h>
#include <sys/spl.h>
#include <sys/dumphdr.h>
#include <sys/compress.h>
#include <sys/time.h>
#include <sys/panic.h>
#include <sys/fm/protocol.h>
#include <sys/fm/util.h>

errorq_t *
errorq_create(const char *name, errorq_func_t func, void *private,
    ulong_t qlen, size_t size, uint_t ipl, uint_t flags)
{
	return NULL;
}

errorq_t *
errorq_nvcreate(const char *name, errorq_func_t func, void *private,
    ulong_t qlen, size_t size, uint_t ipl, uint_t flags)
{
	return NULL;
}

void
errorq_destroy(errorq_t *eqp)
{
}

void
errorq_dispatch(errorq_t *eqp, const void *data, size_t len, uint_t flag)
{
}

void
errorq_drain(errorq_t *eqp)
{
}

void
errorq_init(void)
{
}

void
errorq_panic(void)
{
}

errorq_elem_t *
errorq_reserve(errorq_t *eqp)
{
	return NULL;
}

void
errorq_commit(errorq_t *eqp, errorq_elem_t *eqep, uint_t flag)
{
}

void
errorq_cancel(errorq_t *eqp, errorq_elem_t *eqep)
{
}

void
errorq_dump(void)
{
}

nvlist_t *
errorq_elem_nvl(errorq_t *eqp, const errorq_elem_t *eqep)
{
	return NULL;
}

nv_alloc_t *
errorq_elem_nva(errorq_t *eqp, const errorq_elem_t *eqep)
{
	return NULL;
}

void *
errorq_elem_dup(errorq_t *eqp, const errorq_elem_t *eqep, errorq_elem_t **neqep)
{
	return NULL;
}
