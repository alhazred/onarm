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

#pragma ident	"@(#)rwlock.c	1.24	07/06/27 SMI"

#include "lint.h"
#include "thr_uberdata.h"
#include <sys/sdt.h>

#define	TRY_FLAG		0x10
#define	READ_LOCK		0
#define	WRITE_LOCK		1
#define	READ_LOCK_TRY		(READ_LOCK | TRY_FLAG)
#define	WRITE_LOCK_TRY		(WRITE_LOCK | TRY_FLAG)

#define	NLOCKS	4	/* initial number of readlock_t structs allocated */

#define	ASSERT_CONSISTENT_STATE(readers)		\
	ASSERT(!((readers) & URW_WRITE_LOCKED) ||	\
		((readers) & ~URW_HAS_WAITERS) == URW_WRITE_LOCKED)

/*
 * Find/allocate an entry for rwlp in our array of rwlocks held for reading.
 * We must be deferring signals for this to be safe.
 * Else if we are returning an entry with ul_rdlockcnt == 0,
 * it could be reassigned behind our back in a signal handler.
 */
static readlock_t *
rwl_entry(rwlock_t *rwlp)
{
	ulwp_t *self = curthread;
	readlock_t *remembered = NULL;
	readlock_t *readlockp;
	uint_t nlocks;

	/* we must be deferring signals */
	ASSERT((self->ul_critical + self->ul_sigdefer) != 0);

	if ((nlocks = self->ul_rdlockcnt) != 0)
		readlockp = self->ul_readlock.array;
	else {
		nlocks = 1;
		readlockp = &self->ul_readlock.single;
	}

	for (; nlocks; nlocks--, readlockp++) {
		if (readlockp->rd_rwlock == rwlp)
			return (readlockp);
		if (readlockp->rd_count == 0 && remembered == NULL)
			remembered = readlockp;
	}
	if (remembered != NULL) {
		remembered->rd_rwlock = rwlp;
		return (remembered);
	}

	/*
	 * No entry available.  Allocate more space, converting the single
	 * readlock_t entry into an array of readlock_t entries if necessary.
	 */
	if ((nlocks = self->ul_rdlockcnt) == 0) {
		/*
		 * Initial allocation of the readlock_t array.
		 * Convert the single entry into an array.
		 */
		self->ul_rdlockcnt = nlocks = NLOCKS;
		readlockp = lmalloc(nlocks * sizeof (readlock_t));
		/*
		 * The single readlock_t becomes the first entry in the array.
		 */
		*readlockp = self->ul_readlock.single;
		self->ul_readlock.single.rd_count = 0;
		self->ul_readlock.array = readlockp;
		/*
		 * Return the next available entry in the array.
		 */
		(++readlockp)->rd_rwlock = rwlp;
		return (readlockp);
	}
	/*
	 * Reallocate the array, double the size each time.
	 */
	readlockp = lmalloc(nlocks * 2 * sizeof (readlock_t));
	(void) _memcpy(readlockp, self->ul_readlock.array,
		nlocks * sizeof (readlock_t));
	lfree(self->ul_readlock.array, nlocks * sizeof (readlock_t));
	self->ul_readlock.array = readlockp;
	self->ul_rdlockcnt *= 2;
	/*
	 * Return the next available entry in the newly allocated array.
	 */
	(readlockp += nlocks)->rd_rwlock = rwlp;
	return (readlockp);
}

/*
 * Free the array of rwlocks held for reading.
 */
void
rwl_free(ulwp_t *ulwp)
{
	uint_t nlocks;

	if ((nlocks = ulwp->ul_rdlockcnt) != 0)
		lfree(ulwp->ul_readlock.array, nlocks * sizeof (readlock_t));
	ulwp->ul_rdlockcnt = 0;
	ulwp->ul_readlock.single.rd_rwlock = NULL;
	ulwp->ul_readlock.single.rd_count = 0;
}

/*
 * Check if a reader version of the lock is held by the current thread.
 * rw_read_is_held() is private to libc.
 */
