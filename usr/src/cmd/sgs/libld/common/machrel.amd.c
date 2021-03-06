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
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include	<string.h>
#include	<stdio.h>
#include	<strings.h>
#include	<sys/elf_amd64.h>
#include	<debug.h>
#include	<reloc.h>
#include	"msg.h"
#include	"_libld.h"

Word
ld_init_rel(Rel_desc *reld, void *reloc)
{
	Rela *	rel = (Rela *)reloc;

	/* LINTED */
	reld->rel_rtype = (Word)ELF_R_TYPE(rel->r_info);
	reld->rel_roffset = rel->r_offset;
	reld->rel_raddend = rel->r_addend;
	reld->rel_typedata = 0;

	reld->rel_flags |= FLG_REL_RELA;

	return ((Word)ELF_R_SYM(rel->r_info));
}

void
ld_mach_eflags(Ehdr *ehdr, Ofl_desc *ofl)
{
	ofl->ofl_dehdr->e_flags |= ehdr->e_flags;
}

void
ld_mach_make_dynamic(Ofl_desc *ofl, size_t *cnt)
{
	if (!(ofl->ofl_flags & FLG_OF_RELOBJ)) {
		/*
		 * Create this entry if we are going to create a PLT table.
		 */
		if (ofl->ofl_pltcnt)
			(*cnt)++;		/* DT_PLTGOT */
	}
}

void
ld_mach_update_odynamic(Ofl_desc *ofl, Dyn **dyn)
{
	if (((ofl->ofl_flags & FLG_OF_RELOBJ) == 0) && ofl->ofl_pltcnt) {
		(*dyn)->d_tag = DT_PLTGOT;
		if (ofl->ofl_osgot)
			(*dyn)->d_un.d_ptr = ofl->ofl_osgot->os_shdr->sh_addr;
		else
			(*dyn)->d_un.d_ptr = 0;
		(*dyn)++;
	}
}

Xword
ld_calc_plt_addr(Sym_desc *sdp, Ofl_desc *ofl)
{
	Xword	value;

	value = (Xword)(ofl->ofl_osplt->os_shdr->sh_addr) +
	    M_PLT_RESERVSZ + ((sdp->sd_aux->sa_PLTndx - 1) * M_PLT_ENTSIZE);
	return (value);
}

/*
 *  Build a single plt entry - code is:
 *	JMP	*name1@GOTPCREL(%rip)
 *	PUSHL	$index
 *	JMP	.PLT0
 */
static uchar_t pltn_entry[M_PLT_ENTSIZE] = {
/* 0x00 jmpq *name1@GOTPCREL(%rip) */	0xff, 0x25, 0x00, 0x00, 0x00, 0x00,
/* 0x06 pushq $index */			0x68, 0x00, 0x00, 0x00, 0x00,
/* 0x0b jmpq  .plt0(%rip) */		0xe9, 0x00, 0x00, 0x00, 0x00
/* 0x10 */
};

static uintptr_t
plt_entry(Ofl_desc * ofl, Sym_desc * sdp)
{
	uchar_t		*plt0, *pltent, *gotent;
	Sword		plt_off;
	Word		got_off;
	Xword		val1;
	int		bswap;

	got_off = sdp->sd_aux->sa_PLTGOTndx * M_GOT_ENTSIZE;
	plt_off = M_PLT_RESERVSZ + ((sdp->sd_aux->sa_PLTndx - 1) *
	    M_PLT_ENTSIZE);
	plt0 = (uchar_t *)(ofl->ofl_osplt->os_outdata->d_buf);
	pltent = plt0 + plt_off;
	gotent = (uchar_t *)(ofl->ofl_osgot->os_outdata->d_buf) + got_off;

	bcopy(pltn_entry, pltent, sizeof (pltn_entry));
	/*
	 * Fill in the got entry with the address of the next instruction.
	 */
	/* LINTED */
	*(Word *)gotent = ofl->ofl_osplt->os_shdr->sh_addr + plt_off +
	    M_PLT_INSSIZE;

	/*
	 * If '-z noreloc' is specified - skip the do_reloc_ld
	 * stage.
	 */
	if (!OFL_DO_RELOC(ofl))
		return (1);

	/*
	 * If the running linker has a different byte order than
	 * the target host, tell do_reloc_ld() to swap bytes.
	 *
	 * We know the PLT is PROGBITS --- we don't have to check
	 */
	bswap = (ofl->ofl_flags1 & FLG_OF1_ENCDIFF) != 0;

	/*
	 * patchup:
	 *	jmpq	*name1@gotpcrel(%rip)
	 *
	 * NOTE: 0x06 represents next instruction.
	 */
	val1 = (ofl->ofl_osgot->os_shdr->sh_addr + got_off) -
	    (ofl->ofl_osplt->os_shdr->sh_addr + plt_off) - 0x06;

	if (do_reloc_ld(R_AMD64_GOTPCREL, &pltent[0x02],
	    &val1, MSG_ORIG(MSG_SYM_PLTENT),
	    MSG_ORIG(MSG_SPECFIL_PLTENT), bswap, ofl->ofl_lml) == 0) {
		eprintf(ofl->ofl_lml, ERR_FATAL, MSG_INTL(MSG_PLT_PLTNFAIL),
		    sdp->sd_aux->sa_PLTndx, demangle(sdp->sd_name));
		return (S_ERROR);
	}

	/*
	 * patchup:
	 *	pushq	$pltndx
	 */
	val1 = (Xword)(sdp->sd_aux->sa_PLTndx - 1);

	if (do_reloc_ld(R_AMD64_32, &pltent[0x07],
	    &val1, MSG_ORIG(MSG_SYM_PLTENT),
	    MSG_ORIG(MSG_SPECFIL_PLTENT), bswap, ofl->ofl_lml) == 0) {
		eprintf(ofl->ofl_lml, ERR_FATAL, MSG_INTL(MSG_PLT_PLTNFAIL),
		    sdp->sd_aux->sa_PLTndx, demangle(sdp->sd_name));
		return (S_ERROR);
	}

	/*
	 * patchup:
	 *	jmpq	.plt0(%rip)
	 * NOTE: 0x10 represents next instruction. The rather complex
	 * series of casts is necessary to sign extend an offset into
	 * a 64-bit value while satisfying various compiler error
	 * checks.  Handle with care.
	 */
	val1 = (Xword)((intptr_t)((uintptr_t)plt0 -
	    (uintptr_t)(&pltent[0x10])));

	if (do_reloc_ld(R_AMD64_PC32, &pltent[0x0c],
	    &val1, MSG_ORIG(MSG_SYM_PLTENT),
	    MSG_ORIG(MSG_SPECFIL_PLTENT), bswap, ofl->ofl_lml) == 0) {
		eprintf(ofl->ofl_lml, ERR_FATAL, MSG_INTL(MSG_PLT_PLTNFAIL),
		    sdp->sd_aux->sa_PLTndx, demangle(sdp->sd_name));
		return (S_ERROR);
	}

	return (1);
}

