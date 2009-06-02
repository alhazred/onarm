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
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2007-2008 NEC Corporation
 */

/*
 * Global include file for all sgs ARM machine dependent macros,
 * constants and declarations.
 */

#ifndef	_MACHDEP_H
#define	_MACHDEP_H

#include <link.h>
#include <sys/machelf.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Elf header information.
 */
#define	M_MACH			EM_ARM
#define	M_CLASS			ELFCLASS32

#define	M_MACHPLUS		M_MACH
#define	M_DATA			ELFDATA2LSB
#define	M_FLAGSPLUS		0

/*
 * Page boundary Macros: truncate to previous page boundary and round to
 * next page boundary (refer to generic macros in ../sgs.h also).
 */
#define	M_PTRUNC(X)	((X) & ~(syspagsz - 1))
#define	M_PROUND(X)	(((X) + syspagsz - 1) & ~(syspagsz - 1))

/*
 * Segment boundary macros: truncate to previous segment boundary and round
 * to next page boundary.
 */
#define	M_SEGSIZE	ELF_ARM_MAXPGSZ

#define	M_STRUNC(X)	((X) & ~(M_SEGSIZE - 1))
#define	M_SROUND(X)	(((X) + M_SEGSIZE - 1) & ~(M_SEGSIZE - 1))

/*
 * TLS static segments must be rounded to the following requirements,
 * due to libthread stack allocation.
 */
#define	M_TLSSTATALIGN	0x08


/*
 * Other machine dependent entities
 */
#define	M_SEGM_ALIGN	ELF_ARM_MAXPGSZ

#define	M_BIND_ADJ	4		/* adjustment for end of */
					/*	elf_rtbndr() address */

/*
 * Values for ARM objects
 */

/*
 * Instruction encodings.
 */
#define	M_INST_NOP		0xe1a00000
#define	M_INST_ADD_IP_PC	0xe28fc600	/* add ip, pc, #XXXXXXX  */
#define	M_INST_ADD_IP_IP	0xe28cca00	/* add ip, ip, #XXXXXXX  */
#define	M_INST_LDR_PC_IP	0xe5bcf000	/* ldr pc, [ip, #XXXXX]! */

#define	M_INST_PUSH_LR		0xe52de004	/* str lr, [sp, #-4]!  */
#define	M_INST_LDR_LR_PC_4	0xe59fe004	/* ldr lr, [pc, #4]    */
#define	M_INST_ADD_LR_PC	0xe08fe00e	/* add lr, pc, lr      */
#define	M_INST_LDR_PC_LR_PRE8	0xe5bef008	/* ldr   pc, [lr, #8]! */

#define	M_WORD_ALIGN	4

/* default first segment offset */
#define	M_SEGM_ORIGIN	(Addr)(0x00008000)

/*
 * Plt and Got information; the first few .got and .plt entries are reserved
 *	PLT[0]	jump to dynamic linker
 *	GOT[0]	address of _DYNAMIC
 */
#define	M_PLT_ENTSIZE	12		/* plt entry size in bytes */
#define	M_PLT_ALIGN	M_WORD_ALIGN	/* alignment of .plt section */
#define	M_PLT_INSSIZE	4		/* single plt instruction size */
#define	M_PLT_RESERVSZ	20		/* PLT[0] reserved */

#define	M_GOT_XDYNAMIC	0		/* got index for _DYNAMIC */
#define	M_GOT_XLINKMAP	1		/* got index for link map */
#define	M_GOT_XRTLD	2		/* got index for rtbinder */
#define	M_GOT_XNumber	3		/* reserved no. of got entries */

#define	M_GOT_ENTSIZE	4		/* got entry size in bytes */

/* Derive EABI version from e_flags */
#define	M_EF_ARM_EABI_VERSION(flags)	((flags) & EF_ARM_EABIMASK)

/* Floating point architecture flags */
#define	M_EF_FPFLAG_MASK						\
	(EF_ARM_APCS_FLOAT|EF_ARM_VFP_FLOAT|EF_ARM_MAVERICK_FLOAT)

/*
 * Make machine class dependent functions transparent to the common code
 */
#ifdef	_ELF64
#define	ELF_R_TYPE		ELF64_R_TYPE
#define	ELF_R_INFO		ELF64_R_INFO
#define	ELF_R_SYM		ELF64_R_SYM
#define	ELF_ST_BIND		ELF64_ST_BIND
#define	ELF_ST_TYPE		ELF64_ST_TYPE
#define	ELF_ST_INFO		ELF64_ST_INFO
#define	ELF_ST_VISIBILITY	ELF64_ST_VISIBILITY
#define	ELF_M_SYM		ELF64_M_SYM
#define	ELF_M_SIZE		ELF64_M_SIZE
#define	ELF_M_INFO		ELF64_M_INFO
#define	elf_checksum		elf64_checksum
#define	elf_fsize		elf64_fsize
#define	elf_getehdr		elf64_getehdr
#define	elf_getphdr		elf64_getphdr
#define	elf_newehdr		elf64_newehdr
#define	elf_newphdr		elf64_newphdr
#define	elf_getshdr		elf64_getshdr
#define	elf_xlatetof		elf64_xlatetof
#define	elf_xlatetom		elf64_xlatetom
#else /* _ELF64 */
#define	ELF_R_TYPE		ELF32_R_TYPE
#define	ELF_R_INFO		ELF32_R_INFO
#define	ELF_R_SYM		ELF32_R_SYM
#define	ELF_ST_BIND		ELF32_ST_BIND
#define	ELF_ST_TYPE		ELF32_ST_TYPE
#define	ELF_ST_INFO		ELF32_ST_INFO
#define	ELF_ST_VISIBILITY	ELF32_ST_VISIBILITY
#define	ELF_M_SYM		ELF32_M_SYM
#define	ELF_M_SIZE		ELF32_M_SIZE
#define	ELF_M_INFO		ELF32_M_INFO
#define	elf_checksum		elf32_checksum
#define	elf_fsize		elf32_fsize
#define	elf_getehdr		elf32_getehdr
#define	elf_getphdr		elf32_getphdr
#define	elf_newehdr		elf32_newehdr
#define	elf_newphdr		elf32_newphdr
#define	elf_getshdr		elf32_getshdr
#define	elf_xlatetof		elf32_xlatetof
#define	elf_xlatetom		elf32_xlatetom
#endif	/* _ELF64 */


