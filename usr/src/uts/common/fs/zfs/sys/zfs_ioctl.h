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

#ifndef	_SYS_ZFS_IOCTL_H
#define	_SYS_ZFS_IOCTL_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/cred.h>
#include <sys/dmu.h>
#include <sys/zio.h>
#include <sys/dsl_deleg.h>
#include <zfs_types.h>

#ifdef _KERNEL
#include <sys/nvpair.h>
#endif	/* _KERNEL */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Property values for snapdir
 */
#define	ZFS_SNAPDIR_HIDDEN		0
#define	ZFS_SNAPDIR_VISIBLE		1

#define	DMU_BACKUP_STREAM_VERSION (1ULL)
#define	DMU_BACKUP_HEADER_VERSION (2ULL)
#define	DMU_BACKUP_MAGIC 0x2F5bacbacULL

#define	DRR_FLAG_CLONE (1<<0)

#ifdef ZFS_IOCTL_MINIMUMSET
#define	zfs_ioc_pool_create		zfs_ioc_notsup
#define	zfs_ioc_pool_destroy		zfs_ioc_notsup
#define	zfs_ioc_pool_import		zfs_ioc_notsup
#define	zfs_ioc_pool_export		zfs_ioc_notsup
#define	zfs_ioc_pool_tryimport		zfs_ioc_notsup
#define	zfs_ioc_pool_scrub		zfs_ioc_notsup
#define	zfs_ioc_pool_freeze		zfs_ioc_notsup
#define	zfs_ioc_pool_upgrade		zfs_ioc_notsup
#define	zfs_ioc_pool_get_history	zfs_ioc_notsup
#define	zfs_ioc_vdev_add		zfs_ioc_notsup
#define	zfs_ioc_vdev_remove		zfs_ioc_notsup
#define	zfs_ioc_vdev_set_state		zfs_ioc_notsup
#define	zfs_ioc_vdev_detach		zfs_ioc_notsup
#define	zfs_ioc_vdev_setpath		zfs_ioc_notsup
#define	zfs_ioc_objset_zplprops		zfs_ioc_notsup
#define	zfs_ioc_set_prop		zfs_ioc_notsup
#define	zfs_ioc_create_minor		zfs_ioc_notsup
#define	zfs_ioc_remove_minor		zfs_ioc_notsup
#define	zfs_ioc_create			zfs_ioc_notsup
#define	zfs_ioc_rename			zfs_ioc_notsup
#define	zfs_ioc_recv			zfs_ioc_notsup
#define	zfs_ioc_send			zfs_ioc_notsup
#define	zfs_ioc_inject_fault		zfs_ioc_notsup
#define	zfs_ioc_clear_fault		zfs_ioc_notsup
#define	zfs_ioc_inject_list_next	zfs_ioc_notsup
#define	zfs_ioc_error_log		zfs_ioc_notsup
#define	zfs_ioc_clear			zfs_ioc_notsup
#define	zfs_ioc_promote			zfs_ioc_notsup
#define	zfs_ioc_destroy_snaps		zfs_ioc_notsup
#define	zfs_ioc_snapshot		zfs_ioc_notsup
#define	zfs_ioc_dsobj_to_dsname		zfs_ioc_notsup
#define	zfs_ioc_obj_to_path		zfs_ioc_notsup
#define	zfs_ioc_pool_set_props		zfs_ioc_notsup
#define	zfs_ioc_pool_get_props		zfs_ioc_notsup
#define	zfs_ioc_iscsi_perm_check	zfs_ioc_notsup
#define	zfs_ioc_share			zfs_ioc_notsup
#define	zfs_ioc_inherit_prop		zfs_ioc_notsup
#endif	/* ZFS_IOCTL_MINIMUMSET */

#if defined(ZFS_NO_MIRROR) || defined(ZFS_IOCTL_MINIMUMSET)
#define	zfs_ioc_vdev_attach		zfs_ioc_notsup
#endif	/* defined(ZFS_NO_MIRROR) || defined(ZFS_IOCTL_MINIMUMSET) */

/*
 * zfs ioctl command structure
 */