#pragma weak rw_read_is_held = _rw_read_held
#pragma weak rw_read_held = _rw_read_held
int
_rw_read_held(rwlock_t *rwlp)
{
	volatile uint32_t *rwstate = (volatile uint32_t *)&rwlp->rwlock_readers;
	uint32_t readers;
	ulwp_t *self = curthread;
	readlock_t *readlockp;
	uint_t nlocks;
	int rval = 0;

	no_preempt(self);

	readers = *rwstate;
	ASSERT_CONSISTENT_STATE(readers);
	if (!(readers & URW_WRITE_LOCKED) &&
	    (readers & URW_READERS_MASK) != 0) {
		/*
		 * The lock is held for reading by some thread.
		 * Search our array of rwlocks held for reading for a match.
		 */
		if ((nlocks = self->ul_rdlockcnt) != 0)
			readlockp = self->ul_readlock.array;
		else {
			nlocks = 1;
			readlockp = &self->ul_readlock.single;
		}
		for (; nlocks; nlocks--, readlockp++) {
			if (readlockp->rd_rwlock == rwlp) {
				if (readlockp->rd_count)
					rval = 1;
				break;
			}
		}
	}

	preempt(self);
	return (rval);
}

/*
 * Check if a writer version of the lock is held by the current thread.
 * rw_write_is_held() is private to libc.
 */
#pragma weak rw_write_is_held = _rw_write_held
#pragma weak rw_write_held = _rw_write_held
int
_rw_write_held(rwlock_t *rwlp)
{
	volatile uint32_t *rwstate = (volatile uint32_t *)&rwlp->rwlock_readers;
	uint32_t readers;
	ulwp_t *self = curthread;
	int rval;

	no_preempt(self);

	readers = *rwstate;
	ASSERT_CONSISTENT_STATE(readers);
	rval = ((readers & URW_WRITE_LOCKED) &&
	    rwlp->rwlock_owner == (uintptr_t)self &&
	    (rwlp->rwlock_type == USYNC_THREAD ||
	    rwlp->rwlock_ownerpid == self->ul_uberdata->pid));

	preempt(self);
	return (rval);
}

#pragma weak rwlock_init = __rwlock_init
#pragma weak _rwlock_init = __rwlock_init
/* ARGSUSED2 */
int
__rwlock_init(rwlock_t *rwlp, int type, void *arg)
{
	if (type != USYNC_THREAD && type != USYNC_PROCESS)
		return (EINVAL);
	/*
	 * Once reinitialized, we can no longer be holding a read or write lock.
	 * We can do nothing about other threads that are holding read locks.
	 */
	sigoff(curthread);
	rwl_entry(rwlp)->rd_count = 0;
	sigon(curthread);
	(void) _memset(rwlp, 0, sizeof (*rwlp));
	rwlp->rwlock_type = (uint16_t)type;
	rwlp->rwlock_magic = RWL_MAGIC;
	rwlp->mutex.mutex_type = (uint8_t)type;
	rwlp->mutex.mutex_flag = LOCK_INITED;
	rwlp->mutex.mutex_magic = MUTEX_MAGIC;
	return (0);
}

#pragma weak rwlock_destroy = __rwlock_destroy
#pragma weak _rwlock_destroy = __rwlock_destroy
#pragma weak pthread_rwlock_destroy = __rwlock_destroy
#pragma weak _pthread_rwlock_destroy = __rwlock_destroy
int
__rwlock_destroy(rwlock_t *rwlp)
{
	/*
	 * Once destroyed, we can no longer be holding a read or write lock.
	 * We can do nothing about other threads that are holding read locks.
	 */
	sigoff(curthread);
	rwl_entry(rwlp)->rd_count = 0;
	sigon(curthread);
	rwlp->rwlock_magic = 0;
	tdb_sync_obj_deregister(rwlp);
	return (0);
}

/*
 * Attempt to acquire a readers lock.  Return true on success.
 */
static int
read_lock_try(rwlock_t *rwlp, int ignore_waiters_flag)
{
	volatile uint32_t *rwstate = (volatile uint32_t *)&rwlp->rwlock_readers;
	uint32_t mask = ignore_waiters_flag?
		URW_WRITE_LOCKED : (URW_HAS_WAITERS | URW_WRITE_LOCKED);
	uint32_t readers;
	ulwp_t *self = curthread;

	no_preempt(self);
	while (((readers = *rwstate) & mask) == 0) {
		if (atomic_cas_32(rwstate, readers, readers + 1) == readers) {
			preempt(self);
			return (1);
		}
	}
	preempt(self);
	return (0);
}

/*
 * Attempt to release a reader lock.  Return true on success.
 */
static int
read_unlock_try(rwlock_t *rwlp)
{
	volatile uint32_t *rwstate = (volatile uint32_t *)&rwlp->rwlock_readers;
	uint32_t readers;
	ulwp_t *self = curthread;

	no_preempt(self);
	while (((readers = *rwstate) & URW_HAS_WAITERS) == 0) {
		if (atomic_cas_32(rwstate, readers, readers - 1) == readers) {
			preempt(self);
			return (1);
		}
	}
	preempt(self);
	return (0);
}

