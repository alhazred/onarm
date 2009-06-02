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

#ifndef	_SYS_NVDRAM_IMPL_H
#define	_SYS_NVDRAM_IMPL_H

#ident	"@(#)arm/sys/nvdram_impl.h"

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

/*
 * Common definitions for nvdram (Non-Volatile DRAM) device.
 * (kernel build environment private)
 *
 * Remarks:
 *	The build environment tool xramconf(1) may need to be rewritten
 *	if you change definitions in this file.
 */

#include <sys/types.h>
#include <sys/dditypes.h>
#include <sys/xramdev_impl.h>
#include <sys/fs/dv_node.h>

/*
 * Physical memory segment for nvdram device.
 */
typedef struct nvdseg {
	const xmemdev_t	ns_memory;	/* memory attributes */
	const mode_t	ns_mode;	/* file access mode */
	const uint_t	ns_flags;	/* flags */
	const uid_t	ns_uid;		/* user ID */
	const gid_t	ns_gid;		/* group ID */
	const char	*ns_name;	/* node name */
	const char	*ns_rpriv;	/* privilege for read */
	const char	*ns_wpriv;	/* privilege for write */
} nvdseg_t;

#define	ns_base		ns_memory.xd_base
#define	ns_count	ns_memory.xd_count
#define	ns_attr		ns_memory.xd_attr

/* Flags for xs_flags */
#define	NVDSEG_SYSDUMP		0x1	/* dump into system dump */
#define	NVDSEG_PRIVONLY		0x2	/* policy-based permission only */

/* Default file permission */
#define	NVDSEG_UID_DEFAULT	DV_UID_DEFAULT
#define	NVDSEG_GID_DEFAULT	DV_GID_DEFAULT
#define	NVDSEG_MODE_DEFAULT	DV_DEVMODE_DEFAULT

/* nvdram device handle */
typedef struct nvdram {
	kmutex_t	nd_lock;	/* mutex */
	dev_info_t	*nd_dip;	/* device info handle */
	uint_t		nd_flags;	/* flags */
	uint_t		nd_unit;	/* device unit number */
	const nvdseg_t	*nd_seg;	/* memory segment info */
	void		*nd_vaddr;	/* virtual address to map device */
} nvdram_t;

/* Flags for nd_flags */
#define	NVDF_INUSE		0x1	/* device in use */

#define	NVDRAM_DRIVER_NAME	"nvdram"

/* Prototypes */
extern nvdseg_t	*nvdram_impl_getseg(int unit);
extern uint_t	nvdram_impl_segcount(void);

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* !_SYS_NVDRAM_IMPL_H */
