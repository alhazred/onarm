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

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * Routines to allocate and deallocate data blocks on the disk
 */

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/fs/pc_label.h>
#include <sys/fs/pc_fs.h>
#include <sys/fs/pc_dir.h>
#include <sys/fs/pc_node.h>

static int pc_getcluster(struct pcfs *, pc_cluster32_t, pc_cluster32_t *);
static int pc_getclusterraw(struct pcfs *, pc_cluster32_t, pc_cluster32_t *);

/*
 * Convert file logical block (cluster) numbers to disk block numbers.
 * Also return number of physically contiguous blocks if asked for.
 * Used for reading only. Use pc_balloc for writing.
 */
int
pc_bmap(
	struct pcnode *pcp,		/* pcnode for file */
	daddr_t lcn,			/* logical cluster no */
	daddr_t *dbnp,			/* ptr to phys block no */
	uint_t *contigbp)		/* ptr to number of contiguous bytes */
					/* may be zero if not wanted */
{
	struct pcfs *fsp;	/* pcfs that file is in */
	struct vnode *vp;
	pc_cluster32_t cn, ncn;		/* current, next cluster number */
	daddr_t olcn = lcn;

	vp = PCTOV(pcp);
	fsp = VFSTOPCFS(vp->v_vfsp);

	if (lcn < 0)
		return (ENOENT);

	/*
	 * FAT12 / FAT16 root directories are a continuous section on disk
	 * before the actual data clusters. Specialcase this here.
	 */
	if (!IS_FAT32(fsp) && (vp->v_flag & VROOT)) {
		daddr_t lbn; /* logical (disk) block number */

		lbn = pc_cltodb(fsp, lcn);
		if (lbn >= fsp->pcfs_rdirsec) {
			PC_DPRINTF0(2, "pc_bmap: ENOENT1\n");
			return (ENOENT);
		}
		*dbnp = pc_dbdaddr(fsp, fsp->pcfs_rdirstart + lbn);
		if (contigbp) {
			ASSERT (*contigbp >= fsp->pcfs_secsize);
			*contigbp = MIN(*contigbp,
			    fsp->pcfs_secsize * (fsp->pcfs_rdirsec - lbn));
		}
		return (0);
	}

	if (lcn >= fsp->pcfs_ncluster) {
		PC_DPRINTF0(2, "pc_bmap: ENOENT2\n");
		return (ENOENT);
	}
	if (vp->v_type == VREG &&
	    (pcp->pc_size == 0 ||
	    lcn >= (daddr_t)howmany((offset_t)pcp->pc_size,
			fsp->pcfs_clsize))) {
		PC_DPRINTF0(2, "pc_bmap: ENOENT3\n");
		return (ENOENT);
	}
	ncn = pcp->pc_scluster;
	if (IS_FAT32(fsp) && ncn == 0)
		ncn = fsp->pcfs_rdirstart;

	/* Do we have a cached index/cluster pair? */
	if (pcp->pc_lindex > 0 && lcn >= pcp->pc_lindex) {
		lcn -= pcp->pc_lindex;
		ncn = pcp->pc_lcluster;
	} else if (pcp->pc_bindex > 0 && lcn >= pcp->pc_bindex) {
		lcn -= pcp->pc_bindex;
		ncn = pcp->pc_bcluster;
	}
	do {
		cn = ncn;
		if (!pc_validcl(fsp, cn)) {
			if (IS_FAT32(fsp) && cn >= PCF_LASTCLUSTER32 &&
			    vp->v_type == VDIR) {
				PC_DPRINTF0(2, "pc_bmap: ENOENT4\n");
				return (ENOENT);
			} else if (!IS_FAT32(fsp) &&
			    cn >= PCF_LASTCLUSTER &&
			    vp->v_type == VDIR) {
				PC_DPRINTF0(2, "pc_bmap: ENOENT5\n");
				return (ENOENT);
			} else {
				PC_DPRINTF1(1,
				    "pc_bmap: badfs cn=%d\n", cn);
				(void) pc_badfs(fsp);
				return (EIO);
			}
		}
		if (pc_getcluster(fsp, cn, &ncn))
			return (EIO);
	} while (lcn--);

	/*
	 * Cache this cluster, as we'll most likely visit the
	 * one after this next time.  Considerably improves
	 * performance on sequential reads and writes.
	 */
	pcp->pc_lindex = olcn;
	pcp->pc_lcluster = cn;
	*dbnp = pc_cldaddr(fsp, cn);

	if (contigbp && *contigbp > fsp->pcfs_clsize) {
		uint_t count = fsp->pcfs_clsize;

		while ((cn + 1) == ncn && count < *contigbp &&
		    pc_validcl(fsp, ncn)) {
			count += fsp->pcfs_clsize;
			cn = ncn;
			if (pc_getcluster(fsp, cn, &ncn))
				return (EIO);
		}
		*contigbp = count;
	}
	return (0);
}

