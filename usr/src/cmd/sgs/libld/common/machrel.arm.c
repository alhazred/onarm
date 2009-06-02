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
 *	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2007-2008 NEC Corporation
 */

#include	<string.h>
#include	<stdio.h>
#include	<sys/elf_ARM.h>
#include	<debug.h>
#include	<reloc.h>
#include	<dwarf.h>
#include	"msg.h"
#include	"_libld.h"

/* Flag to determine whether text relocation should be fixed or not. */
typedef enum {
	FPLT_NONE,		/* Don't rewrite text relocation */
	FPLT_WARN,		/* Rewrite, but print warning */
	FPLT_FORCE		/* Force to rewrite text relocation */
} force_plt_t;

static force_plt_t	force_plt = FPLT_NONE;

/*
 * Section names that corresponding to .gnu.linkonce sections.
 */
typedef struct lo_name {
	const Msg	ln_type;	/* section type in section name */
	size_t		ln_typelen;	/* length of ln_type */
	const Msg	ln_name;	/* actual section name */
	size_t		ln_namelen;	/* length of ln_name */
} lo_name_t;

#define	LOMSG_TYPE(type)	MSG_LINKONCE_TYPE_##type
#define	LOMSG_TYPE_SIZE(type)	MSG_LINKONCE_TYPE_##type##_SIZE
#define	LOMSG_SCN(type)		MSG_SCN_##type
#define	LOMSG_SCN_SIZE(type)	MSG_SCN_##type##_SIZE

#define	LONAME_DECL(type)					\
	{LOMSG_TYPE(type), LOMSG_TYPE_SIZE(type),		\
	 LOMSG_SCN(type), LOMSG_SCN_SIZE(type)}

static lo_name_t	gnu_linkonce_names[] = {
	/* .data.rel.ro must be checked at first. */
	LONAME_DECL(DATA_REL_RO),
	LONAME_DECL(TEXT),
	LONAME_DECL(RODATA),
	LONAME_DECL(DATA),
	LONAME_DECL(BSS),
	LONAME_DECL(TDATA),
	LONAME_DECL(TBSS),
	LONAME_DECL(DEBUG_INFO),
	{0, 0, 0, 0}
};

/*
 * Hash table to keep .gnu.linkonce sections that are already linked.
 */

#define	LO_HASH_SIZE	512		/* must be a power of 2 */

#define	LO_HASHFUNC(name)	(sgs_str_hash(name) & (LO_HASH_SIZE - 1))

struct lo_hashent;
typedef struct lo_hashent	lo_hashent_t;

struct lo_hashent {
	char		*lhe_name;		/* original section name */
	Is_desc		*lhe_isp;		/* input section */
	lo_hashent_t	*lhe_next;
};

typedef struct lo_hash {
	lo_hashent_t	*lh_table[LO_HASH_SIZE];	/* hash table */
} lo_hash_t;

static uintptr_t	lo_hash_insert(lo_hash_t *lhash, Is_desc *isp);
static Is_desc		*lo_hash_lookup(lo_hash_t *lhash, const char *name);

Word
ld_init_rel(Rel_desc *reld, void *reloc)
{
	Rel *	rel = (Rel *)reloc;

	/* LINTED */
	reld->rel_rtype = (Word)ELF_R_TYPE(rel->r_info);
	reld->rel_roffset = rel->r_offset;
	reld->rel_raddend = 0;
	reld->rel_typedata = 0;

	return ((Word)ELF_R_SYM(rel->r_info));
}