uintptr_t
ld_perform_outreloc(Rel_desc * orsp, Ofl_desc * ofl)
{
	Os_desc *	relosp, * osp = 0;
	Word		ndx;
	Xword		roffset, value;
	Sxword		raddend;
	Rela		rea;
	char		*relbits;
	Sym_desc *	sdp, * psym = (Sym_desc *)0;
	int		sectmoved = 0;

	raddend = orsp->rel_raddend;
	sdp = orsp->rel_sym;

	/*
	 * If the section this relocation is against has been discarded
	 * (-zignore), then also discard (skip) the relocation itself.
	 */
	if (orsp->rel_isdesc && ((orsp->rel_flags &
	    (FLG_REL_GOT | FLG_REL_BSS | FLG_REL_PLT | FLG_REL_NOINFO)) == 0) &&
	    (orsp->rel_isdesc->is_flags & FLG_IS_DISCARD)) {
		DBG_CALL(Dbg_reloc_discard(ofl->ofl_lml, M_MACH, orsp));
		return (1);
	}

	/*
	 * If this is a relocation against a move table, or expanded move
	 * table, adjust the relocation entries.
	 */
	if (orsp->rel_move)
		ld_adj_movereloc(ofl, orsp);

	/*
	 * If this is a relocation against a section then we need to adjust the
	 * raddend field to compensate for the new position of the input section
	 * within the new output section.
	 */
	if (ELF_ST_TYPE(sdp->sd_sym->st_info) == STT_SECTION) {
		if (ofl->ofl_parsym.head &&
		    (sdp->sd_isc->is_flags & FLG_IS_RELUPD) &&
		    /* LINTED */
		    (psym = ld_am_I_partial(orsp, orsp->rel_raddend))) {
			DBG_CALL(Dbg_move_outsctadj(ofl->ofl_lml, psym));
			sectmoved = 1;
			if (ofl->ofl_flags & FLG_OF_RELOBJ)
				raddend = psym->sd_sym->st_value;
			else
				raddend = psym->sd_sym->st_value -
				    psym->sd_isc->is_osdesc->os_shdr->sh_addr;
			/* LINTED */
			raddend += (Off)_elf_getxoff(psym->sd_isc->is_indata);
			if (psym->sd_isc->is_shdr->sh_flags & SHF_ALLOC)
				raddend +=
				    psym->sd_isc->is_osdesc->os_shdr->sh_addr;
		} else {
			/* LINTED */
			raddend += (Off)_elf_getxoff(sdp->sd_isc->is_indata);
			if (sdp->sd_isc->is_shdr->sh_flags & SHF_ALLOC)
				raddend +=
				    sdp->sd_isc->is_osdesc->os_shdr->sh_addr;
		}
	}

	value = sdp->sd_sym->st_value;

	if (orsp->rel_flags & FLG_REL_GOT) {
		/*
		 * Note: for GOT relative relocations on amd64
		 *	 we discard the addend.  It was relevant
		 *	 to the reference - not to the data item
		 *	 being referenced (ie: that -4 thing).
		 */
		raddend = 0;
		osp = ofl->ofl_osgot;
		roffset = ld_calc_got_offset(orsp, ofl);

	} else if (orsp->rel_flags & FLG_REL_PLT) {
		/*
		 * Note that relocations for PLT's actually
		 * cause a relocation againt the GOT.
		 */
		osp = ofl->ofl_osplt;
		roffset = (ofl->ofl_osgot->os_shdr->sh_addr) +
		    sdp->sd_aux->sa_PLTGOTndx * M_GOT_ENTSIZE;
		raddend = 0;
		if (plt_entry(ofl, sdp) == S_ERROR)
			return (S_ERROR);

	} else if (orsp->rel_flags & FLG_REL_BSS) {
		/*
		 * This must be a R_AMD64_COPY.  For these set the roffset to
		 * point to the new symbols location.
		 */
		osp = ofl->ofl_isbss->is_osdesc;
		roffset = value;

		/*
		 * The raddend doesn't mean anything in a R_SPARC_COPY
		 * relocation.  Null it out because it can confuse people.
		 */
		raddend = 0;
	} else {
		osp = orsp->rel_osdesc;

		/*
		 * Calculate virtual offset of reference point; equals offset
		 * into section + vaddr of section for loadable sections, or
		 * offset plus section displacement for nonloadable sections.
		 */
		roffset = orsp->rel_roffset +
		    (Off)_elf_getxoff(orsp->rel_isdesc->is_indata);
		if (!(ofl->ofl_flags & FLG_OF_RELOBJ))
			roffset += orsp->rel_isdesc->is_osdesc->
			    os_shdr->sh_addr;
	}

	if ((osp == 0) || ((relosp = osp->os_relosdesc) == 0))
		relosp = ofl->ofl_osrel;

	/*
	 * Assign the symbols index for the output relocation.  If the
	 * relocation refers to a SECTION symbol then it's index is based upon
	 * the output sections symbols index.  Otherwise the index can be
	 * derived from the symbols index itself.
	 */
	if (orsp->rel_rtype == R_AMD64_RELATIVE)
		ndx = STN_UNDEF;
	else if ((orsp->rel_flags & FLG_REL_SCNNDX) ||
	    (ELF_ST_TYPE(sdp->sd_sym->st_info) == STT_SECTION)) {
		if (sectmoved == 0) {
			/*
			 * Check for a null input section. This can
			 * occur if this relocation references a symbol
			 * generated by sym_add_sym().
			 */
			if ((sdp->sd_isc != 0) &&
			    (sdp->sd_isc->is_osdesc != 0))
				ndx = sdp->sd_isc->is_osdesc->os_scnsymndx;
			else
				ndx = sdp->sd_shndx;
		} else
			ndx = ofl->ofl_sunwdata1ndx;
	} else
		ndx = sdp->sd_symndx;

	/*
	 * Add the symbols 'value' to the addend field.
	 */
	if (orsp->rel_flags & FLG_REL_ADVAL)
		raddend += value;

	/*
	 * The addend field for R_AMD64_DTPMOD64 means nothing.  The addend
	 * is propagated in the corresponding R_AMD64_DTPOFF64 relocation.
	 */
	if (orsp->rel_rtype == R_AMD64_DTPMOD64)
		raddend = 0;

	relbits = (char *)relosp->os_outdata->d_buf;

	rea.r_info = ELF_R_INFO(ndx, orsp->rel_rtype);
	rea.r_offset = roffset;
	rea.r_addend = raddend;
	DBG_CALL(Dbg_reloc_out(ofl, ELF_DBG_LD, SHT_RELA, &rea, relosp->os_name,
	    orsp->rel_sname));

	/*
	 * Assert we haven't walked off the end of our relocation table.
	 */
	assert(relosp->os_szoutrels <= relosp->os_shdr->sh_size);

	(void) memcpy((relbits + relosp->os_szoutrels),
	    (char *)&rea, sizeof (Rela));
	relosp->os_szoutrels += (Xword)sizeof (Rela);

	/*
	 * Determine if this relocation is against a non-writable, allocatable
	 * section.  If so we may need to provide a text relocation diagnostic.
	 * Note that relocations against the .plt (R_AMD64_JUMP_SLOT) actually
	 * result in modifications to the .got.
	 */
	if (orsp->rel_rtype == R_AMD64_JUMP_SLOT)
		osp = ofl->ofl_osgot;

	ld_reloc_remain_entry(orsp, osp, ofl);
	return (1);
}

