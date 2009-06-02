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

#ident	"@(#)common/dis/arm/dis_string.c"

/*
 * Disassembler for ARM instructions.
 *
 * Remarks:
 *	Do NOT use instr_t defined in procfs_isa.h in this file or
 *	cross compilation environment will not work correctly.
 */

#include "dis_string.h"

#ifdef	_KERNEL

#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/systm.h>

#define	DIS_ASSERT(ex)	ASSERT(ex)

#else	/* !_KERNEL */

#include <string.h>
#include <errno.h>

#ifdef	DIS_DEBUG
#include <assert.h>
#define	DIS_ASSERT(ex)	assert(ex)
#else	/* !DIS_DEBUG */
#define	DIS_ASSERT(ex)	((void)(0))
#endif	/* DIS_DEBUG */

#endif	/* _KERNEL */

/* Column width for instruction mnemonic. */
#define	DIS_INST_WIDTH		10
#define	DIS_INST_MINPAD		1

/* Determine comment format. */
#define	DIS_COMMENT_POSITION	18
#define	DIS_COMMENT_MINPAD	2
#define	DIS_COMMENT_MARK	"; "

static const char	empty[] = {'\0'};
static const char	comma_space[] = ", ";

/* Redefine register number here for cross compile. */
#define	ARM_REG_PC	15

/*
 * String representation of general registers.
 * Index must be [0, 15] that indicates register number.
 */
static const char	*dis_greg[] = {
	"r0",
	"r1",
	"r2",
	"r3",
	"r4",
	"r5",
	"r6",
	"r7",
	"r8",
	"r9",
	"r10",
	"r11",
	"r12",
	"sp",
	"lr",
	"pc",
};

#define	ARM_NGREG		(sizeof(dis_greg) / sizeof(const char *))
#define	ARM_SPECREG_START	13

/*
 * String representation of condition field.
 * Index must be [0, 14] that indicates condition field value.
 */
static const char	*dis_cond[] = {
	"eq",		/* Z == 1 */
	"ne",		/* Z == 0 */
	"cs",		/* C == 1 */
	"cc",		/* C == 0 */
	"mi",		/* N == 1 */
	"pl",		/* N == 0 */
	"vs",		/* V == 1 */
	"vc",		/* V == 0 */
	"hi",		/* C == 1 && Z == 0 */
	"ls",		/* C == 0 || Z == 1 */
	"ge",		/* N == V */
	"lt",		/* N != V */
	"gt",		/* Z == 0 && N == V */
	"le",		/* Z == 1 || N != V */
	empty,		/* Always */
};

/* String representation of shift operand */
static const char	*dis_shift[] = {
	"lsl", "lsr", "asr", "ror",
};

/*
 * Instruction format is determined by format control string embedded in
 * format string. The following BNF defines the format:
 *
 *	<format-string> := "${" <control-string> "}" |
 *		    "${" <control-string> ":" <bit-range> "}" |
 *		    "${" <control-string> ":" <bit-range> "}" ":" <string>
 *	<bit-range> := <bit-number> | <bit-number> "-" <bit-number>
 *	<bit-number> := Bit number. zero means LSB.
 *	<control-string> := string that determines format.
 *	<string> := Any string, that may contains ":".
 *
 * Remarks:
 *	- <bit-range> must have larger value in the left parameter.
 *	  For instance, "31-28" is valid, but "28-31" is not.
 *
 * The following control characters are recognized:
 *
 *	Rm		Replace with ARM general register that is determined
 *			by value of bits [0, 3].
 *	Rs		Replace with ARM general register that is determined
 *			by value of bits [8, 11].
 *	Rd		Replace with ARM general register that is determined
 *			by value of bits [12, 15].
 *	Rn		Replace with ARM general register that is determined
 *			by value of bits [16, 19].
 *	Dm		Replace with double precision VFP register that is
 *			determined by value of bits [0, 3].
 *	Dd		Replace with double precision VFP register that is
 *			determined by value of bits [12, 15].
 *	Dn		Replace with double precision VFP register that is
 *			determined by value of bits [16, 19].
 *	DM		Replace with double precision VFP register list
 *			for load/store multiple.
 *	Fm		Replace with single precision VFP register that is
 *			determined by value of bits [0, 3] and bit [5].
 *	Fd		Replace with single precision VFP register that is
 *			determined by value of bits [12, 15] and bit [22].
 *	Fn		Replace with single precision VFP register that is
 *			determined by value of bits [16, 19] and bit [7].
 *	FM		Replace with single precision VFP register list
 *			for load/store multiple.
 *	FP		Replace with single precision VFP register pair
 *			determined by value of bits [0, 3] and bit [5].
 *	a		Replace with string representation of addressing mode
 *			for ldr/str.
 *	A		Replace with string representation of addressing mode
 *			for ldc/stc.
 *	b		Replace with target address for bl (signed immed 24)
 *	B		Replace with target address for blx
 *			(signed immed 24 + H)
 *	c		Replace with string representation of condition field.
 *			If ${c} is not specified in mnemonic format,
 *			it will be appended at the tail of mnemonic
 *			automatically unless ARM_IF_NOCOND is defined.
 *	C		Replace with comment mark. (";")
 *	d:<bit-range>	Replace with decimal string that indicates value
 *			of bits specified by <bit-range>.
 *	h		Replace with string representation of addressing mode
 *			for halfword, signed, doubleword ldr/str.
 *	m		Replace with string representation of addressing mode
 *			for load/store multiple.
 *	M		Replace with register list of load/store multiple.
 *	o		Replace with string representation of shifter operand.
 *	r		Replace with rotation field (bits [10, 11]) for
 *			sign or zero expansion instructions, such as sxth.
 *	s:<bit-range>:<string-1>:<string-2>
 *			Replace with <string-1> if the value of bits
 *			<bit-range> is true, otherwise <string-2>.
 *			If <string-2> is omitted, "" is used as <string-2>.
 *	p:<bit-range>	Replace with decimal string that indicates value
 *			of bits specified by <bit-range> plus 1.
 *	S		Replace with PSR field name. The field mask is
 *			determined by value of bits <19-16>.
 *	t		Replace with "t" if P is not set and W is set.
 *			This is used to dump ldrt/strt.
 *	x:<bit-range>	Replace with hex string that indicates value of bits
 *			specified by <bit-range>.
 *	V		Replace with VFP system register name. The field mask
 *			is determined by value of bits <19-16>.
 *
 * If you want to output "$", you must quote like "$$".
 */

#define	DIS_CTRL_DOLLAR		'$'
#define	DIS_CTRL_LBRACE		'{'
#define	DIS_CTRL_RBRACE		'}'
#define	DIS_CTRL_HYPHEN		'-'
#define	DIS_CTRL_COMMA		':'

/*
 * arm_inst structure defines ARM instructions.
 */
typedef struct arm_inst {
	/*
	 * The instruction is determined by the following two members.
	 * If ((instruction & ai_mask) == ai_value) is true, this arm_inst
	 * structure is taken to stringfy instruction.
	 */
	uint32_t	ai_value;
	uint32_t	ai_mask;

	/* Format strings */
	const char	*ai_mnemonic;		/* Instruction mnemonic */
	const char	*ai_operands;		/* Operands */

	uint_t		ai_flags;		/* Flags */
} arm_inst_t;

/* Flags for ai_flags */
#define	ARM_IF_NOCOND		0x1		/* No condition field */
#define	ARM_IF_UNDEF		0x2		/* Undefined instruction */

/*
 * Shared format of operand
 */

/* Common register number field */
#define	ARM_RF_RN		"${Rn}"
#define	ARM_RF_RD		"${Rd}"
#define	ARM_RF_RS		"${Rs}"
#define	ARM_RF_RM		"${Rm}"

static const char	op_rf_rm[] = ARM_RF_RM;
static const char	op_rf_rdm[] = ARM_RF_RD ", " ARM_RF_RM;
static const char	op_rf_rdmn[] = ARM_RF_RD ", " ARM_RF_RM ", " ARM_RF_RN;
static const char	op_rf_rdnm[] = ARM_RF_RD ", " ARM_RF_RN ", " ARM_RF_RM;
static const char	op_rf_rnms[] = ARM_RF_RN ", " ARM_RF_RM ", " ARM_RF_RS;
static const char	op_rf_rnmsd[] =
	ARM_RF_RN ", " ARM_RF_RM ", "ARM_RF_RS ", " ARM_RF_RD;
static const char	op_rf_rdnms[] =
	ARM_RF_RD ", " ARM_RF_RN ", "ARM_RF_RM ", " ARM_RF_RS;

static const char	op_rf_multi[] = ARM_RF_RN "${s:21:!}, ${M}${s:22:^}";

#define	ARM_OP_CPS	"${s:8:a}${s:7:i}${s:6:f}"
#define	ARM_OP_CPS_MODE	"#${d:4-0}${C}0x${x:4-0}"

static const char	op_cps[] = ARM_OP_CPS;
static const char	op_cps_mode[] = ARM_OP_CPS ", " ARM_OP_CPS_MODE;

#define	ARM_OP_SHIFT_OPERAND	"${o}"

static const char	op_rd_shifter[] =
	ARM_RF_RD ", " ARM_OP_SHIFT_OPERAND;
static const char	op_rn_shifter[] =
	ARM_RF_RN ", " ARM_OP_SHIFT_OPERAND;
static const char	op_rdn_shifter[] =
	ARM_RF_RD ", " ARM_RF_RN ", " ARM_OP_SHIFT_OPERAND;

static const char	op_rd_adm[] = ARM_RF_RD ", ${a}";
static const char	op_rd_hadm[] = ARM_RF_RD ", ${h}";

#define	ARM_CPR_NAME(fmt)	"cr" fmt
#define	ARM_OP_CPR	"${d:11-8}, ${d:23-21}, " ARM_RF_RD ", "	\
	ARM_CPR_NAME("${d:19-16}") ", " ARM_CPR_NAME("${d:3-0}")	\
	", {${d:7-5}}"
#define	ARM_OP_CPR_CDP	"${d:11-8}, ${d:23-20}, "			\
	ARM_CPR_NAME("${d:15-12}") ", "	ARM_CPR_NAME("${d:19-16}") ", "	\
	ARM_CPR_NAME("${d:3-0}") ", {${d:7-5}}"
#define	ARM_OP_MCPR	"${d:11-8}, ${d:7-4}, " ARM_RF_RD ", "\
	ARM_RF_RN ", " ARM_CPR_NAME("${d:3-0}")
#define	ARM_OP_LDC	"${d:11-8}, " ARM_CPR_NAME("${d:15-12}") ", ${A}"

static const char	op_cpr[] = ARM_OP_CPR;
static const char	op_cpr_cdp[] = ARM_OP_CPR_CDP;
static const char	op_mcpr[] = ARM_OP_MCPR;
static const char	op_ldc[] = ARM_OP_LDC;

static const char	op_ldrex[] = ARM_RF_RD ", [" ARM_RF_RN "]";
static const char	op_strex[] =
	ARM_RF_RD ", " ARM_RF_RM ", [" ARM_RF_RN "]";

#define	ARM_OP_PKHBT							\
	ARM_RF_RD ", " ARM_RF_RN ", " ARM_RF_RM ", lsl #${d:11-7}"
#define	ARM_OP_PKHTB_ASR32						\
	ARM_RF_RD ", " ARM_RF_RN ", " ARM_RF_RM ", asr #32"
#define	ARM_OP_PKHTB							\
	ARM_RF_RD ", " ARM_RF_RN ", " ARM_RF_RM ", asr #${d:11-7}"

#define	ARM_OP_SSAT							\
	ARM_RF_RD ", #${p:20-16}, " ARM_RF_RM
#define	ARM_OP_SSAT_LSL							\
	ARM_RF_RD ", #${p:20-16}, " ARM_RF_RM ", lsl #${d:11-7}"
#define	ARM_OP_SSAT_ASR							\
	ARM_RF_RD ", #${p:20-16}, " ARM_RF_RM ", asr #${d:11-7}"
#define	ARM_OP_SSAT_ASR32						\
	ARM_RF_RD ", #${p:20-16}, " ARM_RF_RM ", asr #32"

#define	ARM_OP_USAT							\
	ARM_RF_RD ", #${d:20-16}, " ARM_RF_RM
#define	ARM_OP_USAT_LSL							\
	ARM_RF_RD ", #${d:20-16}, " ARM_RF_RM ", lsl #${d:11-7}"
#define	ARM_OP_USAT_ASR							\
	ARM_RF_RD ", #${d:20-16}, " ARM_RF_RM ", asr #${d:11-7}"
#define	ARM_OP_USAT_ASR32						\
	ARM_RF_RD ", #${d:20-16}, " ARM_RF_RM ", asr #32"

/* SWP instruction format */
#define	ARM_OP_SWP	ARM_RF_RD ", " ARM_RF_RM ", [" ARM_RF_RN "]"
#define	ARM_IVALUE_SWP							\
	((ARM_IVALUE_OPC1(0x10)|ARM_IMASK_DECL(7, 4, 9)) & ~ARM_IMASK_RDMN)
#define	ARM_IMASK_SWP	(ARM_IMASK_SET(22, 22)|ARM_IMASK_RDMN)

static const char	op_sat16[] = ARM_RF_RD ", #${p:19-16}, " ARM_RF_RM;

static const char	op_xta[] =
	ARM_RF_RD ", " ARM_RF_RN ", " ARM_RF_RM "${r}";
static const char	op_xt[] = ARM_RF_RD ", " ARM_RF_RM "${r}";

/* Create instruction mask and value */
#define	ARM_IMASK_SHIFTLEN(bithi, bitlo)	(32 - ((bithi) - (bitlo) + 1))
#define	ARM_IMASK_DECL(bithi, bitlo, val)				\
	(((((uint32_t)(val)) << ARM_IMASK_SHIFTLEN(bithi, bitlo)) >>	\
	  ARM_IMASK_SHIFTLEN(bithi, bitlo)) << (bitlo))

#define	ARM_IMASK_SET(hibit, lobit)			\
	ARM_IMASK_DECL(hibit, lobit, 0xffffffffU)

#define	ARM_IMASK_GET(bits, hibit, lobit)				\
	(((uint32_t)(bits) & ARM_IMASK_SET(hibit, lobit)) >> (lobit))

#define	ARM_IMASK_COND		0xf0000000
#define	ARM_IMASK_RN		ARM_IMASK_DECL(19, 16, 0xfU)
#define	ARM_IMASK_RD		ARM_IMASK_DECL(15, 12, 0xfU)
#define	ARM_IMASK_RS		ARM_IMASK_DECL(11, 8, 0xfU)
#define	ARM_IMASK_RM		ARM_IMASK_DECL(3, 0, 0xfU)
#define	ARM_IMASK_RDM		(ARM_IMASK_RD|ARM_IMASK_RM)
#define	ARM_IMASK_RDN		(ARM_IMASK_RD|ARM_IMASK_RN)
#define	ARM_IMASK_RDMN		(ARM_IMASK_RD|ARM_IMASK_RM|ARM_IMASK_RN)
#define	ARM_IMASK_RDNM		ARM_IMASK_RDMN
#define	ARM_IMASK_RNMS		(ARM_IMASK_RN|ARM_IMASK_RM|ARM_IMASK_RS)
#define	ARM_IMASK_RNMSD						\
	(ARM_IMASK_RN|ARM_IMASK_RM|ARM_IMASK_RS|ARM_IMASK_RD)
#define	ARM_IMASK_RDNMS		ARM_IMASK_RNMSD
#define	ARM_IMASK_MULTI		(ARM_IMASK_SET(24, 21)|ARM_IMASK_SET(19, 0))
#define	ARM_IMASK_CPR							\
	(ARM_IMASK_SET(23, 21)|ARM_IMASK_SET(19, 5)|ARM_IMASK_SET(3, 0))
#define	ARM_IMASK_MCPR	ARM_IMASK_SET(19, 0)
#define	ARM_IMASK_LDC	(ARM_IMASK_SET(24, 21)|ARM_IMASK_SET(19, 0))

#define	ARM_IMASK_GET_RM(inst)	ARM_IMASK_GET(inst, 3, 0)
#define	ARM_IMASK_GET_RS(inst)	ARM_IMASK_GET(inst, 11, 8)
#define	ARM_IMASK_GET_RD(inst)	ARM_IMASK_GET(inst, 15, 12)
#define	ARM_IMASK_GET_RN(inst)	ARM_IMASK_GET(inst, 19, 16)

/* VFP v2 format */
#define	ARM_IMASK_GET_VFP_DM(inst)	ARM_IMASK_GET(inst, 3, 0)
#define	ARM_IMASK_GET_VFP_DD(inst)	ARM_IMASK_GET(inst, 15, 12)
#define	ARM_IMASK_GET_VFP_DN(inst)	ARM_IMASK_GET(inst, 19, 16)

#define	ARM_IMASK_GET_VFP_FM(inst)					\
	((ARM_IMASK_GET(inst, 3, 0) << 1) | ARM_IMASK_GET(inst, 5, 5))
#define	ARM_IMASK_GET_VFP_FD(inst)					\
	((ARM_IMASK_GET(inst, 15, 12) << 1) | ARM_IMASK_GET(inst, 22, 22))
#define	ARM_IMASK_GET_VFP_FN(inst)					\
	((ARM_IMASK_GET(inst, 19, 16) << 1) | ARM_IMASK_GET(inst, 7, 7))

#define	ARM_RF_DM		"${Dm}"
#define	ARM_RF_DD		"${Dd}"
#define	ARM_RF_DN		"${Dn}"

