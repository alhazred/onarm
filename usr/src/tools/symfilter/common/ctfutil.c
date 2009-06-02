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
 * Copyright (c) 2007-2009 NEC Corporation
 * All rights reserved.
 */

#ident	"@(#)tools/symfilter/common/ctfutil.c"

/*
 * CTF utilities
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gelf.h>
#include <sys/sysmacros.h>
#include "ctfutil.h"
#include "elfutil.h"

/* Values for ctfu_ctx.cc_flags */
#define	CTFU_CTX_LBLFIX		1	/* Label section is fixed */
#define	CTFU_CTX_OBJFIX		2	/* Object section is fixed */
#define	CTFU_CTX_FUNCFIX	3	/* Function section is fixed */
#define	CTFU_CTX_TYPEFIX	4	/* Type section is fixed */

/*
 * void
 * ctfu_getdata(ctfu_data_t *cdp, void *data, size_t dsize)
 *	Derive CTF data from CTF section.
 *
 * Calling/Exit State:
 *	Upon successful completion, ctfu_getdata() set CTF data address into
 *	*cdp and returns B_TRUE. Otherwise causes fatal error.
 */
void
ctfu_getdata(ctfu_data_t *cdp, void *data, size_t dsize)
{
	ctf_preamble_t	*cprp;
	ctf_header_t	*hdrp;
	size_t		size;
	void		*newdata = NULL;

	if (data == NULL || dsize < sizeof(ctf_header_t)) {
		fatal(0, "Invalid size of CTF data: %d", dsize);
	}

	cprp = (ctf_preamble_t *)data;
	if (cprp->ctp_magic != CTF_MAGIC) {
		fatal(0, "Invalid CTF magic: 0x%x", cprp->ctp_magic);
	}
	if (cprp->ctp_version != CTF_VERSION) {
		fatal(0, "Unsuported version of CTF data: %d",
		      cprp->ctp_version);
	}

	cdp->cd_header = hdrp = (ctf_header_t *)data;
	data = (caddr_t)data + sizeof(ctf_header_t);
	size = dsize - sizeof(ctf_header_t);

	if (hdrp->cth_flags & CTF_F_COMPRESS) {
		size_t	newsz, sz;

		/* CTF data is compressed. We need to decompress it. */
		newsz = hdrp->cth_stroff + hdrp->cth_strlen;
		newdata = xmalloc(newsz);
		sz = zio_inflate(data, size, newdata, newsz);
		if (sz != newsz) {
			fatal(0, "CTF data is corrupted: sz=0x%x, "
			      "newsz=0x%x", sz, newsz);
		}
		data = newdata;
		size = newsz;
	}

	/*
	 * Check whether offset field is valid or not.
	 */

	/* Offset of label section */
	if (!IS_P2ALIGNED(hdrp->cth_lbloff, sizeof(int))) {
		fatal(0, "Invalid label offset: lbloff=0x%x",
		      hdrp->cth_lbloff);
	}
	if (hdrp->cth_lbloff >= size) {
		fatal(0, "Label offset exceeds CTF data size: lbloff=0x%x, "
		      "size=0x%x", hdrp->cth_lbloff, size);
	}
	if (hdrp->cth_objtoff >= size) {
		fatal(0, "Object offset exceeds CTF data size: objtoff=0x%x, "
		      "size=0x%x", hdrp->cth_objtoff, size);
	}
	if (hdrp->cth_lbloff > hdrp->cth_objtoff) {
		fatal(0, "Label offset exceeds object offset: lbloff=0x%x, "
		      "objtoff=0x%x", hdrp->cth_lbloff, hdrp->cth_objtoff);
	}

	/* LINTED - pointer alignment */
	cdp->cd_label = (ctf_lblent_t *)((caddr_t)data + hdrp->cth_lbloff);
	cdp->cd_nlabels = (hdrp->cth_objtoff - hdrp->cth_lbloff) /
		sizeof(ctf_lblent_t);

	/* Offset of object section */
	if (!IS_P2ALIGNED(hdrp->cth_objtoff, sizeof(ushort_t))) {
		fatal(0, "Invalid object offset: objtoff=0x%x",
		      hdrp->cth_objtoff);
	}
	if (hdrp->cth_funcoff >= size) {
		fatal(0, "Function offset exceeds CTF data size: funcoff=0x%x, "
		      "size=0x%x", hdrp->cth_funcoff, size);
	}
	if (hdrp->cth_objtoff > hdrp->cth_funcoff) {
		fatal(0, "Object offset exceeds function offset: objtoff=0x%x, "
		      "funcoff=0x%x", hdrp->cth_objtoff, hdrp->cth_funcoff);
	}

	/* LINTED - pointer alignment */
	cdp->cd_obj = (ushort_t *)((caddr_t)data + hdrp->cth_objtoff);
	cdp->cd_nobjs = (hdrp->cth_funcoff - hdrp->cth_objtoff) /
		sizeof(ushort_t);

	/* Offset of function section */
	if (!IS_P2ALIGNED(hdrp->cth_funcoff, sizeof(ushort_t))) {
		fatal(0, "Invalid function offset: funcoff=0x%x",
		      hdrp->cth_funcoff);
	}
	if (hdrp->cth_typeoff >= size) {
		fatal(0, "Type offset exceeds CTF data size: typeoff=0x%x, "
		      "size=0x%x", hdrp->cth_typeoff, size);
	}
	if (hdrp->cth_funcoff > hdrp->cth_typeoff) {
		fatal(0, "Function offset exceeds type offset: funcoff=0x%x, "
		      "typeoff=0x%x", hdrp->cth_funcoff, hdrp->cth_typeoff);
	}

	/* LINTED - pointer alignment */
	cdp->cd_func = (ushort_t *)((caddr_t)data + hdrp->cth_funcoff);

	/* Offset of type section */
	if (!IS_P2ALIGNED(hdrp->cth_typeoff, sizeof(ushort_t))) {
		fatal(0, "Invalid type offset: typeoff=0x%x",
		      hdrp->cth_typeoff);
	}
	if (hdrp->cth_typeoff > hdrp->cth_stroff) {
		fatal(0, "Type offset exceeds string offset: typeoff=0x%x, "
		      "stroff=0x%x", hdrp->cth_typeoff, hdrp->cth_stroff);
	}

	/* LINTED - pointer alignment */
	cdp->cd_type = (ctf_type_t *)((caddr_t)data + hdrp->cth_typeoff);

	cdp->cd_str = (char *)((caddr_t)data + hdrp->cth_stroff);
	cdp->cd_allocated = newdata;
}

