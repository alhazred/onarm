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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_LX_DEBUG_H
#define	_LX_DEBUG_H

#pragma ident	"@(#)lx_debug.h	1.1	06/09/11 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* initialize the debugging subsystem */
extern void lx_debug_init(void);

/* printf() style debug message functionality */
extern void lx_debug(const char *, ...);

/* set non-zero if the debugging subsystem is enabled */
extern int lx_debug_enabled;

#ifdef	__cplusplus
}
#endif

#endif	/* _LX_DEBUG_H */
