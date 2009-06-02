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
 * Copyright (c) 2006-2009 NEC Corporation
 * All rights reserved.
 */

#pragma ident	"@(#)sysarm.c"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/thread.h>
#include <sys/proc.h>
#include <sys/cachectl.h>
#include <sys/sysarm.h>
#include <sys/mman.h>

/*
 *  sysarm System Call
 * 
 *    The system call that can implement various ARM specific function.
 */

/* ARGSUSED */
int
sysarm(int cmd, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3)
{
	int error = 0;

	switch (cmd) {

	/*
	 * Invalidate all instruction cache line for user space.
	 */
	case SARM_IC_INVALL:
		error = sync_user_icache(curproc->p_as, NULL, NULL);
		break;

	/*
	 * Invalidate appointed address instruction cache line for user space.
	 */
	case SARM_IC_INVVADDR:

		/*
		 * arg1 : caddr_t addr
		 * arg2 : uint_t len
		 */

		/* Now, we ignore appointed address and size */
		error = sync_user_icache(curproc->p_as, (caddr_t)arg1, arg2);
		break;

	/*
	 * Flush all data cache lines on the all CPUs.
	 */
	case SARM_DC_FLUSHALL:
		error = sync_user_dcache(curproc->p_as, NULL, NULL);
		break;

	/*
	 * Flush appointed address instruction cache line for user space.
	 */
	case SARM_DC_FLUSHVADDR:

		/*
		 * arg1 : caddr_t addr
		 * arg2 : uint_t len
		 */

		/* Now, we ignore appointed address and size */
		error = sync_user_dcache(curproc->p_as, (caddr_t)arg1, arg2);
		break;

	/*
	 * Cycle Counter Register log routine.
	 */
	case SARM_CYCLECOUNT_LOG:
#ifdef USE_CYCLECOUNT
		error = cyclecount_syscall(arg1, arg2, arg3);
#else
		error = ENOTSUP;
#endif /* USE_CYCLECOUNT */
		break;

	case SARM_SYSINIT_DONE:
		break;

	default:
		error = EINVAL;
		break;
	}
	return (error == 0 ? 0 : set_errno(error));
}
