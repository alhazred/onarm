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
 * Copyright (c) 2007-2008 NEC Corporation
 * All rights reserved.
 */

#ident	"@(#)common/dis/arm/dis_trace.c"

/*
 * Disassembly routines for a stack backtrace.
 */

#include <sys/types.h>
#include "dis.h"

#define	IS_SPREG(regnum)	((regnum) == 13)
#define	IS_LRREG(regnum)	((regnum) == 14)

/* definitions for LDC/STC */
#define	INSTR_LDC_P	0x01000000
#define	INSTR_LDC_U	0x00800000
#define	INSTR_LDC_N	0x00400000
#define	INSTR_LDC_W	0x00200000
#define	INSTR_LDC_L	0x00100000

/* definitions for LDM/STM */
#define	INSTR_LDM_P	0x01000000
#define	INSTR_LDM_U	0x00800000
#define	INSTR_LDM_W	0x00200000
#define	INSTR_LDM_L	0x00100000
#define	INSTR_LDM_IA	INSTR_LDM_U
#define	INSTR_LDM_IB	(INSTR_LDM_P|INSTR_LDM_U)
#define	INSTR_LDM_DA	0
#define	INSTR_LDM_DB	INSTR_LDM_P
#define	IS_STM(instr)	(((instr) & INSTR_LDM_L) == 0)

/* definitions for LDR/STR */
#define	INSTR_LDR_P	0x01000000
#define	INSTR_LDR_U	0x00800000
#define	INSTR_LDR_W	0x00200000
#define	INSTR_LDR_L	0x00100000
#define	IS_STR(instr)	(((instr) & INSTR_LDR_L) == 0)

/* definitions for data processing instructions */
#define	INSTR_DP_S	0x00100000

/*
 * Decode LDC/STC instructions (INSTR_TYPE == 6).
 */
static void
decode_ldcstc(uint32_t inst, int *spsize, int *lroff)
{
	uint_t	immed;

	if (!IS_SPREG(INSTR_RN(inst))) {
		/* sp is not changed */
		return;
	}
	if ((inst & INSTR_LDC_W) == 0) {
		/* base register is not changed */
		return;
	}

	immed = (inst & 0xff) * 4;
	if (inst & INSTR_LDC_U) {
		immed *= -1;
	}

	*spsize += immed;
	if (*lroff != -1) {
		*lroff += immed;
	}
}

/*
 * Decode LDM/STM instructions (INSTR_TYPE == 4).
 */
static void
decode_ldmstm(uint32_t inst, int *spsize, int *lroff)
{
	int		i;
	int		regnum;
	int		lridx = -1;
	uint32_t	tinst;

	if (!IS_SPREG(INSTR_RN(inst))) {
		/* sp is not changed */
		/* Assumption: lr is saved to sp relative address */
		return;
	}

	tinst = inst;
	regnum = 0;
	for (i = 0; i < 16; i++) {
		if (tinst & 0x1) {
			if (IS_LRREG(i)) {
				lridx = regnum;
			}
			regnum++;
		}
		tinst >>= 1;
	}
	if (regnum == 0) {
		return;
	}

	switch (inst & (INSTR_LDM_P|INSTR_LDM_U)) {
	case INSTR_LDM_IA:
		if (inst & INSTR_LDM_W) {
			*spsize -= (regnum * 4);
			if (*lroff != -1) {
				*lroff -= (regnum * 4);
			}
		}
		if (IS_STM(inst) && lridx != -1) {
			*lroff = (lridx * 4) - (regnum * 4);
		}
		break;
	case INSTR_LDM_IB:
		if (inst & INSTR_LDM_W) {
			*spsize -= (regnum * 4);
			if (*lroff != -1) {
				*lroff -= (regnum * 4);
			}
		}
		if (IS_STM(inst) && lridx != -1) {
			*lroff = (lridx * 4) + 4 - (regnum * 4);
		}
		break;
	case INSTR_LDM_DA:
		if (inst & INSTR_LDM_W) {
			*spsize += (regnum * 4);
			if (*lroff != -1) {
				*lroff += (regnum * 4);
			}
		}
		if (IS_STM(inst) && lridx != -1) {
			*lroff = (lridx * 4) + 4;
		}
		break;
	case INSTR_LDM_DB:
		if (inst & INSTR_LDM_W) {
			*spsize += (regnum * 4);
			if (*lroff != -1) {
				*lroff += (regnum * 4);
			}
		}
		if (IS_STM(inst) && lridx != -1) {
			*lroff = (lridx * 4);
		}
		break;
	}

	return;
}

/*
 * Decode LDR/STR instructions (INSTR_TYPE == 2,3).
 */
