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
 * Copyright (c) 2006-2008 NEC Corporation
 */
	
/*
 * This crt1.o module is provided as the bare minimum required to build
 * a 32-bit executable with gcc.  It is installed in /usr/lib
 * where it will be picked up by gcc, along with crti.o and crtn.o
 */

	.ident	"@(#)crt1.s	1.2	05/06/08 SMI"

#include <sys/asm_linkage.h>

#if defined(PIC)
#define _fref_(name)	name(PLT)
#else
#define _fref_(name)	name
#endif
	.ident	"@(#)crt1.s"

	.file	"crt1.s"

/* global entities defined elsewhere but used here */
	.globl	main
#if !defined(PIC)
	.globl	libc_init
	.globl	__tls_static_mods
#endif
	.globl	exit
	.globl	_exit
#if !defined(__ARM_EABI__)
	.globl	_init
	.globl	_fini
#endif /* !defined(__ARM_EABI__) */
#if defined(PIC)
	.weak	_DYNAMIC
#endif
	.align	2

	ENTRY(_start)
#if !defined(PIC)
	mov	r0, #0			/* set NULL */
	mov	r1, #0			/* set zero size */
	bl	__tls_static_mods
#endif
	ldr	r0, [sp, #0]		/* get argc */
	add	r3, r0, #2		/* r3: # of entries */
	add	r2, sp, r3, LSL #2	/* r2: sp + (r3 * 4) */
	ldr	r3, =_environ
	str	r2, [r3, #0]		/* store &envp[0] to _environ */
#if !defined(PIC)
	bl	libc_init
#endif
#if defined(PIC)
	ldr	r0, _val_dynamic	/* #_DYNAMIC */
	cmp	r0, #0
	beq	1f
	mov	r0, r4			/* r4 is passed from ld.so.1. */
	bl	_fref_(atexit)
1:
#endif
#if !defined(__ARM_EABI__)
	ldr	r0, =_fini
	bl	_fref_(atexit)
	bl	_fref_(_init)
#endif /* !defined(__ARM_EABI__) */

	ldr	r0, [sp, #0]		/* r0: argc */
	add	r1, sp, #4		/* r1: &argv[0] */
	ldr	r3, =_environ
	ldr	r2, [r3, #0]		/* r2: environ */
	
	sub	sp, sp, #(8*CLONGSIZE)	/* BIAS for syscall and systemcall() */
					/* See syscall.s in libc */
					/* EABI 8byte alignment */

	bl	main			/* main(argc, argv, envp) */
	bl	_fref_(exit)		/* r0: return value of main */

	mov	r0, #0
	bl	_fref_(_exit)		/* force to terminate this process */

	add	sp, sp, #(8*CLONGSIZE)	/* EABI 8byte alignment */
	/* NOTREACHED */

.Lloop:	nop
	b	.Lloop

#if defined(PIC)
_val_dynamic:
	.word	_DYNAMIC
#endif
	SET_SIZE(_start)

	.data
	.global	_environ
	.align	4
	.type	_environ, %object
	.size	_environ, 4
_environ:
	.word	4

	.weak	environ
	.set	environ, _environ
