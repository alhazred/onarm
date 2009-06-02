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

#ident	"@(#)tools/symfilter/common/symfilter.c"

/*
 * symfilter:	.symtab filtering tool.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include "symfilter.h"
#include "unixhack.h"

#define	PROGNAME	"symfilter"

#define	SHDR_ISRELOC(shdr)						\
	((shdr)->sh_type == SHT_REL || (shdr)->sh_type == SHT_RELA)

struct flist;
typedef struct flist	flist_t;
struct flist {
	char	*f_path;
	flist_t	*f_next;
};

/* Name of temporary file */
static char	TmpFile[PATH_MAX];
static char	TmpFile1[PATH_MAX];
static char	*CleanFile;
static char	*CleanFile1;

static char	*RemoveSource;

/* Internal Prototypes */
static void	usage(int status);
static void	cleanup(void);
static int	*rmsct_add(int *rmsct, int index);
static void	write_elf(char *srcfile, char *dstfile, int symidx, int stridx,
			  int bssidx, int *rmsct, uint_t flags);
static int	is_rel_alloc(GElf_Shdr *shdr, scinfo_t **stable);
static void	do_unixhack(char *file, mode_t mode);
static scinfo_t	*scinfo_get(Elf *elf, GElf_Ehdr *ehdr, char *input,
			    int **xlatep, uint_t *nump, int *removed,
			    uint_t flags);
static void	switch_symtab(char *srcfile, char *dstfile);

GElf_Xword	RtldInfoTag;	/* .dynamic tag to be replaced with RTLDINFO */
GElf_Addr	RtldInfoValue;	/* Value of RTLDINFO */

