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

#ifndef _SYS_FS_XNODE_H
#define	_SYS_FS_XNODE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * basic types
 */
typedef uint32_t	xram_xno_t;	/* xno number */
typedef uint32_t	xram_blkno_t;	/* offset of blocks */

#define XRAMFS_IMGVERSION	(1U)

#define XRAM_BLKNO_MAX		(0xffffffffU)
#define XRAM_XNO_MAX		(0xffffffffU)
#define XRAM_LINK_MAX		(0x7fffU)
#define XRAM_LINK_MASK		(0x7fffU)
#define XRAM_MNODE_MAX		(0xfffffffdU)	/* # of media node we have */
#define XRAM_FILES_MAX		(UINT32_MAX)    /* xramdircheader->xdh_nent */
#define XRAM_XMN_START_MAX	(UINT32_MAX)    /* xrammnode->xmn_start */
#define XRAM_XMN_SIZE_MAX	(UINT32_MAX)    /* xrammnode->xmn_size */
#define XRAM_XDE_START_MAX	(0xffffffU)	/* xramdirent->xde_start */
#define XRAM_XDE_LEN_MAX	(0xffU)		/* xramdirent->xde_len */
#define XRAM_MAXOFFSET_T	(0xffffffffUL) /* max file size/ abs(offset) */

/* magic fourCC */
#define XRAM_MAGIC		('X' << 24 | 'R' << 16 | 'A' << 8 | 'M')

/*
 * xramheader
 */
struct xramheader {
	/* +0,  [4] */
	uint32_t	xh_magic;	/* "XRAM" */
	/* +4,  [4] */
	uint32_t	xh_version;	/* version of this xram image */
	/* +8, [4] */
	uint32_t	xh_ctime;	/* image creation time. (time_t) */
	/* +12, [4] */
	uint32_t	xh_blocks;	/* number of blocks (pages) held. */
	/* +16, [4] */
	uint32_t	xh_files;	/* number of XNODE held. */
	/* +20, [1,1,1,1] */
	uint8_t		xh_dname_shift;	/* dname unit size (log 2) */
	uint8_t		xh_symlink_shift; /* symlink unit size (log 2) */
	uint8_t		xh_pfile_shift; /* packed file unit size(log 2) */
	uint8_t		xh_pageblk_shift;	/* paging block size (log 2) */

	/* +24, [4] */
	xram_blkno_t	xh_off_dir;	/* offset of directory blocks. */
	/* +28, [4] */
	xram_blkno_t	xh_off_symlink;	/* offset of symlink blocks. */
	/* +32, [4] */
	xram_blkno_t	xh_off_pfile;	/* offset of packed file blocks. */
	/* +36, [4] */
	xram_blkno_t	xh_off_xfile;	/* offset of mappable file blocks. */

	/* +40, [4] */
	xram_blkno_t	xh_extheader;	/* offset of extra xramheader block */
	/* +44,  [4] */
	uint32_t	xh_off_dnamergn; /* offset of dentry name region */
	/* +48, [16] */
	char		xh_reserved[16];/* reserved */
};

/* modes */
#define XMODEMASK	0007777		/* mode of file */
#define XIFMT		0170000		/* type of file */
#define XIFIFO		0010000		/* named pipe (fifo) */
#define XIFCHR		0020000		/* character special */
#define XIFDIR		0040000		/* directory */
#define XIFBLK		0060000		/* block special */
#define XIFPACK		0100000		/* packed file */
#define XIFMAP		0110000		/* mappable file */
#define XIFLNK		0120000		/* symbolic link */

/*
 * xrammnode: xram media node
 */
struct xrammnode {
	/* +0, [2] */
	uint16_t	xmn_typemode;	/* M: 4bit type + 12bit mode :L */
	/* +2, [2] */
	uint16_t	xmn_nlink;	/* number of links from dirent. */
	/* +4, [4] */
	uint32_t	_xmn_reserved;	/* reserved */
	/* +8, [4] */
	uint32_t	xmn_uid;	/* owner userID of the xnode. */
	/* +12, [4] */
	uint32_t	xmn_gid;	/* owner groupID of the xnode. */
	/* +16, [8] */
	union {
		struct {
			/* PACKED or MAP or DIR or LNK */
			/* start number of the file. */
			uint32_t	_xmn_u_file_start;
			/* file size in bytes. */
			uint32_t	_xmn_u_file_size;
		} _xmn_u_file;
		struct {
			/* CHR or BLK */
			uint32_t	_xmn_u_rdev_rdev_major;
			uint32_t	_xmn_u_rdev_rdev_minor;
		} _xmn_u_rdev;
	} _xmn_un;
#define xmn_start	_xmn_un._xmn_u_file._xmn_u_file_start
#define xmn_size	_xmn_un._xmn_u_file._xmn_u_file_size
#define xmn_rdev_major	_xmn_un._xmn_u_rdev._xmn_u_rdev_rdev_major
#define xmn_rdev_minor	_xmn_un._xmn_u_rdev._xmn_u_rdev_rdev_minor
#define xmn_union	_xmn_un

	/* +24, [4,4] */
	uint32_t	xmn_time_sec;	/* time: secs (in UNIX time) */
	uint32_t	xmn_time_nsec;	/* time: nano-seconds */
};
#define XRAMMNODE_XNO_ROOT	(2U)
#define XRAMMNODE_XNO_INVAL	(0U)
#define XRAMMNODE_TYPE(xmp) ((xmp)->xmn_typemode & XIFMT)
#define XRAMMNODE_MODE(xmp) ((xmp)->xmn_typemode & MODEMASK)

/*
 * directory
 */
#define XRAMDIR_ALIGN_SHIFT	(3U)	/* << 3 (= *8) */

