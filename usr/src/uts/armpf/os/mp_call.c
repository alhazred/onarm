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
 * Copyright (c) 1990-1993, 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Copyright (c) 2006-2009 NEC Corporation
 */

#pragma ident	"@(#)mp_call.c	1.8	05/06/08 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/thread.h>
#include <sys/cpuvar.h>
#include <sys/machsystm.h>
#include <sys/systm.h>
#include <sys/platform.h>
#include <sys/gic.h>

/*
 * Interrupt another CPU.
 * 	This is useful to make the other CPU go through a trap so that
 *	it recognizes an address space trap (AST) for preempting a thread.
 *
 *	It is possible to be preempted here and be resumed on the CPU
 *	being poked, so it isn't an error to poke the current CPU.
 *	We could check this and still get preempted after the check, so
 *	we don't bother.
 */
void
poke_cpu(int cpun)
{
	cpuset_t cpuset;

	CPUSET_ZERO(cpuset);
	CPUSET_ADD(cpuset, cpun);

	/*
	 * We don't need to receive an ACK from the CPU being poked,
	 * so just send out a directed interrupt.
	 */
	gic_send_ipi(cpuset, IRQ_IPI_CPUPOKE);
}

/*
 * void
 * siron_poke_cpu(cpuset_t poke)
 *	Generate software interrupt on the given cpus.
 */
void
siron_poke_cpu(cpuset_t poke)
{
	int	cpuid = CPU->cpu_id;

	/*
	 * If we are poking to ourself then we can simply
	 * generate level1 using siron()
	 */
	if (CPU_IN_SET(poke, cpuid)) {
		siron();
		CPUSET_DEL(poke, cpuid);
		if (CPUSET_ISNULL(poke)) {
			return;
		}
	}

	gic_send_ipi(poke, IRQ_IPI_CPUPOKE);
}