/*
 * Allocate file logical blocks (clusters).
 * Return disk address of last allocated cluster.
 */
int
pc_balloc(
	struct pcnode *pcp,	/* pcnode for file */
	daddr_t lcn,		/* logical cluster no */
	int zwrite,			/* zerofill blocks? */
	daddr_t *dbnp)			/* ptr to phys block no */
{
	struct pcfs *fsp;	/* pcfs that file is in */
	struct vnode *vp;
	pc_cluster32_t cn;	/* current cluster number */
	pc_cluster32_t ncn;	/* next cluster number */
	int error, zw = 0;

	vp = PCTOV(pcp);
	fsp = VFSTOPCFS(vp -> v_vfsp);

	if (lcn < 0) {
		return (EFBIG);
	}
	if (zwrite == 1)
		zw = 1;

	/*
	 * Again, FAT12/FAT16 root directories are not data clusters.
	 */
	if (!IS_FAT32(fsp) && (vp->v_flag & VROOT)) {
		daddr_t lbn;

		lbn = pc_cltodb(fsp, lcn);
		if (lbn >= fsp->pcfs_rdirsec)
			return (ENOSPC);
		*dbnp = pc_dbdaddr(fsp, fsp->pcfs_rdirstart + lbn);
		return (0);
	}

	if (lcn >= fsp->pcfs_ncluster)
		return (ENOSPC);

	if (IS_FAT32(fsp)) {
		if (pcp->pc_scluster == PCF_LASTCLUSTERMARK32)
			pcp->pc_scluster = 0;
	} else if (pcp->pc_scluster == PCF_LASTCLUSTERMARK) {
		pcp->pc_scluster = 0;
	}

	if ((vp->v_type == VREG && pcp->pc_scluster == 0) ||
	    (vp->v_type == VDIR && lcn == 0)) {
		error = pc_alloccluster(fsp, 1, &cn);
		if (error)
			return (error);
		switch (cn) {
		case PCF_FREECLUSTER:
			return (ENOSPC);
		case PCF_ERRORCLUSTER:
			return (EIO);
		}
		pcp->pc_scluster = cn;
	} else {
		cn = pcp->pc_scluster;
		if (IS_FAT32(fsp) && cn == 0)
			cn = fsp->pcfs_rdirstart;
		if (!pc_validcl(fsp, cn)) {
			PC_DPRINTF1(1, "pc_balloc: badfs cn=%d\n", cn);
			(void) pc_badfs(fsp);
			return (EIO);
		}
	}

	if (pcp->pc_lindex > 0 && lcn > pcp->pc_lindex) {
		lcn -= pcp->pc_lindex;
		cn = pcp->pc_lcluster;
	}
	while (lcn-- > 0) {
		if (!pc_validcl(fsp, cn)) {
			PC_DPRINTF1(1, "pc_balloc: badfs cn=%d\n", cn);
			(void) pc_badfs(fsp);
			return (EIO);
		}
		if (pc_getcluster(fsp, cn, &ncn))
			return (EIO);

		if ((IS_FAT32(fsp) && ncn >= PCF_LASTCLUSTER32) ||
		    (!IS_FAT32(fsp) && ncn >= PCF_LASTCLUSTER)) {
			/*
			 * Extend file (no holes).
			 */
			if (zwrite == 2 && lcn == 0)
				zw = 1;
			error = pc_alloccluster(fsp, zw, &ncn);
			if (error)
				return (error);
			switch (ncn) {
			case PCF_FREECLUSTER:
				return (ENOSPC);
			case PCF_ERRORCLUSTER:
				return (EIO);
			}
			if (pc_setcluster(fsp, cn, ncn))
				return (EIO);
		} else if (!pc_validcl(fsp, ncn)) {
			PC_DPRINTF1(1,
			    "pc_balloc: badfs ncn=%d\n", ncn);
			(void) pc_badfs(fsp);
			return (EIO);
		}
		cn = ncn;
	}
	/*
	 * Do not cache the new cluster/index values; when
	 * extending the file we're interested in the last
	 * written cluster and not the last cluster allocated.
	 */
	*dbnp = pc_cldaddr(fsp, cn);

	/* pc_lindex and pc_lcluster are backed up here. */
	pcp->pc_bindex = pcp->pc_lindex;
	pcp->pc_bcluster = pcp->pc_lcluster;

	return (0);
}