/* type of directory definition */
#define XRAM_DTYPE_LINEAR	0x0
#define XRAM_DTYPE_SORTED	0x1

/*
 * xramdircheader: xram directory common header
 */
struct xramdircheader {
	uint32_t	xdh_nent;	/* number of entries */
	xram_xno_t	xdh_xnodot;	/* xno of dot */
	xram_xno_t	xdh_xnodotdot;	/* xno of dotdot */
	uint8_t		xdh_type;	/* directory type */
	uint8_t		xdh_reserved[3];	/* reserved */
};

/* number of 'virtual' (exclude from list but exist) nodes */
#define XRAMFS_NUM_VIRTUAL_NODES	(2U)

/*
 * xramdirent
 * type 0, 1 directory entry
 */
struct xramdirent {
	xram_xno_t	xde_xno;	/* xno of entry */
	uint32_t	xde_start_len;	/* start of name and length of name */
};
#define XRAMDIRENT_START_MASK		(0xffffff00U)
#define XRAMDIRENT_START_MASK_SHIFT	(8)
#define XRAMDIRENT_LENGTH_MASK		(0x000000ffU)
#define XRAMDIRENT_LENGTH_MASK_SHIFT	(0)

#define XRAMDIRENT_START(xde)					\
	(((xde)->xde_start_len & XRAMDIRENT_START_MASK) >>	\
	 XRAMDIRENT_START_MASK_SHIFT)
#define XRAMDIRENT_LENGTH(xde)					\
	(((xde)->xde_start_len & XRAMDIRENT_LENGTH_MASK) >>	\
	 XRAMDIRENT_LENGTH_MASK_SHIFT)

#ifdef _KERNEL

/*
 * xnode is the file system dependent node for xramfs.
 * xnode has an indirect reference to xrammnode.
 * Any xnodes exist only in newly allocated memory.
 */

struct xnode {
	struct xnode		*xn_next;	/* pointer to next node */
	struct xnode		**xn_pprev;	/* pointer to prev->xn_next */
	struct xrammnode	*xn_xmp;	/* pointer to disk node */
	vnode_t		 	*xn_vp;		/* back pointer */

#define xn_typemode	xn_xmp->xmn_typemode
#define xn_nlink	xn_xmp->xmn_nlink
#define xn_uid		xn_xmp->xmn_uid
#define xn_gid		xn_xmp->xmn_gid
#define xn_start	xn_xmp->xmn_start
#define xn_size		xn_xmp->xmn_size
#define xn_rdev_major	xn_xmp->xmn_rdev_major
#define xn_rdev_minor	xn_xmp->xmn_rdev_minor
#define xn_time_sec	xn_xmp->xmn_time_sec
#define xn_time_nsec	xn_xmp->xmn_time_nsec
};
struct xmount;

extern	void	xnode_create_cache(void);
extern	void	xnode_destroy_cache(void);
extern	int	xnode_lookup(struct xmount *, xram_xno_t, struct xnode **);
extern	void	xnode_inactive(struct xmount *, struct xnode *);
extern	void	xnode_free(struct xmount *, struct xnode *);
extern	kmutex_t *	xnode_getmutex(struct xmount *, xram_xno_t);
extern	void	xnode_lock_all(struct xmount *);
extern	int	xnode_isalone(struct xmount *);
extern	void	xnode_unlock_all(struct xmount *);

/* xp means xnode */
#define xnode_hold(xp)	VN_HOLD(XNTOV(xp))
#define xnode_rele(xp)	VN_RELE(XNTOV(xp))
#define XNODE_TYPE(xp)	((xp)->xn_typemode & XIFMT)
#define XNODE_MODE(xp)	((xp)->xn_typemode & XMODEMASK)
#define XNODE_DIR(xm, dir)						\
	(struct xramdircheader *)((xm)->xm_dir +			\
				  ((dir)->xn_start << (XRAMDIR_ALIGN_SHIFT)))
#define XNODE_XNO(xm, xp)					\
	((xp)->xn_xmp - (struct xrammnode *)(xm)->xm_kasaddr)
#define XNO_NODEID(xno)		(xno)
#define XNODE_NODEID(xm, xp)	(XNO_NODEID(XNODE_XNO(xm, xp)))
#define XNODE_SYMLINK(xm, sym)						\
	((xm)->xm_symlink + ((sym)->xn_start <<				\
			     XMOUNT_XRAMHEADER(xm)->xh_symlink_shift))
#define XNODE_MAPFILE(xm, map)					\
	((xm)->xm_map + ((map)->xn_start <<			\
			 XMOUNT_XRAMHEADER(xm)->xh_pageblk_shift))
#define XRAMDIRENT_DNAME(xm, xde)					\
	((xm)->xm_dname + (XRAMDIRENT_START(xde) <<			\
			   XMOUNT_XRAMHEADER(xm)->xh_dname_shift))

#if 0
#define XRAM_PAGESIZE(xm)						\
	(1ULL << ((struct xramheader *)(xm)->xm_kasaddr)->xh_pageblk_shift)
#define XRAM_PAGESHIFT(xm)						\
	(((struct xramheader *)(xm)->xm_kasaddr)->xh_pageblk_shift)
#else
#define XRAM_PAGESIZE(xm)	(PAGESIZE)
#define XRAM_PAGESHIFT(xm)	(PAGESHIFT)
#endif
#define XNO2XMN(xm, xno)  \
	(&(((struct xrammnode *)(xm)->xm_kasaddr)[(xno)]))

#endif  /* defined(_KERNEL) */

#ifdef __cplusplus
};
#endif /* defined(__cplusplus) */

#endif	/* defined(_SYS_FS_XNODE_H) */
