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
 * Copyright (c) 2006-2009 NEC Corporation
 * All rights reserved.
 */

#ident	"@(#)tools/elfdatamod/elfdatamod.c"

/*
 * elfdatamod:	Utility to modify data in text/data section.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/sysmacros.h>
#include <libelf.h>
#include <gelf.h>
#include <errno.h>
#include "unixhack.h"

#define	PROGNAME	"elfdatamod"

#define	DEFAULT_SIZE	4

/* Section header */
typedef struct elf_section {
	int		es_index;
	GElf_Shdr	es_shdr;
	Elf_Scn		*es_scn;
	char		*es_name;
} elf_section_t;

/* Argument type */
typedef enum arg_type {
	ARG_IMMEDIATE		= 1,	/* Immediate value or address */
	ARG_SECTION_START	= 2,	/* Start offset of section */
	ARG_SECTION_END		= 3,	/* End offset of section */
	ARG_SYMBOL_ADDR		= 4,	/* Symbol address */
	ARG_SYMBOL		= 5,	/* Value bound to symbol */
	ARG_SYMBOL_OFFSET	= 6,	/* File offset bound to symbol */
	ARG_IMM_OFFSET		= 7	/* File offset of the address */
} arg_type_t;

/* Arguments */
typedef struct arg_value {
	arg_type_t		av_type;	/* Argument type */
	int			av_deref;	/* Dereference count */
	union {
		uint64_t	integer;	/* Immediate value */
		char		*string;	/* String value */
	} av_v;
	uint64_t		av_rvalue;	/* Evaluated value */
} arg_value_t;

#define	av_integer	av_v.integer
#define	av_string	av_v.string

typedef struct arg_print {
	arg_value_t		ap_value;	/* Specified value */
	char			*ap_arg;	/* Original argument */
	int			ap_size;	/* Access type */
	struct arg_print	*ap_next;
} arg_print_t;

typedef struct arg_modify {
	arg_value_t		am_src;		/* Source value */
	arg_value_t		am_dst;		/* Destination value */
	char			*am_arg;	/* Original argument */
	int			am_size;	/* Access type */
	struct arg_modify	*am_next;
} arg_modify_t;

/* Section information to be stripped from static unix executable. */
typedef struct strip_section {
	int		ss_index;		/* Section index */
	GElf_Word	ss_type;		/* Section type */
	GElf_Off	ss_offset;		/* File offset */
	GElf_Word	ss_size;		/* Section size */
	struct strip_section	*ss_next;
} strip_section_t;

/* Symbols to keep section address. */
typedef struct patchsym {
	/* Symbol name to keep start and end address of the section. */
	const char	*ps_ssym;		/* Start address */
	const char	*ps_esym;		/* End address */

	GElf_Word	ps_type;		/* Target section type */
	const char	*ps_sname;		/* Target section name */
	GElf_Off	ps_start;		/* Start offset */
	GElf_Off	ps_end;			/* End offset */
} patchsym_t;

static patchsym_t	PatchSymbols[] = {
	/* .symtab */
	{"symtab_start_address", "symtab_end_address", SHT_SYMTAB,
	 ".symtab", 0, 0},

	/* .strtab */
	{"strtab_start_address", "strtab_end_address", SHT_STRTAB,
	 ".strtab", 0, 0},

	/* .hash */
	{"symhash_start_address", "symhash_end_address", SHT_UNIX_SYMHASH,
	 SHNAME_UNIX_SYMHASH, 0, 0},

	/* Sentinel */
	{NULL, NULL, SHT_NULL, 0, 0},
};

/* Symbol name that keeps number of local symbols. */
#define	SYMTAB_LOCALS_SYMBOL	"symtab_locals"

/* Internal prototypes */
static void		usage(int status);
static void		verbose(const char *fmt, ...);
static void		verbose_value(arg_value_t *avp);
static void		die(const char *fmt, ...);
static void		elfdie(char *filename, const char *fmt, ...);
static void		read_ehdr(Elf *elfp, GElf_Ehdr *ehdr);
static elf_section_t	*read_shdr(Elf *elfp, GElf_Ehdr *ehdr);
static void		parse_argument(char *arg, arg_value_t *avp);
static int		parse_size(char *arg, char **sepp);
static void		parse_print_argument(char *arg);
static void		parse_modify_argument(char *arg);
static uint64_t		dereference_immediate(arg_value_t *avp, uint64_t addr,
					      int asz);
static uint64_t		dereference(arg_value_t *avp, uint64_t addr);
static void		eval_value(Elf *elfp, arg_value_t *avp, int asz);
static elf_section_t	*find_section(char *name);
static uint64_t		search_symbol(Elf *elfp, char *name);
static uint64_t		get_file_offset(uint64_t addr);
static int		read_value(uint64_t addr, uint64_t *value, int asz);
static int		write_value(uint64_t addr, uint64_t value, int asz);
static void		write_value_to_symbol(Elf *elfp, char *name,
					      uint64_t value);
static strip_section_t	*tnf_resolve(Elf *elfp, GElf_Ehdr *ehdr,
				     uint64_t probe_nextoff);
static void		tnf_resolve_reloc(uint64_t addr, char *symname,
					  uint64_t *pist, uint64_t *tlist,
					  uint64_t probe_nextoff, int asz);
static void		add_strip_section(strip_section_t **headpp,
					  elf_section_t *sh);
static void		fixup_static_unix(Elf *elfp, GElf_Ehdr *ehdr,
					  char *outfile,
					  strip_section_t *remove);
static GElf_Off		fixup_offset(GElf_Off off, strip_section_t *remove,
				     GElf_Word align);
static void		fixup_patch_symbol(GElf_Shdr *shdr, char *sname);

static arg_type_t	parse_symbol_arg(char *arg, arg_value_t *avp);

static int		Verbose;

static char		*ElfFile;
static int		ByteSwap;
static uint8_t		ElfClass;
static int		IgnoreSectionError;
static int		Decimal;
static uint64_t		BaseOffset;

static int		ShdrCount;
static elf_section_t	*Shdr;

static arg_print_t	*ArgPrint;
static arg_print_t	**ArgPrintNext;

static arg_modify_t	*ArgModify;
static arg_modify_t	**ArgModifyNext;

