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
 * Copyright (c) 2006 NEC Corporation
 */

#ifndef	_SYS_REG_H
#define	_SYS_REG_H

#pragma ident	"@(#)reg.h	1.20	05/06/08 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This file only exists for backwards compatibility.
 * Kernel code should not include it.
 */

#ifdef _KERNEL
#error "kernel include of reg.h"
#else
#include <sys/regset.h>
#endif	/* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_REG_H */