/*
 * amd64 Instructions for TLS processing
 */
static uchar_t tlsinstr_gd_ie[] = {
	/*
	 *	0x00 movq %fs:0, %rax
	 */
	0x64, 0x48, 0x8b, 0x04, 0x25,
	0x00, 0x00, 0x00, 0x00,
	/*
	 *	0x09 addq x@gottpoff(%rip), %rax
	 */
	0x48, 0x03, 0x05, 0x00, 0x00,
	0x00, 0x00
};

static uchar_t tlsinstr_gd_le[] = {
	/*
	 *	0x00 movq %fs:0, %rax
	 */
	0x64, 0x48, 0x8b, 0x04, 0x25,
	0x00, 0x00, 0x00, 0x00,
	/*
	 *	0x09 leaq x@gottpoff(%rip), %rax
	 */
	0x48, 0x8d, 0x80, 0x00, 0x00,
	0x00, 0x00
};

static uchar_t tlsinstr_ld_le[] = {
	/*
	 * .byte 0x66
	 */
	0x66,
	/*
	 * .byte 0x66
	 */
	0x66,
	/*
	 * .byte 0x66
	 */
	0x66,
	/*
	 * movq %fs:0, %rax
	 */
	0x64, 0x48, 0x8b, 0x04, 0x25,
	0x00, 0x00, 0x00, 0x00
};


static Fixupret
tls_fixups(Ofl_desc *ofl, Rel_desc *arsp)
{
	Sym_desc	*sdp = arsp->rel_sym;
	Word		rtype = arsp->rel_rtype;
	uchar_t		*offset;

	offset = (uchar_t *)((uintptr_t)arsp->rel_roffset +
	    (uintptr_t)_elf_getxoff(arsp->rel_isdesc->is_indata) +
	    (uintptr_t)arsp->rel_osdesc->os_outdata->d_buf);

	if (sdp->sd_ref == REF_DYN_NEED) {
		/*
		 * IE reference model
		 */
		switch (rtype) {
		case R_AMD64_TLSGD:
			/*
			 *  GD -> IE
			 *
			 * Transition:
			 *	0x00 .byte 0x66
			 *	0x01 leaq x@tlsgd(%rip), %rdi
			 *	0x08 .word 0x6666
			 *	0x0a rex64
			 *	0x0b call __tls_get_addr@plt
			 *	0x10
			 * To:
			 *	0x00 movq %fs:0, %rax
			 *	0x09 addq x@gottpoff(%rip), %rax
			 *	0x10
			 */
			DBG_CALL(Dbg_reloc_transition(ofl->ofl_lml, M_MACH,
			    R_AMD64_GOTTPOFF, arsp));
			arsp->rel_rtype = R_AMD64_GOTTPOFF;
			arsp->rel_roffset += 8;
			arsp->rel_raddend = (Sxword)-4;

			/*
			 * Adjust 'offset' to beginning of instruction
			 * sequence.
			 */
			offset -= 4;
			(void) memcpy(offset, tlsinstr_gd_ie,
			    sizeof (tlsinstr_gd_ie));
			return (FIX_RELOC);

		case R_AMD64_PLT32:
			/*
			 * Fixup done via the TLS_GD relocation.
			 */
			DBG_CALL(Dbg_reloc_transition(ofl->ofl_lml, M_MACH,
			    R_AMD64_NONE, arsp));
			return (FIX_DONE);
		}
	}

	/*
	 * LE reference model
	 */
	switch (rtype) {
	case R_AMD64_TLSGD:
		/*
		 * GD -> LE
		 *
		 * Transition:
		 *	0x00 .byte 0x66
		 *	0x01 leaq x@tlsgd(%rip), %rdi
		 *	0x08 .word 0x6666
		 *	0x0a rex64
		 *	0x0b call __tls_get_addr@plt
		 *	0x10
		 * To:
		 *	0x00 movq %fs:0, %rax
		 *	0x09 leaq x@tpoff(%rax), %rax
		 *	0x10
		 */
		DBG_CALL(Dbg_reloc_transition(ofl->ofl_lml, M_MACH,
		    R_AMD64_TPOFF32, arsp));
		arsp->rel_rtype = R_AMD64_TPOFF32;
		arsp->rel_roffset += 8;
		arsp->rel_raddend = 0;

		/*
		 * Adjust 'offset' to beginning of instruction sequence.
		 */
		offset -= 4;
		(void) memcpy(offset, tlsinstr_gd_le, sizeof (tlsinstr_gd_le));
		return (FIX_RELOC);

	case R_AMD64_GOTTPOFF:
		/*
		 * IE -> LE
		 *
		 * Transition:
		 *	0x00 movq %fs:0, %rax
		 *	0x09 addq x@gottopoff(%rip), %rax
		 *	0x10
		 * To:
		 *	0x00 movq %fs:0, %rax
		 *	0x09 leaq x@tpoff(%rax), %rax
		 *	0x10
		 */
		DBG_CALL(Dbg_reloc_transition(ofl->ofl_lml, M_MACH,
		    R_AMD64_TPOFF32, arsp));
		arsp->rel_rtype = R_AMD64_TPOFF32;
		arsp->rel_raddend = 0;

		/*
		 * Adjust 'offset' to beginning of instruction sequence.
		 */
		offset -= 12;

		/*
		 * Same code sequence used in the GD -> LE transition.
		 */
		(void) memcpy(offset, tlsinstr_gd_le, sizeof (tlsinstr_gd_le));
		return (FIX_RELOC);

	case R_AMD64_TLSLD:
		/*
		 * LD -> LE
		 *
		 * Transition
		 *	0x00 leaq x1@tlsgd(%rip), %rdi
		 *	0x07 call __tls_get_addr@plt
		 *	0x0c
		 * To:
		 *	0x00 .byte 0x66
		 *	0x01 .byte 0x66
		 *	0x02 .byte 0x66
		 *	0x03 movq %fs:0, %rax
		 */
		DBG_CALL(Dbg_reloc_transition(ofl->ofl_lml, M_MACH,
		    R_AMD64_NONE, arsp));
		offset -= 3;
		(void) memcpy(offset, tlsinstr_ld_le, sizeof (tlsinstr_ld_le));
		return (FIX_DONE);

	case R_AMD64_DTPOFF32:
		/*
		 * LD->LE
		 *
		 * Transition:
		 *	0x00 leaq x1@dtpoff(%rax), %rcx
		 * To:
		 *	0x00 leaq x1@tpoff(%rax), %rcx
		 */
		DBG_CALL(Dbg_reloc_transition(ofl->ofl_lml, M_MACH,
		    R_AMD64_TPOFF32, arsp));
		arsp->rel_rtype = R_AMD64_TPOFF32;
		arsp->rel_raddend = 0;
		return (FIX_RELOC);
	}

	return (FIX_RELOC);
}

