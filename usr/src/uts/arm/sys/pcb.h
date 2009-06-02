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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2006-2007 NEC Corporation
 */

#ifndef _SYS_PCB_H
#define	_SYS_PCB_H

#pragma ident	"@(#)pcb.h	1.23	06/02/03 SMI"

#include <sys/regset.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _ASM

typedef struct fpu_ctx {
	kfpu_t		fpu_regs;	/* kernel save area for FPU */
	uint_t		fpu_flags;	/* FPU state flags */
} fpu_ctx_t;

union save_instr {
	uint32_t	arm;		/* arm instruction 32bit */
	uint16_t	thumb;		/* Thumb instruction 16bit */
};

struct bp_info {
	caddr_t		addr;		/* break point address */
	union save_instr instr;		/* save instruction */
};

typedef struct pcb {
	fpu_ctx_t	pcb_fpu;	/* fpu state */
	uint_t		pcb_flags;	/* state flags; cleared on fork */
	greg_t		pcb_usertp;	/* user tid (CP15 c13 0 c0 2) */
	uint32_t	pcb_instr;	/* /proc: instruction at stop */
	int		pcb_step;	/* used while single-stepping */
	struct bp_info	pcb_bpinfo;	/* used while single-stepping */
} pcb_t;

#endif /* ! _ASM */

/* pcb_flags */
#define	INSTR_VALID	0x08	/* value in pcb_instr is valid (/proc) */
#define	NORMAL_STEP	0x10	/* normal debugger-requested single-step */
#define	WATCH_STEP	0x20	/* single-stepping in watchpoint emulation */
#define	CPC_OVERFLOW	0x40	/* performance counters overflowed */
#define	RUPDATE_PENDING	0x80	/* new register values in the pcb -> regs */

/* fpu_flags */
#define	FPU_EN		0x1	/* flag signifying fpu in use */
#define	FPU_VALID	0x2	/* fpu_regs has valid fpu state */

#define	FPU_INVALID	0x0	/* fpu context is not in use */

/* pcb_step */
#define	STEP_NONE	0	/* no single step */
#define	STEP_REQUESTED	1	/* arrange to single-step the lwp */
#define	STEP_ACTIVE	2	/* actively patching addr, set active flag */
#define	STEP_WASACTIVE	3	/* wrap up after taking single-step fault */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PCB_H */
