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
 * Copyright (c) 2007-2008 NEC Corporation
 * All rights reserved.
 */

/*	This file is used as input file to make librt.	*/
/*	There is no GLOBAL symbol in made librt.		*/

#define FILTER_SYM(x)		\
	.global	x;		\
	.type	x, %function;	\
x:;

	.struct	0

	FILTER_SYM(clock_nanosleep)
	FILTER_SYM(mq_reltimedreceive_np)
	FILTER_SYM(mq_reltimedsend_np)
	FILTER_SYM(mq_timedreceive)
	FILTER_SYM(mq_timedsend)
	FILTER_SYM(sem_reltimedwait_np)
	FILTER_SYM(sem_timedwait)
	FILTER_SYM(aio_waitn)
	FILTER_SYM(aio_waitn64)
	FILTER_SYM(close)
	FILTER_SYM(aio_cancel64)
	FILTER_SYM(aio_error64)
	FILTER_SYM(aio_fsync64)
	FILTER_SYM(aio_read64)
	FILTER_SYM(aio_return64)
	FILTER_SYM(aio_suspend64)
	FILTER_SYM(aio_write64)
	FILTER_SYM(fork)
	FILTER_SYM(lio_listio64)
	FILTER_SYM(aio_cancel)
	FILTER_SYM(aio_error)
	FILTER_SYM(aio_fsync)
	FILTER_SYM(aio_read)
	FILTER_SYM(aio_return)
	FILTER_SYM(aio_suspend)
	FILTER_SYM(aio_write)
	FILTER_SYM(clock_getres)
	FILTER_SYM(clock_gettime)
	FILTER_SYM(clock_settime)
	FILTER_SYM(fdatasync)
	FILTER_SYM(lio_listio)
	FILTER_SYM(mq_close)
	FILTER_SYM(mq_getattr)
	FILTER_SYM(mq_notify)
	FILTER_SYM(mq_open)
	FILTER_SYM(mq_receive)
	FILTER_SYM(mq_send)
	FILTER_SYM(mq_setattr)
	FILTER_SYM(mq_unlink)
	FILTER_SYM(nanosleep)
	FILTER_SYM(sched_getparam)
	FILTER_SYM(sched_get_priority_max)
	FILTER_SYM(sched_get_priority_min)
	FILTER_SYM(sched_getscheduler)
	FILTER_SYM(sched_rr_get_interval)
	FILTER_SYM(sched_setparam)
	FILTER_SYM(sched_setscheduler)
	FILTER_SYM(sched_yield)
	FILTER_SYM(sem_close)
	FILTER_SYM(sem_destroy)
	FILTER_SYM(sem_getvalue)
	FILTER_SYM(sem_init)
	FILTER_SYM(sem_open)
	FILTER_SYM(sem_post)
	FILTER_SYM(sem_trywait)
	FILTER_SYM(sem_unlink)
	FILTER_SYM(sem_wait)
	FILTER_SYM(shm_open)
	FILTER_SYM(shm_unlink)
	FILTER_SYM(sigqueue)
	FILTER_SYM(sigtimedwait)
	FILTER_SYM(sigwaitinfo)
	FILTER_SYM(timer_create)
	FILTER_SYM(timer_delete)
	FILTER_SYM(timer_getoverrun)
	FILTER_SYM(timer_gettime)
	FILTER_SYM(timer_settime)
	FILTER_SYM(_clock_getres)
	FILTER_SYM(_clock_gettime)
	FILTER_SYM(_clock_nanosleep)
	FILTER_SYM(_clock_settime)
	FILTER_SYM(_nanosleep)
	FILTER_SYM(_sem_close)
	FILTER_SYM(_sem_destroy)
	FILTER_SYM(_sem_getvalue)
	FILTER_SYM(_sem_init)
	FILTER_SYM(_sem_open)
	FILTER_SYM(_sem_post)
	FILTER_SYM(_sem_reltimedwait_np)
	FILTER_SYM(_sem_timedwait)
	FILTER_SYM(_sem_trywait)
	FILTER_SYM(_sem_unlink)
	FILTER_SYM(_sem_wait)
	FILTER_SYM(_sigqueue)
	FILTER_SYM(_sigtimedwait)
	FILTER_SYM(_sigwaitinfo)
	FILTER_SYM(_timer_create)
	FILTER_SYM(_timer_delete)
	FILTER_SYM(_timer_getoverrun)
	FILTER_SYM(_timer_gettime)
	FILTER_SYM(_timer_settime)