uintptr_t
ld_do_activerelocs(Ofl_desc *ofl)
{
	Rel_desc	*arsp;
	Rel_cache	*rcp;
	Listnode	*lnp;
	uintptr_t	return_code = 1;
	Word		flags = ofl->ofl_flags;

	if (ofl->ofl_actrels.head)
		DBG_CALL(Dbg_reloc_doact_title(ofl->ofl_lml));

	/*
	 * Process active relocations.
	 */
	for (LIST_TRAVERSE(&ofl->ofl_actrels, lnp, rcp)) {
		/* LINTED */
		for (arsp = (Rel_desc *)(rcp + 1);
		    arsp < rcp->rc_free; arsp++) {
			uchar_t		*addr;
			Xword 		value;
			Sym_desc	*sdp;
			const char	*ifl_name;
			Xword		refaddr;
			int		moved = 0;
			Gotref		gref;

			/*
			 * If the section this relocation is against has been
			 * discarded (-zignore), then discard (skip) the
			 * relocation itself.
			 */
			if ((arsp->rel_isdesc->is_flags & FLG_IS_DISCARD) &&
			    ((arsp->rel_flags &
			    (FLG_REL_GOT | FLG_REL_BSS |
			    FLG_REL_PLT | FLG_REL_NOINFO)) == 0)) {
				DBG_CALL(Dbg_reloc_discard(ofl->ofl_lml,
				    M_MACH, arsp));
				continue;
			}

			/*
			 * We deteremine what the 'got reference'
			 * model (if required) is at this point.  This
			 * needs to be done before tls_fixup() since
			 * it may 'transition' our instructions.
			 *
			 * The got table entries have already been assigned,
			 * and we bind to those initial entries.
			 */
			if (arsp->rel_flags & FLG_REL_DTLS)
				gref = GOT_REF_TLSGD;
			else if (arsp->rel_flags & FLG_REL_MTLS)
				gref = GOT_REF_TLSLD;
			else if (arsp->rel_flags & FLG_REL_STLS)
				gref = GOT_REF_TLSIE;
			else
				gref = GOT_REF_GENERIC;

			/*
			 * Perform any required TLS fixups.
			 */
			if (arsp->rel_flags & FLG_REL_TLSFIX) {
				Fixupret	ret;

				if ((ret = tls_fixups(ofl, arsp)) == FIX_ERROR)
					return (S_ERROR);
				if (ret == FIX_DONE)
					continue;
			}

			/*
			 * If this is a relocation against a move table, or
			 * expanded move table, adjust the relocation entries.
			 */
			if (arsp->rel_move)
				ld_adj_movereloc(ofl, arsp);

			sdp = arsp->rel_sym;
			refaddr = arsp->rel_roffset +
			    (Off)_elf_getxoff(arsp->rel_isdesc->is_indata);

			if ((arsp->rel_flags & FLG_REL_CLVAL) ||
			    (arsp->rel_flags & FLG_REL_GOTCL))
				value = 0;
			else if (ELF_ST_TYPE(sdp->sd_sym->st_info) ==
			    STT_SECTION) {
				Sym_desc	*sym;

				/*
				 * The value for a symbol pointing to a SECTION
				 * is based off of that sections position.
				 *
				 * The second argument of the ld_am_I_partial()
				 * is the value stored at the target address
				 * relocation is going to be applied.
				 */
				if ((sdp->sd_isc->is_flags & FLG_IS_RELUPD) &&
				    /* LINTED */
				    (sym = ld_am_I_partial(arsp, *(Xword *)
				    ((uchar_t *)
				    arsp->rel_isdesc->is_indata->d_buf +
				    arsp->rel_roffset)))) {
					/*
					 * If the symbol is moved,
					 * adjust the value
					 */
					value = sym->sd_sym->st_value;
					moved = 1;
				} else {
					value = _elf_getxoff(
					    sdp->sd_isc->is_indata);
					if (sdp->sd_isc->is_shdr->sh_flags &
					    SHF_ALLOC)
						value +=
						    sdp->sd_isc->is_osdesc->
						    os_shdr->sh_addr;
				}
				if (sdp->sd_isc->is_shdr->sh_flags & SHF_TLS)
					value -= ofl->ofl_tlsphdr->p_vaddr;

			} else if (IS_SIZE(arsp->rel_rtype)) {
				/*
				 * Size relocations require the symbols size.
				 */
				value = sdp->sd_sym->st_size;
			} else {
				/*
				 * Else the value is the symbols value.
				 */
				value = sdp->sd_sym->st_value;
			}

			/*
			 * Relocation against the GLOBAL_OFFSET_TABLE.
			 */
			if (arsp->rel_flags & FLG_REL_GOT)
				arsp->rel_osdesc = ofl->ofl_osgot;

			/*
			 * If loadable and not producing a relocatable object
			 * add the sections virtual address to the reference
			 * address.
			 */
			if ((arsp->rel_flags & FLG_REL_LOAD) &&
			    ((flags & FLG_OF_RELOBJ) == 0))
				refaddr += arsp->rel_isdesc->is_osdesc->
				    os_shdr->sh_addr;

			/*
			 * If this entry has a PLT assigned to it, it's
			 * value is actually the address of the PLT (and
			 * not the address of the function).
			 */
			if (IS_PLT(arsp->rel_rtype)) {
				if (sdp->sd_aux && sdp->sd_aux->sa_PLTndx)
					value = ld_calc_plt_addr(sdp, ofl);
			}

			/*
			 * Add relocations addend to value.  Add extra
			 * relocation addend if needed.
			 *
			 * Note: for GOT relative relocations on amd64
			 *	 we discard the addend.  It was relevant
			 *	 to the reference - not to the data item
			 *	 being referenced (ie: that -4 thing).
			 */
			if ((arsp->rel_flags & FLG_REL_GOT) == 0)
				value += arsp->rel_raddend;

			/*
			 * Determine whether the value needs further adjustment.
			 * Filter through the attributes of the relocation to
			 * determine what adjustment is required.  Note, many
			 * of the following cases are only applicable when a
			 * .got is present.  As a .got is not generated when a
			 * relocatable object is being built, any adjustments
			 * that require a .got need to be skipped.
			 */
			if ((arsp->rel_flags & FLG_REL_GOT) &&
			    ((flags & FLG_OF_RELOBJ) == 0)) {
				Xword		R1addr;
				uintptr_t	R2addr;
				Word		gotndx;
				Gotndx		*gnp;

				/*
				 * Perform relocation against GOT table.  Since
				 * this doesn't fit exactly into a relocation
				 * we place the appropriate byte in the GOT
				 * directly
				 *
				 * Calculate offset into GOT at which to apply
				 * the relocation.
				 */
				gnp = ld_find_gotndx(&(sdp->sd_GOTndxs), gref,
				    ofl, arsp);
				assert(gnp);

				if (arsp->rel_rtype == R_AMD64_DTPOFF64)
					gotndx = gnp->gn_gotndx + 1;
				else
					gotndx = gnp->gn_gotndx;

				R1addr = (Xword)(gotndx * M_GOT_ENTSIZE);

				/*
				 * Add the GOTs data's offset.
				 */
				R2addr = R1addr + (uintptr_t)
				    arsp->rel_osdesc->os_outdata->d_buf;

				DBG_CALL(Dbg_reloc_doact(ofl->ofl_lml,
				    ELF_DBG_LD, M_MACH, SHT_RELA,
				    arsp->rel_rtype, R1addr, value,
				    arsp->rel_sname, arsp->rel_osdesc));

				/*
				 * And do it.
				 */
				if (ofl->ofl_flags1 & FLG_OF1_ENCDIFF)
					*(Xword *)R2addr =
					    ld_byteswap_Xword(value);
				else
					*(Xword *)R2addr = value;
				continue;

			} else if (IS_GOT_BASED(arsp->rel_rtype) &&
			    ((flags & FLG_OF_RELOBJ) == 0)) {
				value -= ofl->ofl_osgot->os_shdr->sh_addr;

			} else if (IS_GOTPCREL(arsp->rel_rtype) &&
			    ((flags & FLG_OF_RELOBJ) == 0)) {
				Gotndx *gnp;

				/*
				 * Calculation:
				 *	G + GOT + A - P
				 */
				gnp = ld_find_gotndx(&(sdp->sd_GOTndxs),
				    gref, ofl, arsp);
				assert(gnp);
				value = (Xword)(ofl->ofl_osgot->os_shdr->
				    sh_addr) + ((Xword)gnp->gn_gotndx *
				    M_GOT_ENTSIZE) + arsp->rel_raddend -
				    refaddr;

			} else if (IS_GOT_PC(arsp->rel_rtype) &&
			    ((flags & FLG_OF_RELOBJ) == 0)) {
				value = (Xword)(ofl->ofl_osgot->os_shdr->
				    sh_addr) - refaddr + arsp->rel_raddend;

			} else if ((IS_PC_RELATIVE(arsp->rel_rtype)) &&
			    (((flags & FLG_OF_RELOBJ) == 0) ||
			    (arsp->rel_osdesc == sdp->sd_isc->is_osdesc))) {
				value -= refaddr;

			} else if (IS_TLS_INS(arsp->rel_rtype) &&
			    IS_GOT_RELATIVE(arsp->rel_rtype) &&
			    ((flags & FLG_OF_RELOBJ) == 0)) {
				Gotndx	*gnp;

				gnp = ld_find_gotndx(&(sdp->sd_GOTndxs), gref,
				    ofl, arsp);
				assert(gnp);
				value = (Xword)gnp->gn_gotndx * M_GOT_ENTSIZE;

			} else if (IS_GOT_RELATIVE(arsp->rel_rtype) &&
			    ((flags & FLG_OF_RELOBJ) == 0)) {
				Gotndx *gnp;

				gnp = ld_find_gotndx(&(sdp->sd_GOTndxs),
				    gref, ofl, arsp);
				assert(gnp);
				value = (Xword)gnp->gn_gotndx * M_GOT_ENTSIZE;

			} else if ((arsp->rel_flags & FLG_REL_STLS) &&
			    ((flags & FLG_OF_RELOBJ) == 0)) {
				Xword	tlsstatsize;

				/*
				 * This is the LE TLS reference model.  Static
				 * offset is hard-coded.
				 */
				tlsstatsize =
				    S_ROUND(ofl->ofl_tlsphdr->p_memsz,
				    M_TLSSTATALIGN);
				value = tlsstatsize - value;

				/*
				 * Since this code is fixed up, it assumes a
				 * negative offset that can be added to the
				 * thread pointer.
				 */
				if (arsp->rel_rtype == R_AMD64_TPOFF32)
					value = -value;
			}

			if (arsp->rel_isdesc->is_file)
				ifl_name = arsp->rel_isdesc->is_file->ifl_name;
			else
				ifl_name = MSG_INTL(MSG_STR_NULL);

			/*
			 * Make sure we have data to relocate.  Compiler and
			 * assembler developers have been known to generate
			 * relocations against invalid sections (normally .bss),
			 * so for their benefit give them sufficient information
			 * to help analyze the problem.  End users should never
			 * see this.
			 */
			if (arsp->rel_isdesc->is_indata->d_buf == 0) {
				Conv_inv_buf_t inv_buf;

				eprintf(ofl->ofl_lml, ERR_FATAL,
				    MSG_INTL(MSG_REL_EMPTYSEC),
				    conv_reloc_amd64_type(arsp->rel_rtype,
				    0, &inv_buf), ifl_name,
				    demangle(arsp->rel_sname),
				    arsp->rel_isdesc->is_name);
				return (S_ERROR);
			}

			/*
			 * Get the address of the data item we need to modify.
			 */
			addr = (uchar_t *)((uintptr_t)arsp->rel_roffset +
			    (uintptr_t)_elf_getxoff(arsp->rel_isdesc->
			    is_indata));

			DBG_CALL(Dbg_reloc_doact(ofl->ofl_lml, ELF_DBG_LD,
			    M_MACH, SHT_RELA, arsp->rel_rtype, EC_NATPTR(addr),
			    value, arsp->rel_sname, arsp->rel_osdesc));
			addr += (uintptr_t)arsp->rel_osdesc->os_outdata->d_buf;

			if ((((uintptr_t)addr - (uintptr_t)ofl->ofl_nehdr) >
			    ofl->ofl_size) || (arsp->rel_roffset >
			    arsp->rel_osdesc->os_shdr->sh_size)) {
				int		class;
				Conv_inv_buf_t inv_buf;

				if (((uintptr_t)addr -
				    (uintptr_t)ofl->ofl_nehdr) > ofl->ofl_size)
					class = ERR_FATAL;
				else
					class = ERR_WARNING;

				eprintf(ofl->ofl_lml, class,
				    MSG_INTL(MSG_REL_INVALOFFSET),
				    conv_reloc_amd64_type(arsp->rel_rtype,
				    0, &inv_buf), ifl_name,
				    arsp->rel_isdesc->is_name,
				    demangle(arsp->rel_sname),
				    EC_ADDR((uintptr_t)addr -
				    (uintptr_t)ofl->ofl_nehdr));

				if (class == ERR_FATAL) {
					return_code = S_ERROR;
					continue;
				}
			}

			/*
			 * The relocation is additive.  Ignore the previous
			 * symbol value if this local partial symbol is
			 * expanded.
			 */
			if (moved)
				value -= *addr;

			/*
			 * If '-z noreloc' is specified - skip the do_reloc_ld
			 * stage.
			 */
			if (OFL_DO_RELOC(ofl)) {
				/*
				 * If this is a PROGBITS section and the
				 * running linker has a different byte order
				 * than the target host, tell do_reloc_ld()
				 * to swap bytes.
				 */
				if (do_reloc_ld((uchar_t)arsp->rel_rtype,
				    addr, &value, arsp->rel_sname, ifl_name,
				    OFL_SWAP_RELOC_DATA(ofl, arsp),
				    ofl->ofl_lml) == 0)
					return_code = S_ERROR;
			}
		}
	}
	return (return_code);
}

