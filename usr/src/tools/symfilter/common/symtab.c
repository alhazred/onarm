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

#ident	"@(#)tools/symfilter/common/symtab.c"

/*
 * ELF symbol and string table handling
 */

#include "symfilter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <regex.h>
#include <sys/mman.h>
#include <sys/sysmacros.h>
#include <sys/ctf.h>
#include "unixhack.h"
#include "ctfutil.h"

/* Expected average collision count in the symbol hash. */
#define	KHASH_AVE_COLLISION	4

/*
 * Hash entry for static kernel symbols.
 * Unlinke common ELF object, OpenSolaris uses short as hash entry.
 */
typedef uint16_t	khash_t;

#define	KHASH_MAX	UINT16_MAX
#define	KHASH_ELFTYPE	ELF_T_HALF
#define	KHASH_BSWAP(i)	BSWAP_16(i)

/* Format for hash statistics */
#define	KHASH_COL_BUCKET	8
#define	KHASH_COL_SYMNDX	8

#define	KHASH_COL_LENGTH	8
#define	KHASH_COL_NUMBER	8
#define	KHASH_COL_TOTAL		10
#define	KHASH_COL_COVERAGE	10

/*
 * Hash entry to keep symbol name.
 * Note that this structure doesn't represent .hash entry.
 */
struct strhash;
typedef struct strhash	strhash_t;

struct strhash {
	char		*s_name;		/* Symbol name */
	GElf_Word	s_offset;		/* Offset in .strtab */
	strhash_t	*s_next;
};

/*
 * Hash entry to keep strings in the original string table.
 * This is used to reduce string table size.
 */
struct namehash;
typedef struct namehash	namehash_t;

struct namehash {
	char		*n_name;		/* String */
	size_t		n_len;			/* String length */
	char		*n_basename;		/* Base string */
	size_t		n_offset;		/* Offset in .strtab */
	namehash_t	*n_next;
};

/* Symbol entry */
typedef struct syment {
	size_t		se_index;		/* Original symbol index */
	GElf_Sym	se_sym;			/* Symbol entry */
} syment_t;

#define	STRHASH_NENTRY	0x2000
#define	STRHASH_MASK	(STRHASH_NENTRY - 1)
#define	STRHASH_FUNC(name)	(elf_hash(name) & STRHASH_MASK)

static strhash_t	*StrHash[STRHASH_NENTRY];
static namehash_t	*NameHash[STRHASH_NENTRY];

/* .strtab pages */
static char	*StrTable;
static char	*CurStr;
static char	*NameTable;
static size_t	StrTableSize;
static size_t	NumString;
static size_t	NameTableSize;

/* Symbol index transtation table */
static GElf_Word	*SymXlate;

/* Number of symbols in the original symbol table */
static size_t	OrgNsyms;

/* New symbol table */
static syment_t	*Symbol;
static syment_t	*CurSym;
static size_t	Nsyms;

/*
 * Keep estimated symbol size.
 * This array is used for NOTYPE hack.
 */
static GElf_Xword	*SymSizeXlate;

/* Regular expression list */
struct regex_list;
typedef struct regex_list regex_list_t;

struct regex_list {
	regex_t		rl_reg;		/* Regular expression */
	regex_list_t	*rl_next;
};

/* Pattern to remove DT_NEEDED */
static regex_list_t	*RemoveNeeded = NULL;
static regex_list_t	**RemoveNeededNext = &RemoveNeeded;

/* Internal Prototypes */
static int		symtab_name_comp(const void *arg1, const void *arg2);
static int		symtab_syment_comp(const void *arg1, const void *arg2);
static namehash_t	*symtab_name_lookup(char *name, uint_t *indexp);
static GElf_Word	symtab_addstr(char *str);
static size_t		symtab_gethashsize(uint_t nsyms);
static boolean_t	symtab_check_dyn(elfimg_t *src, GElf_Shdr *shdr,
					 GElf_Dyn *dyn);

/*
 * void
 * strtab_init(char *input, Elf *elfp, GElf_Shdr *symtab, Elf_Data *symdata,
 *	       int bssidx, GElf_Shdr *bss, uint_t flags)
 *	Initialize string table generator.
 *	Section header of original symbol table must be specified to symtab.
 */
