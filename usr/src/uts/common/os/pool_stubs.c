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
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Copyright (c) 2008 NEC Corporation
 */

#pragma ident	"@(#)pool_stubs.c"

#include <sys/pool.h>
#include <sys/pool_impl.h>
#include <sys/pool_pset.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/thread.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/procset.h>
#include <sys/zone.h>
#include <sys/debug.h>
	
pool_t	*pool_default;			/* default pool which always exists */
int	pool_state = POOL_DISABLED;	/* pools state -- enabled/disabled */
static pool_t	pool0;
static kmutex_t		pool_mutex;		/* protects pool_busy_* */
static kcondvar_t	pool_busy_cv;		/* waiting for "pool_lock" */
static kthread_t	*pool_busy_thread;	/* thread holding "pool_lock" */

hrtime_t pool_pset_mod;		/* last modification time for psets */
hrtime_t pool_cpu_mod;		/* last modification time for CPUs */

/*
 * Boot-time pool initialization.
 */
void
pool_init(void)
{
	pool_default = &pool0;
	pool_default->pool_id = POOL_DEFAULT;

	p0.p_pool = pool_default;
	global_zone->zone_pool = pool_default;
	pool_default->pool_ref = 1;
}

/*
 * Synchronization routines.
 *
 * pool_lock is only called from syscall-level routines (processor_bind(),
 * pset_*(), and /dev/pool ioctls).  The pool "lock" may be held for long
 * periods of time, including across sleeping operations, so we allow its
 * acquisition to be interruptible.
 *
 * The current thread that owns the "lock" is stored in the variable
 * pool_busy_thread, both to let pool_lock_held() work and to aid debugging.
 */
void
pool_lock(void)
{
	mutex_enter(&pool_mutex);
	ASSERT(!pool_lock_held());
	while (pool_busy_thread != NULL)
		cv_wait(&pool_busy_cv, &pool_mutex);
	pool_busy_thread = curthread;
	mutex_exit(&pool_mutex);
}

int
pool_lock_intr(void)
{
	mutex_enter(&pool_mutex);
	ASSERT(!pool_lock_held());
	while (pool_busy_thread != NULL) {
		if (cv_wait_sig(&pool_busy_cv, &pool_mutex) == 0) {
			cv_signal(&pool_busy_cv);
			mutex_exit(&pool_mutex);
			return (1);
		}
	}
	pool_busy_thread = curthread;
	mutex_exit(&pool_mutex);
	return (0);
}

int
pool_lock_held(void)
{
	return (pool_busy_thread == curthread);
}

void
pool_unlock(void)
{
	mutex_enter(&pool_mutex);
	ASSERT(pool_lock_held());
	pool_busy_thread = NULL;
	cv_signal(&pool_busy_cv);
	mutex_exit(&pool_mutex);
}

/*
 * Routines allowing fork(), exec(), exit(), and lwp_create() to synchronize
 * with pool_do_bind().
 */
void
pool_barrier_enter(void)
{
	/* nothing to do */
}

void
pool_barrier_exit(void)
{
	/* nothing to do */
}

/*
 * Return the scheduling class for this pool, or
 * 	POOL_CLASS_UNSET if not set
 * 	POOL_CLASS_INVAL if set to an invalid class ID.
 */
id_t
pool_get_class(pool_t *pool)
{
	return (POOL_CLASS_UNSET);
}

/*
 * The meat of the bind operation.
 */
int
pool_do_bind(pool_t *pool, idtype_t idtype, id_t id, int flags)
{
	/* This function is called only if pool_state is POOL_ENABLED. */
	ASSERT(0);
	return (EINVAL);
}

/*
 * Make the processor set visible to the zone.  A NULL value for
 * the zone means that the special ALL_ZONES token should be added to
 * the visibility list.
 */
void
pool_pset_visibility_add(psetid_t psetid, zone_t *zone)
{
	/* This function is called only if pool_state is POOL_ENABLED. */
	ASSERT(0);
}

/*
 * Remove zone's visibility into the processor set.  A NULL value for
 * the zone means that the special ALL_ZONES token should be removed
 * from the visibility list.
 */
void
pool_pset_visibility_remove(psetid_t psetid, zone_t *zone)
{
	/* This function is called only if pool_state is POOL_ENABLED. */
	ASSERT(0);
}

/*
 * Quick way of seeing if pools are enabled (as far as processor sets are
 * concerned) without holding pool_lock().
 */
boolean_t
pool_pset_enabled(void)
{
	return (B_FALSE);
}
