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
 * Copyright (c) 2008-2009 NEC Corporation
 * All rights reserved.
 */

#ifndef	_SYS_PLATFORM_MACH_H
#define	_SYS_PLATFORM_MACH_H

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

#ident	"@(#)ne1/sys/platform_mach.h"

#include <sys/int_const.h>


/*
 * NaviEngine-1 specific definitions
 */
/* 256MB SDRAM */
#define	ARMMACH_SDRAM0_PADDR	UINT32_C(0x80000000)
#define	ARMMACH_SDRAM0_SIZE	UINT32_C(0x08000000)	/* 128MB */
#define	ARMMACH_SDRAM1_PADDR	UINT32_C(0x88000000)
#define	ARMMACH_SDRAM1_SIZE	UINT32_C(0x08000000)	/* 128MB */
/* Core FPGA */
#define	ARMMACH_COREFPGA_PADDR	UINT32_C(0x04010000)


#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* !_SYS_PLATFORM_MACH_H */
