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

#ifndef _SYS_MACHSYSTM_H
#define	_SYS_MACHSYSTM_H

#pragma ident	"@(#)machsystm.h	1.28	06/02/17 SMI"

/*
 * Numerous platform-dependent interfaces that don't seem to belong
 * in any other header file.
 *
 * This file should not be included by code that purports to be
 * platform-independent.
 *
 */

#include <sys/machparam.h>
#include <sys/varargs.h>
#include <sys/thread.h>
#include <sys/cpuvar.h>
#include <sys/privregs.h>
#include <vm/page.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Trap information on PANIC */
struct panic_trap_info {
	struct regs	*trap_regs;	/* address of struct regs */
	uint_t		trap_type;	/* Trap type (See sys/trap.h) */
	caddr_t		trap_addr;	/* Faulted address */
	uint32_t	trap_fsr;	/* Fault status register */
};

/*
 * On ARM architecture, struct panic_trap_info is used on normal t
 */
typedef struct panic_trap_info	trap_info_t;

#ifdef _KERNEL

extern void mp_halt(char *);

extern void siron(void);

extern void return_instr(void);

extern int kcpc_hw_load_pcbe(void);
extern void kcpc_hw_init(void);
extern void kcpc_hw_startup_cpu(cpu_t *cp);

extern int cpuid2nodeid(int);
extern void map_kaddr(caddr_t, pfn_t, int, int);

extern void memscrub_init(void);
extern void memscrub_disable(void);

extern int use_mp;

extern struct cpu	cpus[];		/* pointer to other cpus */
extern struct cpu	*cpu[];		/* pointer to all cpus */

/*
 * Import build tree private definitions.
 */
#ifdef	_KERNEL_BUILD_TREE
#include <sys/machsystm_impl.h>
#endif	/* _KERNEL_BUILD_TREE */

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_MACHSYSTM_H */
