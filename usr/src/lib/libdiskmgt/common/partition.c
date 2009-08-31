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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Copyright (c) 2007-2008 NEC Corporation
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <fcntl.h>
#include <libdevinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sunddi.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/dkio.h>

#include "libdiskmgt.h"
#include "disks_private.h"
#include "partition.h"

#ifdef sparc
#define	les(val)	((((val)&0xFF)<<8)|(((val)>>8)&0xFF))
#define	lel(val)	(((unsigned)(les((val)&0x0000FFFF))<<16) | \
			    (les((unsigned)((val)&0xffff0000)>>16)))
#else
#define	les(val)	(val)
#define	lel(val)	(val)
#endif

#define	ISIZE		FD_NUMPART * sizeof (struct ipart)

#if (defined(_EXTFDISK_PARTITION) && (_EXTFDISK_PARTITION > 0))
#define _DM_EXT_FDISK
#define	EXTFDISK_RETURN(ret, ptr) { \
	if (ptr != NULL) { \
		free(ptr); \
	} \
	return (ret); \
}
#define	_DM_EXTNUM	_EXTFDISK_PARTITION
#else
#define	EXTFDISK_RETURN(ret, ptr)	return (ret)
#define	_DM_EXTNUM	0
#endif

static int	desc_ok(descriptor_t *dp);
static int	get_attrs(descriptor_t *dp, struct ipart *iparts,
		    nvlist_t *attrs);
static int	get_parts(disk_t *disk, struct ipart *iparts, char *opath,
		    int opath_len);
static int	open_disk(disk_t *diskp, char *opath, int len);
static int	has_slices(descriptor_t *desc, int *errp);

descriptor_t **
partition_get_assoc_descriptors(descriptor_t *desc, dm_desc_type_t type,
    int *errp)
{
	if (!desc_ok(desc)) {
	    *errp = ENODEV;
	    return (NULL);
	}

	switch (type) {
	case DM_MEDIA:
	    return (media_get_assocs(desc, errp));
	case DM_SLICE:
	    if (!has_slices(desc, errp)) {
		if (*errp != 0) {
		    return (NULL);
		}
		return (libdiskmgt_empty_desc_array(errp));
	    }
	    return (slice_get_assocs(desc, errp));
	}

	*errp = EINVAL;
	return (NULL);
}

/*
 * This is called by media/slice to get the associated partitions.
 * For a media desc. we just get all the partitions, but for a slice desc.
 * we just get the active solaris partition.
 */