typedef struct dmu_replay_record {
	enum {
		DRR_BEGIN, DRR_OBJECT, DRR_FREEOBJECTS,
		DRR_WRITE, DRR_FREE, DRR_END,
	} drr_type;
	uint32_t drr_payloadlen;
	union {
		struct drr_begin {
			uint64_t drr_magic;
			uint64_t drr_version;
			uint64_t drr_creation_time;
			dmu_objset_type_t drr_type;
			uint32_t drr_flags;
			uint64_t drr_toguid;
			uint64_t drr_fromguid;
			char drr_toname[MAXNAMELEN];
		} drr_begin;
		struct drr_end {
			zio_cksum_t drr_checksum;
		} drr_end;
		struct drr_object {
			objid_t drr_object;
#ifdef ZFS_COMPACT
			uint32_t drr_pad2;
#endif	/* ZFS_COMPACT */
			dmu_object_type_t drr_type;
			dmu_object_type_t drr_bonustype;
			uint32_t drr_blksz;
			uint32_t drr_bonuslen;
			uint8_t drr_checksum;
			uint8_t drr_compress;
			uint8_t drr_pad[6];
			/* bonus content follows */
		} drr_object;
		struct drr_freeobjects {
			objid_t drr_firstobj;
			objid_t drr_numobjs;
		} drr_freeobjects;
		struct drr_write {
			objid_t drr_object;
#ifdef ZFS_COMPACT
			uint32_t drr_pad2;
#endif	/* ZFS_COMPACT */
			dmu_object_type_t drr_type;
			uint32_t drr_pad;
			uint64_t drr_offset;
			uint64_t drr_length;
			/* content follows */
		} drr_write;
		struct drr_free {
			objid_t drr_object;
			uint64_t drr_offset;
			uint64_t drr_length;
		} drr_free;
	} drr_u;
} dmu_replay_record_t;

typedef struct zinject_record {
	objid_t		zi_objset;
	objid_t		zi_object;
	uint64_t	zi_start;
	uint64_t	zi_end;
	uint64_t	zi_guid;
	uint32_t	zi_level;
	uint32_t	zi_error;
	uint64_t	zi_type;
	uint32_t	zi_freq;
	uint32_t	zi_pad;	/* pad out to 64 bit alignment */
} zinject_record_t;

#define	ZINJECT_NULL		0x1
#define	ZINJECT_FLUSH_ARC	0x2
#define	ZINJECT_UNLOAD_SPA	0x4

typedef struct zfs_share {
	uint64_t	z_exportdata;
	uint64_t	z_sharedata;
	uint64_t	z_sharetype;	/* 0 = share, 1 = unshare */
	uint64_t	z_sharemax;  /* max length of share string */
} zfs_share_t;

/*
 * ZFS file systems may behave the usual, POSIX-compliant way, where
 * name lookups are case-sensitive.  They may also be set up so that
 * all the name lookups are case-insensitive, or so that only some
 * lookups, the ones that set an FIGNORECASE flag, are case-insensitive.
 */
typedef enum zfs_case {
	ZFS_CASE_SENSITIVE,
	ZFS_CASE_INSENSITIVE,
	ZFS_CASE_MIXED
} zfs_case_t;

typedef struct zfs_cmd {
	char		zc_name[MAXPATHLEN];
	char		zc_value[MAXPATHLEN * 2];
	char		zc_string[MAXNAMELEN];
	uint64_t	zc_guid;
	uint64_t	zc_nvlist_conf;		/* really (char *) */
	uint64_t	zc_nvlist_conf_size;
	uint64_t	zc_nvlist_src;		/* really (char *) */
	uint64_t	zc_nvlist_src_size;
	uint64_t	zc_nvlist_dst;		/* really (char *) */
	uint64_t	zc_nvlist_dst_size;
	uint64_t	zc_cookie;
	uint64_t	zc_objset_type;
	uint64_t	zc_perm_action;
	uint64_t 	zc_history;		/* really (char *) */
	uint64_t 	zc_history_len;
	uint64_t	zc_history_offset;
	objid_t		zc_obj;
	zfs_share_t	zc_share;
	dmu_objset_stats_t zc_objset_stats;
	struct drr_begin zc_begin_record;
	zinject_record_t zc_inject_record;
} zfs_cmd_t;

#define	ZVOL_MAX_MINOR	(1 << 16)
#define	ZFS_MIN_MINOR	(ZVOL_MAX_MINOR + 1)

#ifdef _KERNEL

typedef struct zfs_creat {
	nvlist_t	*zct_zplprops;
	nvlist_t	*zct_props;
} zfs_creat_t;

extern dev_info_t *zfs_dip;

extern int zfs_secpolicy_snapshot_perms(const char *name, cred_t *cr);
extern int zfs_secpolicy_rename_perms(const char *from,
    const char *to, cred_t *cr);
extern int zfs_secpolicy_destroy_perms(const char *name, cred_t *cr);
extern int zfs_busy(void);
extern int zfs_unmount_snap(char *, void *);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ZFS_IOCTL_H */