/*
 * void
 * ctfu_freedata(ctfu_data_t *cdp)
 *	Release resources allocated by ctfu_getdata().
 */
void
ctfu_freedata(ctfu_data_t *cdp)
{
	xfree(cdp->cd_allocated);
}

/*
 * void
 * ctfu_ctx_init(ctfu_ctx_t *ccp)
 *	Initialize ctfu_ctx structure that is used to generate CTF data.
 */
void
ctfu_ctx_init(ctfu_ctx_t *ccp)
{
	ctf_header_t	*hdrp = &(ccp->cc_header);
	ziodef_t	*zdp = &(ccp->cc_zio);

	/* Initialize new CTF header. */
	(void)memset(hdrp, 0, sizeof(*hdrp));
	hdrp->cth_magic = CTF_MAGIC;
	hdrp->cth_version = CTF_VERSION;
	hdrp->cth_flags = CTF_F_COMPRESS;

	/* Initialize zlib stream. */
	zio_deflate_init(zdp, hdrp, sizeof(*hdrp));

	ccp->cc_state = 0;
}

/*
 * void
 * ctfu_ctx_addlabel(ctfu_ctx_t *ccp, void *data, size_t size, boolean_t fini)
 *	Append label entry into label section.
 *	Label entry must be added before other section data.
 *	This function must be called with specifying B_TRUE to fini
 *	when there is no more label entry to be added.
 */
