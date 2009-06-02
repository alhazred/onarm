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

#ifndef	_SYS_MACHCPUVAR_H
#define	_SYS_MACHCPUVAR_H

#pragma ident	"@(#)machcpuvar.h	1.47	06/03/20 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/inttypes.h>
#include <sys/xc_levels.h>
#include <sys/avintr.h>
#include <sys/pte.h>

#ifndef	_ASM
/*
 * Machine specific fields of the cpu struct
 * defined in common/sys/cpuvar.h.
 *
 * Note:  This is kinda kludgy but seems to be the best
 * of our alternatives.
 */

struct	machcpu {
	/* define all the x_call stuff */
	volatile int	xc_pend[X_CALL_LEVELS];
	volatile int	xc_wait[X_CALL_LEVELS];
	volatile int	xc_ack[X_CALL_LEVELS];
	volatile int	xc_state[X_CALL_LEVELS];
	volatile int	xc_retval[X_CALL_LEVELS];

	int		mcpu_pri;		/* CPU priority */

	struct hat	*mcpu_current_hat;	/* cpu's current hat */
	uint32_t	mcpu_asid;		/* Current ASID */
	uint32_t	mcpu_asid_gen;		/* Generation ID for ASID */

	kmutex_t	mcpu_ppaddr_mutex;

	caddr_t		mcpu_caddr1;		/* per cpu CADDR1 */
	caddr_t		mcpu_caddr2;		/* per cpu CADDR2 */
	void		*mcpu_caddr1pte;
	void		*mcpu_caddr2pte;
	struct softint mcpu_softinfo;
	uint64_t	pil_high_start[HIGH_LEVELS];
	uint64_t	intrstat[PIL_MAX + 1][2];

	uint32_t	mcpu_idcode;
	uint32_t	mcpu_cachetype;
	uint32_t	mcpu_tlbtype;
	uint32_t	mcpu_cpuid;

	/*
	 * CPU local virtual space used to maintain instruction cache
	 * coherency.
	 */
	kmutex_t	mcpu_isync_mutex;
	caddr_t		mcpu_isync_addr;
	void		*mcpu_isync_pte;

	caddr_t		mcpu_fiq_modestack;	/* Top of FIQ mode stack */
	caddr_t		mcpu_fiq_stack;		/* Top of FIQ stack */
};

#ifndef NINTR_THREADS
#define	NINTR_THREADS	(LOCK_LEVEL)	/* number of interrupt threads */
#endif

#endif	/* _ASM */

#define	cpu_pri			cpu_m.mcpu_pri
#define	cpu_current_hat		cpu_m.mcpu_current_hat
#define	cpu_asid		cpu_m.mcpu_asid
#define	cpu_asid_gen		cpu_m.mcpu_asid_gen
#define	cpu_ppaddr_mutex	cpu_m.mcpu_ppaddr_mutex
#define	cpu_caddr1		cpu_m.mcpu_caddr1
#define	cpu_caddr2		cpu_m.mcpu_caddr2
#define	cpu_softinfo		cpu_m.mcpu_softinfo
#define	cpu_caddr1pte		cpu_m.mcpu_caddr1pte
#define	cpu_caddr2pte		cpu_m.mcpu_caddr2pte
#define	cpu_isync_mutex		cpu_m.mcpu_isync_mutex
#define	cpu_isync_addr		cpu_m.mcpu_isync_addr
#define	cpu_isync_pte		cpu_m.mcpu_isync_pte
#define	cpu_fiq_modestack	cpu_m.mcpu_fiq_modestack
#define	cpu_fiq_stack		cpu_m.mcpu_fiq_stack

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MACHCPUVAR_H */