void
symtab_init(char *input, Elf *elfp, GElf_Shdr *symtab, Elf_Data *symdata,
	    int bssidx, GElf_Shdr *bss, uint_t flags)
{
	Elf_Scn		*scn;
	GElf_Shdr	shdr;
	size_t		entsize;

	/* Allocate buffer for new symbol entries. */
	OrgNsyms = Nsyms = symtab->sh_size / symtab->sh_entsize;
	verbose(1, "Original number of symbols: %d", OrgNsyms);
	entsize = sizeof(syment_t) * Nsyms;
	CurSym = Symbol = (syment_t *)xmalloc(entsize);

	/* Allocate symbol index transration table. */
	SymXlate = (GElf_Word *)xmalloc(sizeof(GElf_Word) * Nsyms);
	(void)memset(SymXlate, 0xff, sizeof(GElf_Word) * Nsyms);

	/* Determine size of string table. */
	if ((scn = elf_getscn(elfp, symtab->sh_link)) == NULL) {
		elfdie(input, "Can't get section descriptor for .strtab at %d",
		       symtab->sh_link);
	}
	if (gelf_getshdr(scn, &shdr) == NULL) {
		elfdie(input, "Can't read section header for .strtab at %d",
		       symtab->sh_link);
	}
	verbose(1, "Original size of .strtab: 0x%llx", shdr.sh_size);

	/* Allocate pages for new string table. */
	StrTableSize = (size_t)shdr.sh_size;
	StrTable = (char *)xmalloc(StrTableSize);

	/* Reserve room for emptry string. */
	*StrTable = '\0';
	CurStr = StrTable + 1;

	/* Estimate symbol size for "NOTYPE" hack. */
	if ((flags & SYMF_NOTYPEHACK) && bss != NULL) {
		syment_t	*sep, *symtable, *symend;
		size_t		tblsize = sizeof(GElf_Xword) * Nsyms;
		int		i, cnt = 0;
		GElf_Addr	bssend = bss->sh_addr + bss->sh_size;

		/*
		 * Allocate symbol size translation table.
		 * Original symbol index is used as table index.
		 */
		SymSizeXlate = (GElf_Xword *)xmalloc(tblsize);
		(void)memset(SymSizeXlate, 0xff, tblsize);

		/* Sort symbol entries in original symbol table. */
		symtable = (syment_t *)xmalloc(entsize);
		for (i = 0, sep = symtable; i < Nsyms; i++, sep++) {
			GElf_Sym	*symp = &(sep->se_sym);

			if (gelf_getsym(symdata, i, symp) == NULL) {
				elfdie(input, "Can't read symbol at %d", i);
			}
			sep->se_index = i;
		}
		qsort(symtable, Nsyms, sizeof(*symtable), symtab_syment_comp);

		symend = symtable + Nsyms;
		for (i = 0, sep = symtable; i < Nsyms; i++, sep++) {
			GElf_Sym	*symp = &(sep->se_sym);
			GElf_Word	type, bind;
			GElf_Addr	nextaddr;
			syment_t	*nsep;

			type = GELF_ST_TYPE(symp->st_info);
			bind = GELF_ST_BIND(symp->st_info);

			if (type != STT_NOTYPE || bind != STB_LOCAL ||
			    symp->st_shndx != bssidx || symp->st_size != 0) {
				continue;
			}

			/*
			 * Determine next symbol in .bss in order to estimate
			 * this symbol size.
			 */
			for (nsep = sep + 1;
			     nsep < symend &&
				     (nsep->se_sym.st_shndx != bssidx ||
				      symp->st_value >= nsep->se_sym.st_value);
			     nsep++);
			if (nsep < symend) {
				nextaddr = nsep->se_sym.st_value;
			}
			else if (symp->st_value < bssend) {
				nextaddr = bssend;
			}
			else {
				fatal(0, "symbol value is out of bss range: "
				      "index=%d, value=0x%llx", i,
				      symp->st_value);
			}
			if (nextaddr == symp->st_value) {
				fatal(0, "nextaddr doesn't change: %d, %d",
				      sep->se_index, nsep->se_index);
			}
			*(SymSizeXlate + sep->se_index) =
				nextaddr - symp->st_value;
			cnt++;
		}

		xfree(symtable);
		if (cnt == 0) {
			xfree(SymSizeXlate);
			SymSizeXlate = NULL;
		}
	}
}

/*
 * void
 * symtab_add(GElf_Word index, GElf_Sym *symp, char *name)
 *	Append a symbol to the new symbol table.
 */
void
symtab_add(GElf_Word index, GElf_Sym *symp, char *name)
{
	size_t		newidx;
	GElf_Sym	*cursym;

	if ((newidx = CurSym - Symbol) >= Nsyms) {
		fatal(0, "Symbol table overflow: %d", newidx);
	}

	/* Copy original entry. */
	CurSym->se_index = index;
	cursym = &(CurSym->se_sym);
	(void)memcpy(cursym, symp, sizeof(GElf_Sym));

	/* Setup symbol index translation table. */
	*(SymXlate + index) = newidx;

	/* Change offset of name in .strtab. */
	cursym->st_name = symtab_addstr(name);

	verbose(3, "ADD: %s: index=%d->%d, name=0x%x", name, index,
		*(SymXlate + index), cursym->st_name);

	CurSym++;
}

/*
 * size_t
 * symtab_oldsymnum(void)
 *	Return number of old symbol entries.
 */
size_t
symtab_oldsymnum(void)
{
	return OrgNsyms;
}

/*
 * size_t
 * symtab_newsymnum(void)
 *	Return number of new symbol entries.
 */
size_t
symtab_newsymnum(void)
{
	return CurSym - Symbol;
}

/*
 * size_t
 * symtab_newstrnum(void)
 *	Return number of strings in new string table.
 */
size_t
symtab_newstrnum(void)
{
	return NumString;
}

/*
 * void
 * symtab_update_symtab(elfimg_t *dst, Elf_Data *data, GElf_Shdr *shdr,
 *			int *xlate)
 *	Update symbol table section.
 *	xlate must be a section index array that converts original
 *	section index to new section index.
 *
 * Remarks:
 *	This function must NOT change order of symbols in this function
 *	because CTF data depends on it.
 */
void
symtab_update_symtab(elfimg_t *dst, Elf_Data *data, GElf_Shdr *shdr,
		     int *xlate)
{
	size_t		size, i, locals = 0;
	syment_t	*sep;

	size = Nsyms * shdr->sh_entsize;

	/* Allocate new data buffer for symbol table entries. */
	data->d_buf = xmalloc(size);
	data->d_size = size;
	data->d_align = shdr->sh_addralign;
	data->d_type = ELF_T_SYM;
	shdr->sh_size = size;

	/* Update entries. */
	for (i = 0, sep = Symbol; i < Nsyms; i++, sep++) {
		GElf_Sym	*symp = &(sep->se_sym);
		char		*name = StrTable + symp->st_name;
		namehash_t	*np;
		uint_t		index;
		int		ondx;
		GElf_Word	type, bind;

		if (*name == '\0') {
			symp->st_name = 0;
		}
		else {
			np = symtab_name_lookup(name, &index);

			/* np must not be NULL. */
			symp->st_name = (GElf_Word)np->n_offset;
		}

		ondx = symp->st_shndx;
		type = GELF_ST_TYPE(symp->st_info);
		bind = GELF_ST_BIND(symp->st_info);
		if (type == STT_SECTION && *(xlate + ondx) == -1) {
			/*
			 * This section will be removed.
			 * Change this symbol into COMMON symbol to cheat
			 * kernel runtime linker.
			 */
			symp->st_info = GELF_ST_INFO(bind, STT_NOTYPE);
			symp->st_shndx = SHN_COMMON;
			verbose(1, "*** %d: Remove section: %d:", i, ondx);
		}
		else if (symp->st_shndx < SHN_LORESERVE) {
			symp->st_shndx = *(xlate + ondx);
		}

		/* Apply substitution chain. */
		rule_subst(symp, NameTable + symp->st_name);
		verbose(3, "%s: New name index=0x%d, ndx=%d -> %d",
			name, symp->st_name, ondx, symp->st_shndx);

		if (bind == STB_LOCAL) {
			locals++;
		}

		if (gelf_update_sym(data, i, symp) == 0) {
			elfdie(dst->e_filename,
			       "Can't update symbol at %d", i);
		}
	}

	shdr->sh_info = locals;
}

