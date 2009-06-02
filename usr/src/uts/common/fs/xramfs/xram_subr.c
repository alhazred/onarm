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
 * Copyright 1989-1999,2001-2003 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2008 NEC Corporation
 * All rights reserved.
 */

#include <sys/param.h> /* sys/cmn_err.h requires this */
#include <sys/cmn_err.h> /* printf (for debugging) */
#include <sys/policy.h> /* secpolicy_vnode_access */
#include <sys/fs/xnode.h> /* struct xnode */
#include <fs/xramfs/xram.h> /* XRAMDBG */

#define	MODESHIFT	3

int
xram_xaccess(struct xmount *xm, struct xnode *xp, int mode, cred_t *cred)
{
	int shift = 0;

	/*
	 * check access based on owner, group and public permissions
	 *  in xrammnode
	 */
	if (crgetuid(cred) != xp->xn_uid) {
		shift += MODESHIFT;
		if (groupmember(xp->xn_gid, cred) == 0)
			shift += MODESHIFT;
	}

	XRAMDBG(("xram_xaccess: for xno %" PRIuMAX " (type/mode 0%06" PRIo16
		 ") reqmode 0%o (sft %d->0%" PRIoMAX ")\n",
		 (uintmax_t)XNODE_XNO(xm, xp), xp->xn_typemode,
		 mode, shift, (uintmax_t)((XNODE_MODE(xp) << shift) & mode)));
	/* compute missing mode bits */
	mode &= ~(XNODE_MODE(xp) << shift);

	if (mode == 0)
		return (0);

	return (secpolicy_vnode_access(cred, XNTOV(xp), xp->xn_uid, mode));
}