static int		Fd;

static int		CtfStrip;

#define	BYTESWAP_16_RAW(x)	((((x) & 0xff) << 8) | (((x) >> 8) & 0xff))
#define	BYTESWAP_32_RAW(x)					\
	((((x) & 0xff) << 24) | (((x) & 0xff00) << 8) |		\
	 (((x) & 0xff0000) >> 8) | (((x) & 0xff000000) >> 24))
#define	BYTESWAP_64_RAW(x)					\
	(BYTESWAP_32_RAW(x >> 32) | BYTESWAP_32_RAW(x) << 32)

#define	BYTESWAP_16(x)	((ByteSwap) ? BYTESWAP_16_RAW(x) : (x))
#define	BYTESWAP_32(x)	((ByteSwap) ? BYTESWAP_32_RAW(x) : (x))
#define	BYTESWAP_64(x)	((ByteSwap) ? BYTESWAP_64_RAW(x) : (x))

#ifdef	_LITTLE_ENDIAN
#define	ELF_DATA_NEED_SWAP	ELFDATA2MSB
#else	/* !_LITTLE_ENDIAN */
#define	ELF_DATA_NEED_SWAP	ELFDATA2LSB
#endif	/* _LITTLE_ENDIAN */

int
main(int argc, char **argv)
{
	int		c, oflags = O_RDONLY, quiet = 0, unixhack = 0;
	Elf		*elfp;
	GElf_Ehdr	ehdr;
	arg_print_t	*app;
	arg_modify_t	*amp;
	uint64_t	nextoff = (uint64_t)-1;
	char		*p, *outfile;

	ArgPrintNext = &ArgPrint;
	ArgModifyNext = &ArgModify;

	while ((c = getopt(argc, argv, "CDB:SXvhqp:m:n:")) != EOF) {
		switch (c) {
		case 'C':
			CtfStrip++;
			break;

		case 'X':
			unixhack++;
			oflags = O_RDWR;
			break;

		case 'B':
			/*
			 * Set base physical address where the LM is loaded.
			 */
			errno = 0;
			BaseOffset = strtoull(optarg, &p, 0);
			if (errno != 0 || *p != '\0') {
				die("Invalid physical address base: %s",
				    optarg);
			}
			break;

		case 'n':
			/*
			 * offsetof(tnf_probe_control_t, next) value.
			 * This value is used only -X mode.
			 */
			errno = 0;
			nextoff = strtoull(optarg, &p, 0);
			if (errno != 0 || *p != '\0') {
				die("Invalid offset: %s", optarg);
			}
			break;

		case 'S':
			IgnoreSectionError++;
			break;

		case 'D':
			Decimal++;
			break;

		case 'v':
			Verbose++;
			break;

		case 'q':
			quiet++;
			break;

		case 'p':
			parse_print_argument(optarg);
			break;

		case 'm':
			parse_modify_argument(optarg);
			oflags = O_RDWR;
			break;

		case 'h':
			usage(0);
			/* NOTREACHED */

		default:
			usage(2);
			/* NOTREACHED */
		}
	}

	if (unixhack) {
		if (optind != argc - 2) {
			usage(2);
			/* NOTREACHED */
		}
		outfile = *(argv + optind + 1);
	}
	else {
		if (optind != argc - 1) {
			usage(2);
			/* NOTREACHED */
		}
	}

	if (!unixhack && ArgPrint == NULL && ArgModify == NULL) {
		die("-p or -m option must be specified.");
	}
	ElfFile = *(argv + optind);

	/* Open ELF file. */
	if ((Fd = open(ElfFile, oflags)) == -1) {
		die("Can't open file: %s", ElfFile);
	}
	(void)elf_version(EV_CURRENT);
	if ((elfp = elf_begin(Fd, ELF_C_READ, NULL)) == NULL) {
		elfdie(ElfFile, "Can't read file.");
	}
	if (elf_kind(elfp) != ELF_K_ELF) {
		die("Not ELF object.");
	}
	read_ehdr(elfp, &ehdr);

	/* Read section header. */
	ShdrCount = (int)ehdr.e_shnum;
	Shdr = read_shdr(elfp, &ehdr);

	if (unixhack) {
		strip_section_t	*remove;

		/*
		 * Special hack for static-linked unix kernel executable.
		 * This is required to build static-linked unix.
		 */
		if (nextoff == (uint64_t)-1) {
			die("Offset of next member in tnf_probe_control_t "
			    "is required via \"-n\".");
		}
		if (ElfClass == ELFCLASS32 && nextoff > UINT32_MAX) {
			die("probe next offset overflow: 0x%llx", nextoff);
		}
		remove = tnf_resolve(elfp, &ehdr, nextoff);
		fixup_static_unix(elfp, &ehdr, outfile, remove);
		return 0;
	}

	/* Process -p option. */
	for (app = ArgPrint; app != NULL; app = app->ap_next) {
		arg_value_t	*avp = &(app->ap_value);

		eval_value(elfp, avp, app->ap_size);
		if (Decimal) {
			(void)printf("%lld\n", avp->av_rvalue);
		}
		else {
			(void)printf("%-16s:  0x%llx\n", app->ap_arg,
				     avp->av_rvalue);
		}
	}

	/* Evaluate arguments before modify file. */
	for (amp = ArgModify; amp != NULL; amp = amp->am_next) {
		arg_value_t	*src = &(amp->am_src);
		arg_value_t	*dst = &(amp->am_dst);
		int		asz = amp->am_size;

		eval_value(elfp, src, asz);
		eval_value(elfp, dst, asz);
		if (asz != 8) {
			uint64_t	max;

			max = (asz == 1) ? UINT8_MAX
				: (asz == 2) ? UINT16_MAX : UINT32_MAX;
			if (src->av_rvalue > max) {
				die("Source value overflow: %s: 0x%llx",
				    amp->am_arg, src->av_rvalue);
			}
		}
		if (get_file_offset(dst->av_rvalue) == (uint64_t)-1) {
			die("Destination address \"0x%llx\" is not "
			    "located in ELF file.", dst->av_rvalue);
		}
	}

	/* Update ELF file. */
	for (amp = ArgModify; amp != NULL; amp = amp->am_next) {
		arg_value_t	*src = &(amp->am_src);
		arg_value_t	*dst = &(amp->am_dst);

		(void)write_value(dst->av_rvalue, src->av_rvalue,
				  amp->am_size);
		if (!quiet) {
			if (ElfClass == ELFCLASS32) {
				(void)printf("%-24s:  0x%08llx <= 0x%llx\n",
					     amp->am_arg, dst->av_rvalue,
					     src->av_rvalue);
			}
			else {
				(void)printf("%-24s:  0x%016llx <= 0x%llx\n",
					     amp->am_arg, dst->av_rvalue,
					     src->av_rvalue);
			}
		}
	}

	(void)elf_end(elfp);
	if (ArgModify != NULL) {
		(void)fsync(Fd);
	}
	(void)close(Fd);

	return 0;
}

