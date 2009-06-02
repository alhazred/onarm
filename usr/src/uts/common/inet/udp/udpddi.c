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

#pragma ident	"@(#)udpddi.c	1.46	07/09/25 SMI"

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/modctl.h>
#include <inet/common.h>
#include <inet/ip.h>
#include <inet/udp_impl.h>

#define	INET_NAME	"udp"
#define	INET_MODDESC	"UDP dummy STREAMS module 1.46"
#define	INET_DEVDESC	"UDP STREAMS driver 1.46"
#define	INET_DEVMINOR	0
#define	INET_MODSTRTAB	dummymodinfo
#define	INET_DEVSTRTAB	udpinfov4
#define	INET_MODMTFLAGS	D_MP
/*
 * We define both synchronous STREAMS and sockfs direct-access
 * mode for UDP module instance, because it is autopushed on
 * top of /dev/ip for the sockets case.
 */
#define	INET_DEVMTFLAGS (D_MP|D_SYNCSTR|_D_DIRECT)

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