/*
 * void
 * symtab_update_str(elfimg_t *dst, Elf_Data *data, GElf_Shdr *shdr)
 *	Update string table section.
 */
/* ARGSUSED */
void
symtab_update_str(elfimg_t *dst, Elf_Data *data, GElf_Shdr *shdr)
{
	data->d_buf = NameTable;
	data->d_size = NameTableSize;
	data->d_align = shdr->sh_addralign;
	data->d_type = ELF_T_BYTE;
	shdr->sh_size = NameTableSize;
}

/*
 * void
 * symtab_update_sym(elfimg_t *src, elfimg_t *dst, Elf_Data *sdata,
 *		     Elf_Data *ddata, GElf_Shdr *shdr, GElf_Addr diffbase,
 *		     GElf_Off diff, int *xlate)
 *	Update symbol table other than .symtab section.
 *	This function updates section index in symbol entry.
 *	If diffbase is not NULL, diff will be added to symbol address
 *	if it is larger than or equals diffbase.
 *
 *	xlate must be a section index array that converts original
 *	section index to new section index.
 */
void
symtab_update_sym(elfimg_t *src, elfimg_t *dst, Elf_Data *sdata,
		  Elf_Data *ddata, GElf_Shdr *shdr, GElf_Addr diffbase,
		  GElf_Off diff, int *xlate)
{
	int	i, num;

	sdata->d_type = ELF_T_SYM;
	(void)memcpy(ddata, sdata, sizeof(Elf_Data));
	ddata->d_buf = xmalloc(sdata->d_size);
	(void)memcpy(ddata->d_buf, sdata->d_buf, sdata->d_size);
	num = sdata->d_size / shdr->sh_entsize;

	/* Update entries. */
	for (i = 0; i < num; i++) {
		GElf_Sym	sym;
		int		ndx;

		if (gelf_getsym(sdata, i, &sym) == NULL) {
			elfdie(src->e_filename, "Can't read symbol at %d", i);
		}

		if (sym.st_shndx < SHN_LORESERVE) {
			ndx = *(xlate + sym.st_shndx);
			sym.st_shndx = ndx;
		}

		if (diffbase != NULL && diffbase <= sym.st_value) {
			sym.st_value += diff;
		}

		if (gelf_update_sym(ddata, i, &sym) == 0) {
			elfdie(dst->e_filename,
			       "Can't update symbol at %d", i);
		}
	}
}

/*
 * void
 * symtab_update_rel(elfimg_t *src, elfimg_t *dst, Elf_Data *sdata,
 *		     Elf_Data *ddata, GElf_Shdr *shdr, int *xlate)
 *	Update relocation section.
 *	This function updates symbol index in relocation entry.
 *
 *	xlate must be a section index array that converts original
 *	section index to new section index.
 */
/* ARGSUSED5 */
void
symtab_update_rel(elfimg_t *src, elfimg_t *dst, Elf_Data *sdata,
		  Elf_Data *ddata, GElf_Shdr *shdr, int *xlate)
{
	int	i, num;

	sdata->d_type = ELF_T_REL;
	(void)memcpy(ddata, sdata, sizeof(Elf_Data));
	ddata->d_buf = xmalloc(sdata->d_size);
	(void)memcpy(ddata->d_buf, sdata->d_buf, sdata->d_size);
	num = sdata->d_size / shdr->sh_entsize;

	/* Update entries. */
	for (i = 0; i < num; i++) {
		GElf_Rel	rel;
		GElf_Word	sym, type;
		int		newsym;

		if (gelf_getrel(sdata, i, &rel) == NULL) {
			elfdie(src->e_filename,
			       "Can't read relocation entry at %d", i);
		}

		sym = GELF_R_SYM(rel.r_info);
		type = GELF_R_TYPE(rel.r_info);
		newsym = *(SymXlate + sym);
		if (newsym == -1) {
			/*
			 * This symbol has been removed.
			 * Set NULL symbol.
			 */
			newsym = 0;
		}
		if (sym != newsym) {
			verbose(3, "%d: New reloc info: sym:%d -> %d",
				i, sym, newsym);
			rel.r_info = GELF_R_INFO(newsym, type);
		}

		if (gelf_update_rel(ddata, i, &rel) == 0) {
			elfdie(dst->e_filename,
			       "Can't update relocation entry at %d", i);
		}
	}
}

/*
 * void
 * symtab_update_ctf(elfimg_t *src, elfimg_t *dst, Elf_Data *sdata,
 *		     Elf_Data *ddata, GElf_Shdr *shdr, int symidx)
 *	Update CTF section.
 *	This function removes data object and function data associated
 *	with removed symbols.
 */
