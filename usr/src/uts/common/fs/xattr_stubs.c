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
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2008 NEC Corporation
 */

#pragma ident	"@(#)xattr_stubs.c"

#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/cred.h>
#include <sys/errno.h>

int
xattr_sysattr_casechk(char *s)
{
	return (0);
}

void
xattr_init(void)
{
}

/* ARGSUSED */
int
xattr_dir_vget(vfs_t *vfsp, vnode_t **vpp, fid_t *fidp)
{
	return (ENOTSUP);
}