static void
usage(int status)
{
	(void)fprintf(stderr, "Usage: " PROGNAME " [options] ELFfile\n");
	(void)fprintf(stderr, "       " PROGNAME "[-B offset] -X -n offset "
		      "vmunix outfile\n");
	(void)fprintf(stderr, "       " PROGNAME " -h\n");
	if (status != 0) {
		exit(status);
	}
	(void)fprintf(stderr, "\nOptions:\n"
		      "  -S\n"
		      "      Ignore undefined section name.\n\n"
		      "  -B offset\n"
		      "      Specify base offset of section offset.\n\n"
		      "  -D\n"
		      "      Print value in decimal.\n\n"
		      "  -X\n"
		      "      Special hack for static-linked unix kernel.\n\n"
		      "  -C\n"
		      "      Strip CTF data. (-X is required)\n\n"
		      "  -n offset\n"
		      "      Specify offset of tnf_probe_control->next.\n\n"
		      "  -v\n"
		      "      Print verbose messages to stderr.\n\n"
		      "  -q\n"
		      "      Do not print message for -m.\n\n"
		      "  -p SRC[:SIZE]\n"
		      "      Print value of the specified SRC.\n\n"
		      "  -m SRC:DST[:SIZE]\n"
		      "      Write value of SRC into DST.\n\n"
		      "  SIZE is access type. Default is 'w'.\n"
		      "     b:  Byte access\n"
		      "     h:  Halfword access\n"
		      "     w:  Word access\n"
		      "     d:  Doubleword access\n");
	exit(0);
}

static void
verbose(const char *fmt, ...)
{
	va_list	ap;

	if (!Verbose) {
		return;
	}

	(void)fprintf(stderr, " + ");
	va_start(ap, fmt);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
}

static void
verbose_value(arg_value_t *avp)
{
	uint64_t	value = avp->av_rvalue;
	const char	*type;

	if (!Verbose) {
		return;
	}

	type = (avp->av_type == ARG_SECTION_START) ? "SECTION_START"
		: (avp->av_type == ARG_SECTION_END) ? "SECTION_END"
		: (avp->av_type == ARG_IMMEDIATE) ? "IMMEDIATE"
		: (avp->av_type == ARG_SYMBOL_ADDR) ? "SYMBOL_ADDR"
		: (avp->av_type == ARG_SYMBOL_OFFSET) ? "SYMBOL_OFFSET"
		: (avp->av_type == ARG_IMM_OFFSET) ? "IMM_OFFSET"
		: "SYMBOL";
	verbose("arg: type = %s, deref = %d, value = %lld (0x%llx)",
		type, avp->av_deref, value, value);
}

static void
die(const char *fmt, ...)
{
	va_list	ap;
	int	err = errno;

	va_start(ap, fmt);
	(void)vfprintf(stderr, fmt, ap);
	if (errno != 0) {
		(void)fprintf(stderr, "\nerrno = %d (%s)\n", err,
			      strerror(err));
	}
	else {
		(void)fprintf(stderr, "\n");
	}
	va_end(ap);

	exit(1);
}

static void
elfdie(char *filename, const char *fmt, ...)
{
	va_list	ap;

	(void)fprintf(stderr, "%s: ", filename);
	va_start(ap, fmt);
	(void)vfprintf(stderr, fmt, ap);
	(void)fprintf(stderr, "\nlibelf error = %s\n", elf_errmsg(-1));
	va_end(ap);

	exit(1);
}

static void
read_ehdr(Elf *elfp, GElf_Ehdr *ehdr)
{
	if (gelf_getehdr(elfp, ehdr) == NULL) {
		elfdie(ElfFile, "Can't read ELF header.");
	}

	ElfClass = ehdr->e_ident[EI_CLASS];
	if (ehdr->e_ident[EI_DATA] == ELF_DATA_NEED_SWAP) {
		ByteSwap = 1;
	}
	verbose("EI_DATA = %u, ByteSwap = %d, class = %u",
		(uint32_t)ehdr->e_ident[EI_DATA], ByteSwap,
		(uint32_t)ElfClass);
}

static elf_section_t *
read_shdr(Elf *elfp, GElf_Ehdr *ehdr)
{
	int	i;
	elf_section_t	*section, *sh;

	section = (elf_section_t *)malloc(sizeof(*section) * ehdr->e_shnum);
	if (section == NULL) {
		die("malloc() for section header failed.");
	}
	for (sh = section, i = 0; i < ehdr->e_shnum; sh++, i++) {
		GElf_Shdr	*shdr = &(sh->es_shdr);

		sh->es_index = i;
		if ((sh->es_scn = elf_getscn(elfp, i)) == NULL) {
			elfdie(ElfFile, "Can't read section at %d", i);
		}
		if (gelf_getshdr(sh->es_scn, shdr) == NULL) {
			elfdie(ElfFile, "Can't read ELF section header.");
		}
		sh->es_name = elf_strptr(elfp, ehdr->e_shstrndx, shdr->sh_name);
		if (sh->es_name == NULL) {
			sh->es_name = "<null>";
		}
		verbose("Shdr[%02d]: name[%s] addr[0x%llx] off[0x%llx] "
			"size[0x%llx]", i, sh->es_name, shdr->sh_addr,
			shdr->sh_offset, shdr->sh_size);
	}

	return section;
}

