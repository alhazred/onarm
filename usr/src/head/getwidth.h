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
 * Copyright 1989 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

#ifndef	_GETWIDTH_H
#define	_GETWIDTH_H

#pragma ident	"@(#)getwidth.h	1.7	05/07/22 SMI"

#include <euc.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	__STDC__
extern void getwidth(eucwidth_t *);
#else	/* __STDC__ */
extern void getwidth();
#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _GETWIDTH_H */
