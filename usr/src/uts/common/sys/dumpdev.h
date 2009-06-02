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
 * Copyright (c) 2007 NEC Corporation
 * All rights reserved.
 */

#ifndef	_SYS_DUMPDEV_H
#define	_SYS_DUMPDEV_H

#ident	"@(#)common/sys/dumpdev.h"

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

#include <sys/types.h>

/*
 * This header file contains common definitions for dumpdev feature.
 */

struct dumpdev;
typedef struct dumpdev	dumpdev_t;

/* Methods for dumpdev operation */
typedef struct dumpdevops {
	int	(*dop_open)(dumpdev_t *ddp, offset_t *startoffp);
	void	(*dop_close)(dumpdev_t *ddp);
	int	(*dop_dump)(dumpdev_t *ddp, caddr_t buf, offset_t off,
			    size_t size);
	int	(*dop_size)(dumpdev_t *ddp, size_t *sizep);
} dumpdevops_t;

/* Dump device descriptor */
struct dumpdev {
	const char	*dd_name;	/* Device name */
	dumpdevops_t	*dd_ops;	/* Methods */
	void		*dd_data;	/* Private data */
};

/* Flags for dd_flags */
#define	DUMPDEV_STREAM		0x1	/* Transferred as byte stream */

/* Macros for dump device methods */
#define	DUMPDEV_OPEN(ddp, startoffp)			\
	((ddp)->dd_ops->dop_open((ddp), (startoffp)))
#define	DUMPDEV_CLOSE(ddp)			\
	((ddp)->dd_ops->dop_close(ddp))
#define	DUMPDEV_DUMP(ddp, buf, off, size)			\
	((ddp)->dd_ops->dop_dump((ddp), (buf), (off), (size)))
#define	DUMPDEV_SIZE(ddp, sizep)			\
	((ddp)->dd_ops->dop_size((ddp), (sizep)))

/* Prototypes */
extern int	dumpdev_install(dumpdev_t *ddp);
extern void	dumpdev_uninstall(dumpdev_t *ddp);

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* !_SYS_DUMPDEV_H */