void
ctfu_ctx_addlabel(ctfu_ctx_t *ccp, void *data, size_t size, boolean_t fini)
{
	ctf_header_t	*hdrp = &(ccp->cc_header);
	ziodef_t	*zdp = &(ccp->cc_zio);

	if (ccp->cc_state != 0) {
		fatal(0, "CTF label section must be added at first.");
	}
	if (data != NULL && size > 0) {
		zio_deflate(zdp, data, size);
		hdrp->cth_objtoff += size;
	}

	if (fini) {
		uint_t	off;

		/* Finalize label section. */
		ccp->cc_state = CTFU_CTX_LBLFIX;
		off = P2ROUNDUP(hdrp->cth_objtoff, sizeof(ushort_t));
		while (hdrp->cth_objtoff < off) {
			char	c = 0;

			zio_deflate(zdp, &c, 1);
			hdrp->cth_objtoff++;
		}
		hdrp->cth_funcoff = hdrp->cth_objtoff;
	}
}

/*
 * void
 * ctfu_ctx_addobj(ctfu_ctx_t *ccp, void *data, size_t size, boolean_t fini)
 *	Append object entry into object section.
 *	Object entry must be added before function section.
 *	This function must be called with specifying B_TRUE to fini
 *	when there is no more object entry to be added.
 */
void
ctfu_ctx_addobj(ctfu_ctx_t *ccp, void *data, size_t size, boolean_t fini)
{
	ctf_header_t	*hdrp = &(ccp->cc_header);
	ziodef_t	*zdp = &(ccp->cc_zio);

	if (ccp->cc_state != CTFU_CTX_LBLFIX) {
		fatal(0, "CTF object section must be located after "
		      "label section.");
	}
	if (data != NULL && size > 0) {
		zio_deflate(zdp, data, size);
		hdrp->cth_funcoff += size;
	}

	if (fini) {
		uint_t	off;

		/* Finalize object section. */
		ccp->cc_state = CTFU_CTX_OBJFIX;
		off = P2ROUNDUP(hdrp->cth_funcoff, sizeof(ushort_t));
		while (hdrp->cth_funcoff < off) {
			char	c = 0;

			zio_deflate(zdp, &c, 1);
			hdrp->cth_funcoff++;
		}
		hdrp->cth_typeoff = hdrp->cth_funcoff;
	}
}

/*
 * void
 * ctfu_ctx_addfunc(ctfu_ctx_t *ccp, void *data, size_t size, boolean_t fini)
 *	Append function entry into function section.
 *	Function entry must be added before type section.
 *	This function must be called with specifying B_TRUE to fini
 *	when there is no more function entry to be added.
 */
void
ctfu_ctx_addfunc(ctfu_ctx_t *ccp, void *data, size_t size, boolean_t fini)
{
	ctf_header_t	*hdrp = &(ccp->cc_header);
	ziodef_t	*zdp = &(ccp->cc_zio);

	if (ccp->cc_state != CTFU_CTX_OBJFIX) {
		fatal(0, "CTF function section must be located after "
		      "object section.");
	}
	if (data != NULL && size > 0) {
		zio_deflate(zdp, data, size);
		hdrp->cth_typeoff += size;
	}

	if (fini) {
		uint_t	off;

		/* Finalize function section. */
		ccp->cc_state = CTFU_CTX_FUNCFIX;
		off = P2ROUNDUP(hdrp->cth_typeoff, sizeof(int));
		while (hdrp->cth_typeoff < off) {
			char	c = 0;

			zio_deflate(zdp, &c, 1);
			hdrp->cth_typeoff++;
		}
		hdrp->cth_stroff = hdrp->cth_typeoff;
	}
}

/*
 * void
 * ctfu_ctx_addtype(ctfu_ctx_t *ccp, void *data, size_t size, boolean_t fini)
 *	Append type entry into type section.
 *	Type entry must be added before string table.
 *	This function must be called with specifying B_TRUE to fini
 *	when there is no more type entry to be added.
 */
void
ctfu_ctx_addtype(ctfu_ctx_t *ccp, void *data, size_t size, boolean_t fini)
{
	ctf_header_t	*hdrp = &(ccp->cc_header);
	ziodef_t	*zdp = &(ccp->cc_zio);

	if (ccp->cc_state != CTFU_CTX_FUNCFIX) {
		fatal(0, "CTF type section must be located after "
		      "function section.");
	}
	if (data != NULL && size > 0) {
		zio_deflate(zdp, data, size);
		hdrp->cth_stroff += size;
	}

	if (fini) {
		/* Finalize type section. */
		ccp->cc_state = CTFU_CTX_TYPEFIX;

		/* Flush and reset state of zlib status here. */
		zio_deflate_flush(zdp, ZIOFL_FULL_FLUSH);
	}
}