/* ARGSUSED1 */
void
symtab_update_ctf(elfimg_t *src, elfimg_t *dst, Elf_Data *sdata,
		  Elf_Data *ddata, GElf_Shdr *shdr, int symidx)
{
	ctfu_data_t	ctf;
	ctfu_ctx_t	nctf;
	ctf_header_t	*ohdrp;
	void		*newdata;
	size_t		newsize, nent;
	Elf		*elf = src->e_elf;
	Elf_Scn		*scn;
	GElf_Shdr	symshdr;
	Elf_Data	*symdata;
	ushort_t	*sp, *endsp;
	int		idx, i;

	/* Get original symbol table. */
	if ((scn = elf_getscn(elf, symidx)) == NULL) {
		elfdie(src->e_filename, "Can't get .symtab at %d",
		       symidx);
	}
	if (gelf_getshdr(scn, &symshdr) == NULL) {
		elfdie(src->e_filename, "Can't read section header for "
		       ".symtab at %d", symidx);
	}
	if ((symdata = elf_getdata(scn, NULL)) == NULL) {
		elfdie(src->e_filename, "Can't read symbol table");
	}

	/* Get CTF data. */
	ctfu_getdata(&ctf, sdata->d_buf, sdata->d_size);
	ohdrp = ctf.cd_header;

	/* Initialize new CTF data. */
	ctfu_ctx_init(&nctf);
	nctf.cc_header.cth_parlabel = ohdrp->cth_parlabel;
	nctf.cc_header.cth_parname = ohdrp->cth_parname;

	/* Copy label section. */
	ctfu_ctx_addlabel(&nctf, ctf.cd_label,
			  sizeof(ctf_lblent_t) * ctf.cd_nlabels, B_TRUE);

	/*
	 * Copy all object section entries but entries associated with
	 * removed symbols.
	 */
	sp = ctf.cd_obj;
	for (idx = CTFU_NOSYM, i = 0; i < ctf.cd_nobjs; i++, sp++) {
		int	nextsym, newsym;
		char	*name;

		nextsym = ctfu_nextsym(src, &symshdr, symdata, idx,
				       STT_OBJECT, &name);
		if (nextsym == CTFU_NOSYM) {
			warning("Symbol for CTF object %d is not found: "
				"symidx=%d", i, idx);
			name = "<null>";
		}
		else {
			idx = nextsym;
			newsym = *(SymXlate + idx);
			if (newsym == -1) {
				/*
				 * Eliminated object that is removed from
				 * symbol table.
				 */
				verbose(2, "Remove CTF object[%d]: sym=%d:%s",
					i, idx, name);
				continue;
			}
		}
		ctfu_ctx_addobj(&nctf, sp, sizeof(ushort_t), B_FALSE);
	}

	/* Finalze object section. */
	ctfu_ctx_addobj(&nctf, NULL, 0, B_TRUE);
	
	/*
	 * Copy all functoin section entries but entries associated with
	 * removed symbols.
	 */
	sp = ctf.cd_func;
	endsp = (ushort_t *)ctf.cd_type;
	nent = 1;
	for (idx = CTFU_NOSYM, i = 0; sp < endsp; i++, sp += nent) {
		int		nextsym, newsym;
		char		*name;
		ushort_t	info, kind, len;

		nextsym = ctfu_nextsym(src, &symshdr, symdata, idx,
				       STT_FUNC, &name);
		info = *sp;
		kind = CTF_INFO_KIND(info);
		len = CTF_INFO_VLEN(info);
		if (nextsym == CTFU_NOSYM) {
			if (kind != CTF_K_UNKNOWN || len != 0) {
				warning("Symbol for CTF function %d is "
					"not found: symidx=%d, kind=%d, "
					"len=%d", i, idx, kind, len);
			}
			name = "<null>";
			nent = 1;
		}
		else {
			idx = nextsym;
			if (kind == CTF_K_UNKNOWN && len == 0) {
				/* This is an entry for padding. */
				nent = 1;
			}
			else if (kind != CTF_K_FUNCTION) {
				fatal(0, "Invalid CTF function entry at %d: "
				      "0x%x", i, info);
			}
			else {
				/* len + info + return type */
				nent = len + 2;
			}

			newsym = *(SymXlate + idx);
			if (newsym == -1) {
				/*
				 * Eliminated function that is removed from
				 * symbol table.
				 */
				verbose(2, "Remove CTF function[%d]: "
					"sym=%d:%s", i, idx, name);
				continue;
			}
		}
		ctfu_ctx_addfunc(&nctf, sp, nent * sizeof(ushort_t), B_FALSE);
	}

	/* Finalze function section. */
	ctfu_ctx_addfunc(&nctf, NULL, 0, B_TRUE);

	/* Copy type section. */
	ctfu_ctx_addtype(&nctf, ctf.cd_type,
			 ohdrp->cth_stroff - ohdrp->cth_typeoff, B_TRUE);

	/*
	 * Copy string table.
	 * We don't need to recreate string table because symbol change
	 * doesn't affect to CTF string table.
	 */
	ctfu_ctx_addstr(&nctf, ctf.cd_str, ohdrp->cth_strlen);

	/* Finalize new CTF data. */
	newdata = ctfu_ctx_fini(&nctf, &newsize);
	verbose(1, "CTF data size: %d -> %d", sdata->d_size, newsize);

	ctfu_freedata(&ctf);

	(void)memcpy(ddata, sdata, sizeof(Elf_Data));
	ddata->d_buf = newdata;
	ddata->d_size = newsize;
}

/*
 * void
 * symtab_fixup(elfimg_t *src, int bssidx, uint_t flags)
 *	Fix up symbol table data.
 *
 *	If SYMF_NOCOMP is set in flags, this function creates hash entry
 *	for substring of symbol name, to reduce size of string table.
 */
