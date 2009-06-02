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

#include <stdio.h>
#include <string.h>
#include <sys/elf_ARM.h>
#include <debug.h>
#include <dwarf.h>
#include "msg.h"
#include "_libld.h"

/*
 * SHT_ARM_ATTRIBUTES support codes.
 */

#define	LSB32ENCODE(ptr, value)				\
	do {						\
		uchar_t	*__ptr = (uchar_t *)(ptr);	\
							\
		*__ptr = (value) & 0xff;		\
		*(__ptr + 1) = ((value) >> 8) & 0xff;	\
		*(__ptr + 2) = ((value) >> 16) & 0xff;	\
		*(__ptr + 3) = ((value) >> 24) & 0xff;	\
	} while (0)

#define	ARM_ATTR_VERSION		'A'
#define	ARM_ATTR_SECTION_SIZE_LEN	4
#define	ARM_ATTR_VENDOR_NAME		"aeabi"
#define	ARM_ATTR_VENDOR_NAME_LEN	6	/* strlen("aeabi") + 1 */

/* Type of attribute value */
typedef enum {
	ARM_ATYPE_INT		= 1,	/* integer */
	ARM_ATYPE_STRING		/* string */
} arm_attr_type_t;

/* Top level structure tags */
#define	ARM_ATTR_TAG_FILE		1

/* Supported tags in file attribute section */
#define	AFTAG_CPU_RAW_NAME		4
#define	AFTAG_CPU_NAME			5
#define	AFTAG_CPU_ARCH			6
#define	AFTAG_CPU_ARCH_PROFILE		7
#define	AFTAG_ARM_ISA_USE		8
#define	AFTAG_THUMB_ISA_USE		9
#define	AFTAG_VFP_ARCH			10
#define	AFTAG_WMMX_ARCH			11
#define	AFTAG_NEON_ARCH			12
#define	AFTAG_PCS_CONFIG		13
#define	AFTAG_ABI_PCS_R9_USE		14
#define	AFTAG_ABI_PCS_RW_DATA		15
#define	AFTAG_ABI_PCS_RO_DATA		16
#define	AFTAG_ABI_PCS_GOT_USE		17
#define	AFTAG_ABI_PCS_WCHAR_T		18
#define	AFTAG_ABI_FP_ROUNDING		19
#define	AFTAG_ABI_FP_DENORMAL		20
#define	AFTAG_ABI_FP_EXCEPTIONS		21
#define	AFTAG_ABI_FP_USER_EXCEPTIONS	22
#define	AFTAG_ABI_FP_NUMBER_MODEL	23
#define	AFTAG_ABI_ALIGN8_NEEDED		24
#define	AFTAG_ABI_ALIGN8_PRESERVED	25
#define	AFTAG_ABI_ENUM_SIZE		26
#define	AFTAG_ABI_HARDFP_USE		27
#define	AFTAG_ABI_VFP_ARGS		28
#define	AFTAG_ABI_WMMX_ARGS		29
#define	AFTAG_ABI_OPTIMIZATION_GOALS	30
#define	AFTAG_ABI_FP_OPTIMIZATION_GOALS	31

#define	ARM_ATTR_FTAG_MIN		AFTAG_CPU_RAW_NAME
#define	ARM_ATTR_FTAG_MAX		AFTAG_ABI_FP_OPTIMIZATION_GOALS
#define	ARM_ATTR_FTAG_NUM		(ARM_ATTR_FTAG_MAX + 1)

/* Value of file section */
typedef struct arm_fattr_val {
	arm_attr_type_t	afv_type;	/* data type */
	size_t		afv_encsize;	/* size of encoded value */
	union {
		uint32_t	i;
		char		*s;
	} afv_v;			/* data */
} arm_fattr_val_t;

#define	afv_integer	afv_v.i
#define	afv_string	afv_v.s

/* File section in ARM attributes */
typedef struct arm_fattr {
	arm_fattr_val_t	*af_tags[ARM_ATTR_FTAG_NUM];
	uint_t		af_ntags;	/* number of tags */
} arm_fattr_t;

#define	ARM_ATTR_FILE_INIT(fattr)				\
	do {							\
		int	__i;					\
								\
		for (__i = 0; __i < ARM_ATTR_FTAG_NUM; __i++) {	\
			(fattr)->af_tags[__i] = NULL;		\
		}						\
		(fattr)->af_ntags = 0;				\
	} while (0)

