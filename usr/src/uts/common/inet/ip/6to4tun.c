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
 * Copyright 2002 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2006 NEC Corporation
 */

/* 6to4 tunnel module */

#pragma ident	"@(#)6to4tun.c	1.2	05/06/08 SMI"

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>

#include <sys/isa_defs.h>

#include <inet/common.h>

#include <inet/ip.h>
#include <inet/ip6.h>
#include <inet/tun.h>

#include <sys/modctl.h>

/* streams linkages */
static struct module_info tun6to4info = {
	TUN6TO4_MODID, TUN6TO4_NAME, 1, INFPSZ, 65536, 1024
};

static struct qinit tun6to4rinit = {
	(pfi_t)tun_rput,
	(pfi_t)tun_rsrv,
	tun_open,
	tun_close,
	NULL,
	&tun6to4info
};

static struct qinit tun6to4winit = {
	(pfi_t)tun_wput,
	(pfi_t)tun_wsrv,
	NULL,
	NULL,
	NULL,
	&tun6to4info
};

static struct streamtab tun6to4_strtab = {
	&tun6to4rinit, &tun6to4winit, NULL, NULL
};

static struct fmodsw tun6to4_fmodsw = {
	TUN6TO4_NAME,
	&tun6to4_strtab,
	(D_MP | D_MTQPAIR | D_MTPUTSHARED)
};

static struct modlstrmod modlstrmod = {
	&mod_strmodops, "6to4 tunneling module", &tun6to4_fmodsw
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modlstrmod,
	NULL
};

int
MODDRV_ENTRY_INIT()
{
	return (mod_install(&modlinkage));
}

#ifndef	STATIC_DRIVER
int
MODDRV_ENTRY_FINI()
{
	return (mod_remove(&modlinkage));
}
#endif	/* !STATIC_DRIVER */

int
MODDRV_ENTRY_INFO(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
