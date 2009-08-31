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
 * Copyright (c) 2006-2008 NEC Corporation
 */

#ifndef _SYS_CMLB_IMPL_H
#define	_SYS_CMLB_IMPL_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/cmlb.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

#if defined(_EXTFDISK_PARTITION) && (_EXTFDISK_PARTITION > 0)
#define	CMLB_EXTPART
#endif /* defined(_EXTFDISK_PARTITION) && (_EXTFDISK_PARTITION > 0) */

#if defined(_SUNOS_VTOC_8)
#define	NSDMAP			NDKMAP
#elif defined(_SUNOS_VTOC_16)
#ifndef CMLB_EXTPART
#define	NSDMAP			(NDKMAP + FD_NUMPART + 1)
#else /* CMLB_EXTPART */
#define	NSDMAP		(NDKMAP + FD_NUMPART + 1 + _EXTFDISK_PARTITION + 1)
#endif /* !CMLB_EXTPART */
#else
#error "No VTOC format defined."
#endif

#define	MAXPART			(NSDMAP + 1)
#define	WD_NODE			7


#if defined(__i386) || defined(__amd64) || defined(__arm)

#define	P0_RAW_DISK		(NDKMAP)
#define	FDISK_P1		(NDKMAP+1)
#define	FDISK_P2		(NDKMAP+2)
#define	FDISK_P3		(NDKMAP+3)
#define	FDISK_P4		(NDKMAP+4)

#endif  /* __i386 || __amd64 || __arm */

/* Driver Logging Levels */
#define	CMLB_LOGMASK_ERROR	0x00000001
#define	CMLB_LOGMASK_INFO	0x00000002
#define	CMLB_LOGMASK_TRACE	0x00000004

#define	CMLB_TRACE		0x00000001
#define	CMLB_INFO		0x00000002
#define	CMLB_ERROR		0x00000004


#define	CMLB_MUTEX(cl)		(&((cl)->cl_mutex))
#define	CMLB_DEVINFO(cl)	((cl)->cl_devi)
#define	CMLB_LABEL(cl)		(DEVI(((cl)->cl_devi))->devi_binding_name)


#define	ISREMOVABLE(cl)		(cl->cl_is_removable == 1)
#define	ISCD(cl)		(cl->cl_device_type == DTYPE_RODIRECT)
#define	ISHOTPLUGGABLE(cl)	(cl->cl_is_hotpluggable == 1)

#if defined(_SUNOS_VTOC_8)

#define	CMLBUNIT_SHIFT		3
#define	CMLBPART_MASK		7

#elif defined(_SUNOS_VTOC_16)

#ifndef CMLB_EXTPART
#define	CMLBUNIT_SHIFT		6
#define	CMLBPART_MASK		63
#else /* CMLB_EXTPART */
#define	CMLBUNIT_SHIFT		7
#define	CMLBPART_MASK		127
#endif /* !CMLB_EXTPART */

#else
#error "No VTOC format defined."
#endif

#define	CMLBUNIT(dev)		(getminor((dev)) >> CMLBUNIT_SHIFT)
#define	CMLBPART(dev)		(getminor((dev)) &  CMLBPART_MASK)


#define	TRUE 			1
#define	FALSE			0

/*
 * Return codes of cmlb_uselabel().
 */
#define	CMLB_LABEL_IS_VALID	0
#define	CMLB_LABEL_IS_INVALID	1

/*
 * fdisk partition mapping structure
 */
struct fmap {
	daddr_t fmap_start;	/* starting block number */
	daddr_t fmap_nblk;	/* number of blocks */
};

/* for cm_state */
typedef enum  {
	CMLB_INITED = 0,
	CMLB_ATTACHED
} cmlb_state_t;

typedef enum
{
	CMLB_LABEL_UNDEF = 0,
	CMLB_LABEL_VTOC,
	CMLB_LABEL_EFI
} cmlb_label_t;


typedef struct cmlb_lun {
	dev_info_t	*cl_devi;		/* pointer to devinfo */
	struct dk_vtoc	cl_vtoc;	/* disk VTOC */
	struct dk_geom	cl_g;		/* disk geometry */

	diskaddr_t	cl_blockcount;		/* capacity */
	uint32_t	cl_tgt_blocksize;	/* blocksize */

	diskaddr_t	cl_solaris_size;	/* size of Solaris partition */
	uint_t		cl_solaris_offset;	/* offset to Solaris part. */

	struct  dk_map  cl_map[MAXPART];	/* logical partitions */
	diskaddr_t	cl_offset[MAXPART];	/* partition start blocks */

#ifndef CMLB_EXTPART
	struct fmap	cl_fmap[FD_NUMPART];	/* fdisk partitions */
#else
	struct fmap	cl_fmap[FD_NUMPART + 1 + _EXTFDISK_PARTITION];
						/* fdisk partitions */
#endif

	uchar_t		cl_asciilabel[LEN_DKL_ASCII];	/* Disk ASCII label */

	/*
	 * This is the HBAs current notion of the geometry of the drive,
	 * for HBAs that support the "geometry" property.
	 */
	struct cmlb_geom	cl_lgeom;

	/*
	 * This is the geometry of the device as reported by the MODE SENSE,
	 * command, Page 3 (Format Device Page) and Page 4 (Rigid Disk Drive
	 * Geometry Page), assuming MODE SENSE is supported by the target.
	 */
	struct cmlb_geom	cl_pgeom;

	ushort_t	cl_dkg_skew;		/* skew */

	cmlb_label_t	cl_def_labeltype;	/* default label type */

	/* label type based on which minor nodes were created last */
	cmlb_label_t	cl_last_labeltype;

	cmlb_label_t	cl_cur_labeltype;	/* current label type */

	/* indicates whether vtoc label is read from media */
	uchar_t		cl_vtoc_label_is_from_media;

	cmlb_state_t	cl_state;		/* state of handle */

	int		cl_f_geometry_is_valid;
	int		cl_sys_blocksize;

	kmutex_t	cl_mutex;

	/* the following are passed in at attach time */
	int		cl_is_removable;	/* 1 is removable */
	int		cl_is_hotpluggable;	/* 1 is hotpluggable */
	int		cl_alter_behavior;
	char 		*cl_node_type;		/* DDI_NT_... */
	int		cl_device_type;		/* DTYPE_DIRECT,.. */
	int		cl_reserved;		/* reserved efi partition # */
	cmlb_tg_ops_t 	*cmlb_tg_ops;

} cmlb_lun_t;