uintptr_t
ld_add_outrel(Word flags, Rel_desc *rsp, Ofl_desc *ofl)
{
	Rel_desc	*orsp;
	Rel_cache	*rcp;
	Sym_desc	*sdp = rsp->rel_sym;

	/*
	 * Static executables *do not* want any relocations against them.
	 * Since our engine still creates relocations against a WEAK UNDEFINED
	 * symbol in a static executable, it's best to disable them here
	 * instead of through out the relocation code.
	 */
	if ((ofl->ofl_flags & (FLG_OF_STATIC | FLG_OF_EXEC)) ==
	    (FLG_OF_STATIC | FLG_OF_EXEC))
		return (1);

	/*
	 * If no relocation cache structures are available allocate
	 * a new one and link it into the cache list.
	 */
	if ((ofl->ofl_outrels.tail == 0) ||
	    ((rcp = (Rel_cache *)ofl->ofl_outrels.tail->data) == 0) ||
	    ((orsp = rcp->rc_free) == rcp->rc_end)) {
		static size_t	nextsize = 0;
		size_t		size;

		/*
		 * Output relocation numbers can vary considerably between
		 * building executables or shared objects (pic vs. non-pic),
		 * etc.  But, they typically aren't very large, so for these
		 * objects use a standard bucket size.  For building relocatable
		 * objects, typically there will be an output relocation for
		 * every input relocation.
		 */
		if (nextsize == 0) {
			if (ofl->ofl_flags & FLG_OF_RELOBJ) {
				if ((size = ofl->ofl_relocincnt) == 0)
					size = REL_LOIDESCNO;
				if (size > REL_HOIDESCNO)
					nextsize = REL_HOIDESCNO;
				else
					nextsize = REL_LOIDESCNO;
			} else
				nextsize = size = REL_HOIDESCNO;
		} else
			size = nextsize;

		size = size * sizeof (Rel_desc);

		if (((rcp = libld_malloc(sizeof (Rel_cache) + size)) == 0) ||
		    (list_appendc(&ofl->ofl_outrels, rcp) == 0))
			return (S_ERROR);

		/* LINTED */
		rcp->rc_free = orsp = (Rel_desc *)(rcp + 1);
		/* LINTED */
		rcp->rc_end = (Rel_desc *)((char *)rcp->rc_free + size);
	}

	/*
	 * If we are adding a output relocation against a section
	 * symbol (non-RELATIVE) then mark that section.  These sections
	 * will be added to the .dynsym symbol table.
	 */
	if (sdp && (rsp->rel_rtype != M_R_RELATIVE) &&
	    ((flags & FLG_REL_SCNNDX) ||
	    (ELF_ST_TYPE(sdp->sd_sym->st_info) == STT_SECTION))) {

		/*
		 * If this is a COMMON symbol - no output section
		 * exists yet - (it's created as part of sym_validate()).
		 * So - we mark here that when it's created it should
		 * be tagged with the FLG_OS_OUTREL flag.
		 */
		if ((sdp->sd_flags & FLG_SY_SPECSEC) &&
		    (sdp->sd_sym->st_shndx == SHN_COMMON)) {
			if (ELF_ST_TYPE(sdp->sd_sym->st_info) != STT_TLS)
				ofl->ofl_flags1 |= FLG_OF1_BSSOREL;
			else
				ofl->ofl_flags1 |= FLG_OF1_TLSOREL;
		} else {
			Os_desc	*osp = sdp->sd_isc->is_osdesc;

			if (osp && ((osp->os_flags & FLG_OS_OUTREL) == 0)) {
				ofl->ofl_dynshdrcnt++;
				osp->os_flags |= FLG_OS_OUTREL;
			}
		}
	}

	*orsp = *rsp;
	orsp->rel_flags |= flags;

	rcp->rc_free++;
	ofl->ofl_outrelscnt++;

	if (flags & FLG_REL_GOT)
		ofl->ofl_relocgotsz += (Xword)sizeof (Rela);
	else if (flags & FLG_REL_PLT)
		ofl->ofl_relocpltsz += (Xword)sizeof (Rela);
	else if (flags & FLG_REL_BSS)
		ofl->ofl_relocbsssz += (Xword)sizeof (Rela);
	else if (flags & FLG_REL_NOINFO)
		ofl->ofl_relocrelsz += (Xword)sizeof (Rela);
	else
		orsp->rel_osdesc->os_szoutrels += (Xword)sizeof (Rela);

	if (orsp->rel_rtype == M_R_RELATIVE)
		ofl->ofl_relocrelcnt++;

	/*
	 * We don't perform sorting on PLT relocations because
	 * they have already been assigned a PLT index and if we
	 * were to sort them we would have to re-assign the plt indexes.
	 */
	if (!(flags & FLG_REL_PLT))
		ofl->ofl_reloccnt++;

	/*
	 * Insure a GLOBAL_OFFSET_TABLE is generated if required.
	 */
	if (IS_GOT_REQUIRED(orsp->rel_rtype))
		ofl->ofl_flags |= FLG_OF_BLDGOT;

	/*
	 * Identify and possibly warn of a displacement relocation.
	 */
	if (orsp->rel_flags & FLG_REL_DISP) {
		ofl->ofl_dtflags_1 |= DF_1_DISPRELPND;

		if (ofl->ofl_flags & FLG_OF_VERBOSE)
			ld_disp_errmsg(MSG_INTL(MSG_REL_DISPREL4), orsp, ofl);
	}
	DBG_CALL(Dbg_reloc_ors_entry(ofl->ofl_lml, ELF_DBG_LD, SHT_RELA,
	    M_MACH, orsp));
	return (1);
}