descriptor_t **
partition_get_assocs(descriptor_t *desc, int *errp)
{
	descriptor_t	**partitions;
	int		pos;
	int		i;
#ifndef _DM_EXT_FDISK
	struct ipart	iparts[FD_NUMPART];
#else
	struct ipart	*iparts = NULL;
#endif
	char		pname[MAXPATHLEN];
	int		conv_flag = 0;
#if defined(i386) || defined(__amd64) || defined(__arm)
	int		len;
#endif

#ifdef _DM_EXT_FDISK
	iparts =
	    (struct ipart*) calloc((FD_NUMPART + _EXTFDISK_PARTITION),
	    sizeof (struct ipart));
	if(iparts == NULL) {
	    *errp = ENOMEM;
	    return (NULL);
	}
#endif
	if (get_parts(desc->p.disk, iparts, pname, sizeof (pname)) != 0) {
	    EXTFDISK_RETURN(libdiskmgt_empty_desc_array(errp), iparts);
	}

	/* allocate the array for the descriptors */
	partitions = (descriptor_t **)calloc(FD_NUMPART + 1 + _DM_EXTNUM,
	    sizeof (descriptor_t *));
	if (partitions == NULL) {
	    *errp = ENOMEM;
	    EXTFDISK_RETURN(NULL, iparts);
	}

#if defined(i386) || defined(__amd64) || defined(__arm)
	    /* convert part. name (e.g. c0d0p0) */
	    len = strlen(pname);
	    if (len > 1 && *(pname + (len - 2)) == 'p') {
		conv_flag = 1;
		*(pname + (len - 1)) = 0;
	    }
#endif

	/*
	 * If this is a slice desc. we need the first active solaris partition
	 * and if there isn't one then we need the first solaris partition.
	 */
	if (desc->type == DM_SLICE) {
	    for (i = 0; i < FD_NUMPART + _DM_EXTNUM; i++) {
		if (iparts[i].bootid == ACTIVE &&
		    (iparts[i].systid == SUNIXOS ||
		    iparts[i].systid == SUNIXOS2)) {
			break;
		}
	    }

	    /* no active solaris part., try to get the first solaris part. */
	    if (i >= (FD_NUMPART + _DM_EXTNUM)) {
		for (i = 0; i < FD_NUMPART + _DM_EXTNUM; i++) {
		    if (iparts[i].systid == SUNIXOS ||
			iparts[i].systid == SUNIXOS2) {
			    break;
		    }
		}
	    }

	    if (i < FD_NUMPART + _DM_EXTNUM) {
		/* we found a solaris partition to use */
		char	part_name[MAXPATHLEN];

		if (conv_flag) {
		    /* convert part. name (e.g. c0d0p0) */
		    if (_DM_EXTNUM && (i >= FD_NUMPART)) {
			*(pname + (len - 2)) = 'l';
			(void) snprintf(part_name, sizeof (part_name), "%s%d",
			    pname, i - FD_NUMPART + 1);
		    } else {
			(void) snprintf(part_name, sizeof (part_name), "%s%d",
			    pname, i);
		    }
		} else {
		    (void) snprintf(part_name, sizeof (part_name), "%d", i);
		}

		/* the media name comes from the slice desc. */
		partitions[0] = cache_get_desc(DM_PARTITION, desc->p.disk,
		    part_name, desc->secondary_name, errp);
		if (*errp != 0) {
		    cache_free_descriptors(partitions);
		    EXTFDISK_RETURN(NULL, iparts);
		}
		partitions[1] = NULL;

		EXTFDISK_RETURN(partitions, iparts);

	    }

	    EXTFDISK_RETURN(libdiskmgt_empty_desc_array(errp), iparts);
	}

	/* Must be for media, so get all the parts. */

	pos = 0;
	for (i = 0; i < FD_NUMPART + _DM_EXTNUM; i++) {
	    if (iparts[i].systid != 0) {
		char	part_name[MAXPATHLEN];

		if (conv_flag) {
		    /* convert part. name (e.g. c0d0p0) */
		    if (_DM_EXTNUM && (i >= FD_NUMPART)) {
			*(pname + (len - 2)) = 'l';
			(void) snprintf(part_name, sizeof (part_name), "%s%d",
			    pname, i - FD_NUMPART + 1);
		    } else {
			(void) snprintf(part_name, sizeof (part_name), "%s%d",
			    pname, i);
		    }
		} else {
		    (void) snprintf(part_name, sizeof (part_name), "%d", i);
		}

		/* the media name comes from the media desc. */
		partitions[pos] = cache_get_desc(DM_PARTITION, desc->p.disk,
		    part_name, desc->name, errp);
		if (*errp != 0) {
		    cache_free_descriptors(partitions);
		    EXTFDISK_RETURN(NULL, iparts);
		}

		pos++;
	    }
	}
	partitions[pos] = NULL;

	*errp = 0;
	EXTFDISK_RETURN(partitions, iparts);
}

nvlist_t *
partition_get_attributes(descriptor_t *dp, int *errp)
{
	nvlist_t	*attrs = NULL;
#ifndef _DM_EXT_FDISK
	struct ipart	iparts[FD_NUMPART];
#else
	struct ipart	*iparts = NULL;
#endif

	if (!desc_ok(dp)) {
	    *errp = ENODEV;
	    return (NULL);
	}

#ifdef _DM_EXT_FDISK
	iparts =
	    (struct ipart*) calloc((FD_NUMPART + _EXTFDISK_PARTITION),
	    sizeof (struct ipart));
	if(iparts == NULL) {
	    *errp = ENOMEM;
	    return (NULL);
	}
#endif
	if ((*errp = get_parts(dp->p.disk, iparts, NULL, 0)) != 0) {
	    EXTFDISK_RETURN(NULL, iparts);
	}

	if (nvlist_alloc(&attrs, NVATTRS, 0) != 0) {
	    *errp = ENOMEM;
	    EXTFDISK_RETURN(NULL, iparts);
	}

	if ((*errp = get_attrs(dp, iparts, attrs)) != 0) {
	    nvlist_free(attrs);
	    attrs = NULL;
	}

	EXTFDISK_RETURN(attrs, iparts);
}

