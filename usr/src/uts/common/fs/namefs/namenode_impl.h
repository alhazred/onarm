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

#ifndef	_FS_NAMENODE_IMPL_H
#define	_FS_NAMENODE_IMPL_H

#pragma	ident	"@(#)common/fs/namefs/namenode_impl.h"

#ifndef	_SYS_FS_NAMENODE_H
#error	Do NOT include namenode_impl.h directly.
#endif	/* _SYS_FS_NAMENODE_H */

#ifdef	NAMEFS_HASH_SIZE
#define	NM_FILEVP_HASH_SIZE	NAMEFS_HASH_SIZE
#else
#define	NM_FILEVP_HASH_SIZE	64
#endif	/* NAMEFS_HASH_SIZE */

#endif	/* _FS_NAMENODE_IMPL_H */