/*
 * Free file cluster chain after the first skipcl clusters.
 */
int
pc_bfree(struct pcnode *pcp, pc_cluster32_t skipcl)
{
	struct pcfs *fsp;
	pc_cluster32_t cn;
	pc_cluster32_t ncn;
	int n;
	struct vnode *vp;

	vp = PCTOV(pcp);
	fsp = VFSTOPCFS(vp->v_vfsp);
	if (!IS_FAT32(fsp) && (vp->v_flag & VROOT)) {
		panic("pc_bfree");
	}

	if (pcp->pc_size == 0 && vp->v_type == VREG) {
		return (0);
	}
	if (vp->v_type == VREG) {
		n = (int)howmany((offset_t)pcp->pc_size, fsp->pcfs_clsize);
		if (n > fsp->pcfs_ncluster) {
			PC_DPRINTF1(1, "pc_bfree: badfs n=%d\n", n);
			(void) pc_badfs(fsp);
			return (EIO);
		}
	} else {
		n = fsp->pcfs_ncluster;
	}
	cn = pcp->pc_scluster;
	if (IS_FAT32(fsp) && cn == 0)
		cn = fsp->pcfs_rdirstart;
	if (skipcl == 0) {
		if (IS_FAT32(fsp))
			pcp->pc_scluster = PCF_LASTCLUSTERMARK32;
		else
			pcp->pc_scluster = PCF_LASTCLUSTERMARK;
	}

	/* Invalidate last used cluster cache */
	pcp->pc_lindex = 0;
	pcp->pc_lcluster = pcp->pc_scluster;

	while (n--) {
		if (!pc_validcl(fsp, cn)) {
			PC_DPRINTF1(1, "pc_bfree: badfs cn=%d\n", cn);
			(void) pc_badfs(fsp);
			return (EIO);
		}
		if (pc_getcluster(fsp, cn, &ncn))
			return (EIO);
		if (skipcl == 0) {
			if (pc_setcluster(fsp, cn, PCF_FREECLUSTER))
				return (EIO);
		} else {
			skipcl--;
			if (skipcl == 0) {
				if (IS_FAT32(fsp)) {
					if (pc_setcluster(fsp, cn,
					    PCF_LASTCLUSTERMARK32))
						return (EIO);
				} else if (pc_setcluster(fsp, cn,
				    PCF_LASTCLUSTERMARK))
					return (EIO);
			}
		}
		if (IS_FAT32(fsp) && ncn >= PCF_LASTCLUSTER32 &&
		    vp->v_type == VDIR)
			break;
		if (!IS_FAT32(fsp) && ncn >= PCF_LASTCLUSTER &&
		    vp->v_type == VDIR)
			break;
		cn = ncn;
	}
	return (0);
}

/*
 * Return the number of free blocks in the filesystem.
 */
int
pc_freeclusters(struct pcfs *fsp)
{
	pc_cluster32_t cn;
	int free = 0;
	pc_cluster32_t ncn;

	if (IS_FAT32(fsp) &&
	    fsp->pcfs_fsinfo.fs_free_clusters != FSINFO_UNKNOWN)
		return (fsp->pcfs_fsinfo.fs_free_clusters);

	/*
	 * make sure the FAT is in core
	 */
	for (cn = PCF_FIRSTCLUSTER;
	    (int)cn < PCF_MAXCLUSTER(fsp); cn++) {
		if (pc_getcluster(fsp, cn, &ncn)) {
			break;
		}
		if (ncn == PCF_FREECLUSTER) {
			free++;
		}
	}

	if (IS_FAT32(fsp)) {
		ASSERT(fsp->pcfs_fsinfo.fs_free_clusters == FSINFO_UNKNOWN);
		fsp->pcfs_fsinfo.fs_free_clusters = free;
	}
	return (free);
}

/*
 * Cluster manipulation routines.
 * FAT must be resident.
 */

#ifdef PCFS_ENABLE_CACHE
int
pc_get_fat_mediadescriptor(struct pcfs *fsp, uchar_t *ci)
{
	pc_cluster32_t ncn;

	if (pc_getclusterraw(fsp, 0, &ncn))
		return (EIO);
	*ci = htoli(ncn) & 0xff;

	return (0);
}
#endif	/* PCFS_ENABLE_CACHE */

/*
 * Get the next cluster in the file cluster chain.
 *	cn = current cluster number in chain
 */
