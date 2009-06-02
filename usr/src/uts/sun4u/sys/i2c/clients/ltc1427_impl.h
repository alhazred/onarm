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
 * Copyright (c) 2000-2001 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_LTC1427_IMPL_H
#define	_LTC1427_IMPL_H

#pragma ident	"@(#)ltc1427_impl.h	1.3	05/06/08 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/i2c/clients/i2c_client.h>

struct ltc1427_unit {
	kmutex_t		ltc1427_mutex;
	int			ltc1427_oflag;
	i2c_client_hdl_t	ltc1427_hdl;
	char			ltc1427_name[24];
	int32_t			current_value;
	int8_t			current_set_flag;
};

#ifdef DEBUG

static int ltc1427debug = 0;
#define	D1CMN_ERR(ARGS) if (ltc1427debug & 0x1) cmn_err ARGS;
#define	D2CMN_ERR(ARGS) if (ltc1427debug & 0x2) cmn_err ARGS;

#else

#define	D1CMN_ERR(ARGS)
#define	D2CMN_ERR(ARGS)

#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _LTC1427_IMPL_H */
