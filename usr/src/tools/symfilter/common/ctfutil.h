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

/* CTF utilities. */

#ifndef	_TOOLS_SYMFILTER_COMMON_CTFUTIL_H
#define	_TOOLS_SYMFILTER_COMMON_CTFUTIL_H

#include <sys/types.h>
#include <sys/ctf.h>
#include "symfilter.h"
#include "zio.h"

/* CTF section data */
typedef struct ctfu_data {
	ctf_header_t	*cd_header;	/* CTF header */
	ctf_lblent_t	*cd_label;	/* Label section */
	size_t		cd_nlabels;	/* Number of label entries */
	ushort_t	*cd_obj;	/* Object section */
	size_t		cd_nobjs;	/* Number of object entries */
	ushort_t	*cd_func;	/* Function section */
	ctf_type_t	*cd_type;	/* Type section */
	char		*cd_str;	/* String table */
	void		*cd_allocated;	/* Allocated buffer or NULL */
} ctfu_data_t;

/* Context to create CTF data */
typedef struct ctfu_ctx {
	ctf_header_t	cc_header;	/* CTF header */
	ziodef_t	cc_zio;		/* zlib stream */
	uint_t		cc_state;	/* Internal state */
} ctfu_ctx_t;

#define	CTFU_NOSYM	-1
#define	CTFU_STT_ANY	((uint8_t)0xff)

/* Prototypes */
extern void	ctfu_getdata(ctfu_data_t *cdp, void *data, size_t dsize);
extern void	ctfu_freedata(ctfu_data_t *cdp);
extern void	ctfu_ctx_init(ctfu_ctx_t *ccp);
extern void	ctfu_ctx_addlabel(ctfu_ctx_t *ccp, void *data, size_t size,
				  boolean_t fini);
extern void	ctfu_ctx_addobj(ctfu_ctx_t *ccp, void *data, size_t size,
				boolean_t fini);
extern void	ctfu_ctx_addfunc(ctfu_ctx_t *ccp, void *data, size_t size,
				 boolean_t fini);
extern void	ctfu_ctx_addtype(ctfu_ctx_t *ccp, void *data, size_t size,
				 boolean_t fini);
extern void	ctfu_ctx_addstr(ctfu_ctx_t *ccp, void *data, size_t size);
extern void	*ctfu_ctx_fini(ctfu_ctx_t *ccp, size_t *sizep);
extern int	ctfu_nextsym(elfimg_t *src, GElf_Shdr *shdr, Elf_Data *symdata,
			     int symidx, uint8_t type, char **namepp);

#endif	/* !_TOOLS_SYMFILTER_COMMON_CTFUTIL_H */