/* ARGSUSED */
void
symtab_fixup(elfimg_t *src, int bssidx, uint_t flags)
{
	size_t		strsize, len, nname, i;
	char		*p, *table, *curp;
	namehash_t	**nametable, **npp;

	if (NumString == 0) {
		Nsyms = 0;
		return;
	}

	Nsyms = CurSym - Symbol;

	/* At first, sort strings in descending order of length. */
	nametable = (namehash_t **)xmalloc(sizeof(namehash_t *) * NumString);
	strsize = (size_t)(CurStr - StrTable);
	table = (char *)xmalloc(strsize);
	*table = '\0';
	curp = table + 1;
	nname = 0;
	for (p = StrTable + 1; p < StrTable + strsize; p += len + 1) {
		namehash_t	*np;

		if (nname >= NumString) {
			fatal(0, "String name table overflow: %d", nname);
		}
		len = strlen(p);
		np = (namehash_t *)xmalloc(sizeof(*np));
		np->n_name = p;
		np->n_len = len;
		np->n_basename = p;
		np->n_offset= 0;
		np->n_next = NULL;
		*(nametable + nname) = np;
		nname++;
	}

	qsort(nametable, nname, sizeof(*nametable), symtab_name_comp);

	/*
	 * Insert string and its substring from a certain index to the end
	 * of string into hash table.
	 */
	for (i = 0, npp = nametable; i < nname; i++, npp++) {
		namehash_t	*np = *npp;
		size_t		soff, len;
		uint_t		index;

		p = np->n_name;
		if (symtab_name_lookup(p, &index) != NULL) {
			/* Already added. This entry can be discarded. */
			*npp = NULL;
			xfree(np);
			continue;
		}

		len = np->n_len + 1;
		if (curp + len > table + strsize) {
			fatal(0, "String table overflow: 0x%x", strsize);
		}
		(void)memcpy(curp, p, len);
		np->n_name = curp;
		np->n_basename = curp;
		np->n_offset = curp - table;
		np->n_next = NameHash[index];
		NameHash[index] = np;
		verbose(5, "%s: enter hash: index=%d, off=%d",
			np->n_name, index, np->n_offset);
		curp += len;

		if (flags & SYMF_NOCOMP) {
			continue;
		}
		for (p++, soff = 1; *p != '\0'; p++, soff++) {
			namehash_t	*snp = symtab_name_lookup(p, &index);

			if (snp != NULL) {
				break;
			}

			snp = (namehash_t *)xmalloc(sizeof(*snp));
			snp->n_name = p;
			snp->n_len = np->n_len - soff;
			snp->n_basename = np->n_name;
			snp->n_offset = np->n_offset + soff;
			snp->n_next = NameHash[index];
			NameHash[index] = snp;
		}
	}

	xfree(nametable);
	NameTable = table;
	NameTableSize = curp - table;

	/* Apply "NOTYPE" hack to symbols in .bss section. */
	if (flags & SYMF_NOTYPEHACK && SymSizeXlate != NULL) {
		GElf_Sym	*symp;
		syment_t	*sep;
		size_t		idx;

		for (sep = Symbol, idx = 0; sep < CurSym; sep++, idx++) {
			GElf_Word	type, bind;
			GElf_Xword	symsize;

			symp = &(sep->se_sym);
			type = GELF_ST_TYPE(symp->st_info);
			bind = GELF_ST_BIND(symp->st_info);

			if (type != STT_NOTYPE || bind != STB_LOCAL ||
			    symp->st_shndx != bssidx || symp->st_size != 0) {
				continue;
			}

			/* Make this symbol as object. */
			type = STT_OBJECT;
			symp->st_info = GELF_ST_INFO(bind, type);

			/* Update symbol size. */
			symsize = *(SymSizeXlate + sep->se_index);
			if (symsize == (GElf_Xword)-1) {
				fatal(0, "Can't estimate symbol size: "
				      "index=%d, value=0x%llx",
				      sep->se_index, symp->st_value);
			}
			symp->st_size = symsize;
		}

		xfree(SymSizeXlate);
		SymSizeXlate = NULL;
	}
}

/*
 * void
 * symtab_addhash(elfimg_t *dst, GElf_Off off, int nameoff, GElf_Shdr *shdr,
 *		  uint_t enc, int symidx)
 *	Add .hash section at the specified file offset.
 *	Note that size of hash entry will be 2.
 */
void
symtab_addhash(elfimg_t *dst, GElf_Off off, int nameoff, GElf_Shdr *shdr,
	       uint_t enc, int symidx)
{
	Elf		*elf = dst->e_elf;
	size_t		nbucket, hsize;
	khash_t		*hash, *bucket, *chain, idx;
	syment_t	*sep;
	GElf_Off	pad;
	Elf_Scn		*scn;
	Elf_Data	*data;

	if (Nsyms > KHASH_MAX) {
		fatal(0, "Too many symbols. nsyms = %d", Nsyms);
	}

	/* Determine hash bucket size. */
	nbucket = symtab_gethashsize(Nsyms);
	if (nbucket > KHASH_MAX) {
		nbucket = KHASH_MAX - 1;
	}

	/* Allocate buffer for hash. */
	hsize = (nbucket + Nsyms + 2) * sizeof(*hash);
	hash = (khash_t *)xmalloc(hsize);
	(void)memset(hash, 0, hsize);

	/* Set bucket and chain size. */
	*hash = (khash_t)nbucket;
	*(hash + 1) = (khash_t)Nsyms;
	bucket = hash + 2;
	chain = bucket + nbucket;

	/* Construct symbol hash. */
	for (sep = Symbol, idx = 0; sep < CurSym; sep++, idx++) {
		GElf_Sym	*symp = &(sep->se_sym);
		khash_t		index, *ip;
		char		*name;

		if (symp->st_name == 0 || symp->st_shndx == SHN_UNDEF) {
			verbose(2, "%d: not inserted to hash", idx);
			continue;
		}

		name = NameTable + symp->st_name;
		if (rule_nohash(symp, name)) {
			verbose(2, "%d[%s]: not inserted to hash", idx, name);
			continue;
		}

		index = elf_hash(name) % nbucket;
		for (ip = bucket + index; *ip != 0; ip = chain + *ip);
		*ip = idx;
	}

	/* Create section. */
	if ((scn = elf_newscn(elf)) == NULL) {
		elfdie(dst->e_filename, "Can't create new section descriptor");
	}
	if (gelf_getshdr(scn, shdr) == NULL) {
		elfdie(dst->e_filename, "Can't read new section header");
	}

	pad = off % sizeof(khash_t);
	if (pad) {
		off += sizeof(khash_t) - pad;
	}
	shdr->sh_name = nameoff;
	shdr->sh_offset = off;
	shdr->sh_type = SHT_UNIX_SYMHASH;
	shdr->sh_size = hsize;
	shdr->sh_addralign = sizeof(khash_t);
	shdr->sh_entsize = sizeof(khash_t);
	shdr->sh_link = symidx;

	if ((data = elf_newdata(scn)) == NULL) {
		elfdie(dst->e_filename, "Can't create data for .hash");
	}
	data->d_buf = hash;
	data->d_size = hsize;
	data->d_align = sizeof(khash_t);
	data->d_type = KHASH_ELFTYPE;

	if (elf32_xlatetof(data, data, enc) == NULL) {
		elfdie(dst->e_filename, "Can't translate hash data");
	}

	verbose(1, "New symbol hash: offset=0x%llx, size=0x%llx",
		shdr->sh_offset, shdr->sh_size);
	if (gelf_update_shdr(scn, shdr) == NULL) {
		elfdie(dst->e_filename, "Can't update .hash");
	}
}