static arg_type_t
parse_symbol_arg(char *arg, arg_value_t *avp)
{
	char		*p;
	uint64_t	v;
	arg_type_t	type;

	while (*arg == '*') {
		avp->av_deref++;
		arg++;
	}
	errno = 0;
	v = strtoull(arg, &p, 0);
	if (errno != 0 || *p != '\0') {
		/* This must be a symbol name. */
		if ((avp->av_string = strdup(arg)) == NULL) {
			die("strdup() failed.");
		}
		type = ARG_SYMBOL;
	}
	else {
		/* Immediate */
		avp->av_integer = v;
		type = ARG_IMMEDIATE;
	}

	return type;
}

static void
parse_argument(char *arg, arg_value_t *avp)
{
	avp->av_deref = 0;

	if (*arg == '&') {
		/* Symbol address */
		if (strlen(arg + 1) == 0) {
			die("Invalid address format: %s", arg);
		}
		if ((avp->av_string = strdup(arg + 1)) == NULL) {
			die("strdup() failed.");
		}
		avp->av_type = ARG_SYMBOL_ADDR;
	}
	else if (*arg == '%') {
		arg_type_t	type;

		/* File offset of the symbol */
		type = parse_symbol_arg(arg + 1, avp);
		if (type == ARG_SYMBOL) {
			avp->av_type = ARG_SYMBOL_OFFSET;
		}
		else {
			avp->av_type = ARG_IMM_OFFSET;
		}
	}
	else if (*arg == '^') {
		/* Start address of the specified section */
		if (strlen(arg + 1) == 0) {
			die("Invalid section address format: %s", arg);
		}
		if ((avp->av_string = strdup(arg + 1)) == NULL) {
			die("strdup() failed.");
		}
		avp->av_type = ARG_SECTION_START;
	}
	else if (*arg == '$') {
		/* End address of the specified section */
		if (strlen(arg + 1) == 0) {
			die("Invalid section address format: %s", arg);
		}
		if ((avp->av_string = strdup(arg + 1)) == NULL) {
			die("strdup() failed.");
		}
		avp->av_type = ARG_SECTION_END;
		return;
	}
	else {
		avp->av_type = parse_symbol_arg(arg, avp);
	}
}

static int
parse_size(char *arg, char **sepp)
{
	char	*szp;
	int	size;

	szp = strchr(arg, '/');
	if (szp) {
		char	c;

		*sepp = szp;
		szp++;
		if (strlen(szp) != 1) {
			die("Invalid size parameter: %s", arg);
		}
		c = *szp;
		if (c == 'b') {
			size = 1;
		}
		else if (c == 'h') {
			size = 2;
		}
		else if (c == 'w') {
			size = 4;
		}
		else if (c == 'd') {
			size = 8;
		}
		else {
			die("Invalid size parameter: %s", arg);
		}
	}
	else {
		*sepp = NULL;
		size = DEFAULT_SIZE;
	}

	return size;
}

static void
parse_print_argument(char *arg)
{
	char	*argp, *szp;
	arg_print_t	*app;

	if ((app = (arg_print_t *)malloc(sizeof(*app))) == NULL) {
		die("malloc() for argument failed.");
	}
	if ((argp = strdup(arg)) == NULL) {
		die("strdup() failed.");
	}

	app->ap_arg = arg;
	app->ap_size = parse_size(argp, &szp);
	if (szp != NULL) {
		*szp = '\0';
	}

	parse_argument(argp, &app->ap_value);
	free(argp);

	app->ap_next = NULL;
	*ArgPrintNext = app;
	ArgPrintNext = &(app->ap_next);
}

static void
parse_modify_argument(char *arg)
{
	char	*argp, *dstp, *szp;
	arg_modify_t	*amp;
	arg_type_t	dtype;

	if ((amp = (arg_modify_t *)malloc(sizeof(*amp))) == NULL) {
		die("malloc() for argument failed.");
	}
	if ((argp = strdup(arg)) == NULL) {
		die("strdup() failed.");
	}

	amp->am_arg = arg;
	dstp = strchr(argp, ':');
	if (dstp == NULL) {
		die("Invalid parameter: %s", arg);
	}
	*dstp = '\0';
	dstp++;

	amp->am_size = parse_size(dstp, &szp);
	if (szp) {
		*szp = '\0';
	}

	parse_argument(argp, &amp->am_src);
	parse_argument(dstp, &amp->am_dst);

	dtype = amp->am_dst.av_type;
	if (dtype != ARG_IMMEDIATE && dtype != ARG_SYMBOL_ADDR) {
		die("Destination address must be immediate or symbol "
		    "address: %s", dstp);
	}
	if (amp->am_dst.av_deref) {
		die("Can't dereference destination: %s", dstp);
	}

	free(argp);

	amp->am_next = NULL;
	*ArgModifyNext = amp;
	ArgModifyNext = &(amp->am_next);
}

static uint64_t
dereference_immediate(arg_value_t *avp, uint64_t addr, int asz)
{
	int		sz, i;
	uint64_t	v;

	if (avp->av_deref <= 0) {
		return addr;
	}

	sz = (ElfClass == ELFCLASS32) ? sizeof(uint32_t) : sizeof(uint64_t);
	for (i = 0; i < avp->av_deref - 1; i++) {

		/* Treat value as address and dereference it. */
		if (!read_value(addr, &v, sz)) {
			die("Dereference failed at 0x%llx: 0x%llx",
			    addr, avp->av_integer);
		}
		addr = v;
	}

	if (!read_value(addr, &v, asz)) {
		die("Dereference failed at 0x%llx: 0x%llx", addr,
		    avp->av_integer);
	}

	return v;
}

static uint64_t
dereference(arg_value_t *avp, uint64_t addr)
{
	int	i;
	int	sz = (ElfClass == ELFCLASS32) ? sizeof(uint32_t)
		: sizeof(uint64_t);

	for (i = 0; i < avp->av_deref; i++) {
		uint64_t	v;

		/* Treat value as address and dereference. */
		if (!read_value(addr, &v, sz)) {
			die("Dereference failed at 0x%llx: %s", addr,
			    avp->av_string);
		}
		addr = v;
	}

	return addr;
}

