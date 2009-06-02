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
 * Copyright (c) 2006 NEC Corporation
 */

	.ident	"@(#)arm/krtld/kobj_crt.s"
	.file	"kobj_crt.s"

/*
 * exit routine from linker/loader to kernel
 */

#include <sys/asm_linkage.h>
#include <sys/reboot.h>
#include <sys/trap.h>

/*
 *  exitto is called from main() and does 1 things
 *	It then jumps directly to the just-loaded standalone.
 *	There is NO RETURN from exitto(). ????
 */

#if defined(lint)

/* ARGSUSED */
void
exitto(caddr_t entrypoint)
{}

#else	/* !lint */

#ifndef	STATIC_UNIX
	.globl	boothowto
	.globl	romp
	.globl	ops
/*
 * REVISIT:	Debugger is not supported.
 */
ENTRY_NP(exitto)
	mov	r3, r0
	adr	r0, romp
	ldr	r0, [r0, #0]
	adr	r1, ops
	ldr	r1, [r1, #0]

	adr	lr, ret
	mov	pc, r3
ret:
	/* NOTREACHED */
	mov	pc, lr
	SET_SIZE(exitto)
#endif	/* !STATIC_UNIX */
#endif	/* lint */