/*
 * Stub routine since register symbols are not supported on amd64.
 */
/* ARGSUSED */
uintptr_t
ld_reloc_register(Rel_desc * rsp, Is_desc * isp, Ofl_desc * ofl)
{
	eprintf(ofl->ofl_lml, ERR_FATAL, MSG_INTL(MSG_REL_NOREG));
	return (S_ERROR);
}

/*
 * process relocation for a LOCAL symbol
 */
uintptr_t
ld_reloc_local(Rel_desc * rsp, Ofl_desc * ofl)
{
	Word		flags = ofl->ofl_flags;
	Sym_desc	*sdp = rsp->rel_sym;
	Word		shndx = sdp->sd_sym->st_shndx;
	Word		ortype = rsp->rel_rtype;

	/*
	 * if ((shared object) and (not pc relative relocation) and
	 *    (not against ABS symbol))
	 * then
	 *	build R_AMD64_RELATIVE
	 * fi
	 */
	if ((flags & FLG_OF_SHAROBJ) && (rsp->rel_flags & FLG_REL_LOAD) &&
	    !(IS_PC_RELATIVE(rsp->rel_rtype)) && !(IS_SIZE(rsp->rel_rtype)) &&
	    !(IS_GOT_BASED(rsp->rel_rtype)) &&
	    !(rsp->rel_isdesc != NULL &&
	    (rsp->rel_isdesc->is_shdr->sh_type == SHT_SUNW_dof)) &&
	    (((sdp->sd_flags & FLG_SY_SPECSEC) == 0) ||
	    (shndx != SHN_ABS) || (sdp->sd_aux && sdp->sd_aux->sa_symspec))) {

		/*
		 * R_AMD64_RELATIVE updates a 64bit address, if this
		 * relocation isn't a 64bit binding then we can not
		 * simplify it to a RELATIVE relocation.
		 */
		if (reloc_table[ortype].re_fsize != sizeof (Addr)) {
			return (ld_add_outrel(NULL, rsp, ofl));
		}

		rsp->rel_rtype = R_AMD64_RELATIVE;
		if (ld_add_outrel(FLG_REL_ADVAL, rsp, ofl) == S_ERROR)
			return (S_ERROR);
		rsp->rel_rtype = ortype;
		return (1);
	}

	/*
	 * If the relocation is against a 'non-allocatable' section
	 * and we can not resolve it now - then give a warning
	 * message.
	 *
	 * We can not resolve the symbol if either:
	 *	a) it's undefined
	 *	b) it's defined in a shared library and a
	 *	   COPY relocation hasn't moved it to the executable
	 *
	 * Note: because we process all of the relocations against the
	 *	text segment before any others - we know whether
	 *	or not a copy relocation will be generated before
	 *	we get here (see reloc_init()->reloc_segments()).
	 */
	if (!(rsp->rel_flags & FLG_REL_LOAD) &&
	    ((shndx == SHN_UNDEF) ||
	    ((sdp->sd_ref == REF_DYN_NEED) &&
	    ((sdp->sd_flags & FLG_SY_MVTOCOMM) == 0)))) {
		Conv_inv_buf_t inv_buf;

		/*
		 * If the relocation is against a SHT_SUNW_ANNOTATE
		 * section - then silently ignore that the relocation
		 * can not be resolved.
		 */
		if (rsp->rel_osdesc &&
		    (rsp->rel_osdesc->os_shdr->sh_type == SHT_SUNW_ANNOTATE))
			return (0);
		(void) eprintf(ofl->ofl_lml, ERR_WARNING,
		    MSG_INTL(MSG_REL_EXTERNSYM),
		    conv_reloc_amd64_type(rsp->rel_rtype, 0, &inv_buf),
		    rsp->rel_isdesc->is_file->ifl_name,
		    demangle(rsp->rel_sname), rsp->rel_osdesc->os_name);
		return (1);
	}

	/*
	 * Perform relocation.
	 */
	return (ld_add_actrel(NULL, rsp, ofl));
}


