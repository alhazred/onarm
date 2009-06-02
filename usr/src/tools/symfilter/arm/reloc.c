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

#ident	"@(#)tools/symfilter/arm/reloc.c"

/*
 * ARM specific ELF relocation.
 */

#include "symfilter.h"
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/sysmacros.h>

#define	R_ARM_PC24	1
#define	R_ARM_ABS32	2
#define	R_ARM_CALL	28
#define	R_ARM_JUMP24	29

/*
 * void
 * reloc_bss(elfimg_t *src, elfimg_t *dst, Elf_Data *sdata, Elf_Data *ddata,
 *	     GElf_Shdr *shdr, GElf_Addr diffbase, GElf_Addr endaddr,
 *	     GElf_Off diff, scinfo_t *scinfo)
 *	Update reloc section for static kernel hack.
 *
 *	This function walks relocation entries, and add "diff" to the target
 *	address if it is larger than or equals diffbase.
 */
void
reloc_bss(elfimg_t *src, elfimg_t *dst, Elf_Data *sdata, Elf_Data *ddata,
	  GElf_Shdr *shdr, GElf_Addr diffbase, GElf_Addr endaddr,
	  GElf_Off diff, scinfo_t *scinfo)
{
	int	i, num;

	sdata->d_type = ELF_T_REL;
	(void)memcpy(ddata, sdata, sizeof(Elf_Data));
	ddata->d_buf = xmalloc(sdata->d_size);
	(void)memcpy(ddata->d_buf, sdata->d_buf, sdata->d_size);
	num = sdata->d_size / shdr->sh_entsize;

	/* Walk reloc entries. */
	for (i = 0; i < num; i++) {
		GElf_Rel	rel;
		GElf_Word	type;
		Elf_Data	*data;
		uint32_t	*ptr, value, ovalue;
		off_t		foff;
		int		update = 0;
		scinfo_t	*sip;

		if (gelf_getrel(sdata, i, &rel) == NULL) {
			elfdie(src->e_filename,
			       "Can't read relocation entry at %d", i);
		}

		type = GELF_R_TYPE(rel.r_info);

		/* Find section that contains this offset. */
		for (sip = scinfo; sip != NULL; sip = sip->si_next) {
			if (sip->si_addr == NULL) {
				continue;
			}

			if (rel.r_offset >= sip->si_addr &&
			    rel.r_offset < sip->si_addr + sip->si_size) {
				break;
			}
		}
		if (sip == NULL) {
			fatal(0, "Can't find section that contains 0x%llx: "
			      "index=%d", rel.r_offset, i);
		}
		foff = (off_t)(rel.r_offset - sip->si_addr);
		data = sip->si_data;
		data->d_type = ELF_T_WORD;
		if (sip->si_newdata == NULL) {
			fatal(0, "No buffer is allocated for "
			      "SHF_ALLOC'ed section");
		}
		/* LINTED(E_BAD_PTR_CAST_ALIGN) */
		ptr = (uint32_t *)((caddr_t)sip->si_newdata + foff);
		ovalue = value = *ptr;

		if (type == R_ARM_ABS32) {
			/* We can simply update with new address. */
			if ((GElf_Word)value >= diffbase) {
				value += diff;
				update = 1;
			}
		}
		else if (type == R_ARM_PC24 || type == R_ARM_CALL ||
			 type == R_ARM_JUMP24) {
			uint32_t	target;

			/*
			 * Remarks:
			 *	This should not happen. BSS symbol must not
			 *	contains text.
			 */

			/* 24-bit immediate. Decode target address. */
			target = value & 0x00ffffff;
			if (target & 0x00800000) {
				target |= 0x3fffffff;
			}
			target <<= 2;
			target += (uint32_t)rel.r_offset + 8;
			if ((GElf_Word)target >= diffbase) {
				/* Encode again... */
				target += diff;
				target -= ((uint32_t)rel.r_offset + 8);
				target = (target >> 2) & 0x00ffffff;
				value = (value & 0xff000000) | target;
				update = 1;
			}
		}
		else {
			fatal(0, "%d: Unsupported relocation type: %d",
			      i, type);
		}
		if (update) {
			if (ovalue > endaddr) {
				/*
				 * .bss must be the last SHF_ALLOC'ed section.
				 */
				fatal(0, "Attempting to relocate address in "
				      "unexpected range: 0x%x, limit = 0x%llx",
				      ovalue, endaddr);
			}
			*ptr = value;
			verbose(2, "%d: Relocated: type=%d, off=0x%llx, "
				"foff=0x%lx, 0x%x -> 0x%x", i, type,
				rel.r_offset, foff, ovalue, value);
		}

		if (gelf_update_rel(ddata, i, &rel) == 0) {
			elfdie(dst->e_filename,
			       "Can't update relocation entry at %d", i);
		}
	}
}

/*
 * void
 * reloc_referred_symbols(char *file, GElf_Shdr *shdr, Elf_Data *data,
 *			  inthash_t *hash)
 *	Put all symbol numbers referred by relocation entries into
 *	the integer hash.
 */
void
reloc_referred_symbols(char *file, GElf_Shdr *shdr, Elf_Data *data,
		       inthash_t *hash)
{
	int		i, num;

	num = shdr->sh_size / shdr->sh_entsize;

	for (i = 0; i < num; i++) {
		GElf_Rel	rel;
		uint_t		sym;

		if (gelf_getrel(data, i, &rel) == NULL) {
			elfdie(file, "Can't read relocation entry at %d", i);
		}
		sym = GELF_R_SYM(rel.r_info);
		(void)inthash_put(hash, sym);
	}
}
