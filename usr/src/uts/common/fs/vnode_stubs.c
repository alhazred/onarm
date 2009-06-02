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

/*	Copyright (c) 1983, 1984, 1985, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*
 * University Copyright- Copyright (c) 1982, 1986, 1988
 * The Regents of the University of California
 * All Rights Reserved
 *
 * University Acknowledgment- Portions of this document are derived from
 * software developed by the University of California, Berkeley, and its
 * contributors.
 */

/*
 * Copyright (c) 2007-2008 NEC Corporation
 */

#pragma ident	"@(#)vnode_stubs.c"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/t_lock.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/uio.h>
#include <sys/file.h>
#include <sys/pathname.h>
#include <sys/vfs.h>
#include <sys/vfs_opreg.h>
#include <sys/vnode.h>
#include <sys/rwstlock.h>
#include <sys/fem.h>
#include <sys/stat.h>
#include <sys/mode.h>
#include <sys/conf.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <c2/audit.h>
#include <sys/acl.h>
#include <sys/nbmlock.h>
#include <sys/fcntl.h>
#include <fs/fs_subr.h>


#ifdef FEM_DISABLE

/* ARGSUSED */
void
vn_reclaim(vnode_t *vp)
{}

/* ARGSUSED */
void
vn_idle(vnode_t *vp)
{}

/* ARGSUSED */
void
vn_exists(vnode_t *vp)
{}

/* ARGSUSED */
void
vn_invalid(vnode_t *vp)
{}

/* ARGSUSED */
int
vnevent_support(vnode_t *vp, caller_context_t *ct)
{
	return (ENOTSUP);
}

/* ARGSUSED */
void
vnevent_rename_src(vnode_t *vp, vnode_t *dvp, char *name, caller_context_t *ct)
{}

/* ARGSUSED */
void
vnevent_rename_dest(vnode_t *vp, vnode_t *dvp, char *name,
    caller_context_t *ct)
{}

/* ARGSUSED */
void
vnevent_rename_dest_dir(vnode_t *vp, caller_context_t *ct)
{}

/* ARGSUSED */
void
vnevent_remove(vnode_t *vp, vnode_t *dvp, char *name, caller_context_t *ct)
{}

/* ARGSUSED */
void
vnevent_rmdir(vnode_t *vp, vnode_t *dvp, char *name, caller_context_t *ct)
{}

/* ARGSUSED */
void
vnevent_create(vnode_t *vp, caller_context_t *ct)
{}

/* ARGSUSED */
void
vnevent_link(vnode_t *vp, caller_context_t *ct)
{}

/* ARGSUSED */
void
vnevent_mountedover(vnode_t *vp, caller_context_t *ct)
{}

#endif	/* FEM_DISABLE */


#ifdef VSD_DISABLE

/* ARGSUSED */
void
vsd_create(uint_t *keyp, void (*destructor)(void *))
{}

/* ARGSUSED */
void
vsd_destroy(uint_t *keyp)
{}

/* ARGSUSED */
void *
vsd_get(vnode_t *vp, uint_t key)
{
	return (NULL);
}

/* ARGSUSED */
int
vsd_set(vnode_t *vp, uint_t key, void *value)
{
	return (ENOTSUP);
}

/* ARGSUSED */
void
vsd_free(vnode_t *vp)
{}

#endif	/* VSD_DISABLE */