#define	ARM_RF_FM		"${Fm}"
#define	ARM_RF_FD		"${Fd}"
#define	ARM_RF_FN		"${Fn}"

#define	ARM_IMASK_VFP_DM	ARM_IMASK_DECL(3, 0, 0xfU)
#define	ARM_IMASK_VFP_DD	ARM_IMASK_DECL(15, 12, 0xfU)
#define	ARM_IMASK_VFP_DN	ARM_IMASK_DECL(19, 16, 0xfU)

#define	ARM_IMASK_VFP_DDM	(ARM_IMASK_VFP_DD|ARM_IMASK_VFP_DM)
#define	ARM_IMASK_VFP_DDNM					\
	(ARM_IMASK_VFP_DD|ARM_IMASK_VFP_DN|ARM_IMASK_VFP_DM)

#define	ARM_IMASK_VFP_FM					\
	(ARM_IMASK_DECL(3, 0, 0xfU) | ARM_IMASK_DECL(5, 5, 1))
#define	ARM_IMASK_VFP_FD					\
	(ARM_IMASK_DECL(15, 12, 0xfU) | ARM_IMASK_DECL(22, 22, 1))
#define	ARM_IMASK_VFP_FN					\
	(ARM_IMASK_DECL(19, 16, 0xfU) | ARM_IMASK_DECL(7, 7, 1))

#define	ARM_IMASK_VFP_FDM	(ARM_IMASK_VFP_FD|ARM_IMASK_VFP_FM)
#define	ARM_IMASK_VFP_FDNM					\
	(ARM_IMASK_VFP_FD|ARM_IMASK_VFP_FN|ARM_IMASK_VFP_FM)

#define	ARM_IMASK_VFP_DDFM	(ARM_IMASK_VFP_DD|ARM_IMASK_VFP_FM)
#define	ARM_IMASK_VFP_FDDM	(ARM_IMASK_VFP_FD|ARM_IMASK_VFP_DM)
#define	ARM_IMASK_VFP_DNRD	(ARM_IMASK_VFP_DN|ARM_IMASK_RD)
#define	ARM_IMASK_VFP_FNRD	(ARM_IMASK_VFP_FN|ARM_IMASK_RD)
#define	ARM_IMASK_VFP_DMRDRN	(ARM_IMASK_VFP_DM|ARM_IMASK_RD|ARM_IMASK_RN)
#define	ARM_IMASK_VFP_RDRNFM1	(ARM_IMASK_RD|ARM_IMASK_RN|ARM_IMASK_VFP_FM)

#define	ARM_IMASK_VFP_FLDD						\
	(ARM_IMASK_VFP_DD|ARM_IMASK_RN|ARM_IMASK_ADM_U|ARM_IMASK_SET(7, 0))
#define	ARM_IMASK_VFP_FLDS						\
	(ARM_IMASK_VFP_FD|ARM_IMASK_RN|ARM_IMASK_ADM_U|ARM_IMASK_SET(7, 0))

#define	ARM_IMASK_VFP_FLDMD						\
	(ARM_IMASK_VFP_DD|ARM_IMASK_RN|ARM_IMASK_ADM_P|ARM_IMASK_ADM_U|	\
	 ARM_IMASK_ADM_W|ARM_IMASK_SET(7, 0))
#define	ARM_IMASK_VFP_FLDMS						\
	(ARM_IMASK_VFP_FD|ARM_IMASK_RN|ARM_IMASK_ADM_P|ARM_IMASK_ADM_U|	\
	 ARM_IMASK_ADM_W|ARM_IMASK_SET(7, 0))

#define	ARM_IMASK_VFP_FMRX	(ARM_IMASK_RD|ARM_IMASK_SET(19, 16))

static const char	op_vfp_dd[] = ARM_RF_DD;
static const char	op_vfp_ddm[] = ARM_RF_DD ", " ARM_RF_DM;
static const char	op_vfp_ddnm[] =
	ARM_RF_DD ", " ARM_RF_DN ", " ARM_RF_DM;

static const char	op_vfp_fd[] = ARM_RF_FD;
static const char	op_vfp_fdm[] = ARM_RF_FD ", " ARM_RF_FM;
static const char	op_vfp_fdnm[] =
	ARM_RF_FD ", " ARM_RF_FN ", " ARM_RF_FM;

static const char	op_vfp_ddfm[] = ARM_RF_DD ", " ARM_RF_FM;
static const char	op_vfp_fddm[] = ARM_RF_FD ", " ARM_RF_DM;
static const char	op_vfp_dnrd[] = ARM_RF_DN ", " ARM_RF_RD;
static const char	op_vfp_fnrd[] = ARM_RF_FN ", " ARM_RF_RD;
static const char	op_vfp_rddn[] = ARM_RF_RD ", " ARM_RF_DN;
static const char	op_vfp_rdfn[] = ARM_RF_RD ", " ARM_RF_FN;
static const char	op_vfp_dmrdrn[] =
	ARM_RF_DM ", " ARM_RF_RD ", " ARM_RF_RN;
static const char	op_vfp_rdrndm[] =
	ARM_RF_RD ", " ARM_RF_RN ", " ARM_RF_DM;
static const char	op_vfp_rdrnfm1[] = ARM_RF_RD ", " ARM_RF_RN ", ${FP}";
static const char	op_vfp_fm1rdrn[] = "${FP}, " ARM_RF_RD ", " ARM_RF_RN;

static const char	op_vfp_fldd[] = ARM_RF_DD ", ${A}";
static const char	op_vfp_flds[] = ARM_RF_FD ", ${A}";

static const char	op_vfp_fldmd[] = ARM_RF_RN "${s:21:!}, ${DM}";
static const char	op_vfp_fldms[] = ARM_RF_RN "${s:21:!}, ${FM}";

/* Addressing mode bits */
#define	ARM_IMASK_ADM		ARM_IMASK_SET(11, 0)
#define	ARM_IMASK_ADM_RN	(ARM_IMASK_ADM|ARM_IMASK_RN)
#define	ARM_IMASK_ADM_I		(1U << 25)
#define	ARM_IMASK_ADM_P		(1U << 24)
#define	ARM_IMASK_ADM_U		(1U << 23)
#define	ARM_IMASK_ADM_B		(1U << 22)
#define	ARM_IMASK_ADM_W		(1U << 21)
#define	ARM_IMASK_ADM_L		(1U << 20)

#define	ARM_IMASK_PSR_S		(1U << 20)

#define	ARM_IMASK_SHIFT		ARM_IMASK_SET(11, 0)
#define	ARM_IMASK_RD_SHIFT	(ARM_IMASK_SHIFT|ARM_IMASK_RD|ARM_IMASK_PSR_S)
#define	ARM_IMASK_RN_SHIFT	(ARM_IMASK_SHIFT|ARM_IMASK_RN)
#define	ARM_IMASK_RDN_SHIFT	\
	(ARM_IMASK_RD_SHIFT|ARM_IMASK_RN|ARM_IMASK_PSR_S)

#define	ARM_IMASK_RD_ADM	(ARM_IMASK_SET(24, 21)|ARM_IMASK_SET(19, 0))
#define	ARM_IMASK_RD_HADM						\
	(ARM_IMASK_SET(24, 21)|ARM_IMASK_SET(19, 8)|ARM_IMASK_SET(3, 0))

#define	ARM_IMASK_IMMED24		ARM_IMASK_SET(23, 0)
#define	ARM_IMASK_IMMED24_MINUS		ARM_IMASK_SET(31, 24)

/* Get immediate for halfword addressing mode */
#define	ARM_IMASK_GET_HALF_IMMED(inst)			\
	((((inst) & 0xf00) >> 4) | ((inst) & 0xf))

/* CPSR or SPSR */
#define	ARM_IMASK_PSR		(1U << 22)

/* Condition field value */
#define	ARM_IVALUE_COND_ALWAYS		ARM_IMASK_DECL(31, 28, 0xe)

#define	ARM_IVALUE_REGION(region)	ARM_IMASK_DECL(27, 25, region)
#define	ARM_IVALUE_SBO(hibit, lobit)	ARM_IMASK_SET(hibit, lobit)

/* opcode [24-21] */
#define	ARM_IVALUE_OPC(code)		ARM_IMASK_DECL(24, 21, code)

/* opcode1 [24-20] */
#define	ARM_IVALUE_OPC1(code)		ARM_IMASK_DECL(24, 20, code)

/*
 * name:	mnemonic
 * operands:	format string for operands.
 * region:	required instruction region field value.
 * value:	mask value that must be set in the instruction bits.
 * clmask:	mask value that defines the instruction bits to be ignored.
 */
#define	ARM_INST_CONDMASK(flags)				\
	(((flags) & ARM_IF_NOCOND) ? 0 : ARM_IMASK_COND)
#define	ARM_INST_VALUE(value, region, flags)				\
	(((value) & ~ARM_INST_CONDMASK(flags)) | ARM_IVALUE_REGION(region))
#define	ARM_INST_MASK(clmask, flags)				\
	(0xffffffffU & ~((clmask) | ARM_INST_CONDMASK(flags)))
#define	ARM_INST_DECL(name, operands, region, value, clmask, flags)	\
	{ARM_INST_VALUE(value, region, flags),				\
	 ARM_INST_MASK(clmask, flags),					\
	 name, operands, flags}

/* Takes one register (Rm) as operand */
#define	ARM_INST_RM_DECL(name, region, value, clmask, flags)		\
	ARM_INST_DECL(name, op_rf_rm, region,				\
		      (value) & ~ARM_IMASK_RM,				\
		      (clmask) | ARM_IMASK_RM, flags)

/* Takes two registers (Rd, Rm) as operand */
#define	ARM_INST_RDM_DECL(name, region, value, clmask, flags)		\
	ARM_INST_DECL(name, op_rf_rdm, region,				\
		      (value) & ~ARM_IMASK_RDM,				\
		      (clmask) | ARM_IMASK_RDM, flags)

/* Takes three registers (Rd, Rm, Rn) as operand */
#define	ARM_INST_RDMN_DECL(name, region, value, clmask, flags)		\
	ARM_INST_DECL(name, op_rf_rdmn, region,				\
		      (value) & ~ARM_IMASK_RDMN,			\
		      (clmask) | ARM_IMASK_RDMN, flags)

/* Takes three registers (Rd, Rn, Rm) as operand */
#define	ARM_INST_RDNM_DECL(name, region, value, clmask, flags)		\
	ARM_INST_DECL(name, op_rf_rdnm, region,				\
		      (value) & ~ARM_IMASK_RDNM,			\
		      (clmask) | ARM_IMASK_RDNM, flags)

/* Takes three registers (Rn, Rm, Rs) as operand */
#define	ARM_INST_RNMS_DECL(name, region, value, clmask, flags)		\
	ARM_INST_DECL(name, op_rf_rnms, region,				\
		      (value) & ~ARM_IMASK_RNMS,			\
		      (clmask) | ARM_IMASK_RNMS, flags)

/* Takes four registers (Rn, Rm, Rs, Rd) as operand */
#define	ARM_INST_RNMSD_DECL(name, region, value, clmask, flags)		\
	ARM_INST_DECL(name, op_rf_rnmsd, region,			\
		      (value) & ~ARM_IMASK_RNMSD,			\
		      (clmask) | ARM_IMASK_RNMSD, flags)

/* Takes four registers (Rd, Rn, Rm, Rs) as operand */
#define	ARM_INST_RDNMS_DECL(name, region, value, clmask, flags)		\
	ARM_INST_DECL(name, op_rf_rdnms, region,			\
		      (value) & ~ARM_IMASK_RDNMS,			\
		      (clmask) | ARM_IMASK_RDNMS, flags)

/* Takes one register (Rd) and shifter as operand */
#define	ARM_INST_RD_SHIFT_DECL(name, region, value, clmask, flags)	\
	ARM_INST_DECL(name, op_rd_shifter, region,			\
		      (value) & ~ARM_IMASK_RD_SHIFT,			\
		      (clmask) | ARM_IMASK_RD_SHIFT, flags)

/* Takes one register (Rn) and shifter as operand */
#define	ARM_INST_RN_SHIFT_DECL(name, region, value, clmask, flags)	\
	ARM_INST_DECL(name, op_rn_shifter, region,			\
		      (value) & ~ARM_IMASK_RN_SHIFT,			\
		      (clmask) | ARM_IMASK_RN_SHIFT, flags)

/* Take two registers(Rd, Rd) and shifter as operand */
#define	ARM_INST_RDN_SHIFT_DECL(name, region, value, clmask, flags)	\
	ARM_INST_DECL(name, op_rdn_shifter, region,			\
		      (value) & ~ARM_IMASK_RDN_SHIFT,			\
		      (clmask) | ARM_IMASK_RDN_SHIFT, flags)

/* Take one register (Rd) and addressing mode */
#define	ARM_INST_RD_ADM_DECL(name, region, value, clmask, flags)	\
	ARM_INST_DECL(name, op_rd_adm, region,				\
		      (value) & ~ARM_IMASK_RD_ADM,			\
		      (clmask) | ARM_IMASK_RD_ADM, flags)

/* Take one register (Rd) and halfword addressing mode */
#define	ARM_INST_RD_HALF_ADM_DECL(name, l, halfop, clmask, flags)	\
	ARM_INST_DECL(name, op_rd_hadm, 0,				\
		      (ARM_IMASK_DECL(20, 20, (l))|			\
		       ARM_IMASK_DECL(7, 4, halfop)) & ~ARM_IMASK_RD_HADM, \
		      (clmask) | ARM_IMASK_RD_HADM, flags)

/* Declare arm_inst for load/store multiple. */
#define	ARM_INST_MULTI_DECL(name, value)				\
	ARM_INST_DECL(name "${c}${m}", op_rf_multi, 4,			\
		      (value) & ~ARM_IMASK_MULTI, ARM_IMASK_MULTI, 0)

/* Declare branch instruction (signed 24 bit immediate) */
#define	ARM_INST_IMMED24_DECL(name, operands, value, clmask)		\
	ARM_INST_DECL(name, operands, 5,				\
		      (value) & ~ARM_IMASK_IMMED24,			\
		      (clmask) | (ARM_IMASK_IMMED24|ARM_IMASK_SET(24, 24)), 0)

/* Declare co-processor transfer instruction */
#define	ARM_INST_CPR_DECL(name, operands, value, clmask, flags)		\
	ARM_INST_DECL(name, operands, 7,				\
		      (value) & ~ARM_IMASK_CPR,				\
		      (clmask) | ARM_IMASK_CPR, flags)

/* Declare multiple co-processor transfer instruction */
#define	ARM_INST_MCPR_DECL(name, value, clmask, flags)			\
	ARM_INST_DECL(name, op_mcpr, 6,	(value) & ~ARM_IMASK_MCPR,	\
		      (clmask) | ARM_IMASK_MCPR, flags)

/* Declare co-processor load/store instruction */
#define	ARM_INST_LDC_DECL(name, l, flags)				\
	ARM_INST_DECL(name, op_ldc, 6,					\
		      ARM_IMASK_DECL(20, 20, (l)) & ~ARM_IMASK_LDC,	\
		      ARM_IMASK_LDC, flags)

/* Declare load/store exclusive instruction */
#define	ARM_INST_LDREX_DECL(name, value)				\
	ARM_INST_DECL(name, op_ldrex, 0,				\
		      ((value) & ~(ARM_IMASK_RDN))|ARM_IMASK_ADM_L|	\
		      ARM_IMASK_SET(11, 8)|ARM_IMASK_DECL(7, 4, 0x9)|	\
		      ARM_IMASK_SET(3, 0), ARM_IMASK_RDN, 0)
#define	ARM_INST_STREX_DECL(name, value)				\
	ARM_INST_DECL(name, op_strex, 0,				\
		      (value) & ~(ARM_IMASK_RDMN|ARM_IMASK_ADM_L)|	\
		      ARM_IMASK_SET(11, 8)|ARM_IMASK_DECL(7, 4, 0x9),	\
		      ARM_IMASK_RDMN, 0)

/*
 * Declare sign or zero expansion instruction.
 * ARM_INST_XT_DECL() must be declared before ARM_INST_XTA_DECL().
 */
#define	ARM_INST_XTA_DECL(name, opcode)					\
	ARM_INST_DECL(name, op_xta, 3,					\
		      (ARM_IVALUE_OPC1(opcode)|ARM_IMASK_DECL(7, 4, 7)) & \
		      ~ARM_IMASK_RDNM,					\
		      ARM_IMASK_RDNM|ARM_IMASK_SET(11, 10), 0)

#define	ARM_INST_XT_DECL(name, opcode)					\
	ARM_INST_DECL(name, op_xt, 3,					\
		      (ARM_IVALUE_OPC1(opcode)|ARM_IMASK_DECL(7, 4, 7)|	\
		       ARM_IMASK_SET(19, 16)) & ~ARM_IMASK_RDM,		\
		      ARM_IMASK_RDM|ARM_IMASK_SET(11, 10), 0)

/* Takes one register (Dd) as operand */
#define	ARM_INST_VFP_DD_DECL(name, region, value, clmask, flags)	\
	ARM_INST_DECL(name, op_vfp_dd, region,				\
		      (value) & ~ARM_IMASK_VFP_DD,			\
		      (clmask) | ARM_IMASK_VFP_DD, flags)

