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
 */

#pragma ident	"@(#)zfs_vfsops_arch.c"

#include <sys/types.h>
#include <sys/systm.h>

#include "zfs_namecheck.h"

/*
 * The boot path passed from the boot loader is in the form of
 * "rootpool-name/root-filesystem-name'.
 */
int
parse_bootpath(char *bpath, char *outpath)
{
	if (dataset_namecheck(bpath, NULL, NULL) != 0)
		return (EINVAL);

	(void) strcpy(outpath, bpath);

	return (0);
}
