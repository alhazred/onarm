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

/*
 * Copyright (c) 2006-2007 NEC Corporation
 */

#ifndef	_SYS_ASM_LINKAGE_H
#define	_SYS_ASM_LINKAGE_H

#include <sys/stack.h>
#include <sys/trap.h>

#ifdef	__cplusplus
extern "C" {

#endif	/* __cplusplus */

#ifdef	_ASM

/*
 * These constants can be used to compute offsets into pointer arrays.
 */
#define	CLONGSHIFT	2
#define	CLONGSIZE	4
#define	CLONGMASK	3

#define	CLONGLONGSHIFT	3
#define	CLONGLONGSIZE	8
#define	CLONGLONGMASK	7

/*
 * ARM architecture is ILP32.
 */
#define	CPTRSHIFT	CLONGSHIFT
#define	CPTRSIZE	CLONGSIZE
#define	CPTRMASK	CLONGMASK

/*
 * profiling causes definitions of the MCOUNT and RTMCOUNT
 * particular to the type
 */
#ifdef	GPROF

#define	MCOUNT(x)					\
	stmfd	sp!, {r0-r3, lr};			\
	bl	_mcount;				\
	ldmfd	sp!, {r0-r3, lr};

#endif	/* GPROF */

#ifdef	PROF

#ifdef	__STDC__
#define	MCOUNT(x)					\
/* CSTYLED */						\
	.lcomm	.L_##x##1, 4;				\
	stmfd	sp!, {r0-r3, lr};			\
/* CSTYLED */						\
	adr	r0, .L_##x##1;				\
	bl	_mcount;				\
	ldmfd	sp!, {r0-r3, lr};
#else	/* !__STDC__ */
#define	MCOUNT(x)					\
/* CSTYLED */						\
	.lcomm	.L_/**/x/**/1, 4;			\
	stmfd	sp!, {r0-r3, lr};			\
/* CSTYLED */						\
	adr	r0, .L_/**/x/**/1;			\
	bl	_mcount;				\
	ldmfd	sp!, {r0-r3, lr};
#endif	/* __STDC__ */

#endif	/* PROF */

/*
 * if we are not profiling, MCOUNT should be defined to nothing
 */
#if	!defined(PROF) && !defined(GPROF)
#define	MCOUNT(x)
#endif	/* !defined(PROF) && !defined(GPROF) */

#define	RTMCOUNT(x)	MCOUNT(x)

/*
 * Macro to define weak symbol aliases. These are similar to the ANSI-C
 *	#pragma weak name = _name
 * except a compiler can determine type. The assembler must be told. Hence,
 * the second parameter must be the type of the symbol (i.e.: function,...)
 */
#ifdef	__STDC__
#define	ANSI_PRAGMA_WEAK(sym, stype)		\
	.weak	sym; 				\
	.type sym, %stype;			\
/* CSTYLED */					\
sym	= _##sym
#else	/* !__STDC__ */
#define	ANSI_PRAGMA_WEAK(sym, stype)		\
	.weak	sym; 				\
	.type sym, %stype;			\
/* CSTYLED */					\
sym	= _/**/sym
#endif	/* __STDC__ */

/*
 * Like ANSI_PRAGMA_WEAK(), but for unrelated names, as in:
 *	#pragma weak sym1 = sym2
 */
#define	ANSI_PRAGMA_WEAK2(sym1, sym2, stype)	\
	.weak	sym1;				\
	.type sym1, %stype;			\
sym1	= sym2

/*
 * ENTRY provides the standard procedure entry code and an easy way to
 * insert the calls to mcount for profiling. ENTRY_NP is identical, but
 * never calls mcount.
 */
#define	ENTRY(x)				\
	.text;					\
	.globl	x;				\
	.align	2;				\
	.type	x, %function;			\
x:	MCOUNT(x)

#define	ENTRY_NP(x)				\
	.text;					\
	.globl	x;				\
	.align	2;				\
	.type	x, %function;			\
x:

#define	RTENTRY(x)				\
	.text;					\
	.globl	x;				\
	.align	2;				\
	.type	x, %function;			\
x:	RTMCOUNT(x)

/*
 * ENTRY2 is identical to ENTRY but provides two labels for the entry point.
 */
#define	ENTRY2(x, y)				\
	.text;					\
	.globl	x, y;				\
	.align	2;				\
	.type	x, %function;			\
	.type	y, %function;			\
x:	;					\
y:	MCOUNT(x)

#define	ENTRY_NP2(x, y)				\
	.text;					\
	.globl	x, y;				\
	.align	2;				\
	.type	x, %function;			\
	.type	y, %function;			\
x:	;					\
y:


/*
 * ALTENTRY provides for additional entry points.
 */
#define	ALTENTRY(x)				\
	.globl x;				\
	.type	x, %function;			\
x:

/*
 * DGDEF and DGDEF2 provide global data declarations.
 *
 * DGDEF provides a word aligned word of storage.
 *
 * DGDEF2 allocates "sz" bytes of storage with **NO** alignment.  This
 * implies this macro is best used for byte arrays.
 *
 * DGDEF3 allocates "sz" bytes of storage with "algn" alignment.
 */
#define	DGDEF2(name, sz)			\
	.data;					\
	.globl	name;				\
	.type	name, %object;			\
	.size	name, sz;			\
name:

#define	DGDEF3(name, sz, algn)			\
	.data;					\
	.align	algn;				\
	.globl	name;				\
	.type	name, %object;			\
	.size	name, sz;			\
name:

#define	DGDEF(name)	DGDEF3(name, CLONGSIZE, CLONGSHIFT)

/*
 * SET_SIZE trails a function and set the size for the ELF symbol table.
 */
#define	SET_SIZE(x)				\
	.size	x, [.-x]

/* NWORD provides native word value. */
#define	NWORD	long

/*
 * Define macros to adjust stack alignment for EABI mode.
 * On EABI mode, stack pointer must be 8-bytes aligned at function entry.
 */
#if	STACK_ENTRY_ALIGN == 8

/*
 * Adjust stack alignment.
 * Original stack pointer will be saved into the specified register.
 * Register should be r4-r11, that will be saved by callee.
 */
#define	ARM_EABI_STACK_ALIGN(reg)			\
	mov	reg, sp;				\
	bic	sp, sp, #(STACK_ENTRY_ALIGN - 1)

/*
 * Restore stack pointer saved by ARM_EABI_STACK_ALIGN(reg).
 * "cond" must be condition mnemonic for ARM instructions.
 */
#ifdef	__STDC__
#define	ARM_EABI_STACK_RESTORE(cond, reg)		\
	mov##cond	sp, reg
#else	/* !__STDC__ */
#define	ARM_EABI_STACK_RESTORE(cond, reg)		\
	mov/**/cond	sp, reg
#endif	/* __STDC__ */

#else	/* STACK_ENTRY_ALIGN != 8 */

/*
 * No need to align stack because it will be already 4-bytes aligned.
 */
#define	ARM_EABI_STACK_ALIGN(reg)
#define	ARM_EABI_STACK_RESTORE(cond, reg)

#endif	/* STACK_ENTRY_ALIGN == 8 */

#endif	/* _ASM */

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* _SYS_ASM_LINKAGE_H */
