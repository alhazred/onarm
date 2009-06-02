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
 * All rights reserved.
 */

#pragma ident   "@(#)czfs:zfs_subr.c"

#ifdef _KERNEL
#include <sys/systm.h>
#else
#include <string.h>
#endif

#include <sys/byteorder.h>

int
zfs_guid_str_to_uint64(char *zfs_guid, uint64_t *guid1, uint64_t *guid2)
{
	char    *cp;

	if (strlen(zfs_guid) != 17)	/* 17 = strlen("xxxxxxxx:yyyyyyyy") */
		return (1);

	cp = strchr(zfs_guid, ':');
	if (cp == NULL || cp != zfs_guid + 8)	/* 8 = strlen("xxxxxxxx") */
		return (1);

	BE_OUT64(guid1, *(uint64_t *)zfs_guid);
	BE_OUT64(guid2, *(uint64_t *)(cp + 1));

	return (0);
}
