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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2007-2008 NEC Corporation
 */

#ident	"@(#)arm/krtld/doreloc.c"

#if	defined(_KERNEL)
#include	<sys/types.h>
#include	"reloc.h"
#else
#include	<stdio.h>
#include	"sgs.h"
#include	"machdep.h"
#include	"libld.h"
#include	"reloc.h"
#include	"conv.h"
#include	"msg.h"
#endif

/* Defines range of value in signed_immed_24 field in b or bl instruction. */
#define	BRANCH_OFFSET_MIN	-33554432
#define	BRANCH_OFFSET_MAX	33554428

#define	BRANCH_MASK_PCOFF	0x00ffffff
#define	BRANCH_MASK_OP		(~BRANCH_MASK_PCOFF)

/* Encode b or bl instruction */
#define	BRANCH_ENCODE(instp, pcoff)					\
	do {								\
		Xword	__inst;						\
		int	__immed24;					\
									\
		__inst = *(instp) & BRANCH_MASK_OP;			\
		__immed24 = ((int)(pcoff) >> 2) & BRANCH_MASK_PCOFF;	\
		*(instp) = __inst | __immed24;				\
	} while (0)

/*
 * This table represents the current relocations that do_reloc() is able to
 * process.  The relocations below that are marked SPECIAL are relocations that
 * take special processing and shouldn't actually ever be passed to do_reloc().
 */
const Rel_entry	reloc_table[R_ARM_NUM] = {
	{FLG_RE_NOTREL, 0},		/* R_ARM_NONE */
	{FLG_RE_PCREL, 3},		/* R_ARM_PC24 */
	{FLG_RE_NOTREL, 4},		/* R_ARM_ABS32 */
	{FLG_RE_PCREL, 4},		/* R_ARM_REL32 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_LDR_PC_G0 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_ABS16 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_ABS12 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_THM_ABS5 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_ABS8 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_SBREL32 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_THM_CALL */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_THM_PC8 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_BREL_ADJ */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_SWI24 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_THM_SWI8 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_XPC25 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_THM_XPC22 */
	{FLG_RE_NOTREL, 4},		/* R_ARM_TLS_DTPMOD32 */
	{FLG_RE_NOTREL, 4},		/* R_ARM_TLS_DTPOFF32 */
	{FLG_RE_NOTREL, 4},		/* R_ARM_TLS_TPOFF32 */
	{FLG_RE_NOTREL, 0},		/* R_ARM_COPY */	/* SPECIAL */
	{FLG_RE_NOTREL, 4},		/* R_ARM_GLOB_DAT */
	{FLG_RE_NOTREL, 4},		/* R_ARM_JUMP_SLOT */
	{FLG_RE_NOTREL, 4},		/* R_ARM_RELATIVE */
	{FLG_RE_GOTREL, 4},		/* R_ARM_GOTOFF32 */
	{FLG_RE_GOTPC|FLG_RE_PCREL, 4},	/* R_ARM_BASE_PREL */
	{FLG_RE_GOTADD, 4},		/* R_ARM_GOT_BREL */
	{FLG_RE_PLTREL|FLG_RE_PCREL, 3},/* R_ARM_PLT32 */
	{FLG_RE_PCREL, 3},		/* R_ARM_CALL */
	{FLG_RE_PCREL, 3},		/* R_ARM_JUMP24 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_THM_JUMP24 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_BASE_ABS */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_ALU_PCREL_7_0 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_ALU_PCREL_15_8 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_ALU_PCREL_23_15 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_LDR_SBREL_11_0_NC */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_ALU_SBREL_19_12_NC */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_ALU_SBREL_27_20_CK */
	{FLG_RE_NOTREL, 4},		/* R_ARM_TARGET1 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_SBREL31 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_V4BX */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_TARGET2 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_PREL31 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_MOVW_ABS_NC */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_MOVT_ABS */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_MOVW_PREL_NC */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_MOVT_PREL */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_THM_MOVW_ABS_NC */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_THM_MOVT_ABS */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_THM_MOVW_PREL_NC */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_THM_MOVT_PREL */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_THM_JUMP19 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_THM_JUMP6 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_THM_ALU_PREL_11_0 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_THM_PC12 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_ABS32_NOI */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_REL32_NOI */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_ALU_PC_G0_NC */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_ALU_PC_G0 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_ALU_PC_G1_NC */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_ALU_PC_G1 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_ALU_PC_G2 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_LDR_PC_G1 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_LDR_PC_G2 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_LDRS_PC_G0 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_LDRS_PC_G1 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_LDRS_PC_G2 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_LDC_PC_G0 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_LDC_PC_G1 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_LDC_PC_G2 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_ALU_SB_G0_NC */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_ALU_SB_G0 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_ALU_SB_G1_NC */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_ALU_SB_G1 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_ALU_SB_G2 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_LDR_SB_G0 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_LDR_SB_G1 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_LDR_SB_G2 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_LDRS_SB_G0 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_LDRS_SB_G1 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_LDRS_SB_G2 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_LDC_SB_G0 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_LDC_SB_G1 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_LDC_SB_G2 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_MOVW_BREL_NC */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_MOVT_BREL */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_MOVW_BREL */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_THM_MOVW_BREL_NC */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_THM_MOVT_BREL */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_THM_MOVW_BREL */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_TLS_GOTDESC */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_TLS_CALL */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_TLS_DESCSEQ */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_THM_TLS_CALL */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_PLT32_ABS */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_GOT_ABS */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_GOT_PREL */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_GOT_BREL12 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_GOTOFF12 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_GOTRELAX */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_GNU_VTENTRY */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_GNU_VTINHERIT */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_THM_JUMP11 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_THM_JUMP8 */
	{FLG_RE_TLSGD|FLG_RE_GOTADD, 4},/* R_ARM_TLS_GD32 */
	{FLG_RE_TLSLD|FLG_RE_GOTADD, 4},/* R_ARM_TLS_LDM32 */
	{FLG_RE_TLSLD, 4},		/* R_ARM_TLS_LDO32 */
	{FLG_RE_TLSIE|FLG_RE_GOTADD, 4},/* R_ARM_TLS_IE32 */
	{FLG_RE_TLSLE, 4},		/* R_ARM_TLS_LE32 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_TLS_LDO12 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_TLS_LE12 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_TLS_IE12GP */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_PRIVATE_0 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_PRIVATE_1 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_PRIVATE_2 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_PRIVATE_3 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_PRIVATE_4 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_PRIVATE_5 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_PRIVATE_6 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_PRIVATE_7 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_PRIVATE_8 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_PRIVATE_9 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_PRIVATE_10 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_PRIVATE_11 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_PRIVATE_12 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_PRIVATE_13 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_PRIVATE_14 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_PRIVATE_15 */
	{FLG_RE_NOTSUP, 0},		/* R_ARM_ME_TOO */
};