/*
 * Attempt to acquire a writer lock.  Return true on success.
 */
static int
write_lock_try(rwlock_t *rwlp, int ignore_waiters_flag)
{
	volatile uint32_t *rwstate = (volatile uint32_t *)&rwlp->rwlock_readers;
	uint32_t mask = ignore_waiters_flag?
		(URW_WRITE_LOCKED | URW_READERS_MASK) :
		(URW_HAS_WAITERS | URW_WRITE_LOCKED | URW_READERS_MASK);
	ulwp_t *self = curthread;
	uint32_t readers;

	no_preempt(self);
	while (((readers = *rwstate) & mask) == 0) {
		if (atomic_cas_32(rwstate, readers, readers | URW_WRITE_LOCKED)
		    == readers) {
			preempt(self);
			return (1);
		}
	}
	preempt(self);
	return (0);
}

/*
 * Attempt to release a writer lock.  Return true on success.
 */
static int
write_unlock_try(rwlock_t *rwlp)
{
	volatile uint32_t *rwstate = (volatile uint32_t *)&rwlp->rwlock_readers;
	uint32_t readers;
	ulwp_t *self = curthread;

	no_preempt(self);
	while (((readers = *rwstate) & URW_HAS_WAITERS) == 0) {
		if (atomic_cas_32(rwstate, readers, 0) == readers) {
			preempt(self);
			return (1);
		}
	}
	preempt(self);
	return (0);
}

/*
 * Wake up thread(s) sleeping on the rwlock queue and then
 * drop the queue lock.  Return non-zero if we wake up someone.
 * This is called when a thread releases a lock that appears to have waiters.
 */
static int
rw_queue_release(queue_head_t *qp, rwlock_t *rwlp)
{
	volatile uint32_t *rwstate = (volatile uint32_t *)&rwlp->rwlock_readers;
	uint32_t readers;
	uint32_t writers;
	int nlwpid = 0;
	int maxlwps = MAXLWPS;
	ulwp_t *self;
	ulwp_t **ulwpp;
	ulwp_t *ulwp;
	ulwp_t *prev = NULL;
	lwpid_t buffer[MAXLWPS];
	lwpid_t *lwpid = buffer;

	readers = *rwstate;
	ASSERT_CONSISTENT_STATE(readers);
	if (!(readers & URW_HAS_WAITERS)) {
		queue_unlock(qp);
		return (0);
	}
	readers &= URW_READERS_MASK;
	writers = 0;

	/*
	 * Walk the list of waiters and prepare to wake up as
	 * many readers as we encounter before encountering
	 * a writer.  If the first thread on the list is a
	 * writer, stop there and wake it up.
	 *
	 * We keep track of lwpids that are to be unparked in lwpid[].
	 * __lwp_unpark_all() is called to unpark all of them after
	 * they have been removed from the sleep queue and the sleep
	 * queue lock has been dropped.  If we run out of space in our
	 * on-stack buffer, we need to allocate more but we can't call
	 * lmalloc() because we are holding a queue lock when the overflow
	 * occurs and lmalloc() acquires a lock.  We can't use alloca()
	 * either because the application may have allocated a small
	 * stack and we don't want to overrun the stack.  So we call
	 * alloc_lwpids() to allocate a bigger buffer using the mmap()
	 * system call directly since that path acquires no locks.
	 */
	ulwpp = &qp->qh_head;
	while ((ulwp = *ulwpp) != NULL) {
		if (ulwp->ul_wchan != rwlp) {
			prev = ulwp;
			ulwpp = &ulwp->ul_link;
			continue;
		}
		if (ulwp->ul_writer) {
			if (writers != 0 || readers != 0)
				break;
			/* one writer to wake */
			writers++;
		} else {
			if (writers != 0)
				break;
			/* at least one reader to wake */
			readers++;
			if (nlwpid == maxlwps)
				lwpid = alloc_lwpids(lwpid, &nlwpid, &maxlwps);
		}
		(void) queue_unlink(qp, ulwpp, prev);
		lwpid[nlwpid++] = ulwp->ul_lwpid;
	}
	if (ulwp == NULL)
		atomic_and_32(rwstate, ~URW_HAS_WAITERS);
	if (nlwpid == 0) {
		queue_unlock(qp);
	} else {
		self = curthread;
		no_preempt(self);
		queue_unlock(qp);
		if (nlwpid == 1)
			(void) __lwp_unpark(lwpid[0]);
		else
			(void) __lwp_unpark_all(lwpid, nlwpid);
		preempt(self);
	}
	if (lwpid != buffer)
		(void) _private_munmap(lwpid, maxlwps * sizeof (lwpid_t));
	return (nlwpid != 0);
}

