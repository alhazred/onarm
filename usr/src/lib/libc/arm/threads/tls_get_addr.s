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

#pragma ident	"@(#)tls_get_addr.s	1.5	05/06/08 SMI"

	.file	"tls_get_addr.s"

/*
 * To make thread-local storage accesses as fast as possible, we
 * hand-craft the __tls_get_addr() function below, from this C code:
 * void *
 * __tls_get_addr(TLS_index *tls_index)
 * {
 *	ulwp_t *self = curthread;
 *	tls_t *tlsent = self->ul_tlsent;
 *	ulong_t moduleid;
 *	caddr_t	base;
 *
 *	if ((moduleid = tls_index->ti_moduleid) < self->ul_ntlsent &&
 *	    (base = tlsent[moduleid].tls_data) != NULL)
 *		return (base + tls_index->ti_tlsoffset);
 *
 *	return (slow_tls_get_addr(tls_index));
 * }
 *
 * ___tls_get_addr() is identical to __tls_get_addr() except that it
 * assumes its argument is GOT offset from lr.
 */

#include "SYS.h"
#include "assym.h"
#include <asm/cpufunc.h>

#if SIZEOF_TLS_T == 8
#define	SHIFT	3
#else
#error "Assumption violated: SIZEOF_TLS_T is not 2 * sizeof (uintptr_t)"
#endif

ENTRY_NP(___tls_get_addr)
	add	r0, r0, lr
ALTENTRY(__tls_get_addr)
	READ_CP15(0, c13, c0, 2, ip)
	ldr	r1, [ip, #UL_TLSENT]
	ldr	r2, [r0, #TI_MODULEID]
	ldr	r3, [ip, #UL_NTLSENT]

	/*
	 * Runtime linker set (module ID + 1) into ti_moduleid to satisfy
	 * ARM EABI specification. So we must adjust module ID here.
	 */
	sub	r2, r2, #1

	cmp	r2, r3
	bcs	1f
	add	r1, r1, r2, LSL #SHIFT
	ldr	r1, [r1, #TLS_DATA]
	cmp	r1, #0
	beq	1f
	ldr	r3, [r0, #TI_TLSOFFSET]
	add	r0, r1, r3
	bx	lr
1:
	str	lr, [sp, #-8]!
	bl	_fref_(slow_tls_get_addr)
	ldr	lr, [sp], #8
	bx	lr
	SET_SIZE(___tls_get_addr)
	SET_SIZE(__tls_get_addr)

#define	ROUNDUP64(x)	(-(-(x) & -64))
#define	ULWP_SIZE	ROUNDUP64(SIZEOF_ULWP_T)

/*
 * void *
 * __aeabi_read_tp(void)
 *	Return static TLS base address for the current thread.
 *
 *	ARM EABI requires that __aeabi_read_tp() never corrupts registers
 *	except for r0, ip, lr, CPSR. So __aeabi_read_tp() must be written in
 *	assembly language.
 */
ENTRY_NP(__aeabi_read_tp)
	READ_CP15(0, c13, c0, 2, r0)
	add	r0, r0, #ULWP_SIZE
	bx	lr
	SET_SIZE(__aeabi_read_tp)