/*
 * Look for the partition by the partition number (which is not too useful).
 */
descriptor_t *
partition_get_descriptor_by_name(char *name, int *errp)
{
	descriptor_t	**partitions;
	int		i;
	descriptor_t	*partition = NULL;

	partitions = cache_get_descriptors(DM_PARTITION, errp);
	if (*errp != 0) {
	    return (NULL);
	}

	for (i = 0; partitions[i]; i++) {
	    if (libdiskmgt_str_eq(name, partitions[i]->name)) {
		partition = partitions[i];
	    } else {
		/* clean up the unused descriptors */
		cache_free_descriptor(partitions[i]);
	    }
	}
	free(partitions);

	if (partition == NULL) {
	    *errp = ENODEV;
	}

	return (partition);
}

/* ARGSUSED */
descriptor_t **
partition_get_descriptors(int filter[], int *errp)
{
	return (cache_get_descriptors(DM_PARTITION, errp));
}

char *
partition_get_name(descriptor_t *desc)
{
	return (desc->name);
}

/* ARGSUSED */
nvlist_t *
partition_get_stats(descriptor_t *dp, int stat_type, int *errp)
{
	/* There are no stat types defined for partitions */
	*errp = EINVAL;
	return (NULL);
}

/* ARGSUSED */
int
partition_has_fdisk(disk_t *dp, int fd)
{
	char		bootsect[512 * 3]; /* 3 sectors to be safe */

#ifdef sparc
	if (dp->drv_type == DM_DT_FIXED) {
	    /* on sparc, only removable media can have fdisk parts. */
	    return (0);
	}
#endif

	/*
	 * We assume the caller already made sure media was inserted and
	 * spun up.
	 */

	if ((ioctl(fd, DKIOCGMBOOT, bootsect) < 0) && (errno != ENOTTY)) {
	    return (0);
	}

	return (1);
}

/*
 * A partition descriptor points to a disk, the name is the partition number
 * and the secondary name is the media name.
 */
int
partition_make_descriptors()
{
	int		error;
	disk_t		*dp;
#ifndef _DM_EXT_FDISK
	struct ipart	iparts[FD_NUMPART];
#else
	struct ipart	*iparts = NULL;

	iparts =
	    (struct ipart*) calloc((FD_NUMPART + _EXTFDISK_PARTITION),
	    sizeof (struct ipart));
	if(iparts == NULL) {
	    return (ENOMEM);
	}
#endif

	dp = cache_get_disklist();
	while (dp != NULL) {
	    char		pname[MAXPATHLEN];

	    if (get_parts(dp, iparts, pname, sizeof (pname)) == 0) {
		int	i;
		char	mname[MAXPATHLEN];
		int	conv_flag = 0;
#if defined(i386) || defined(__amd64) || defined(__arm)
		/* convert part. name (e.g. c0d0p0) */
		int	len;

		len = strlen(pname);
		if (len > 1 && *(pname + (len - 2)) == 'p') {
		    conv_flag = 1;
		    *(pname + (len - 1)) = 0;
		}
#endif

		mname[0] = 0;
		(void) media_read_name(dp, mname, sizeof (mname));

		for (i = 0; i < FD_NUMPART + _DM_EXTNUM; i++) {
		    if (iparts[i].systid != 0) {
			char	part_name[MAXPATHLEN];

			if (conv_flag) {
			    /* convert part. name (e.g. c0d0p0) */
			    if (_DM_EXTNUM && (i >= FD_NUMPART)) {
				*(pname + (len - 2)) = 'l';
				(void) snprintf(part_name, sizeof (part_name),
				    "%s%d", pname, i - FD_NUMPART + 1);
			    } else {
				(void) snprintf(part_name, sizeof (part_name),
				    "%s%d", pname, i);
			    }
			} else {
			    (void) snprintf(part_name, sizeof (part_name),
				"%d", i);
			}

			cache_load_desc(DM_PARTITION, dp, part_name, mname,
			    &error);
			if (error != 0) {
			    EXTFDISK_RETURN(error, iparts);
			}
		    }
		}
#ifdef _DM_EXT_FDISK
		memset (iparts, 0x0,
		    (FD_NUMPART + _EXTFDISK_PARTITION) * sizeof (struct ipart));
#endif
	    }
	    dp = dp->next;
	}

	EXTFDISK_RETURN(0, iparts);
}

