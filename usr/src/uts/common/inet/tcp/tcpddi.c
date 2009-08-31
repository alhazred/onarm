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
/* Copyright (c) 1990 Mentat Inc. */

/*
 * Copyright (c) 2006-2008 NEC Corporation
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/modctl.h>
#include <inet/common.h>
#include <inet/ip.h>

#define	INET_NAME	"tcp"
#define	INET_MODSTRTAB	dummymodinfo
#define	INET_DEVSTRTAB	tcpinfov4
#define	INET_DEVDESC	"TCP STREAMS driver %I%"
#define	INET_MODDESC	"TCP dummy STREAMS module %I%"
#define	INET_DEVMINOR	0
#define	INET_MODMTFLAGS	D_MP
/*
 * Note that unlike UDP, TCP uses synchronous STREAMS only
 * for TCP Fusion (loopback); this is why we don't define
 * D_SYNCSTR here.
 */
#define	INET_DEVMTFLAGS	(D_MP|_D_DIRECT)

#include "../inetddi.c"

int
MODDRV_ENTRY_INIT(void)
{
	/*
	 * device initialization happens when the actual code containing
	 * module (/kernel/drv/ip) is loaded, and driven from ip_ddi_init()
	 */
	return (mod_install(&modlinkage));
}

#ifndef	STATIC_DRIVER
int
MODDRV_ENTRY_FINI(void)
{
	return (mod_remove(&modlinkage));
}
#endif	/* !STATIC_DRIVER */

int
MODDRV_ENTRY_INFO(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
