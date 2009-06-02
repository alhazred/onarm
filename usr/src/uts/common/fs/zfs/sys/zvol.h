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
 * Copyright (c) 2007-2008 NEC Corporation
 */

#ifndef	_SYS_ZVOL_H
#define	_SYS_ZVOL_H

#pragma ident	"@(#)zvol.h	1.4	07/08/02 SMI"

#include <sys/zfs_context.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _KERNEL
extern int zvol_open(dev_t *devp, int flag, int otyp, cred_t *cr);
extern int zvol_close(dev_t dev, int flag, int otyp, cred_t *cr);
extern int zvol_strategy(buf_t *bp);
#ifndef ZFS_NO_ZVOL
extern int zvol_check_volsize(uint64_t volsize, uint64_t blocksize);
extern int zvol_check_volblocksize(uint64_t volblocksize);
extern int zvol_get_stats(objset_t *os, nvlist_t *nv);
extern void zvol_create_cb(objset_t *os, void *arg, cred_t *cr, dmu_tx_t *tx);
extern int zvol_create_minor(const char *, major_t);
extern int zvol_remove_minor(const char *);
extern int zvol_set_volsize(const char *, major_t, uint64_t);
extern int zvol_set_volblocksize(const char *, uint64_t);
extern int zvol_read(dev_t dev, uio_t *uiop, cred_t *cr);
extern int zvol_write(dev_t dev, uio_t *uiop, cred_t *cr);
extern int zvol_aread(dev_t dev, struct aio_req *aio, cred_t *cr);
extern int zvol_awrite(dev_t dev, struct aio_req *aio, cred_t *cr);
extern int zvol_ioctl(dev_t dev, int cmd, intptr_t arg, int flag, cred_t *cr,
    int *rvalp);
extern int zvol_busy(void);
extern void zvol_init(void);
extern void zvol_fini(void);
#else	/* ZFS_NO_ZVOL */
#define	zvol_check_volsize(volsize, blocksize)		EINVAL
#define	zvol_check_volblocksize(volblocksize)		EDOM
#define	zvol_get_stats(os, nv)				0
#define	zvol_create_cb					NULL
#define	zvol_create_minor(name, major)			ENXIO
#define	zvol_remove_minor(name)				ENXIO
#define	zvol_set_volsize(name, maj, volsize)		EINVAL
#define	zvol_set_volblocksize(name, volblocksize)	EINVAL
#define	zvol_read					nodev
#define	zvol_write					nodev
#define	zvol_ioctl(dev, cmd, arg, flag, cr, rvalp)	ENXIO
#define	zvol_busy()					0
#define	zvol_init()
#define	zvol_fini()
#endif	/* ZFS_NO_ZVOL */
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ZVOL_H */