static void
eval_value(Elf *elfp, arg_value_t *avp, int asz)
{
	uint64_t	value;
	arg_type_t	type = avp->av_type;

	if (type == ARG_IMMEDIATE) {
		value = dereference_immediate(avp, avp->av_integer, asz);
	}
	else if (type == ARG_SECTION_START || type == ARG_SECTION_END) {
		elf_section_t	*sh;

		/* Find specified section */
		if ((sh = find_section(avp->av_string)) != NULL) {
			GElf_Shdr	*shdr;

			shdr = &(sh->es_shdr);
			if (shdr->sh_offset == 0 && shdr->sh_size) {
				die("Section \"%s\" has no content.",
				    avp->av_string);
			}

			value = (uint64_t)shdr->sh_offset;
			if (type == ARG_SECTION_END) {
				value += (uint64_t)shdr->sh_size;
			}
			value += BaseOffset;
		}
		else if (IgnoreSectionError) {
			value = 0;
		}
		else {
			die("No section named \"%s\".", avp->av_string);
		}
	}
	else if (type == ARG_IMM_OFFSET) {
		value = dereference_immediate(avp, avp->av_integer, asz);
		if ((value = get_file_offset(value)) == (uint64_t)-1) {
			die("Address \"0x%llx\" is not located in ELF file.",
			    avp->av_integer);
		}
	}
	else {
		value = search_symbol(elfp, avp->av_string);
		if (value == (uint64_t)-1) {
			die("Symbol not found: \"%s\"", avp->av_string);
		}
		if (type == ARG_SYMBOL) {
			uint64_t	v;

			value = dereference(avp, value);
			if (!read_value(value, &v, asz)) {
				die("Value of symbol \"%s\" is not located "
				    "in ELF file.", avp->av_string);
			}
			value = v;
		}
		else if (type == ARG_SYMBOL_OFFSET) {
			value = dereference(avp, value);
			if ((value = get_file_offset(value)) == (uint64_t)-1) {
				die("Value of symbol \"%s\" is not located "
				    "in ELF file.", avp->av_string);
			}
		}
	}

	avp->av_rvalue = value;
	verbose_value(avp);
}

static elf_section_t *
find_section(char *name)
{
	int	i;
	elf_section_t	*sh;

	for (i = 0, sh = Shdr; i < ShdrCount; sh++, i++) {
		if (strcmp(sh->es_name, name) == 0) {
			return sh;
		}
	}

	return NULL;
}

static uint64_t
search_symbol(Elf *elfp, char *name)
{
	Elf_Data	*data;
	GElf_Shdr	*shdr;
	elf_section_t	*sh, *symtab = NULL;
	uint64_t	value = (uint64_t)-1;
	int		i;

	/* Search symbol table to retrieve symbol. */
	for (i = 0, sh = Shdr; i < ShdrCount; sh++, i++) {
		if (sh->es_shdr.sh_type == SHT_SYMTAB) {
			symtab = sh;
			break;
		}
	}
	if (symtab == NULL) {
		die("No symbol table.");
	}

	data = elf_getdata(symtab->es_scn, NULL);
	if (data == NULL) {
		elfdie(ElfFile, "Failed to get symbol table data.");
	}

	shdr = &(symtab->es_shdr);
	for (i = 0; i < shdr->sh_size / shdr->sh_entsize; i++) {
		GElf_Sym	sym;
		char		*symname;
		int		bind;

		if (gelf_getsym(data, i, &sym) == NULL) {
			elfdie(ElfFile,
			       "Failed to search symbol table at index %d", i);
		}
		symname = elf_strptr(elfp, shdr->sh_link, sym.st_name);
		if (strcmp(symname, name) != 0) {
			continue;
		}
		if (sym.st_shndx == SHN_UNDEF) {
			die("\"%s\" is undefined symbol.", name);
		}
		if (sym.st_shndx != SHN_ABS && sym.st_shndx >= ShdrCount) {
			die("\"%s\" is not bound to valid section: 0x%x",
			    name, sym.st_shndx);
		}

		bind = ELF64_ST_BIND(sym.st_info);
		if (bind != STB_GLOBAL) {
			die("\"%s\" is not global symbol: %d", name, bind);
		}
		value = (uint64_t)sym.st_value;
		break;
	}

	return value;
}

static uint64_t
get_file_offset(uint64_t addr)
{
	int	i;
	elf_section_t	*sh;
	GElf_Shdr	*shdr = NULL;
	uint64_t	off;

	for (i = 0, sh = Shdr; i < ShdrCount; sh++, i++) {
		GElf_Shdr	*sp = &(sh->es_shdr);

		/* Skip non-allocated section and bss. */
		if (!(sp->sh_flags & SHF_ALLOC) || sp->sh_size == 0 ||
		    sp->sh_type == SHT_NOBITS) {
			continue;
		}
		if (addr >= (uint64_t)sp->sh_addr &&
		    addr < (uint64_t)(sp->sh_addr + sp->sh_size)) {
			shdr = sp;
			break;
		}
	}
	if (shdr == NULL) {
		return (uint64_t)-1;
	}

	off = addr - (uint64_t)shdr->sh_addr + (uint64_t)shdr->sh_offset;
	return off;
}

static int
read_value(uint64_t addr, uint64_t *value, int asz)
{
	uint64_t	foff;
	off_t		off;
	size_t		size;
	ssize_t		sz;
	uint8_t		v8;
	uint16_t	v16;
	uint32_t	v32;
	uint64_t	v64;
	void		*vp;

	if ((foff = get_file_offset(addr)) == (uint64_t)-1) {
		return 0;
	}
	if (foff > INT_MAX) {
		die("Too large file offset: 0x%llx", foff);
	}
	off = (off_t)foff;
	if (lseek(Fd, off, SEEK_SET) == -1) {
		die("lseek(0x%x) failed.", off);
	}

	if (asz == 1) {
		vp = &v8;
	}
	else if (asz == 2) {
		vp = &v16;
	}
	else if (asz == 4) {
		vp = &v32;
	}
	else {
		vp = &v64;
	}
	size = asz;

	if ((sz = read(Fd, vp, size)) == (ssize_t)-1) {
		die("read(off=0x%x) failed.", off);
	}
	if (sz != size) {
		die("read(off=0x%x, size=%d) returned %d.", off, size, sz);
	}

	if (asz == 1) {
		*value = (uint64_t)v8;
	}
	else if (asz == 2) {
		*value = (uint64_t)BYTESWAP_16(v16);
	}
	else if (asz == 4) {
		*value = (uint64_t)BYTESWAP_32(v32);
	}
	else {
		*value = (uint64_t)BYTESWAP_64(v64);
	}

	return 1;
}

