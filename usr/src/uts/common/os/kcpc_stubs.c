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

#pragma ident   "@(#)kcpc_stubs.c"

/*
 * Stub routines for the CPC subsystem.
 */

#include <sys/thread.h>
#include <sys/cpuvar.h>
#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/mutex.h>
#include <sys/kcpc.h>
#include <sys/cpc_impl.h>
#include <sys/cpc_pcbe.h>
#include <sys/sunddi.h>

/* the cpc modules refer the following variables. */
kmutex_t        kcpc_ctx_llock[CPC_HASH_BUCKETS];
krwlock_t       kcpc_cpuctx_lock;
int             kcpc_cpuctx;
uint_t		cpc_ncounters = 0;
pcbe_ops_t	*pcbe_ops = NULL;

void
kcpc_register_pcbe(pcbe_ops_t *ops)
{
}

int
kcpc_bind_cpu(kcpc_set_t *set, processorid_t cpuid, int *subcode)
{
	return (1);
}

int
kcpc_bind_thread(kcpc_set_t *set, kthread_t *t, int *subcode)
{
	return (1);
}

int
kcpc_sample(kcpc_set_t *set, uint64_t *buf, hrtime_t *hrtime, uint64_t *tick)
{
	return (1);
}

int
kcpc_unbind(kcpc_set_t *set)
{
	return (1);
}

int
kcpc_preset(kcpc_set_t *set, int index, uint64_t preset)
{
	return (1);
}

int
kcpc_restart(kcpc_set_t *set)
{
	return (1);
}

int
kcpc_enable(kthread_t *t, int cmd, int enable)
{
	return (1);
}

void *
kcpc_next_config(void *token, void *current, uint64_t **data)
{
	ASSERT(0);	/* should not be called */
	return (NULL);
}

kcpc_ctx_t *
kcpc_overflow_intr(caddr_t arg, uint64_t bitmap)
{
	ASSERT(0);	/* should not be called */
	return (NULL);
}

uint_t
kcpc_hw_overflow_intr(caddr_t arg1, caddr_t arg2)
{
	return (DDI_INTR_UNCLAIMED);
}

int
kcpc_overflow_ast(void)
{
	return (0);
}

void
kcpc_idle_save(struct cpu *cp)
{
	ASSERT(0);	/* should not be called */
}

void
kcpc_idle_restore(struct cpu *cp)
{
	ASSERT(0);	/* should not be called */
}

void
kcpc_free_set(kcpc_set_t *set)
{
}

void
kcpc_invalidate_all(void)
{
}

void
kcpc_passivate(void)
{
}

int
kcpc_allow_nonpriv(void *token)
{
	return (0);
}

void
kcpc_invalidate(kthread_t *t)
{
}

int
kcpc_pcbe_tryload(const char *prefix, uint_t first, uint_t second, uint_t third)
{
	ASSERT(0);	/* should not be called */
	return (-1);
}
