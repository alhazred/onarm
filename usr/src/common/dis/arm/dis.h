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

/*
 * Copyright (c) 2007 NEC Corporation
 * All rights reserved.
 */

#ifndef	_COMMON_DIS_ARM_DIS_H
#define	_COMMON_DIS_ARM_DIS_H

#ident	"@(#)common/dis/arm/dis.h"

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

/*
 * Common constants and data types to analyze ARM instructions.
 */

#include <sys/types.h>
#include <sys/procfs_isa.h>

#define	INSTR_COND(instr)	(((instr) >> 28) & 0x0f)	/* 0xf0000000 */
#define	INSTR_TYPE(instr)	(((instr) >> 25) & 0x07)	/* 0x0e000000 */
#define	INSTR_OPCODE(instr)	(((instr) >> 21) & 0x0f)	/* 0x01e00000 */
#define	INSTR_RN(instr)		(((instr) >> 16) & 0x0f)	/* 0x000f0000 */
#define	INSTR_RD(instr)		(((instr) >> 12) & 0x0f)	/* 0x0000f000 */
#define	INSTR_REGION(instr)	INSTR_TYPE(instr)

/* Features for ARM processor */
#define	ARM_FEAT_V6	0x00000001		/* ARM V6 */
#define	ARM_FEAT_V6K	0x00000002		/* ARM V6K */
#define	ARM_FEAT_VFP2	0x00000004		/* VFP ver. 2 */

/* CPU type */
#define	ARM_CPU_MPCORE	(ARM_FEAT_V6|ARM_FEAT_V6K|ARM_FEAT_VFP2)

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* !_COMMON_DIS_ARM_DIS_H */