int
main(int argc, char **argv)
{
	int		c, fd, exclude = 0, sidx, symfound = -1, unixhash = 0;
	int		dynsym = 0, bssidx, *rmsct = NULL, rmsrc = 0;
	char		*output = NULL, *input, *rtldinfo = NULL, *p;
	uint_t		i, flags = 0;
	Elf		*elf;
	GElf_Ehdr	ehdr;
	GElf_Shdr	symshdr, *symshdrp = NULL;
	GElf_Shdr	bssshdr, *bss = NULL;
	GElf_Off	minreloff = (GElf_Off)-1, maxallocoff = 0;
	Elf_Data	*symdata;
	size_t		nent, dropped = 0;
	pid_t		pid = getpid();
	inthash_t	rsymhash;

	rule_init();

	while ((c = getopt(argc, argv, "CHERXNDZL:he:f:o:r:v")) != EOF) {
		switch (c) {
		case 'h':
			usage(0);
			break;

		case 'e':
			/* Append filtering rule. */
			rule_add(optarg);
			break;

		case 'f':
			/* Import rules from file. */
			(void)rule_import(optarg);
			break;

		case 'o':
			if (output != NULL) {
				usage(2);
			}
			output = optarg;
			break;

		case 'r':
			if (rtldinfo != NULL) {
				usage(2);
			}
			if ((p = strchr(optarg, ':')) == NULL) {
				usage(2);
			}
			*p = '\0';
			rtldinfo = p + 1;
			errno = 0;
			RtldInfoTag = strtoull(optarg, &p, 0);
			if (*p != '\0' || errno != 0) {
				fatal(0, "Invalid tag value: %s", optarg);
			}
			break;

		case 'v':
			Verbose++;
			break;

		case 'C':
			flags |= SYMF_CTFSTRIP;
			break;

		case 'N':
			flags |= SYMF_NOCOMP;
			break;

		case 'E':
			exclude = 1;
			break;

		case 'X':
			flags |= SYMF_UNIXHACK;
			break;

		case 'H':
			unixhash = 1;
			break;

		case 'D':
			dynsym = 1;
			break;

		case 'R':
			rmsrc = 1;
			break;

		case 'Z':
			flags |= SYMF_NOTYPEHACK;
			break;

		case 'L':
			/* Eliminate DT_NEEDED. */
			symtab_remove_needed_add((const char *)optarg);
			break;

		default:
			usage(2);
		}
	}

	if ((flags & (SYMF_UNIXHACK|SYMF_CTFSTRIP)) ==
	    (SYMF_UNIXHACK|SYMF_CTFSTRIP)) {
		fatal(0, "Unix hack mode doesn't support -C option");
	}

	(void)elf_version(EV_CURRENT);

	if (optind != argc - 1) {
		usage(2);
	}
	input = *(argv + optind);
	if (rmsrc) {
		RemoveSource = input;
	}

	if (dynsym) {
		/* Change section type of .symtab. */
		switch_symtab(input, output);
		RemoveSource = NULL;
		return 0;
	}

	if ((fd = open(input, O_RDONLY)) == -1) {
		fatal(errno, "Can't open \"%s\"", input);
	}

	/* Create name of temporary file. */
	if (output == NULL) {
		(void)snprintf(TmpFile, sizeof(TmpFile), "%s_sym.%d",
			       input, pid);
	}
	else {
		(void)snprintf(TmpFile, sizeof(TmpFile), "%s_sym.%d",
			       output, pid);
	}

	/* Read symbol table. */
	if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
		elfdie(input, "Can't read file");
	}
	if (elf_kind(elf) != ELF_K_ELF) {
		fatal(0, "\"%s\" is a not ELF object", input);
	}

	if (gelf_getehdr(elf, &ehdr) == NULL) {
		elfdie(input, "Can't read ELF header");
	}

	if (unixhash) {
		/* Show statistics of static kernel symbol hash. */
		symtab_hashinfo(elf, input, fd, &ehdr);
		RemoveSource = NULL;
		return 0;
	}

	for (sidx = 0; sidx < ehdr.e_shnum; sidx++) {
		Elf_Scn		*scn;
		GElf_Shdr	shdr;
		char		*sname;

		if ((scn = elf_getscn(elf, sidx)) == NULL) {
			elfdie(input, "Can't get section descriptor at %d",
			       sidx);
		}
		if (gelf_getshdr(scn, &shdr) == NULL) {
			elfdie(input, "Can't read section header at %d", sidx);
		}
		if (shdr.sh_type == SHT_SYMTAB) {
			if (symfound >= 0) {
				fatal(0, "Two (or more) symbol tables exist");
			}
			symfound = sidx;
			(void)memcpy(&symshdr, &shdr, sizeof(shdr));
			symshdrp = &symshdr;
			if ((symdata = elf_getdata(scn, NULL)) == NULL) {
				elfdie(input, "Can't read symbol table");
			}
		}
		else if (shdr.sh_type == SHT_NOBITS) {
			if (bss != NULL) {
				fatal(0, "Two (or more) BSS exist");
			}
			(void)memcpy(&bssshdr, &shdr, sizeof(shdr));
			bss = &bssshdr;
			bssidx = sidx;
		}

		sname = elf_strptr(elf, ehdr.e_shstrndx, shdr.sh_name);
		if (sname == NULL) {
			elfdie(input, "Can't find section name: %d",
			       shdr.sh_name);
		}
		if (flags & SYMF_CTFSTRIP) {
			/* Remove CTF data. */
			if (strcmp(sname, SHNAME_CTF) == 0) {
				rmsct = rmsct_add(rmsct, sidx);
				verbose(1, "Remove CTF data section at %d",
					sidx);
			}
		}
		else if (flags & SYMF_UNIXHACK) {
			if (shdr.sh_type == SHT_HASH ||
			    SECTION_IS_UNIX_SYMHASH(&shdr, sname)) {
				fatal(0, "This image already has symbol hash "
				      "section");
			}
			if (shdr.sh_type == SHT_REL &&
			    minreloff > shdr.sh_offset) {
				minreloff = shdr.sh_offset;
			}
			if ((shdr.sh_flags & SHF_ALLOC) &&
			    shdr.sh_type != SHT_NOBITS) {
				GElf_Off	maxoff =
					shdr.sh_offset + shdr.sh_size;

				if (maxallocoff < maxoff) {
					maxallocoff = maxoff;
				}
			}
		}
	}

	if ((flags & SYMF_UNIXHACK) && minreloff < maxallocoff) {
		fatal(0, "REL section located before ALLOC section.");
	}

	if (symshdrp == NULL) {
		fatal(0, "No symbol table in \"%s\"", input);
	}

	/* Initialize new symbol table. */
	symtab_init(input, elf, symshdrp, symdata, bssidx, bss, flags);

	/*
	 * Collect symbols referred by relocation entry.
	 * If the file is static kernel, we don't need to consider
	 * relocation entry because it will be removed later.
	 */
	if ((flags & SYMF_UNIXHACK) == 0) {
		inthash_init(&rsymhash, 0);

		for (sidx = 0; sidx < ehdr.e_shnum; sidx++) {
			Elf_Scn		*scn;
			GElf_Shdr	shdr;

			if ((scn = elf_getscn(elf, sidx)) == NULL) {
				elfdie(input,
				       "Can't get section descriptor at %d",
				       sidx);
			}
			if (gelf_getshdr(scn, &shdr) == NULL) {
				elfdie(input,
				       "Can't read section header at %d",
				       sidx);
			}
			if (shdr.sh_type == SHT_REL &&
			    shdr.sh_link == symfound) {
				Elf_Data	*data;

				if ((data = elf_getdata(scn, NULL)) == NULL) {
					elfdie(input,
					       "Can't read section data at %d",
					       sidx);
				}
				reloc_referred_symbols(input, &shdr, data,
						       &rsymhash);
			}
		}
	}

	/*
	 * Iterate symbol table entries, and evaluate filter rules.
	 */
	verbose(1, "Do filtering");
	nent = symshdrp->sh_size / symshdrp->sh_entsize;
	for (i = 0; i < nent; i++) {
		GElf_Sym	sym;
		char		*name;
		int		include = 0;

		if (gelf_getsym(symdata, i, &sym) == NULL) {
			elfdie(input, "Can't read symbol at %d", i);
		}

		name = elf_strptr(elf, symshdrp->sh_link, sym.st_name);
		if (name == NULL) {
			elfdie(input, "No symbol string at 0x%lx",
			       sym.st_name);
		}

		/*
		 * The first symbol entry, may be NULL symbol, should not be
		 * removed. And any symbols referred by relocation entry
		 * should not be removed.
		 */
		if (i == 0 || rule_eval(&sym, name, exclude)) {
			include = 1;
		}
		else if ((flags & SYMF_UNIXHACK) == 0 &&
			 inthash_exists(&rsymhash, i)) {
			include = 1;
			verbose(1, "[%d] %s: referred by reloc entry",
				i, name);
		}
		if (include) {
			symtab_add(i, &sym, name);
			if (rtldinfo != NULL && strcmp(name, rtldinfo) == 0) {
				if (RtldInfoValue != 0 &&
				    sym.st_value != RtldInfoValue) {
					fatal(0, "RTLDINFO must be a unique "
					      "symbol: 0x%llx: 0x%llx",
					      sym.st_value, RtldInfoValue);
				}
				RtldInfoValue = sym.st_value;
				verbose(1, "RTLDINFO = 0x%llx, tag = 0x%llx",
					RtldInfoValue, RtldInfoTag);
			}
		}
		else {
			dropped++;
			verbose(2, "DROP: %s", name);
		}
	}
	(void)elf_end(elf);

	verbose(1, "Number of symbols = %d, dropped = %d", nent, dropped);

	/* Create new ELF file. */
	if (atexit(cleanup) == -1) {
		fatal(errno, "atexit() failed");
	}

	CleanFile = TmpFile;
	write_elf(input, TmpFile, symfound, symshdrp->sh_link, bssidx, rmsct,
		  flags);

	/* Move new file to the specified path. */
	if (output == NULL) {
		output = input;
	}
	move_file(TmpFile, output);

	CleanFile = NULL;
	RemoveSource = NULL;

	return 0;
}

