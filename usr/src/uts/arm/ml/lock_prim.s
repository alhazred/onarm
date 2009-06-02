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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2006-2008 NEC Corporation
 */

	.ident	"@(#)lock_prim.s"
	.file	"lock_prim.s"

#include <sys/asm_linkage.h>
#include <asm/thread.h>
#include <sys/lockstat.h>
#include <sys/machlock.h>
#include <sys/mutex_impl.h>
#include <sys/rwlock_impl.h>
#include <sys/cpuvar_impl.h>
#include <asm/cpufunc.h>
#include "assym.h"

/*
 * Call lockstat_wrapper() if current thread is not NULL.
 *
 * Remarks:
 *	This macro will destroy r0-r3.
 *	The caller must set lock pointer to r1.
 *	The caller must preserve at least lr.
 */
#define	CALL_LOCKSTAT_WRAPPER(event)					\
	THREADP(r2);			/* r2 = curthread() */		\
	teq	r2, #0;							\
	movne	r0, #(event * DTRACE_IDSIZE);				\
	blne	lockstat_wrapper	/* Call lockstat_wrapper() */

/*
 * Call lockstat_wrapper_arg() if current thread is not NULL.
 *
 * Remarks:
 *	This macro will destroy r0-r3.
 *	The caller must set lock pointer to r1.
 *	The caller must preserve at least lr.
 */
#define	CALL_LOCKSTAT_WRAPPER_ARG(event, arg0)				\
	THREADP(r3);			/* r3 = curthread() */		\
	teq	r3, #0;							\
	movne	r0, #(event * DTRACE_IDSIZE);				\
	movne	r2, #(arg0);						\
	blne	lockstat_wrapper_arg	/* Call lockstat_wrapper_arg() */

/*
 * int
 * lock_try(lock_t *lp)
 *	Try to acquire lock.
 *	0xFF in lock_t means the lock is busy.
 *
 * Calling/Exit State:
 *	Upon successful completion, lock_try() returns non-zero value.
 *	lock_try() doesn't block interrupts so don't use this to spin
 *	on a lock.
 */
ENTRY(lock_try)
	mov	r1, #LOCK_HELD_VALUE
1:
	ldrexb	r3, [r0]
	strexb	r2, r1, [r0]
	teq	r2, #0
	bne	1b			/* Retry when fails */

	mov	r1, r0			/* Required for hot patch */
	eors	r0, r3, #LOCK_HELD_VALUE
	mcrne	p15, 0, r2, c7, c10, 5	/* Issue memory barrier on success */
.lock_try_lockstat_patch_point:
	mov	pc, lr
	moveq	pc, lr			/* Return on failure */

	stmfd	sp!, {r0, lr}
	CALL_LOCKSTAT_WRAPPER(LS_LOCK_TRY_ACQUIRE)
	ldmfd	sp!, {r0, pc}
	SET_SIZE(lock_try)

/*
 * int
 * lock_spin_try(lock_t *lp)
 *	Same as lock_try(), but it has no patch point for dtrace.
 */
ENTRY(lock_spin_try)
	mov	r3, #LOCK_HELD_VALUE
1:
	ldrexb	r1, [r0]
	strexb	r2, r3, [r0]
	teq	r2, #0
	bne	1b			/* Retry when fails */

	eors	r0, r1, #LOCK_HELD_VALUE
	mcrne	p15, 0, r2, c7, c10, 5	/* Issue memory barrier on success */
	mov	pc, lr
	SET_SIZE(lock_spin_try)

/*
 * int
 * ulock_try(lock_t *lp)
 *	Try to acquire lock.
 *	Same as lock_try(), but the specified address must be a user address.
 *
 * Remarks:
 *	On MPCore, user and kernel spaces are in the same context.
 *	So we can access user space using ldrex/strex.
 *	The caller must guarantee that the lock bits is allowed to access.
 *
 *	If the specified address is not mapped, fatal data abort will
 *	be raised.
 */
ENTRY(ulock_try)
	mov	r3, #1		/* Same lock value as that used in libc */