/*
 * void
 * symtab_hashinfo(Elf *elf, char *file, int fd, GElf_Ehdr *ehdr)
 *	Show statistics of static kernel symbol hash.
 *
 *	Common ELF utility, such as elfdump(1), can't recognize symbol hash
 *	added by -X option. That's why we provide utility to show usage of
 *	symbol hash.
 */
void
symtab_hashinfo(Elf *elf, char *file, int fd, GElf_Ehdr *ehdr)
{
	int		i;
	Elf_Scn		*symscn;
	GElf_Shdr	*hshdr = NULL, *symshdr = NULL;
	Elf_Data	*symdata;
	khash_t		*hash, *bucket, *chain, idx;
	size_t		nbucket, nsyms, hsize, *hist, maxhist, hidx;
	size_t		symsum;

	for (i = 0; i < ehdr->e_shnum; i++) {
		Elf_Scn		*scn;
		GElf_Shdr	shdr;
		char		*sname;

		if ((scn = elf_getscn(elf, i)) == NULL) {
			elfdie(file, "Can't get section descriptor at %d", i);
		}
		if (gelf_getshdr(scn, &shdr) == NULL) {
			elfdie(file, "Can't read section header at %d", i);
		}
		sname = elf_strptr(elf, ehdr->e_shstrndx, shdr.sh_name);
		if (sname == NULL) {
			elfdie(file, "Can't find section name: %d",
			       shdr.sh_name);
		}
		if (SECTION_IS_UNIX_SYMHASH(&shdr, sname)) {
			hshdr = (GElf_Shdr *)xmalloc(sizeof(*hshdr));
			(void)memcpy(hshdr, &shdr, sizeof(shdr));
		}
		else if (shdr.sh_type == SHT_SYMTAB) {
			symshdr = (GElf_Shdr *)xmalloc(sizeof(*symshdr));
			(void)memcpy(symshdr, &shdr, sizeof(shdr));
			symscn = scn;
		}
	}

	if (hshdr == NULL) {
		fatal(0, "No symbol hash");
	}
	if (symshdr == NULL) {
		fatal(0, "No symbol table");
	}
	if (hshdr->sh_entsize != sizeof(khash_t)) {
		fatal(0, "This symbol hash is not dumped by -X option");
	}

	/*
	 * We can't use elf_getdata() because this symbol hash uses short
	 * as entry size.
	 */
	hash = (khash_t *)xmalloc(hshdr->sh_size);
	if (pread(fd, hash, hshdr->sh_size, hshdr->sh_offset) == -1) {
		fatal(errno, "Can't read symbol hash");
	}

	if (NEED_BSWAP(ehdr->e_ident[EI_DATA])) {
		khash_t	*ip;

		for (ip = hash; (caddr_t)ip < (caddr_t)hash + hshdr->sh_size;
		     ip++) {
			*ip = KHASH_BSWAP(*ip);
		}
	}

	nbucket = (size_t)*hash;
	nsyms = (size_t)*(hash + 1);
	hsize = (nbucket + nsyms + 2) * sizeof(khash_t);
	if (hsize != hshdr->sh_size) {
		fatal(0, "Invalid data size(%lld): nbucket=%d, nsyms=%d, "
		      "required=%d", hshdr->sh_size, nbucket, nsyms, hsize);
	}
	bucket = hash + 2;
	chain = bucket + nbucket;

	if (Verbose) {
		(void)printf("%*s   %*s   NAME\n", KHASH_COL_BUCKET, "BUCKET",
			     -KHASH_COL_SYMNDX, "SYMNDX");
		if ((symdata = elf_getdata(symscn, NULL)) == NULL) {
			elfdie(file, "Can't read symbol table");
		}
	}

	/* Create histogram. */
	hist = (size_t *)xmalloc(sizeof(*hist) * KHASH_AVE_COLLISION);
	for (hidx = 0; hidx < KHASH_AVE_COLLISION; hidx++) {
		*(hist + hidx) = 0;
	}
	maxhist = KHASH_AVE_COLLISION;
	for (idx = 0; idx < nbucket; idx++) {
		khash_t	*ip;
		size_t	cnt = 0, *hp;

		for (ip = bucket + idx; *ip != 0; ip = chain + *ip, cnt++) {
			GElf_Sym	sym;
			int		symidx;
			char		*name, symndx[KHASH_COL_SYMNDX];

			if (Verbose == 0) {
				continue;
			}

			symidx = *ip;
			if (gelf_getsym(symdata, symidx, &sym) == NULL) {
				elfdie(file, "Can't read symbol at %d",
				       symidx);
			}
			name = elf_strptr(elf, symshdr->sh_link, sym.st_name);
			if (name == NULL) {
				elfdie(file, "No symbol string at 0x%lx",
				       sym.st_name);
			}
			(void)snprintf(symndx, sizeof(symndx), "[%d]", symidx);
			if (cnt == 0) {
				(void)printf("%*d   %*s   %s\n",
					     KHASH_COL_BUCKET, idx,
					     -KHASH_COL_SYMNDX, symndx, name);
			}
			else {
				(void)printf("%*s   %*s   %s\n",
					     KHASH_COL_BUCKET, "",
					     -KHASH_COL_SYMNDX, symndx, name);
			}
		}
		if (Verbose && cnt) {
			(void)printf("%*s   %*s   collisions = %d\n",
				     KHASH_COL_BUCKET, "",
				     -KHASH_COL_SYMNDX, "", cnt);
		}

		if (cnt >= maxhist) {
			size_t	newmax, newsize;

			/* Expand histogram. */
			newmax = cnt + 1;
			newsize = sizeof(*hist) * newmax;
			hist = (size_t *)xrealloc(hist, newsize);
			for (hidx = maxhist; hidx <= cnt; hidx++) {
				*(hist + hidx) = 0;
			}
			maxhist = newmax;
		}
		hp = hist + cnt;
		*hp = *hp + 1;
	}

	(void)printf("\nHistogram for collision of symbol hash bucket.\n"
		     "Bucket size = %d, Number of symbols = %d\n",
		     nbucket, nsyms);
	(void)printf("  %*s %*s %*s   COVERAGE\n", KHASH_COL_LENGTH, "LENGTH",
		     KHASH_COL_NUMBER, "NUMBER", KHASH_COL_TOTAL, "TOTAL");
	symsum = 0;
	for (hidx = 0; hidx < maxhist; hidx++) {
		size_t	cnt, *hp;
		char	total[KHASH_COL_TOTAL + 1];
		char	coverage[KHASH_COL_COVERAGE + 1];

		hp = hist + hidx;
		cnt = *hp;
		symsum += cnt * hidx;
		if (symsum == 0) {
			coverage[0] = '\0';
		}
		else {
			(void)snprintf(coverage, sizeof(coverage), "(%3.1f%%)",
				       ((double)symsum / (double)nsyms) *
				       100.0);
		}
		(void)snprintf(total, sizeof(total), "%3.1f%%",
			       ((double)cnt / (double)nbucket) * 100.0);
		(void)printf("  %*d %*d %*s   %s\n", KHASH_COL_LENGTH, hidx,
			     KHASH_COL_NUMBER, cnt, KHASH_COL_TOTAL, total,
			     coverage);
	}

	xfree(hshdr);
	xfree(symshdr);
	xfree(hash);
	xfree(hist);
}