static void
usage(int status)
{
	FILE	*out = (status == 0) ? stdout : stderr;

	(void)fprintf(out, "Usage: " PROGNAME " [options] ELFfile\n");
	(void)fprintf(out, "       " PROGNAME " -h\n");
	if (status != 0) {
		exit(status);
	}
	(void)fprintf(out, "\nOptions:\n"
		      "  -e pattern\n"
		      "      Specify filtering patterm.\n"
		      "  -f file\n"
		      "      Specify file path that contains patterns.\n"
		      "  -o output\n"
		      "      Specify output file path.\n"
		      "  -r tag:symbol\n"
		      "      Replace tag in .dynamic with DT_SUNW_RTLDINF.\n"
		      "  -C\n"
		      "      Strip CTF data.\n"
		      "  -E\n"
		      "      Exclude all symbols by default.\n"
		      "  -D\n"
		      "      Change .symtab into .dynsym.\n"
		      "  -R\n"
		      "      Remove source file on error.\n"
		      "  -Z\n"
		      "      Special hack for notype symbols.\n"
		      "  -X\n"
		      "      Special hack for static kernel executable.\n"
		      "  -H\n"
		      "      Show statistics of static kernel symbol "
		      "hash.\n"
		      "  -L pattern\n"
		      "      Eliminate DT_NEEDED that match the pattern.\n ");
	RemoveSource = NULL;
	exit(0);
}

static void
cleanup(void)
{
	Exiting = 1;

	if (CleanFile != NULL) {
		verbose(1, "unlink(%s)", CleanFile);
		(void)unlink(CleanFile);
	}
	if (CleanFile1 != NULL) {
		verbose(1, "unlink(%s)", CleanFile1);
		(void)unlink(CleanFile1);
	}
	if (RemoveSource != NULL) {
		verbose(1, "unlink(%s)", RemoveSource);
		(void)unlink(RemoveSource);
	}
}

/*
 * static int *
 * rmsct_add(int *rmsct, int index)
 *	Append the specified section index into "rmsct" array.
 *	This function is used to keep section index to be removed.
 */
static int *
rmsct_add(int *rmsct, int index)
{
	int	*rmp;
	size_t	cnt, newsize;

	for (rmp = rmsct, cnt = 0; rmp != NULL && *rmp != -1; rmp++, cnt++);
	newsize = sizeof(*rmsct) * (cnt + 2);

	rmsct = (int *)xrealloc(rmsct, newsize);
	*(rmsct + cnt) = index;
	*(rmsct + cnt + 1) = -1;

	return rmsct;
}

/*
 * static void
 * write_elf(char *srcfile, char *dstfile, int symidx, int stridx, int bssidx,
 *	     int *rmsct, uint_t flags)
 *	Create new ELF file with updating symbol table.
 *	symidx is a section index for .symtab, and stridx is for string table
 *	for .symtab, and bssidx is for .bss section.
 *
 *	"rmsct" is an array of section index to be removed. The last element
 *	must be -1. NULL can be specified to "rmsct" if we don't need to
 *	remove any section. Note that write_elf() releases buffer set to
 *	"rmsct".
 */
