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

/* Common header for symfilter. */

#ifndef	_TOOLS_SYMFILTER_COMMON_SYMFILTER_H
#define	_TOOLS_SYMFILTER_COMMON_SYMFILTER_H

#include <gelf.h>
#include <libelf.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/byteorder.h>
#include "elfutil.h"

#if	defined(_LITTLE_ENDIAN)
#define	NEED_BSWAP(data)	((data) == ELFDATA2MSB)
#elif defined(_BIG_ENDIAN)
#define	NEED_BSWAP(data)	((data) == ELFDATA2LSB)
#else	/* !_LITTLE_ENDIAN && !_BIG_ENDIAN */
#error	"Can't determine byte order"
#endif	/* defined(_LITTLE_ENDIAN) */

/* Section header and its index */
struct scinfo;
typedef struct scinfo	scinfo_t;

struct scinfo {
	int		si_index;	/* Section index */
	Elf_Scn		*si_scn;	/* Section descriptor */
	GElf_Shdr	si_shdr;	/* Section header */
	Elf_Data	*si_data;	/* Contents of section */
	GElf_Addr	si_addr;	/* Base address */
	GElf_Word	si_size;	/* Section size */
	GElf_Off	si_offset;	/* File offset */
	void		*si_newdata;	/* Updated data */
	Elf_Scn		*si_dscn;	/* Descriptor for new section */
	scinfo_t	*si_next;
};

/* Elf image information */
typedef struct elfimg {
	int		e_fd;			/* File descriptor */
	char		*e_filename;		/* Filename */
	Elf		*e_elf;			/* Elf descriptor */
} elfimg_t;

extern GElf_Xword	RtldInfoTag;
extern GElf_Addr	RtldInfoValue;

/* Flags for global options */
#define	SYMF_NOCOMP		0x1	/* Do not compress .strtab */
#define	SYMF_UNIXHACK		0x2	/* Special hack for static kernel */
#define	SYMF_NOTYPEHACK		0x4	/* Apply "NOTYPE" hack */
#define	SYMF_CTFSTRIP		0x8	/* Strip CTF data */

/* Simple integer hash table */
struct ihashent;
typedef struct ihashent	ihashent_t;

struct inthash;
typedef struct inthash	inthash_t;

struct inthash {
	ihashent_t	**ih_table;	/* hash bucket */
	size_t		ih_size;	/* number of hash bucket entries */
};

/* Prototypes */
extern void	rule_init(void);
extern void	rule_add(char *rule);
extern uint_t	rule_import(char *file);
extern int	rule_eval(GElf_Sym *symp, char *name, int exclude);
extern void	rule_subst(GElf_Sym *symp, char *name);
extern int	rule_nohash(GElf_Sym *symp, char *name);

extern void	symtab_init(char *input, Elf *elfp, GElf_Shdr *symtab,
			    Elf_Data *symdata, int bssidx, GElf_Shdr *bss,
			    uint_t flags);
extern void	symtab_add(GElf_Word index, GElf_Sym *symp, char *name);
extern size_t	symtab_oldsymnum(void);
extern size_t	symtab_newsymnum(void);
extern size_t	symtab_newstrnum(void);
extern void	symtab_update_symtab(elfimg_t *dst, Elf_Data *data,
				     GElf_Shdr *shdr, int *xlate);
extern void	symtab_update_str(elfimg_t *dst, Elf_Data *data,
				  GElf_Shdr *shdr);
extern void	symtab_update_sym(elfimg_t *src, elfimg_t *dst,
				  Elf_Data *sdata, Elf_Data *ddata,
				  GElf_Shdr *shdr, GElf_Addr diffbase,
				  GElf_Off diff, int *xlate);
extern void	symtab_update_rel(elfimg_t *src, elfimg_t *dst,
				  Elf_Data *sdata, Elf_Data *ddata,
				  GElf_Shdr *shdr, int *xlate);
extern void	symtab_update_ctf(elfimg_t *src, elfimg_t *dst,
				  Elf_Data *sdata, Elf_Data *ddata,
				  GElf_Shdr *shdr, int symidx);
extern void	symtab_fixup(elfimg_t *src, int bssidx, uint_t flags);
extern void	symtab_addhash(elfimg_t *dst, GElf_Off off, int nameoff,
			       GElf_Shdr *shdr, uint_t enc, int symidx);
extern void	symtab_hashinfo(Elf *elf, char *file, int fd, GElf_Ehdr *ehdr);
extern void	symtab_remove_needed_add(const char *pattern);
extern void	symtab_update_dynamic(elfimg_t *src, elfimg_t *dst,
				      Elf_Data *sdata, Elf_Data *ddata,
				      GElf_Shdr *shdr);

extern void	reloc_bss(elfimg_t *src, elfimg_t *dst, Elf_Data *sdata,
			  Elf_Data *ddata, GElf_Shdr *shdr, GElf_Addr diffbase,
			  GElf_Addr endaddr, GElf_Off diff, scinfo_t *scinfo);
extern void	reloc_referred_symbols(char *file, GElf_Shdr *shdr,
				       Elf_Data *data, inthash_t *hash);

extern void		inthash_init(inthash_t *hash, size_t size);
extern boolean_t	inthash_put(inthash_t *hash, uint_t value);
extern boolean_t	inthash_exists(inthash_t *hash, uint_t value);

#endif	/* !_TOOLS_SYMFILTER_COMMON_SYMFILTER_H */