static int
write_value(uint64_t addr, uint64_t value, int asz)
{
	uint64_t	foff;
	off_t		off;
	size_t		size;
	ssize_t		sz;
	uint8_t		v8;
	uint16_t	v16;
	uint32_t	v32;
	uint64_t	v64;
	void		*vp;

	if ((foff = get_file_offset(addr)) == (uint64_t)-1) {
		return 0;
	}
	if (foff > INT_MAX) {
		die("Too large file offset: 0x%llx", foff);
	}
	off = (off_t)foff;
	if (lseek(Fd, off, SEEK_SET) == -1) {
		die("lseek(0x%x) failed.", off);
	}

	if (asz == 1) {
		v8 = (uint8_t)value;
		vp = &v8;
	}
	else if (asz == 2) {
		v16 = (uint16_t)BYTESWAP_16(value & 0xffff);
		vp = &v16;
	}
	else if (asz == 4) {
		v32 = (uint32_t)BYTESWAP_32(value & 0xffffffff);
		vp = &v32;
	}
	else {
		v64 = BYTESWAP_64(value);
		vp = &v64;
	}
	size = asz;

	if ((sz = write(Fd, vp, size)) == (ssize_t)-1) {
		die("write(off=0x%x) failed.", off);
	}
	if (sz != size) {
		die("write(off=0x%x, size=%d) returned %d.", off, size, sz);
	}

	return 1;
}

static void
write_value_to_symbol(Elf *elfp, char *name, uint64_t value)
{
	int		asz = (ElfClass == ELFCLASS32) ? 4 : 8;
	uint64_t	addr;

	addr = search_symbol(elfp, name);
	if (addr == (uint64_t)-1) {
		die("Symbol not found: \"%s\"", name);
	}
	verbose("%s(0x%08llx): 0x%08llx", name, addr, value);
	(void)write_value(addr, value, asz);
}

#define	PROBE_MARKER_SYMBOL	"__tnf_probe_version_1"
#define	TAG_MARKER_SYMBOL	"__tnf_tag_version_1"

#define	PROBE_LIST_HEAD_SYMBOL	"static_tnf_probelist"
#define	TAG_LIST_HEAD_SYMBOL	"static_tnf_taglist"

/* ARGSUSED1 */
static strip_section_t *
tnf_resolve(Elf *elfp, GElf_Ehdr *ehdr, uint64_t probe_nextoff)
{
	elf_section_t	*sh, *symtab = NULL, *bss = NULL, *hash = NULL;
	Elf_Data	*symdata;
	GElf_Shdr	*symshdr;
	uint64_t	probelist = 0, taglist = 0;
	int	asz = (ElfClass == ELFCLASS32) ? 4 : 8;
	int	i;
	strip_section_t	*remove = NULL;
	GElf_Off	minreloff = (GElf_Off)-1, maxallocoff = 0;

	/* Search symbol table to retrieve symbol. */
	for (i = 0, sh = Shdr; i < ShdrCount; sh++, i++) {
		if (sh->es_shdr.sh_type == SHT_SYMTAB) {
			symtab = sh;
			break;
		}
	}
	if (symtab == NULL) {
		die("No symbol table.");
	}

	symdata = elf_getdata(symtab->es_scn, NULL);
	if (symdata == NULL) {
		elfdie(ElfFile, "Failed to get symbol table data.");
	}
	symshdr = &(symtab->es_shdr);

	/*
	 * Walk .rel.* sections.
	 * Currently, we don't check .rela.* sections.
	 */
	for (i = 0, sh = Shdr; i < ShdrCount; sh++, i++) {
		int		j;
		Elf_Data	*reldata;
		GElf_Shdr	*shdr;

		if (CtfStrip && strcmp(sh->es_name, SHNAME_CTF) == 0) {
			/* Remove CTF data section. */
			add_strip_section(&remove, sh);
			continue;
		}

		shdr = &(sh->es_shdr);
		if (shdr->sh_flags & SHF_ALLOC) {
			if (maxallocoff < shdr->sh_offset) {
				maxallocoff = shdr->sh_offset;
			}

			if (shdr->sh_type == SHT_NOBITS) {
				if (bss != NULL) {
					die("Multiple bss.");
				}
				bss = sh;

				/* Check address alignment. */
				if (!IS_P2ALIGNED(shdr->sh_addr,
						  UNIX_BSS_ALIGN)) {
					die("Bad .bss alignment: addr=0x%llx",
					    shdr->sh_addr);
				}
				if (!IS_P2ALIGNED(shdr->sh_size,
						  UNIX_BSS_ALIGN)) {
					die("Bad .bss alignment: size=0x%llx",
					    shdr->sh_size);
				}
			}
			else if (SECTION_IS_UNIX_SYMHASH(shdr, sh->es_name)) {
				if (hash != NULL) {
					die("Multiple symbol hash.");
				}
				hash = sh;
			}
			continue;
		}
		if (shdr->sh_type != SHT_REL) {
			continue;
		}

		if (minreloff > shdr->sh_offset) {
			minreloff = shdr->sh_offset;
		}
		add_strip_section(&remove, sh);

		verbose("REL[%d]: %s", i, sh->es_name);
		reldata = elf_getdata(sh->es_scn, NULL);
		if (reldata == NULL) {
			elfdie(ElfFile, "Failed to get relocation data.");
		}

		for (j = 0; j < shdr->sh_size / shdr->sh_entsize; j++) {
			GElf_Rel	rel;
			GElf_Sym	sym;
			int		index;
			char		*symname;

			if (gelf_getrel(reldata, j, &rel) == NULL) {
				elfdie(ElfFile,
				       "Failed to get relocation entry at "
				       "index %d", j);
			}
			if (ELF64_R_TYPE(rel.r_info) != STB_WEAK) {
				continue;
			}
			index = ELF64_R_SYM(rel.r_info);

			if (gelf_getsym(symdata, index, &sym) == NULL) {
				elfdie(ElfFile,
				       "Failed to search symbol at index %d",
				       index);
			}
			symname = elf_strptr(elfp, symshdr->sh_link,
					     sym.st_name);
			tnf_resolve_reloc(rel.r_offset, symname, &probelist,
					  &taglist, probe_nextoff, asz);
		}
	}

	if (minreloff < maxallocoff) {
		die("REL section located before ALLOC section.");
	}
	if (bss == NULL) {
		die("No bss.");
	}
	if (hash == NULL) {
		die("No symbol hash table.");
	}

	/* Install the head of probed lists. */
	write_value_to_symbol(elfp, PROBE_LIST_HEAD_SYMBOL, probelist);
	write_value_to_symbol(elfp, TAG_LIST_HEAD_SYMBOL, taglist);

	return remove;
}