static int
get_attrs(descriptor_t *dp, struct ipart *iparts, nvlist_t *attrs)
{
	char		*p;
	int		part_num;
#ifdef _DM_EXT_FDISK
	int		extend = 0;
	int		i;
#endif

	/*
	 * We already made sure the media was loaded and ready in the
	 * get_parts call within partition_get_attributes.
	 */

#ifndef _DM_EXT_FDISK
	p = strrchr(dp->name, 'p');
	if (p == NULL) {
	    p = dp->name;
	} else {
	    p++;
	}
	part_num = atoi(p);
#else
	p = strrchr(dp->name, 'p');
	if (p != NULL) {
		p++;
	} else {
		p = strrchr(dp->name, 'l');
		if (p == NULL) {
			p = dp->name;
		} else {
			p++;
			extend = 1;
		}
	}

	part_num = atoi(p);
	if (extend) {
	    if (part_num == 0) {
		/* if cXtXdXl0 */
		for (i = 0; i < FD_NUMPART; i++) {
		    if (iparts[i].systid == EXTDOS ||
			iparts[i].systid == FDISK_EXTLBA ) {
			    part_num = i;
			    break;
		    }
		}
		if (i >= FD_NUMPART) {
		    return (ENODEV);
		}
			
	    } else {
		    part_num += FD_NUMPART - 1;
	    }
	}
#endif
	if (part_num >= FD_NUMPART + _DM_EXTNUM ||
	    iparts[part_num].systid == 0) {
	    return (ENODEV);
	}

	/* we found the partition */

	if (nvlist_add_uint32(attrs, DM_BOOTID,
	    (unsigned int)iparts[part_num].bootid) != 0) {
	    return (ENOMEM);
	}

	if (nvlist_add_uint32(attrs, DM_PTYPE,
	    (unsigned int)iparts[part_num].systid) != 0) {
	    return (ENOMEM);
	}

	if (nvlist_add_uint32(attrs, DM_BHEAD,
	    (unsigned int)iparts[part_num].beghead) != 0) {
	    return (ENOMEM);
	}

	if (nvlist_add_uint32(attrs, DM_BSECT,
	    (unsigned int)((iparts[part_num].begsect) & 0x3f)) != 0) {
	    return (ENOMEM);
	}

	if (nvlist_add_uint32(attrs, DM_BCYL, (unsigned int)
	    ((iparts[part_num].begcyl & 0xff) |
	    ((iparts[part_num].begsect & 0xc0) << 2))) != 0) {
	    return (ENOMEM);
	}

	if (nvlist_add_uint32(attrs, DM_EHEAD,
	    (unsigned int)iparts[part_num].endhead) != 0) {
	    return (ENOMEM);
	}

	if (nvlist_add_uint32(attrs, DM_ESECT,
	    (unsigned int)((iparts[part_num].endsect) & 0x3f)) != 0) {
	    return (ENOMEM);
	}

	if (nvlist_add_uint32(attrs, DM_ECYL, (unsigned int)
	    ((iparts[part_num].endcyl & 0xff) |
	    ((iparts[part_num].endsect & 0xc0) << 2))) != 0) {
	    return (ENOMEM);
	}

	if (nvlist_add_uint32(attrs, DM_RELSECT,
	    (unsigned int)iparts[part_num].relsect) != 0) {
	    return (ENOMEM);
	}

	if (nvlist_add_uint32(attrs, DM_NSECTORS,
	    (unsigned int)iparts[part_num].numsect) != 0) {
	    return (ENOMEM);
	}

	return (0);
}