static void
decode_ldrstr(uint32_t inst, int *spsize, int *lroff)
{
	uint_t	immed;

	if (!IS_SPREG(INSTR_RN(inst))) {
		/* sp is not changed */
		/* Assumption: lr is saved to sp relative address */
		return;
	}
	if (INSTR_TYPE(inst) == 3) {
		/* Assumption: sp is changed by immediate offset instruction */
		/* Assumption: lr is saved to sp relative address */
		/*        not support "ldr/str r?, [sp, r?, xxx]" */
		return;
	}

	immed = inst & 0xfff;
	if (inst & INSTR_LDR_U) {
		immed *= -1;
	}

	if (inst & INSTR_LDR_P) {
		if (inst & INSTR_LDR_W) {
			*spsize += immed;
			if (*lroff != -1) {
				*lroff += immed;
			}
			if (IS_STR(inst) && IS_LRREG(INSTR_RD(inst))) {
				*lroff = 0;
			}
		} else {
			if (IS_STR(inst) && IS_LRREG(INSTR_RD(inst))) {
				*lroff = immed;
			}
		}
	} else {
		*spsize += immed;
		if (*lroff != -1) {
			*lroff += immed;
		}
		if (IS_STR(inst) && IS_LRREG(INSTR_RD(inst))) {
			*lroff = immed;
		}
	}
}

/*
 * Addressing mode 1 - 32-bit immediate
 */
static uint_t
addrmode1_immediate32bit(uint32_t inst)
{
	uint_t	rotate_imm, immed_8, immed;

	rotate_imm = ((inst >> 8) & 0xf) * 2;
	immed_8 = inst & 0xff;

	if (rotate_imm == 0) {
		immed = immed_8;
	} else if (rotate_imm >= 8) {
		immed = immed_8 << (32 - rotate_imm);
	} else {
		immed = (immed_8 & ((2 ^ rotate_imm) - 1))
			<< (32 - rotate_imm);
		immed |= (immed_8 >> rotate_imm);
	}

	return immed;
}

/*
 * Decode data processing instructions (INSTR_TYPE == 0,1).
 */
static void
decode_dataprocessing(uint32_t inst, int *spsize, int *lroff)
{
	int	opcode = INSTR_OPCODE(inst);

	if (INSTR_TYPE(inst) == 0 && (inst & 0x90) == 0x90) {
		/* ldr{d,ex,h,sb,sh}, mla, mul, str{d,ex,h},
		   swp, swpb, umaal, umlal, umull */
		/* Assumption: mul and additional ld/st is not used for sp/lr.*/
		return;
	}
	if (opcode >= 8 && opcode <= 11) {
		if ((inst & INSTR_DP_S) == 0) {
			/* EMPTY */
			/* bkpt, bx, blx, bxj, clz, cps, mrs, msr,
			   qadd, qdadd, qdsub, qsub, setend,
			   smla, smlal, smlaw, smul, smulw, undef */
			/* Assumption: ext instr is not used for sp and lr. */
		}
		/* tst, teq, cmp, cmn */
		return;
	}
	if (!IS_SPREG(INSTR_RD(inst))) {
		/* sp is not changed */
		return;
	}

	/*
	 * SUB instruction
	 */
	if (opcode == 2) {
		uint_t	immed;
		if (!IS_SPREG(INSTR_RN(inst))) {
			/* Assumption: source register is only sp. */
			return;
		}
		if (INSTR_TYPE(inst) == 0) {
			/* Assumption: source register is only sp. */
			return;
		}
		immed = addrmode1_immediate32bit(inst);
		*spsize += immed;
		if (*lroff != -1) {
			*lroff += immed;
		}
		return;
	}

	/*
	 * ADD instruction
	 */
	if (opcode == 4) {
		uint_t	immed;
		if (!IS_SPREG(INSTR_RN(inst))) {
			/* Assumption: source register is only sp. */
			return;
		}
		if (INSTR_TYPE(inst) == 0) {
			/* Assumption: source register is only sp. */
			return;
		}
		immed = addrmode1_immediate32bit(inst);
		*spsize -= immed;
		if (*lroff != -1) {
			*lroff -= immed;
		}
		return;
	}

	/* not support */
	/* and, eor, rsb, adc, sbc, rsc, orr, mov, bic, mvn */
	return;
}

/*
 * void
 * dis_stacktrace(uint32_t inst, int *spsize, int *lroff)
 *	Disassemble ARM instruction for a stack backtrace.
 *	This function compute stack size and position of lr
 *	in a stack frame.
 *
 * Remarks:
 *	This function assumes that the stack expansion and the
 *	storing lr is done by sub/add, ldr/str, ldm/stm, ldc/stc instructions.
 *	If other instruction is used for this purpose,
 *	a stack backtrace will be failed.
 */
void
dis_stacktrace(uint32_t inst, int *spsize, int *lroff)
{
	if (INSTR_COND(inst) != 0xe) {
		/* Assumption: conditional instr is not used for sp and lr. */
		return;
	}

	switch (INSTR_TYPE(inst)) {
	case 0:
	case 1:
		decode_dataprocessing(inst, spsize, lroff);
		break;
	case 2:
	case 3:
		decode_ldrstr(inst, spsize, lroff);
		break;
	case 4:
		decode_ldmstm(inst, spsize, lroff);
		break;
	case 5:
		/* b, bl */
		break;
	case 6:
		/* ldc, stc */
		decode_ldcstc(inst, spsize, lroff);
		break;
	case 7:
		/* cdp, mrc, mcr, swi */
		/* Assumption: mrc/mcr is not used for sp and lr. */
		break;
	}
	return;
}
