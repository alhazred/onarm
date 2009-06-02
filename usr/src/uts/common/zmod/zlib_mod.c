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
 * Copyright 2003 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2008 NEC Corporation
 */

/*
 * Kernel "misc" module that contains RFC1950 decompression routines.
 */

#include <sys/types.h>
#include <sys/modctl.h>
#include <sys/errno.h>

static struct modlmisc modlmisc = {
	&mod_miscops, "RFC 1950 decompression routines"
};

static struct modlinkage modlinkage = {
	MODREV_1, &modlmisc, NULL
};

int
MODDRV_ENTRY_INIT(void)
{
	return mod_install(&modlinkage);
}

int
MODDRV_ENTRY_INFO(struct modinfo *mip)
{
	return mod_info(&modlinkage, mip);
}

#ifndef STATIC_DRIVER
int
MODDRV_ENTRY_FINI(void)
{
	return mod_remove(&modlinkage);
}
#endif  /* !STATIC_DRIVER */