/*
 * Common code for rdlock, timedrdlock, wrlock, timedwrlock, tryrdlock,
 * and trywrlock for process-shared (USYNC_PROCESS) rwlocks.
 *
 * Note: if the lock appears to be contended we call __lwp_rwlock_rdlock()
 * or __lwp_rwlock_wrlock() holding the mutex. These return with the mutex
 * released, and if they need to sleep will release the mutex first. In the
 * event of a spurious wakeup, these will return EAGAIN (because it is much
 * easier for us to re-acquire the mutex here).
 */
int
shared_rwlock_lock(rwlock_t *rwlp, timespec_t *tsp, int rd_wr)
{
	volatile uint32_t *rwstate = (volatile uint32_t *)&rwlp->rwlock_readers;
	mutex_t *mp = &rwlp->mutex;
	/* LINTED set but not used */
	uint32_t readers;
	int try_flag;
	int error;

	try_flag = (rd_wr & TRY_FLAG);
	rd_wr &= ~TRY_FLAG;
	ASSERT(rd_wr == READ_LOCK || rd_wr == WRITE_LOCK);

	if (!try_flag) {
		DTRACE_PROBE2(plockstat, rw__block, rwlp, rd_wr);
	}

	do {
		if (try_flag && (*rwstate & URW_WRITE_LOCKED)) {
			error = EBUSY;
			break;
		}
		if ((error = _private_mutex_lock(mp)) != 0)
			break;
		if (rd_wr == READ_LOCK) {
			if (read_lock_try(rwlp, 0)) {
				(void) _private_mutex_unlock(mp);
				break;
			}
		} else {
			if (write_lock_try(rwlp, 0)) {
				(void) _private_mutex_unlock(mp);
				break;
			}
		}
		atomic_or_32(rwstate, URW_HAS_WAITERS);
		readers = *rwstate;
		ASSERT_CONSISTENT_STATE(readers);
		/*
		 * The calls to __lwp_rwlock_*() below will release the mutex,
		 * so we need a dtrace probe here.
		 */
		mp->mutex_owner = 0;
		DTRACE_PROBE2(plockstat, mutex__release, mp, 0);
		/*
		 * The waiters bit may be inaccurate.
		 * Only the kernel knows for sure.
		 */
		if (rd_wr == READ_LOCK) {
			if (try_flag)
				error = __lwp_rwlock_tryrdlock(rwlp);
			else
				error = __lwp_rwlock_rdlock(rwlp, tsp);
		} else {
			if (try_flag)
				error = __lwp_rwlock_trywrlock(rwlp);
			else
				error = __lwp_rwlock_wrlock(rwlp, tsp);
		}
	} while (error == EAGAIN || error == EINTR);

	if (!try_flag) {
		DTRACE_PROBE3(plockstat, rw__blocked, rwlp, rd_wr, error == 0);
	}

	return (error);
}

/*
 * Common code for rdlock, timedrdlock, wrlock, timedwrlock, tryrdlock,
 * and trywrlock for process-private (USYNC_THREAD) rwlocks.
 */
