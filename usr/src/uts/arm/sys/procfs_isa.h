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
 * Copyright (c) 2006-2007 NEC Corporation
 */

#ifndef _SYS_PROCFS_ISA_H
#define	_SYS_PROCFS_ISA_H

#pragma ident	"@(#)procfs_isa.h	1.11	05/09/29 SMI"

/*
 * Instruction Set Architecture specific component of <sys/procfs.h>
 * arm version
 */

#include <sys/regset.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Possible values of pr_dmodel.
 * This isn't arm-specific, but it needs to be defined here for other reasons.
 */
#define	PR_MODEL_UNKNOWN 0
#define	PR_MODEL_ILP32	1	/* process data model is ILP32 */
#define	PR_MODEL_LP64	2	/* process data model is LP64 */

/*
 * To determine whether application is running native.
 */
#if defined(_LP64)
#define	PR_MODEL_NATIVE	PR_MODEL_LP64
#elif defined(_ILP32)
#define	PR_MODEL_NATIVE	PR_MODEL_ILP32
#else
#error "No DATAMODEL_NATIVE specified"
#endif	/* _LP64 || _ILP32 */

typedef	uint32_t	instr_t;
#define	NPRGREG		_NGREG
#define	prgreg_t	greg_t
#define	prgregset_t	gregset_t
#define	prfpregset	fpu
#define	prfpregset_t	fpregset_t

#if defined(_SYSCALL32)
/*
 * kernel view of the arm register set
 */
typedef	uint32_t	instr32_t;
#define	NPRGREG32	_NGREG
#define	prgreg32_t	greg_t
#define	prgregset32_t	gregset_t
#define	prfpregset32	fpu
#define	prfpregset32_t	fpregset_t
#endif	/* _SYSCALL32 */

/*
 * The following defines are for portability (see <sys/regset.h>).
 */
#define	R_PC	REG_PC
#define	R_PS	REG_CPSR
#define	R_SP	REG_SP
#define	R_R0	REG_R0
#define	R_R1	REG_R1

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PROCFS_ISA_H */
