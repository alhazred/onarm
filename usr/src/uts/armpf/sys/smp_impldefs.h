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

#ifndef _SYS_SMP_IMPLDEFS_H
#define	_SYS_SMP_IMPLDEFS_H

#pragma ident	"@(#)smp_impldefs.h	1.25	06/03/20 SMI"

#include <sys/types.h>
#include <sys/sunddi.h>
#include <sys/cpuvar.h>
#include <sys/avintr.h>
#include <sys/pic.h>
#include <sys/xc_levels.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *	External Reference Functions
 */
extern int (*slvltovect)(int);	/* ipl interrupt priority level		*/
extern int setlvl(int); 
extern void setlvlx(int);	/* mask intr below or equal given ipl	*/
extern int (*addspl)(int, int, int, int); /* add intr mask of vector 	*/
extern int (*delspl)(int, int, int, int); /* delete intr mask of vector */

/* trigger a software intr */
extern void (*setsoftint)(int, struct av_softinfo *);

/* kmdb private entry point */
extern void (*kdisetsoftint)(int, struct av_softinfo *);

extern uint_t xc_serv_hipri(caddr_t); /* x-call service routine for high pri */
extern uint_t xc_serv_lopri(caddr_t); /* x-call service routine for low pri */
extern void av_set_softint_pending();	/* set software interrupt pending */
extern void kdi_av_set_softint_pending(); /* kmdb private entry point */

extern void mach_init(void);
extern void mach_cpu_start(int);

/*
 *	External Reference Data
 */
extern struct av_head autovect[]; /* array of auto intr vectors		*/
extern uint32_t rm_platter_pa;	/* phy addr realmode startup storage	*/
extern caddr_t rm_platter_va;	/* virt addr realmode startup storage	*/
extern cpuset_t mp_cpus;	/* bit map of possible cpus found	*/

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SMP_IMPLDEFS_H */