1:
	ldrexb	r1, [r0]
	strexb	r2, r3, [r0]
	teq	r2, #0
	bne	1b			/* Retry when fails */

	eors	r0, r1, #1	/* Same lock value as that used in libc */
	mcrne	p15, 0, r2, c7, c10, 5	/* Issue memory barrier on success */
	mov	pc, lr
	SET_SIZE(ulock_try)

/*
 * void
 * lock_clear(lock_t *lp)
 *	Release lock without changing interrupt priority level.
 */
ENTRY(lock_clear)
	mov	r3, #0
	MEMORY_BARRIER(r3)		/* Memory barrier */
1:
	ldrexb	r1, [r0]
	strexb	r2, r3, [r0]
	teq	r2, #0
	bne	1b			/* Retry when fails */

.lock_clear_lockstat_patch_point:
	mov	pc, lr
	stmfd	sp!, {lr}
	mov	r1, r0
	CALL_LOCKSTAT_WRAPPER(LS_LOCK_CLEAR_RELEASE)
	ldmfd	sp!, {pc}
	SET_SIZE(lock_clear)

/*
 * void
 * ulock_clear(lock_t *lp)
 *	Same as lock_clear(), but the specified adderss must be a user address.
 *	See comment on lock_try().
 */
ENTRY(ulock_clear)
	mov	r3, #0
	MEMORY_BARRIER(r3)		/* Memory barrier */
1:
	ldrexb	r1, [r0]
	strexb	r2, r3, [r0]
	teq	r2, #0
	bne	1b			/* Retry when fails */
	mov	pc, lr
	SET_SIZE(ulock_clear)

/*
 * void
 * lock_set_spl(lock_t *lp, int new_pil, u_short *old_pil_addr)
 *	Acquire lock, and set interrupt priority to new_pil.
 *	Old priority is stored in *old_pil.
 */
	.globl	splr
	.globl	lock_set_spl_spin
ENTRY(lock_set_spl)
	stmfd	sp!, {r4-r6, lr}
	mov	r4, r0
	mov	r5, r1
	mov	r6, r2
	mov	r0, r1
	bl	splr			/* Raise priority level */

	/* r0 = old PIL */
	mov	r3, #LOCK_HELD_VALUE
1:
	ldrexb	r1, [r4]
	strexb	r2, r3, [r4]
	teq	r2, #0
	bne	1b			/* Retry when fails */

	/* Check result */
	eors	r1, r1, #LOCK_HELD_VALUE
	beq	10f

	/* Succeeded. */
	strh	r0, [r6]		/* *old_pil_addr = old PIL */
	MEMORY_BARRIER(r2)		/* Memory barrier */
	mov	r1, r4			/* Required for hot patch */
	ldmfd	sp!, {r4-r6, lr}
.lock_set_spl_lockstat_patch_point:
	mov	pc, lr
	stmfd	sp!, {lr}
	CALL_LOCKSTAT_WRAPPER(LS_LOCK_SET_SPL_ACQUIRE)
	ldmfd	sp!, {pc}
	/* NOTREACHED */
10:
	/* Failed. Let lock_set_spl_spin() do the rest of work. */
	mov	r3, r0			/* r3 = old PIL */
	mov	r0, r4			/* r0 = lp */
	mov	r1, r5			/* r1 = new_pil */
	mov	r2, r6			/* r2 = old_pil_addr */
	ldmfd	sp!, {r4-r6, lr}
	b	lock_set_spl_spin
	SET_SIZE(lock_set_spl)

/*
 * void
 * lock_init(lock_t *lp)
 *	Initialize lock.
 */
ENTRY(lock_init)
	mov	r1, #0
	strb	r1, [r0]
	mov	pc, lr
	SET_SIZE(lock_init)

/*
 * void
 * lock_set(lp)
 *	Acquire spin lock.
 */
	.globl	lock_set_spin
ENTRY(lock_set)
	mov	r3, #LOCK_HELD_VALUE
1:
	ldrexb	r1, [r0]
	strexb	r2, r3, [r0]
	teq	r2, #0
	bne	1b			/* Retry when fails */

	eors	r1, r1, #LOCK_HELD_VALUE
	/* If failed, let lock_set_spin() do the rest of work. */
	beq	lock_set_spin

	/* Succeeded. */
	MEMORY_BARRIER(r2)		/* Memory barrier */
