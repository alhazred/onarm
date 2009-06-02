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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2006-2008 NEC Corporation
 */

#ifndef	_SYS_PRIVREGS_H
#define	_SYS_PRIVREGS_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This file describes the cpu's privileged register set, and
 * how the machine state is saved on the stack when a trap occurs.
 */

#ifndef _ASM

/*
 * This is NOT the structure to use for general purpose debugging;
 * see /proc for that.  This is NOT the structure to use to decode
 * the ucontext or grovel about in a core file; see <sys/regset.h>.
 */

struct regs {
	greg_t	r_r0;
	greg_t	r_r1;
	greg_t	r_r2;
	greg_t	r_r3;
	greg_t	r_r4;
	greg_t	r_r5;
	greg_t	r_r6;
	greg_t	r_r7;
	greg_t	r_r8;
	greg_t	r_r9;
	greg_t	r_r10;
	greg_t	r_r11;
	greg_t	r_r12;
	greg_t	r_r13;
	greg_t	r_r14;
	greg_t	r_r13_svc;
	greg_t	r_r14_svc;
	greg_t	r_r15;
	greg_t	r_cpsr;
};

#define	r_sp	r_r13
#define	r_lr	r_r14
#define	r_pc	r_r15

#define	r_sp_svc	r_r13_svc
#define	r_lr_svc	r_r14_svc

#endif	/* !_ASM */

#ifdef _KERNEL
#define	lwptoregs(lwp)	((struct regs *)((lwp)->lwp_regs))
#endif /* _KERNEL */

#define	USERMODE(cpsr)	((cpsr & PSR_MODE) == PSR_MODE_USER)

#include <sys/controlregs.h>

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_PRIVREGS_H */