static void
tnf_resolve_reloc(uint64_t addr, char *symname, uint64_t *plist,
		  uint64_t *tlist, uint64_t probe_nextoff, int asz)
{
	uint64_t	dst, value;

	if (strcmp(symname, PROBE_MARKER_SYMBOL) == 0) {
		uint64_t	next = addr + probe_nextoff;
		verbose("WRITE: PROBE: 0x%08llx => 0x%08llx (0x%08llx)",
			*plist, next, addr);
		value = *plist;
		dst = next;
		*plist = addr;
	}
	else if (strcmp(symname, TAG_MARKER_SYMBOL) == 0) {
		verbose("WRITE: TAG: 0x%08llx => 0x%08llx", *tlist, addr);
		value = *tlist;
		*tlist = addr;
		dst = addr;
	}

	(void)write_value(dst, value, asz);
}

#define	SECTION_CONTAINS(sp, off)					\
	((off) >= (sp)->ss_offset && (off) < (sp)->ss_offset + (sp)->ss_size)

static void
add_strip_section(strip_section_t **headpp, elf_section_t *sh)
{
	strip_section_t	*nsp, **spp;
	GElf_Shdr	*shdr;

	if ((nsp = (strip_section_t *)malloc(sizeof(*nsp))) == NULL) {
		die("malloc() for strip_section failed.");
	}

	shdr = &(sh->es_shdr);
	nsp->ss_index = sh->es_index;
	nsp->ss_type = shdr->sh_type;
	nsp->ss_offset = shdr->sh_offset;
	nsp->ss_size = shdr->sh_size;

	/* Put new section in file offset order. */
	for (spp = headpp; *spp != NULL; spp = &((*spp)->ss_next)) {
		strip_section_t	*sp = *spp;
		GElf_Off	soff, eoff;

		soff = nsp->ss_offset;
		eoff = soff + nsp->ss_size;

		if (SECTION_CONTAINS(sp, soff) || SECTION_CONTAINS(sp, eoff)) {
			die("Section %d overlaps with section %d",
			    nsp->ss_index, sp->ss_index);
		}
		if (sp->ss_offset > nsp->ss_offset) {
			break;
		}
	}
	nsp->ss_next = *spp;
	*spp = nsp;
}