static void
write_elf(char *srcfile, char *dstfile, int symidx, int stridx, int bssidx,
	  int *rmsct, uint_t flags)
{
	Elf		*self, *delf;
	int		sfd, dfd, i, *xlate, hashnameoff, ctfupdate = 0;
	GElf_Ehdr	sehdr, dehdr;
	GElf_Off	newoff;
	size_t		align, nsymnum, osymnum;
	uint_t		newnsct;
	scinfo_t	*scinfo, *sip;
	struct stat	st;
	elfimg_t	src, dst;

	verbose(1, "src = \"%s\", dst = \"%s\"", srcfile, dstfile);
	if ((sfd = open(srcfile, O_RDONLY)) == -1) {
		fatal(errno, "Can't open \"%s\"", srcfile);
	}
	if (fstat(sfd, &st) == -1) {
		fatal(errno, "stat(\"%s\") failed", srcfile);
	}

	if ((dfd = open(dstfile, O_RDWR|O_CREAT|O_TRUNC|O_EXCL,
			st.st_mode)) == -1) {
		fatal(errno, "Can't open \"%s\"", dstfile);
	}

	if ((self = elf_begin(sfd, ELF_C_READ, NULL)) == NULL) {
		elfdie(srcfile, "Can't read file");
	}
	if ((delf = elf_begin(dfd, ELF_C_WRITE, NULL)) == NULL) {
		elfdie(srcfile, "Can't write file");
	}

	/* Create new ELF header. */
	if (gelf_newehdr(delf, gelf_getclass(self)) == NULL) {
		elfdie(dstfile, "Can't copy ELF header");
	}
	if (gelf_getehdr(self, &sehdr) == NULL) {
		elfdie(srcfile, "Can't read ELF header");
	}
	(void)memcpy(&dehdr, &sehdr, sizeof(sehdr));

	src.e_fd = sfd;
	src.e_filename = srcfile;
	src.e_elf = self;
	dst.e_fd = dfd;
	dst.e_filename = dstfile;
	dst.e_elf = delf;

	/* Check whether new symbol table exists or not. */
	if ((nsymnum = symtab_newsymnum()) == 0 || symtab_newstrnum() == 0) {
		/* Strip symbol table. */
		if (flags & SYMF_UNIXHACK) {
			fatal(0, "Symbols for kernel must not be stripped");
		}
		verbose(1, "Strip symbol table.");

		rmsct = rmsct_add(rmsct, symidx);
		rmsct = rmsct_add(rmsct, stridx);
	}
	else {
		/* Fix up symbol table. */
		symtab_fixup(&src, bssidx, flags);
	}

	/*
	 * If a symbol entry is removed, we need to update CTF data because
	 * it depends on order of symbol entries.
	 */
	osymnum = symtab_oldsymnum();
	ctfupdate = (nsymnum != osymnum);
	verbose(1, "CTF update => %d (nsyms=%d, osyms=%d)", ctfupdate,
		nsymnum, osymnum);

	/* Determine order of new section headers. */
	scinfo = scinfo_get(self, &sehdr, srcfile, &xlate, &newnsct,
			    rmsct, flags);
	xfree(rmsct);

	/*
	 * Copy program headers.
	 * This function treats updating .symtab and .strtab that don't have
	 * SHF_ALLOC flag. Although this function may append .hash section
	 * for vmunix, the static vmunix loader never uses program header.
	 * So we can just copy program headers.
	 */
	if (sehdr.e_phnum > 0) {
		/* We must determine all file positions by outselves. */
		(void)elf_flagelf(delf, ELF_C_SET, ELF_F_LAYOUT);
		if (gelf_newphdr(delf, sehdr.e_phnum) == NULL) {
			elfdie(dstfile, "Can't create program headers");
		}
		for (i = 0; i < sehdr.e_phnum; i++) {
			GElf_Phdr	phdr;

			if (gelf_getphdr(self, i, &phdr) == NULL) {
				elfdie(srcfile,
				       "Can't read program header at %d", i);
			}
			if (gelf_update_phdr(delf, i, &phdr) == NULL) {
				elfdie(dstfile,
				       "Can't write program header at %d", i);
			}
		}
	}

	/* Copy all sections. NULL section will be added automatically. */
	sip = scinfo->si_next;
	newoff = 0;
	for (i = 1; sip != NULL; i++, sip = sip->si_next) {
		Elf_Scn		*sscn, *dscn;
		GElf_Shdr	*shdr;
		Elf_Data	*ddata;
		int		sidx;

		sscn = sip->si_scn;
		shdr = &(sip->si_shdr);
		sidx = sip->si_index;

		if ((dscn = elf_newscn(delf)) == NULL) {
			elfdie(dstfile, "Can't create new section for %d", i);
		}
		sip->si_dscn = dscn;
		sip->si_offset = shdr->sh_offset;
		sip->si_size = shdr->sh_size;

		if ((ddata = elf_newdata(dscn)) == NULL) {
			elfdie(dstfile,
			       "Can't create new section data for %d", i);
		}

		if (sidx == symidx) {
			/* Update symbol table. */
			verbose(1, "%d: Update .symtab", i);
			symtab_update_symtab(&dst, ddata, shdr, xlate);
		}
		else if (sidx == stridx) {
			/* Update string table. */
			verbose(1, "%d: Update .strtab", i);
			symtab_update_str(&dst, ddata, shdr);
		}
		else {
			Elf_Data	*sdata;
			char		*sname;

			sname = elf_strptr(self, sehdr.e_shstrndx,
					   shdr->sh_name);
			if ((sdata = elf_getdata(sscn, NULL)) == NULL) {
				elfdie(srcfile,
				       "Can't read section data at %d", sidx);
			}
			if (shdr->sh_type == SHT_DYNSYM) {
				/* Update .dynsym. */
				verbose(1, "%d: Update .dynsym", i);
				symtab_update_sym(&src, &dst, sdata, ddata,
						  shdr, NULL, 0, xlate);
			}
			else if (shdr->sh_type == SHT_REL &&
				 shdr->sh_link == symidx) {
				/* Update relocation entry. */
				verbose(1, "%d: Update reloc section", i);
				symtab_update_rel(&src, &dst, sdata, ddata,
						  shdr, xlate);
			}
			else if (shdr->sh_type == SHT_DYNAMIC) {
				/* Update dynamic entry. */
				verbose(1, "%d: Update dynamic section", i);
				symtab_update_dynamic(&src, &dst, sdata, ddata,
						      shdr);
			}
			else if (ctfupdate && strcmp(sname, SHNAME_CTF) == 0) {
				/* Update CTF data. */
				verbose(1, "%d: Update CTF section", i);
				symtab_update_ctf(&src, &dst, sdata, ddata,
						  shdr, symidx);
			}
			else {
				/* Copy original data. */
				(void)memcpy(ddata, sdata, sizeof(Elf_Data));
			}

			if (sidx == sehdr.e_shstrndx) {
				/* Update index for .shstrtab. */
				dehdr.e_shstrndx = i;

				if (flags & SYMF_UNIXHACK) {
					char	*nm;
					size_t	ssz;

					/*
					 * Append section name for new symbol
					 * hash.
					 */
					ssz = shdr->sh_size +
						SHNAMELEN_UNIX_SYMHASH + 1;
					nm = (char *)xmalloc(ssz);
					(void)memcpy(nm, sdata->d_buf,
						     shdr->sh_size);
					(void)strcpy(nm + shdr->sh_size,
						     SHNAME_UNIX_SYMHASH);
					hashnameoff = shdr->sh_size;
					shdr->sh_size = ssz;
					ddata->d_size = ssz;
					ddata->d_buf = nm;
				}
			}
		}

		/*
		 * Adjust file offset of this section.
		 * We don't want to change offset for SHF_ALLOC'ed section.
		 */
		if (shdr->sh_flags & SHF_ALLOC) {
			newoff = shdr->sh_offset;
		}
		else {
			if (newoff == 0) {
				fatal(0, "Can't determine new offset for "
				      "section %d", i);
			}
			if (shdr->sh_addralign > 1) {
				newoff = roundup(newoff, shdr->sh_addralign);
			}
		}
		verbose(1, "Section %d: offset=0x%llx -> 0x%llx, size=0x%llx",
			i, shdr->sh_offset, newoff, shdr->sh_size);
		shdr->sh_offset = newoff;

		/* Translate section index link. */
		if (shdr->sh_link != 0) {
			verbose(1, "link: %d -> %d", 
				shdr->sh_link, *(xlate + shdr->sh_link));
			shdr->sh_link = *(xlate + shdr->sh_link);
		}
		if (SHDR_ISRELOC(shdr) && shdr->sh_info != 0) {
			verbose(1, "info: %d -> %d",
				shdr->sh_info, *(xlate + shdr->sh_info));
			shdr->sh_info = *(xlate + shdr->sh_info);
		}

		/* Update section header. */
		if (gelf_update_shdr(dscn, shdr) == NULL) {
			elfdie(dstfile, "Can't update section at %d", i);
		}

		if (shdr->sh_type != SHT_NOBITS) {
			newoff = shdr->sh_offset + shdr->sh_size;
		}
	}

	if (flags & SYMF_UNIXHACK) {
		GElf_Shdr	hshdr;

		/* Insert symbol hash section here. */
		symtab_addhash(&dst, newoff, hashnameoff, &hshdr,
			       dehdr.e_ident[EI_DATA], *(xlate + symidx));
		newoff = hshdr.sh_offset + hshdr.sh_size;
	}

	/*
	 * Update the location of section headers.
	 * No need to change program header offset.
	 */
	align = gelf_fsize(delf, ELF_T_ADDR, 1, EV_CURRENT);
	if (align <= 1) {
		align = 4;
	}
	newoff = roundup(newoff, align);
	verbose(1, "New section header offset: 0x%llx -> 0x%llx",
		sehdr.e_shoff, newoff);
	dehdr.e_shoff = newoff;

	if (sehdr.e_shnum != newnsct) {
		verbose(1, "New number of section headers: %d", newnsct);
		dehdr.e_shnum = newnsct;
	}
	verbose(1, "New shdr string index: %d", dehdr.e_shstrndx);

	if (gelf_update_ehdr(delf, &dehdr) == NULL) {
		elfdie(dstfile, "Can't update ELF header");
	}

	for (; scinfo != NULL; scinfo = sip) {
		sip = scinfo->si_next;
		xfree(scinfo);
	}
	xfree(xlate);

	/* Commit changes. */
	if (elf_update(delf, ELF_C_WRITE) < 0) {
		elfdie(dstfile, "Can't update ELF file image");
	}

	(void)elf_end(self);
	(void)elf_end(delf);
	(void)close(sfd);
	(void)close(dfd);

	if (flags & SYMF_UNIXHACK) {
		do_unixhack(dstfile, st.st_mode);
	}
}

