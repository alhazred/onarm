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
 * Copyright (c) 2008 NEC Corporation
 * All rights reserved.
 */

#ifndef _CZFS_ZAP_IMPL_H
#define	_CZFS_ZAP_IMPL_H

#pragma ident	"@(#)czfs:zap_impl.h"

#include <../zfs/sys/zap_impl.h>

#ifdef	__cplusplus
extern "C" {
#endif

void byteswap_none(void *, size_t);

#define	FZAP_ADD_SIZE				(mze->mze_size)
#define	ZAP_INTEGER_SIZE			(mze->mze_phys.mze_size)

#define	fzap_byteswap(buf, size)		byteswap_none(buf, size)
#define	zap_normalize(zap, name, namenorm)	ENOTSUP

#ifdef	__cplusplus
}
#endif

#endif	/* _CZFS_ZAP_IMPL_H */