/* Takes two registers (Dd, Dm) as operand */
#define	ARM_INST_VFP_DDM_DECL(name, region, value, clmask, flags)	\
	ARM_INST_DECL(name, op_vfp_ddm, region,				\
		      (value) & ~ARM_IMASK_VFP_DDM,			\
		      (clmask) | ARM_IMASK_VFP_DDM, flags)

/* Takes three registers (Dd, Dn, Dm) as operand */
#define	ARM_INST_VFP_DDNM_DECL(name, region, value, clmask, flags)	\
	ARM_INST_DECL(name, op_vfp_ddnm, region,			\
		      (value) & ~ARM_IMASK_VFP_DDNM,			\
		      (clmask) | ARM_IMASK_VFP_DDNM, flags)

/* Takes one register (Sd) as operand */
#define	ARM_INST_VFP_FD_DECL(name, region, value, clmask, flags)	\
	ARM_INST_DECL(name, op_vfp_fd, region,				\
		      (value) & ~ARM_IMASK_VFP_FD,			\
		      (clmask) | ARM_IMASK_VFP_FD, flags)

/* Takes two registers (Sd, Sm) as operand */
#define	ARM_INST_VFP_FDM_DECL(name, region, value, clmask, flags)	\
	ARM_INST_DECL(name, op_vfp_fdm, region,				\
		      (value) & ~ARM_IMASK_VFP_FDM,			\
		      (clmask) | ARM_IMASK_VFP_FDM, flags)

/* Takes three registers (Sd, Sn, Sm) as operand */
#define	ARM_INST_VFP_FDNM_DECL(name, region, value, clmask, flags)	\
	ARM_INST_DECL(name, op_vfp_fdnm, region,			\
		      (value) & ~ARM_IMASK_VFP_FDNM,			\
		      (clmask) | ARM_IMASK_VFP_FDNM, flags)

/* Takes two registers (Dd, Sm) as operand */
#define	ARM_INST_VFP_DDFM_DECL(name, region, value, clmask, flags)	\
	ARM_INST_DECL(name, op_vfp_ddfm, region,			\
		      (value) & ~ARM_IMASK_VFP_DDFM,			\
		      (clmask) | ARM_IMASK_VFP_DDFM, flags)

/* Takes two registers (Sd, Dm) as operand */
#define	ARM_INST_VFP_FDDM_DECL(name, region, value, clmask, flags)	\
	ARM_INST_DECL(name, op_vfp_fddm, region,			\
		      (value) & ~ARM_IMASK_VFP_FDDM,			\
		      (clmask) | ARM_IMASK_VFP_FDDM, flags)

/* Takes two registers (Dn, Rd) as operand */
#define	ARM_INST_VFP_DNRD_DECL(name, region, value, clmask, flags)	\
	ARM_INST_DECL(name, op_vfp_dnrd, region,			\
		      (value) & ~ARM_IMASK_VFP_DNRD,			\
		      (clmask) | ARM_IMASK_VFP_DNRD, flags)

/* Takes two registers (Rd, Dn) as operand */
#define	ARM_INST_VFP_RDDN_DECL(name, region, value, clmask, flags)	\
	ARM_INST_DECL(name, op_vfp_rddn, region,			\
		      (value) & ~ARM_IMASK_VFP_DNRD,			\
		      (clmask) | ARM_IMASK_VFP_DNRD, flags)

/* Takes two registers (Sn, Rd) as operand */
#define	ARM_INST_VFP_FNRD_DECL(name, region, value, clmask, flags)	\
	ARM_INST_DECL(name, op_vfp_fnrd, region,			\
		      (value) & ~ARM_IMASK_VFP_FNRD,			\
		      (clmask) | ARM_IMASK_VFP_FNRD, flags)

/* Takes two registers (Rd, Sn) as operand */
#define	ARM_INST_VFP_RDFN_DECL(name, region, value, clmask, flags)	\
	ARM_INST_DECL(name, op_vfp_rdfn, region,			\
		      (value) & ~ARM_IMASK_VFP_FNRD,			\
		      (clmask) | ARM_IMASK_VFP_FNRD, flags)

/* Takes three registers (Dm, Rd, rn) as operand */
#define	ARM_INST_VFP_DMRDRN_DECL(name, region, value, clmask, flags)	\
	ARM_INST_DECL(name, op_vfp_dmrdrn, region,			\
		      (value) & ~ARM_IMASK_VFP_DMRDRN,			\
		      (clmask) | ARM_IMASK_VFP_DMRDRN, flags)

/* Takes three registers (Rd, Rn, Dm) as operand */
#define	ARM_INST_VFP_RDRNDM_DECL(name, region, value, clmask, flags)	\
	ARM_INST_DECL(name, op_vfp_rdrndm, region,			\
		      (value) & ~ARM_IMASK_VFP_DMRDRN,			\
		      (clmask) | ARM_IMASK_VFP_DMRDRN, flags)

/* Takes four registers (Rd, Rn, Sm, Sm1) as operand */
#define	ARM_INST_VFP_RDRNFM1_DECL(name, region, value, clmask, flags)	\
	ARM_INST_DECL(name, op_vfp_rdrnfm1, region,			\
		      (value) & ~ARM_IMASK_VFP_RDRNFM1,			\
		      (clmask) | ARM_IMASK_VFP_RDRNFM1, flags)

/* Takes four registers (Sm, Sm1, Rd, Rn) as operand */
#define	ARM_INST_VFP_FM1RDRN_DECL(name, region, value, clmask, flags)	\
	ARM_INST_DECL(name, op_vfp_fm1rdrn, region,			\
		      (value) & ~ARM_IMASK_VFP_RDRNFM1,			\
		      (clmask) | ARM_IMASK_VFP_RDRNFM1, flags)

/* Declare VFP double precision load/store instruction */
#define	ARM_INST_VFP_FLDD_DECL(name, region, value, clmask, flags)	\
	ARM_INST_DECL(name, op_vfp_fldd, region,			\
		      (value) & ~ARM_IMASK_VFP_FLDD,			\
		      (clmask) | ARM_IMASK_VFP_FLDD, flags)

/* Declare VFP single precision load/store instruction */
#define	ARM_INST_VFP_FLDS_DECL(name, region, value, clmask, flags)	\
	ARM_INST_DECL(name, op_vfp_flds, region,			\
		      (value) & ~ARM_IMASK_VFP_FLDS,			\
		      (clmask) | ARM_IMASK_VFP_FLDS, flags)

/* Declare VFP double precision multiple load/store instruction */
#define	ARM_INST_VFP_FLDMD_DECL(name, region, value, clmask, flags)	\
	ARM_INST_DECL(name, op_vfp_fldmd, region,			\
		      (value) & ~ARM_IMASK_VFP_FLDMD,			\
		      (clmask) | ARM_IMASK_VFP_FLDMD, flags)

/* Declare VFP single precision multiple load/store instruction */
#define	ARM_INST_VFP_FLDMS_DECL(name, region, value, clmask, flags)	\
	ARM_INST_DECL(name, op_vfp_fldms, region,			\
		      (value) & ~ARM_IMASK_VFP_FLDMS,			\
		      (clmask) | ARM_IMASK_VFP_FLDMS, flags)

/* Declare VFP unknown precision multiple load/store instruction */
#define	ARM_INST_VFP_FLDMX_DECL(name, region, value, clmask, flags)	\
	ARM_INST_DECL(name, op_vfp_fldmd, region,			\
		      ((value) & ~ARM_IMASK_VFP_FLDMD) | 0x1U,		\
		      ((clmask) | ARM_IMASK_VFP_FLDMD) & ~0x1U, flags)

/* Declare fmrx/fmxr instruction */
#define	ARM_INST_VFP_FMRX_DECL(name, operand, value, clmask, flags)	\
	ARM_INST_DECL(name, operand, 7,					\
		      ((value) & ~ARM_IMASK_VFP_FMRX),			\
		      ((clmask) | ARM_IMASK_VFP_FMRX), flags)

/* Mnemonic format that takes x, y suffix, such as smla<x><y> */
#define	ARM_MNF_X		"${s:5:t:b}"
#define	ARM_MNF_Y		"${s:6:t:b}"
#define	ARM_MN_XY(name)		name ARM_MNF_X ARM_MNF_Y
#define	ARM_MN_Y(name)		name ARM_MNF_Y

/* Instruction mnemonic that associated with two or more arm_inst structure */
static const char	inst_blx[] = "blx";
static const char	inst_mrs[] = "mrs";
static const char	inst_msr[] = "msr";
static const char	inst_cpsie[] = "cpsie";
static const char	inst_cpsid[] = "cpsid";

static const char	inst_mov[] = "mov${c}${s:20:s}";
static const char	inst_mvn[] = "mvn${c}${s:20:s}";

static const char	inst_tst[] = "tst${c}";
static const char	inst_teq[] = "teq${c}";
static const char	inst_cmp[] = "cmp${c}";
static const char	inst_cmn[] = "cmn${c}";

static const char	inst_add[] = "add${c}${s:20:s}";
static const char	inst_sub[] = "sub${c}${s:20:s}";
static const char	inst_rsb[] = "rsb${c}${s:20:s}";
static const char	inst_and[] = "and${c}${s:20:s}";
static const char	inst_eor[] = "eor${c}${s:20:s}";
static const char	inst_adc[] = "adc${c}${s:20:s}";
static const char	inst_sbc[] = "sbc${c}${s:20:s}";
static const char	inst_rsc[] = "rsc${c}${s:20:s}";
static const char	inst_orr[] = "orr${c}${s:20:s}";
static const char	inst_bic[] = "bic${c}${s:20:s}";

/* ldr, ldrb ,ldrbt */
static const char	inst_ldr[] = "ldr${c}${s:22:b}${t}";

/* str, strb, strbt */
static const char	inst_str[] = "str${c}${s:22:b}${t}";

/* ldrh, ldrsb, ldrsh */
static const char	inst_ldrh[] = "ldr${c}${s:6:s}${s:5:h:b}";

/* strh */
static const char	inst_strh[] = "str${c}h";

#define	ARM_INST_NENTRY(array)	(sizeof(array) / sizeof(arm_inst_t))

/*
 * Definition of ARM V6 instructions.
 * More characteristic instruction must have lower index.
 */

/* Unconditional */
static const arm_inst_t v6_inst_nocond[] = {
	/* ARM V6K instructions */
	ARM_INST_DECL("clrex", empty, 0, 0xf57ff01fU, 0, ARM_IF_NOCOND),

	/* cps(ie|id)? */
	ARM_INST_DECL(inst_cpsie, op_cps, 0,
		      ARM_IMASK_DECL(31, 16, 0xf108),
		      ARM_IMASK_SET(8, 6), ARM_IF_NOCOND),
	ARM_INST_DECL(inst_cpsie, op_cps_mode, 0,
		      ARM_IMASK_DECL(31, 16, 0xf10a),
		      ARM_IMASK_SET(8, 6)|ARM_IMASK_SET(4, 0), ARM_IF_NOCOND),
	ARM_INST_DECL(inst_cpsid, op_cps, 0,
		      ARM_IMASK_DECL(31, 16, 0xf10c),
		      ARM_IMASK_SET(8, 6), ARM_IF_NOCOND),
	ARM_INST_DECL(inst_cpsid, op_cps_mode, 0,
		      ARM_IMASK_DECL(31, 16, 0xf10e),
		      ARM_IMASK_SET(8, 6)|ARM_IMASK_SET(4, 0), ARM_IF_NOCOND),
	ARM_INST_DECL("cps", ARM_OP_CPS_MODE, 0,
		      ARM_IMASK_DECL(31, 16, 0xf102),
		      ARM_IMASK_SET(4, 0), ARM_IF_NOCOND),

	ARM_INST_DECL("setend", "${s:9:b:l}e", 0,
		      ARM_IMASK_DECL(31, 16, 0xf101),
		      ARM_IMASK_SET(9, 9), ARM_IF_NOCOND),
	ARM_INST_DECL("pld", "${a}", 0,
		      ARM_IMASK_DECL(31, 26, 0x3d)|ARM_IMASK_SET(24, 24)|
		      ARM_IMASK_DECL(22, 20, 5)|ARM_IMASK_SET(15, 12),
		      ARM_IMASK_ADM_RN|ARM_IMASK_SET(25, 25)|
		      ARM_IMASK_SET(23, 23), ARM_IF_NOCOND),
	ARM_INST_DECL("rfe${m}", ARM_RF_RN "${s:21:!}", 0,
		      ARM_IMASK_DECL(31, 25, 0x7c)|ARM_IMASK_SET(20, 20)|
		      ARM_IMASK_DECL(11, 8, 0xa),
		      ARM_IMASK_SET(24, 23)|ARM_IMASK_SET(21, 21)|
		      ARM_IMASK_RN, ARM_IF_NOCOND),
	ARM_INST_DECL("srs${m}", "#${d:4-0}${s:24:!}${C}0x${x:4-0}", 0,
		      ARM_IMASK_DECL(31, 25, 0x7c)|ARM_IMASK_SET(22, 22)|
		      ARM_IMASK_DECL(19, 16, 0xd)|ARM_IMASK_DECL(11, 8, 0x5),
		      ARM_IMASK_SET(24, 23)|ARM_IMASK_SET(21, 21)|
		      ARM_IMASK_SET(4, 0), ARM_IF_NOCOND),
	ARM_INST_DECL(inst_blx, "%B", 0,
		      ARM_IMASK_DECL(31, 25, 0x7d), ARM_IMASK_SET(24, 0),
		      ARM_IF_NOCOND),

	ARM_INST_MCPR_DECL("mcrr2", ARM_IVALUE_OPC1(0x4), 0, ARM_IF_NOCOND),
	ARM_INST_MCPR_DECL("mrrc2", ARM_IVALUE_OPC1(0x5), 0, ARM_IF_NOCOND),
	ARM_INST_LDC_DECL("ldc2${s:22:l}", 1, ARM_IF_NOCOND),
	ARM_INST_LDC_DECL("stc2${s:22:l}", 0, ARM_IF_NOCOND),

	ARM_INST_CPR_DECL("mcr2", op_cpr,
			  ARM_IMASK_DECL(31, 24, 0xfe)|ARM_IMASK_SET(4, 4),
			  0, ARM_IF_NOCOND),
	ARM_INST_CPR_DECL("mrc2", op_cpr,
			  ARM_IMASK_DECL(31, 24, 0xfe)|ARM_IMASK_SET(4, 4)|
			  ARM_IMASK_ADM_L, 0, ARM_IF_NOCOND),
	ARM_INST_DECL("cdp2", op_cpr_cdp, 0, ARM_IMASK_DECL(31, 24, 0xfe),
		      ARM_IMASK_SET(23, 5)|ARM_IMASK_SET(3, 0), ARM_IF_NOCOND),
};

