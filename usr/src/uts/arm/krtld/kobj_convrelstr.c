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
 * Copyright (c) 2006-2008 NEC Corporation
 */

#ident	"@(#)arm/krtld/kobj_convrelstr.c"

#include	<sys/types.h>
#include	"reloc.h"

static const char	*rels[R_ARM_NUM] = {
	"R_ARM_NONE",
	"R_ARM_PC24",
	"R_ARM_ABS32",
	"R_ARM_REL32",
	"R_ARM_LDR_PC_G0",
	"R_ARM_ABS16",
	"R_ARM_ABS12",
	"R_ARM_THM_ABS5",
	"R_ARM_ABS8",
	"R_ARM_SBREL32",
	"R_ARM_THM_CALL",
	"R_ARM_THM_PC8",
	"R_ARM_BREL_ADJ",
	"R_ARM_SWI24",
	"R_ARM_THM_SWI8",
	"R_ARM_XPC25",
	"R_ARM_THM_XPC22",
	"R_ARM_TLS_DTPMOD32",
	"R_ARM_TLS_DTPOFF32",
	"R_ARM_TLS_TPOFF32",
	"R_ARM_COPY",
	"R_ARM_GLOB_DAT",
	"R_ARM_JUMP_SLOT",
	"R_ARM_RELATIVE",
	"R_ARM_GOTOFF32",
	"R_ARM_BASE_PREL",
	"R_ARM_GOT_BREL",
	"R_ARM_PLT32",
	"R_ARM_CALL",
	"R_ARM_JUMP24",
	"R_ARM_THM_JUMP24",
	"R_ARM_BASE_ABS",
	"R_ARM_ALU_PCREL_7_0",
	"R_ARM_ALU_PCREL_15_8",
	"R_ARM_ALU_PCREL_23_15",
	"R_ARM_LDR_SBREL_11_0_NC",
	"R_ARM_ALU_SBREL_19_12_NC",
	"R_ARM_ALU_SBREL_27_20_CK",
	"R_ARM_TARGET1",
	"R_ARM_SBREL31",
	"R_ARM_V4BX",
	"R_ARM_TARGET2",
	"R_ARM_PREL31",
	"R_ARM_MOVW_ABS_NC",
	"R_ARM_MOVT_ABS",
	"R_ARM_MOVW_PREL_NC",
	"R_ARM_MOVT_PREL",
	"R_ARM_THM_MOVW_ABS_NC",
	"R_ARM_THM_MOVT_ABS",
	"R_ARM_THM_MOVW_PREL_NC",
	"R_ARM_THM_MOVT_PREL",
	"R_ARM_THM_JUMP19",
	"R_ARM_THM_JUMP6",
	"R_ARM_THM_ALU_PREL_11_0",
	"R_ARM_THM_PC12",
	"R_ARM_ABS32_NOI",
	"R_ARM_REL32_NOI",
	"R_ARM_ALU_PC_G0_NC",
	"R_ARM_ALU_PC_G0",
	"R_ARM_ALU_PC_G1_NC",
	"R_ARM_ALU_PC_G1",
	"R_ARM_ALU_PC_G2",
	"R_ARM_LDR_PC_G1",
	"R_ARM_LDR_PC_G2",
	"R_ARM_LDRS_PC_G0",
	"R_ARM_LDRS_PC_G1",
	"R_ARM_LDRS_PC_G2",
	"R_ARM_LDC_PC_G0",
	"R_ARM_LDC_PC_G1",
	"R_ARM_LDC_PC_G2",
	"R_ARM_ALU_SB_G0_NC",
	"R_ARM_ALU_SB_G0",
	"R_ARM_ALU_SB_G1_NC",
	"R_ARM_ALU_SB_G1",
	"R_ARM_ALU_SB_G2",
	"R_ARM_LDR_SB_G0",
	"R_ARM_LDR_SB_G1",
	"R_ARM_LDR_SB_G2",
	"R_ARM_LDRS_SB_G0",
	"R_ARM_LDRS_SB_G1",
	"R_ARM_LDRS_SB_G2",
	"R_ARM_LDC_SB_G0",
	"R_ARM_LDC_SB_G1",
	"R_ARM_LDC_SB_G2",
	"R_ARM_MOVW_BREL_NC",
	"R_ARM_MOVT_BREL",
	"R_ARM_MOVW_BREL",
	"R_ARM_THM_MOVW_BREL_NC",
	"R_ARM_THM_MOVT_BREL",
	"R_ARM_THM_MOVW_BREL",
	"R_ARM_TLS_GOTDESC",
	"R_ARM_TLS_CALL",
	"R_ARM_TLS_DESCSEQ",
	"R_ARM_THM_TLS_CALL",
	"R_ARM_PLT32_ABS",
	"R_ARM_GOT_ABS",
	"R_ARM_GOT_PREL",
	"R_ARM_GOT_BREL12",
	"R_ARM_GOTOFF12",
	"R_ARM_GOTRELAX",
	"R_ARM_GNU_VTENTRY",
	"R_ARM_GNU_VTINHERIT",
	"R_ARM_THM_JUMP11",
	"R_ARM_THM_JUMP8",
	"R_ARM_TLS_GD32",
	"R_ARM_TLS_LDM32",
	"R_ARM_TLS_LDO32",
	"R_ARM_TLS_IE32",
	"R_ARM_TLS_LE32",
	"R_ARM_TLS_LDO12",
	"R_ARM_TLS_LE12",
	"R_ARM_TLS_IE12GP",
	"R_ARM_PRIVATE_0",
	"R_ARM_PRIVATE_1",
	"R_ARM_PRIVATE_2",
	"R_ARM_PRIVATE_3",
	"R_ARM_PRIVATE_4",
	"R_ARM_PRIVATE_5",
	"R_ARM_PRIVATE_6",
	"R_ARM_PRIVATE_7",
	"R_ARM_PRIVATE_8",
	"R_ARM_PRIVATE_9",
	"R_ARM_PRIVATE_10",
	"R_ARM_PRIVATE_11",
	"R_ARM_PRIVATE_12",
	"R_ARM_PRIVATE_13",
	"R_ARM_PRIVATE_14",
	"R_ARM_PRIVATE_15",
	"R_ARM_ME_TOO",
};

/*
 * This is a 'stub' of the orignal version defined in liblddbg.so.  This stub
 * returns the 'int string' of the relocation in question instead of converting
 * the relocation to it's full syntax.
 */
const char *
conv_reloc_ARM_type(Word type)
{
	static char 	strbuf[32];
	int		ndx = 31;

	if (type < R_ARM_NUM) {
		return rels[type];
	}

	strbuf[ndx--] = '\0';
	do {
		strbuf[ndx--] = '0' + (type % 10);
		type = type / 10;
	} while ((ndx >= (int)0) && (type > (Word)0));

	return &strbuf[ndx + 1];
}