static int
get_parts(disk_t *disk, struct ipart *iparts, char *opath, int opath_len)
{
	int		fd;
	struct dk_minfo	minfo;
	struct mboot	bootblk;
	char		bootsect[512];
	int		i;
#ifdef _DM_EXT_FDISK
	int		extendpart = -1, offset_ext = 0, offset_ebr = 0;
	struct extboot	*extBootCod = NULL;
#endif

	/* Can't use drive_open_disk since we need the partition dev name. */
	if ((fd = open_disk(disk, opath, opath_len)) < 0) {
	    return (ENODEV);
	}

	/* First make sure media is inserted and spun up. */
	if (!media_read_info(fd, &minfo)) {
	    (void) close(fd);
	    return (ENODEV);
	}

	if (!partition_has_fdisk(disk, fd)) {
	    (void) close(fd);
	    return (ENOTTY);
	}

	if (lseek(fd, 0, 0) == -1) {
	    (void) close(fd);
	    return (ENODEV);
	}

	if (read(fd, bootsect, 512) != 512) {
	    (void) close(fd);
	    return (ENODEV);
	}
#ifndef _DM_EXT_FDISK
	(void) close(fd);
#endif

	(void) memcpy(&bootblk, bootsect, sizeof (bootblk));

	if (les(bootblk.signature) != MBB_MAGIC)  {
#ifdef _DM_EXT_FDISK
	    (void) close(fd);
#endif
	    return (ENOTTY);
	}

	(void) memcpy(iparts, bootblk.parts, ISIZE);

#ifndef _DM_EXT_FDISK
	for (i = 0; i < FD_NUMPART; i++) {
	    if (iparts[i].systid != 0) {
		iparts[i].relsect = lel(iparts[i].relsect);
		iparts[i].numsect = lel(iparts[i].numsect);
	    }
	}
#else
	for (i = 0; i < FD_NUMPART; i++) {
	    if (iparts[i].systid != 0) {
		iparts[i].relsect = lel(iparts[i].relsect);
		iparts[i].numsect = lel(iparts[i].numsect);

		if (extendpart < 0 && ((iparts[i].systid == EXTDOS) ||
		    (iparts[i].systid == FDISK_EXTLBA))) {
		    extendpart = i;
		    offset_ext = iparts[i].relsect;
		    break;
		}
	    }
	}

	if(extendpart < 0) {
	    (void) close(fd);
	    return (0);
	}
	
	extBootCod =
	    (struct extboot *)calloc((_EXTFDISK_PARTITION + 1),
	    sizeof (struct extboot));
	if (extBootCod == NULL) {
		(void) close(fd);
		return (ENOMEM);
	}
	if ((ioctl(fd, DKIOCGEBR, extBootCod) < 0) && (errno != ENOTTY)) {
	    free(extBootCod);
	    (void) close(fd);
	    return (ENODEV);
	}

	for (i = 1; i < _EXTFDISK_PARTITION + 1; i++) {
	    struct ipart tmp_part[FD_NUMPART];

	    memcpy(&iparts[FD_NUMPART + i - 1], extBootCod[i].parts,
		sizeof (struct ipart));

	    iparts[FD_NUMPART + i - 1].relsect =
		lel(iparts[FD_NUMPART + i - 1].relsect) +
		offset_ext + offset_ebr;
	    iparts[FD_NUMPART + i - 1].numsect =
		lel(iparts[FD_NUMPART + i - 1].numsect);

	    memcpy(tmp_part, extBootCod[i].parts,
		sizeof (struct ipart) * FD_NUMPART);
	    if (tmp_part[1].systid == EXTDOS ||
	        tmp_part[1].systid == FDISK_EXTLBA) {
		offset_ebr = lel(tmp_part[1].relsect);
		continue;
	    }
	    break;
	}

	free(extBootCod);
	(void) close(fd);

#endif /* !_DM_EXT_FDISK */

	return (0);
}
/* return 1 if the partition descriptor is still valid, 0 if not. */
static int
desc_ok(descriptor_t *dp)
{
	/* First verify the media name for removable media */
	if (dp->p.disk->removable) {
	    char	mname[MAXPATHLEN];

	    if (!media_read_name(dp->p.disk, mname, sizeof (mname))) {
		return (0);
	    }

	    if (mname[0] == 0) {
		return (libdiskmgt_str_eq(dp->secondary_name, NULL));
	    } else {
		return (libdiskmgt_str_eq(dp->secondary_name, mname));
	    }
	}

	/*
	 * We could verify the partition is still there but this is kind of
	 * expensive and other code down the line will do that (e.g. see
	 * get_attrs).
	 */

	return (1);
}

