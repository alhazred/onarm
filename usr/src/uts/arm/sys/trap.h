/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
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
/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2006-2008 NEC Corporation
 */

#ifndef _SYS_TRAP_H
#define	_SYS_TRAP_H

#ident	"@(#)arm/sys/trap.h"

#include <sys/int_const.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Trap type values
 */
#define	T_RESET		0x0	/* reset				*/
#define	T_UNDEF		0x1	/* undefined instruction		*/
#define	T_PABT		0x2	/* prefetch abort			*/
#define	T_DABT		0x3	/* data abort				*/
#define	T_FIQ		0x4	/* Fast IRQ, aka FIQ			*/

/* T_SOFTINT value should be enable to set to ARM assembler immediate. */
#define	T_SOFTINT	0x3fc0	/*	pseudo softint trap type	*/

/*
 * Pseudo traps.
 */
#define	T_INTERRUPT		0x100
#define	T_FAULT			0x200
#define	T_AST			0x400
#define	T_SYSCALL		0x180


/*
 *  Values of error code on stack in case of page fault
 */

#define	PF_ERR_MASK	0x01	/* Mask for error bit */
#define	PF_ERR_PAGE	0x00	/* page not present */
#define	PF_ERR_PROT	0x01	/* protection error */
#define	PF_ERR_WRITE	0x02	/* fault caused by write (else read) */
#define	PF_ERR_USER	0x04	/* processor was in user mode */
				/*	(else supervisor) */
#define	PF_ERR_EXEC	0x10	/* attempt to execute a No eXec page (AMD) */

/*
 *  Definitions for fast system call subfunctions
 */
#define	T_FNULL		0	/* Null trap for testing		*/
#define	T_FGETFP	1	/* Get emulated FP context		*/
#define	T_FSETFP	2	/* Set emulated FP context		*/
#define	T_GETHRTIME	3	/* Get high resolution time		*/
#define	T_GETHRVTIME	4	/* Get high resolution virtual time	*/
#define	T_GETHRESTIME	5	/* Get high resolution time		*/
#define	T_GETLGRP	6	/* Get home lgrpid			*/

#define	T_LASTFAST	6	/* Last valid subfunction		*/

#define	T_SWI_MASK	0xff0000	/* SWI type mask */
#define	T_SWI_SYSCALL	0x000000	/* normal syscall */
#define	T_SWI_FASTCALL	0x010000	/* fast syscall */

/* Exception vector address */
#define	ARM_VECTORS_NORMAL	UINT32_C(0x00000000)
#define ARM_VECTORS_HIGH	UINT32_C(0xffff0000)

/*
 * BKPT ARM instruction
 *    1110 0001 0010 xxxx xxxx xxxx 0111 xxxx
 * immediate field
 *   bit [0:3] : Category
 *                -  reserved for debugger         0x00
 *                -  /proc                         0x01
 *                -  calculation                   0x02
 *   bit[19:8] : Detail
 *                - (/proc) single step            0x01
 *                - (calc) integer divide by zero  0x01
 *
 * When BKPT instruction occured,
 * We need to judge BKPT-instruction with immediate field
 *  in prefetch_abort_handler().
 *
 */
#define	ARM_BKPT_INSTR		0xe1200070

/* Category */
#define	ARM_BKPT_PROC		0x01
#define	ARM_BKPT_CALC		0x02
#define	ARM_BKPT_CTGRY_MASK	0x0f

/* Detail */
#define	ARM_BKPT_PROC_STEP	0x100
#define	ARM_BKPT_CALC_ZDIV	0x100
#define	ARM_BKPT_DETAIL_MASK	0xfff00

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_TRAP_H */
