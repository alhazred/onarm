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

#ifndef	_SYS_DEVHALT_H
#define	_SYS_DEVHALT_H

#pragma ident	"@(#)devhalt.h"

#include <sys/t_lock.h>
#include <sys/thread.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef	void * devhalt_id_t;

#ifdef  _KERNEL
extern void	devhalt_init(void);
extern devhalt_id_t     devhalt_add(boolean_t (*)(void));
extern devhalt_id_t	devhalt_remove(devhalt_id_t dh);
extern void	devhalt_execute(void);
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DEVHALT_H */