/* Region 0 */
static const arm_inst_t	v6_inst_reg0[] = {
	/* Control and DSP extention */
	ARM_INST_DECL(inst_mrs, ARM_RF_RD ", ${s:22:s:c}psr", 0,
		      ARM_IVALUE_OPC1(0x10)|ARM_IVALUE_SBO(19, 16),
		      ARM_IMASK_RD|ARM_IMASK_PSR, 0),
	ARM_INST_DECL(inst_msr, "${s:22:s:c}psr${S}, " ARM_RF_RM, 0,
		      ARM_IVALUE_OPC1(0x12)|ARM_IVALUE_SBO(15, 12),
		      ARM_IMASK_RM|ARM_IMASK_SET(19, 16)|ARM_IMASK_PSR, 0),
	ARM_INST_RM_DECL("bx", 0,
			 ARM_IVALUE_OPC1(0x12)|ARM_IMASK_DECL(7, 4, 1)|
			 ARM_IVALUE_SBO(19, 8), 0, 0),
	ARM_INST_RDM_DECL("clz", 0,
			  ARM_IVALUE_OPC1(0x16)|ARM_IVALUE_SBO(19, 16)|
			  ARM_IVALUE_SBO(11, 8)|ARM_IMASK_DECL(7, 4, 1), 0, 0),
	ARM_INST_RM_DECL("bxj", 0,
			 ARM_IVALUE_OPC1(0x12)|ARM_IMASK_DECL(7, 4, 2)|
			 ARM_IVALUE_SBO(19, 8), 0, 0),
	ARM_INST_RM_DECL(inst_blx, 0,
			 ARM_IVALUE_OPC1(0x12)|ARM_IVALUE_SBO(19, 8)|
			 ARM_IMASK_DECL(7, 4, 3), 0, 0),
	ARM_INST_RDMN_DECL("qadd", 0,
			   ARM_IVALUE_OPC1(0x10)|ARM_IMASK_DECL(7, 4, 5), 0, 0),
	ARM_INST_RDMN_DECL("qsub", 0,
			   ARM_IVALUE_OPC1(0x12)|ARM_IMASK_DECL(7, 4, 5), 0, 0),
	ARM_INST_RDMN_DECL("qdadd", 0,
			   ARM_IVALUE_OPC1(0x14)|ARM_IMASK_DECL(7, 4, 5), 0, 0),
	ARM_INST_RDMN_DECL("qdsub", 0,
			   ARM_IVALUE_OPC1(0x16)|ARM_IMASK_DECL(7, 4, 5), 0, 0),
	ARM_INST_DECL("bkpt", "0x${x:19-8}${x:3-0}", 0,
		      ARM_IVALUE_COND_ALWAYS|ARM_IVALUE_OPC1(0x12)|
		      ARM_IMASK_DECL(7, 4, 7),
		      ARM_IMASK_SET(19, 8)|ARM_IMASK_SET(3, 0),
		      ARM_IF_NOCOND),
	ARM_INST_RNMSD_DECL(ARM_MN_XY("smla"), 0,
			    ARM_IVALUE_OPC1(0x10)|ARM_IMASK_SET(7, 7),
			    ARM_IMASK_SET(6, 5), 0),
	ARM_INST_RNMSD_DECL(ARM_MN_Y("smlaw"), 0,
			    ARM_IVALUE_OPC1(0x12)|ARM_IMASK_SET(7, 7),
			    ARM_IMASK_SET(6, 6), 0),
	ARM_INST_RNMS_DECL(ARM_MN_Y("smulw"), 0,
			   ARM_IVALUE_OPC1(0x12)|ARM_IMASK_SET(7, 7)|
			   ARM_IMASK_SET(5, 5), ARM_IMASK_SET(6, 6), 0),
	ARM_INST_RDNMS_DECL(ARM_MN_XY("smlal"), 0,
			    ARM_IVALUE_OPC1(0x14)|ARM_IMASK_SET(7, 7),
			    ARM_IMASK_SET(6, 5), 0),
	ARM_INST_RNMS_DECL(ARM_MN_XY("smul"), 0,
			   ARM_IVALUE_OPC1(0x16)|ARM_IMASK_SET(7, 7),
			   ARM_IMASK_SET(6, 5), 0),

	/* Multiplication expansion */
	ARM_INST_RNMS_DECL("mul${c}${s:20:s}", 0,
			   ARM_IVALUE_OPC(0x0)|ARM_IMASK_DECL(7, 4, 9),
			   ARM_IMASK_PSR_S, 0),
	ARM_INST_RNMSD_DECL("mla${c}${s:20:s}", 0,
			    ARM_IVALUE_OPC(0x1)|ARM_IMASK_DECL(7, 4, 9),
			    ARM_IMASK_PSR_S, 0),
	ARM_INST_RDNMS_DECL("umull${c}${s:20:s}", 0,
			    ARM_IVALUE_OPC(0x4)|ARM_IMASK_DECL(7, 4, 9),
			    ARM_IMASK_PSR_S, 0),
	ARM_INST_RDNMS_DECL("umlal${c}${s:20:s}", 0,
			    ARM_IVALUE_OPC(0x5)|ARM_IMASK_DECL(7, 4, 9),
			    ARM_IMASK_PSR_S, 0),
	ARM_INST_RDNMS_DECL("smull${c}${s:20:s}", 0,
			    ARM_IVALUE_OPC(0x6)|ARM_IMASK_DECL(7, 4, 9),
			    ARM_IMASK_PSR_S, 0),
	ARM_INST_RDNMS_DECL("smlal${c}${s:20:s}", 0,
			    ARM_IVALUE_OPC(0x7)|ARM_IMASK_DECL(7, 4, 9),
			    ARM_IMASK_PSR_S, 0),
	ARM_INST_RDNMS_DECL("umaal${c}", 0,
			    ARM_IVALUE_OPC1(0x4)|ARM_IMASK_DECL(7, 4, 9),
			    0, 0),

	/* Data processing */
	/* mov r0, r0 should be treated as no operation. */
	ARM_INST_DECL("nop", "(mov r0, r0)", 0,
		      ARM_IMASK_DECL(31, 20, 0xe1a), 0, ARM_IF_NOCOND),

	/*
	 * Load/Store expansion
	 */

	/* Load/Store exclusive */
	ARM_INST_LDREX_DECL("ldrex", ARM_IVALUE_OPC(0xc)),
	ARM_INST_LDREX_DECL("ldrexd", ARM_IVALUE_OPC(0xd)),
	ARM_INST_LDREX_DECL("ldrexb", ARM_IVALUE_OPC(0xe)),
	ARM_INST_LDREX_DECL("ldrexh", ARM_IVALUE_OPC(0xf)),
	ARM_INST_STREX_DECL("strex", ARM_IVALUE_OPC(0xc)),
	ARM_INST_STREX_DECL("strexd", ARM_IVALUE_OPC(0xd)),
	ARM_INST_STREX_DECL("strexb", ARM_IVALUE_OPC(0xe)),
	ARM_INST_STREX_DECL("strexh", ARM_IVALUE_OPC(0xf)),

	/* Swap */
	ARM_INST_DECL("swp${c}${s:22:b}", ARM_OP_SWP, 0,
		      ARM_IVALUE_SWP, ARM_IMASK_SWP, 0),

	/* Halfword addressing format */
	ARM_INST_RD_HALF_ADM_DECL("ldr${c}d", 0, 0xd, 0, 0),
	ARM_INST_RD_HALF_ADM_DECL("str${c}d", 0, 0xf, 0, 0),
	ARM_INST_RD_HALF_ADM_DECL(inst_ldrh, 1, 0x9, ARM_IMASK_SET(6, 5), 0),
	ARM_INST_RD_HALF_ADM_DECL(inst_strh, 0, 0xb, 0, 0),

	/*
	 * Legacy instructions (must comes last)
	 */
	ARM_INST_RD_SHIFT_DECL(inst_mov, 0, ARM_IVALUE_OPC(0xd), 0, 0),
	ARM_INST_RD_SHIFT_DECL(inst_mvn, 0, ARM_IVALUE_OPC(0xf), 0, 0),

	ARM_INST_RN_SHIFT_DECL(inst_tst, 0,
			       ARM_IVALUE_OPC(0x8)|ARM_IMASK_PSR_S, 0, 0),
	ARM_INST_RN_SHIFT_DECL(inst_teq, 0,
			       ARM_IVALUE_OPC(0x9)|ARM_IMASK_PSR_S, 0, 0),
	ARM_INST_RN_SHIFT_DECL(inst_cmp, 0,
			       ARM_IVALUE_OPC(0xa)|ARM_IMASK_PSR_S, 0, 0),
	ARM_INST_RN_SHIFT_DECL(inst_cmn, 0,
			       ARM_IVALUE_OPC(0xb)|ARM_IMASK_PSR_S, 0, 0),

	ARM_INST_RDN_SHIFT_DECL(inst_and, 0, ARM_IVALUE_OPC(0x0), 0, 0),
	ARM_INST_RDN_SHIFT_DECL(inst_eor, 0, ARM_IVALUE_OPC(0x1), 0, 0),
	ARM_INST_RDN_SHIFT_DECL(inst_sub, 0, ARM_IVALUE_OPC(0x2), 0, 0),
	ARM_INST_RDN_SHIFT_DECL(inst_rsb, 0, ARM_IVALUE_OPC(0x3), 0, 0),
	ARM_INST_RDN_SHIFT_DECL(inst_add, 0, ARM_IVALUE_OPC(0x4), 0, 0),
	ARM_INST_RDN_SHIFT_DECL(inst_adc, 0, ARM_IVALUE_OPC(0x5), 0, 0),
	ARM_INST_RDN_SHIFT_DECL(inst_sbc, 0, ARM_IVALUE_OPC(0x6), 0, 0),
	ARM_INST_RDN_SHIFT_DECL(inst_rsc, 0, ARM_IVALUE_OPC(0x7), 0, 0),
	ARM_INST_RDN_SHIFT_DECL(inst_orr, 0, ARM_IVALUE_OPC(0xc), 0, 0),
	ARM_INST_RDN_SHIFT_DECL(inst_bic, 0, ARM_IVALUE_OPC(0xe), 0, 0),
};

/* Region 1 */
static const arm_inst_t	v6_inst_reg1[] = {
	/* Undefined */
	ARM_INST_DECL(NULL, NULL, 1,
		      ARM_IMASK_DECL(27, 23, 0x6),
		      ARM_IMASK_SET(22, 22)|ARM_IMASK_SET(19, 0),
		      ARM_IF_UNDEF),

	/* ARM V6K instructions */
	ARM_INST_DECL("yield", empty, 1, 0x120f001, 0, 0),
	ARM_INST_DECL("wfe", empty, 1, 0x120f002, 0, 0),
	ARM_INST_DECL("wfi", empty, 1, 0x120f003, 0, 0),
	ARM_INST_DECL("sev", empty, 1, 0x120f004, 0, 0),
	ARM_INST_DECL("nop", "${d:7-0}", 1, 0x120f000, ARM_IMASK_SET(7, 0), 0),

	/* Immediate data processing */

	/* Legacy instructions (must comes last) */
	ARM_INST_RD_SHIFT_DECL(inst_mov, 1, ARM_IVALUE_OPC(0xd), 0, 0),
	ARM_INST_RD_SHIFT_DECL(inst_mvn, 1, ARM_IVALUE_OPC(0xf), 0, 0),

	ARM_INST_RN_SHIFT_DECL(inst_tst, 1,
			       ARM_IVALUE_OPC(0x8)|ARM_IMASK_PSR_S, 0, 0),
	ARM_INST_RN_SHIFT_DECL(inst_teq, 1,
			       ARM_IVALUE_OPC(0x9)|ARM_IMASK_PSR_S, 0, 0),
	ARM_INST_RN_SHIFT_DECL(inst_cmp, 1,
			       ARM_IVALUE_OPC(0xa)|ARM_IMASK_PSR_S, 0, 0),
	ARM_INST_RN_SHIFT_DECL(inst_cmn, 1,
			       ARM_IVALUE_OPC(0xb)|ARM_IMASK_PSR_S, 0, 0),

	ARM_INST_RDN_SHIFT_DECL(inst_and, 1, ARM_IVALUE_OPC(0x0), 0, 0),
	ARM_INST_RDN_SHIFT_DECL(inst_eor, 1, ARM_IVALUE_OPC(0x1), 0, 0),
	ARM_INST_RDN_SHIFT_DECL(inst_sub, 1, ARM_IVALUE_OPC(0x2), 0, 0),
	ARM_INST_RDN_SHIFT_DECL(inst_rsb, 1, ARM_IVALUE_OPC(0x3), 0, 0),
	ARM_INST_RDN_SHIFT_DECL(inst_add, 1, ARM_IVALUE_OPC(0x4), 0, 0),
	ARM_INST_RDN_SHIFT_DECL(inst_adc, 1, ARM_IVALUE_OPC(0x5), 0, 0),
	ARM_INST_RDN_SHIFT_DECL(inst_sbc, 1, ARM_IVALUE_OPC(0x6), 0, 0),
	ARM_INST_RDN_SHIFT_DECL(inst_rsc, 1, ARM_IVALUE_OPC(0x7), 0, 0),
	ARM_INST_RDN_SHIFT_DECL(inst_orr, 1, ARM_IVALUE_OPC(0xc), 0, 0),
	ARM_INST_RDN_SHIFT_DECL(inst_bic, 1, ARM_IVALUE_OPC(0xe), 0, 0),
};

/* Region 2 */
static const arm_inst_t	v6_inst_reg2[] = {
	ARM_INST_RD_ADM_DECL(inst_ldr, 2, ARM_IMASK_ADM_L, 0, 0),
	ARM_INST_RD_ADM_DECL(inst_str, 2, 0, 0, 0),
};

