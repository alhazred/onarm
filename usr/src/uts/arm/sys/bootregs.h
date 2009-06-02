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

#ifndef	_SYS_BOOTREGS_H
#define	_SYS_BOOTREGS_H

#pragma ident	"@(#)bootregs.h	1.2	05/06/08 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_KERNEL) || defined(_BOOT)

/*
 *  REVISIT
 *    "bop_regs" is used to
 *        1) struct bootops
 *        2) BOP_DOINT()
 *    Please pay attention.
 */
struct bop_regs {
	uint32_t r_r0;
	uint32_t r_r1;
	uint32_t r_r2;
	uint32_t r_r3;
	uint32_t r_r4;
	uint32_t r_r5;
	uint32_t r_r6;
	uint32_t r_r7;
	uint32_t r_r8;
	uint32_t r_r9;
	uint32_t r_r10;
	uint32_t r_r11;
	uint32_t r_r12;
	uint32_t r_r13;
	uint32_t r_r14;
	uint32_t r_r15;
	uint32_t r_cspr;
};

#endif	/* _KERNEL || _BOOT */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_BOOTREGS_H */