/*
 * static int
 * symtab_name_comp(const void *arg1, const void *arg2)
 *	Comparator for namehash_t.
 *	This will sort namehash_t entries in descending order of string length.
 */
static int
symtab_name_comp(const void *arg1, const void *arg2)
{
	namehash_t	*n1 = *((namehash_t **)arg1);
	namehash_t	*n2 = *((namehash_t **)arg2);

	if (n1->n_len < n2->n_len) {
		return 1;
	}
	else if (n1->n_len > n2->n_len) {
		return -1;
	}

	return 0;
}

/*
 * static int
 * symtab_syment_comp(const void *arg1, const void *arg2)
 *	Comparator for syment_t.
 *	This will sort syment_t array in ascending order of symbol value.
 */
static int
symtab_syment_comp(const void *arg1, const void *arg2)
{
	GElf_Sym	*s1 = &(((syment_t *)arg1)->se_sym);
	GElf_Sym	*s2 = &(((syment_t *)arg2)->se_sym);

	if (s1->st_value < s2->st_value) {
		return -1;
	}
	else if (s1->st_value > s2->st_value) {
		return 1;
	}

	return 0;
}

/*
 * static namehash_t *
 * symtab_name_lookup(char *name, uint_t *indexp)
 *	Lookup name hash.
 */
static namehash_t *
symtab_name_lookup(char *name, uint_t *indexp)
{
	namehash_t	*np;
	uint_t		index;

	index = STRHASH_FUNC(name);
	*indexp = index;
	for (np = NameHash[index];
	     np != NULL && strcmp(np->n_name, name) != 0; np = np->n_next);

	return np;
}

/*
 * static GElf_Word
 * symtab_addstr(char *str)
 *	Append string to .strtab page.
 *
 * Calling/Exit State:
 *	symtab_addstr() returns offset for the give string in .strtab,
 *	that is, it should be set in st_name in symbol entry.
 */
static GElf_Word
symtab_addstr(char *str)
{
	size_t		len = strlen(str);
	strhash_t	*shp;
	uint_t		index;

	if (len == 0) {
		/* Already added at the top of .strtab. */
		return 0;
	}

	/* Search symbol name hash. */
	index = STRHASH_FUNC(str);
	for (shp = StrHash[index]; shp != NULL; shp = shp->s_next) {
		if (strcmp(shp->s_name, str) == 0) {
			/* Already added. */
			return shp->s_offset;
		}
	}

	len++;
	if (CurStr + len > StrTable + StrTableSize) {
		size_t	off = CurStr - StrTable;
		size_t	newsize = StrTableSize + len + 0x1000;
		uint_t	idx;

		/* Expand string table. */
		verbose(1, "Expand string table: 0x%x => 0x%x",
			StrTableSize, newsize);
		StrTable = (char *)xrealloc(StrTable, newsize);
		CurStr = StrTable + off;
		StrTableSize = newsize;

		/* Relocate string in hash entries. */
		for (idx = 0; idx < STRHASH_NENTRY; idx++) {
			for (shp = StrHash[idx]; shp != NULL;
			     shp = shp->s_next) {
				shp->s_name = StrTable + shp->s_offset;
			}
		}
	}
	(void)memcpy(CurStr, str, len);
	NumString++;

	/* Insert a new hash entry. */
	shp = (strhash_t *)xmalloc(sizeof(*shp));
	shp->s_name = CurStr;
	shp->s_offset = (GElf_Word)(CurStr - StrTable);
	shp->s_next = NULL;
	shp->s_next = StrHash[index];
	StrHash[index] = shp;
	verbose(3, "New string: [%s][0x%x]", shp->s_name, shp->s_offset);

	CurStr += len;

	return shp->s_offset;
}

/*
 * static size_t
 * symtab_gethashsize(uint_t nsyms)
 *	Determine bucket size of .hash section.
 *	This function returns the next prime number larger than
 *	(nsyms / KHASH_AVE_COLLISION).
 */