static int
pc_getcluster(struct pcfs *fsp, pc_cluster32_t cn, pc_cluster32_t *ncn)
{
	pc_cluster32_t cnraw;
	int ret;

	if (fsp->pcfs_fatp == (uchar_t *)0 || !pc_validcl(fsp, cn))
		return (EIO);

	ret = pc_getclusterraw(fsp, cn, &cnraw);
	if (ret)
		return (ret);

	if (IS_FAT32(fsp))
		*ncn = cnraw & ~PCFS_CLUSTER_RESERVEDMASK;
	else
		*ncn = cnraw;

	return (0);
}

static int
pc_getclusterraw(struct pcfs *fsp, pc_cluster32_t cn, pc_cluster32_t *ncn)
{
	unsigned char *fp;
#ifdef PCFS_ENABLE_CACHE
	int error;
	int32_t offset;
	struct pcfs_fatcache_unit *unitp;

	switch (fsp->pcfs_fattype) {
	case FAT32:
		offset = cn << 2;
		unitp = pc_fatcachesearch(fsp, offset, &error);
		if (unitp == NULL)
			return (error);
		fp = unitp->fatp + (offset - unitp->offset);
		cn = ltohi(*(pc_cluster32_t *)fp);
		break;
	case FAT16:
		offset = cn << 1;
		unitp = pc_fatcachesearch(fsp, offset, &error);
		if (unitp == NULL)
			return (error);
		fp = unitp->fatp + (offset - unitp->offset);
		cn = ltohs(*(pc_cluster16_t *)fp);
		break;
	case FAT12:
		offset = cn + (cn >> 1);
		unitp = pc_fatcachesearch(fsp, offset, &error);
		if (unitp == NULL)
			return (error);
		fp = unitp->fatp + (offset - unitp->offset);
		if (cn & 01) {
			cn = (((unsigned int)*fp++ & 0xf0) >> 4);
			cn += (*fp << 4);
		} else {
			cn = *fp++;
			cn += ((*fp & 0x0f) << 8);
		}
		if (cn >= PCF_12BCLUSTER)
			cn |= PCF_RESCLUSTER;
		break;
#else
	switch (fsp->pcfs_fattype) {
	case FAT32:
		fp = fsp->pcfs_fatp + (cn << 2);
		cn = ltohi(*(pc_cluster32_t *)fp);
		break;
	case FAT16:
		fp = fsp->pcfs_fatp + (cn << 1);
		cn = ltohs(*(pc_cluster16_t *)fp);
		break;
	case FAT12:
		fp = fsp->pcfs_fatp + (cn + (cn >> 1));
		if (cn & 01) {
			cn = (((unsigned int)*fp++ & 0xf0) >> 4);
			cn += (*fp << 4);
		} else {
			cn = *fp++;
			cn += ((*fp & 0x0f) << 8);
		}
		if (cn >= PCF_12BCLUSTER)
			cn |= PCF_RESCLUSTER;
		break;
#endif	/* PCFS_ENABLE_CACHE */
	default:
		pc_mark_irrecov(fsp);
		cn = PCF_ERRORCLUSTER;
	}
	*ncn = cn;
	return (0);
}

/*
 * Set a cluster in the FAT to a value.
 *	cn = cluster number to be set in FAT
 *	ncn = new value
 */
int
pc_setcluster(struct pcfs *fsp, pc_cluster32_t cn, pc_cluster32_t ncn)
{
	unsigned char *fp;
	pc_cluster16_t ncn16;
#ifdef PCFS_ENABLE_CACHE
	struct pcfs_fatcache_unit *unitp;
	int error;
	int32_t offset;
	pc_cluster32_t hfv;
#endif	/* PCFS_ENABLE_CACHE */

	if (fsp->pcfs_fatp == (uchar_t *)0 || !pc_validcl(fsp, cn))
		panic("pc_setcluster");
	fsp->pcfs_flags |= PCFS_FATMOD;
#ifdef PCFS_ENABLE_CACHE
	switch (fsp->pcfs_fattype) {
	case FAT32:
		offset = cn << 2;
		unitp = pc_fatcachesearch(fsp, offset, &error);
		if (unitp == NULL)
			return (error);
		fp = unitp->fatp + (offset - unitp->offset);
		hfv = ltohi(*(pc_cluster32_t *)fp);
		hfv = (hfv & PCFS_CLUSTER_RESERVEDMASK) |
		    (ncn & ~PCFS_CLUSTER_RESERVEDMASK);
		*(pc_cluster32_t *)fp = htoli(hfv);
		unitp->dirty = 1;
		break;
	case FAT16:
		offset = cn << 1;
		unitp = pc_fatcachesearch(fsp, offset, &error);
		if (unitp == NULL)
			return (error);
		fp = unitp->fatp + (offset - unitp->offset);
		unitp->dirty = 1;
		ncn16 = (pc_cluster16_t)ncn;
		*(pc_cluster16_t *)fp = htols(ncn16);
		break;
	case FAT12:
		offset = cn + (cn >> 1);
		unitp = pc_fatcachesearch(fsp, offset, &error);
		if (unitp == NULL)
			return (error);
		fp = unitp->fatp + (offset - unitp->offset);
		unitp->dirty = 1;
		if (cn & 01) {
			*fp = (*fp & 0x0f) | ((ncn << 4) & 0xf0);
			fp++;
			*fp = (ncn >> 4) & 0xff;
		} else {
			*fp++ = ncn & 0xff;
			*fp = (*fp & 0xf0) | ((ncn >> 8) & 0x0f);
		}
		break;
#else
	pc_mark_fat_updated(fsp, cn);
	switch (fsp->pcfs_fattype) {
	case FAT32:
		fp = fsp->pcfs_fatp + (cn << 2);
		*(pc_cluster32_t *)fp = htoli(ncn);
		break;
	case FAT16:
		fp = fsp->pcfs_fatp + (cn << 1);
		ncn16 = (pc_cluster16_t)ncn;
		*(pc_cluster16_t *)fp = htols(ncn16);
		break;
	case FAT12:
		fp = fsp->pcfs_fatp + (cn + (cn >> 1));
		if (cn & 01) {
			*fp = (*fp & 0x0f) | ((ncn << 4) & 0xf0);
			fp++;
			*fp = (ncn >> 4) & 0xff;
		} else {
			*fp++ = ncn & 0xff;
			*fp = (*fp & 0xf0) | ((ncn >> 8) & 0x0f);
		}
		break;
#endif	/* PCFS_ENABLE_CACHE */
	default:
		pc_mark_irrecov(fsp);
	}
	if (ncn == PCF_FREECLUSTER) {
		fsp->pcfs_nxfrecls = PCF_FIRSTCLUSTER;
		if (IS_FAT32(fsp)) {
			if (fsp->pcfs_fsinfo.fs_free_clusters !=
			    FSINFO_UNKNOWN)
				fsp->pcfs_fsinfo.fs_free_clusters++;
		}
	}
	return (0);
}

/*
 * Allocate a new cluster.
 */
int
pc_alloccluster(
	struct pcfs *fsp,	/* file sys to allocate in */
	int zwrite,			/* boolean for writing zeroes */
	pc_cluster32_t *ncn)
{
	pc_cluster32_t cn;
	int	error;
	pc_cluster32_t chkcl;

	if (fsp->pcfs_fatp == (uchar_t *)0)
		panic("pc_addcluster: no FAT");

	for (cn = fsp->pcfs_nxfrecls;
	    (int)cn < PCF_MAXCLUSTER(fsp); cn++) {
		if (pc_getcluster(fsp, cn, &chkcl))
			return (EIO);

		if (chkcl == PCF_FREECLUSTER) {
			struct buf *bp;

			if (IS_FAT32(fsp)) {
				error = pc_setcluster(fsp, cn,
				    PCF_LASTCLUSTERMARK32);
				if (error)
					return (error);
				if (fsp->pcfs_fsinfo.fs_free_clusters !=
				    FSINFO_UNKNOWN)
					fsp->pcfs_fsinfo.fs_free_clusters--;
			} else {
				error = pc_setcluster(fsp, cn,
				    PCF_LASTCLUSTERMARK);
				if (error)
					return (error);
			}
			if (zwrite) {
				/*
				 * zero the new cluster
				 */
				bp = getblk(fsp->pcfs_xdev,
				    pc_cldaddr(fsp, cn), fsp->pcfs_clsize);
				clrbuf(bp);
				bwrite2(bp);
				error = geterror(bp);
				brelse(bp);
				if (error) {
					pc_mark_irrecov(fsp);
					*ncn = PCF_ERRORCLUSTER;
					return (0);
				}
			}
			fsp->pcfs_nxfrecls = cn + 1;
			*ncn = cn;
			return (0);
		}
	}
	*ncn = PCF_FREECLUSTER;
	return (0);
}

/*
 * Get the number of clusters used by a file or subdirectory
 */
int
pc_fileclsize(
	struct pcfs *fsp,
	pc_cluster32_t startcl, pc_cluster32_t *ncl)
{
	int count = 0;
	int error;

	*ncl = 0;
	for (count = 0; pc_validcl(fsp, startcl); ) {
		if (count++ >= fsp->pcfs_ncluster)
			return (EIO);
		error = pc_getcluster(fsp, startcl, &startcl);
		if (error)
			return (error);
	}
	*ncl = (pc_cluster32_t)count;

	return (0);
}