/* Region 3 */
static const arm_inst_t	v6_inst_reg3[] = {
	/* Undefined */
	ARM_INST_DECL(NULL, NULL, 3,
		      ARM_IMASK_DECL(27, 20, 0x7f)|ARM_IMASK_SET(7, 4),
		      ARM_IMASK_SET(19, 8)|ARM_IMASK_SET(3, 0), ARM_IF_UNDEF),

	/* Media instructions */
	ARM_INST_RDNM_DECL("sel", 3,
			   ARM_IVALUE_OPC1(0x8)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 11), 0, 0),

	ARM_INST_RDNM_DECL("qadd16", 3,
			   ARM_IVALUE_OPC1(0x2)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 1), 0, 0),
	ARM_INST_RDNM_DECL("qadd8", 3,
			   ARM_IVALUE_OPC1(0x2)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 9), 0, 0),
	ARM_INST_RDNM_DECL("qaddsubx", 3,
			   ARM_IVALUE_OPC1(0x2)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 3), 0, 0),
	ARM_INST_RDNM_DECL("qsub16", 3,
			   ARM_IVALUE_OPC1(0x2)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 7), 0, 0),
	ARM_INST_RDNM_DECL("qsub8", 3,
			   ARM_IVALUE_OPC1(0x2)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 15), 0, 0),
	ARM_INST_RDNM_DECL("qsubaddx", 3,
			   ARM_IVALUE_OPC1(0x2)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 5), 0, 0),

	ARM_INST_RDNM_DECL("sadd16", 3,
			   ARM_IVALUE_OPC1(0x1)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 1), 0, 0),
	ARM_INST_RDNM_DECL("sadd8", 3,
			   ARM_IVALUE_OPC1(0x1)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 9), 0, 0),
	ARM_INST_RDNM_DECL("saddsubx", 3,
			   ARM_IVALUE_OPC1(0x1)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 3), 0, 0),
	ARM_INST_RDNM_DECL("ssub16", 3,
			   ARM_IVALUE_OPC1(0x1)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 7), 0, 0),
	ARM_INST_RDNM_DECL("ssub8", 3,
			   ARM_IVALUE_OPC1(0x1)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 15), 0, 0),
	ARM_INST_RDNM_DECL("ssubaddx", 3,
			   ARM_IVALUE_OPC1(0x1)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 5), 0, 0),

	ARM_INST_RDNM_DECL("shadd16", 3,
			   ARM_IVALUE_OPC1(0x3)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 1), 0, 0),
	ARM_INST_RDNM_DECL("shadd8", 3,
			   ARM_IVALUE_OPC1(0x3)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 9), 0, 0),
	ARM_INST_RDNM_DECL("shaddsubx", 3,
			   ARM_IVALUE_OPC1(0x3)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 3), 0, 0),
	ARM_INST_RDNM_DECL("shsub16", 3,
			   ARM_IVALUE_OPC1(0x3)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 7), 0, 0),
	ARM_INST_RDNM_DECL("shsub8", 3,
			   ARM_IVALUE_OPC1(0x3)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 15), 0, 0),
	ARM_INST_RDNM_DECL("shsubaddx", 3,
			   ARM_IVALUE_OPC1(0x3)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 5), 0, 0),

	ARM_INST_RDNM_DECL("uadd16", 3,
			   ARM_IVALUE_OPC1(0x5)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 1), 0, 0),
	ARM_INST_RDNM_DECL("uadd8", 3,
			   ARM_IVALUE_OPC1(0x5)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 9), 0, 0),
	ARM_INST_RDNM_DECL("uaddsubx", 3,
			   ARM_IVALUE_OPC1(0x5)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 3), 0, 0),
	ARM_INST_RDNM_DECL("usub16", 3,
			   ARM_IVALUE_OPC1(0x5)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 7), 0, 0),
	ARM_INST_RDNM_DECL("usub8", 3,
			   ARM_IVALUE_OPC1(0x5)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 15), 0, 0),
	ARM_INST_RDNM_DECL("usubaddx", 3,
			   ARM_IVALUE_OPC1(0x5)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 5), 0, 0),

	ARM_INST_RDNM_DECL("uhadd16", 3,
			   ARM_IVALUE_OPC1(0x7)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 1), 0, 0),
	ARM_INST_RDNM_DECL("uhadd8", 3,
			   ARM_IVALUE_OPC1(0x7)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 9), 0, 0),
	ARM_INST_RDNM_DECL("uhaddsubx", 3,
			   ARM_IVALUE_OPC1(0x7)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 3), 0, 0),
	ARM_INST_RDNM_DECL("uhsub16", 3,
			   ARM_IVALUE_OPC1(0x7)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 7), 0, 0),
	ARM_INST_RDNM_DECL("uhsub8", 3,
			   ARM_IVALUE_OPC1(0x7)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 15), 0, 0),
	ARM_INST_RDNM_DECL("uhsubaddx", 3,
			   ARM_IVALUE_OPC1(0x7)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 5), 0, 0),

	ARM_INST_RDNM_DECL("uqadd16", 3,
			   ARM_IVALUE_OPC1(0x6)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 1), 0, 0),
	ARM_INST_RDNM_DECL("uqadd8", 3,
			   ARM_IVALUE_OPC1(0x6)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 9), 0, 0),
	ARM_INST_RDNM_DECL("uqaddsubx", 3,
			   ARM_IVALUE_OPC1(0x6)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 3), 0, 0),
	ARM_INST_RDNM_DECL("uqsub16", 3,
			   ARM_IVALUE_OPC1(0x6)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 7), 0, 0),
	ARM_INST_RDNM_DECL("uqsub8", 3,
			   ARM_IVALUE_OPC1(0x6)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 15), 0, 0),
	ARM_INST_RDNM_DECL("uqsubaddx", 3,
			   ARM_IVALUE_OPC1(0x6)|ARM_IVALUE_SBO(11, 8)|
			   ARM_IMASK_DECL(7, 4, 5), 0, 0),

	ARM_INST_RDNM_DECL("pkhbt", 3,
			   ARM_IVALUE_OPC1(0x8)|ARM_IMASK_DECL(6, 4, 1), 0, 0),
	ARM_INST_DECL("pkhbt", ARM_OP_PKHBT, 3,
		      ARM_IVALUE_OPC1(0x8)|ARM_IMASK_DECL(6, 4, 1),
		      ARM_IMASK_SET(11, 7)|ARM_IMASK_RDNM, 0),
	ARM_INST_DECL("pkhtb", ARM_OP_PKHTB_ASR32, 3,
		      ARM_IVALUE_OPC1(0x8)|ARM_IMASK_DECL(6, 4, 5),
		      ARM_IMASK_RDNM, 0),
	ARM_INST_DECL("pkhtb", ARM_OP_PKHTB, 3,
		      ARM_IVALUE_OPC1(0x8)|ARM_IMASK_DECL(6, 4, 5),
		      ARM_IMASK_SET(11, 7)|ARM_IMASK_RDNM, 0),

	ARM_INST_DECL("ssat", ARM_OP_SSAT, 3,
		      ARM_IVALUE_OPC(0x5)|ARM_IMASK_DECL(6, 4, 1),
		      ARM_IMASK_SET(20, 16)|ARM_IMASK_RDM, 0),
	ARM_INST_DECL("ssat", ARM_OP_SSAT_LSL, 3,
		      ARM_IVALUE_OPC(0x5)|ARM_IMASK_DECL(6, 4, 1),
		      ARM_IMASK_SET(20, 16)|ARM_IMASK_RDM|ARM_IMASK_SET(11, 7),
		      0),
	ARM_INST_DECL("ssat", ARM_OP_SSAT_ASR32, 3,
		      ARM_IVALUE_OPC(0x5)|ARM_IMASK_DECL(6, 4, 5),
		      ARM_IMASK_SET(20, 16)|ARM_IMASK_RDM, 0),
	ARM_INST_DECL("ssat", ARM_OP_SSAT_ASR, 3,
		      ARM_IVALUE_OPC(0x5)|ARM_IMASK_DECL(6, 4, 5),
		      ARM_IMASK_SET(20, 16)|ARM_IMASK_RDM|ARM_IMASK_SET(11, 7),
		      0),

	ARM_INST_DECL("ssat16", op_sat16, 3,
		      ARM_IVALUE_OPC1(0xa)|ARM_IMASK_DECL(7, 4, 3)|
		      ARM_IVALUE_SBO(11, 8),
		      ARM_IMASK_SET(19, 16)|ARM_IMASK_RDM, 0),

	ARM_INST_DECL("usat", ARM_OP_USAT, 3,
		      ARM_IVALUE_OPC(0x7)|ARM_IMASK_DECL(6, 4, 1),
		      ARM_IMASK_SET(20, 16)|ARM_IMASK_RDM, 0),
	ARM_INST_DECL("usat", ARM_OP_USAT_LSL, 3,
		      ARM_IVALUE_OPC(0x7)|ARM_IMASK_DECL(6, 4, 1),
		      ARM_IMASK_SET(20, 16)|ARM_IMASK_RDM|ARM_IMASK_SET(11, 7),
		      0),
	ARM_INST_DECL("usat", ARM_OP_USAT_ASR32, 3,
		      ARM_IVALUE_OPC(0x7)|ARM_IMASK_DECL(6, 4, 5),
		      ARM_IMASK_SET(20, 16)|ARM_IMASK_RDM, 0),
	ARM_INST_DECL("usat", ARM_OP_USAT_ASR, 3,
		      ARM_IVALUE_OPC(0x7)|ARM_IMASK_DECL(6, 4, 5),
		      ARM_IMASK_SET(20, 16)|ARM_IMASK_RDM|ARM_IMASK_SET(11, 7),
		      0),

	ARM_INST_DECL("usat16", op_sat16, 3,
		      ARM_IVALUE_OPC1(0xe)|ARM_IMASK_DECL(7, 4, 3)|
		      ARM_IVALUE_SBO(11, 8),
		      ARM_IMASK_SET(19, 16)|ARM_IMASK_RDM, 0),

	/* Sign or zero expansion */
	ARM_INST_XT_DECL("sxtb", 0xa),
	ARM_INST_XT_DECL("sxtb16", 0x8),
	ARM_INST_XT_DECL("sxth", 0xb),
	ARM_INST_XT_DECL("uxtb", 0xe),
	ARM_INST_XT_DECL("uxtb16", 0xc),
	ARM_INST_XT_DECL("uxth", 0xf),

	ARM_INST_XTA_DECL("sxtab", 0xa),
	ARM_INST_XTA_DECL("sxtab16", 0x8),
	ARM_INST_XTA_DECL("sxtah", 0xb),
	ARM_INST_XTA_DECL("uxtab", 0xe),
	ARM_INST_XTA_DECL("uxtab16", 0xc),
	ARM_INST_XTA_DECL("uxtah", 0xf),

	/* Unsigned absolute value addition */
	ARM_INST_RNMS_DECL("usad8", 3,
			   ARM_IVALUE_OPC1(0x8)|ARM_IMASK_DECL(7, 4, 1)|
			   ARM_IMASK_SET(15, 12), 0, 0),
	ARM_INST_RNMSD_DECL("usada8", 3,
			    ARM_IVALUE_OPC1(0x8)|ARM_IMASK_DECL(7, 4, 1),
			    0, 0),

	/* Signed dual instructions */
	ARM_INST_RNMSD_DECL("smlad${s:5:x}", 3,
			    ARM_IVALUE_OPC1(0x10)|ARM_IMASK_SET(4, 4),
			    ARM_IMASK_SET(5, 5), 0),
	ARM_INST_RDNMS_DECL("smlald${s:5:x}", 3,
			    ARM_IVALUE_OPC1(0x14)|ARM_IMASK_SET(4, 4),
			    ARM_IMASK_SET(5, 5), 0),
	ARM_INST_RNMSD_DECL("smlsd${s:5:x}", 3,
			    ARM_IVALUE_OPC1(0x10)|ARM_IMASK_DECL(7, 6, 1)|
			    ARM_IMASK_SET(4, 4),
			    ARM_IMASK_SET(5, 5), 0),
	ARM_INST_RDNMS_DECL("smlsld${s:5:x}", 3,
			    ARM_IVALUE_OPC1(0x14)|ARM_IMASK_DECL(7, 6, 1)|
			    ARM_IMASK_SET(4, 4),
			    ARM_IMASK_SET(5, 5), 0),
	ARM_INST_RNMS_DECL("smuad${s:5:x}", 3,
			   ARM_IVALUE_OPC1(0x10)|ARM_IMASK_SET(15, 12)|
			   ARM_IMASK_SET(4, 4), ARM_IMASK_SET(5, 5), 0),
	ARM_INST_RNMS_DECL("smusd${s:5:x}", 3,
			   ARM_IVALUE_OPC1(0x10)|ARM_IMASK_SET(15, 12)|
			   ARM_IMASK_DECL(7, 6, 1)|ARM_IMASK_SET(4, 4),
			   ARM_IMASK_SET(5, 5), 0),

	/* Most significant word operations */
	ARM_INST_RNMSD_DECL("smmla${s:5:r}", 3,
			    ARM_IVALUE_OPC1(0x15)|ARM_IMASK_SET(4, 4),
			    ARM_IMASK_SET(5, 5), 0),
	ARM_INST_RNMSD_DECL("smmls${s:5:r}", 3,
			    ARM_IVALUE_OPC1(0x15)|ARM_IMASK_SET(7, 6)|
			    ARM_IMASK_SET(4, 4),
			    ARM_IMASK_SET(5, 5), 0),
	ARM_INST_RNMS_DECL("smmul${s:5:r}", 3,
			   ARM_IVALUE_OPC1(0x15)|ARM_IMASK_SET(15, 12)|
			   ARM_IMASK_SET(4, 4),
			   ARM_IMASK_SET(5, 5), 0),

	/* Byte swapping */
	ARM_INST_RDM_DECL("rev", 3,
			  ARM_IVALUE_OPC1(0xb)|ARM_IVALUE_SBO(19, 16)|
			  ARM_IVALUE_SBO(11, 8)|ARM_IMASK_DECL(7, 4, 3), 0, 0),
	ARM_INST_RDM_DECL("rev16", 3,
			  ARM_IVALUE_OPC1(0xb)|ARM_IVALUE_SBO(19, 16)|
			  ARM_IVALUE_SBO(11, 8)|ARM_IMASK_DECL(7, 4, 11), 0, 0),
	ARM_INST_RDM_DECL("revsh", 3,
			  ARM_IVALUE_OPC1(0xf)|ARM_IVALUE_SBO(19, 16)|
			  ARM_IVALUE_SBO(11, 8)|ARM_IMASK_DECL(7, 4, 11), 0, 0),

	/* Load/Store by register offset */
	ARM_INST_RD_ADM_DECL(inst_ldr, 3, ARM_IMASK_ADM_L, 0, 0),
	ARM_INST_RD_ADM_DECL(inst_str, 3, 0, 0, 0),
};

/* Region 4 */
static const arm_inst_t	v6_inst_reg4[] = {
	/* Load/Store multiple */
	ARM_INST_MULTI_DECL("ldm", ARM_IMASK_ADM_L),
	ARM_INST_MULTI_DECL("stm", 0),
};

/* Region 5 */
static const arm_inst_t	v6_inst_reg5[] = {
	/* Branch (immediate 24) */
	ARM_INST_IMMED24_DECL("b${s:24:l}", "${b}", 0, 0),
	ARM_INST_IMMED24_DECL(inst_blx, "${B}", 0, 0),
};

/* Region 6 */
static const arm_inst_t v6_inst_reg6[] = {
	/* Multible co-processor transfer */
	ARM_INST_MCPR_DECL("mcrr", ARM_IVALUE_OPC1(0x4), 0, 0),
	ARM_INST_MCPR_DECL("mrrc", ARM_IVALUE_OPC1(0x5), 0, 0),

	/* Co-processor load/store */
	ARM_INST_LDC_DECL("ldc${c}${s:22:l}", 1, 0),
	ARM_INST_LDC_DECL("stc${c}${s:22:l}", 0, 0),
};

/* Region 7 */
static const arm_inst_t v6_inst_reg7[] = {
	/* Co-processor transfer */
	ARM_INST_CPR_DECL("mcr", op_cpr, ARM_IMASK_SET(4, 4), 0, 0),
	ARM_INST_CPR_DECL("mrc", op_cpr,
			  ARM_IMASK_SET(4, 4)|ARM_IMASK_ADM_L, 0, 0),
	ARM_INST_DECL("cdp", op_cpr_cdp, 7, 0,
		      ARM_IMASK_SET(23, 5)|ARM_IMASK_SET(3, 0), 0),

	/* Software interrupt */
	ARM_INST_DECL("swi", "0x${x:23-0}", 7, ARM_IMASK_SET(24, 24),
		      ARM_IMASK_SET(23, 0), 0),
};

/* VFP v2 region 6 */
static const arm_inst_t	vfp2_inst_reg6[] = {
	ARM_INST_VFP_FLDD_DECL("fldd", 6,
			       ARM_IVALUE_OPC1(0x11)|
			       ARM_IMASK_DECL(11, 8, 0xb), 0, 0),
	ARM_INST_VFP_FLDS_DECL("flds", 6,
			       ARM_IVALUE_OPC1(0x11)|
			       ARM_IMASK_DECL(11, 8, 0xa), 0, 0),

	ARM_INST_VFP_DMRDRN_DECL("fmdrr", 6,
				 ARM_IVALUE_SBO(22, 22)|
				 ARM_IMASK_DECL(11, 4, 0xb1), 0, 0),
	ARM_INST_VFP_RDRNDM_DECL("fmrrd", 6,
				 ARM_IVALUE_OPC1(0x5)|
				 ARM_IMASK_DECL(11, 4, 0xb1), 0, 0),
	ARM_INST_VFP_RDRNFM1_DECL("fmrrs", 6,
				  ARM_IVALUE_OPC1(0x5)|
				  ARM_IMASK_DECL(11, 4, 0xa1), 0, 0),
	ARM_INST_VFP_FM1RDRN_DECL("fmsrr", 6,
				  ARM_IVALUE_SBO(22, 22)|
				  ARM_IMASK_DECL(11, 4, 0xa1), 0, 0),
	ARM_INST_VFP_FLDD_DECL("fstd", 6,
			       ARM_IVALUE_OPC1(0x10)|
			       ARM_IMASK_DECL(11, 8, 0xb), 0, 0),
	ARM_INST_VFP_FLDS_DECL("fsts", 6,
			       ARM_IVALUE_OPC1(0x10)|
			       ARM_IMASK_DECL(11, 8, 0xa), 0, 0),

	/*
	 * Multiple load/store must be tested at last.
	 * In addition, fldmx/fstmx must be tested before fldmd/fstmd.
	 */
	ARM_INST_VFP_FLDMX_DECL("fldm${m}x", 6,
				ARM_IVALUE_SBO(20, 20)|
				ARM_IMASK_DECL(11, 8, 0xb), 0, 0),
	ARM_INST_VFP_FLDMX_DECL("fstm${m}x", 6,
				ARM_IMASK_DECL(11, 8, 0xb), 0, 0),

	ARM_INST_VFP_FLDMD_DECL("fldm${m}d", 6,
				ARM_IVALUE_SBO(20, 20)|
				ARM_IMASK_DECL(11, 8, 0xb), 0, 0),
	ARM_INST_VFP_FLDMS_DECL("fldm${m}s", 6,
				ARM_IVALUE_SBO(20, 20)|
				ARM_IMASK_DECL(11, 8, 0xa), 0, 0),
	ARM_INST_VFP_FLDMD_DECL("fstm${m}d", 6,
				ARM_IMASK_DECL(11, 8, 0xb), 0, 0),
	ARM_INST_VFP_FLDMS_DECL("fstm${m}s", 6,
				ARM_IMASK_DECL(11, 8, 0xa), 0, 0),
};

