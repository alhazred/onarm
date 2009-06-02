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
 * Copyright (c) 2006-2008 NEC Corporation
 * All rights reserved.
 */

#ifndef	_SYS_BOOT_IMPL_H
#define	_SYS_BOOT_IMPL_H

#ident	"@(#)arm/sys/boot_impl.h"

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

/*
 * Definitions for boot arguments.
 * This is build environment private header.
 */

typedef struct bootarg {
	char	*name;
	char	*value;
} bootarg_t;

extern bootarg_t	default_bootargs[];

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* !_SYS_BOOT_IMPL_H */