/*
 * static int
 * is_rel_alloc(GElf_Shdr *shdr, scinfo_t **stable)
 *	Determine wheter the given section is a reloc section to relocate
 *	SHF_ALLOC section.
 */
static int
is_rel_alloc(GElf_Shdr *shdr, scinfo_t **stable)
{
	int	ret;

	if (SHDR_ISRELOC(shdr) && shdr->sh_info != 0) {
		scinfo_t	*sip = *(stable + shdr->sh_info);

		ret = (sip->si_shdr.sh_flags & SHF_ALLOC) ? 1 : 0;
	}
	else {
		ret = 0;
	}

	return ret;
}

/*
 * static void
 * do_unixhack(char *file, mode_t mode)
 *	Apply special hack for static kernel image.
 */
static void
do_unixhack(char *file, mode_t mode)
{
	Elf		*self, *delf;
	GElf_Ehdr	ehdr, dehdr;
	GElf_Addr	oldbss, oldbssend, lastoff, baseaddr = 0;
	GElf_Off	*offxlate, *oxlp, off, bssdiff;
	int		sfd, dfd, *xlate, i, bssidx = -1, newbssidx;
	scinfo_t	**stable, **stable1, *sip, **sipp, **sipp1, *list;
	scinfo_t	*symsip, *strsip, *bsssip, *hashsip, *ctfsip;
	char		*dstfile;
	elfimg_t	src, dst;
	size_t		align;

	verbose(1, "Do static unix hack");

	if ((sfd = open(file, O_RDONLY)) == -1) {
		fatal(errno, "Can't open \"%s\"", file);
	}
	if ((self = elf_begin(sfd, ELF_C_READ, NULL)) == NULL) {
		elfdie(file, "Can't read file");
	}

	if (gelf_getehdr(self, &ehdr) == NULL) {
		elfdie(file, "Can't read ELF header");
	}

	if (ehdr.e_phnum == 0) {
		fatal(0, "No program header");
	}

	/* Read all section headers. */
	stable = (scinfo_t **)xmalloc(sizeof(scinfo_t *) * ehdr.e_shnum);
	symsip = strsip = bsssip = hashsip = ctfsip = NULL;
	for (i = 0, sipp = stable; i < ehdr.e_shnum; i++, sipp++) {
		GElf_Shdr	*shdr;

		sip = (scinfo_t *)xmalloc(sizeof(*sip));
		if ((sip->si_scn = elf_getscn(self, i)) == NULL) {
			elfdie(file, "Can't get section descriptor at %d", i);
		}
		shdr = &(sip->si_shdr);
		if (gelf_getshdr(sip->si_scn, shdr) == NULL) {
			elfdie(file, "Can't read section header at %d", i);
		}
		if (shdr->sh_type == SHT_NOBITS) {
			bssidx = i;
			oldbss = shdr->sh_addr;
			verbose(1, "Old BSS start address = 0x%llx", oldbss);
		}
		sip->si_index = i;
		sip->si_addr = shdr->sh_addr;
		sip->si_size = shdr->sh_size;
		sip->si_offset = shdr->sh_offset;
		*sipp = sip;

		if (shdr->sh_type == SHT_SYMTAB) {
			symsip = sip;
			shdr->sh_flags |= SHF_ALLOC;
			if (shdr->sh_link != 0 && shdr->sh_link < i) {
				strsip = *(stable + shdr->sh_link);
				strsip->si_shdr.sh_flags |= SHF_ALLOC;
			}
		}
		else if (symsip != NULL && symsip->si_shdr.sh_link == i) {
			strsip = sip;
			shdr->sh_flags |= SHF_ALLOC;
		}
		else if (shdr->sh_type == SHT_NOBITS) {
			bsssip = sip;
		}
		else {
			char	*sname;

			sname = elf_strptr(self, ehdr.e_shstrndx,
					   shdr->sh_name);
			if (sname == NULL) {
				elfdie(file, "Can't find section name: %d",
				       shdr->sh_name);
			}
			if (SECTION_IS_UNIX_SYMHASH(shdr, sname)) {
				hashsip = sip;
				shdr->sh_flags |= SHF_ALLOC;
			}
			else if (strcmp(sname, SHNAME_CTF) == 0) {
				ctfsip = sip;
			}
		}
	}

	/*
	 * Check whether mandatory sections exist or not.
	 * Note that CTF header may not exist.
	 */
	if (bsssip == NULL) {
		fatal(0, "No BSS is found.");
	}
	if (symsip == NULL) {
		fatal(0, "No symbol table is found.");
	}
	if (strsip == NULL) {
		fatal(0, "No string table for .symtab is found.");
	}
	if (hashsip == NULL) {
		fatal(0, "No symbol hash table is found.");
	}

	/*
	 * Change order of sections.
	 * 
	 * - Move symbol table (.symtab, .strtab) and symbol hash table
	 *   (.UNIX_hash) below BSS.
	 * - Move CTF data (.SUNW_ctf) just above BSS.
	 */
	stable1 = (scinfo_t **)xmalloc(sizeof(scinfo_t *) * ehdr.e_shnum);
	bssidx = bsssip->si_index;
	newbssidx = -1;
	for (i = 0, sipp = stable, sipp1 = stable1; i < ehdr.e_shnum;
	     i++, sipp++) {
		sip = *sipp;
		if (i == bssidx) {
			*sipp1 = symsip;
			*(sipp1 + 1) = strsip;
			*(sipp1 + 2) = hashsip;
			*(sipp1 + 3) = bsssip;
			newbssidx = bssidx + 3;
			sipp1 += 4;

			if (ctfsip != NULL) {
				*sipp1 = ctfsip;
				sipp1++;
			}
		}
		else if (i < bssidx ||
			 (sip != bsssip && sip != symsip && sip != strsip &&
			  sip != hashsip && sip != ctfsip)) {
			*sipp1 = sip;
			sipp1++;
		}
	}
	if (sipp1 - stable1 != ehdr.e_shnum) {
		fatal(0, "Unexpected section index: %d, required = %d",
		      sipp1 - stable1, ehdr.e_shnum);
	}
	xfree(stable);
	stable = stable1;

	/* Construct section index translation table. */
	xlate = (int *)xmalloc(sizeof(*xlate) * ehdr.e_shnum);
	offxlate = (GElf_Off *)xmalloc(sizeof(*offxlate) * ehdr.e_shnum);
	lastoff = off = 0;
	for (i = 0, sipp = stable, oxlp = offxlate; i < ehdr.e_shnum;
	     i++, sipp++, oxlp++) {
		GElf_Shdr	*shdr;

		sip = *sipp;
		shdr = &(sip->si_shdr);
		*(xlate + sip->si_index) = i;

		if (i < bssidx) {
			*oxlp = shdr->sh_offset;
			if (shdr->sh_addr != NULL) {
				off = shdr->sh_offset + shdr->sh_size;
			}
			continue;
		}
		if (off == 0) {
			fatal(0, "No allocated section exists before BSS");
		}
		if (shdr->sh_addralign > 1) {
			off = roundup(off, shdr->sh_addralign);
		}
		*oxlp = off;

		if (shdr->sh_type == SHT_NOBITS) {
			GElf_Addr	newaddr;

			if (i != newbssidx) {
				fatal(0, "Unexpected section index for .bss: "
				      "%d", i);
			}
			if (lastoff == 0) {
				fatal(0, "Can't detect end offset of the "
				      "last allocated section");
			}

			/*
			 * Calculate difference between old and new .bss
			 * start address.
			 */
			bssdiff = lastoff -
				roundup(shdr->sh_offset, UNIX_BSS_ALIGN);
			newaddr = shdr->sh_addr + bssdiff;
			shdr->sh_addralign = UNIX_BSS_ALIGN;
			newaddr = roundup(newaddr, UNIX_BSS_ALIGN);
			bssdiff = newaddr - shdr->sh_addr;

			oldbssend = shdr->sh_addr + shdr->sh_size;
			verbose(1, "Old BSS end address = 0x%llx", oldbssend);
		}
		else {
			off += shdr->sh_size;
			if (i == newbssidx - 1) {
				lastoff = off;
				verbose(1, "lastoff = 0x%llx", lastoff);
			}
		}
	}

	verbose(1, "BSS offset change = 0x%llx", bssdiff);

	/* Create new image. */
	(void)snprintf(TmpFile1, sizeof(TmpFile1), "%s_1", file);
	if ((dfd = open(TmpFile1, O_RDWR|O_CREAT|O_TRUNC|O_EXCL, mode))
	    == -1) {
		fatal(errno, "open(\"%s\") failed", file);
	}
	dstfile = CleanFile1 = TmpFile1;

	if ((delf = elf_begin(dfd, ELF_C_WRITE, NULL)) == NULL) {
		elfdie(dstfile, "Can't read file");
	}

	/* Create new ELF header. */
	if (gelf_newehdr(delf, gelf_getclass(self)) == NULL) {
		elfdie(dstfile, "Can't copy ELF header");
	}
	(void)memcpy(&dehdr, &ehdr, sizeof(ehdr));

	/* Update program header. */
	(void)elf_flagelf(delf, ELF_C_SET, ELF_F_LAYOUT);
	if (gelf_newphdr(delf, ehdr.e_phnum) == NULL) {
		elfdie(dstfile, "Can't create program headers");
	}
	for (i = 0; i < ehdr.e_phnum; i++) {
		GElf_Phdr	phdr;

		if (gelf_getphdr(self, i, &phdr) == NULL) {
			elfdie(file, "Can't read program header at %d", i);
		}

		if (baseaddr == 0) {
			baseaddr = phdr.p_vaddr - phdr.p_offset;
			verbose(1, "baseaddr = 0x%llx", baseaddr);
		}
		else if (phdr.p_vaddr - phdr.p_offset != baseaddr) {
			fatal(0, "phdr[%d]: All region must be mapped at "
			      "sequential address range", i);
		}

		if (phdr.p_vaddr <= oldbss &&
		    phdr.p_vaddr + phdr.p_memsz >= oldbssend) {
			verbose(1, "%d: phdr: filesz = 0x%llx->0x%llx, "
				"memsz = 0x%llx-> 0x%llx", i,
				phdr.p_filesz, lastoff,
				phdr.p_memsz, phdr.p_memsz + bssdiff);
			phdr.p_filesz += bssdiff;
			phdr.p_memsz += bssdiff;
		}
		if (gelf_update_phdr(delf, i, &phdr) == NULL) {
			elfdie(dstfile, "Can't write program header at %d", i);
		}
	}

	src.e_fd = sfd;
	src.e_filename = file;
	src.e_elf = self;
	dst.e_fd = dfd;
	dst.e_filename = dstfile;
	dst.e_elf = delf;

	/*
	 * Update sections.
	 * At first, setup original data in scinfo structure, and create
	 * scinfo list.
	 */
	for (i = 0, sipp = &list; i < ehdr.e_shnum; i++) {
		Elf_Data	*sdata;
		Elf_Scn		*scn;

		sip = *(stable + i);
		scn = sip->si_scn;
		if ((sdata = elf_getdata(scn, NULL)) == NULL) {
			elfdie(file, "Can't read section data at %d", i);
		}

		sip->si_data = sdata;
		if ((sip->si_shdr.sh_flags & SHF_ALLOC) &&
		    sip->si_shdr.sh_type != SHT_NOBITS) {
			sip->si_newdata = xmalloc(sdata->d_size);
			(void)memcpy(sip->si_newdata, sdata->d_buf,
				     sdata->d_size);
		}
		else {
			sip->si_newdata = NULL;
		}
		*sipp = sip;
		sipp = &(sip->si_next);
	}
	*sipp = NULL;

	/* Update shdr */
	for (i = 1, sipp = stable + 1; i < ehdr.e_shnum; i++, sipp++) {
		int		sidx = *(xlate + i);
		GElf_Off	off = *(offxlate + i);
		Elf_Scn		*dscn;
		Elf_Data	*sdata, *ddata;
		GElf_Shdr	*shdr;

		sip = *sipp;
		shdr = &(sip->si_shdr);
		verbose(1, "shdr: idx:%d => %d, off:0x%llx => 0x%llx, "
			"size=0x%llx", sidx, i, shdr->sh_offset, off,
			shdr->sh_size);
		shdr->sh_offset = off;

		if ((dscn = elf_newscn(delf)) == NULL) {
			elfdie(dstfile, "Can't create new section for %d", i);
		}
		sip->si_dscn = dscn;

		if ((ddata = elf_newdata(dscn)) == NULL) {
			elfdie(dstfile,
			       "Can't create new section data for %d", i);
		}

		if (shdr->sh_addr == NULL) {
			if (shdr->sh_flags & SHF_ALLOC) {
				shdr->sh_addr = baseaddr + off;
			}
		}
		else if (shdr->sh_type == SHT_NOBITS) {
			shdr->sh_addr += bssdiff;
		}
		if (shdr->sh_addr != NULL) {
			verbose(1, "shdr[%d]: vaddr = 0x%llx",
				i, shdr->sh_addr);
		}
		if (shdr->sh_link != 0) {
			shdr->sh_link = *(xlate + shdr->sh_link);
		}
		if (SHDR_ISRELOC(shdr) && shdr->sh_info != 0) {
			shdr->sh_info = *(xlate + shdr->sh_info);
		}

		/* Copy section data. */
		sdata = sip->si_data;
		if (shdr->sh_type == SHT_SYMTAB) {
			/* Update symbol entries. */
			symtab_update_sym(&src, &dst, sdata, ddata, shdr,
					  oldbss, bssdiff, xlate);
		}
		else if (is_rel_alloc(shdr, stable)) {
			/* We must relocate BSS symbols. */
			verbose(1, "shdr[%d]: reloc BSS: info=%d", i,
				shdr->sh_info);
			reloc_bss(&src, &dst, sdata, ddata, shdr, oldbss,
				  oldbssend, bssdiff, list);
		}
		else {
			(void)memcpy(ddata, sdata, sizeof(Elf_Data));
			if (sip->si_newdata) {
				ddata->d_buf = sip->si_newdata;
			}
		}
	}

	off = 0;
	for (i = 1, sipp = stable + 1; i < ehdr.e_shnum; i++, sipp++) {
		GElf_Shdr	*shdr;

		sip = *sipp;
		shdr = &(sip->si_shdr);
		if (gelf_update_shdr(sip->si_dscn, shdr) == NULL) {
			elfdie(dstfile, "Can't update section at %d", i);
		}
		off = MAX(off, shdr->sh_offset + shdr->sh_size);
	}

	dehdr.e_shstrndx = *(xlate + ehdr.e_shstrndx);

	/* Update the location of section headers. */
	align = gelf_fsize(delf, ELF_T_ADDR, 1, EV_CURRENT);
	if (align <= 1) {
		align = 4;
	}
	dehdr.e_shoff = roundup(off, align);
	
	if (gelf_update_ehdr(delf, &dehdr) == NULL) {
		elfdie(dstfile, "Can't update ELF header");
	}

	/* Commit changes. */
	if (elf_update(delf, ELF_C_WRITE) < 0) {
		elfdie(dstfile, "Can't update ELF file image");
	}

	xfree(stable);
	xfree(xlate);
	xfree(offxlate);
	(void)elf_end(self);
	(void)elf_end(delf);

	move_file(dstfile, file);
	CleanFile1 = NULL;
}