/* VFP v2 region 7 */
static const arm_inst_t	vfp2_inst_reg7[] = {
	/* At first, fmstat must be tested. */
	ARM_INST_DECL("fmstat", "", 7, ARM_IMASK_DECL(24, 0, 0xf1fa10), 0, 0),

	ARM_INST_VFP_DDM_DECL("fabsd", 7,
			      ARM_IVALUE_OPC1(0xb)|ARM_IMASK_DECL(11, 4, 0xbc),
			      0, 0),
	ARM_INST_VFP_FDM_DECL("fabss", 7,
			      ARM_IMASK_DECL(24, 23, 1)|
			      ARM_IMASK_DECL(21, 16, 0x30)|
			      ARM_IMASK_DECL(11, 4, 0xac), 0, 0),
	ARM_INST_VFP_DDNM_DECL("faddd", 7,
			       ARM_IVALUE_OPC1(3)|ARM_IMASK_DECL(11, 4, 0xb0),
			       0, 0),
	ARM_INST_VFP_FDNM_DECL("fadds", 7,
			       ARM_IVALUE_SBO(21, 20)|
			       ARM_IMASK_DECL(11, 8, 0xa), 0, 0),
	ARM_INST_VFP_DDM_DECL("fcmpd", 7,
			      ARM_IMASK_DECL(24, 16, 0xb4)|
			      ARM_IMASK_DECL(11, 4, 0xb4), 0, 0),
	ARM_INST_VFP_FDM_DECL("fcmpes", 7,
			      ARM_IMASK_DECL(24, 23, 1)|
			      ARM_IMASK_DECL(21, 16, 0x34)|
			      ARM_IMASK_DECL(11, 4, 0xac),  0, 0),
	ARM_INST_VFP_DD_DECL("fcmpezd", 7,
			     ARM_IMASK_DECL(24, 16, 0xb5)|
			     ARM_IMASK_DECL(11, 4, 0xbc),  0, 0),
	ARM_INST_VFP_FD_DECL("fcmpezs", 7,
			     ARM_IMASK_DECL(24, 16, 0xb5)|
			     ARM_IMASK_DECL(11, 4, 0xac), 0, 0),
	ARM_INST_VFP_FDM_DECL("fcmps", 7,
			      ARM_IMASK_DECL(24, 16, 0xb4)|
			      ARM_IMASK_DECL(11, 4, 0xa4), 0, 0),
	ARM_INST_VFP_DD_DECL("fcmpzd", 7,
			     ARM_IMASK_DECL(24, 16, 0xb5)|
			     ARM_IMASK_DECL(11, 4, 0xb4),  0, 0),
	ARM_INST_VFP_FD_DECL("fcmpzs", 7,
			     ARM_IMASK_DECL(24, 16, 0xb5)|
			     ARM_IMASK_DECL(11, 4, 0xa4), 0, 0),
	ARM_INST_VFP_DDM_DECL("fcpyd", 7,
			      ARM_IMASK_DECL(24, 16, 0xb0)|
			      ARM_IMASK_DECL(11, 4, 0xb4), 0, 0),
	ARM_INST_VFP_FDM_DECL("fcpys", 7,
			      ARM_IMASK_DECL(24, 16, 0xb0)|
			      ARM_IMASK_DECL(11, 4, 0xa4), 0, 0),
	ARM_INST_VFP_DDFM_DECL("fcvtds", 7,
			       ARM_IMASK_DECL(24, 16, 0xb7)|
			       ARM_IMASK_DECL(11, 4, 0xac), 0, 0),
	ARM_INST_VFP_FDDM_DECL("fcvtsd", 7,
			       ARM_IMASK_DECL(24, 16, 0xb7)|
			       ARM_IMASK_DECL(11, 4, 0xbc), 0, 0),
	ARM_INST_VFP_DDNM_DECL("fdivd", 7,
			       ARM_IVALUE_OPC1(8)|
			       ARM_IMASK_DECL(11, 4, 0xb0), 0, 0),
	ARM_INST_VFP_FDNM_DECL("fdivs", 7,
			       ARM_IMASK_DECL(24, 23, 1)|
			       ARM_IMASK_DECL(11, 4, 0xa0), 0, 0),
	ARM_INST_VFP_DDNM_DECL("fmacd", 7, ARM_IMASK_DECL(11, 4, 0xb0),
			       0, 0),
	ARM_INST_VFP_FDNM_DECL("fmacs", 7, ARM_IMASK_DECL(11, 4, 0xa0),
			       0, 0),
	ARM_INST_VFP_DNRD_DECL("fmdhr", 7,
			       ARM_IVALUE_OPC1(2)|ARM_IMASK_DECL(11, 4, 0xb1),
			       0, 0),
	ARM_INST_VFP_DNRD_DECL("fmdlr", 7, ARM_IMASK_DECL(11, 4, 0xb1), 0, 0),
	ARM_INST_VFP_RDDN_DECL("fmrdh", 7,
			       ARM_IVALUE_SBO(21, 20)|
			       ARM_IMASK_DECL(11, 4, 0xb1), 0, 0),
	ARM_INST_VFP_RDDN_DECL("fmrdl", 7,
			       ARM_IVALUE_SBO(20, 20)|
			       ARM_IMASK_DECL(11, 4, 0xb1), 0, 0),
	ARM_INST_VFP_RDFN_DECL("fmrs", 7,
			       ARM_IVALUE_SBO(20, 20)|
			       ARM_IMASK_DECL(11, 4, 0xa1), 0, 0),
	ARM_INST_VFP_FMRX_DECL("fmrx", ARM_RF_RD ", ${V}",
			       ARM_IVALUE_OPC1(0xf)|
			       ARM_IMASK_DECL(11, 4, 0xa1), 0, 0),
	ARM_INST_VFP_DDNM_DECL("fmscd", 7,
			       ARM_IVALUE_SBO(20, 20)|
			       ARM_IMASK_DECL(11, 4, 0xb0), 0, 0),
	ARM_INST_VFP_FDNM_DECL("fmscs", 7,
			       ARM_IVALUE_SBO(20, 20)|
			       ARM_IMASK_DECL(11, 4, 0xa0), 0, 0),
	ARM_INST_VFP_FNRD_DECL("fmsr", 7, ARM_IMASK_DECL(11, 4, 0xa1), 0, 0),
	ARM_INST_VFP_DDNM_DECL("fmuld", 7,
			       ARM_IVALUE_SBO(21, 21)|
			       ARM_IMASK_DECL(11, 4, 0xb0), 0, 0),
	ARM_INST_VFP_FDNM_DECL("fmuls", 7,
			       ARM_IVALUE_SBO(21, 21)|
			       ARM_IMASK_DECL(11, 4, 0xa0), 0, 0),
	ARM_INST_VFP_FMRX_DECL("fmxr", "${V}, " ARM_RF_RD,
			       ARM_IVALUE_OPC1(0xe)|
			       ARM_IMASK_DECL(11, 4, 0xa1), 0, 0),
	ARM_INST_VFP_DDM_DECL("fnegd", 7,
			      ARM_IMASK_DECL(24, 16, 0xb1)|
			      ARM_IMASK_DECL(11, 4, 0xb4), 0, 0),
	ARM_INST_VFP_FDM_DECL("fnegs", 7,
			      ARM_IMASK_DECL(24, 16, 0xb1)|
			      ARM_IMASK_DECL(11, 4, 0xa4), 0, 0),
	ARM_INST_VFP_DDNM_DECL("fnmacd", 7, ARM_IMASK_DECL(11, 4, 0xb4), 0, 0),
	ARM_INST_VFP_FDNM_DECL("fnmacs", 7, ARM_IMASK_DECL(11, 4, 0xa4), 0, 0),
	ARM_INST_VFP_DDNM_DECL("fnmscd", 7,
			       ARM_IVALUE_SBO(20, 20)|
			       ARM_IMASK_DECL(11, 4, 0xb4), 0, 0),
	ARM_INST_VFP_FDNM_DECL("fnmscs", 7,
			       ARM_IVALUE_SBO(20, 20)|
			       ARM_IMASK_DECL(11, 4, 0xa4), 0, 0),
	ARM_INST_VFP_DDNM_DECL("fnmuld", 7,
			       ARM_IVALUE_SBO(21, 21)|
			       ARM_IMASK_DECL(11, 4, 0xb4), 0, 0),
	ARM_INST_VFP_FDNM_DECL("fnmuls", 7,
			       ARM_IVALUE_SBO(21, 21)|
			       ARM_IMASK_DECL(11, 4, 0xa4), 0, 0),
	ARM_INST_VFP_DDFM_DECL("fsitod", 7,
			       ARM_IMASK_DECL(24, 16, 0xb8)|
			       ARM_IMASK_DECL(11, 4, 0xbc), 0, 0),
	ARM_INST_VFP_FDM_DECL("fsitos", 7,
			      ARM_IMASK_DECL(24, 16, 0xb8)|
			      ARM_IMASK_DECL(11, 4, 0xac), 0, 0),
	ARM_INST_VFP_DDM_DECL("fsqrtd", 7,
			      ARM_IMASK_DECL(24, 16, 0xb1)|
			      ARM_IMASK_DECL(11, 4, 0xbc), 0, 0),
	ARM_INST_VFP_FDM_DECL("fsqrts", 7,
			      ARM_IMASK_DECL(24, 16, 0xb1)|
			      ARM_IMASK_DECL(11, 4, 0xac), 0, 0),
	ARM_INST_VFP_DDNM_DECL("fsubd", 7,
			       ARM_IVALUE_SBO(21, 20)|
			       ARM_IMASK_DECL(11, 4, 0xb4), 0, 0),
	ARM_INST_VFP_FDNM_DECL("fsubs", 7,
			       ARM_IVALUE_SBO(21, 20)|
			       ARM_IMASK_DECL(11, 4, 0xa4), 0, 0),
	ARM_INST_VFP_FDDM_DECL("ftosi${s:7:z}d", 7,
			       ARM_IMASK_DECL(24, 16, 0xbd)|
			       ARM_IMASK_DECL(11, 4, 0xb4),
			       ARM_IVALUE_SBO(7, 7), 0),
	ARM_INST_VFP_FDM_DECL("ftosi${s:7:z}s", 7,
			      ARM_IMASK_DECL(24, 16, 0xbd)|
			      ARM_IMASK_DECL(11, 4, 0xa4),
			      ARM_IVALUE_SBO(7, 7), 0),
	ARM_INST_VFP_FDDM_DECL("ftoui${s:7:z}d", 7,
			       ARM_IMASK_DECL(24, 16, 0xbc)|
			       ARM_IMASK_DECL(11, 4, 0xb4),
			       ARM_IVALUE_SBO(7, 7), 0),
	ARM_INST_VFP_FDM_DECL("ftoui${s:7:z}s", 7,
			      ARM_IMASK_DECL(24, 16, 0xbc)|
			      ARM_IMASK_DECL(11, 4, 0xa4),
			      ARM_IVALUE_SBO(7, 7), 0),
	ARM_INST_VFP_DDFM_DECL("fuitod", 7,
			       ARM_IMASK_DECL(24, 16, 0xb8)|
			       ARM_IMASK_DECL(11, 4, 0xb4), 0, 0),
	ARM_INST_VFP_FDM_DECL("fuitos", 7,
			      ARM_IMASK_DECL(24, 16, 0xb8)|
			      ARM_IMASK_DECL(11, 4, 0xa4), 0, 0),
};

/* Append character to the output buffer */
#define	DIS_OUTPUT(buf, endp, c, err)		\
	do {					\
		if ((buf) >= (endp)) {		\
			(err) = EFAULT;		\
		}				\
		else {				\
			*(buf) = (c);		\
			(buf)++;		\
		}				\
	} while (0)

/* Append string to the output buffer */
#define	DIS_OUTPUT_STR(buf, endp, str, err)				\
	do {								\
		char	*__p;						\
		for (__p = (char *)(str); *__p != '\0'; __p++) {	\
			DIS_OUTPUT(buf, endp, *__p, err);		\
			if (err) {					\
				break;					\
			}						\
		}							\
	} while (0)

/* Append a part of string to the output buffer */
#define	DIS_OUTPUT_SUBSTR(buf, endp, str, strend, err)			\
	do {								\
		char	*__p;						\
		for (__p = (char *)(str); __p < (strend); __p++) {	\
			DIS_OUTPUT(buf, endp, *__p, err);		\
			if (err) {					\
				break;					\
			}						\
		}							\
	} while (0)

/* Calculate branch target address */
#define	DIS_BRANCH_TARGET(target, pc, inst, hval)		\
	do {							\
		uint32_t	__immed24;			\
		__immed24 = (inst) & ARM_IMASK_IMMED24;		\
		if (__immed24 & ARM_IMASK_SET(23, 23)) {	\
			/* Extend sign field */			\
			__immed24 |= ARM_IMASK_IMMED24_MINUS;	\
		}						\
		__immed24 <<= 2;				\
		__immed24 += (hval);				\
		(target) = (pc) + 8 + __immed24;		\
	} while (0)

/* Internal prototypes */
static const arm_inst_t	*dis_arm_inst_lookup(const arm_inst_t *op, uint_t nent,
					     uint32_t inst);
static void	dis_arm_parse_bitrange(const char **fmtpp, uint_t *hip,
				       uint_t *lop);
static int	dis_arm_output_dec(uint32_t value, char **bufpp, char *endp);
static int	dis_arm_output_hex(uint32_t value, char **bufpp, char *endp,
				   uint_t flags);
static int	dis_arm_output_shift(uint32_t inst, char **bufpp, char *endp);
static int	dis_arm_output_addrmode(uint32_t pc, uint32_t inst,
					char **bufpp, char *startp, char *endp,
					uint32_t *target, uint_t flags);
static int	dis_arm_output_half_addrmode(uint32_t pc, uint32_t inst,
					     char **bufpp, char *startp,
					     char *endp, uint32_t *target,
					     uint_t flags);
static int	dis_arm_output_ldc_addrmode(uint32_t inst, char **bufpp,
					    char *endp);
static int	dis_arm_output_reglist(uint32_t inst, char **bufpp, char *endp);
static int	dis_arm_output_vfp_reglist(uint32_t inst, char **bufpp,
					   char *endp, boolean_t single);
static int	dis_arm_output_comment(char **bufpp, char *startp, char *endp);
static int	dis_arm_parse_format(const char *fmt, uint32_t pc,
				     uint32_t inst, char **bufpp, char *endp,
				     uint32_t *target, boolean_t *condout,
				     uint_t flags);

