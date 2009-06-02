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

#ifndef	_SYS_MP_CPU_H
#define	_SYS_MP_CPU_H

#ident	"@(#)armpf/sys/mp_cpu.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/cpuvar.h>
#include <sys/bootconf.h>
#include <sys/archsystm.h>
#include <sys/prom_debug.h>
#include <asm/tlb.h>

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

/*
 * Definitions for struct cpu.
 * Kernel build tree private.
 */

extern caddr_t		cpu_buffer;

/*
 * Required address aligmnent for struct cpu.
 */
#define	MP_CPU_ALIGN		sizeof(uint64_t)

/*
 * Size to be allocated per struct cpu.
 */
#define	MP_CPU_MEMSIZE		P2ROUNDUP(sizeof(cpu_t), MP_CPU_ALIGN)

/*
 * MP_CPU_BOOT_ALLOC(cp, ncpus)
 *	Allocate struct cpu for boot processor.
 *
 *	This macro allocates struct cpu for all processors.
 *	It may help to reduce external cache slashing by struct cpu access.
 */
#define	MP_CPU_BOOT_ALLOC(cp, ncpus)					\
	do {								\
		/* Allocate buffer for struct cpu. */			\
		BOOT_ALLOC((cp), cpu_t *, MP_CPU_MEMSIZE * (ncpus),	\
			   MP_CPU_ALIGN,				\
			   "Failed to allocate struct cpu");		\
		FAST_BZERO_ALIGNED((cp), sizeof(cpu_t));		\
									\
		/* Set cpu buffer for other CPUs. */			\
		cpu_buffer = (caddr_t)(cp) + MP_CPU_MEMSIZE;		\
	} while (0)

/*
 * MP_CPU_ALLOC(cp)
 *	Allocate struct cpu for processors other than boot processor.
 *
 * Remarks:
 *	This macro must be called from boot processor.
 */
#define	MP_CPU_ALLOC(cp)						\
	do {								\
		/*							\
		 * No atomic operation is needed because we must be	\
		 * on boot processor.					\
		 */							\
		(cp) = (cpu_t *)cpu_buffer;				\
		cpu_buffer = (caddr_t)(cp) + MP_CPU_MEMSIZE;		\
		FAST_BZERO_ALIGNED((cp), sizeof(cpu_t));		\
	} while (0)

/*
 * MP_CPU_BOOTSTRAP(cp)
 *	Initialize struct cpu for processors other than boot processor.
 */
#define	MP_CPU_BOOTSTRAP(cp)						\
	do {								\
		/* Flush entire TLB to purge VA == PA mappings. */	\
		SYNC_BARRIER();						\
		TLB_FLUSH();						\
		PRM_PRINTF("CPU[%d] booting...\n", HARD_PROCESSOR_ID()); \
	} while (0)

#define	MP_CPU_MPSTART_INIT()
#define	MP_CPU_MPSTART_FINI()
#define	MP_CPU_REBOOT_FIXUP()

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* !_SYS_MP_CPU_H */
