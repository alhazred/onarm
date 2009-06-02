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
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2006 NEC Corporation
 */

#pragma ident	"@(#)rpcsec_gssmod.c	1.14	05/06/08 SMI"

#include <sys/types.h>
#include <sys/modctl.h>
#include <sys/errno.h>

MODDRV_DEPENDS_ON("strmod/rpcmod misc/kgssapi");

/*
 * Module linkage information for the kernel.
 */
static struct modlmisc modlmisc = {
	&mod_miscops, "kernel RPCSEC_GSS security service."
};

static struct modlinkage modlinkage = {
	MODREV_1, &modlmisc, NULL
};

int
MODDRV_ENTRY_INIT()
{
	int retval = 0;
	extern void gssauth_init();
	extern void svc_gss_init();
	extern void gssauth_fini();
	extern void svc_gss_fini();

	gssauth_init();
	svc_gss_init();

	if ((retval = mod_install(&modlinkage)) != 0) {
		gssauth_fini();
		svc_gss_fini();
	}

	return (retval);
}

#ifndef	STATIC_DRIVER
int
MODDRV_ENTRY_FINI()
{
	return (EBUSY);
}
#endif	/* !STATIC_DRIVER */

int
MODDRV_ENTRY_INFO(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
