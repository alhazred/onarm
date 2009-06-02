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
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2007-2008 NEC Corporation
 */

	.ident	"@(#)boot.s	1.14	05/06/08 SMI"

/*
 * Bootstrap routine for run-time linker.
 * We get control from exec which has loaded our text and
 * data into the process' address space and created the process
 * stack.
 *
 * On entry, the process stack looks like this:
 *
 *	#			#
 *	#_______________________#  high addresses
 *	#	strings		#
 *	#_______________________#
 *	#	0 word		#
 *	#_______________________#
 *	#	Auxiliary	#
 *	#	entries		#
 *	#	...		#
 *	#	(size varies)	#
 *	#_______________________#
 *	#	0 word		#
 *	#_______________________#
 *	#	Environment	#
 *	#	pointers	#
 *	#	...		#
 *	#	(one word each)	#
 *	#_______________________#
 *	#	0 word		#
 *	#_______________________#
 *	#	Argument	# low addresses
 *	#	pointers	#
 *	#	Argc words	#
 *	#_______________________#
 *	#	argc		#
 *	#_______________________# <- sp
 *
 *
 * First, we construct bootstrap structure, i.e., copy tag-value
 * pairs of EB_ARGV, EB_ENVP, and EB_AUXV on the process stack.
 * Next, we find the address of the dynamic section of ld.so.
 * And then pass them to _setup().
 * Finally, jump to the entry point for the a.out.
 */

/*
 * ld.so.1 bootstrap routine for ARM architecture
 */

#if	defined(lint)

extern	unsigned long	_setup();
extern	void		atexit_fini();
void
main()
{
	(void)_setup();
	atexit_fini();
}

#else
	.file	"boot.s"

#include	<sys/asm_linkage.h>
#include	<link.h>

	ENTRY_NP(_rt_boot)		/* do not need to profile */

	mov 	fp, sp			/* make stack frame */
	sub	sp, sp, #EB_MAX_SIZE32	/* make room for a bootstrap vector */
	mov	r0, sp			/* use r0 as a pointer to &eb[0] */

	mov	r3, #EB_ARGV		/* set up EB_ARGV tag for argv */
	str	r3, [r0]
	add	r3, fp, #4		/* get address of argv */
	str	r3, [r0, #4]		/* and put it after EB_ARGV tag */

	mov	r3, #EB_ENVP		/* set up EB_ENVP tag for envp */
	str	r3, [r0, #8]
	ldr	r3, [fp]		/* get argc */
	add	r3, r3, #2		/* one for the zero and one for argc */
	add	r4, fp, r3, LSL #2  	/* get address of envp */
	str	r4, [r0, #12]		/* and put it after EB_ENVP tag */

	mov	r3, #EB_AUXV		/* set up EB_AUXV tag for auxv */
	str	r3, [r0, #16]
.Lloop:					/* skip envp elements to get auxv */
	ldr	r3, [r4], #4		/* address */
	cmp	r3, #0
	bne	.Lloop
	str	r4, [r0, #20]		/* now r4 points to address of auxv */
					/* and put it after EB_AUXV tag */

	mov	r3, #EB_NULL		/* set up EB_NULL tag for terminator */
	str	r3, [r0, #24]

/*
 * Now bootstrap structure has been constructed.
 * The process stack looks like this:
 *
 *	#	...		#
 *	#_______________________#
 *	#	Argument	# high addresses
 *	#	pointers	#
 *	#	Argc words	#
 *	#_______________________#
 *	#	argc		#
 *	#_______________________# <- fp (= sp on entry)
 *	#   reserved area of    #
 *	#  bootstrap structure  #
 *	#  (currently not used) #
 *	#	...		#
 *	#_______________________#
 *	#  garbage (not used)   #
 *	#_ _ _ _ _ _ _ _ _ _ _ _#
 *	#	EB_NULL		#
 *	#_______________________# <- sp + 24 (= &eb[3])
 *	#	&auxv[0]	#
 *	#_ _ _ _ _ _ _ _ _ _ _ _#
 *	#	EB_AUXV		#
 *	#_______________________# <- sp + 16 (= &eb[2])
 *	#	&envp[0]	#
 *	#_ _ _ _ _ _ _ _ _ _ _ _#
 *	#	EB_ENVP		#
 *	#_______________________# <- sp + 8  (= &eb[1])
 *	#	&argv[0]	#
 *	#_ _ _ _ _ _ _ _ _ _ _ _# low addresses
 *	#	EB_ARGV		#
 *	#_______________________# <- sp (= fp - EB_MAX_SIZE32) = r0 (= &eb[0])
 */

.Lget_GOT_addr:				/* Get runtime address of ld.so's */
	ldrd	r4, .LGOT		/* _GLOBAL_OFFSET_TABLE_ (GOT) and */
					/* offset of atexit_fini() in GOT. */
					/* r4 = GOT, r5 = atexit_fini() */
.LPIC:
	add	r4, pc, r4		/* r4 contains runtime addr of GOT. */

	ldr	r1, [r4, #0]		/* Get ld.so's dynamic table address */
					/* (i.e., _DYNAMIC) which is the */
					/* first entry of GOT. */

	bl	_setup			/* call _setup(&eb[0], _DYNAMIC) */
	 				/* _setup() returns the entry point */
					/* (_start() in crt1.s as usual) of */
					/* the executable at r0. */

	mov	sp, fp			/* release stack frame */

	ldr	r4, [r4, r5]		/* Set runtime addr of atexit_fini() */
					/* at r4. It is passed to the entry */
					/* point later. */

	bx	r0			/* Transfer control to executable. */
	 				/* r0 contains the address of the */
					/* entry point of the executable. */
					/* NOTE: */
					/*   Runtime address of atexit_fini() */
					/*   is passed to the entry point */
					/*   implicitly via r4. */

.LGOT:
	.word	_GLOBAL_OFFSET_TABLE_ - (.LPIC + 8)
					/* pc relative offset of GOT */
	.word	atexit_fini(GOT)	/* offset of atexit_fini() in GOT */

	SET_SIZE(_rt_boot)
#endif