/*
 * Driver minor node structure
 */
struct driver_minor_data {
	char	*name;
	minor_t	minor;
	int	type;
};

_NOTE(MUTEX_PROTECTS_DATA(cmlb_lun::cl_mutex, cmlb_lun))
_NOTE(SCHEME_PROTECTS_DATA("stable data", cmlb_lun::cmlb_tg_ops))
_NOTE(SCHEME_PROTECTS_DATA("stable data", cmlb_lun::cl_devi))
_NOTE(SCHEME_PROTECTS_DATA("stable data", cmlb_lun::cl_is_removable))
_NOTE(SCHEME_PROTECTS_DATA("stable data", cmlb_lun::cl_is_hotpluggable))
_NOTE(SCHEME_PROTECTS_DATA("stable data", cmlb_lun::cl_node_type))
_NOTE(SCHEME_PROTECTS_DATA("stable data", cmlb_lun::cl_sys_blocksize))
_NOTE(SCHEME_PROTECTS_DATA("stable data", cmlb_lun::cl_alter_behavior))
_NOTE(SCHEME_PROTECTS_DATA("private data", cmlb_geom))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", cmlb_lun::cl_f_geometry_is_valid))



#define	DK_TG_READ(ihdlp, bufaddr, start_block, reqlength, tg_cookie)\
	(ihdlp->cmlb_tg_ops->tg_rdwr)(CMLB_DEVINFO(ihdlp), TG_READ, \
	bufaddr, start_block, reqlength, tg_cookie)

#define	DK_TG_WRITE(ihdlp,  bufaddr, start_block, reqlength, tg_cookie)\
	(ihdlp->cmlb_tg_ops->tg_rdwr)(CMLB_DEVINFO(ihdlp), TG_WRITE,\
	bufaddr, start_block, reqlength, tg_cookie)

#define	DK_TG_GETPHYGEOM(ihdlp, phygeomp, tg_cookie) \
	(ihdlp->cmlb_tg_ops->tg_getinfo)(CMLB_DEVINFO(ihdlp), TG_GETPHYGEOM,\
	    (void *)phygeomp, tg_cookie)

#define	DK_TG_GETVIRTGEOM(ihdlp, virtgeomp, tg_cookie) \
	(ihdlp->cmlb_tg_ops->tg_getinfo)(CMLB_DEVINFO(ihdlp), TG_GETVIRTGEOM,\
	    (void *)virtgeomp, tg_cookie)

#define	DK_TG_GETCAP(ihdlp, capp, tg_cookie) \
	(ihdlp->cmlb_tg_ops->tg_getinfo)(CMLB_DEVINFO(ihdlp), TG_GETCAPACITY,\
	capp, tg_cookie)

#define	DK_TG_GETBLOCKSIZE(ihdlp, lbap, tg_cookie) \
	(ihdlp->cmlb_tg_ops->tg_getinfo)(CMLB_DEVINFO(ihdlp),\
	TG_GETBLOCKSIZE, lbap, tg_cookie)

#define	DK_TG_GETATTRIBUTE(ihdlp, attributep, tg_cookie) \
	(ihdlp->cmlb_tg_ops->tg_getinfo)(CMLB_DEVINFO(ihdlp), TG_GETATTR,\
	    attributep, tg_cookie)

#ifdef CMLB_EXTPART
extern int cmlb_update_fdisk_and_vtoc(struct cmlb_lun *cl, void *tg_cookie); 
extern void cmlb_setup_default_geometry(struct cmlb_lun *cl, void *tg_cookie);
extern int cmlb_create_logical_partition_nodes(struct cmlb_lun *cl,
    int instance);
extern void cmlb_setup_logical_partition(struct cmlb_lun *cl);
extern int cmlb_read_logical_partition(struct cmlb_lun *cl,
    struct ipart fdisk[], caddr_t bufp, int *uidx, uint_t *solaris_offset,
    daddr_t *solaris_size, uint32_t blocksize, void *tg_cookie);
extern int cmlb_dkio_get_ebr(struct cmlb_lun *cl, caddr_t arg, int flag,
    void *tg_cookie);
extern int cmlb_dkio_set_ebr(struct cmlb_lun *cl, caddr_t arg, int flag,
    void *tg_cookie);
#endif /* CMLB_EXTPART */

#ifdef DISK_ACCESS_CTRL
extern void cmlb_access_ctrl_invalidate_nodes(struct cmlb_lun *cl, int instance,
    struct driver_minor_data *dmdp);
extern void cmlb_access_ctrl_invalidate_extpartition(struct cmlb_lun *cl,
    struct driver_minor_data dmdp_blk[], struct driver_minor_data dmdp_chr[]);
extern void cmlb_dbg(uint_t comp, struct cmlb_lun *cl, const char *fmt, ...);
#endif /* DISK_ACCESS_CTRL */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_CMLB_IMPL_H */