#define	ARM_ATTR_FTAG_IS_VALID(ftag)	\
	((ftag) >= ARM_ATTR_FTAG_MIN && (ftag) <= ARM_ATTR_FTAG_MAX)
#define	ARM_ATTR_FTAG_TYPE(ftag)					\
	(((ftag) == AFTAG_CPU_RAW_NAME || (ftag) == AFTAG_CPU_NAME)	\
	 ? ARM_ATYPE_STRING : ARM_ATYPE_INT)

/* Macro to merge CPU name */
#define	ARM_ATTR_FTAG_MERGE_CPUNAME(dst, src, tag)			\
	do {								\
		arm_fattr_val_t	*__sv = (src)->af_tags[tag];		\
		arm_fattr_val_t	*__dv = (dst)->af_tags[tag];		\
		arm_fattr_val_t	*__sarch =				\
			(src)->af_tags[AFTAG_CPU_ARCH];			\
		arm_fattr_val_t	*__darch =				\
			(dst)->af_tags[AFTAG_CPU_ARCH];			\
									\
		/*							\
		 * Copy source CPU name if:				\
		 * - CPU name is not yet set.				\
		 * - CPU architecture is not yet set.			\
		 * - Source architecture is greater than destination.	\
		 */							\
		if (__dv == NULL || __darch == NULL ||			\
		    (__sarch != NULL &&					\
		     __darch->afv_integer < __sarch->afv_integer)) {	\
			(dst)->af_tags[tag] = __sv;			\
		}							\
	} while (0)

/* Internal prototypes */
static uintptr_t	arm_fattr_read(Ofl_desc *ofl, Is_desc *isp,
				       arm_fattr_t *fattr);
static uintptr_t	arm_fattr_merge(Ofl_desc *ofl, Is_desc *isp,
					arm_fattr_t *dst, arm_fattr_t *src);
static size_t		arm_fattr_size(arm_fattr_t *fattr, size_t *lenp,
				       size_t *flenp);
static void		*arm_fattr_encode(arm_fattr_t *fattr, size_t *sizep);
static size_t		uleb_nbytes(uint_t value);
static uchar_t		*uleb_encode(uchar_t *buf, uint_t value);

/*
 * uintptr_t
 * ld_merge_attribute(Ofl_desc *ofl, Os_desc *osp)
 *	Merge SHT_ARM_ATTRIBUTES sections in all input files into one section.
 */
uintptr_t
ld_merge_attribute(Ofl_desc *ofl, Os_desc *osp)
{
	Is_desc		*isp;
	Listnode	*lnp;
	arm_fattr_t	fattr;
	size_t		size;
	void		*buf;
	Elf_Data	*data;

	assert(osp->os_shdr->sh_type == SHT_ARM_ATTRIBUTES);

	/* LINTED */
	ARM_ATTR_FILE_INIT(&fattr);

	/* Parse all attribute data, and merge them into fattr. */
	for (LIST_TRAVERSE(&(osp->os_isdescs), lnp, isp)) {
		arm_fattr_t	ifattr;

		/* Read file attributes in this section. */
		if (arm_fattr_read(ofl, isp, &ifattr) == S_ERROR) {
			return (S_ERROR);
		}

		/* Merge attributes. */
		if (arm_fattr_merge(ofl, isp, &fattr, &ifattr) == S_ERROR) {
			return (S_ERROR);
		}

		/*
		 * Set DISCARD flag to protect from duplication of
		 * this section.
		 */
		isp->is_flags |= FLG_IS_DISCARD;
	}

	/* Encode new ARM attributes. */
	if ((buf = arm_fattr_encode(&fattr, &size)) == NULL) {
		return (S_ERROR);
	}

	/* Create data for new SHT_ARM_ATTRIBUTES section. */
	if ((data = elf_newdata(osp->os_scn)) == NULL) {
		eprintf(ofl->ofl_lml, ERR_ELF, MSG_INTL(MSG_ELF_NEWDATA),
			ofl->ofl_name);
		return (S_ERROR);
	}
	data->d_buf = buf;
	data->d_size = size;
	data->d_off = 0;
	data->d_align = 1;

	return (1);
}