int
rwlock_lock(rwlock_t *rwlp, timespec_t *tsp, int rd_wr)
{
	volatile uint32_t *rwstate = (volatile uint32_t *)&rwlp->rwlock_readers;
	uint32_t readers;
	ulwp_t *self = curthread;
	queue_head_t *qp;
	ulwp_t *ulwp;
	int try_flag;
	int error = 0;

	try_flag = (rd_wr & TRY_FLAG);
	rd_wr &= ~TRY_FLAG;
	ASSERT(rd_wr == READ_LOCK || rd_wr == WRITE_LOCK);

	if (!try_flag) {
		DTRACE_PROBE2(plockstat, rw__block, rwlp, rd_wr);
	}

	qp = queue_lock(rwlp, MX);
retry:
	while (error == 0) {
		if (rd_wr == READ_LOCK) {
			if (read_lock_try(rwlp, 0))
				goto out;
		} else {
			if (write_lock_try(rwlp, 0))
				goto out;
		}
		atomic_or_32(rwstate, URW_HAS_WAITERS);
		readers = *rwstate;
		ASSERT_CONSISTENT_STATE(readers);
		if ((readers & URW_WRITE_LOCKED) ||
		    (rd_wr == WRITE_LOCK &&
		    (readers & URW_READERS_MASK) != 0))
			/* EMPTY */;	/* somebody holds the lock */
		else if ((ulwp = queue_waiter(qp, rwlp)) == NULL) {
			atomic_and_32(rwstate, ~URW_HAS_WAITERS);
			break;		/* no queued waiters */
		} else {
			int our_pri = real_priority(self);
			int his_pri = real_priority(ulwp);

			if (rd_wr == WRITE_LOCK) {
				/*
				 * We defer to a queued thread that has
				 * a higher priority than ours.
				 */
				if (his_pri <= our_pri)
					break;
			} else {
				/*
				 * We defer to a queued thread that has
				 * a higher priority than ours or that
				 * is a writer whose priority equals ours.
				 */
				if (his_pri < our_pri ||
				    (his_pri == our_pri && !ulwp->ul_writer))
					break;
			}
		}
		/*
		 * We are about to block.
		 * If we're doing a trylock, return EBUSY instead.
		 */
		if (try_flag) {
			error = EBUSY;
			break;
		}
		/*
		 * Enqueue writers ahead of readers of the
		 * same priority.
		 */
		self->ul_writer = rd_wr;	/* *must* be 0 or 1 */
		enqueue(qp, self, rwlp, MX);
		set_parking_flag(self, 1);
		queue_unlock(qp);
		if ((error = __lwp_park(tsp, 0)) == EINTR)
			error = 0;
		self->ul_writer = 0;
		set_parking_flag(self, 0);
		qp = queue_lock(rwlp, MX);
		if (self->ul_sleepq && dequeue_self(qp, rwlp) == 0)
			atomic_and_32(rwstate, ~URW_HAS_WAITERS);
	}

	if (error == 0) {
		if (rd_wr == READ_LOCK) {
			if (!read_lock_try(rwlp, 1))
				goto retry;
		} else {
			if (!write_lock_try(rwlp, 1))
				goto retry;
		}
	}

out:
	queue_unlock(qp);

	if (!try_flag) {
		DTRACE_PROBE3(plockstat, rw__blocked, rwlp, rd_wr, error == 0);
	}

	return (error);
}

int
rw_rdlock_impl(rwlock_t *rwlp, timespec_t *tsp)
{
	ulwp_t *self = curthread;
	uberdata_t *udp = self->ul_uberdata;
	readlock_t *readlockp;
	tdb_rwlock_stats_t *rwsp = RWLOCK_STATS(rwlp, udp);
	int error;

	/*
	 * If we already hold a readers lock on this rwlock,
	 * just increment our reference count and return.
	 */
	sigoff(self);
	readlockp = rwl_entry(rwlp);
	if (readlockp->rd_count != 0) {
		if (readlockp->rd_count == READ_LOCK_MAX) {
			sigon(self);
			error = EAGAIN;
			goto out;
		}
		sigon(self);
		error = 0;
		goto out;
	}
	sigon(self);

	/*
	 * If we hold the writer lock, bail out.
	 */
	if (rw_write_is_held(rwlp)) {
		if (self->ul_error_detection)
			rwlock_error(rwlp, "rwlock_rdlock",
			    "calling thread owns the writer lock");
		error = EDEADLK;
		goto out;
	}

	if (read_lock_try(rwlp, 0))
		error = 0;
	else if (rwlp->rwlock_type == USYNC_PROCESS)	/* kernel-level */
		error = shared_rwlock_lock(rwlp, tsp, READ_LOCK);
	else						/* user-level */
		error = rwlock_lock(rwlp, tsp, READ_LOCK);

out:
	if (error == 0) {
		sigoff(self);
		rwl_entry(rwlp)->rd_count++;
		sigon(self);
		if (rwsp)
			tdb_incr(rwsp->rw_rdlock);
		DTRACE_PROBE2(plockstat, rw__acquire, rwlp, READ_LOCK);
	} else {
		DTRACE_PROBE3(plockstat, rw__error, rwlp, READ_LOCK, error);
	}

	return (error);
}

