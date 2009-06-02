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

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*		All Rights Reserved				*/

/*
 * Copyright (c) 2006-2007 NEC Corporation
 */

#ifndef _SYS_FP_H
#define	_SYS_FP_H

#pragma ident	"@(#)fp.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * VFP floating point processor definitions
 */

/*
 * values that go into fp_kind
 */
#define	FP_NO	0	/* no fp chip, no emulator (no fp support)	*/
#define	FP_SW	1	/* no fp chip, using software emulator		*/
#define	FP_VFP	2	/* VFP is present */

/*
 * FPSID masks
 */
#define	FPSID_IMPL	0xff000000	/* implementer */
#define	FPSID_SW	0x00800000	/* hardware/software */
#define	FPSID_FMT	0x00600000	/* FSTMX/FLDMX format */
#define	FPSID_SNG	0x00100000	/* precisions supported */
#define	FPSID_ARCH	0x000f0000	/* architecture version */
#define	FPSID_PART	0x0000ff00	/* part number */
#define	FPSID_VAR	0x000000f0	/* variant */
#define	FPSID_REV	0x0000000f	/* revision */

/*
 * FPSCR masks
 */
#define	FPSCR_N		0x80000000	/* N bit */
#define	FPSCR_Z		0x40000000	/* Z bit */
#define	FPSCR_C		0x20000000	/* C bit */
#define	FPSCR_V		0x10000000	/* V bit */
#define	FPSCR_DN	0x02000000	/* default NaN mode enable */
#define	FPSCR_FZ	0x01000000	/* flush-to-zero mode enable */
#define	FPSCR_RMODE	0x00c00000	/* rounding mode control field */
#define	FPSCR_STRIDE	0x00300000	/* vector stride control */
#define	FPSCR_LEN	0x00070000	/* vector length control */
#define	FPSCR_IDE	0x00008000	/* input subnormal exception enable */
#define	FPSCR_IXE	0x00001000	/* inexact exception enable */
#define	FPSCR_UFE	0x00000800	/* underflow exception enable */
#define	FPSCR_OFE	0x00000400	/* overflow exception enable */
#define	FPSCR_DZE	0x00000200	/* division by zero exception enable */
#define	FPSCR_IOE	0x00000100	/* invalid operation exception enable */
#define	FPSCR_IDC	0x00000080	/* input subnormal cumulative flag */
#define	FPSCR_IXC	0x00000010	/* inexact cumulative flag */
#define	FPSCR_UFC	0x00000008	/* underflow cumulative flag */
#define	FPSCR_OFC	0x00000004	/* overflow cumulative flag */
#define	FPSCR_DZC	0x00000002	/* division by zero cumulative flag */
#define	FPSCR_IOC	0x00000001	/* invalid operation cumulative flag */

/* rounding mode value */
#define	FPSCR_RMODE_RN	0x00000000	/* round to nearest mode */
#define	FPSCR_RMODE_RP	0x00400000	/* round towards plus infinity mode */
#define	FPSCR_RMODE_RM	0x00800000	/* round towards minus infinity mode */
#define	FPSCR_RMODE_RZ	0x00c00000	/* round towards zero mode */

/* initial FPSCR (RunFast mode) */
#define	FPSCR_INIT	(FPSCR_DN|FPSCR_FZ|FPSCR_RMODE_RN)

/*
 * FPEXC masks
 */
#define	FPEXC_EX	0x80000000	/* exception flag */
#define	FPEXC_EN	0x40000000	/* VFP enable bit */
#define	FPEXC_FP2V	0x10000000	/* FPINST2 instruction valid flag */
#define	FPEXC_VECITR	0x00000700	/* vector iteration count field */
#define	FPEXC_INV	0x00000080	/* input exception flag */
#define	FPEXC_UFC	0x00000008	/* potential underflow flag */
#define	FPEXC_OFC	0x00000004	/* potential overflow flag */
#define	FPEXC_IOC	0x00000001	/* potential invalid operation flag */

extern int fp_kind;		/* kind of fp support			*/
extern int fpu_exists;		/* FPU hw exists			*/

#ifdef _KERNEL

extern void fpu_probe(void);

struct regs;
struct k_siginfo;
extern int fpu_exception(struct regs *, struct k_siginfo *);

#endif	/* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_FP_H */