/*
 * static uintptr_t
 * arm_fattr_read(Ofl_desc *ofl, Is_desc *isp, arm_fattr_t *fattr)
 *	Read contents of SHT_ARM_ATTRIBUTES section.
 *	This function reads only file section, and put contents into the
 *	given arm_fattr.
 */
static uintptr_t
arm_fattr_read(Ofl_desc *ofl, Is_desc *isp, arm_fattr_t *fattr)
{
	uchar_t	*buf = isp->is_indata->d_buf;
	size_t	bufsize = isp->is_indata->d_size;

	/* LINTED */
	ARM_ATTR_FILE_INIT(fattr);

	/* Check attribute format version. */
	if (*buf != ARM_ATTR_VERSION) {
		eprintf(ofl->ofl_lml, ERR_FATAL, MSG_INTL(MSG_ATTR_BADVER),
			isp->is_file->ifl_name);
		ofl->ofl_flags |= FLG_OF_FATAL;
		return (S_ERROR);
	}

	buf++;
	bufsize--;

	/* Read attribute sections. */
	while (bufsize > 0) {
		size_t	secsize = LSB32EXTRACT(buf);
		size_t	hsize;
		char	*vendor;

		if (secsize > bufsize) {
			eprintf(ofl->ofl_lml, ERR_FATAL,
				MSG_INTL(MSG_ATTR_BADLEN),
				isp->is_file->ifl_name, secsize, bufsize);
			ofl->ofl_flags |= FLG_OF_FATAL;
			return (S_ERROR);
		}
		bufsize -= secsize;
		vendor = (char *)buf + ARM_ATTR_SECTION_SIZE_LEN;
		if (strcmp(vendor, ARM_ATTR_VENDOR_NAME) != 0) {
			/* Ignore unsupported vendor section. */
			continue;
		}
		hsize = ARM_ATTR_VENDOR_NAME_LEN + ARM_ATTR_SECTION_SIZE_LEN;
		if (hsize > secsize) {
			eprintf(ofl->ofl_lml, ERR_FATAL,
				MSG_INTL(MSG_ATTR_BADSUBLEN),
				isp->is_file->ifl_name, hsize, secsize);
			ofl->ofl_flags |= FLG_OF_FATAL;
			return (S_ERROR);
		}
		secsize -= hsize;
		buf += hsize;

		/* Read public ARM EABI section. */
		while (secsize > 0) {
			uint_t		tag;
			size_t		ssize;
			uint64_t	nb;
			uchar_t		*endp;

			nb = 0;
			tag = (uint_t)uleb_extract(buf, &nb);
			buf += nb;
			ssize = LSB32EXTRACT(buf);

			/* ssize contains size of tag field. */
			endp = buf + ssize - nb;
			buf += ARM_ATTR_SECTION_SIZE_LEN;

			if (ssize > secsize) {
				eprintf(ofl->ofl_lml, ERR_FATAL,
					MSG_INTL(MSG_ATTR_BADSUBLEN),
					isp->is_file->ifl_name, ssize,
					secsize);
				ofl->ofl_flags |= FLG_OF_FATAL;
				return (S_ERROR);
			}
			secsize -= ssize;

			if (tag != ARM_ATTR_TAG_FILE) {
				/* Ignore unsupported section. */
				continue;
			}

			/* Read all tag and value pairs. */
			while (buf < endp) {
				uint_t		ftag;
				arm_attr_type_t	type;
				arm_fattr_val_t	*fvp;

				nb = 0;
				ftag = (uint_t)uleb_extract(buf, &nb);

				if (!ARM_ATTR_FTAG_IS_VALID(ftag)) {
					/* Unknown attribute tag. */
					eprintf(ofl->ofl_lml, ERR_FATAL,
						MSG_INTL(MSG_ATTR_UNKFTAG),
						isp->is_file->ifl_name, ftag);
					ofl->ofl_flags |= FLG_OF_FATAL;
					return (S_ERROR);
				}

				if (fattr->af_tags[ftag] != NULL) {
					/* Duplicated attribute tag. */
					eprintf(ofl->ofl_lml, ERR_FATAL,
						MSG_INTL(MSG_ATTR_DUPFTAG),
						isp->is_file->ifl_name, ftag);
					ofl->ofl_flags |= FLG_OF_FATAL;
					return (S_ERROR);
				}

				if ((fvp = (arm_fattr_val_t *)
				     libld_malloc(sizeof(*fvp))) == NULL) {
					return (S_ERROR);
				}
				fattr->af_tags[ftag] = fvp;
				fattr->af_ntags++;

				type = ARM_ATTR_FTAG_TYPE(ftag);
				fvp->afv_type = type;
				if (type == ARM_ATYPE_INT) {
					uint32_t	v;
					uint64_t	b;

					/*
					 * Integer value is encoded in unsigned
					 * LEB128 format.
					 */
					b = nb;
					v = (uint32_t)uleb_extract(buf, &nb);
					fvp->afv_encsize = (size_t)(nb - b);
					fvp->afv_integer = v;
				}
				else {
					char	*v = (char *)buf + nb, *str;
					size_t	sz = strlen(v) + 1;

					/*
					 * String value is encoded in C style
					 * string.
					 */
					fvp->afv_encsize = sz;
					nb += sz;
					if ((str = libld_malloc(sz)) == NULL) {
						return (S_ERROR);
					}
					(void)memcpy(str, v, sz);
					fvp->afv_string = str;
				}

				buf += nb;
			}
		}
	}

	return (1);
}

