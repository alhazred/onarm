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

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include "lint.h"
#include "thr_uberdata.h"
#include <sched.h>

/*
 * Default attribute object for pthread_create() with NULL attr pointer.
 * Note that the 'guardsize' field is initialized on the first call.
 */
const thrattr_t *
def_thrattr(void)
{
	static thrattr_t thrattr = {
		0,				/* stksize */
		NULL,				/* stkaddr */
		PTHREAD_CREATE_JOINABLE,	/* detachstate */
		PTHREAD_CREATE_NONDAEMON_NP,	/* daemonstate */
		PTHREAD_SCOPE_PROCESS,		/* scope */
		0,				/* prio */
		SCHED_OTHER,			/* policy */
		PTHREAD_EXPLICIT_SCHED,		/* inherit */
		0				/* guardsize */
	};
	if (thrattr.guardsize == 0)
		thrattr.guardsize = _sysconf(_SC_PAGESIZE);
	return (&thrattr);
}

/*
 * pthread_attr_init: allocates the attribute object and initializes it
 * with the default values.
 */
#pragma weak pthread_attr_init = _pthread_attr_init
int
_pthread_attr_init(pthread_attr_t *attr)
{
	thrattr_t *ap;

	if ((ap = lmalloc(sizeof (thrattr_t))) != NULL) {
		*ap = *def_thrattr();
		attr->__pthread_attrp = ap;
		return (0);
	}
	return (ENOMEM);
}

/*
 * pthread_attr_destroy: frees the attribute object and invalidates it
 * with NULL value.
 */
#pragma weak pthread_attr_destroy = _pthread_attr_destroy
int
_pthread_attr_destroy(pthread_attr_t *attr)
{
	if (attr == NULL || attr->__pthread_attrp == NULL)
		return (EINVAL);
	lfree(attr->__pthread_attrp, sizeof (thrattr_t));
	attr->__pthread_attrp = NULL;
	return (0);
}

/*
 * _pthread_attr_clone: make a copy of a pthread_attr_t.
 */
int
_pthread_attr_clone(pthread_attr_t *attr, const pthread_attr_t *old_attr)
{
	thrattr_t *ap;
	const thrattr_t *old_ap =
		old_attr? old_attr->__pthread_attrp : def_thrattr();

	if (old_ap == NULL)
		return (EINVAL);
	if ((ap = lmalloc(sizeof (thrattr_t))) == NULL)
		return (ENOMEM);
	*ap = *old_ap;
	attr->__pthread_attrp = ap;
	return (0);
}

/*
 * _pthread_attr_equal: compare two pthread_attr_t's, return 1 if equal.
 * A NULL pthread_attr_t pointer implies default attributes.
 * This is a consolidation-private interface, for librt.
 */
int
_pthread_attr_equal(const pthread_attr_t *attr1, const pthread_attr_t *attr2)
{
	const thrattr_t *ap1 = attr1? attr1->__pthread_attrp : def_thrattr();
	const thrattr_t *ap2 = attr2? attr2->__pthread_attrp : def_thrattr();

	if (ap1 == NULL || ap2 == NULL)
		return (0);
	return (ap1 == ap2 || _memcmp(ap1, ap2, sizeof (thrattr_t)) == 0);
}

/*
 * pthread_attr_setstacksize: sets the user stack size, minimum should
 * be PTHREAD_STACK_MIN (MINSTACK).
 * This is equivalent to stksize argument in thr_create().
 */
#pragma weak pthread_attr_setstacksize = _pthread_attr_setstacksize
int
_pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL &&
	    stacksize >= MINSTACK) {
		ap->stksize = stacksize;
		return (0);
	}
	return (EINVAL);
}

/*
 * pthread_attr_getstacksize: gets the user stack size.
 */
#pragma weak pthread_attr_getstacksize = _pthread_attr_getstacksize
int
_pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL &&
	    stacksize != NULL) {
		*stacksize = ap->stksize;
		return (0);
	}
	return (EINVAL);
}

/*
 * pthread_attr_setstackaddr: sets the user stack addr.
 * This is equivalent to stkaddr argument in thr_create().
 */