/*
 * Make common relocation information transparent to the common code
 */
#define	M_REL_DT_TYPE	DT_REL		/* .dynamic entry */
#define	M_REL_DT_SIZE	DT_RELSZ	/* .dynamic entry */
#define	M_REL_DT_ENT	DT_RELENT	/* .dynamic entry */
#define	M_REL_DT_COUNT	DT_RELCOUNT	/* .dynamic entry */
#define	M_REL_SHT_TYPE	SHT_REL		/* section header type */
#define	M_REL_ELF_TYPE	ELF_T_REL	/* data buffer type */

/*
 * Make common relocation types transparent to the common code
 */
#define	M_R_NONE	R_ARM_NONE
#define	M_R_GLOB_DAT	R_ARM_GLOB_DAT
#define	M_R_COPY	R_ARM_COPY
#define	M_R_RELATIVE	R_ARM_RELATIVE
#define	M_R_JMP_SLOT	R_ARM_JUMP_SLOT
#define	M_R_FPTR	R_ARM_NONE
#define	M_R_ARRAYADDR	R_ARM_GLOB_DAT
#define	M_R_NUM		R_ARM_NUM

/*
 * The following are defined as M_R_NONE so that checks
 * for these relocations can be performed in common code - although
 * the checks are really only relevant to SPARC.
 */
#define	M_R_REGISTER	M_R_NONE

/*
 * DT_REGISTER is not valid on ARM
 */
#define	M_DT_REGISTER	0xffffffff
#define	M_DT_PLTRESERVE	0xfffffffe

/*
 * Make plt section information transparent to the common code.
 */
#define	M_PLT_SHF_FLAGS	(SHF_ALLOC | SHF_EXECINSTR)

/*
 * Make data segment information transparent to the common code.
 */
#define	M_DATASEG_PERM	(PF_R | PF_W | PF_X)

/*
 * On ARM architecture, pc points 2 instructions beyond the current
 * program counter.
 */
#define	M_PC_ACCESS_OFFSET	8

/*
 * Size of thread control block in static TLS.
 */
#define	M_TCB_SIZE		8

/*
 * Define a set of identifies for special sections.  These allow the sections
 * to be ordered within the output file image.  These values should be
 * maintained consistently, where appropriate, in each platform specific header
 * file.
 *
 *  o	null identifies that this section does not need to be added to the
 *	output image (ie. shared object sections or sections we're going to
 *	recreate (sym tables, string tables, relocations, etc.)).
 *
 *  o	any user defined section will be first in the associated segment.
 *
 *  o	interp and capabilities sections are next, as these are accessed
 *	immediately the first page of the image is mapped.
 *
 *  o	the syminfo, hash, dynsym, dynstr and rel's are grouped together as
 *	these will all be accessed first by ld.so.1 to perform relocations.
 *
 *  o	the got and dynamic are grouped together as these may also be
 *	accessed first by ld.so.1 to perform relocations, fill in DT_DEBUG
 *	(executables only), and .got[0].
 *
 *  o	unknown sections (stabs, comments, etc.) go at the end.
 *
 * Note that .tlsbss/.bss are given the largest identifiers.  This insures that
 * if any unknown sections become associated to the same segment as the .bss,
 * the .bss sections are always the last section in the segment.
 */
#define	M_ID_NULL	0x00
#define	M_ID_USER	0x01

#define	M_ID_INTERP	0x03			/* SHF_ALLOC */
#define	M_ID_CAP	0x04
#define	M_ID_UNWINDHDR	0x05
#define	M_ID_UNWIND	0x06
#define	M_ID_SYMINFO	0x07
#define	M_ID_HASH	0x08
#define	M_ID_LDYNSYM	0x09			/* always right before DYNSYM */
#define	M_ID_DYNSYM	0x0a
#define	M_ID_DYNSTR	0x0b
#define	M_ID_VERSION	0x0c
#define	M_ID_DYNSORT	0x0d
#define	M_ID_REL	0x0e
#define	M_ID_PLT	0x0f			/* SHF_ALLOC + SHF_EXECISNTR */
#define	M_ID_TEXT	0x10
#define	M_ID_DATA	0x20

/*	M_ID_USER	0x02			dual entry - listed above */
#define	M_ID_GOT	0x03			/* SHF_ALLOC + SHF_WRITE */
#define	M_ID_DYNAMIC	0x05
#define	M_ID_ARRAY	0x06

#define	M_ID_UNKNOWN	0xfb			/* just before TLS */

#define	M_ID_TLS	0xfc			/* just before bss */
#define	M_ID_TLSBSS	0xfd
#define	M_ID_BSS	0xfe
#define	M_ID_LBSS	0xff

#define	M_ID_SYMTAB_NDX	0x02			/* ! SHF_ALLOC */
#define	M_ID_SYMTAB	0x03
#define	M_ID_STRTAB	0x04
#define	M_ID_DYNSYM_NDX	0x05
#define	M_ID_NOTE	0x06

#ifdef	__cplusplus
}
#endif

#endif /* _MACHDEP_H */