/*
 * static scinfo_t *
 * scinfo_get(Elf *elf, GElf_Ehdr *ehdr, int **xlatep, uint_t *nump,
 *	     int *rmsct, uint_t flags)
 *	Get section header list for new ELF file.
 *
 *	"rmsct" is an array of section index to be removed. The last element
 *	must be -1.
 *
 * Calling/Exit State:
 *	scinfo_get() returns scinfo list that represents section headers for
 *	new image. Section index translation table is set to *xlatep.
 *	Translation table takes original section index as array index,
 *	and it keeps new section index in its element. Removed section keeps
 *	-1 as element. Number of new sections is set to *nump.
 *
 *	Returned array is allocated by xmalloc(). The caller should free it
 *	using xfree().
 */
static scinfo_t *
scinfo_get(Elf *elf, GElf_Ehdr *ehdr, char *input, int **xlatep, uint_t *nump,
	   int *rmsct, uint_t flags)
{
	int		i, *xlate;
	uint_t		nsct = ehdr->e_shnum, newnsct;
	size_t		tblsz, xlsz;
	scinfo_t	**stable, *list, **sipp, **nextpp;

	/* Collect all file offsets. */
	tblsz = sizeof(*stable) * nsct;
	stable = (scinfo_t **)xmalloc(tblsz);
	(void)memset(stable, 0xff, tblsz);

	xlsz = sizeof(*xlate) * nsct;
	xlate = (int *)xmalloc(xlsz);
	(void)memset(xlate, 0xff, xlsz);
	for (i = 0, sipp = stable; i < nsct; i++, sipp++) {
		scinfo_t	*sip;
		int		*rmp = rmsct, strip = 0;
		GElf_Shdr	*shdr;

		sip = (scinfo_t *)xmalloc(sizeof(*sip));
		if ((sip->si_scn = elf_getscn(elf, i)) == NULL) {
			elfdie(input, "Can't get section descriptor at %d", i);
		}
		shdr = &(sip->si_shdr);
		if (gelf_getshdr(sip->si_scn, shdr) == NULL) {
			elfdie(input, "Can't read section header at %d", i);
		}
		sip->si_index = i;

		if (*(stable + shdr->sh_link) == NULL) {
			xfree(sip);
			*sipp = NULL;
			continue;
		}

		while (rmp && *rmp != -1) {
			if (*rmp == i) {
				strip = 1;
				break;
			}
			rmp++;
		}

		if (strip) {
			int		j;

			xfree(sip);

			for (j = 0; j < i; j++) {
				if ((sip = *(stable + j)) != NULL) {
					shdr = &(sip->si_shdr);
					if (shdr->sh_link == i) {
						xfree(sip);
						*(stable + j) = NULL;
					}
				}
			}
			sip = NULL;
		}
		*sipp = sip;
	}

	if (flags & SYMF_UNIXHACK) {
		/*
		 * The order of section headers in the ELF file generated
		 * by GNU ld is invalid. The section headers for reloc
		 * sections should be moved to the end of sections.
		 */
		for (i = 0, sipp = stable; i < nsct; i++, sipp++) {
			scinfo_t	*sip = *sipp;
			GElf_Shdr	*shdr;
			int		ctfidx, j, k;

			if (sip == NULL) {
				continue;
			}

			shdr = &(sip->si_shdr);
			if (shdr->sh_type != SHT_REL) {
				continue;
			}

			ctfidx = -1;

			/* Move reloc section just above CTF data. */
			for (j = nsct - 1; j > i; j--) {
				scinfo_t	*sp = *(stable + j);
				GElf_Shdr	*sh;
				char		*sname;

				if (sp == NULL) {
					/* Skip removed section. */
					continue;
				}
				sh = &(sp->si_shdr);
				sname = elf_strptr(elf, ehdr->e_shstrndx,
						   sh->sh_name);
				if (strcmp(sname, SHNAME_CTF) == 0) {
					ctfidx = j;
					break;
				}
			}
			if (ctfidx <= 0) {
				j = nsct - 1;
			}
			else {
				j = ctfidx - 1;
			}
			verbose(1, "*** %d => %d", i, j);
			for (k = i; k <= j; k++) {
				*(stable + k) = *(stable + k + 1);
			}
			*(stable + j) = sip;
		}
	}

	/* Eliminates sections to be removed. */
	nextpp = &list;
	for (i = 0, sipp = stable, newnsct = 0; i < nsct; i++, sipp++) {
		scinfo_t	*sip = *sipp;

		if (sip != NULL) {
			*nextpp = sip;
			nextpp = &(sip->si_next);
			*(xlate + sip->si_index) = newnsct;
			newnsct++;
		}
	}

	*nextpp = NULL;
	*xlatep = xlate;
	*nump = newnsct;
	xfree(stable);

	return list;
}