#pragma weak pthread_attr_setstackaddr = _pthread_attr_setstackaddr
int
_pthread_attr_setstackaddr(pthread_attr_t *attr, void *stackaddr)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL) {
		ap->stkaddr = stackaddr;
		return (0);
	}
	return (EINVAL);
}

/*
 * pthread_attr_getstackaddr: gets the user stack addr.
 */
#pragma weak pthread_attr_getstackaddr = _pthread_attr_getstackaddr
int
_pthread_attr_getstackaddr(const pthread_attr_t *attr, void **stackaddr)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL &&
	    stackaddr != NULL) {
		*stackaddr = ap->stkaddr;
		return (0);
	}
	return (EINVAL);
}

/*
 * pthread_attr_setdetachstate: sets the detach state to DETACHED or JOINABLE.
 * PTHREAD_CREATE_DETACHED is equivalent to thr_create(THR_DETACHED).
 */
#pragma weak pthread_attr_setdetachstate = _pthread_attr_setdetachstate
int
_pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL &&
	    (detachstate == PTHREAD_CREATE_DETACHED ||
	    detachstate == PTHREAD_CREATE_JOINABLE)) {
		ap->detachstate = detachstate;
		return (0);
	}
	return (EINVAL);
}

/*
 * pthread_attr_getdetachstate: gets the detach state.
 */
#pragma weak pthread_attr_getdetachstate = _pthread_attr_getdetachstate
int
_pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL &&
	    detachstate != NULL) {
		*detachstate = ap->detachstate;
		return (0);
	}
	return (EINVAL);
}

/*
 * pthread_attr_setdaemonstate_np: sets the daemon state to DAEMON or NONDAEMON.
 * PTHREAD_CREATE_DAEMON is equivalent to thr_create(THR_DAEMON).
 * For now, this is a private interface in libc.
 */
int
_pthread_attr_setdaemonstate_np(pthread_attr_t *attr, int daemonstate)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL &&
	    (daemonstate == PTHREAD_CREATE_DAEMON_NP ||
	    daemonstate == PTHREAD_CREATE_NONDAEMON_NP)) {
		ap->daemonstate = daemonstate;
		return (0);
	}
	return (EINVAL);
}

/*
 * pthread_attr_getdaemonstate_np: gets the daemon state.
 * For now, this is a private interface in libc.
 */
int
_pthread_attr_getdaemonstate_np(const pthread_attr_t *attr, int *daemonstate)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL &&
	    daemonstate != NULL) {
		*daemonstate = ap->daemonstate;
		return (0);
	}
	return (EINVAL);
}

/*
 * pthread_attr_setscope: sets the scope to SYSTEM or PROCESS.
 * This is equivalent to setting THR_BOUND flag in thr_create().
 */
#pragma weak pthread_attr_setscope = _pthread_attr_setscope
int
_pthread_attr_setscope(pthread_attr_t *attr, int scope)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL &&
	    (scope == PTHREAD_SCOPE_SYSTEM ||
	    scope == PTHREAD_SCOPE_PROCESS)) {
		ap->scope = scope;
		return (0);
	}
	return (EINVAL);
}

/*
 * pthread_attr_getscope: gets the scheduling scope.
 */
#pragma weak pthread_attr_getscope = _pthread_attr_getscope
int
_pthread_attr_getscope(const pthread_attr_t *attr, int *scope)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL &&
	    scope != NULL) {
		*scope = ap->scope;
		return (0);
	}
	return (EINVAL);
}

/*
 * pthread_attr_setinheritsched: sets the scheduling parameters to be
 * EXPLICIT or INHERITED from parent thread.
 */
#pragma weak pthread_attr_setinheritsched = _pthread_attr_setinheritsched
int
_pthread_attr_setinheritsched(pthread_attr_t *attr, int inherit)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL &&
	    (inherit == PTHREAD_EXPLICIT_SCHED ||
	    inherit == PTHREAD_INHERIT_SCHED)) {
		ap->inherit = inherit;
		return (0);
	}
	return (EINVAL);
}

/*
 * pthread_attr_getinheritsched: gets the scheduling inheritance.
 */