#pragma weak rw_rdlock = __rw_rdlock
#pragma weak _rw_rdlock = __rw_rdlock
#pragma weak pthread_rwlock_rdlock = __rw_rdlock
#pragma weak _pthread_rwlock_rdlock = __rw_rdlock
int
__rw_rdlock(rwlock_t *rwlp)
{
	ASSERT(!curthread->ul_critical || curthread->ul_bindflags);
	return (rw_rdlock_impl(rwlp, NULL));
}

void
lrw_rdlock(rwlock_t *rwlp)
{
	enter_critical(curthread);
	(void) rw_rdlock_impl(rwlp, NULL);
}

#pragma weak pthread_rwlock_reltimedrdlock_np = \
	_pthread_rwlock_reltimedrdlock_np
int
_pthread_rwlock_reltimedrdlock_np(rwlock_t *rwlp, const timespec_t *reltime)
{
	timespec_t tslocal = *reltime;
	int error;

	ASSERT(!curthread->ul_critical || curthread->ul_bindflags);
	error = rw_rdlock_impl(rwlp, &tslocal);
	if (error == ETIME)
		error = ETIMEDOUT;
	return (error);
}

#pragma weak pthread_rwlock_timedrdlock = _pthread_rwlock_timedrdlock
int
_pthread_rwlock_timedrdlock(rwlock_t *rwlp, const timespec_t *abstime)
{
	timespec_t tslocal;
	int error;

	ASSERT(!curthread->ul_critical || curthread->ul_bindflags);
	abstime_to_reltime(CLOCK_REALTIME, abstime, &tslocal);
	error = rw_rdlock_impl(rwlp, &tslocal);
	if (error == ETIME)
		error = ETIMEDOUT;
	return (error);
}

int
rw_wrlock_impl(rwlock_t *rwlp, timespec_t *tsp)
{
	ulwp_t *self = curthread;
	uberdata_t *udp = self->ul_uberdata;
	tdb_rwlock_stats_t *rwsp = RWLOCK_STATS(rwlp, udp);
	int error;

	/*
	 * If we hold a readers lock on this rwlock, bail out.
	 */
	if (rw_read_is_held(rwlp)) {
		if (self->ul_error_detection)
			rwlock_error(rwlp, "rwlock_wrlock",
			    "calling thread owns the readers lock");
		error = EDEADLK;
		goto out;
	}

	/*
	 * If we hold the writer lock, bail out.
	 */
	if (rw_write_is_held(rwlp)) {
		if (self->ul_error_detection)
			rwlock_error(rwlp, "rwlock_wrlock",
			    "calling thread owns the writer lock");
		error = EDEADLK;
		goto out;
	}

	if (write_lock_try(rwlp, 0))
		error = 0;
	else if (rwlp->rwlock_type == USYNC_PROCESS)	/* kernel-level */
		error = shared_rwlock_lock(rwlp, tsp, WRITE_LOCK);
	else						/* user-level */
		error = rwlock_lock(rwlp, tsp, WRITE_LOCK);

out:
	if (error == 0) {
		rwlp->rwlock_owner = (uintptr_t)self;
		if (rwlp->rwlock_type == USYNC_PROCESS)
			rwlp->rwlock_ownerpid = udp->pid;
		if (rwsp) {
			tdb_incr(rwsp->rw_wrlock);
			rwsp->rw_wrlock_begin_hold = gethrtime();
		}
		DTRACE_PROBE2(plockstat, rw__acquire, rwlp, WRITE_LOCK);
	} else {
		DTRACE_PROBE3(plockstat, rw__error, rwlp, WRITE_LOCK, error);
	}
	return (error);
}

#pragma weak rw_wrlock = __rw_wrlock
#pragma weak _rw_wrlock = __rw_wrlock
#pragma weak pthread_rwlock_wrlock = __rw_wrlock
#pragma weak _pthread_rwlock_wrlock = __rw_wrlock
int
__rw_wrlock(rwlock_t *rwlp)
{
	ASSERT(!curthread->ul_critical || curthread->ul_bindflags);
	return (rw_wrlock_impl(rwlp, NULL));
}

void
lrw_wrlock(rwlock_t *rwlp)
{
	enter_critical(curthread);
	(void) rw_wrlock_impl(rwlp, NULL);
}

#pragma weak pthread_rwlock_reltimedwrlock_np = \
	_pthread_rwlock_reltimedwrlock_np
int
_pthread_rwlock_reltimedwrlock_np(rwlock_t *rwlp, const timespec_t *reltime)
{
	timespec_t tslocal = *reltime;
	int error;

	ASSERT(!curthread->ul_critical || curthread->ul_bindflags);
	error = rw_wrlock_impl(rwlp, &tslocal);
	if (error == ETIME)
		error = ETIMEDOUT;
	return (error);
}