/*
 * static void
 * switch_symtab(char *srcfile, char *dstfile)
 *	Switch symbol table type. SHT_SYMTAB will be changed to SHT_DYNSYM,
 *	and vice versa.
 *
 *	GNU objdump can't treat ELF image that contains .symtab in loaded
 *	section. The purpose of this function is to avoid that restriction.
 *	So this function changes only section type, without changing
 *	section name.
 */
static void
switch_symtab(char *srcfile, char *dstfile)
{
	int		fd, i;
	char		*file;
	Elf		*elf;
	GElf_Ehdr	ehdr;
	char		*buf;
	size_t		typeoff, typesize;
	uint_t		encode;

	if (dstfile == NULL) {
		file = srcfile;
	}
	else {
		/* Copy source file. */
		copy_file(srcfile, dstfile);
		file = dstfile;
	}

	if ((fd = open(file, O_RDWR)) == -1) {
		fatal(errno, "Can't open \"%s\"", file);
	}

	if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
		elfdie(file, "Can't read file");
	}
	if (elf_kind(elf) != ELF_K_ELF) {
		fatal(0, "\"%s\" is a not ELF object", file);
	}
	if (gelf_getehdr(elf, &ehdr) == NULL) {
		elfdie(file, "Can't read ELF header");
	}

	buf = (char *)xmalloc(ehdr.e_shentsize);
	if (ehdr.e_shentsize == sizeof(Elf32_Shdr)) {
		typeoff = offsetof(Elf32_Shdr, sh_type);
		typesize = sizeof(Elf32_Word);
	}
	else {
		typeoff = offsetof(Elf64_Shdr, sh_type);
		typesize = sizeof(Elf64_Word);
	}
	encode = ehdr.e_ident[EI_DATA];

	for (i = 0; i < ehdr.e_shnum; i++) {
		Elf_Scn		*scn;
		GElf_Shdr	shdr;
		GElf_Word	type = SHT_NULL;
		Elf_Data	data;
		off_t		foff;

		if ((scn = elf_getscn(elf, i)) == NULL) {
			elfdie(file, "Can't get section descriptor at %d", i);
		}
		if (gelf_getshdr(scn, &shdr) == NULL) {
			elfdie(file, "Can't read section header at %d", i);
		}

		if (shdr.sh_type == SHT_SYMTAB) {
			type = SHT_DYNSYM;
			verbose(1, "shdr[%d]: SYMTAB -> DYNSYM", i);
		}
		else if (shdr.sh_type == SHT_DYNSYM) {
			type = SHT_SYMTAB;
			verbose(1, "shdr[%d]: DYNSYM -> SYMTAB", i);
		}
		else {
			continue;
		}

		data.d_buf = &type;
		data.d_type = ELF_T_WORD;
		data.d_align = typesize;
		data.d_off = 0;
		data.d_version = EV_CURRENT;
		if (gelf_xlatetof(elf, &data, &data, encode) == NULL) {
			elfdie(file, "Can't translate section type");
		}

		foff = ehdr.e_shoff + (ehdr.e_shentsize * i) + typeoff;
		if (pwrite(fd, &type, sizeof(type), foff) == -1) {
			fatal(errno, "Can't update section type for %d", i);
		}
	}

	xfree(buf);

	(void)elf_end(elf);
	(void)close(fd);
}
