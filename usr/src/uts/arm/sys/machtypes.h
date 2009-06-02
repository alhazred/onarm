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
 * Copyright (c) 2006 NEC Corporation
 * All rights reserved.
 */

#ifndef	_SYS_MACHTYPES_H
#define	_SYS_MACHTYPES_H

/* ARM dependent types. */

#include <sys/feature_tests.h>

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

#define	_ARM_SETJMP_SAVEREV_START	4
#define	_ARM_SETJMP_SAVEREV_END		14
#define	_ARM_SETJMP_SIZE						\
	(_ARM_SETJMP_SAVEREV_END - _ARM_SETJMP_SAVEREV_START + 1)

typedef struct _label_t	{ long val[_ARM_SETJMP_SIZE]; } label_t;

typedef unsigned char	lock_t;		/* lock work for busy wait */

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* _SYS_MACHTYPES_H */