uintptr_t
/* ARGSUSED */
ld_reloc_GOTOP(Boolean local, Rel_desc * rsp, Ofl_desc * ofl)
{
	/*
	 * Stub routine for common code compatibility, we shouldn't
	 * actually get here on amd64.
	 */
	assert(0);
	return (S_ERROR);
}

uintptr_t
ld_reloc_TLS(Boolean local, Rel_desc * rsp, Ofl_desc * ofl)
{
	Word		rtype = rsp->rel_rtype;
	Sym_desc	*sdp = rsp->rel_sym;
	Word		flags = ofl->ofl_flags;
	Gotndx		*gnp;

	/*
	 * If we're building an executable - use either the IE or LE access
	 * model.  If we're building a shared object process any IE model.
	 */
	if ((flags & FLG_OF_EXEC) || (IS_TLS_IE(rtype))) {
		/*
		 * Set the DF_STATIC_TLS flag.
		 */
		ofl->ofl_dtflags |= DF_STATIC_TLS;

		if (!local || ((flags & FLG_OF_EXEC) == 0)) {
			/*
			 * Assign a GOT entry for static TLS references.
			 */
			if ((gnp = ld_find_gotndx(&(sdp->sd_GOTndxs),
			    GOT_REF_TLSIE, ofl, rsp)) == 0) {

				if (ld_assign_got_TLS(local, rsp, ofl, sdp,
				    gnp, GOT_REF_TLSIE, FLG_REL_STLS,
				    rtype, R_AMD64_TPOFF64, 0) == S_ERROR)
					return (S_ERROR);
			}

			/*
			 * IE access model.
			 */
			if (IS_TLS_IE(rtype))
				return (ld_add_actrel(FLG_REL_STLS, rsp, ofl));

			/*
			 * Fixups are required for other executable models.
			 */
			return (ld_add_actrel((FLG_REL_TLSFIX | FLG_REL_STLS),
			    rsp, ofl));
		}

		/*
		 * LE access model.
		 */
		if (IS_TLS_LE(rtype))
			return (ld_add_actrel(FLG_REL_STLS, rsp, ofl));

		return (ld_add_actrel((FLG_REL_TLSFIX | FLG_REL_STLS),
		    rsp, ofl));
	}

	/*
	 * Building a shared object.
	 *
	 * Assign a GOT entry for a dynamic TLS reference.
	 */
	if (IS_TLS_LD(rtype) && ((gnp = ld_find_gotndx(&(sdp->sd_GOTndxs),
	    GOT_REF_TLSLD, ofl, rsp)) == 0)) {

		if (ld_assign_got_TLS(local, rsp, ofl, sdp, gnp, GOT_REF_TLSLD,
		    FLG_REL_MTLS, rtype, R_AMD64_DTPMOD64, 0) == S_ERROR)
			return (S_ERROR);

	} else if (IS_TLS_GD(rtype) &&
	    ((gnp = ld_find_gotndx(&(sdp->sd_GOTndxs), GOT_REF_TLSGD,
	    ofl, rsp)) == 0)) {

		if (ld_assign_got_TLS(local, rsp, ofl, sdp, gnp, GOT_REF_TLSGD,
		    FLG_REL_DTLS, rtype, R_AMD64_DTPMOD64,
		    R_AMD64_DTPOFF64) == S_ERROR)
			return (S_ERROR);
	}

	if (IS_TLS_LD(rtype))
		return (ld_add_actrel(FLG_REL_MTLS, rsp, ofl));

	return (ld_add_actrel(FLG_REL_DTLS, rsp, ofl));
}

/* ARGSUSED3 */
Gotndx *
ld_find_gotndx(List * lst, Gotref gref, Ofl_desc * ofl, Rel_desc * rdesc)
{
	Listnode *	lnp;
	Gotndx *	gnp;

	assert(rdesc != 0);

	if ((gref == GOT_REF_TLSLD) && ofl->ofl_tlsldgotndx)
		return (ofl->ofl_tlsldgotndx);

	for (LIST_TRAVERSE(lst, lnp, gnp)) {
		if ((rdesc->rel_raddend == gnp->gn_addend) &&
		    (gnp->gn_gotref == gref)) {
			return (gnp);
		}
	}
	return ((Gotndx *)0);
}

Xword
ld_calc_got_offset(Rel_desc * rdesc, Ofl_desc * ofl)
{
	Os_desc		*osp = ofl->ofl_osgot;
	Sym_desc	*sdp = rdesc->rel_sym;
	Xword		gotndx;
	Gotref		gref;
	Gotndx		*gnp;

	if (rdesc->rel_flags & FLG_REL_DTLS)
		gref = GOT_REF_TLSGD;
	else if (rdesc->rel_flags & FLG_REL_MTLS)
		gref = GOT_REF_TLSLD;
	else if (rdesc->rel_flags & FLG_REL_STLS)
		gref = GOT_REF_TLSIE;
	else
		gref = GOT_REF_GENERIC;

	gnp = ld_find_gotndx(&(sdp->sd_GOTndxs), gref, ofl, rdesc);
	assert(gnp);

	gotndx = (Xword)gnp->gn_gotndx;

	if ((rdesc->rel_flags & FLG_REL_DTLS) &&
	    (rdesc->rel_rtype == R_AMD64_DTPOFF64))
		gotndx++;

	return ((Xword)(osp->os_shdr->sh_addr + (gotndx * M_GOT_ENTSIZE)));
}


