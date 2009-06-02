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
 * Copyright (c) 2001 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Copyright (c) 2006-2008 NEC Corporation
 */

/*
 * These crt*.o modules are provided as the bare minimum required
 * from a crt*.o for inclusion in building low level system
 * libraries.  The are only be to included in libraries which
 * contain *no* C++ code and want to avoid the startup code
 * that the C++ runtime has introduced into the crt*.o modules.
 *
 * For further details - see bug#4433015
 */

	.ident	"@(#)crti.s	1.2	05/06/08 SMI"
	.file	"crti.s"

#if defined(__ARM_EABI__)
/*
 * _init function
 */
	.globl	_init
	.type	_init,%function
	.align	2
_init:	
	bx	lr

/*
 * _fini function
 */
	.globl	_fini
	.type	_fini,%function
	.align	2
_fini:
	bx	lr

#else /* defined(__ARM_EABI__) */
/*
 * _init function prologue
 */
	.globl	_init
	.type	_init,%function
	.align	2
_init:	
	stmfd	sp!, {r4, lr}
	ldr	r4, =_ctors_i	/* loading the start of .ctros section */
				/* load register is loop controler */
				/* so, register must caller saved */
	add	r4, r4, #4	/* the first entry is dmy . so skiped */
.LOOPC:
	ldr	r0, [r4]	/* loading the address of function */
	cmp	r0, #0		/* checking the last entry. See crtn.s */
	beq	.LENDC
	mov	lr, pc		/* return address set */
	bx	r0		/* calling the function in .ctors */
	add	r4, r4, #4
	b	.LOOPC
.LENDC:
	ldmfd	sp!, {r4, pc}

/*
 * _fini function prologue
 */
	.globl	_fini
	.type	_fini,%function
	.align	2
_fini:
	stmfd	sp!, {r4, lr}
	ldr	r4, =_fini_i	/* loading the start of .dtros section */
				/* load register is loop controler */
				/* so, register must caller saved */
	add	r4, r4, #4	/* the first entry is dmy . so skiped */
.LOOPD:
	ldr	r0, [r4]	/* loading the address of function */
	cmp	r0, #0		/* checking the last entry. See crtn.s */
	beq	.LENDD
	mov	lr, pc		/* return address set */
	bx	r0		/* calling the function in .dtors */
	add	r4, r4, #4
	b	.LOOPD
.LENDD: 
	ldmfd	sp!, {r4, pc}

	.section	.ctors
_ctors_i:
	.word	0x00000000	/* .ctors segment start. */
				/* but this address not accessed */

	.section	.dtors
_fini_i:
	.word	0x00000000	/* .dtors segment start. */
				/* but this address not accessed */
#endif /* defined(__ARM_EABI__) */