#pragma weak pthread_rwlock_timedwrlock = _pthread_rwlock_timedwrlock
int
_pthread_rwlock_timedwrlock(rwlock_t *rwlp, const timespec_t *abstime)
{
	timespec_t tslocal;
	int error;

	ASSERT(!curthread->ul_critical || curthread->ul_bindflags);
	abstime_to_reltime(CLOCK_REALTIME, abstime, &tslocal);
	error = rw_wrlock_impl(rwlp, &tslocal);
	if (error == ETIME)
		error = ETIMEDOUT;
	return (error);
}

#pragma weak rw_tryrdlock = __rw_tryrdlock
#pragma weak _rw_tryrdlock = __rw_tryrdlock
#pragma weak pthread_rwlock_tryrdlock = __rw_tryrdlock
#pragma weak _pthread_rwlock_tryrdlock = __rw_tryrdlock
int
__rw_tryrdlock(rwlock_t *rwlp)
{
	ulwp_t *self = curthread;
	uberdata_t *udp = self->ul_uberdata;
	tdb_rwlock_stats_t *rwsp = RWLOCK_STATS(rwlp, udp);
	readlock_t *readlockp;
	int error;

	ASSERT(!curthread->ul_critical || curthread->ul_bindflags);

	if (rwsp)
		tdb_incr(rwsp->rw_rdlock_try);

	/*
	 * If we already hold a readers lock on this rwlock,
	 * just increment our reference count and return.
	 */
	sigoff(self);
	readlockp = rwl_entry(rwlp);
	if (readlockp->rd_count != 0) {
		if (readlockp->rd_count == READ_LOCK_MAX) {
			sigon(self);
			error = EAGAIN;
			goto out;
		}
		sigon(self);
		error = 0;
		goto out;
	}
	sigon(self);

	if (read_lock_try(rwlp, 0))
		error = 0;
	else if (rwlp->rwlock_type == USYNC_PROCESS)	/* kernel-level */
		error = shared_rwlock_lock(rwlp, NULL, READ_LOCK_TRY);
	else						/* user-level */
		error = rwlock_lock(rwlp, NULL, READ_LOCK_TRY);

out:
	if (error == 0) {
		sigoff(self);
		rwl_entry(rwlp)->rd_count++;
		sigon(self);
		DTRACE_PROBE2(plockstat, rw__acquire, rwlp, READ_LOCK);
	} else {
		if (rwsp)
			tdb_incr(rwsp->rw_rdlock_try_fail);
		if (error != EBUSY) {
			DTRACE_PROBE3(plockstat, rw__error, rwlp, READ_LOCK,
			    error);
		}
	}

	return (error);
}

#pragma weak rw_trywrlock = __rw_trywrlock
#pragma weak _rw_trywrlock = __rw_trywrlock
#pragma weak pthread_rwlock_trywrlock = __rw_trywrlock
#pragma weak _pthread_rwlock_trywrlock = __rw_trywrlock
int
__rw_trywrlock(rwlock_t *rwlp)
{
	ulwp_t *self = curthread;
	uberdata_t *udp = self->ul_uberdata;
	tdb_rwlock_stats_t *rwsp = RWLOCK_STATS(rwlp, udp);
	int error;

	ASSERT(!self->ul_critical || self->ul_bindflags);

	if (rwsp)
		tdb_incr(rwsp->rw_wrlock_try);

	if (write_lock_try(rwlp, 0))
		error = 0;
	else if (rwlp->rwlock_type == USYNC_PROCESS)	/* kernel-level */
		error = shared_rwlock_lock(rwlp, NULL, WRITE_LOCK_TRY);
	else						/* user-level */
		error = rwlock_lock(rwlp, NULL, WRITE_LOCK_TRY);

	if (error == 0) {
		rwlp->rwlock_owner = (uintptr_t)self;
		if (rwlp->rwlock_type == USYNC_PROCESS)
			rwlp->rwlock_ownerpid = udp->pid;
		if (rwsp)
			rwsp->rw_wrlock_begin_hold = gethrtime();
		DTRACE_PROBE2(plockstat, rw__acquire, rwlp, WRITE_LOCK);
	} else {
		if (rwsp)
			tdb_incr(rwsp->rw_wrlock_try_fail);
		if (error != EBUSY) {
			DTRACE_PROBE3(plockstat, rw__error, rwlp, WRITE_LOCK,
			    error);
		}
	}
	return (error);
}

