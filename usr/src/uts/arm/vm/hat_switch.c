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
 * Copyright (c) 2007 NEC Corporation
 * All rights reserved.
 */

#ident	"@(#)arm/vm/hat_switch.c"

#include <asm/tlb.h>
#include <vm/hat.h>
#include <vm/hat_arm.h>

extern uint32_t hat_ttb_shared;

/* Internal prototypes */
static void	hat_l1pt_install(hat_t *hat);
static void	hat_asid_assign(hat_t *hat, cpu_t *cp);

/*
 * static inline void
 * hat_l1pt_install(hat_t *hat)
 *	Install L1PT into the TTB(0) register on the current CPU.
 *
 * Remarks:
 *	This function must be called with preemption disabled.
 */
static inline void
hat_l1pt_install(hat_t *hat)
{
	uint32_t	dacr, ttb;
	processorid_t	cpu;

	/* Drain write buffer. */
	SYNC_BARRIER();

	/* Set Context ID Register. */
	cpu = CPU->cpu_id;
	CONTEXT_ID_SET(hat->hat_context[cpu]);

	/* Flush branch target cache. */
	BTC_FLUSH_ALL();

	/* Setup TTB(0). */
	ttb = (uint32_t)hat->hat_l1paddr;
	ttb |= hat_ttb_shared;	/* Page table walk is to shared memory. */
	TTB_SET(0, ttb);	/* Set L1PT into TTB(0). */
}

/*
 * static void
 * hat_asid_assign(hat_t *hat, cpu_t *cp)
 *	Assign ASID for the specified HAT on the specified CPU.
 */
static void
hat_asid_assign(hat_t *hat, cpu_t *cp)
{
	processorid_t	cpuid = cp->cpu_id;
	uint32_t	cur_gen, gen, cur_asid;

	ASSERT(!HAT_IS_KERNEL(hat));
	ASSERT(mutex_owned(&hat->hat_switch_mutex));

	cur_gen = cp->cpu_asid_gen;
	cur_asid = cp->cpu_asid;
	gen = hat->hat_asid_gen[cpuid];

	if (gen == 0 || gen != cur_gen) {
		/*
		 * ASID is not yet assigned, or generation ID doesn't match
		 * to the current generation ID. We need to assign new ASID.
		 */
		cur_asid = ((cur_asid + 1) & CONTEXT_ID_ASID_MASK);
		if (cur_asid == 0) {
			/* ASID overflow. Bump up generation. */
			cur_gen++;

			if (cur_gen == 0) {
				hat_t	*h;

				/*
				 * Generation ID overflow.
				 * Although this is very rare case, we should
				 * invalidate ASIDs for all hat on this CPU.
				 */
				mutex_enter(&hat_list_lock);
				HAT_LIST_FOREACH(h, &hat_kas) {
					h->hat_asid_gen[cpuid] = 0;
				}
				mutex_exit(&hat_list_lock);
				cur_gen = 1;
			}

			/* Invalidate entire TLB on this CPU. */
			TLB_FLUSH();
			cp->cpu_asid_gen = cur_gen;
			cur_asid = 1;
		}
		cp->cpu_asid = cur_asid;
		hat->hat_context[cpuid] = cur_asid;
		hat->hat_asid_gen[cpuid] = cur_gen;
	}
}

/*
 * void
 * hat_switch(hat_t *hat)
 *	Switch to a new address space.
 *	If no ASID is assigned to user hat, or ASID should be reassigned
 *	because of ASID overflow, hat_switch() assigns new ASID.
 */
void
hat_switch(hat_t *hat)
{
	cpu_t		*cp = CPU;
	hat_t		*old = cp->cpu_current_hat;
	processorid_t	cpuid = cp->cpu_id;

	if (old == hat) {
		/* Same address space. */
		return;
	}

	if (!HAT_IS_KERNEL(hat)) {
		/*
		 * Acquire hat_switch_mutex which is a spin lock
		 * at DISP_LEVEL.
		 */
		HAT_SWITCH_LOCK(hat);

		/* Assign ASID for this address space. */
		hat_asid_assign(hat, cp);

		CPUSET_ATOMIC_ADD(hat->hat_cpus, cpuid);
		HAT_SWITCH_UNLOCK(hat);
	}
	cp->cpu_current_hat = hat;

	/* Install new L1PT into Translation Table Base Register. */
	hat_l1pt_install(hat);
}