void
ld_mach_eflags(Ehdr *ehdr, Ofl_desc *ofl)
{
	/*
	 * Do NOT copy EABI version flag.
	 * Compatibility check is already done by ld_mach_ifl_verify().
	 */
	ofl->ofl_dehdr->e_flags |= (ehdr->e_flags & ~EF_ARM_EABIMASK);
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
 *	ADD	r12, pc, #(got_off@GOT & 0xfff00000)
 *	ADD	r12, r12, #(got_off@GOT & 0x000ff000)
 *	LDR	pc, [r12, #(got_off@GOT & 0x00000fff)]!
 *
 *	LDR pc jumps address in the GOT entry with GOT entry address in r12.
 */
static void
plt_entry(Ofl_desc *ofl, Sym_desc *sdp)
{
	uchar_t		*pltent, *gotent;
	Word		plt_off;
	Word		got_off;
	Word		diff;
	Addr		got_addr, plt_addr;

	got_off = sdp->sd_aux->sa_PLTGOTndx * M_GOT_ENTSIZE;
	plt_off = M_PLT_RESERVSZ + ((sdp->sd_aux->sa_PLTndx - 1) *
	    M_PLT_ENTSIZE);
	pltent = (uchar_t *)(ofl->ofl_osplt->os_outdata->d_buf) + plt_off;
	gotent = (uchar_t *)(ofl->ofl_osgot->os_outdata->d_buf) + got_off;
	plt_addr = ofl->ofl_osplt->os_shdr->sh_addr;
	got_addr = ofl->ofl_osgot->os_shdr->sh_addr;

	/*
	 * Calculate the displacement between the PLT entry and the
	 * corresponding GOT entry. We must subtract M_PC_ACCESS_OFFSET
	 * from the actual displacement because it will be used by
	 * pc relative access.
	 */
	diff = ((Word)got_addr + got_off) -
		((Word)plt_addr + plt_off + M_PC_ACCESS_OFFSET);

	/*
	 * Fill in the got entry with the address of the next instruction.
	 */
	*(Word *)(uintptr_t)gotent = (Word)plt_addr;

	/* add ip, pc, #XXXXX */
	*(Word *)(uintptr_t)pltent =
		M_INST_ADD_IP_PC | (Word)((diff & 0xfff00000) >> 20);

	/* add ip, ip, #XXXXX */
	pltent += M_PLT_INSSIZE;
	*(Word *)(uintptr_t)pltent =
		M_INST_ADD_IP_IP | (Word)((diff & 0x000ff000) >> 12);

	/* ldr pc, [ip, #XXXXX] */
	pltent += M_PLT_INSSIZE;
	*(Word *)(uintptr_t)pltent =
		M_INST_LDR_PC_IP | (Word)(diff & 0x00000fff);
}

uintptr_t
ld_perform_outreloc(Rel_desc * orsp, Ofl_desc * ofl)
{
	Os_desc *	relosp, * osp = 0;
	Word		ndx, roffset, value;
	Rel		rea;
	char		*relbits;
	Sym_desc *	sdp, * psym = (Sym_desc *)0;
	int		sectmoved = 0;

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
	 * If this is a relocation against a section using a partial initialized
	 * symbol, adjust the embedded symbol info.
	 *
	 * The second argument of the am_I_partial() is the value stored at the
	 * target address relocation is going to be applied.
	 */
	if (ELF_ST_TYPE(sdp->sd_sym->st_info) == STT_SECTION) {
		if (ofl->ofl_parsym.head &&
		    (sdp->sd_isc->is_flags & FLG_IS_RELUPD) &&
		    /* LINTED */
		    (psym = ld_am_I_partial(orsp, *(Xword *)
		    ((uchar_t *)(orsp->rel_isdesc->is_indata->d_buf) +
		    orsp->rel_roffset)))) {
			DBG_CALL(Dbg_move_outsctadj(ofl->ofl_lml, psym));
			sectmoved = 1;
		}
	}

	value = sdp->sd_sym->st_value;

	if (orsp->rel_flags & FLG_REL_GOT) {
		osp = ofl->ofl_osgot;
		roffset = (Word)ld_calc_got_offset(orsp, ofl);

	} else if (orsp->rel_flags & FLG_REL_PLT) {
		/*
		 * Note that relocations for PLT's actually
		 * cause a relocation againt the GOT.
		 */
		osp = ofl->ofl_osplt;
		roffset = (Word) (ofl->ofl_osgot->os_shdr->sh_addr) +
		    sdp->sd_aux->sa_PLTGOTndx * M_GOT_ENTSIZE;

		plt_entry(ofl, sdp);

	} else if (orsp->rel_flags & FLG_REL_BSS) {
		/*
		 * This must be a R_ARM.  For these set the roffset to
		 * point to the new symbols location.
		 */
		osp = ofl->ofl_isbss->is_osdesc;
		roffset = (Word)value;
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
	if (orsp->rel_rtype == R_ARM_RELATIVE)
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
	 * If we have a replacement value for the relocation
	 * target, put it in place now.
	 */
	if (orsp->rel_flags & FLG_REL_NADDEND) {
		Xword	addend = orsp->rel_raddend;
		uchar_t	*addr;

		/*
		 * Get the address of the data item we need to modify.
		 */
		addr = (uchar_t *)((uintptr_t)orsp->rel_roffset +
		    (uintptr_t)_elf_getxoff(orsp->rel_isdesc->is_indata));
		addr += (uintptr_t)orsp->rel_osdesc->os_outdata->d_buf;
		if (ld_reloc_targval_set(ofl, orsp, addr, addend) == 0)
			return (S_ERROR);
	}

	relbits = (char *)relosp->os_outdata->d_buf;

	rea.r_info = ELF_R_INFO(ndx, orsp->rel_rtype);
	rea.r_offset = roffset;
	DBG_CALL(Dbg_reloc_out(ofl, ELF_DBG_LD, SHT_REL, &rea, relosp->os_name,
	    orsp->rel_sname));

	/*
	 * Assert we haven't walked off the end of our relocation table.
	 */
	assert(relosp->os_szoutrels <= relosp->os_shdr->sh_size);

	(void) memcpy((relbits + relosp->os_szoutrels),
	    (char *)&rea, sizeof (Rel));
	relosp->os_szoutrels += sizeof (Rel);

	/*
	 * Determine if this relocation is against a non-writable, allocatable
	 * section.  If so we may need to provide a text relocation diagnostic.
	 * Note that relocations against the .plt (R_ARM_JMP_SLOT) actually
	 * result in modifications to the .got.
	 */
	if (orsp->rel_rtype == R_ARM_JUMP_SLOT)
		osp = ofl->ofl_osgot;

	ld_reloc_remain_entry(orsp, osp, ofl);
	return (1);
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
			 * model (if required) is at this point.
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
			 * If this is a relocation against a move table, or
			 * expanded move table, adjust the relocation entries.
			 */
			if (arsp->rel_move)
				ld_adj_movereloc(ofl, arsp);

			sdp = arsp->rel_sym;
			refaddr = arsp->rel_roffset +
			    (Off)_elf_getxoff(arsp->rel_isdesc->is_indata);

			if (arsp->rel_flags & FLG_REL_CLVAL)
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
						value += sdp->sd_isc->
						    is_osdesc->os_shdr->sh_addr;
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
				    ofl, 0);
				assert(gnp);

				if (arsp->rel_rtype == R_ARM_TLS_DTPOFF32)
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
				    ELF_DBG_LD, M_MACH, SHT_REL,
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

			} else if (IS_GOT_PC(arsp->rel_rtype) &&
			    ((flags & FLG_OF_RELOBJ) == 0)) {
				value =
				    (Xword)(ofl->ofl_osgot->os_shdr->sh_addr) -
				    refaddr;

			} else if ((IS_PC_RELATIVE(arsp->rel_rtype)) &&
			    (((flags & FLG_OF_RELOBJ) == 0) ||
			    (arsp->rel_osdesc == sdp->sd_isc->is_osdesc))) {
				value -= refaddr;

			} else if (IS_TLS_INS(arsp->rel_rtype) &&
			    IS_GOT_RELATIVE(arsp->rel_rtype) &&
			    ((flags & FLG_OF_RELOBJ) == 0)) {
				Gotndx	*gnp;
				Shdr	*gshdr = ofl->ofl_osgot->os_shdr;
				Xword	gotbase, gotoff;

				/*
				 * Set TLS GOT entry offset from the reference
				 * address.
				 */
				gnp = ld_find_gotndx(&(sdp->sd_GOTndxs), gref,
				    ofl, 0);
				assert(gnp);
				gotbase = gshdr->sh_addr;
				gotoff = (Xword)gnp->gn_gotndx * M_GOT_ENTSIZE;
				value = gotbase + gotoff - refaddr;
			} else if (IS_GOT_RELATIVE(arsp->rel_rtype) &&
			    ((flags & FLG_OF_RELOBJ) == 0)) {
				Gotndx *gnp;

				gnp = ld_find_gotndx(&(sdp->sd_GOTndxs),
				    GOT_REF_GENERIC, ofl, 0);
				assert(gnp);
				value = (Xword)gnp->gn_gotndx * M_GOT_ENTSIZE;
			} else if (arsp->rel_rtype == R_ARM_TLS_LE32 &&
			    ((flags & FLG_OF_RELOBJ) == 0)) {
				/*
				 * TLS LE reference model.
				 * The size of thread control block must be
				 * added to hard-coded TLS offset.
				 */
				value += M_TCB_SIZE;
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
				Conv_inv_buf_t	inv_buf;

				eprintf(ofl->ofl_lml, ERR_FATAL,
				    MSG_INTL(MSG_REL_EMPTYSEC),
				    conv_reloc_ARM_type(arsp->rel_rtype,
				    0, &inv_buf),
				    ifl_name, demangle(arsp->rel_sname),
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
			    M_MACH, SHT_REL, arsp->rel_rtype, EC_NATPTR(addr),
			    value, arsp->rel_sname, arsp->rel_osdesc));
			addr += (uintptr_t)arsp->rel_osdesc->os_outdata->d_buf;

			if ((((uintptr_t)addr - (uintptr_t)ofl->ofl_nehdr) >
			    ofl->ofl_size) || (arsp->rel_roffset >
			    arsp->rel_osdesc->os_shdr->sh_size)) {
				Conv_inv_buf_t	inv_buf;
				int		class;

				if (((uintptr_t)addr -
				    (uintptr_t)ofl->ofl_nehdr) > ofl->ofl_size)
					class = ERR_FATAL;
				else
					class = ERR_WARNING;

				eprintf(ofl->ofl_lml, class,
				    MSG_INTL(MSG_REL_INVALOFFSET),
				    conv_reloc_ARM_type(arsp->rel_rtype,
				    0, &inv_buf),
				    ifl_name, arsp->rel_isdesc->is_name,
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
			 * If we have a replacement value for the relocation
			 * target, put it in place now.
			 */
			if (arsp->rel_flags & FLG_REL_NADDEND) {
				Xword addend = arsp->rel_raddend;

				if (ld_reloc_targval_set(ofl, arsp,
				    addr, addend) == 0)
					return (S_ERROR);
			}

			/*
			 * If '-z noreloc' is specified - skip the do_reloc_ld
			 * stage.
			 */
			if (OFL_DO_RELOC(ofl)) {
				if (do_reloc_ld((uchar_t)arsp->rel_rtype, addr,
				    &value, arsp->rel_sname, ifl_name,
				    OFL_SWAP_RELOC_DATA(ofl, arsp),
				    ofl->ofl_lml) == 0)
					return_code = S_ERROR;
			}
		}
	}
	return (return_code);
}

/*
 * Add an output relocation record.
 */
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
		ofl->ofl_relocgotsz += (Xword)sizeof (Rel);
	else if (flags & FLG_REL_PLT)
		ofl->ofl_relocpltsz += (Xword)sizeof (Rel);
	else if (flags & FLG_REL_BSS)
		ofl->ofl_relocbsssz += (Xword)sizeof (Rel);
	else if (flags & FLG_REL_NOINFO)
		ofl->ofl_relocrelsz += (Xword)sizeof (Rel);
	else
		orsp->rel_osdesc->os_szoutrels += (Xword)sizeof (Rel);

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
	DBG_CALL(Dbg_reloc_ors_entry(ofl->ofl_lml, ELF_DBG_LD, SHT_REL,
	    M_MACH, orsp));
	return (1);
}

