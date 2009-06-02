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

#ident	"@(#)cmd/sgs/libconv/common/relocate_arm.c"

/*
 * String conversion routine for relocation types.
 */
#include	<stdio.h>
#include	<sys/elf_ARM.h>
#include	"_conv.h"
#include	"relocate_arm_msg.h"

/*
 * ARM specific relocations.
 */
static const Msg rels[R_ARM_NUM] = {
	MSG_R_ARM_NONE,
	MSG_R_ARM_PC24,
	MSG_R_ARM_ABS32,
	MSG_R_ARM_REL32,
	MSG_R_PC_G0,
	MSG_R_ARM_ABS16,
	MSG_R_ARM_ABS12,
	MSG_R_ARM_THM_ABS5,
	MSG_R_ARM_ABS8,
	MSG_R_ARM_SBREL32,
	MSG_R_ARM_THM_CALL,
	MSG_R_ARM_THM_PC8,
	MSG_R_ARM_BREL_ADJ,
	MSG_R_ARM_SWI24,
	MSG_R_ARM_THM_SWI8,
	MSG_R_ARM_XPC25,
	MSG_R_ARM_THM_XPC22,
	MSG_R_ARM_TLS_DTPMOD32,
	MSG_R_ARM_TLS_DTPOFF32,
	MSG_R_ARM_TLS_TPOFF32,
	MSG_R_ARM_COPY,
	MSG_R_ARM_GLOB_DAT,
	MSG_R_ARM_JUMP_SLOT,
	MSG_R_ARM_RELATIVE,
	MSG_R_ARM_GOTOFF32,
	MSG_R_ARM_BASE_PREL,
	MSG_R_ARM_GOT_BREL,
	MSG_R_ARM_PLT32,
	MSG_R_ARM_CALL,
	MSG_R_ARM_JUMP24,
	MSG_R_ARM_THM_JUMP24,
	MSG_R_ARM_BASE_ABS,
	MSG_R_ARM_ALU_PCREL_7_0,
	MSG_R_ARM_ALU_PCREL_15_8,
	MSG_R_ARM_ALU_PCREL_23_15,
	MSG_R_SBREL_11_0_NC,
	MSG_R_ARM_ALU_SBREL_19_12_NC,
	MSG_R_ARM_ALU_SBREL_27_20_CK,
	MSG_R_ARM_TARGET1,
	MSG_R_ARM_SBREL31,
	MSG_R_ARM_V4BX,
	MSG_R_ARM_TARGET2,
	MSG_R_ARM_PREL31,
	MSG_R_ARM_MOVW_ABS_NC,
	MSG_R_ARM_MOVT_ABS,
	MSG_R_ARM_MOVW_PREL_NC,
	MSG_R_ARM_MOVT_PREL,
	MSG_R_ARM_THM_MOVW_ABS_NC,
	MSG_R_ARM_THM_MOVT_ABS,
	MSG_R_ARM_THM_MOVW_PREL_NC,
	MSG_R_ARM_THM_MOVT_PREL,
	MSG_R_ARM_THM_JUMP19,
	MSG_R_ARM_THM_JUMP6,
	MSG_R_ARM_THM_ALU_PREL_11_0,
	MSG_R_ARM_THM_PC12,
	MSG_R_ARM_ABS32_NOI,
	MSG_R_ARM_REL32_NOI,
	MSG_R_ARM_ALU_PC_G0_NC,
	MSG_R_ARM_ALU_PC_G0,
	MSG_R_ARM_ALU_PC_G1_NC,
	MSG_R_ARM_ALU_PC_G1,
	MSG_R_ARM_ALU_PC_G2,
	MSG_R_PC_G1,
	MSG_R_PC_G2,
	MSG_R_ARM_LDRS_PC_G0,
	MSG_R_ARM_LDRS_PC_G1,
	MSG_R_ARM_LDRS_PC_G2,
	MSG_R_ARM_LDC_PC_G0,
	MSG_R_ARM_LDC_PC_G1,
	MSG_R_ARM_LDC_PC_G2,
	MSG_R_ARM_ALU_SB_G0_NC,
	MSG_R_ARM_ALU_SB_G0,
	MSG_R_ARM_ALU_SB_G1_NC,
	MSG_R_ARM_ALU_SB_G1,
	MSG_R_ARM_ALU_SB_G2,
	MSG_R_SB_G0,
	MSG_R_SB_G1,
	MSG_R_SB_G2,
	MSG_R_ARM_LDRS_SB_G0,
	MSG_R_ARM_LDRS_SB_G1,
	MSG_R_ARM_LDRS_SB_G2,
	MSG_R_ARM_LDC_SB_G0,
	MSG_R_ARM_LDC_SB_G1,
	MSG_R_ARM_LDC_SB_G2,
	MSG_R_ARM_MOVW_BREL_NC,
	MSG_R_ARM_MOVT_BREL,
	MSG_R_ARM_MOVW_BREL,
	MSG_R_ARM_THM_MOVW_BREL_NC,
	MSG_R_ARM_THM_MOVT_BREL,
	MSG_R_ARM_THM_MOVW_BREL,
	MSG_R_ARM_TLS_GOTDESC,
	MSG_R_ARM_TLS_CALL,
	MSG_R_ARM_TLS_DESCSEQ,
	MSG_R_ARM_THM_TLS_CALL,
	MSG_R_ARM_PLT32_ABS,
	MSG_R_ARM_GOT_ABS,
	MSG_R_ARM_GOT_PREL,
	MSG_R_ARM_GOT_BREL12,
	MSG_R_ARM_GOTOFF12,
	MSG_R_ARM_GOTRELAX,
	MSG_R_ARM_GNU_VTENTRY,
	MSG_R_ARM_GNU_VTINHERIT,
	MSG_R_ARM_THM_JUMP11,
	MSG_R_ARM_THM_JUMP8,
	MSG_R_ARM_TLS_GD32,
	MSG_R_ARM_TLS_LDM32,
	MSG_R_ARM_TLS_LDO32,
	MSG_R_ARM_TLS_IE32,
	MSG_R_ARM_TLS_LE32,
	MSG_R_ARM_TLS_LDO12,
	MSG_R_ARM_TLS_LE12,
	MSG_R_ARM_TLS_IE12GP,
	MSG_R_ARM_PRIVATE_0,
	MSG_R_ARM_PRIVATE_1,
	MSG_R_ARM_PRIVATE_2,
	MSG_R_ARM_PRIVATE_3,
	MSG_R_ARM_PRIVATE_4,
	MSG_R_ARM_PRIVATE_5,
	MSG_R_ARM_PRIVATE_6,
	MSG_R_ARM_PRIVATE_7,
	MSG_R_ARM_PRIVATE_8,
	MSG_R_ARM_PRIVATE_9,
	MSG_R_ARM_PRIVATE_10,
	MSG_R_ARM_PRIVATE_11,
	MSG_R_ARM_PRIVATE_12,
	MSG_R_ARM_PRIVATE_13,
	MSG_R_ARM_PRIVATE_14,
	MSG_R_ARM_PRIVATE_15,
	MSG_R_ARM_ME_TOO
};

#if	R_ARM_NUM != R_ARM_ME_TOO + 1
#error	"R_ARM_NUM has grown"
#endif	/* R_ARM_NUM != R_ARM_ME_TOO + 1 */

/*
 * const char *
 * conv_reloc_ARM_type(Word type, Conv_fmt_flags_t fmt_flags,
 *		       Conv_inv_buf_t *inv_buf)
 *	Return string representation of the specified relocation type.
 */
const char *
conv_reloc_ARM_type(Word type, Conv_fmt_flags_t fmt_flags,
		    Conv_inv_buf_t *inv_buf)
{
	if (type >= R_ARM_NUM) {
		return conv_invalid_val(inv_buf, type, fmt_flags);
	}

	return MSG_ORIG(rels[type]);
}