/*
 * void
 * ctfu_ctx_addstr(ctfu_ctx_t *ccp, void *data, size_t size)
 *	Append string table.
 *	Type entry must be added at the end of CTF data.
 */
void
ctfu_ctx_addstr(ctfu_ctx_t *ccp, void *data, size_t size)
{
	ctf_header_t	*hdrp = &(ccp->cc_header);
	ziodef_t	*zdp = &(ccp->cc_zio);

	if (ccp->cc_state != CTFU_CTX_TYPEFIX) {
		fatal(0, "CTF string table must be located after "
		      "type section.");
	}
	if (data != NULL && size > 0) {
		zio_deflate(zdp, data, size);
		hdrp->cth_strlen += size;
	}
}

/*
 * void *
 * ctfu_ctx_fini(ctfu_ctx_t *ccp, size_t *sizep)
 *	Finalize CTF data generation session.
 *
 * Calling/Exit State:
 *	ctfu_ctx_fini() returns CTF data address, and set its size
 *	to *sizep.
 */
void *
ctfu_ctx_fini(ctfu_ctx_t *ccp, size_t *sizep)
{
	ctf_header_t	*hdrp = &(ccp->cc_header);
	ziodef_t	*zdp = &(ccp->cc_zio);
	void		*data;

	/* Finalize zlib stream. */
	data = zio_deflate_fini(zdp, sizep);

	/* Update CTF header. */
	(void)memcpy(data, hdrp, sizeof(*hdrp));

	return data;
}

/*
 * int
 * ctfu_nextsym(elfimg_t *src, GElf_Shdr *shdr, Elf_Data *symdata, int symidx,
 *		uint8_t type, char **namepp)
 *	Retrieve next symbol for CTF data.
 *
 *	If symidx is CTFU_NOSYM, ctfu_nosym() searches symbol from the
 *	beginning of the symbol table. If not, it searches from the given
 *	symbol index.
 *
 *	If type is not CTFU_STT_ANY, ctfu_nextsym() searches symbols
 *	that have the specified symbol type.
 *
 * Calling/Exit State:
 *	Upon successful completion, ctfu_nextsym() returns symbol index
 *	that satisfies the given constraints. Name of symbol will be set
 *	to *namepp if namepp is not NULL.
 *
 *	Returns CTFU_NOSYM if not found.
 */
int
ctfu_nextsym(elfimg_t *src, GElf_Shdr *shdr, Elf_Data *symdata, int symidx,
	     uint8_t type, char **namepp)
{
	int	idx;
	size_t	nsyms = shdr->sh_size / shdr->sh_entsize;
	Elf	*elf = src->e_elf;

	idx = (symidx == CTFU_NOSYM) ? 0 : symidx + 1;
	for (; idx < nsyms; idx++) {
		GElf_Sym	sym;
		uint8_t		symtype;
		char		*name;

		if (gelf_getsym(symdata, idx, &sym) == NULL) {
			elfdie(src->e_filename, "Can't read symbol at %d",
			       idx);
		}
		
		symtype = GELF_ST_TYPE(sym.st_info);
		if (type != CTFU_STT_ANY && symtype != type) {
			/* Skip symbol that doesn't match symbol type. */
			continue;
		}

		if (sym.st_shndx == SHN_UNDEF || sym.st_name == 0) {
			/* Skip undefined or anonymous symbol. */
			continue;
		}

		name = elf_strptr(elf, shdr->sh_link, sym.st_name);
		if (strcmp(name, "_START_") == 0 ||
		    strcmp(name, "_END_") == 0) {
			/* Skip special symbols added by linker. */
			continue;
		}

		if (symtype == STT_OBJECT && sym.st_shndx == SHN_ABS &&
		    sym.st_value == 0) {
			/*
			 * Skip absolute-valued object symbols that have
			 * zero value.
			 */
			continue;
		}

		/* Matched. */
		if (namepp != NULL) {
			*namepp = name;
		}
		return idx;
	}

	/* Not found. */
	return CTFU_NOSYM;
}