#pragma weak pthread_attr_getinheritsched = _pthread_attr_getinheritsched
int
_pthread_attr_getinheritsched(const pthread_attr_t *attr, int *inherit)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL &&
	    inherit != NULL) {
		*inherit = ap->inherit;
		return (0);
	}
	return (EINVAL);
}

/*
 * pthread_attr_setschedpolicy: sets the scheduling policy to SCHED_RR,
 * SCHED_FIFO or SCHED_OTHER.
 */
#pragma weak pthread_attr_setschedpolicy = _pthread_attr_setschedpolicy
int
_pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL &&
	    (policy == SCHED_OTHER ||
	    policy == SCHED_FIFO ||
	    policy == SCHED_RR)) {
		ap->policy = policy;
		return (0);
	}
	return (EINVAL);
}

/*
 * pthread_attr_getpolicy: gets the scheduling policy.
 */
#pragma weak pthread_attr_getschedpolicy = _pthread_attr_getschedpolicy
int
_pthread_attr_getschedpolicy(const pthread_attr_t *attr, int *policy)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL &&
	    policy != NULL) {
		*policy = ap->policy;
		return (0);
	}
	return (EINVAL);
}

/*
 * pthread_attr_setschedparam: sets the scheduling parameters.
 * Currently, we support priority only.
 */
#pragma weak pthread_attr_setschedparam = _pthread_attr_setschedparam
int
_pthread_attr_setschedparam(pthread_attr_t *attr,
	const struct sched_param *param)
{
	thrattr_t *ap;
	int	policy;
	int	pri;

	if (attr == NULL || (ap = attr->__pthread_attrp) == NULL)
		return (EINVAL);

	policy = ap->policy;
	pri = param->sched_priority;
	if (policy == SCHED_OTHER) {
		if ((pri < THREAD_MIN_PRIORITY || pri > THREAD_MAX_PRIORITY) &&
		    _validate_rt_prio(policy, pri))
			return (EINVAL);
	} else if (_validate_rt_prio(policy, pri)) {
		return (EINVAL);
	}
	ap->prio = pri;
	return (0);
}

/*
 * pthread_attr_getschedparam: gets the scheduling parameters.
 * Currently, only priority is defined as sched parameter.
 */
#pragma weak pthread_attr_getschedparam = _pthread_attr_getschedparam
int
_pthread_attr_getschedparam(const pthread_attr_t *attr,
					struct sched_param *param)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL &&
	    param != NULL) {
		param->sched_priority = ap->prio;
		return (0);
	}
	return (EINVAL);
}

/*
 * UNIX98
 * pthread_attr_setguardsize: sets the guardsize
 */
#pragma weak pthread_attr_setguardsize = _pthread_attr_setguardsize
int
_pthread_attr_setguardsize(pthread_attr_t *attr, size_t guardsize)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL) {
		ap->guardsize = guardsize;
		return (0);
	}
	return (EINVAL);
}

/*
 * UNIX98
 * pthread_attr_getguardsize: gets the guardsize
 */
#pragma weak pthread_attr_getguardsize = _pthread_attr_getguardsize
int
_pthread_attr_getguardsize(const pthread_attr_t *attr, size_t *guardsize)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL &&
	    guardsize != NULL) {
		*guardsize = ap->guardsize;
		return (0);
	}
	return (EINVAL);
}

/*
 * pthread_attr_setstack: sets the user stack addr and stack size.
 * This is equivalent to the stack_base and stack_size arguments
 * to thr_create().
 */
#pragma weak pthread_attr_setstack = _pthread_attr_setstack
int
_pthread_attr_setstack(pthread_attr_t *attr,
	void *stackaddr, size_t stacksize)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL &&
	    stacksize >= MINSTACK) {
		ap->stkaddr = stackaddr;
		ap->stksize = stacksize;
		return (0);
	}
	return (EINVAL);
}

/*
 * pthread_attr_getstack: gets the user stack addr and stack size.
 */
#pragma weak pthread_attr_getstack = _pthread_attr_getstack
int
_pthread_attr_getstack(const pthread_attr_t *attr,
	void **stackaddr, size_t *stacksize)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL &&
	    stackaddr != NULL && stacksize != NULL) {
		*stackaddr = ap->stkaddr;
		*stacksize = ap->stksize;
		return (0);
	}
	return (EINVAL);
}