static void
fixup_static_unix(Elf *elfp, GElf_Ehdr *ehdr, char *outfile,
		  strip_section_t *remove)
{
	strip_section_t	*sp;
	elf_section_t	*sh;
	int		ofd, *xlate, *xlp, i, nsdx, shstrndx;
	Elf		*delf;
	GElf_Ehdr	dehdr;
	struct stat	st;
	GElf_Off	newoff;
	size_t		align, locals = 0;
	patchsym_t	*psp;

	if (stat(ElfFile, &st) == -1) {
		die("stat(%s) failed", ElfFile);
	}
	if ((ofd = open(outfile, O_CREAT|O_TRUNC|O_RDWR|O_EXCL,
			st.st_mode & S_IAMB)) == -1) {
		die("open(%s) failed", outfile);
	}

	/* Construct transration table for section index. */
	if ((xlate = (int *)malloc(sizeof(int) * ShdrCount)) == NULL) {
		die("malloc() failed for section index xlate table.");
	}

	nsdx = 0;
	for (i = 0, sh = Shdr, xlp = xlate; i < ShdrCount; i++, sh++, xlp++) {
		for (sp = remove; sp != NULL && sp->ss_index != i;
		     sp = sp->ss_next);
		if (sp == NULL) {
			/* This section should NOT be removed. */
			*xlp = nsdx;
			nsdx++;
		}
		else {
			/* This section should be removed. */
			*xlp = -1;
		}
	}

	if (Verbose) {
		for (sp = remove; sp != NULL; sp = sp->ss_next) {
			verbose("REMOVE[%d]: type[%d] offset[0x%08llx] "
				"size[0x%08lx]", sp->ss_index,
				sp->ss_type, sp->ss_offset, sp->ss_size);
		}
	}

	if ((delf = elf_begin(ofd, ELF_C_WRITE, NULL)) == NULL) {
		elfdie(outfile, "Can't read file");
	}

	/* Create new ELF header. */
	if (gelf_newehdr(delf, gelf_getclass(elfp)) == NULL) {
		elfdie(outfile, "Can't create ELF header");
	}
	(void)memcpy(&dehdr, ehdr, sizeof(dehdr));

	/* Update program header. */
	(void)elf_flagelf(delf, ELF_C_SET, ELF_F_LAYOUT);
	if (gelf_newphdr(delf, ehdr->e_phnum) == NULL) {
		elfdie(outfile, "Can't create program headers");
	}

	for (i = 0; i < ehdr->e_phnum; i++) {
		GElf_Phdr	phdr;
		GElf_Off	poff;

		if (gelf_getphdr(elfp, i, &phdr) == NULL) {
			elfdie(ElfFile, "Can't read program header at %d", i);
		}

		/*
		 * We know that SHF_ALLOC'ed section is never removed.
		 * This is just sanity check.
		 */
		poff = fixup_offset(phdr.p_offset, remove, 0);
		if (poff != phdr.p_offset) {
			die("p_offset is changed: 0x%llx -> 0x%llx",
			    phdr.p_offset, poff);
		}

		if (gelf_update_phdr(delf, i, &phdr) == NULL) {
			elfdie(outfile, "Can't write program header at %d", i);
		}
	}

	/*
	 * Dump all sections but removed.
	 * NULL section will be automatically added by libelf.
	 */
	for (i = 1, sh = Shdr + 1, xlp = xlate + 1; i < ShdrCount;
	     i++, sh++, xlp++) {
		int		n;
		Elf_Scn		*sscn, *dscn;
		Elf_Data	*sdata, *ddata;
		GElf_Shdr	shdr;

		if ((n = *xlp) == -1) {
			/* Skip section to be removed. */
			continue;
		}

		sscn = sh->es_scn;
		(void)memcpy(&shdr, &(sh->es_shdr), sizeof(shdr));

		/*
		 * Copy section data.
		 * Note that we can't use elf_getdata() because it can't read
		 * .hash section data for static-linked kernel because
		 * of its entry size.
		 */
		if ((sdata = elf_rawdata(sscn, NULL)) == NULL) {
			elfdie(ElfFile, "Can't read data for section %d", i);
		}
		sdata->d_type = ELF_T_BYTE;
		sdata->d_version = EV_CURRENT;

		if ((dscn = elf_newscn(delf)) == NULL) {
			elfdie(outfile, "Can't create new section for %d", i);
		}
		if ((ddata = elf_newdata(dscn)) == NULL) {
			elfdie(outfile,
			       "Can't create new section data for %d", i);
		}
		(void)memcpy(ddata, sdata, sizeof(Elf_Data));

		/* Update index for section link. */
		if (shdr.sh_link != 0) {
			int	ldx = *(xlate + shdr.sh_link);

			if (ldx == -1) {
				die("Linked section can't be removed: %d",
				    shdr.sh_link);
			}
			shdr.sh_link = ldx;
		}

		/* Update section offset. */
		newoff = fixup_offset(shdr.sh_offset, remove,
				      shdr.sh_addralign);
		verbose("New section: index: %d->%d, off: 0x%llx -> 0x%llx",
			i, n, shdr.sh_offset, newoff);

		shdr.sh_offset = newoff;
		fixup_patch_symbol(&shdr, sh->es_name);

		if (shdr.sh_type == SHT_SYMTAB) {
			locals = shdr.sh_info;
		}

		if (gelf_update_shdr(dscn, &shdr) == NULL) {
			elfdie(outfile, "Can't update section at %d", nsdx);
		}
	}

	/* Update the location of section and program headers. */
	align = gelf_fsize(delf, ELF_T_ADDR, 1, EV_CURRENT);
	newoff = fixup_offset(ehdr->e_shoff, remove, align);
	verbose("New section header offset: 0x%llx -> 0x%llx",
		ehdr->e_shoff, newoff);
	dehdr.e_shoff = newoff;

	newoff = fixup_offset(ehdr->e_phoff, remove, align);
	verbose("New program header offset: 0x%llx -> 0x%llx",
		ehdr->e_phoff, newoff);
	dehdr.e_phoff = newoff;

	verbose("New number of section headers: %d -> %d",
		ehdr->e_shnum, nsdx);
	dehdr.e_shnum = nsdx;

	/* Update index for .shstrtab. */
	shstrndx = *(xlate + ehdr->e_shstrndx);
	if (shstrndx == -1) {
		die(".shstrtab must not be removed.");
	}
	verbose("New section index for .shstrtab: %d -> %d",
		ehdr->e_shstrndx, shstrndx);
	dehdr.e_shstrndx = shstrndx;

	if (gelf_update_ehdr(delf, &dehdr) == NULL) {
		elfdie(outfile, "Can't update ELF header");
	}

	/* Commit changes. */
	if (elf_update(delf, ELF_C_WRITE) < 0) {
		elfdie(outfile, "Can't update ELF file image");
	}

	free(xlate);
	(void)elf_end(elfp);
	(void)elf_end(delf);
	(void)fsync(ofd);
	(void)close(Fd);

	/* Do binary patch to keep section address in kernel symbol. */
	if (lseek(ofd, 0, SEEK_SET) == -1) {
		die("lseek(0) failed.", 0);
	}
	Fd = ofd;
	ElfFile = outfile;
	if ((elfp = elf_begin(Fd, ELF_C_READ, NULL)) == NULL) {
		elfdie(ElfFile, "Can't read file.");
	}
	read_ehdr(elfp, ehdr);
	ShdrCount = (int)ehdr->e_shnum;
	free(Shdr);
	Shdr = read_shdr(elfp, ehdr);

	for (psp = PatchSymbols; psp->ps_ssym != NULL; psp++) {
		GElf_Off	start, end;

		if ((start = psp->ps_start) == 0 ||
		    (end = psp->ps_end) == 0) {
			die("Can't detect offset for section type: %d",
			    psp->ps_type);
		}

		start += BaseOffset;
		end += BaseOffset;
		write_value_to_symbol(elfp, (char *)psp->ps_ssym, start);
		write_value_to_symbol(elfp, (char *)psp->ps_esym, end);
	}

	write_value_to_symbol(elfp, SYMTAB_LOCALS_SYMBOL, (uint64_t)locals);

	(void)elf_end(elfp);
	(void)fsync(ofd);
	(void)close(ofd);
}

static GElf_Off
fixup_offset(GElf_Off off, strip_section_t *remove, GElf_Word align)
{
	strip_section_t	*sp;
	GElf_Off	newoff = off;

	for (sp = remove; sp != NULL; sp = sp->ss_next) {
		if (off > sp->ss_offset) {
			newoff -= sp->ss_size;
		}
	}

	if (align > 1) {
		newoff = roundup(newoff, align);
	}

	return newoff;
}

/*
 * static void
 * fixup_patch_symbol(GElf_Shdr *shdr, char *sname)
 *	Fix up value for symbol to be patched.
 *	shdr must contains values for new kernel image.
 */
static void
fixup_patch_symbol(GElf_Shdr *shdr, char *sname)
{
	patchsym_t	*psp;

	for (psp = PatchSymbols; psp->ps_ssym != NULL; psp++) {
		if (psp->ps_sname != NULL &&
		    strcmp(psp->ps_sname, sname) != 0) {
			continue;
		}
		if (shdr->sh_type == psp->ps_type) {
			/* Found. */
			psp->ps_start = shdr->sh_offset;
			psp->ps_end = shdr->sh_offset + shdr->sh_size;
			break;
		}
	}
}
