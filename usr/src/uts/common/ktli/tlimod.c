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
 * Copyright (c) 1990-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Copyright (c) 2006 NEC Corporation
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/types.h>
#include <sys/sunddi.h>
#include <sys/errno.h>
#include <sys/modctl.h>

static struct modlmisc modlmisc = {
	&mod_miscops, "KTLI misc module"
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};

int
MODDRV_ENTRY_INIT(void)
{
	return (mod_install(&modlinkage));
}

#ifndef	STATIC_DRIVER
int
MODDRV_ENTRY_FINI(void)
{
	return (EBUSY);
}
#endif	/* !STATIC_DRIVER */

int
MODDRV_ENTRY_INFO(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