#pragma weak rw_unlock = __rw_unlock
#pragma weak _rw_unlock = __rw_unlock
#pragma weak pthread_rwlock_unlock = __rw_unlock
#pragma weak _pthread_rwlock_unlock = __rw_unlock
int
__rw_unlock(rwlock_t *rwlp)
{
	volatile uint32_t *rwstate = (volatile uint32_t *)&rwlp->rwlock_readers;
	uint32_t readers;
	ulwp_t *self = curthread;
	uberdata_t *udp = self->ul_uberdata;
	tdb_rwlock_stats_t *rwsp;
	queue_head_t *qp;
	int rd_wr;
	int waked = 0;

	readers = *rwstate;
	ASSERT_CONSISTENT_STATE(readers);
	if (readers & URW_WRITE_LOCKED) {
		rd_wr = WRITE_LOCK;
		readers = 0;
	} else {
		rd_wr = READ_LOCK;
		readers &= URW_READERS_MASK;
	}

	if (rd_wr == WRITE_LOCK) {
		/*
		 * Since the writer lock is held, we'd better be
		 * holding it, else we cannot legitimately be here.
		 */
		if (!rw_write_is_held(rwlp)) {
			if (self->ul_error_detection)
				rwlock_error(rwlp, "rwlock_unlock",
				    "writer lock held, "
				    "but not by the calling thread");
			return (EPERM);
		}
		if ((rwsp = RWLOCK_STATS(rwlp, udp)) != NULL) {
			if (rwsp->rw_wrlock_begin_hold)
				rwsp->rw_wrlock_hold_time +=
				    gethrtime() - rwsp->rw_wrlock_begin_hold;
			rwsp->rw_wrlock_begin_hold = 0;
		}
		rwlp->rwlock_owner = 0;
		rwlp->rwlock_ownerpid = 0;
	} else if (readers > 0) {
		/*
		 * A readers lock is held; if we don't hold one, bail out.
		 */
		readlock_t *readlockp;

		sigoff(self);
		readlockp = rwl_entry(rwlp);
		if (readlockp->rd_count == 0) {
			sigon(self);
			if (self->ul_error_detection)
				rwlock_error(rwlp, "rwlock_unlock",
				    "readers lock held, "
				    "but not by the calling thread");
			return (EPERM);
		}
		/*
		 * If we hold more than one readers lock on this rwlock,
		 * just decrement our reference count and return.
		 */
		if (--readlockp->rd_count != 0) {
			sigon(self);
			goto out;
		}
		sigon(self);
	} else {
		/*
		 * This is a usage error.
		 * No thread should release an unowned lock.
		 */
		if (self->ul_error_detection)
			rwlock_error(rwlp, "rwlock_unlock", "lock not owned");
		return (EPERM);
	}

	if (rd_wr == WRITE_LOCK && write_unlock_try(rwlp)) {
		/* EMPTY */;
	} else if (rd_wr == READ_LOCK && read_unlock_try(rwlp)) {
		/* EMPTY */;
	} else if (rwlp->rwlock_type == USYNC_PROCESS) {
		(void) _private_mutex_lock(&rwlp->mutex);
		(void) __lwp_rwlock_unlock(rwlp);
		(void) _private_mutex_unlock(&rwlp->mutex);
		waked = 1;
	} else {
		qp = queue_lock(rwlp, MX);
		if (rd_wr == READ_LOCK)
			atomic_dec_32(rwstate);
		else
			atomic_and_32(rwstate, ~URW_WRITE_LOCKED);
		waked = rw_queue_release(qp, rwlp);
	}

out:
	DTRACE_PROBE2(plockstat, rw__release, rwlp, rd_wr);

	/*
	 * Yield to the thread we just waked up, just in case we might
	 * be about to grab the rwlock again immediately upon return.
	 * This is pretty weak but it helps on a uniprocessor and also
	 * when cpu affinity has assigned both ourself and the other
	 * thread to the same CPU.  Note that lwp_yield() will yield
	 * the processor only if the writer is at the same or higher
	 * priority than ourself.  This provides more balanced program
	 * behavior; it doesn't guarantee acquisition of the lock by
	 * the pending writer.
	 */
	if (waked)
		lwp_yield();
	return (0);
}

void
lrw_unlock(rwlock_t *rwlp)
{
	(void) __rw_unlock(rwlp);
	exit_critical(curthread);
}