/*
 * Return 1 if partition has slices, 0 if not.
 */
static int
has_slices(descriptor_t *desc, int *errp)
{
	int		pnum;
	int		i;
	char		*p;
#ifndef _DM_EXT_FDISK
	struct ipart	iparts[FD_NUMPART];
#else
	struct ipart	*iparts = NULL;
	int		extend = 0;

	iparts =
	    (struct ipart*) calloc((FD_NUMPART + _EXTFDISK_PARTITION),
	    sizeof (struct ipart));
	if(iparts == NULL) {
	    *errp = ENOMEM;
	    return (0);
	}
#endif

	if (get_parts(desc->p.disk, iparts, NULL, 0) != 0) {
	    *errp = ENODEV;
	    EXTFDISK_RETURN(0, iparts);
	}

	p = strrchr(desc->name, 'p');
	if (p == NULL) {
#ifndef _DM_EXT_FDISK
	    p = desc->name;
#else
	    p = strrchr(desc->name, 'l');
	    if (p == NULL) {
		p = desc->name;
	    } else {
		p++;
		extend = 1;
	    }
#endif
	} else {
	    p++;
	}
	pnum = atoi(p);
#ifdef _DM_EXT_FDISK
	if (extend) {
	    if (pnum == 0) {
		*errp = 0;
		free(iparts);
		return (0);
	    }
	    pnum += FD_NUMPART -1;
	}
#endif

	/*
	 * Slices are associated with the active solaris partition or if there
	 * is no active solaris partition, then the first solaris partition.
	 */

	*errp = 0;
	if (iparts[pnum].bootid == ACTIVE &&
	    (iparts[pnum].systid == SUNIXOS ||
	    iparts[pnum].systid == SUNIXOS2)) {
		EXTFDISK_RETURN(1, iparts);
	} else {
	    int	active = 0;

	    /* Check if there are no active solaris partitions. */
	    for (i = 0; i < FD_NUMPART + _DM_EXTNUM; i++) {
		if (iparts[i].bootid == ACTIVE &&
		    (iparts[i].systid == SUNIXOS ||
		    iparts[i].systid == SUNIXOS2)) {
			active = 1;
			break;
		}
	    }

	    if (!active) {
		/* Check if this is the first solaris partition. */
		for (i = 0; i < FD_NUMPART + _DM_EXTNUM; i++) {
		    if (iparts[i].systid == SUNIXOS ||
			iparts[i].systid == SUNIXOS2) {
			    break;
		    }
		}

		if (i < (FD_NUMPART + _DM_EXTNUM) && i == pnum) {
		    EXTFDISK_RETURN(1, iparts);
		}
	    }
	}

	EXTFDISK_RETURN(0, iparts);
}

static int
open_disk(disk_t *diskp, char *opath, int len)
{
	/*
	 * Just open the first devpath.
	 */
	if (diskp->aliases != NULL && diskp->aliases->devpaths != NULL) {
#ifdef sparc
	    if (opath != NULL) {
		(void) strlcpy(opath, diskp->aliases->devpaths->devpath, len);
	    }
	    return (open(diskp->aliases->devpaths->devpath, O_RDONLY|O_NDELAY));
#else /* !sparc */
	    /* On intel we need to open partition device (e.g. c0d0p0). */
	    char	part_dev[MAXPATHLEN];
	    char	*p;

	    (void) strlcpy(part_dev, diskp->aliases->devpaths->devpath,
		sizeof (part_dev));
	    p = strrchr(part_dev, '/');
	    if (p == NULL) {
		p = strrchr(part_dev, 's');
		if (p != NULL) {
		    *p = 'p';
		}
#ifdef _DM_EXT_FDISK
		else if((p = strrchr(part_dev, 'l')) != NULL) {
		    *p = 'p';
		}
#endif
	    } else {
		char *ps;

		*p = 0;
		ps = strrchr((p + 1), 's');
		if (ps != NULL) {
		    *ps = 'p';
		}
#ifdef _DM_EXT_FDISK
		else if((ps = strrchr((p + 1), 'l')) != NULL) {
		    *ps = 'p';
		}
#endif
		*p = '/';
	    }

	    if (opath != NULL) {
		(void) strlcpy(opath, part_dev, len);
	    }
	    return (open(part_dev, O_RDONLY|O_NDELAY));
#endif /* sparc */
	}

	return (-1);
}

