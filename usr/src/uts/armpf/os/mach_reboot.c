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
 * Copyright (c) 2007-2009 NEC Corporation
 * All rights reserved.
 */

#ident	"@(#)armpf/os/mach_reboot.c"

/*
 * ARM Platform specific reboot code.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sunddi.h>
#include <sys/systm.h>
#include <sys/archsystm.h>
#include <sys/mman.h>
#include <sys/pte.h>
#include <sys/mach_reboot.h>
#include <sys/mpcore.h>
#include <sys/platform.h>
#include <sys/cpuvar.h>
#include <sys/smp_impldefs.h>
#include <asm/cpufunc.h>
#include <asm/tlb.h>
#include <vm/vm_dep.h>
#include <vm/hat_arm.h>
#include <vm/hat_armpt.h>
#include <vm/hat_machdep.h>
#include <sys/devhalt.h>
#include <sys/mp_cpu.h>


/*
 * void
 * rbt_enter_reboot(rbt_caller_t caller)
 *	Core code for ARM platform specific reboot feature.
 *	rbt_enter_reboot() must be called on each CPU on system reboot.
 *
 *	The caller must specify where it is called from.
 *
 * Remarks
 *	This function never returns.
 */
void
rbt_enter_reboot(rbt_caller_t caller)
{
	while (1);
	/*NOTREACHED*/
}

