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

/*
 * Copyright (c) 2008 NEC Corporation
 */

#pragma ident	"@(#)zfs_vfsops_arch.c"

#include <sys/types.h>
#include <sys/systm.h>
#include <zfs_types.h>

/*
 * Convert a decimal digit string to a objid_t(uint64_t or uint32_t) integer.
 */
static int
str_to_objid(char *str, objid_t *objnum)
{
	objid_t num = 0;

	while (*str) {
		if (*str < '0' || *str > '9')
			return (EINVAL);

		num = num*10 + *str++ - '0';
	}

	*objnum = num;
	return (0);
}

/*
 * The boot path passed from the boot loader is in the form of
 * "rootpool-name/root-filesystem-name'. Convert this
 * string to a dataset name: "rootpool-name/root-filesystem-name".
 */
int
parse_bootpath(char *bpath, char *outpath)
{
	char *slashp;
	objid_t objnum;
	int error;

	if (*bpath == 0 || *bpath == '/')
		return (EINVAL);

	slashp = strchr(bpath, '/');

	/* if no '/', just return the pool name */
	if (slashp == NULL) {
		(void) strcpy(outpath, bpath);
		return (0);
	}

	if (error = str_to_objid(slashp+1, &objnum))
		return (error);

	*slashp = '\0';
	error = dsl_dsobj_to_dsname(bpath, objnum, outpath);
	*slashp = '/';

	return (error);
}