/*
 * Stub routine since register symbols are not supported on ARM
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

	/*
	 * if ((shared object) and (not pc relative relocation) and
	 *    (not against ABS symbol))
	 * then
	 *	build R_ARM_RELATIVE
	 * fi
	 */
	if ((flags & FLG_OF_SHAROBJ) && (rsp->rel_flags & FLG_REL_LOAD) &&
	    !(IS_PC_RELATIVE(rsp->rel_rtype)) && !(IS_SIZE(rsp->rel_rtype)) &&
	    !(IS_GOT_BASED(rsp->rel_rtype)) &&
	    !(rsp->rel_isdesc != NULL &&
	    (rsp->rel_isdesc->is_shdr->sh_type == SHT_SUNW_dof)) &&
	    (((sdp->sd_flags & FLG_SY_SPECSEC) == 0) ||
	    (shndx != SHN_ABS) || (sdp->sd_aux && sdp->sd_aux->sa_symspec))) {
		Word	ortype = rsp->rel_rtype;

		rsp->rel_rtype = R_ARM_RELATIVE;
		if (ld_add_outrel(NULL, rsp, ofl) == S_ERROR)
			return (S_ERROR);
		rsp->rel_rtype = ortype;
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
		eprintf(ofl->ofl_lml, ERR_WARNING, MSG_INTL(MSG_REL_EXTERNSYM),
		    conv_reloc_ARM_type(rsp->rel_rtype, 0, &inv_buf),
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
	 * actually get here on ARM.
	 */
	assert(0);
	return (S_ERROR);
}

uintptr_t
ld_reloc_TLS(Boolean local, Rel_desc * rsp, Ofl_desc * ofl)
{
	Word		rtype = rsp->rel_rtype;
	Sym_desc	*sdp = rsp->rel_sym;
	Gotndx		*gnp;

	/*
	 * TLS reference model relaxation is not supported on ARM for lack of
	 * relocation information to change TLS reference model.
	 */

	if (IS_TLS_IE(rtype)) {
		ofl->ofl_dtflags |= DF_STATIC_TLS;

		/*
		 * Assign a GOT entry for static TLS references.
		 */
		if ((gnp = ld_find_gotndx(&(sdp->sd_GOTndxs), GOT_REF_TLSIE,
					  ofl, 0)) == 0) {
			if (ld_assign_got_TLS(local, rsp, ofl, sdp,
					      gnp, GOT_REF_TLSIE, FLG_REL_STLS,
					      rtype, R_ARM_TLS_TPOFF32, 0)
			    == S_ERROR) {
				return (S_ERROR);
			}
		}
		return (ld_add_actrel(FLG_REL_STLS, rsp, ofl));
	}
	if (IS_TLS_LE(rtype)) {
		ofl->ofl_dtflags |= DF_STATIC_TLS;
		return (ld_add_actrel(FLG_REL_STLS, rsp, ofl));
	}

	/*
	 * Assign a GOT entry for a dynamic TLS reference.
	 */
	if (IS_TLS_LD(rtype) && ((gnp = ld_find_gotndx(&(sdp->sd_GOTndxs),
	    GOT_REF_TLSLD, ofl, 0)) == 0)) {
		if (ld_assign_got_TLS(local, rsp, ofl, sdp, gnp, GOT_REF_TLSLD,
		    FLG_REL_MTLS, rtype, R_ARM_TLS_DTPMOD32, 0) == S_ERROR)
			return (S_ERROR);

	} else if (IS_TLS_GD(rtype) &&
	    ((gnp = ld_find_gotndx(&(sdp->sd_GOTndxs), GOT_REF_TLSGD,
	    ofl, 0)) == 0)) {
		if (ld_assign_got_TLS(local, rsp, ofl, sdp, gnp, GOT_REF_TLSGD,
		    FLG_REL_DTLS, rtype, R_ARM_TLS_DTPMOD32,
		    R_ARM_TLS_DTPOFF32) == S_ERROR)
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

	if ((gref == GOT_REF_TLSLD) && ofl->ofl_tlsldgotndx)
		return (ofl->ofl_tlsldgotndx);

	for (LIST_TRAVERSE(lst, lnp, gnp)) {
		if (gnp->gn_gotref == gref)
			return (gnp);
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

	gnp = ld_find_gotndx(&(sdp->sd_GOTndxs), gref, ofl, 0);
	assert(gnp);

	gotndx = (Xword)gnp->gn_gotndx;

	if ((rdesc->rel_flags & FLG_REL_DTLS) &&
	    (rdesc->rel_rtype == R_ARM_TLS_DTPOFF32))
		gotndx++;

	return ((Xword)(osp->os_shdr->sh_addr + (gotndx * M_GOT_ENTSIZE)));
}


/* ARGSUSED4 */
uintptr_t
ld_assign_got_ndx(List * lst, Gotndx * pgnp, Gotref gref, Ofl_desc * ofl,
    Rel_desc * rsp, Sym_desc * sdp)
{
	Gotndx	*gnp;
	uint_t	gotents;

	if (pgnp)
		return (1);

	if ((gref == GOT_REF_TLSGD) || (gref == GOT_REF_TLSLD))
		gotents = 2;
	else
		gotents = 1;

	if ((gnp = libld_calloc(sizeof (Gotndx), 1)) == 0)
		return (S_ERROR);
	gnp->gn_gotndx = ofl->ofl_gotcnt;
	gnp->gn_gotref = gref;

	ofl->ofl_gotcnt += gotents;

	if (gref == GOT_REF_TLSLD) {
		ofl->ofl_tlsldgotndx = gnp;
		return (1);
	}

	if (list_appendc(lst, (void *)gnp) == 0)
		return (S_ERROR);

	return (1);
}

void
ld_assign_plt_ndx(Sym_desc * sdp, Ofl_desc *ofl)
{
	sdp->sd_aux->sa_PLTndx = 1 + ofl->ofl_pltcnt++;
	sdp->sd_aux->sa_PLTGOTndx = ofl->ofl_gotcnt++;
	ofl->ofl_flags |= FLG_OF_BLDGOT;
}

/*
 * Initializes .got[0] with the _DYNAMIC symbol value.
 */
uintptr_t
ld_fillin_gotplt(Ofl_desc *ofl)
{
	Word	flags = ofl->ofl_flags;

	if (ofl->ofl_osgot) {
		Sym_desc	*sdp;

		if ((sdp = ld_sym_find(MSG_ORIG(MSG_SYM_DYNAMIC_U),
		    SYM_NOHASH, 0, ofl)) != NULL) {
			uchar_t	*genptr;

			genptr = ((uchar_t *)ofl->ofl_osgot->os_outdata->d_buf +
			    (M_GOT_XDYNAMIC * M_GOT_ENTSIZE));
			/* LINTED */
			*(Word *)genptr = (Word)sdp->sd_sym->st_value;
		}
	}

	/*
	 * Fill in the reserved slot in the procedure linkage table the first
	 * entry is:
	 *
	 */
	if ((flags & FLG_OF_DYNAMIC) && ofl->ofl_osplt) {
		uchar_t	*pltent;
		Addr	got = ofl->ofl_osgot->os_shdr->sh_addr;
		Addr	plt = ofl->ofl_osplt->os_shdr->sh_addr;

		pltent = (uchar_t *)ofl->ofl_osplt->os_outdata->d_buf;

		*(Word *)(uintptr_t)pltent = M_INST_PUSH_LR;
		pltent += M_PLT_INSSIZE;
		*(Word *)(uintptr_t)pltent = M_INST_LDR_LR_PC_4;
		pltent += M_PLT_INSSIZE;
		*(Word *)(uintptr_t)pltent = M_INST_ADD_LR_PC;
		pltent += M_PLT_INSSIZE;
		*(Word *)(uintptr_t)pltent = M_INST_LDR_PC_LR_PRE8;
		pltent += M_PLT_INSSIZE;

		/*
		 * Install &GOT[0] - (PLT + (M_PLT_INSSIZE * 4)) into
		 * PLT + 0x10. It will be loaded by M_INST_LDR_LR_PC_4.
		 */
		*(Word *)(uintptr_t)pltent =
			(Word)(got - (plt + (M_PLT_INSSIZE * 4)));

        }
	return (1);
}

#define	XLATE_GOTNDX(ndx, newndx, xlate, gref, gotcnt)	\
	do {						\
		Word	*__x = (xlate) + (ndx);		\
							\
		if (*__x == (Word)-1) {			\
			*__x = (gotcnt);		\
			(newndx) = (gotcnt);		\
			if ((gref) == GOT_REF_TLSGD ||	\
			    (gref) == GOT_REF_TLSLD) {	\
				(gotcnt) += 2;		\
			}				\
			else {				\
				(gotcnt)++;		\
			}				\
		}					\
		else {					\
			(newndx) = *__x;		\
		}					\
	} while (0)

static uintptr_t
reassign_gotndx(List *lst, List *assigned, Word *cntp, Word *xlate)
{
	Listnode	*lnp;
	Rel_cache	*rcp;
	Word		gotcnt = *cntp;

	for (LIST_TRAVERSE(lst, lnp, rcp)) {
		Rel_desc	*orsp;

		/*LINTED*/
		for (orsp = (Rel_desc *)(rcp + 1); orsp < rcp->rc_free;
		     orsp++) {
			Sym_desc	*sdp = orsp->rel_sym;
			Listnode	*glnp;
			Gotndx		*gnp;

			for (LIST_TRAVERSE(&(sdp->sd_GOTndxs), glnp, gnp)) {
				Word		ndx = gnp->gn_gotndx;
				Listnode	*alnp;
				Gotndx		*agnp;

				for (LIST_TRAVERSE(assigned, alnp, agnp)) {
					if (agnp == gnp) {
						break;
					}
				}
				if (alnp != NULL) {
					continue;
				}

				if (list_appendc(assigned, gnp) == 0) {
					return (S_ERROR);
				}

				/* LINTED */
				XLATE_GOTNDX(ndx, gnp->gn_gotndx, xlate,
					     gnp->gn_gotref, gotcnt);
			}
		}
	}

	*cntp = gotcnt;
	return (1);
}

/*
 * uintptr_t
 * ld_reassign_got_ndx(Ofl_desc *ofl)
 *	Reassign GOT indices.
 *	ARM architecture requires contiguous GOT entries for PLT from
 *	GOT[3]. So GOT indices must be reassigned here.
 */
uintptr_t
ld_reassign_got_ndx(Ofl_desc *ofl)
{
	Listnode	*lnp;
	Rel_cache	*rcp;
	Word		i, *xlate, gotcnt = M_GOT_XNumber;
	List		assigned;

	if (ofl->ofl_pltcnt == 0) {
		/* Nothing to do. */
		return (1);
	}

	assigned.head = NULL;
	assigned.tail = NULL;

	if ((xlate = libld_malloc(sizeof(Word) * ofl->ofl_gotcnt)) == NULL) {
		return (S_ERROR);
	}
	for (i = 0; i < ofl->ofl_gotcnt; i++) {
		*(xlate + i) = (Word)-1;
	}

	/* At first, reassign PLTGOT indices. */
	for (LIST_TRAVERSE(&ofl->ofl_outrels, lnp, rcp)) {
		Rel_desc	*orsp;

		/*LINTED*/
		for (orsp = (Rel_desc *)(rcp + 1); orsp < rcp->rc_free;
		     orsp++) {
			if (orsp->rel_flags & FLG_REL_PLT) {
				Sym_desc	*sdp = orsp->rel_sym;
				Sym_aux		*aux = sdp->sd_aux;
				Word		ndx = aux->sa_PLTGOTndx;

				/* LINTED */
				XLATE_GOTNDX(ndx, aux->sa_PLTGOTndx, xlate,
					     GOT_REF_GENERIC, gotcnt);
			}
		}
	}

	/* Reassign rest of all GOT indices. */
	if (ofl->ofl_tlsldgotndx != NULL) {
		Gotndx	*gnp = ofl->ofl_tlsldgotndx;
		Word	ndx = gnp->gn_gotndx;

		assert(gnp->gn_gotref == GOT_REF_TLSLD);

		/* LINTED */
		XLATE_GOTNDX(ndx, gnp->gn_gotndx, xlate, GOT_REF_TLSLD,
			     gotcnt);
	}
	if (reassign_gotndx(&ofl->ofl_outrels, &assigned, &gotcnt, xlate)
	    == S_ERROR ||
	    reassign_gotndx(&ofl->ofl_actrels, &assigned, &gotcnt, xlate)
	    == S_ERROR) {
		return (S_ERROR);
	}

	assert(gotcnt == ofl->ofl_gotcnt);

	return (1);
}

/*
 * uintptr_t
 * ld_parse_force_plt(char *arg, Ofl_desc *ofl)
 *	Set "force PLT relocation" flag.
 */
uintptr_t
ld_parse_force_plt(char *arg, Ofl_desc *ofl)
{
	if (arg == NULL || strcmp(arg, MSG_ORIG(MSG_ARG_FPLT_FORCE)) == 0) {
		force_plt = FPLT_FORCE;
	}
	else if (strcmp(arg, MSG_ORIG(MSG_ARG_FPLT_WARN)) == 0) {
		force_plt = FPLT_WARN;
	}
	else if (strcmp(arg, MSG_ORIG(MSG_ARG_FPLT_NONE)) == 0) {
		force_plt = FPLT_NONE;
	}
	else {
		eprintf(ofl->ofl_lml, ERR_FATAL, MSG_INTL(MSG_ARG_ILLEGAL),
			MSG_ORIG(MSG_ARG_FORCEPLT_EQ), arg);
		return (S_ERROR);
	}

	return (1);
}

/*
 * uintptr_t
 * ld_mach_reloc_func(Ofl_desc *ofl, Rel_desc *rsp)
 *	Do ARM-specific relocation against function.
 *
 *	This function rewrite R_ARM_CALL relocation to R_ARM_PLT32
 *	if all of the following conditions are satisfied:
 *
 *	- We are now creating shared object.
 *	- force_plt is not FPLT_NONE, or the function is __aeabi_read_tp().
 *	  Rerecence to static TLS symbol is resolved by __aeabi_read_tp()
 *	  function call. gcc always uses R_ARM_CALL relocation type for it
 *	  even if PIC object is required. But we should use PLT relocation
 *	  type if we are creating shared object.
 *
 * Calling/Exit State:
 *	1 is returned if no more relocation is needed.
 *	0 is returned if relocation process must be proceeded.
 *	S_ERROR is returned on error.
 */
uintptr_t
ld_mach_reloc_func(Rel_desc *rsp, Ofl_desc *ofl)
{
	Word		rtype = rsp->rel_rtype;
	Sym_desc	*sdp = rsp->rel_sym;
	Word		flags = ofl->ofl_flags;
	uintptr_t	ret;

	if (sdp->sd_ref != REF_REL_NEED || !(flags & FLG_OF_SHAROBJ) ||
	    (flags & FLG_OF_BFLAG) || rtype != R_ARM_CALL) {
		return (0);
	}

	if (force_plt == FPLT_NONE &&
	    !(strcmp(sdp->sd_name, MSG_ORIG(MSG_SYM_AEABI_READ_TP_UU)) == 0 &&
	      sdp->sd_sym->st_shndx == SHN_UNDEF)) {
		return (0);
	}

	/* Rewrite R_ARM_CALL to R_ARM_PLT32. */
	if (force_plt == FPLT_WARN) {
		Ifl_desc	*ifl = rsp->rel_isdesc->is_file;
		Word		machine = ifl->ifl_ehdr->e_machine;
		Conv_inv_buf_t	inv_buf1, inv_buf2;

		eprintf(ofl->ofl_lml, ERR_WARNING, MSG_INTL(MSG_REL_FORCEPLT),
			ofl->ofl_name, ifl->ifl_name, sdp->sd_name,
			(uint32_t)rsp->rel_roffset,
			conv_reloc_type(machine, R_ARM_PLT32, 0, &inv_buf1),
			conv_reloc_type(machine, rtype, 0, &inv_buf2));
	}

	rsp->rel_rtype = R_ARM_PLT32;
	ret = ld_reloc_plt(rsp, ofl);
	rsp->rel_rtype = rtype;

	return (ret);
}

/*
 * static uintptr_t
 * lo_hash_insert(lo_hash_t *lhash, Is_desc *isp)
 *	Insert input section into linkonly hash table.
 */
static uintptr_t
lo_hash_insert(lo_hash_t *lhash, Is_desc *isp)
{
	lo_hashent_t	*lhp;
	size_t		size;
	uint_t		idx;

	if ((lhp = (lo_hashent_t *)libld_malloc(sizeof(*lhp))) == NULL) {
		return (S_ERROR);
	}

	size = strlen(isp->is_name) + 1;
	if ((lhp->lhe_name = (char *)libld_malloc(size)) == NULL) {
		return (S_ERROR);
	}
	(void)strlcpy(lhp->lhe_name, isp->is_name, size);
	lhp->lhe_isp = isp;

	idx = LO_HASHFUNC(lhp->lhe_name);
	lhp->lhe_next = lhash->lh_table[idx];
	lhash->lh_table[idx] = lhp;

	return (1);
}

/*
 * static Is_desc *
 * lo_hash_lookup(lo_hash_t *lhash, const char *name)
 *	Search hash table for .gnu.linkonce section.
 */
static Is_desc *
lo_hash_lookup(lo_hash_t *lhash, const char *name)
{
	lo_hashent_t	*lhp;
	Is_desc		*ret = NULL;
	uint_t		idx = LO_HASHFUNC(name);

	for (lhp = lhash->lh_table[idx]; lhp != NULL; lhp = lhp->lhe_next) {
		if (strcmp(name, lhp->lhe_name) == 0) {
			ret = lhp->lhe_isp;
			break;
		}
	}

	return (ret);
}

/*
 * uintptr_t
 * ld_mach_rename_section(Ofl_desc *ofl, Is_desc *isp)
 *	Rename section if the given section is .gnu.linkonce section.
 *
 * Remarks
 *	1 is returned if the section should not be discarded.
 *	0 is returned if the given section should be discarded.
 *	S_ERROR is returned on error.
 */
uintptr_t
ld_mach_rename_section(Ofl_desc *ofl, Is_desc *isp)
{
	char		*newname;
	const char	*isname, *type;
	size_t		isnamelen;
	lo_hash_t	*lhash;
	lo_name_t	*lnp;

	if (strncmp(isp->is_name, MSG_ORIG(MSG_LINKONCE_PREFIX),
		    MSG_LINKONCE_PREFIX_SIZE) != 0) {
		/* Not .gnu.linkonce section. */
		return (1);
	}

	if (ofl->ofl_flags & FLG_OF_RELOBJ) {
		/* No need to rename .gnu.linkonce section. */
		return (1);
	}

	/* Create lo_hash if not yet created. */
	if ((lhash = (lo_hash_t *)ofl->ofl_private) == NULL) {
		if ((lhash = (lo_hash_t *)libld_calloc(1, sizeof(*lhash)))
		    == NULL) {
			return (S_ERROR);
		}
	}

	/* Check whether this type of linkonce section is already linked. */
	if (lo_hash_lookup(lhash, isp->is_name) != NULL) {
		/* This section must be discarded. */
		isp->is_flags |= FLG_IS_DISCARD;
		return (0);
	}

	/* Determine input section type. */
	type = isp->is_name + MSG_LINKONCE_PREFIX_SIZE;
	isname = NULL;
	for (lnp = gnu_linkonce_names; lnp->ln_type != 0; lnp++) {
		if (strncmp(type, MSG_ORIG(lnp->ln_type), lnp->ln_typelen)
		    == 0) {
			isname = MSG_ORIG(lnp->ln_name);
			isnamelen = lnp->ln_namelen + 1;
			break;
		}
	}
	if (isname == NULL) {
		isname = isp->is_name;
		isnamelen = strlen(isname) + 1;
	}

	/* Insert this section into hash table, and rename it. */
	if ((newname = (char *)libld_malloc(isnamelen)) == NULL) {
		return (S_ERROR);
	}
	(void)strlcpy(newname, isname, isnamelen);

	if (lo_hash_insert(lhash, isp) == S_ERROR) {
		return (S_ERROR);
	}
	isp->is_name = newname;

	return (1);
}
