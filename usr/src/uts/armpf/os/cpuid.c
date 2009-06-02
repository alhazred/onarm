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

#ident	"@(#)armpf/os/cpuid.c"

/*
 * Various routines to handle identification
 * and classification of ARM processors.
 */

#include <sys/types.h>
#include <sys/archsystm.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/cpuvar.h>
#include <sys/processor.h>
#include <sys/fp.h>
#include <sys/controlregs.h>
#include <asm/cpufunc.h>

uint_t
cpuid_pass1(cpu_t *cpu)
{
	cpu->cpu_m.mcpu_idcode    = READ_CP15(0, c0, c0, 0);
	cpu->cpu_m.mcpu_cachetype = READ_CP15(0, c0, c0, 1);
	cpu->cpu_m.mcpu_tlbtype   = READ_CP15(0, c0, c0, 3);
	cpu->cpu_m.mcpu_cpuid     = READ_CP15(0, c0, c0, 5);

	cpu->cpu_asid = 0;
	cpu->cpu_asid_gen = 1;

	return 0;
}

/*
 * Make copies of the cpuid table entries we depend on, in
 * part for ease of parsing now, in part so that we have only
 * one place to correct any of it, in part for ease of
 * later export to userland, and in part so we can look at
 * this stuff in a crash dump.
 */
/*
 * By ARM platform initial porting, hold nothing.
 */
/*ARGSUSED*/
void
cpuid_pass2(cpu_t *cpu)
{
}

/*
 * This routine is called just after kernel memory allocation
 * becomes available on cpu0, and as part of mp_startup() on
 * the other cpus.
 *
 * Fixup the brand string.
 */
/*
 * By ARM platform initial porting, hold nothing.
 */
/*ARGSUSED*/
void
cpuid_pass3(cpu_t *cpu)
{
}

/*
 * This routine is called out of bind_hwcap() much later in the life
 * of the kernel (post_startup()).  The job of this routine is to resolve
 * the hardware feature support and kernel support for those features into
 * what we're actually going to tell applications via the aux vector.
 */
/*
 * By ARM platform initial porting, hold nothing.
 */
uint_t
cpuid_pass4(cpu_t *cpu)
{
	uint_t hwcap_flags = 0;
	return hwcap_flags;
}

/*
 * Return CPU brand string for cpu_t.
 * This routine supports MPCore only.
 */
int
cpuid_getbrandstr(cpu_t *cpu, char *s, size_t n)
{
	uint_t idcode = cpu->cpu_m.mcpu_idcode;
	uint_t rev, err = 0;

	switch(idcode | CP15_ID_REVISION) {
	case CP15_ID_ARM11MPCORE:
		rev = idcode & CP15_ID_REVISION;
		err = snprintf(s, n, "ARM11 MPCore Rev.%d", rev);
		break;
	default:
		strncpy(s, "ARM (unknown)", n);
		break;
	}
	return err;
}

/*
 * Return the implementor string.
 * This routine supports arm only.
 */
const char *
cpuid_getimplstr(int idcode)
{
	const char *implstr;

	switch (ARM_IDCODE_IMPL(idcode)) {
	case CP15_ID_IMPL_ARM:
		implstr = "arm";
		break;
	default:
		implstr = "unknown";
		break;
	}

	return implstr;
}

/*
 * Return CPU id string for cpu_t.
 */
int
cpuid_getidstr(cpu_t *cpu, char *s, size_t n)
{
	static const char fmt[] = "ARM (idcode 0x%x clock %d MHz)";

	return snprintf(s, n, fmt,
		cpu->cpu_m.mcpu_idcode, cpu->cpu_type_info.pi_clock);
}

/*
 * Return CPU id code for cpu_t.
 */
int
cpuid_getidcode(cpu_t *cpu)
{
	return (cpu->cpu_m.mcpu_idcode);
}

/*
 * create a node for the given cpu under the prom root node.
 * Also, create a cpu node in the device tree.
 */
static dev_info_t *cpu_nex_devi = NULL;
static kmutex_t cpu_node_lock;

/*
 * Called from post_startup() and mp_startup()
 */
void
add_cpunode2devtree(cpu_t *cpu)
{
	dev_info_t *cpu_devi;

	mutex_enter(&cpu_node_lock);

	/*
	 * create a nexus node for all cpus identified as 'cpu_id' under
	 * the root node.
	 */
	if (cpu_nex_devi == NULL) {
		if (ndi_devi_alloc(ddi_root_node(), "cpus",
		    (pnode_t)DEVI_SID_NODEID, &cpu_nex_devi) != NDI_SUCCESS) {
			mutex_exit(&cpu_node_lock);
			return;
		}
		(void) ndi_devi_online(cpu_nex_devi, 0);
	}

	/*
	 * create a child node for cpu identified as 'cpu_id'
	 */
	cpu_devi = ddi_add_child(cpu_nex_devi, "cpu", DEVI_SID_NODEID,
		cpu->cpu_id);
	if (cpu_devi == NULL) {
		mutex_exit(&cpu_node_lock);
		return;
	}

	/* device_type */

	(void) ndi_prop_update_string(DDI_DEV_T_NONE, cpu_devi,
	    "device_type", "cpu");

	/* reg */

	(void) ndi_prop_update_int(DDI_DEV_T_NONE, cpu_devi,
	    "reg", cpu->cpu_id);

	/* cpu-mhz, and clock-frequency */

	if (cpu_freq > 0) {
		long long mul;

		(void) ndi_prop_update_int(DDI_DEV_T_NONE, cpu_devi,
		    "cpu-mhz", cpu_freq);

		if ((mul = cpu_freq * 1000000LL) <= INT_MAX)
			(void) ndi_prop_update_int(DDI_DEV_T_NONE, cpu_devi,
			    "clock-frequency", (int)mul);
	}

	(void) ndi_devi_online(cpu_devi, 0);

	/* id-code */
	(void) ndi_prop_update_int(DDI_DEV_T_NONE, cpu_devi,
		"id-code", cpu->cpu_m.mcpu_idcode);

	/* cache-type */
	(void) ndi_prop_update_int(DDI_DEV_T_NONE, cpu_devi,
		"cache-type", cpu->cpu_m.mcpu_cachetype);

	/* tlb-type */
	(void) ndi_prop_update_int(DDI_DEV_T_NONE, cpu_devi,
		"tlb-type", cpu->cpu_m.mcpu_tlbtype);

	/* cpu-id */
	(void) ndi_prop_update_int(DDI_DEV_T_NONE, cpu_devi,
		"cpu-id", cpu->cpu_m.mcpu_cpuid);

	mutex_exit(&cpu_node_lock);
}
