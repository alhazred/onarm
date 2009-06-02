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

#ifndef _SN1_BRAND_H
#define	_SN1_BRAND_H

#pragma ident	"@(#)sn1_brand.h	1.1	06/09/11 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	NSYSCALL 	256		/* number of system calls */

#ifndef	_ASM

extern void sn1_handler(void);

#endif	/* _ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SN1_BRAND_H */