/* ARGSUSED5 */
uintptr_t
ld_assign_got_ndx(List * lst, Gotndx * pgnp, Gotref gref, Ofl_desc * ofl,
    Rel_desc * rsp, Sym_desc * sdp)
{
	Xword		raddend;
	Gotndx		*gnp, *_gnp;
	Listnode	*lnp, *plnp;
	uint_t		gotents;

	raddend = rsp->rel_raddend;
	if (pgnp && (pgnp->gn_addend == raddend) &&
	    (pgnp->gn_gotref == gref))
		return (1);

	if ((gref == GOT_REF_TLSGD) || (gref == GOT_REF_TLSLD))
		gotents = 2;
	else
		gotents = 1;

	plnp = 0;
	for (LIST_TRAVERSE(lst, lnp, _gnp)) {
		if (_gnp->gn_addend > raddend)
			break;
		plnp = lnp;
	}

	/*
	 * Allocate a new entry.
	 */
	if ((gnp = libld_calloc(sizeof (Gotndx), 1)) == 0)
		return (S_ERROR);
	gnp->gn_addend = raddend;
	gnp->gn_gotndx = ofl->ofl_gotcnt;
	gnp->gn_gotref = gref;

	ofl->ofl_gotcnt += gotents;

	if (gref == GOT_REF_TLSLD) {
		ofl->ofl_tlsldgotndx = gnp;
		return (1);
	}

	if (plnp == 0) {
		/*
		 * Insert at head of list
		 */
		if (list_prependc(lst, (void *)gnp) == 0)
			return (S_ERROR);
	} else if (_gnp->gn_addend > raddend) {
		/*
		 * Insert in middle of lest
		 */
		if (list_insertc(lst, (void *)gnp, plnp) == 0)
			return (S_ERROR);
	} else {
		/*
		 * Append to tail of list
		 */
		if (list_appendc(lst, (void *)gnp) == 0)
			return (S_ERROR);
	}
	return (1);
}

void
ld_assign_plt_ndx(Sym_desc * sdp, Ofl_desc *ofl)
{
	sdp->sd_aux->sa_PLTndx = 1 + ofl->ofl_pltcnt++;
	sdp->sd_aux->sa_PLTGOTndx = ofl->ofl_gotcnt++;
	ofl->ofl_flags |= FLG_OF_BLDGOT;
}

static uchar_t plt0_template[M_PLT_ENTSIZE] = {
/* 0x00 PUSHQ GOT+8(%rip) */	0xff, 0x35, 0x00, 0x00, 0x00, 0x00,
/* 0x06 JMP   *GOT+16(%rip) */	0xff, 0x25, 0x00, 0x00, 0x00, 0x00,
/* 0x0c NOP */			0x90,
/* 0x0d NOP */			0x90,
/* 0x0e NOP */			0x90,
/* 0x0f NOP */			0x90
};

/*
 * Initializes .got[0] with the _DYNAMIC symbol value.
 */
uintptr_t
ld_fillin_gotplt(Ofl_desc *ofl)
{
	if (ofl->ofl_osgot) {
		Sym_desc	*sdp;

		if ((sdp = ld_sym_find(MSG_ORIG(MSG_SYM_DYNAMIC_U),
		    SYM_NOHASH, 0, ofl)) != NULL) {
			uchar_t	*genptr;

			genptr = ((uchar_t *)ofl->ofl_osgot->os_outdata->d_buf +
			    (M_GOT_XDYNAMIC * M_GOT_ENTSIZE));
			/* LINTED */
			*(Xword *)genptr = sdp->sd_sym->st_value;
		}
	}

	/*
	 * Fill in the reserved slot in the procedure linkage table the first
	 * entry is:
	 *	0x00 PUSHQ	GOT+8(%rip)	    # GOT[1]
	 *	0x06 JMP	*GOT+16(%rip)	    # GOT[2]
	 *	0x0c NOP
	 *	0x0d NOP
	 *	0x0e NOP
	 *	0x0f NOP
	 */
	if ((ofl->ofl_flags & FLG_OF_DYNAMIC) && ofl->ofl_osplt) {
		uchar_t	*pltent;
		Xword	val1;
		int	bswap;

		pltent = (uchar_t *)ofl->ofl_osplt->os_outdata->d_buf;
		bcopy(plt0_template, pltent, sizeof (plt0_template));

		/*
		 * If '-z noreloc' is specified - skip the do_reloc_ld
		 * stage.
		 */
		if (!OFL_DO_RELOC(ofl))
			return (1);

		/*
		 * If the running linker has a different byte order than
		 * the target host, tell do_reloc_ld() to swap bytes.
		 *
		 * We know the GOT is PROGBITS --- we don't have
		 * to check.
		 */
		bswap = (ofl->ofl_flags1 & FLG_OF1_ENCDIFF) != 0;

		/*
		 * filin:
		 *	PUSHQ GOT + 8(%rip)
		 *
		 * Note: 0x06 below represents the offset to the
		 *	 next instruction - which is what %rip will
		 *	 be pointing at.
		 */
		val1 = (ofl->ofl_osgot->os_shdr->sh_addr) +
		    (M_GOT_XLINKMAP * M_GOT_ENTSIZE) -
		    ofl->ofl_osplt->os_shdr->sh_addr - 0x06;

		if (do_reloc_ld(R_AMD64_GOTPCREL, &pltent[0x02],
		    &val1, MSG_ORIG(MSG_SYM_PLTENT),
		    MSG_ORIG(MSG_SPECFIL_PLTENT), bswap, ofl->ofl_lml) == 0) {
			eprintf(ofl->ofl_lml, ERR_FATAL,
			    MSG_INTL(MSG_PLT_PLT0FAIL));
			return (S_ERROR);
		}

		/*
		 * filin:
		 *  JMP	*GOT+16(%rip)
		 */
		val1 = (ofl->ofl_osgot->os_shdr->sh_addr) +
		    (M_GOT_XRTLD * M_GOT_ENTSIZE) -
		    ofl->ofl_osplt->os_shdr->sh_addr - 0x0c;

		if (do_reloc_ld(R_AMD64_GOTPCREL, &pltent[0x08],
		    &val1, MSG_ORIG(MSG_SYM_PLTENT),
		    MSG_ORIG(MSG_SPECFIL_PLTENT), bswap, ofl->ofl_lml) == 0) {
			eprintf(ofl->ofl_lml, ERR_FATAL,
			    MSG_INTL(MSG_PLT_PLT0FAIL));
			return (S_ERROR);
		}
	}

	return (1);
}