/*
 * static uintptr_t
 * arm_fattr_merge(Ofl_desc *ofl, Is_desc *isp, arm_fattr_t *dst,
 *		   arm_fattr_t *src)
 *	Merge file attributes in the SHT_ARM_ATTRIBUTES section.
 *	1 is returned on success, S_ERROR on failure.
 */
static uintptr_t
arm_fattr_merge(Ofl_desc *ofl, Is_desc *isp, arm_fattr_t *dst,
		arm_fattr_t *src)
{
	uint_t		ftag;
	arm_fattr_val_t	*sv, *dv;

	if (dst->af_ntags == 0) {
		/* Copy source attributes. */
		if (src->af_ntags != 0) {
			*dst = *src;
		}
		return (1);
	}

	/* Merge CPU name. */
	/* LINTED */
	ARM_ATTR_FTAG_MERGE_CPUNAME(dst, src, AFTAG_CPU_NAME);
	/* LINTED */
	ARM_ATTR_FTAG_MERGE_CPUNAME(dst, src, AFTAG_CPU_RAW_NAME);

	/* Check whether CPU architecture profile is the same. */
	dv = dst->af_tags[AFTAG_CPU_ARCH_PROFILE];
	sv = src->af_tags[AFTAG_CPU_ARCH_PROFILE];
	if (dv == NULL) {
		dst->af_tags[AFTAG_CPU_ARCH_PROFILE] = sv;
	}
	else if (sv != NULL && sv->afv_integer != dv->afv_integer) {
		/* Architecture profile does not match. */
		eprintf(ofl->ofl_lml, ERR_FATAL, MSG_INTL(MSG_ATTR_CFLFTAG),
			isp->is_file->ifl_name, AFTAG_CPU_ARCH_PROFILE,
			sv->afv_integer);
		ofl->ofl_flags |= FLG_OF_FATAL;
		return (S_ERROR);
	}

	for (ftag = ARM_ATTR_FTAG_MIN; ftag <= ARM_ATTR_FTAG_MAX; ftag++) {
		if (ftag == AFTAG_CPU_NAME || ftag == AFTAG_CPU_RAW_NAME ||
		    ftag == AFTAG_CPU_ARCH_PROFILE) {
			/* Already merged. */
			continue;
		}

		dv = dst->af_tags[ftag];
		sv = src->af_tags[ftag];

		/* Simply copy source if this tag is not yet merged. */
		if (dv == NULL) {
			dst->af_tags[ftag] = sv;
			continue;
		}

		if (ftag == AFTAG_ABI_OPTIMIZATION_GOALS ||
		    ftag == AFTAG_ABI_FP_OPTIMIZATION_GOALS) {
			/* Use the first value. */
			continue;
		}

		/*
		 * The rest of tags are copied if source value is greater than
		 * destination.
		 *
		 * Remarks:
		 *	More restrict check may be needed.
		 */
		if (sv != NULL && sv->afv_integer > dv->afv_integer) {
			dst->af_tags[ftag] = sv;
		}
	}

	return (1);
}

/*
 * static size_t
 * arm_fattr_size(arm_fattr_t *fattr, size_t *lenp, size_t *flenp)
 *	Determine the size of SHT_ARM_ATTRIBUTES section to store the given
 *	file attributes. Value to be set as file section size is also set
 *	into *flenp, and attribute section length into *lenp.
 */