#ifdef	_KERNEL

/* The kernel has no GOT so we can ignore GOT reference. */
#define	IS_PCREL_ADJ_NEEDED(rtype)	IS_PC_RELATIVE(rtype)

#define	ARM_PC_ACCESS_OFFSET	8

#else	/* !_KERNEL */

/*
 * Although GOT relative reference is also PC relative, we must NOT adjust
 * PC access offset because it should be done by the code itself.
 */
#define	IS_PCREL_ADJ_NEEDED(rtype)			\
	(IS_PC_RELATIVE(rtype) && !IS_GOT_PC(rtype))

#define	ARM_PC_ACCESS_OFFSET	M_PC_ACCESS_OFFSET

#endif	/* _KERNEL */

/*
 * Write a single relocated value to its reference location.
 * We assume we wish to add the relocation amount, value, to the
 * value of the address already present at the offset.
 *
 * Currently, only the following relocation types are supported.
 *
 * NAME			VALUE	FIELD		CALCULATION
 *
 * R_ARM_NONE		0	none		none
 * R_ARM_PC24		1	signed_immed24	((S + A) | T) - P
 * R_ARM_ABS32		2	word32		(S + A) | T
 * R_ARM_REL32		3	word32		((S + A) | T) - P
 * R_ARM_TLS_DTPMOD32	17	word32		Module[S]
 * R_ARM_TLS_DTPOFF32	18	word32		S + A - TLS
 * R_ARM_TLS_TPOFF32	19	word32		S + A - tp
 * R_ARM_COPY		20	none		none
 * R_ARM_GLOB_DAT	21	word32		(S + A) | T
 * R_ARM_JUMP_SLOT	22	word32		(S + A) | T
 * R_ARM_RELATIVE	23	word32		B(S) + A
 * R_ARM_GOTOFF32	24	word32		((S + A) | T) - GOT_ORG
 * R_ARM_BASE_PREL	25	word32		B(S) + A - P
 * R_ARM_GOT_BREL	26	word32		GOT(S) + A - GOT_ORG
 * R_ARM_PLT32		27	signed_immed24	((S + A) | T) - P
 * R_ARM_CALL		28	signed_immed24	((S + A) | T) - P
 * R_ARM_JUMP24		29	signed_immed24	((S + A) | T) - P
 * R_ARM_TARGET1	38	word32		(S + A) | T
 *
 * See "ELF for the ARM Architecture" for more details.
 *
 * Relocation calculations:
 *
 * CALCULATION uses the following notation:
 *	S	the value of the symbol
 *	A	the addend for the relocation
 *	P	the address of the place being relocated
 *	T	1 if the target symbol S has type STT_FUNC and the symbol
 *		addresses a Thumb instruction. Otherwise 0.
 *	B(S)	the addressing origin of the output segment defining the
 *		symbol S. The origin is not required to be the base address of
 *		the segment. This value must always be word-aligned.
 *	Module[S]
 *		Resolves to the module number of the module defining the
 *		specified TLS symbol.
 *	GOT_ORG	the addressing origin of the Global Offset Table
 *	GOT(S)	the address of the GOT entry for the symbol S
 *
 * The calculations in the CALCULATION column are assumed to have
 * been performed before calling this function except for the addition of
 * the addresses in the instructions.
 *
 * Remarks:
 *	- Thumb code is not supported.
 */
