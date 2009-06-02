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
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc. */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T		*/
/*	All Rights Reserved	*/

/*
 * Copyright (c) 2006 NEC Corporation
 */

#ifndef	_SYS_REGSET_H
#define	_SYS_REGSET_H

#include <sys/feature_tests.h>

#if !defined(_ASM)
#include <sys/types.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)

#define	REG_R0		0
#define	REG_R1		1
#define	REG_R2		2
#define	REG_R3		3
#define	REG_R4		4
#define	REG_R5		5
#define	REG_R6		6
#define	REG_R7		7
#define	REG_R8		8
#define	REG_R9		9
#define	REG_R10		10
#define	REG_R11		11
#define	REG_R12		12
#define	REG_R13		13
#define	REG_R14		14
#define	REG_R15		15
#define REG_CPSR	16
#define REG_CP15TID2	17

#define	REG_SP		REG_R13
#define	REG_LR		REG_R14
#define	REG_PC		REG_R15
#define	REG_TP		REG_CP15TID2

#endif	/* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

/*
 * A gregset_t is defined as an array type for compatibility with the reference
 * source. This is important due to differences in the way the C language
 * treats arrays and structures as parameters.
 */
#define	_NGREG	18
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#define	NGREG	_NGREG
#endif

#if !defined(_ASM)

#if defined(_LP64) || defined(_I32LPx)
typedef long	greg_t;
#else
typedef int	greg_t;
#endif

#if defined(_SYSCALL32)

typedef int32_t greg32_t;
typedef int64_t	greg64_t;

#endif	/* _SYSCALL32 */

typedef greg_t	gregset_t[_NGREG];

#if defined(_SYSCALL32)

#define	_NGREG32	_NGREG
#define	_NGREG64	_NGREG

typedef greg32_t gregset32_t[_NGREG32];
typedef	greg64_t gregset64_t[_NGREG64];

#endif	/* _SYSCALL32 */

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)

/*
 * Floating point definitions.
 */

typedef struct {
	uint32_t	fp_exponent;
	uint32_t	fp_mantissa_hi;
	uint32_t	fp_mantissa_lo;
} fpe_reg_t;

typedef struct fpu {
	union {
		struct {
			uint32_t	fp_fpsr;
			fpe_reg_t	fp_fr[8];
		} fp_fpe;
		struct {
			uint32_t	fp_fpscr;
			uint32_t	fp_regs[32];
		} fp_vfp;
	} fp_u;
} fpregset_t;

#if defined(_SYSCALL32)

typedef struct fpu	fpregset32_t;

#endif	/* _SYSCALL32 */

/*
 * Kernel's FPU save area
 */
typedef struct {
	uint32_t	fp_fpscr;
	uint32_t	fp_regs[32];
	uint32_t	fp_fpexc;
	uint32_t	fp_fpinst;
	uint32_t	fp_fpinst2;
} kfpu_t;

/*
 * Structure mcontext defines the complete hardware machine state.
 */
typedef struct {
	gregset_t	gregs;		/* general register set */
	fpregset_t	fpregs;		/* floating point register set */
} mcontext_t;

#if defined(_SYSCALL32)

typedef struct {
	gregset32_t	gregs;		/* general register set */
	fpregset32_t	fpregs;		/* floating point register set */
} mcontext32_t;

#endif	/* _SYSCALL32 */

#endif	/* _ASM */
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

/*
 * The version of privregs.h that is used on implementations that run
 * on processors that support the ARM instruction set is deliberately not
 * imported here.
 *
 * The ARM 'struct regs' definition is -not- compatible with either 32-bit
 * or 64-bit core file contents, nor with the ucontext.  As a result, the
 * 'regs' structure cannot be used portably by applications, and should
 * only be used by the kernel implementation.
 *
 * The inclusion of the ARM V6 version of privregs.h allows for some
 * limited source compatibility with 32-bit applications who expect to use
 * 'struct regs' to match the content of a 32-bit core file, or a ucontext_t.
 *
 * Note that the ucontext_t actually describes the general registers in
 * terms of the gregset_t data type, as described in this file.  Note also
 * that the core file content is defined by core(4) in terms of data types
 * defined by procfs -- see proc(4).
 */
#if	!defined(_KERNEL) && !defined(_XPG4_2) || defined(__EXTENSIONS__)
#include <sys/privregs.h>
#endif	/* !defined(_KERNEL) && !defined(_XPG4_2) || defined(__EXTENSIONS__) */

/*
 * The following is here for XPG4.2 standards compliance.
 * regset.h is included in ucontext.h for the definition of
 * mcontext_t, all of which breaks XPG4.2 namespace.
 */

#if defined(_XPG4_2) && !defined(__EXTENSIONS__) && !defined(_ASM)

/*
 * The following is here for UNIX 95 compliance (XPG Issue 4, Version 2
 * System Interfaces and Headers). The structures included here are identical
 * to those visible elsewhere in this header except that the structure
 * element names have been changed in accordance with the X/Open namespace
 * rules.  Specifically, depending on the name and scope, the names have
 * been prepended with a single or double underscore (_ or __).  See the
 * structure definitions in the non-X/Open namespace for more detailed
 * comments describing each of these structures.
 */

typedef struct __fpu {
	union {
		struct {
			uint32_t	__fp_fpsr;
			struct {
				uint32_t	__fp_exponent;
				uint32_t	__fp_mantissa_hi;
				uint32_t	__fp_mantissa_lo;
			} __fp_fr[8];
		} __fp_fpe;
		struct {
			uint32_t	__fp_fpscr;
			uint32_t	__fp_regs[32];
		} __fp_vfp;
	} __fp_u;
} fpregset_t;

typedef struct {
	gregset_t	__gregs;	/* general register set */
	fpregset_t	__fpregs;	/* floating point register set */
} mcontext_t;

#endif /* _XPG4_2 && !__EXTENSIONS__ && !_ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_REGSET_H */
