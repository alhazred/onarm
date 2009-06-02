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
 * Copyright (c) 2008 NEC Corporation
 * All rights reserved.
 */

#pragma ident	"@(#)arm/os/mode_setup.c"

#include <sys/types.h>
#include <sys/cpuvar.h>
#include <sys/stack.h>
#include <sys/trap.h>
#include <sys/kmem.h>
#include <sys/machparam.h>

/* FIQ mode stack for boot processor. */
#pragma	align	STACK_ENTRY_ALIGN(fiq_modestack0)
static char	fiq_modestack0[FIQ_MODE_STACKSZ];

/* FIQ stack for boot processor. */
#pragma	align	STACK_ENTRY_ALIGN(fiq_stack0)
static char	fiq_stack0[FIQ_STACKSZ];

extern void	arm_fiq_init(cpu_t *cp);

void
fiq_mlsetup(cpu_t *cp)
{
	/* Setup stack for FIQ mode. */
	cp->cpu_fiq_modestack = fiq_modestack0;
	cp->cpu_fiq_stack = fiq_stack0;

	/* Install FIQ mode stack. */
	arm_fiq_init(cp);
}

/*
 * Allocate stack for FIQ mode.
 */
void
fiq_mp_startup(struct cpu *cp, int cpun)
{
	caddr_t	fiqsp, fiqmsp;

	/*
	 * Allocate stack for FIQ mode.
	 * We can use kmem_alloc() to allocate stack because it returns
	 * at least double word aligned address.
	 */
	if ((fiqmsp = kmem_alloc(FIQ_MODE_STACKSZ, KM_NOSLEEP)) == NULL) {
		panic("mp_startup_init: cpu%d: "
		      "no memory for FIQ mode stack", cpun);
	}
	if ((fiqsp = kmem_alloc(FIQ_STACKSZ, KM_NOSLEEP)) == NULL) {
		panic("mp_startup_init: cpu%d: "
		      "no memory for FIQ stack", cpun);
	}

	cp->cpu_fiq_modestack = fiqmsp;
	cp->cpu_fiq_stack = fiqsp;
}

/*
 * Install FIQ mode stack.
 */
void
fiq_mp_init(cpu_t *cp)
{
	/* Install FIQ mode stack. */
	arm_fiq_init(cp);
}
