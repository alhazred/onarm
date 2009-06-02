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
 * Copyright (c) 2007-2008 NEC Corporation
 * All rights reserved.
 */

#ifndef	_COMMON_DIS_ARM_DIS_STRING_H
#define	_COMMON_DIS_ARM_DIS_STRING_H

#ident	"@(#)common/dis/arm/dis_string.h"

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

/*
 * Common constants and data types to stringfy ARM instructions.
 */

#include "dis.h"

/* Prototypes */
extern int	dis_arm_string(uint_t arch, uint32_t pc, uint32_t inst,
			       char *buf, size_t bufsize,  uint32_t *target,
			       uint_t flags);

/* Flags for dis_arm_string() */
#define	DIS_ARM_STR_OCTAL	0x1	/* dump number in octal */
#define	DIS_ARM_STR_VFP2	0x2	/* dump VFP v2 instruction */

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* !_COMMON_DIS_ARM_DIS_STRING_H */
