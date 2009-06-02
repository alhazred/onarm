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
 * Copyright (c) 2007 NEC Corporation
 * All rights reserved.
 */

/*
 * Definitions for cross compile.
 */

#ifndef	_XSAVECORE_H_
#define	_XSAVECORE_H_

#include "xstruct.h"

#ifdef	XSAVECORE

#define	PROGNAME	"xsavecore"

/*
 * Dump translation map hash function.
 */
#define	DUMP_HASH(dhp, as, va)	\
	((((XENV(uintptr_t))(as) >> 3) + ((va) >> (dhp)->dump_pageshift)) & \
	 (dhp)->dump_hashmask)

#include "xconst.h"

#else	/* !XSAVECORE */

#define	PROGNAME	"savecore"

#endif	/* XSAVECORE */

#endif	/* !_XSAVECORE_H_ */