static size_t
symtab_gethashsize(uint_t nsyms)
{
	size_t	size = MAX(nsyms / KHASH_AVE_COLLISION, 4);

	/* Determine the next prime number. */
	/* CONSTANTCONDITION */
	while (1) {
		/*
		 * We know "size" is larger than 2.
		 * So multiple of 2 is not prime number.
		 */
		if ((size & 1) != 0) {
			size_t	n;
			int	notprime = 0;

			for (n = 3; n * n <= size; n += 2) {
				if ((size % n) == 0) {
					notprime = 1;
					break;
				}
			}
			if (notprime == 0) {
				break;
			}
		}
		size++;
	}

	verbose(1, "Hash bucket size = %d, nsyms = %d", size, nsyms);
	return size;
}

/*
 * void
 * symtab_remove_needed_add(const char *pattern)
 *	Add regular expression that is used to remove DT_NEEDED.
 */
void
symtab_remove_needed_add(const char *pattern)
{
	regex_list_t	*rlp;
	regex_t		*preg;
	int		err;

	rlp = (regex_list_t *)xmalloc(sizeof(*rlp));
	preg = &rlp->rl_reg;

	if ((err = regcomp(preg, pattern, REG_EXTENDED|REG_NOSUB)) != 0) {
		char	buf[128];

		(void)regerror(err, preg, buf, sizeof(buf));
		fatal(0, "Invalid DT_NEEDED pattern \"%s\": %s",
		      pattern, buf);
	}

	rlp->rl_next = NULL;
	*RemoveNeededNext = rlp;
	RemoveNeededNext = &rlp->rl_next;
}

/*
 * void
 * symtab_update_dynamic(elfimg_t *src, elfimg_t *dst, Elf_Data *sdata,
 *			 Elf_Data *ddata, GElf_Shdr *shdr)
 *	Update .dynamic section.
 *	This function does the followings:
 *
 *	  - Eliminate DT_NEEDED entry.
 *	  - Append DT_SUNW_RTLDINF entry.
 */
void
symtab_update_dynamic(elfimg_t *src, elfimg_t *dst, Elf_Data *sdata,
		      Elf_Data *ddata, GElf_Shdr *shdr)
{
	int	i, num, idx, nulldone = 0;
	GElf_Dyn	dyn;

	/* Copy original data. */
	(void)memcpy(ddata, sdata, sizeof(Elf_Data));

	if (RemoveNeeded == NULL && RtldInfoValue == 0) {
		/* We can simply copy source. */
		return;
	}

	/* Allocate data buffer for new .dynamic. */
	ddata->d_buf = xmalloc(ddata->d_size);
	num = sdata->d_size / shdr->sh_entsize;

	if (RtldInfoValue != 0) {
		int	found = 0;

		/* Check whether specified tag exists in .dynamic section. */
		for (i = 0, idx = 0; i < num; i++) {
			if (gelf_getdyn(sdata, i, &dyn) == NULL) {
				elfdie(src->e_filename,
				       "Can't read dynamic entry at %d", i);
			}
			if ((uint_t)dyn.d_tag == (uint_t)RtldInfoTag) {
				found++;
			}
		}

		if (found == 0) {
			fatal(0, "Tag 0x%llx is not found in .dynamic.\n",
			      RtldInfoTag);
		}
		else if (found > 1) {
			fatal(0, "Too many tag 0x%llx in .dynamic.\n",
			      RtldInfoTag);
		}
	}

	/* Iterate entries in .dynamic section. */
	for (i = 0, idx = 0; i < num; i++) {
		if (gelf_getdyn(sdata, i, &dyn) == NULL) {
			elfdie(src->e_filename,
			       "Can't read dynamic entry at %d", i);
		}

		if (symtab_check_dyn(src, shdr, &dyn)) {
			/* Copy to new dynamic section. */

			if (RtldInfoValue != 0) {
				if ((uint_t)dyn.d_tag == (uint_t)RtldInfoTag ||
				    dyn.d_tag == DT_NULL) {
					/*
					 * Replace this entry with
					 * DT_SUNW_RTLDINF.
					 */
					dyn.d_tag = DT_SUNW_RTLDINF;
					dyn.d_un.d_ptr = RtldInfoValue;
					RtldInfoValue = 0;
				}
			}
			else if ((uint_t)dyn.d_tag == (uint_t)RtldInfoTag) {
				/* Replace with DT_NULL. */
				dyn.d_tag = DT_NULL;
				dyn.d_un.d_val = 0;
				nulldone = 1;
			}
			else if (dyn.d_tag == DT_NULL) {
				nulldone = 1;
			}

			if (gelf_update_dyn(ddata, idx, &dyn) == 0) {
				elfdie(dst->e_filename,
				       "Can't update dynamic entry at %d",
				       idx);
			}
			idx++;
		}
	}

	/*
	 * We don't change section size.
	 * symfilter can't expand ALLOC'ed section.
	 */
	if (nulldone == 0) {
		fatal(0, "No room in .dynamic section.");
	}
}

/*
 * static boolean_t
 * symtab_check_dyn(elfimg_t *src, GElf_Shdr *shdr, GElf_Dyn *dyn)
 *	Determine whether the specified dynamic entry should be removed.
 */
static boolean_t
symtab_check_dyn(elfimg_t *src, GElf_Shdr *shdr, GElf_Dyn *dyn)
{
	regex_list_t	*rlp;
	char		*libname;

	if (dyn->d_tag != DT_NEEDED) {
		/* Do not remove this entry. */
		return B_TRUE;
	}

	/*
	 * Check whether this DT_NEEDED matches the specified pattern
	 * to be removed.
	 */
	libname = elf_strptr(src->e_elf, shdr->sh_link, dyn->d_un.d_val);
	if (libname == NULL) {
		elfdie(src->e_filename, "Can't find library name: %d",
		       dyn->d_un.d_val);
	}
	for (rlp = RemoveNeeded; rlp != NULL; rlp = rlp->rl_next) {
		if (regexec(&rlp->rl_reg, (const char *)libname, 0, NULL, 0)
		    == 0) {
			/* This entry should be removed. */
			return B_FALSE;
		}
	}

	return B_TRUE;
}