#define	CHOOSE_INST_REGION(reg, op, nent)			\
	do {							\
		(op) = v6_inst_reg##reg;			\
		(nent) = ARM_INST_NENTRY(v6_inst_reg##reg);	\
	} while (0)

#define	CHOOSE_VFP_INST(reg, op, inst, flags)				\
	do {								\
		if (flags & DIS_ARM_STR_VFP2) {				\
			uint_t	__nent;					\
									\
			(op) = vfp2_inst_reg##reg;			\
			__nent = ARM_INST_NENTRY(vfp2_inst_reg##reg);	\
			(op) = dis_arm_inst_lookup(op, __nent, inst);	\
		}							\
		else {							\
			(op) = NULL;					\
		}							\
	} while (0)

/*
 * int
 * dis_arm_string(uint_t arch, uint32_t pc, uint32_t inst, char *buf,
 *		  size_t bufsize,  uint32_t *target, uint_t flags)
 *	Disassemble ARM instruction.
 *
 *	"arch" determines CPU architecture for the given instruction.
 *	If the given instruction is branch instruction, target address
 *	is set into *target, otherwise -1.
 *
 * Calling/Exit State:
 *	Upon successful completion, dis_arm_string() returns 0.
 *	String representation of instruction is set into *buf.
 *	Mnemonic and operands are separated by single tab character.
 *
 *	On error, returns error number that indicates the cause of error.
 *
 *		ENOENT		Undefined instruction
 *		EFAULT		buf is too small
 *
 * Remarks:
 *	Currently, dis_arm_string() ignores "arch".
 *	It always treats that architecture is MPCore, and instruction mode
 *	is ARM mode.
 */
/* ARGSUSED */
int
dis_arm_string(uint_t arch, uint32_t pc, uint32_t inst, char *buf,
	       size_t bufsize,  uint32_t *target, uint_t flags)
{
	int		i, err, nent, nwhite;
	char		*startp, *endp;
	boolean_t	condout;
	const arm_inst_t	*op;

	DIS_ASSERT(bufsize > 0);

	/* Determine appropriate arm_inst array. */
	if (INSTR_COND(inst) == 0xf) {
		/* Unconditional */
		op = v6_inst_nocond;
		nent = ARM_INST_NENTRY(v6_inst_nocond);
	}
	else {
		switch (INSTR_TYPE(inst)) {
		case 0:
			CHOOSE_INST_REGION(0, op, nent);
			break;

		case 1:
			CHOOSE_INST_REGION(1, op, nent);
			break;

		case 2:
			CHOOSE_INST_REGION(2, op, nent);
			break;

		case 3:
			CHOOSE_INST_REGION(3, op, nent);
			break;

		case 4:
			CHOOSE_INST_REGION(4, op, nent);
			break;

		case 5:
			CHOOSE_INST_REGION(5, op, nent);
			break;

		case 6:
			CHOOSE_VFP_INST(6, op, inst, flags);
			if (op != NULL) {
				goto found;
			}
			CHOOSE_INST_REGION(6, op, nent);
			break;

		case 7:
			CHOOSE_VFP_INST(7, op, inst, flags);
			if (op != NULL) {
				goto found;
			}
			CHOOSE_INST_REGION(7, op, nent);
			break;

		default:
			return ENOENT;
		}
	}

	/* Lookup definition for instruction. */
	for (i = 0; i < nent && (inst & op->ai_mask) != op->ai_value;
	     i++, op++);
	if (i == nent) {
		return ENOENT;
	}

found:
	if (op->ai_flags & ARM_IF_UNDEF) {
		/* Undefined instruction defined by architecture. */
		return ENOENT;
	}

	startp = buf;
	endp = buf + bufsize;

	/* Dump mnemonic. */
	(void)memset(buf, 0, bufsize);
	err = dis_arm_parse_format(op->ai_mnemonic, pc, inst, &buf, endp,
				   NULL, &condout, flags);
	if (err) {
		goto out;
	}

	if ((op->ai_flags & ARM_IF_NOCOND) == 0 && condout == B_FALSE) {
		uint32_t	cond;
		const char	*condstr;

		/* Append condition to mnemonic. */
		cond = INSTR_COND(inst);
		DIS_ASSERT(cond < 15);
		condstr = dis_cond[cond];
		DIS_OUTPUT_STR(buf, endp, condstr, err);
		if (err) {
			goto out;
		}
	}

	nwhite = buf - startp;
	nwhite = (nwhite >= DIS_INST_WIDTH)
		? DIS_INST_MINPAD : DIS_INST_WIDTH - nwhite;
	for (i = 0; i < nwhite; i++) {
		DIS_OUTPUT(buf, endp, ' ', err);
		if (err) {
			goto out;
		}
	}

	/* Dump operands. */
	err = dis_arm_parse_format(op->ai_operands, pc, inst, &buf, endp,
				   target, NULL, flags);

 out:
	if (buf >= endp) {
		buf = endp - 1;
	}
	*buf = '\0';
	return err;
}

/*
 * static const arm_inst_t *
 * dis_arm_inst_lookup(const arm_inst_t *op, uint_t nent, uint32_t inst)
 *	Lookup arm_inst array.
 *	It returns arm_inst_t pointer that contains instruction format
 *	of the given instruction. NULL is returned if not found.
 */
static const arm_inst_t *
dis_arm_inst_lookup(const arm_inst_t *op, uint_t nent, uint32_t inst)
{
	uint_t	i;

	for (i = 0; i < nent; i++, op++) {
		if ((inst & op->ai_mask) == op->ai_value) {
			return op;
		}
	}

	return NULL;
}

/*
 * static void
 * dis_arm_parse_bitrange(const char **fmtpp, uint_t *hip, uint_t *lop)
 *	Parse bit range in the format control string.
 *	fmtpp must be a pointer to format string, and it must point the
 *	start of bit range.
 *
 * Calling/Exit State:
 *	dis_arm_parse_bitrange() sets higher bit to *hip, and lower to *lop.
 *	If the bit range specifies a single bit, higher and lower bit
 *	will be the same value. And it also forwards format string pointer
 *	for the next parse.
 */
static void
dis_arm_parse_bitrange(const char **fmtpp, uint_t *hip, uint_t *lop)
{
	const char	*fmt = *fmtpp;
	uint_t		hi, lo;

	/* Parse higher bit number. */
	hi = 0;
	while (*fmt != '\0' && (*fmt >= '0' && *fmt <= '9')) {
		hi = (hi * 10) + (*fmt - '0');
		fmt++;
	}

	if (*fmt != DIS_CTRL_HYPHEN) {
		/* Low bit number is not defined. */
		DIS_ASSERT(*fmt == DIS_CTRL_RBRACE || *fmt == DIS_CTRL_COMMA);
		*fmtpp = fmt;
		*hip = hi;
		*lop = hi;
		return;
	}

	/* Parse lower bit number. */
	lo = 0;
	fmt++;
	while (*fmt != '\0' && (*fmt >= '0' && *fmt <= '9')) {
		lo = (lo * 10) + (*fmt - '0');
		fmt++;
	}

	DIS_ASSERT(*fmt == DIS_CTRL_RBRACE || *fmt == DIS_CTRL_COMMA);
	*fmtpp = fmt;
	*hip = hi;
	*lop = lo;
}

/*
 * static int
 * dis_arm_output_dec(uint32_t value, char **bufpp, char *endp)
 *	Store string representation of decimal value into string buffer.
 *	bufpp must be a pointer to a string buffer address. endp must be
 *	an end boundary of string buffer.
 *
 * Calling/Exit State:
 *	Upon successful completion, dis_arm_output_dec() returns 0.
 *	Otherwise error number that indicates the cause of error.
 */
static int
dis_arm_output_dec(uint32_t value, char **bufpp, char *endp)
{
	char		*buf = *bufpp;
	uint32_t	div = 1000000000, tmp;
	int		err = 0, out = 0, minus = 0;

	if ((int)value < 0) {
		minus = 1;
		value = (uint32_t)(-((int)value));
	}
	tmp = value;
	while (div > 0) {
		uint32_t	digit;

		digit = tmp / div;
		if (!out && div > 1 && digit == 0) {
			div /= 10;
			continue;
		}
		tmp -= (digit * div);
		div /= 10;
		if (!out && minus) {
			DIS_OUTPUT(buf, endp, '-', err);
			if (err) {
				break;
			}
		}
		DIS_OUTPUT(buf, endp, digit + '0', err);
		if (err) {
			break;
		}
		out = 1;
	}

	*bufpp = buf;
	return err;
}

/*
 * static int
 * dis_arm_output_hex(uint32_t value, char **bufpp, char *endp, uint_t flags)
 *	Store string representation of hexadecimal or octal value into string
 *	buffer. If DIS_ARM_STR_OCTAL is set in flags, octal number is dumped.
 *	bufpp must be a pointer to a string buffer address. endp must be
 *	an end boundary of string buffer.
 *
 * Calling/Exit State:
 *	Upon successful completion, dis_arm_output_hex() returns 0.
 *	Otherwise error number that indicates the cause of error.
 */
static int
dis_arm_output_hex(uint32_t value, char **bufpp, char *endp, uint_t flags)
{
	char		*buf = *bufpp;
	uint32_t	tmp = value;
	int		err = 0, out = 0, shift;

	if (flags & DIS_ARM_STR_OCTAL) {
		DIS_OUTPUT(buf, endp, '0', err);
		if (err) {
			*bufpp = buf;
			return err;
		}
		shift = 30;
		while (shift >= 0) {
			uint32_t	digit;
			char		c;

			digit = (tmp >> shift) & 0x7;
			if (!out && shift > 0 && digit == 0) {
				shift -= 3;
				continue;
			}
			shift -= 3;
			c = digit + '0';
			DIS_OUTPUT(buf, endp, c, err);
			if (err) {
				break;
			}
			out = 1;
		}
	}
	else {
		shift = 28;
		while (shift >= 0) {
			uint32_t	digit;
			char		c;

			digit = (tmp >> shift) & 0xf;
			if (!out && shift > 0 && digit == 0) {
				shift -= 4;
				continue;
			}
			shift -= 4;
			c = (digit >= 10) ? digit - 10 + 'a' : digit + '0';
			DIS_OUTPUT(buf, endp, c, err);
			if (err) {
				break;
			}
			out = 1;
		}
	}

	*bufpp = buf;
	return err;
}

/*
 * static int
 * dis_arm_output_shift(uint32_t inst, char **bufpp, char *endp)
 *	Store string representation of shifter operand in the specified
 *	instruction into string buffer.
 *	bufpp must be a pointer to a string buffer address. endp must be
 *	an end boundary of string buffer.
 *
 * Calling/Exit State:
 *	Upon successful completion, dis_arm_output_shift() returns 0.
 *	Otherwise error number that indicates the cause of error.
 */
static int
dis_arm_output_shift(uint32_t inst, char **bufpp, char *endp)
{
	char		*buf = *bufpp;
	int		rm, rs, err = 0;
	uint32_t	shift;

	rm = ARM_IMASK_GET_RM(inst);
	DIS_OUTPUT_STR(buf, endp, dis_greg[rm], err);
	if (err) {
		goto out;
	}

	shift = ARM_IMASK_GET(inst, 6, 5);
	if (inst & ARM_IMASK_SET(4, 4)) {
		/* Register shift */
		DIS_OUTPUT_STR(buf, endp, comma_space, err);
		if (err) {
			goto out;
		}
		rs = ARM_IMASK_GET_RS(inst);
		DIS_OUTPUT_STR(buf, endp, dis_shift[shift], err);
		if (err) {
			goto out;
		}
		DIS_OUTPUT(buf, endp, ' ', err);
		if (err) {
			goto out;
		}
		DIS_OUTPUT_STR(buf, endp, dis_greg[rs], err);
	}
	else {
		uint32_t	imm;

		/* Immediate shift */
		imm = ARM_IMASK_GET(inst, 11, 7);
		if (imm == 0) {
			if (shift == 3) {
				/* Extended right rotate shift. */
				DIS_OUTPUT_STR(buf, endp, ", rrx", err);
				goto out;
			}
			if (shift == 0) {
				/* No shift */
				goto out;
			}

			/* zero shift_imm is treated as 32bit shift */
			imm = 32;
		}

		DIS_OUTPUT_STR(buf, endp, comma_space, err);
		if (err) {
			goto out;
		}
		DIS_OUTPUT_STR(buf, endp, dis_shift[shift], err);
		if (err) {
			goto out;
		}
		DIS_OUTPUT_STR(buf, endp, " #", err);
		if (err) {
			goto out;
		}
		err = dis_arm_output_dec(imm, &buf, endp);
	}

 out:
	*bufpp = buf;
	return err;
}

/*
 * static int
 * dis_arm_output_addrmode(uint32_t pc, uint32_t inst, char **bufpp,
 *			   char *startp, char *endp, uint32_t *target,
 *			   uint_t flags)
 *	Store string representation of ARM addressing mode in the specified
 *	instruction into string buffer.
 *	bufpp must be a pointer to a string buffer address. endp must be
 *	an end boundary of string buffer.
 *
 *	pc must be an address of the instruction. If the operand of the
 *	specified instruction is pc (r15) relative format, the target address
 *	is set into *target.
 *
 * Calling/Exit State:
 *	Upon successful completion, dis_arm_output_addrmode() returns 0.
 *	Otherwise error number that indicates the cause of error.
 */
static int
dis_arm_output_addrmode(uint32_t pc, uint32_t inst, char **bufpp, char *startp,
			char *endp, uint32_t *target, uint_t flags)
{
	char		*buf = *bufpp;
	int		rn, dumpaddr = 0, addroff, sign = 1, err = 0;
	uint32_t	offset;

	rn = ARM_IMASK_GET_RN(inst);
	DIS_OUTPUT(buf, endp, '[', err);
	if (err) {
		goto out;
	}

	DIS_OUTPUT_STR(buf, endp, dis_greg[rn], err);
	if (err) {
		goto out;
	}
	if ((inst & ARM_IMASK_ADM_U) == 0) {
		sign = -1;
	}

	if (inst & ARM_IMASK_ADM_P) {
		/* Offset or pre-indexed */
		if (inst & ARM_IMASK_ADM_I) {
			/* Register or scaled register offset */
			DIS_OUTPUT_STR(buf, endp, comma_space, err);
			if (err) {
				goto out;
			}
			if (sign < 0) {
				DIS_OUTPUT(buf, endp, '-', err);
				if (err) {
					goto out;
				}
			}
			err = dis_arm_output_shift(inst, &buf, endp);
			if (err) {
				goto out;
			}
		}
		else {
			/* Immediate offset */
			offset = ARM_IMASK_GET(inst, 11, 0);
			if (rn == ARM_REG_PC) {
				dumpaddr = 1;
				addroff = pc + 8 + ((int)offset * sign);
			}
			if (offset) {
				DIS_OUTPUT_STR(buf, endp, ", #", err);
				if (err) {
					goto out;
				}
				offset *= sign;
				err = dis_arm_output_dec(offset, &buf, endp);
				if (err) {
					goto out;
				}
			}
		}
		DIS_OUTPUT(buf, endp, ']', err);
		if (err) {
			goto out;
		}
		if (inst & ARM_IMASK_ADM_W) {
			DIS_OUTPUT(buf, endp, '!', err);
			if (err) {
				goto out;
			}
		}
	}
	else {
		/* Post-indexed */
		if (inst & ARM_IMASK_ADM_I) {
			/* Register or scaled register offset */
			DIS_OUTPUT_STR(buf, endp, "], ", err);
			if (err) {
				goto out;
			}
			if (sign < 0) {
				DIS_OUTPUT(buf, endp, '-', err);
				if (err) {
					goto out;
				}
			}
			err = dis_arm_output_shift(inst, &buf, endp);
			if (err) {
				goto out;
			}
		}
		else {
			/* Immediate offset */
			offset = ARM_IMASK_GET(inst, 11, 0);
			if (rn == ARM_REG_PC) {
				dumpaddr = 1;
				addroff = pc + 8;
			}
			if (offset) {
				DIS_OUTPUT_STR(buf, endp, "], #", err);
				if (err) {
					goto out;
				}
				offset *= sign;
				err = dis_arm_output_dec(offset, &buf, endp);
				if (err) {
					goto out;
				}
			}
			else {
				DIS_OUTPUT(buf, endp, ']', err);
				if (err) {
					goto out;
				}
			}
		}
	}

	if (dumpaddr) {
		if ((err = dis_arm_output_comment(&buf, startp, endp)) != 0) {
			goto out;
		}
		DIS_OUTPUT_STR(buf, endp, "0x", err);
		if (err) {
			goto out;
		}
		err = dis_arm_output_hex(addroff, &buf, endp, flags);
		*target = addroff;
	}

 out:
	*bufpp = buf;
	return err;
}

/*
 * static int
 * dis_arm_output_half_addrmode(uint32_t pc, uint32_t inst, char **bufpp,
 *				char *startp, char *endp, uint32_t *target,
 *				uint_t flags)
 *	Store string representation of ARM addressing mode
 *	(halfword/signed/doubleword) in the specified instruction into
 *	string buffer.
 *	bufpp must be a pointer to a string buffer address. endp must be
 *	an end boundary of string buffer.
 *
 *	pc must be an address of the instruction. If the operand of the
 *	specified instruction is pc (r15) relative format, the target address
 *	is set into *target.
 *
 * Calling/Exit State:
 *	Upon successful completion, dis_arm_output_half_addrmode() returns 0.
 *	Otherwise error number that indicates the cause of error.
 */
static int
dis_arm_output_half_addrmode(uint32_t pc, uint32_t inst, char **bufpp,
			     char *startp, char *endp, uint32_t *target,
			     uint_t flags)
{
	char		*buf = *bufpp;
	int		rn, rm, dumpaddr = 0, addroff, sign = 1, err = 0;
	uint32_t	offset;

	rn = ARM_IMASK_GET_RN(inst);
	DIS_OUTPUT(buf, endp, '[', err);
	if (err) {
		goto out;
	}

	DIS_OUTPUT_STR(buf, endp, dis_greg[rn], err);
	if (err) {
		goto out;
	}
	if ((inst & ARM_IMASK_ADM_U) == 0) {
		sign = -1;
	}

	if (inst & ARM_IMASK_ADM_P) {
		/* Offset or pre-indexed */
		if (inst & ARM_IMASK_ADM_B) {
			/* Immediate */
			offset = ARM_IMASK_GET_HALF_IMMED(inst);
			if (rn == ARM_REG_PC) {
				dumpaddr = 1;
				addroff = pc + 8 + ((int)offset * sign);
			}
			if (offset) {
				DIS_OUTPUT_STR(buf, endp, ", #", err);
				if (err) {
					goto out;
				}
				offset *= sign;
				err = dis_arm_output_dec(offset, &buf, endp);
				if (err) {
					goto out;
				}
			}
		}
		else {
			/* Register */
			DIS_OUTPUT_STR(buf, endp, comma_space, err);
			if (err) {
				goto out;
			}
			if (sign < 0) {
				DIS_OUTPUT(buf, endp, '-', err);
				if (err) {
					goto out;
				}
			}
			rm = ARM_IMASK_GET_RM(inst);
			DIS_OUTPUT_STR(buf, endp, dis_greg[rm], err);
			if (err) {
				goto out;
			}
		}
		DIS_OUTPUT(buf, endp, ']', err);
		if (err) {
			goto out;
		}
		if (inst & ARM_IMASK_ADM_W) {
			DIS_OUTPUT(buf, endp, '!', err);
			if (err) {
				goto out;
			}
		}
	}
	else {
		/* Post-indexed */
		if (inst & ARM_IMASK_ADM_B) {
			/* Immediate */
			offset = ARM_IMASK_GET_HALF_IMMED(inst);
			if (rn == ARM_REG_PC) {
				dumpaddr = 1;
				addroff = pc + 8;
			}
			if (offset) {
				DIS_OUTPUT_STR(buf, endp, "], #", err);
				if (err) {
					goto out;
				}
				offset *= sign;
				err = dis_arm_output_dec(offset, &buf, endp);
				if (err) {
					goto out;
				}
			}
			else {
				DIS_OUTPUT(buf, endp, ']', err);
				if (err) {
					goto out;
				}
			}
		}
		else {
			/* Register */
			DIS_OUTPUT_STR(buf, endp, "], ", err);
			if (err) {
				goto out;
			}
			if (sign < 0) {
				DIS_OUTPUT(buf, endp, '-', err);
				if (err) {
					goto out;
				}
			}
			rm = ARM_IMASK_GET_RM(inst);
			DIS_OUTPUT_STR(buf, endp, dis_greg[rm], err);
			if (err) {
				goto out;
			}
		}
	}

	if (dumpaddr) {
		if ((err = dis_arm_output_comment(&buf, startp, endp)) != 0) {
			goto out;
		}
		DIS_OUTPUT_STR(buf, endp, "0x", err);
		if (err) {
			goto out;
		}
		err = dis_arm_output_hex(addroff, &buf, endp, flags);
		*target = addroff;
	}

 out:
	*bufpp = buf;
	return err;
}

/*
 * static int
 * dis_arm_output_ldc_addrmode(uint32_t inst, char **bufpp, char *endp)
 *	Store string representation of ARM addressing mode for ldc/stc
 *	in the specified instruction into string buffer.
 *	bufpp must be a pointer to a string buffer address. endp must be
 *	an end boundary of string buffer.
 *
 * Calling/Exit State:
 *	Upon successful completion, dis_arm_output_ldc_addrmode() returns 0.
 *	Otherwise error number that indicates the cause of error.
 */
static int
dis_arm_output_ldc_addrmode(uint32_t inst, char **bufpp, char *endp)
{
	char		*buf = *bufpp;
	int		rn, sign = 1, err = 0;
	uint32_t	offset;

	rn = ARM_IMASK_GET_RN(inst);
	DIS_OUTPUT(buf, endp, '[', err);
	if (err) {
		goto out;
	}

	DIS_OUTPUT_STR(buf, endp, dis_greg[rn], err);
	if (err) {
		goto out;
	}
	if ((inst & ARM_IMASK_ADM_U) == 0) {
		sign = -1;
	}
	offset = ARM_IMASK_GET(inst, 7, 0);

	if (inst & ARM_IMASK_ADM_P) {
		/* Offset or pre-indexed */
		if (offset) {
			DIS_OUTPUT_STR(buf, endp, ", #", err);
			if (err) {
				goto out;
			}
			offset *= 4;
			offset *= sign;
			err = dis_arm_output_dec(offset, &buf, endp);
			if (err) {
				goto out;
			}
			DIS_OUTPUT(buf, endp, ']', err);
			if (err) {
				goto out;
			}
			if (inst & ARM_IMASK_ADM_W) {
				DIS_OUTPUT(buf, endp, '!', err);
				if (err) {
					goto out;
				}
			}
		}
		else {
			DIS_OUTPUT(buf, endp, ']', err);
			if (err) {
				goto out;
			}
		}
	}
	else {
		/* Post-indexed or no index */
		DIS_OUTPUT(buf, endp, ']', err);
		if (err) {
			goto out;
		}
		if (inst & ARM_IMASK_ADM_W) {
			/* Post-indexed */
			if (offset) {
				DIS_OUTPUT_STR(buf, endp, ", #", err);
				if (err) {
					goto out;
				}
				offset *= 4;
				offset *= sign;
				err = dis_arm_output_dec(offset, &buf, endp);
				if (err) {
					goto out;
				}
			}
		}
		else {
			/* No index */
			DIS_OUTPUT_STR(buf, endp, ", {", err);
			if (err) {
				goto out;
			}
			err = dis_arm_output_dec(offset, &buf, endp);
			if (err) {
				goto out;
			}
			DIS_OUTPUT(buf, endp, '}', err);
			if (err) {
				goto out;
			}
		}
	}

 out:
	*bufpp = buf;
	return err;
}

/*
 * static int
 * dis_arm_output_reglist(uint32_t inst, char **bufpp, char *endp)
 *	Store string representation of ARM addressing mode for multiple
 *	register access in the specified instruction into string buffer.
 *	bufpp must be a pointer to a string buffer address. endp must be
 *	an end boundary of string buffer.
 *
 * Calling/Exit State:
 *	Upon successful completion, dis_arm_output_reglist() returns 0.
 *	Otherwise error number that indicates the cause of error.
 */
static int
dis_arm_output_reglist(uint32_t inst, char **bufpp, char *endp)
{
	char		*buf = *bufpp;
	int		i, err = 0, regstart;
	uint32_t	orginst = inst;
	const char	*sep = empty;

	DIS_OUTPUT(buf, endp, '{', err);
	if (err) {
		goto out;
	}

	/* Dump list of register ranges. */
	regstart = -1;
	inst &= ~(1 << ARM_SPECREG_START);
	for (i = 0; i <= ARM_SPECREG_START; i++) {
		if (inst & (1 << i)) {
			if (regstart < 0) {
				regstart = i;
			}
		}
		else if (regstart >= 0) {
			int		regend = i - 1;
			const char	*endreg = dis_greg[regend];

			DIS_OUTPUT_STR(buf, endp, sep, err);
			if (err) {
				goto out;
			}
			if (regstart == regend) {
				DIS_OUTPUT_STR(buf, endp, endreg, err);
				if (err) {
					goto out;
				}
			}
			else {
				const char	*startreg = dis_greg[regstart];

				DIS_ASSERT(regstart < regend);
				DIS_OUTPUT_STR(buf, endp, startreg, err);
				if (err) {
					goto out;
				}
				DIS_OUTPUT(buf, endp, '-', err);
				if (err) {
					goto out;
				}
				DIS_OUTPUT_STR(buf, endp, endreg, err);
				if (err) {
					goto out;
				}
			}
			sep = comma_space;
			regstart = -1;
		}
	}

	/* Dump sp, lr, and pc individually. */
	for (i = ARM_SPECREG_START; i < ARM_NGREG; i++) {
		if (orginst & (1 << i)) {
			DIS_OUTPUT_STR(buf, endp, sep, err);
			if (err) {
				goto out;
			}
			DIS_OUTPUT_STR(buf, endp, dis_greg[i], err);
			if (err) {
				goto out;
			}
			sep = comma_space;
		}
	}

	DIS_OUTPUT(buf, endp, '}', err);

 out:
	*bufpp = buf;
	return err;
}

/*
 * static int
 * dis_arm_output_reglist(uint32_t inst, char **bufpp, char *endp,
 *			  boolean_t single)
 *	Store string representation of VFP multiple register access
 *	in the specified instruction into string buffer.
 *	bufpp must be a pointer to a string buffer address. endp must be
 *	an end boundary of string buffer.
 *	If single is true, VFP registers is treated as single precision.
 *
 * Calling/Exit State:
 *	Upon successful completion, dis_arm_output_reglist() returns 0.
 *	Otherwise error number that indicates the cause of error.
 */
static int
dis_arm_output_vfp_reglist(uint32_t inst, char **bufpp, char *endp,
			   boolean_t single)
{
	char		*buf = *bufpp, regname;
	int		err = 0;
	uint_t		first, nregs;

	if (single) {
		/* Single precision */
		regname = 's';
		first = ARM_IMASK_GET_VFP_FD(inst);
		nregs = ARM_IMASK_GET(inst, 7, 0);
	}
	else {
		/* Double precision */
		regname = 'd';
		first = ARM_IMASK_GET_VFP_DD(inst);
		nregs = (ARM_IMASK_GET(inst, 7, 0) >> 1);
	}

	DIS_OUTPUT(buf, endp, '{', err);
	if (err) {
		goto out;
	}
	DIS_OUTPUT(buf, endp, regname, err);
	if (err) {
		goto out;
	}
	err = dis_arm_output_dec(first, &buf, endp);
	if (err) {
		goto out;
	}
	if (nregs > 1) {
		DIS_OUTPUT(buf, endp, '-', err);
		if (err) {
			goto out;
		}
		DIS_OUTPUT(buf, endp, regname, err);
		if (err) {
			goto out;
		}
		err = dis_arm_output_dec(first + nregs - 1, &buf, endp);
		if (err) {
			goto out;
		}
	}

	DIS_OUTPUT(buf, endp, '}', err);

out:
	*bufpp = buf;
	return err;
}

/*
 * static int
 * dis_arm_output_comment(char **bufpp, char *startp, char *endp)
 *	Dump comment mark.
 */
static int
dis_arm_output_comment(char **bufpp, char *startp, char *endp)
{
	char	*buf = *bufpp;
	int	nwhite, i, err = 0;

	nwhite = buf - startp;
	nwhite = (nwhite >= DIS_COMMENT_POSITION)
		? DIS_COMMENT_MINPAD : DIS_COMMENT_POSITION - nwhite;
	for (i = 0; i < nwhite; i++) {
		DIS_OUTPUT(buf, endp, ' ', err);
		if (err) {
			goto out;
		}
	}
	DIS_OUTPUT_STR(buf, endp, DIS_COMMENT_MARK, err);
	if (err) {
		goto out;
	}

out:
	*bufpp = buf;
	return err;
}

/*
 * static int
 * dis_arm_parse_format(const char *fmt, uint32_t pc, uint32_t inst,
 *			char **bufpp, char *endp, uint32_t *target,
 *			boolean_t *condout, uint_t flags)
 *	Dump instruction into the specified buffer according to the
 *	specified format control string.
 *	bufpp must be a pointer to a string buffer address. endp must be
 *	an end boundary of string buffer.
 *
 *	If the given instruction is a branch instruction, or load/store
 *	using pc (r15) relative addressing mode, the target address is set
 *	into *target.
 *
 *	If format control string contains the format for condition field
 *	("${c}"), B_TRUE is set into *condout, otherwise B_FALSE.
 *
 * Calling/Exit State:
 *	Upon successful completion, dis_arm_parse_format() returns 0.
 *	Otherwise error number that indicates the cause of error.
 */
static int
dis_arm_parse_format(const char *fmt, uint32_t pc, uint32_t inst, char **bufpp,
		     char *endp, uint32_t *target, boolean_t *condout,
		     uint_t flags)
{
	char		*buf = *bufpp, *startp;
	const char	*f = fmt;
	int		err = 0, pair;
	uint32_t	tgt = (uint32_t)-1;
	boolean_t	cout = B_FALSE;

	startp = buf;
	while (err == 0 && *f != '\0') {
		const char	*s1, *s2;
		char		c, fc;
		uint_t		hi, lo, regnum;
		uint32_t	cond, val, hval = 0;

		if (*f != DIS_CTRL_DOLLAR) {
			/* Normal string. */
			DIS_OUTPUT(buf, endp, *f, err);
			if (err) {
				break;
			}
			f++;
			continue;
		}
		f++;
		if (*f == DIS_CTRL_DOLLAR) {
			/* Quoted */
			DIS_OUTPUT(buf, endp, *f, err);
			if (err) {
				break;
			}
			f++;
			continue;
		}
		if (*f == '\0') {
			break;
		}
		if (*f != DIS_CTRL_LBRACE) {
			DIS_OUTPUT(buf, endp, *f, err);
			if (err) {
				break;
			}
			f++;
			continue;
		}
		f++;
		if (*f == '\0') {
			break;
		}

		fc = *f;
		f++;
		switch (fc) {
		case 'R':
			/* Convert general register. */
			switch (*f) {
			case 'm':
				regnum = ARM_IMASK_GET_RM(inst);
				break;

			case 's':
				regnum = ARM_IMASK_GET_RS(inst);
				break;

			case 'd':
				regnum = ARM_IMASK_GET_RD(inst);
				break;

			default:
				DIS_ASSERT(*f == 'n');
				regnum = ARM_IMASK_GET_RN(inst);
				break;
			}
			s1 = dis_greg[regnum];
			DIS_OUTPUT_STR(buf, endp, s1, err);
			f++;
			break;

		case 'D':
			/* VFP double precision register. */
			if (*f == 'M') {
				/* Multiple registers */
				err = dis_arm_output_vfp_reglist
					(inst, &buf, endp, B_FALSE);
				f++;
				break;
			}
			switch (*f) {
			case 'm':
				regnum = ARM_IMASK_GET_VFP_DM(inst);
				break;

			case 'd':
				regnum = ARM_IMASK_GET_VFP_DD(inst);
				break;

			default:
				DIS_ASSERT(*f == 'n');
				regnum = ARM_IMASK_GET_VFP_DN(inst);
				break;
			}
			DIS_OUTPUT(buf, endp, 'd', err);
			if (err) {
				break;
			}
			err = dis_arm_output_dec(regnum, &buf, endp);
			if (err) {
				break;
			}
			f++;
			break;

		case 'F':
			/* VFP single precision register. */
			if (*f == 'M') {
				/* Multiple registers */
				err = dis_arm_output_vfp_reglist
					(inst, &buf, endp, B_TRUE);
				f++;
				break;
			}

			pair = 0;
			switch (*f) {
			case 'P':
				pair = 1;
				DIS_OUTPUT(buf, endp, '{', err);
				if (err) {
					goto out;
				}
				/* FALLTHROUGH */
			case 'm':
				regnum = ARM_IMASK_GET_VFP_FM(inst);
				break;

			case 'd':
				regnum = ARM_IMASK_GET_VFP_FD(inst);
				break;

			default:
				DIS_ASSERT(*f == 'n');
				regnum = ARM_IMASK_GET_VFP_FN(inst);
				break;
			}

			DIS_OUTPUT(buf, endp, 's', err);
			if (err) {
				break;
			}
			err = dis_arm_output_dec(regnum, &buf, endp);
			if (err) {
				break;
			}
			if (pair) {
				uint_t	reg1 = regnum + 1;

				DIS_OUTPUT_STR(buf, endp, ", ", err);
				if (err) {
					break;
				}
				DIS_OUTPUT(buf, endp, 's', err);
				if (err) {
					break;
				}
				err = dis_arm_output_dec(reg1, &buf, endp);
				if (err) {
					break;
				}
				DIS_OUTPUT(buf, endp, '}', err);
				if (err) {
					break;
				}
			}
			f++;
			break;

		case 'a':
			/* Convert ARM addressing mode */
			err = dis_arm_output_addrmode(pc, inst, &buf, startp,
						      endp, &tgt, flags);
			break;

		case 'A':
			/* Convert addressing mode for ldc/stc */
			err = dis_arm_output_ldc_addrmode(inst, &buf, endp);
			break;

		case 'h':
			/* Convert halfword addressing mode */
			err = dis_arm_output_half_addrmode(pc, inst, &buf,
							   startp, endp, &tgt,
							   flags);
			break;

		case 'c':
			/* Convert mnemonic for condition field. */
			cond = INSTR_COND(inst);
			DIS_ASSERT(cond < 15);
			s1 = dis_cond[cond];
			DIS_OUTPUT_STR(buf, endp, s1, err);
			cout = B_TRUE;
			break;

		case 'C':
			/* Insert comment mark. */
			err = dis_arm_output_comment(&buf, startp, endp);
			break;

		case 'd':
			/* Decimal value */
			DIS_ASSERT(*f == DIS_CTRL_COMMA);
			f++;
			dis_arm_parse_bitrange(&f, &hi, &lo);
			val = ARM_IMASK_GET(inst, hi, lo);
			err = dis_arm_output_dec(val, &buf, endp);
			break;

		case 'p':
			/* Decimal value + 1 */
			DIS_ASSERT(*f == DIS_CTRL_COMMA);
			f++;
			dis_arm_parse_bitrange(&f, &hi, &lo);
			val = ARM_IMASK_GET(inst, hi, lo);
			err = dis_arm_output_dec(val + 1, &buf, endp);
			break;

		case 'x':
			/* Hex value */
			DIS_ASSERT(*f == DIS_CTRL_COMMA);
			f++;
			dis_arm_parse_bitrange(&f, &hi, &lo);
			val = ARM_IMASK_GET(inst, hi, lo);
			err = dis_arm_output_hex(val, &buf, endp, flags);
			break;

		case 's':
			/* Conditional string replace */
			DIS_ASSERT(*f == DIS_CTRL_COMMA);
			f++;
			dis_arm_parse_bitrange(&f, &hi, &lo);
			DIS_ASSERT(*f == DIS_CTRL_COMMA);
			f++;
			s1 = s2 = f;
			val = ARM_IMASK_GET(inst, hi, lo);
			if (val) {
				while (*s2 != DIS_CTRL_COMMA &&
				       *s2 != DIS_CTRL_RBRACE) {
					s2++;
				}
				f = s2;
				while (*f != DIS_CTRL_RBRACE) {
					f++;
				}
			}
			else {
				while (*s1 != DIS_CTRL_COMMA &&
				       *s1 != DIS_CTRL_RBRACE) {
					s1++;
				}
				if (*s1 == DIS_CTRL_COMMA) {
					s1++;
				}
				s2 = s1;
				while (*s2 != DIS_CTRL_RBRACE) {
					s2++;
				}
				f = s2;
			}
			DIS_OUTPUT_SUBSTR(buf, endp, s1, s2, err);
			break;

		case 'S':
			/* PSR field name */
			DIS_OUTPUT(buf, endp, '_', err);
			if (err) {
				break;
			}
			if (inst & ARM_IMASK_SET(19, 19)) {
				DIS_OUTPUT(buf, endp, 'f', err);
				if (err) {
					break;
				}
			}
			if (inst & ARM_IMASK_SET(18, 18)) {
				DIS_OUTPUT(buf, endp, 's', err);
				if (err) {
					break;
				}
			}
			if (inst & ARM_IMASK_SET(17, 17)) {
				DIS_OUTPUT(buf, endp, 'x', err);
				if (err) {
					break;
				}
			}
			if (inst & ARM_IMASK_SET(16, 16)) {
				DIS_OUTPUT(buf, endp, 'c', err);
			}
			break;

		case 'B':
			/* Branch target for blx. */
			hval = (1U << (inst & ARM_IMASK_SET(24, 24)));
			/* FALLTHROUGH */

		case 'b':
			/* Branch target for bl. */
			DIS_BRANCH_TARGET(tgt, pc, inst, hval);
			err = dis_arm_output_hex(tgt, &buf, endp, flags);
			break;

		case 'm':
			/*
			 * ldm/stm addressing mode
			 */

			/* Test U bit */
			c = (inst & ARM_IMASK_SET(23, 23)) ? 'i' : 'd';
			DIS_OUTPUT(buf, endp, c, err);
			if (err) {
				break;
			}

			/* Test P bit */
			c = (inst & ARM_IMASK_SET(24, 24)) ? 'b' : 'a';
			DIS_OUTPUT(buf, endp, c, err);
			if (err) {
				break;
			}
			break;

		case 'M':
			/* ldm/stm register list */
			err = dis_arm_output_reglist(inst, &buf, endp);
			break;

		case 'o':
			/* Shifter operand. */
			if (inst & ARM_IMASK_ADM_I) {
				uint32_t	rotate, immed8;

				/* 32bit immediate */
				rotate = ARM_IMASK_GET(inst, 11, 8) << 1;
				immed8 = ARM_IMASK_GET(inst, 7, 0);
				val = ((immed8 << (32 - rotate)) |
				       (immed8 >> rotate));
				DIS_OUTPUT_STR(buf, endp, "#", err);
				if (err) {
					break;
				}
				err = dis_arm_output_dec(val, &buf, endp);
				if (err) {
					break;
				}
				err = dis_arm_output_comment(&buf, startp,
							     endp);
				if (err) {
					break;
				}
				DIS_OUTPUT_STR(buf, endp, "0x", err);
				if (err) {
					break;
				}
				err = dis_arm_output_hex(val, &buf, endp,
							 flags);
			}
			else {
				/* Immediate shift or register shift */
				err = dis_arm_output_shift(inst, &buf, endp);
			}
			break;

		case 'r':
			/* Rotation field for sign or zero expansion */
			val = ARM_IMASK_GET(inst, 11, 10);
			if (val == 0) {
				/* Rotation field is omitted. */
				err = 0;
				break;
			}

			val *= 8;
			DIS_OUTPUT_STR(buf, endp, ", ror #", err);
			if (err) {
				break;
			}
			err = dis_arm_output_dec(val, &buf, endp);
			break;

		case 't':
			/* ldrt/strt */
			if ((inst & ARM_IMASK_ADM_P) == 0 &&
			    (inst & ARM_IMASK_ADM_W)) {
				DIS_OUTPUT(buf, endp, 't', err);
			}
			break;

		case 'V':
			/* VFP system register name */
			regnum = ARM_IMASK_GET(inst, 19, 16);
			if (regnum == 0) {
				DIS_OUTPUT_STR(buf, endp, "fpsid", err);
				if (err) {
					break;
				}
			}
			else if (regnum == 1) {
				DIS_OUTPUT_STR(buf, endp, "fpscr", err);
				if (err) {
					break;
				}
			}
			else if (regnum == 6) {
				DIS_OUTPUT_STR(buf, endp, "mvfr1", err);
				if (err) {
					break;
				}
			}
			else if (regnum == 7) {
				DIS_OUTPUT_STR(buf, endp, "mvfr0", err);
				if (err) {
					break;
				}
			}
			else if (regnum == 8) {
				DIS_OUTPUT_STR(buf, endp, "fpexc", err);
				if (err) {
					break;
				}
			}
			else if (regnum == 9) {
				DIS_OUTPUT_STR(buf, endp, "fpinst", err);
				if (err) {
					break;
				}
			}
			else if (regnum == 10) {
				DIS_OUTPUT_STR(buf, endp, "fpinst2", err);
				if (err) {
					break;
				}
			}
			else {
				if ((flags & DIS_ARM_STR_OCTAL) == 0) {
					DIS_OUTPUT_STR(buf, endp, "0x", err);
					if (err) {
						break;
					}
				}
				err = dis_arm_output_hex(val, &buf, endp,
							 flags);
			}
			break;
		}

		DIS_ASSERT(*f == DIS_CTRL_RBRACE);
		f++;
	}

out:
	*bufpp = buf;
	if (condout != NULL) {
		*condout = cout;
	}
	if (target != NULL) {
		*target = tgt;
	}
	return err;
}
