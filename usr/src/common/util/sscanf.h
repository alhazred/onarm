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
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_COMMON_UTIL_SSCANF_H
#define	_COMMON_UTIL_SSCANF_H

#pragma ident	"@(#)sscanf.h	1.1	07/01/10 SMI"

#include <sys/types.h>
#include <sys/varargs.h>

#ifdef __cplusplus
extern "C" {
#endif

/*SCANFLIKE2*/
extern int sscanf(const char *, const char *, ...);
extern int vsscanf(const char *, const char *, va_list);

#ifdef __cplusplus
}
#endif

#endif	/* _COMMON_UTIL_SSCANF_H */