/* ARGSUSED5 */
#if	defined(_KERNEL)
#define	lml	0		/* Needed by arglist of REL_ERR_* macros */
int
do_reloc_krtld(uchar_t rtype, uchar_t *off, Xword *value, const char *sym,
	       const char *file)
#elif	defined(DO_RELOC_LIBLD)
/*ARGSUSED5*/
int
do_reloc_ld(uchar_t rtype, uchar_t *off, Xword *value, const char *sym,
	    const char *file, int bswap, void *lml)
#else	/* !_KERNEL && !DO_RELOC_LIBLD */
int
do_reloc_rtld(uchar_t rtype, uchar_t *off, Xword *value, const char *sym,
	      const char *file, void *lml)
#endif	/* _KERNEL */
{
	const Rel_entry	*rep;
	int	boff;

#ifdef	DO_RELOC_LIBLD
	/*
	 * We do not support building the ARM linker as a cross linker
	 * at this time.
	 */
	if (bswap) {
		REL_ERR_NOSWAP(lml, file, sym, rtype);
		return (0);
	}
#endif	/* DO_RELOC_LIBLD */

	if (IS_PCREL_ADJ_NEEDED(rtype)) {
		/*
		 * On ARM architecture, pc points 2 instructions beyond
		 * the current program counter.
		 */
		*value -= ARM_PC_ACCESS_OFFSET;
	}

	rep = &reloc_table[rtype];

	switch (rep->re_fsize) {
	case 1:
		/* LINTED */
		*((uchar_t *)off) += (uchar_t)(*value);
		break;

	case 2:
		/* LINTED */
		*((Half *)off) += (Half)(*value);
		break;

	case 3:
		/* PC relative branch. */
		boff = (int)*value;
		if (boff < BRANCH_OFFSET_MIN || boff > BRANCH_OFFSET_MAX) {
			/*
			 * NOTE:
			 * 	We should use more suitable message.
			 *	Note that you must update message catalog for
			 *	userland runtime linker as well when you
			 *	want to change this error message.
			 */
			REL_ERR_NOFIT(lml, file, sym, rtype, boff);
			return 0;
		}
		/* LINTED */
		BRANCH_ENCODE((Xword *)off, boff);
		break;

	case 4:
		/* LINTED */
		*((Xword *)off) += *value;
		break;

	default:
		/*
		 * To keep chkmsg() happy: MSG_INTL(MSG_REL_UNSUPSZ)
		 */
		REL_ERR_UNSUPSZ(lml, file, sym, rtype, rep->re_fsize);
		return 0;
	}

	return 1;
}