.lock_set_lockstat_patch_point:
	mov	pc, lr
	stmfd	sp!, {lr}
	mov	r1, r0
	CALL_LOCKSTAT_WRAPPER(LS_LOCK_SET_ACQUIRE)
	ldmfd	sp!, {pc}
	SET_SIZE(lock_set)

/*
 * void
 * lock_clear_splx(lock_t *lp, int s)
 *	Release lock and restore PIL.
 */
	.globl	splx
ENTRY(lock_clear_splx)
	stmfd	sp!, {r4, lr}
	mov	r3, #0
	MEMORY_BARRIER(r3)		/* Memory barrier */
1:
	ldrexb	r2, [r0]
	strexb	r4, r3, [r0]
	teq	r4, #0
	bne	1b			/* Retry when fails */

	mov	r4, r0			/* Required for hot patch */
	mov	r0, r1
	bl	splx			/* Restore PIL */
	mov	r1, r4			/* Required for hot patch */
	ldmfd	sp!, {r4, lr}
.lock_clear_splx_lockstat_patch_point:
	mov	pc, lr
	stmfd	sp!, {lr}
	CALL_LOCKSTAT_WRAPPER(LS_LOCK_CLEAR_SPLX_RELEASE)
	ldmfd	sp!, {pc}
	SET_SIZE(lock_clear_splx)

/*
 * mutex_enter() and mutex_exit().
 *
 * These routines handle the simple cases of mutex_enter() (adaptive
 * lock, not held) and mutex_exit() (adaptive lock, held, no waiters).
 * If anything complicated is going on we punt to mutex_vector_enter().
 *
 * mutex_tryenter() is similar to mutex_enter() but returns zero if
 * the lock cannot be acquired, nonzero on success.
 *
 * If mutex_exit() gets preempted in the window between checking waiters
 * and clearing the lock, we can miss wakeups.  Disabling preemption
 * in the mutex code is prohibitively expensive, so instead we detect
 * mutex preemption by examining the trapped PC in the interrupt path.
 * If we interrupt a thread in mutex_exit() that has not yet cleared
 * the lock, cmnint() resets its PC back to the beginning of
 * mutex_exit() so it will check again for waiters when it resumes.
 *
 * The lockstat code below is activated when the lockstat driver
 * calls lockstat_hot_patch() to hot-patch the kernel mutex code.
 * Note that we don't need to test lockstat_event_mask here -- we won't
 * patch this code in unless we're gathering ADAPTIVE_HOLD lockstats.
 */

/*
 * void
 * mutex_enter(kmutex_t *lp)
 *	Acquire mutex lock.
 */
	.globl	mutex_vector_enter
ENTRY(mutex_enter)
	THREADP(r3)			/* r3 = curthread() */
1:
	ldrex	r2, [r0]
	teq	r2, #0			/* if (lp->m_owner != NULL) */
	bne	mutex_vector_enter	/*   mutex_vector_enter, return */
	strex	r2, r3, [r0]
	teq	r2, #0
	bne	1b			/* Retry when fails */
	MEMORY_BARRIER(r2)		/* Memory barrier */
.mutex_enter_lockstat_patch_point:
	mov	pc, lr
	stmfd	sp!, {lr}
	mov	r1, r0
	CALL_LOCKSTAT_WRAPPER(LS_MUTEX_ENTER_ACQUIRE)
	ldmfd	sp!, {pc}
	SET_SIZE(mutex_enter)

/*
 * int
 * mutex_tryenter(kmutex_t *lp)
 *	Try to acquire mutex lock.
 */
	.globl	mutex_vector_tryenter
ENTRY(mutex_tryenter)
	THREADP(r3)			/* r3 = curthread() */
1:
	ldrex	r2, [r0]
	teq	r2, #0			/* if (lp->m_owner != NULL) */
	bne	mutex_vector_tryenter	/*   mutex_vector_tryenter, return */
	strex	r2, r3, [r0]
	teq	r2, #0
	bne	1b			/* Retry when fails */
	mov	r1, r0			/* Required for hot patch */
	MEMORY_BARRIER(r2)		/* Memory barrier */
	mov	r0, #1