#ifdef _DM_EXT_FDISK
/* 
 * This function judge whether it is extended partition. If device is
 * extended partition, 1 is returned. Otherwise 0 is returned. 
 * When error occurs, negative integer is returned.
 */
int
is_extended_partition(char *dev_name, char **msg, int *errp) {
	char		*dname = NULL;
	char		*p, *s;
	char		*use = "%s is currently used by Extended partition. "
			       "Please see fdisk(1M).\n";
	int		fd, i, len0, len1;
	struct mboot	bootblk;
	struct ipart	*iparts;

	*errp = 0;

	/* If extended partition(e.g. c0d0l0) check logical partition */
	p = strrchr(dev_name, 'l');
	if (p != NULL) {
		if (*(p + 1) == '0') {
		    goto found;
		}
	}

	/* If extended partition(e.g. c0d0pX) check logical partition */
	p = strrchr(dev_name, 'p');
	if (p == NULL) {
		return (0);
	}

found:
	dname = getfullrawname(dev_name);
	if (dname == NULL || *dname == '\0') {
	    return (0);
	}

	if ((s = strrchr(dname, 'p')) != NULL) {
	    *(s + 1) = '0';
	} else if ((s = strrchr(dname, 'l'))){
	    *s = 'p';
	}

	if ((fd = open(dname, O_RDONLY|O_NDELAY)) < 0) {
	    *errp = ENODEV;
	    return (-1);
	}

	if (lseek(fd, 0, 0) == -1) {
	    (void) close(fd);
	    *errp = ENODEV;
	    return (-1);
	}

	if (read(fd, &bootblk, sizeof(bootblk)) != sizeof(bootblk)) {
	    (void) close(fd);
	    *errp = ENODEV;
	    return (-1);
	}

	if (les(bootblk.signature) != MBB_MAGIC)  {
	    (void) close(fd);
	    return (0);
	}

	iparts = (struct ipart *)&bootblk.parts[0];	
	for (i = 0; i < FD_NUMPART; i++, iparts++) {
	    if ((iparts->systid == EXTDOS) ||
		(iparts->systid == FDISK_EXTLBA)) {
		break;
	    }
	}
	if (i == FD_NUMPART) {
	    /* Not Found Extended Partition */
	    (void) close(fd);
	    return (0);
	}

	if (*p == 'p') {
	    if (atoi((p + 1)) != (i + 1)) {
		(void) close(fd);
		return (0);
	    }
	}

	/* Check logical partition */
	if (lseek(fd, (les(iparts->relsect) * sizeof(bootblk)), 0) == -1) {
	    (void) close(fd);
	    *errp = ENODEV;
	    return (-1);
	}

	if (read(fd, &bootblk, sizeof(bootblk)) != sizeof(bootblk)) {
	    (void) close(fd);
	    *errp = ENODEV;
	    return (-1);
	}

	(void) close(fd);

	if (les(bootblk.signature) != MBB_MAGIC) {
	    /* No logical partition exists. */
	    return (0);
	}

	/* Chaeck partition Table */
	iparts = (struct ipart *)&bootblk.parts[0];	
	if (((iparts->systid == UNUSED) || (iparts->systid == 0x0))){
	    iparts++;
	    if(!(iparts->systid == EXTDOS) &&
		!(iparts->systid == FDISK_EXTLBA)) {
			return(0);
	    }
	}

	/* Logical prtition exists. */
	if (*msg)
		len0 = strlen(*msg);
	else
		len0 = 0;

	dname = getfullblkname(dev_name);
	if (dname == NULL || *dname == '\0') {
		dname = dev_name;
	}

	len1 = snprintf(NULL, 0, use, dname);
	if ((p = realloc(*msg, len0 + len1 + 1)) == NULL) {
		*errp = errno;
		free(*msg);
		return (-1);
	}
	*msg = p;

	(void) snprintf(*msg + len0, len1 + 1, use, dname);
	return (1);
}
#endif /* _DM_EXT_FDISK */