static size_t
arm_fattr_size(arm_fattr_t *fattr, size_t *lenp, size_t *flenp)
{
	size_t	size = 0;
	uint_t	ftag;

	/* Calculate file section size. */
	for (ftag = ARM_ATTR_FTAG_MIN; ftag <= ARM_ATTR_FTAG_MAX; ftag++) {
		arm_fattr_val_t	*fvp = fattr->af_tags[ftag];

		if (fvp != NULL) {
			size += uleb_nbytes(ftag);
			size += fvp->afv_encsize;
		}
	}

	/* Add size of section length field and tag field. */
	size += ARM_ATTR_SECTION_SIZE_LEN + uleb_nbytes(ARM_ATTR_TAG_FILE);
	*flenp = size;

	/* Add vendor name length, and size of section length field. */
	size += ARM_ATTR_VENDOR_NAME_LEN + ARM_ATTR_SECTION_SIZE_LEN;
	*lenp = size;

	/* Add size of format version field. */
	size++;

	return size;
}

/*
 * static void *
 * arm_fattr_encode(arm_fattr_t *fattr, size_t *sizep)
 *	Encode file attribute.
 *
 *	Upon successful completion, a pointer to buffer that contains encoded
 *	attributes is returned, and whole buffer size is set into *sizep.
 *	NULL is returned on failure.
 */
static void *
arm_fattr_encode(arm_fattr_t *fattr, size_t *sizep)
{
	uchar_t	*buf, *p;
	size_t	size, secsize, fsize;
	uint_t	ftag;

	/* Calculate size of new SHT_ARM_ATTRIBUTES section. */
	size = arm_fattr_size(fattr, &secsize, &fsize);
	*sizep = size;

	/* Allocate buffer for encoded data. */
	if ((buf = (uchar_t *)libld_malloc(size)) == NULL) {
		return (NULL);
	}

	/* Encode attribute format version and section size. */
	*buf = ARM_ATTR_VERSION;
	p = buf + 1;
	/* LINTED */
	LSB32ENCODE(p, secsize);
	p += ARM_ATTR_SECTION_SIZE_LEN;

	/* Encode vendor name. */
	(void)memcpy(p, ARM_ATTR_VENDOR_NAME, ARM_ATTR_VENDOR_NAME_LEN);
	p += ARM_ATTR_VENDOR_NAME_LEN;

	/* Encode file section tag and file section size. */
	p = uleb_encode(p, ARM_ATTR_TAG_FILE);
	/* LINTED */
	LSB32ENCODE(p, fsize);
	p += ARM_ATTR_SECTION_SIZE_LEN;

	/* Encode file section tag and value pairs. */
	for (ftag = ARM_ATTR_FTAG_MIN; ftag <= ARM_ATTR_FTAG_MAX; ftag++) {
		arm_fattr_val_t	*fvp = fattr->af_tags[ftag];

		if (fvp != NULL) {
			p = uleb_encode(p, ftag);
			if (fvp->afv_type == ARM_ATYPE_INT) {
				p = uleb_encode(p, fvp->afv_integer);
			}
			else {
				size_t	encsize = fvp->afv_encsize;

				(void)memcpy(p, fvp->afv_string, encsize);
				p += encsize;
			}
		}
	}

	assert(p == buf + size);

	return (buf);
}

/*
 * static size_t
 * uleb_nbytes(uint_t value)
 *	Determine the number of bytes is required to encode the given value
 *	in unsigned LEB128 format.
 */
static size_t
uleb_nbytes(uint_t value)
{
	size_t	sz = 1;

	while (value >= 0x80U) {
		value >>= 7;
		sz++;
	}

	return sz;
}

/*
 * static uchar_t *
 * uleb_encode(uchar_t *buf, uint_t value)
 *	Encode the given value in unsigned LEB128 format, and store it to the
 *	given buffer. This function returns a pointer to the buffer for
 *	subsequent data.
 */
static uchar_t *
uleb_encode(uchar_t *buf, uint_t value)
{
	do {
		uchar_t	c;

		/* Encode lower 7 bits. */
		c = value & 0x7fU;
		value >>= 7;
		if (value) {
			/* Set continue bit. */
			c |= 0x80U;
		}
		*buf = c;
		buf++;
	} while (value);

	return buf;
}