.mutex_tryenter_lockstat_patch_point:
	mov	pc, lr
	stmfd	sp!, {lr}
	CALL_LOCKSTAT_WRAPPER(LS_MUTEX_ENTER_ACQUIRE)
	mov	r0, #1
	ldmfd	sp!, {pc}
	SET_SIZE(mutex_tryenter)

/*
 * int
 * mutex_adaptive_tryenter(mutex_impl_t *lp)
 *	Same as mutex_tryenter(), but:
 *	- No patch point
 *	- Return 0 on error.
 */
ENTRY(mutex_adaptive_tryenter)
	THREADP(r3)			/* r3 = curthread() */
1:
	ldrex	r2, [r0]
	teq	r2, #0			/* if (lp->m_owner != NULL) */
	movne	r0, #0			/*   r0 = 0 */
	bne	10f			/*   goto 10f */
	strex	r2, r3, [r0]
	teq	r2, #0
	bne	1b			/* Retry when fails */
	MEMORY_BARRIER(r2)		/* Memory barrier */
	mov	r0, #1
10:
	mov	pc, lr
	SET_SIZE(mutex_adaptive_tryenter)

/*
 * void *
 * mutex_owner_running(mutex_impl_t *lp)
 *	Return struct cpu where mutex owner thread is currently running on.
 *	NULL is returned if owner is not running.
 */
	.globl	mutex_owner_running_critical_start
