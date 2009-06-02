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
 * Copyright (c) 2007-2009 NEC Corporation
 * All rights reserved.
 */

#ifndef	_SYS_MACH_REBOOT_H
#define	_SYS_MACH_REBOOT_H

#ident	"@(#)armpf/sys/mach_reboot.h"

/*
 * Definitions for ARM platform specific reboot code.
 * Kernel build tree private.
 */

#include <sys/types.h>
#include <sys/ccompile.h>

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */


/*
 * Flags for rbt_enter_reboot() to denote where it is called from.
 */
typedef enum {
	RBT_CALLER_RESET = 1,		/* Called from reset() */
	RBT_CALLER_HALT = 2,		/* Called from mp_halt() */
	RBT_CALLER_PANIC = 3		/* Called from panic_idle() */
} rbt_caller_t;

/*
 * Prototypes
 */
extern void	rbt_enter_reboot(rbt_caller_t caller) __NORETURN;


#define	rbt_add_softintr()


#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* !_SYS_MACH_REBOOT_H */