ENTRY(mutex_owner_running)
mutex_owner_running_critical_start:
	ldr	r3, [r0]
	bics	r3, r3, #MUTEX_THREAD_CLRBITS	/* r3 = lp->m_owner */
	beq	1f			/* if (owner == NULL) return NULL */

	ldr	r1, [r3, #T_CPU]	/* r1 = owner->t_cpu */
	ldr	r2, [r1, #CPU_THREAD]	/* r2 = owner->t_cpu->cpu_thread */
.mutex_owner_running_critical_end:
	teq	r3, r2			/* owner == running thread ? */
	moveq	r0, r1			/* yes: return cpu */
	movne	r0, #0			/* no: return NULL */
	mov	pc, lr
1:
	mov	r0, #0			/* return NULL */
	mov	pc, lr
	SET_SIZE(mutex_owner_running)

	.globl	mutex_owner_running_critical_size
	.type	mutex_owner_running_critical_size, %object
	.align	CPTRSHIFT
mutex_owner_running_critical_size:
	.long	.mutex_owner_running_critical_end - mutex_owner_running_critical_start
	SET_SIZE(mutex_owner_running_critical_size)

/*
 * void
 * mutex_exit(kmutex_t *lp)
 *	Release mutex.
 */
	.globl	mutex_exit_critical_start
	.globl	mutex_vector_exit
ENTRY(mutex_exit)
mutex_exit_critical_start:	/* If we are interrupted, restart here */
	THREADP(r3)			/* r3 = curthread() */
	mov	r2, #0
	MEMORY_BARRIER(r2)		/* Memory barrier */
1:
	ldrex	r1, [r0]
	teq	r1, r3			/* if (lp->m_owner != curthread()) */
	bne	mutex_vector_exit	/*   mutex_vector_exit, return */

	/* We own this lock without waiter. */
	strex	r1, r2, [r0]		/* Clear owner and lock */
.mutex_exit_critical_end:
	teq	r1, #0
	bne	1b			/* Retry when fails */
.mutex_exit_lockstat_patch_point:
	mov	pc, lr
	stmfd	sp!, {lr}
	mov	r1, r0
	CALL_LOCKSTAT_WRAPPER(LS_MUTEX_EXIT_RELEASE)
	ldmfd	sp!, {pc}
	SET_SIZE(mutex_exit)

	.globl	mutex_exit_critical_size
	.type	mutex_exit_critical_size, %object
	.align	CPTRSHIFT
mutex_exit_critical_size:
	.long	.mutex_exit_critical_end - mutex_exit_critical_start
	SET_SIZE(mutex_exit_critical_size)

/*
 * rw_enter() and rw_exit().
 *
 * These routines handle the simple cases of rw_enter (write-locking an unheld
 * lock or read-locking a lock that's neither write-locked nor write-wanted)
 * and rw_exit (no waiters or not the last reader).  If anything complicated
 * is going on we punt to rw_enter_sleep() and rw_exit_wakeup(), respectively.
 */

#define	THREAD_KPRI_REQUEST(tp, reg)					\
	/* tp->t_kpri_req++ */						\
	ldr	reg, [tp, #T_KPRI_REQ];					\
	add	reg, reg, #1;						\
	str	reg, [tp, #T_KPRI_REQ]

#define	THREAD_KPRI_RELEASE(tp, reg)					\
	/* tp->t_kpri_req-- */						\
	ldr	reg, [tp, #T_KPRI_REQ];					\
	sub	reg, reg, #1;						\
	str	reg, [tp, #T_KPRI_REQ]

/*
 * void
 * rw_enter(krwlock_t *lp, krw_t rw)
 *	Acquire read/write lock as the specified mode.
 */
	.globl	rw_enter_sleep
ENTRY(rw_enter)
	THREADP(r3)			/* r3 = curthread() */
	teq	r1, #RW_WRITER		/* if entering as writer */
	beq	10f			/*   goto 10f */

	/* Acquire reader lock */
	THREAD_KPRI_REQUEST(r3, r2)
1:
	ldrex	r2, [r0]		/* r2 = lp->rw_wwwh */
	tst	r2, #RW_WRITE_CLAIMED	/* if (r2 & RW_WRITE_CLAIMED) */
	bne	2f			/*   goto 2f */
	add	r2, r2, #RW_READ_LOCK	/* Increment hold count */
	strex	r1, r2, [r0]		/* Install new rw_wwwh */
	teq	r1, #0
	bne	1b			/* Retry when fails */
	mov	r3, #0
	MEMORY_BARRIER(r3)		/* Memory barrier */
.rw_read_enter_lockstat_patch_point:
	mov	pc, lr
	stmfd	sp!, {lr}
	mov	r1, r0
	CALL_LOCKSTAT_WRAPPER_ARG(LS_RW_ENTER_ACQUIRE, RW_READER)
	ldmfd	sp!, {pc}
2:
	/* We must block the thread. Let rw_enter_sleep() do the rest. */
	mov	r1, #RW_READER		/* strex might destroy r1. */
	b	rw_enter_sleep
	/* NOTREACHED */

	/* Acquire writer lock */
10:
	ldrex	r2, [r0]		/* r2 = lp->rw_wwwh */
	teq	r2, #0			/* if (r2 != 0) */
	bne	20f			/*   goto 20f */
	orr	r2, r3, #RW_WRITE_LOCKED
	strex	r1, r2, [r0]		/* Install writer lock */
	teq	r1, #0
	bne	10b			/* Retry when fails */
	mov	r3, #0
	MEMORY_BARRIER(r3)		/* Memory barrier */
.rw_write_enter_lockstat_patch_point:
	mov	pc, lr
	stmfd	sp!, {lr}
	mov	r1, r0
	CALL_LOCKSTAT_WRAPPER_ARG(LS_RW_ENTER_ACQUIRE, RW_WRITER)
	ldmfd	sp!, {pc}
20:
	/* We must block the thread. Let rw_enter_sleep() do the rest. */
	mov	r1, #RW_WRITER		/* strex might destroy r1. */
	b	rw_enter_sleep
	/* NOTREACHED */
	SET_SIZE(rw_enter)

/*
 * void
 * rw_exit(krwlock_t *lp)
 *	Release read/write lock.
 */
	.globl	rw_exit_wakeup
#ifdef	DEBUG
	.globl	panic
#endif	/* DEBUG */
ENTRY(rw_exit)
	ldr	r3, [r0]		/* Read lock value */
	mov	r2, #0
	MEMORY_BARRIER(r2)		/* Memory barrier */
	sub	r1, r3, #RW_READ_LOCK	/* r1 = new lock value for reader */
	teq	r1, #0			/* if !(single-reader && no waiter) */
	bne	10f			/*   goto 10 */

	/* Try to release read lock here. */
.Lrw_read_release:
	ldrex	r2, [r0]
	teq	r2, r3			/* if the lock has been changed */
	bne	rw_exit_wakeup		/*   rw_exit_wakeup, return */
	strex	r2, r1, [r0]		/* Install new lock value */
	teq	r2, #0
	bne	.Lrw_read_release	/* Retry when fails */
#ifdef	DEBUG
	tst	r1, #RW_HAS_WAITERS
	beq	1f
	mvn	r3, #RW_READ_LOCK
	add	r3, r3, #1		/* r3 = RW_HOLD_COUNT */
	tst	r1, r3
	beq	.Lrw_exit_panic
1:
#endif	/* DEBUG */
	THREADP(r3)
	THREAD_KPRI_RELEASE(r3, r2)
.rw_read_exit_lockstat_patch_point:
	mov	pc, lr
	stmfd	sp!, {lr}
	mov	r1, r0
	CALL_LOCKSTAT_WRAPPER_ARG(LS_RW_EXIT_RELEASE, RW_READER)
	ldmfd	sp!, {pc}
10:
	/* Check whether we own this lock as writer mode. */
	tst	r3, #RW_WRITE_LOCKED
	bne	.Lrw_write_release

	cmp	r1, #RW_READ_LOCK	/* if the lock would still be held */
	bhs	.Lrw_read_release	/*   goto .Lrw_read_release */
	b	rw_exit_wakeup		/* Let rw_exit_wakeup() do the rest */
.Lrw_write_release:
	/* Try to release write lock here. */
	THREADP(r3)
	orr	r3, r3, #RW_WRITE_LOCKED
	mov	r1, #0			/* r1 = new lock value (unheld) */
20:
	ldrex	r2, [r0]
	teq	r2, r3			/* if the lock has been changed */
	bne	rw_exit_wakeup		/*   rw_exit_wakeup, return */
	strex	r2, r1, [r0]		/* Install new lock value */
	teq	r2, #0
	bne	20b			/* Retry when fails */
.rw_write_exit_lockstat_patch_point:
	mov	pc, lr
	stmfd	sp!, {lr}
	mov	r1, r0
	CALL_LOCKSTAT_WRAPPER_ARG(LS_RW_EXIT_RELEASE, RW_WRITER)
	ldmfd	sp!, {pc}
#ifdef	DEBUG
.Lrw_exit_panic:
	mov	r2, r1
	mov	r1, r0
	adr	r0, .Lrw_exit_panic_msg
	bl	panic
	/* NOTREACHED */
.Lrw_exit_panic_msg:
	.asciz	"rw_exit: waiter exists: lp = 0x%p, value = 0x%x"
#endif	/* DEBUG */
	SET_SIZE(rw_exit)

#define	INST_RETURN	0xe1a0f00e		/* mov	pc, lr */
#define	INST_NOP	0xe1a00000		/* mov	r0, r0 */

#define	HOT_PATCH(label, event)						\
	ldr	r0, .Lprobemap;		/* r0 = &lockstat_probemap[0] */ \
	mov	r1, #(event * DTRACE_IDSIZE);				\
	ldr	r0, [r0, r1];		/* r0 = lockstat_probemap[event] */ \
	teq	r0, #0;							\
	ldreq	r1, .Linst_return;	/* r1 = INST_RETURN if (!r0) */	\
	ldrne	r1, .Linst_nop;		/* r1 = INST_NOP if (r0) */	\
	ldr	r0, label;		/* r0 = Address to be patched */ \
	mov	r2, #4;			/* r2 = 4 (instruction size) */	\
	bl	hot_patch_kernel_text	/* call hot_patch_kernel_text */

.Linst_return:
	.word	INST_RETURN
.Linst_nop:
	.word	INST_NOP

.Llock_try_lockstat_patch_point:
	.word	.lock_try_lockstat_patch_point
.Llock_clear_lockstat_patch_point:
	.word	.lock_clear_lockstat_patch_point
.Llock_set_lockstat_patch_point:
	.word	.lock_set_lockstat_patch_point
.Llock_set_spl_lockstat_patch_point:
	.word	.lock_set_spl_lockstat_patch_point
.Llock_clear_splx_lockstat_patch_point:
	.word	.lock_clear_splx_lockstat_patch_point
.Lmutex_enter_lockstat_patch_point:
	.word	.mutex_enter_lockstat_patch_point
.Lmutex_tryenter_lockstat_patch_point:
	.word	.mutex_tryenter_lockstat_patch_point
.Lmutex_exit_lockstat_patch_point:
	.word	.mutex_exit_lockstat_patch_point
.Lrw_read_enter_lockstat_patch_point:
	.word	.rw_read_enter_lockstat_patch_point
.Lrw_write_enter_lockstat_patch_point:
	.word	.rw_write_enter_lockstat_patch_point
.Lrw_read_exit_lockstat_patch_point:
	.word	.rw_read_exit_lockstat_patch_point
.Lrw_write_exit_lockstat_patch_point:
	.word	.rw_write_exit_lockstat_patch_point

/*
 * void
 * lockstat_hot_patch(void)
 *	Apply hot patch for lockstat.
 *
 * Remarks:
 *	This function write nop at patch points if lockstat is activated.
 *	Normal instruction on address to be patched must be "mov pc, lr".
 */
	.globl	hot_patch_kernel_text
ENTRY(lockstat_hot_patch)
	stmfd	sp!, {lr}
#if	STACK_ENTRY_ALIGN == 8
	sub	sp, sp, #4
#endif	/* STACK_ENTRY_ALIGN == 8 */
	HOT_PATCH(.Llock_try_lockstat_patch_point,
		  LS_LOCK_TRY_ACQUIRE)
	HOT_PATCH(.Llock_clear_lockstat_patch_point,
		  LS_LOCK_CLEAR_RELEASE)
	HOT_PATCH(.Llock_set_lockstat_patch_point,
		  LS_LOCK_SET_ACQUIRE)
	HOT_PATCH(.Llock_set_spl_lockstat_patch_point,
		  LS_LOCK_SET_SPL_ACQUIRE)
	HOT_PATCH(.Llock_clear_splx_lockstat_patch_point,
		  LS_LOCK_CLEAR_SPLX_RELEASE)
	HOT_PATCH(.Lmutex_enter_lockstat_patch_point,
		  LS_MUTEX_ENTER_ACQUIRE)
	HOT_PATCH(.Lmutex_tryenter_lockstat_patch_point,
		  LS_MUTEX_ENTER_ACQUIRE)
	HOT_PATCH(.Lmutex_exit_lockstat_patch_point,
		  LS_MUTEX_EXIT_RELEASE)
	HOT_PATCH(.Lrw_read_enter_lockstat_patch_point,
		  LS_RW_ENTER_ACQUIRE)
	HOT_PATCH(.Lrw_write_enter_lockstat_patch_point,
		  LS_RW_ENTER_ACQUIRE)
	HOT_PATCH(.Lrw_read_exit_lockstat_patch_point,
		  LS_RW_EXIT_RELEASE)
	HOT_PATCH(.Lrw_write_exit_lockstat_patch_point,
		  LS_RW_EXIT_RELEASE)
#if	STACK_ENTRY_ALIGN == 8
	add	sp, sp, #4
#endif	/* STACK_ENTRY_ALIGN == 8 */
	ldmfd	sp!, {pc}
	SET_SIZE(lockstat_hot_patch)

/*
 * Wrapper for lockstat probe routine.
 * r0 = (lockstat event * DTRACE_IDSIZE), r1 = lock_t *lockp,
 * r2 = curthread()
 */
	.globl	lockstat_probemap
.Lprobemap:
	.word	lockstat_probemap

	.globl	lockstat_probe
.Lprobe:
	.word	lockstat_probe

ENTRY(lockstat_wrapper)
	stmfd	sp!, {r4-r5, lr}

	/* curthread->t_lockstat++ */
	ldrb	r3, [r2, #T_LOCKSTAT]
	add	r3, r3, #1
	strb	r3, [r2, #T_LOCKSTAT]
	mov	r3, #0
	MEMORY_BARRIER(r3)		/* Memory barrier */

	mov	r4, r2			/* Preserve curthread() in r4. */
	ldr	r5, .Lprobemap
	ldr	r0, [r5, r0]		/* r0 = lockstat_probemap[event] */
	teq	r0, #0
	beq	10f

	/* If probe ID is not zero, call probe function */
	ARM_EABI_STACK_ALIGN(r5)
	adr	lr, 5f
	ldr	r3, .Lprobe
	ldr	pc, [r3]		/* lockstat_probe(id, lockp) */
5:
	ARM_EABI_STACK_RESTORE(, r5)

10:
	/* curthread->t_lockstat-- */
	ldrb	r3, [r4, #T_LOCKSTAT]
	sub	r3, r3, #1
	strb	r3, [r4, #T_LOCKSTAT]
	mov	r3, #0
	MEMORY_BARRIER(r3)		/* Memory barrier */

	ldmfd	sp!, {r4-r5, pc}
	SET_SIZE(lockstat_wrapper)

/*
 * Wrapper for lockstat probe routine.
 * r0 = (lockstat event * DTRACE_IDSIZE), r1 = lock_t *lockp,
 * r2 = arg0 for probe routine, r3 = curthread()
 */
ENTRY(lockstat_wrapper_arg)
	stmfd	sp!, {r4-r6, lr}

	/* curthread->t_lockstat++ */
	ldrb	r6, [r3, #T_LOCKSTAT]
	add	r6, r6, #1
	strb	r6, [r3, #T_LOCKSTAT]
	mov	r6, #0
	MEMORY_BARRIER(r6)		/* Memory barrier */

	mov	r4, r3			/* Preserve curthread() in r4. */
	ldr	r5, .Lprobemap
	ldr	r0, [r5, r0]		/* r0 = lockstat_probemap[event] */
	teq	r0, #0
	beq	10f

	/* If probe ID is not zero, call probe function */
	ARM_EABI_STACK_ALIGN(r6)
	adr	lr, 5f
	ldr	r5, .Lprobe
	ldr	pc, [r5]		/* lockstat_probe(id, lockp, arg0) */
5:
	ARM_EABI_STACK_RESTORE(, r6)
10:
	/* curthread->t_lockstat-- */
	ldrb	r3, [r4, #T_LOCKSTAT]
	sub	r3, r3, #1
	strb	r3, [r4, #T_LOCKSTAT]
	mov	r3, #0
	MEMORY_BARRIER(r3)		/* Memory barrier */

	ldmfd	sp!, {r4-r6, pc}
	SET_SIZE(lockstat_wrapper_arg)

/*
 * Memory barrier operations
 */
ENTRY(membar_enter)
ALTENTRY(membar_exit)
ALTENTRY(membar_producer)
ALTENTRY(membar_consumer)
	mov	r0, #0
	MEMORY_BARRIER(r0)
	mov	pc, lr
	SET_SIZE(membar_consumer)
	SET_SIZE(membar_producer)
	SET_SIZE(membar_exit)
	SET_SIZE(membar_enter)

ENTRY(membar_sync)
	mov	r0, #0
	SYNC_BARRIER(r0)
	mov	pc, lr
	SET_SIZE(membar_sync)

/*
 * void
 * thread_onproc(kthread_id_t t, cpu_t *cp)
 *	Set thread in onproc state for the specified CPU.
 *	Also set the thread lock pointer to the CPU's onproc lock.
 *	Since the new lock isn't held, the store ordering is important.
 *	If not done in assembler, the compiler could reorder the stores.
 */
ENTRY(thread_onproc)
	mov	r2, #TS_ONPROC
	str	r2, [r0, #T_STATE]	/* t->t_state = TS_ONPROC */
	mov	r2, #0
	MEMORY_BARRIER(r2)		/* Memory barrier */

	LOADCPU_THREAD_LOCK(r3, r1)

	/* t->t_lockp = &cpu->cpu_thread_lock */
	str	r3, [r0, #T_LOCKP]
	mov	pc, lr
	SET_SIZE(thread_onproc)

#define	MUTEX_DELAY_SPINCOUNT	64

/*
 * void
 * mutex_delay_default(void)
 *	Spins for approx a few hundred processor cycles and returns to caller.
 */
ENTRY(mutex_delay_default)
	mov	r0, #MUTEX_DELAY_SPINCOUNT
1:
	subs	r0, #1
	bne	1b
	mov	pc, lr
	SET_SIZE(mutex_delay_default)
