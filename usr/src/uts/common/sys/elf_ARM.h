/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 *	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
 *	  All Rights Reserved
 *
 *
 *	Copyright 2003 Sun Microsystems, Inc.  All rights reserved.
 *	Use is subject to license terms.
 */

/*
 * Copyright (c) 2006-2008 NEC Corporation
 */

#ifndef _SYS_ELF_ARM_H
#define	_SYS_ELF_ARM_H

#pragma ident	"@(#)elf_ARM.h"

#ifdef	__cplusplus
extern "C" {
#endif

#define	EF_ARM_RELEXEC		0x01
#define	EF_ARM_HASENTRY		0x02
#define	EF_ARM_INTERWORK	0x04
#define	EF_ARM_APCS_26		0x08
#define	EF_ARM_APCS_FLOAT	0x10
#define	EF_ARM_PIC		0x20
#define	EF_ARM_ALIGN8		0x40	/* 8-bit structure alignment is
					   in use.  */
#define	EF_ARM_NEW_ABI		0x80
#define	EF_ARM_OLD_ABI		0x100
#define	EF_ARM_SOFT_FLOAT	0x200
#define	EF_ARM_VFP_FLOAT	0x400
#define	EF_ARM_MAVERICK_FLOAT	0x800

#define	EF_ARM_EABIMASK		0xff000000	/* bits for version number */
#define	EF_ARM_BE8		0x00800000	/* contains BE-8 code */

#define	EF_ARM_EABI_VER1	0x01000000
#define	EF_ARM_EABI_VER2	0x02000000
#define	EF_ARM_EABI_VER3	0x03000000
#define	EF_ARM_EABI_VER4	0x04000000
#define	EF_ARM_EABI_VER5	0x05000000

#define	R_ARM_NONE		0		/* relocation type */
#define	R_ARM_PC24		1
#define	R_ARM_ABS32		2
#define	R_ARM_REL32		3
#define	R_ARM_LDR_PC_G0		4
#define	R_ARM_ABS16		5
#define	R_ARM_ABS12		6
#define	R_ARM_THM_ABS5		7
#define	R_ARM_ABS8		8
#define	R_ARM_SBREL32		9
#define	R_ARM_THM_CALL		10
#define	R_ARM_THM_PC8		11
#define	R_ARM_BREL_ADJ		12
#define	R_ARM_SWI24		13
#define	R_ARM_THM_SWI8		14
#define	R_ARM_XPC25		15
#define	R_ARM_THM_XPC22		16
#define	R_ARM_TLS_DTPMOD32	17
#define	R_ARM_TLS_DTPOFF32	18
#define	R_ARM_TLS_TPOFF32	19
#define	R_ARM_COPY		20
#define	R_ARM_GLOB_DAT		21
#define	R_ARM_JUMP_SLOT		22
#define	R_ARM_RELATIVE		23
#define	R_ARM_GOTOFF32		24
#define	R_ARM_BASE_PREL		25
#define	R_ARM_GOT_BREL		26
#define	R_ARM_PLT32		27
#define	R_ARM_CALL		28
#define	R_ARM_JUMP24		29
#define	R_ARM_THM_JUMP24	30
#define	R_ARM_BASE_ABS		31
#define	R_ARM_ALU_PCREL_7_0	32
#define	R_ARM_ALU_PCREL_15_8	33
#define	R_ARM_ALU_PCREL_23_15	34
#define	R_ARM_LDR_SBREL_11_0_NC	35
#define	R_ARM_ALU_SBREL_19_12_NC	36
#define	R_ARM_ALU_SBREL_27_20_CK	37
#define	R_ARM_TARGET1		38
#define	R_ARM_SBREL31		39
#define	R_ARM_V4BX		40
#define	R_ARM_TARGET2		41
#define	R_ARM_PREL31		42
#define	R_ARM_MOVW_ABS_NC	43
#define	R_ARM_MOVT_ABS		44
#define	R_ARM_MOVW_PREL_NC	45
#define	R_ARM_MOVT_PREL		46
#define	R_ARM_THM_MOVW_ABS_NC	47
#define	R_ARM_THM_MOVT_ABS	48
#define	R_ARM_THM_MOVW_PREL_NC	49
#define	R_ARM_THM_MOVT_PREL	50
#define	R_ARM_THM_JUMP19	51
#define	R_ARM_THM_JUMP6		52
#define	R_ARM_THM_ALU_PREL_11_0	53
#define	R_ARM_THM_PC12		54
#define	R_ARM_ABS32_NOI		55
#define	R_ARM_REL32_NOI		56
#define	R_ARM_ALU_PC_G0_NC	57
#define	R_ARM_ALU_PC_G0		58
#define	R_ARM_ALU_PC_G1_NC	59
#define	R_ARM_ALU_PC_G1		60
#define	R_ARM_ALU_PC_G2		61
#define	R_ARM_LDR_PC_G1		62
#define	R_ARM_LDR_PC_G2		63
#define	R_ARM_LDRS_PC_G0	64
#define	R_ARM_LDRS_PC_G1	65
#define	R_ARM_LDRS_PC_G2	66
#define	R_ARM_LDC_PC_G0		67
#define	R_ARM_LDC_PC_G1		68
#define	R_ARM_LDC_PC_G2		69
#define	R_ARM_ALU_SB_G0_NC	70
#define	R_ARM_ALU_SB_G0		71
#define	R_ARM_ALU_SB_G1_NC	72
#define	R_ARM_ALU_SB_G1		73
#define	R_ARM_ALU_SB_G2		74
#define	R_ARM_LDR_SB_G0		75
#define	R_ARM_LDR_SB_G1		76
#define	R_ARM_LDR_SB_G2		77
#define	R_ARM_LDRS_SB_G0	78
#define	R_ARM_LDRS_SB_G1	79
#define	R_ARM_LDRS_SB_G2	80
#define	R_ARM_LDC_SB_G0		81
#define	R_ARM_LDC_SB_G1		82
#define	R_ARM_LDC_SB_G2		83
#define	R_ARM_MOVW_BREL_NC	84
#define	R_ARM_MOVT_BREL		85
#define	R_ARM_MOVW_BREL		86
#define	R_ARM_THM_MOVW_BREL_NC	87
#define	R_ARM_THM_MOVT_BREL	88
#define	R_ARM_THM_MOVW_BREL	89
#define	R_ARM_TLS_GOTDESC	90
#define	R_ARM_TLS_CALL		91
#define	R_ARM_TLS_DESCSEQ	92
#define	R_ARM_THM_TLS_CALL	93
#define	R_ARM_PLT32_ABS		94
#define	R_ARM_GOT_ABS		95
#define	R_ARM_GOT_PREL		96
#define	R_ARM_GOT_BREL12	97
#define	R_ARM_GOTOFF12		98
#define	R_ARM_GOTRELAX		99
#define	R_ARM_GNU_VTENTRY	100
#define	R_ARM_GNU_VTINHERIT	101
#define	R_ARM_THM_JUMP11	102
#define	R_ARM_THM_JUMP8		103
#define	R_ARM_TLS_GD32		104
#define	R_ARM_TLS_LDM32		105
#define	R_ARM_TLS_LDO32		106
#define	R_ARM_TLS_IE32		107
#define	R_ARM_TLS_LE32		108
#define	R_ARM_TLS_LDO12		109
#define	R_ARM_TLS_LE12		110
#define	R_ARM_TLS_IE12GP	111
#define	R_ARM_PRIVATE_0		112
#define	R_ARM_PRIVATE_1		113
#define	R_ARM_PRIVATE_2		114
#define	R_ARM_PRIVATE_3		115
#define	R_ARM_PRIVATE_4		116
#define	R_ARM_PRIVATE_5		117
#define	R_ARM_PRIVATE_6		118
#define	R_ARM_PRIVATE_7		119
#define	R_ARM_PRIVATE_8		120
#define	R_ARM_PRIVATE_9		121
#define	R_ARM_PRIVATE_10	122
#define	R_ARM_PRIVATE_11	123
#define	R_ARM_PRIVATE_12	124
#define	R_ARM_PRIVATE_13	125
#define	R_ARM_PRIVATE_14	126
#define	R_ARM_PRIVATE_15	127
#define	R_ARM_ME_TOO		128

#define	R_ARM_NUM		129		/* must be >last */

#define	ELF_ARM_MAXPGSZ		0x8000		/* maximum page size */

/*
 * Processor specific section types
 */
#define	SHT_ARM_EXIDX		0x70000001	/* exception index table */
#define	SHT_ARM_PREEMPTMAP	0x70000002	/* BPABI DLL dyanmic linking */
						/*   pre-emption map */
#define	SHT_ARM_ATTRIBUTES	0x70000003	/* Object file compatibility */
						/*   attributes */

#define	SHF_ORDERED		0x40000000
#define	SHF_EXCLUDE		0x80000000

#define	SHN_BEFORE		0xff00
#define	SHN_AFTER		0xff01

/*
 * Processor specific segment types
 */
#define	PT_ARM_ARCHEXT			0x70000000
#define	PT_ARM_EXIDX			0x70000001
#define	PT_ARM_UNWIND			PT_ARM_EXIDX

#define	PT_ARM_ARCHEXT_FMTMSK		0xff000000
#define	PT_ARM_ARCHEXT_PROFMSK		0x00ff0000
#define	PT_ARM_ARCHEXT_ARCHMSK		0x0000ffff

#define	PT_ARM_ARCHEXT_FMT_OS		0x00000000

#define	PT_ARM_ARCHEXT_PROF_NONE	0x00000000
#define	PT_ARM_ARCHEXT_PROF_ARM		0x00410000
#define	PT_ARM_ARCHEXT_PROF_RT		0x00520000
#define	PT_ARM_ARCHEXT_PROF_MC		0x004d0000

#define	PT_ARM_ARCHEXT_ARCH_UNKN	0x00000000
#define	PT_ARM_ARCHEXT_ARCHv4		0x00000001
#define	PT_ARM_ARCHEXT_ARCHv4T		0x00000002
#define	PT_ARM_ARCHEXT_ARCHv5T		0x00000003
#define	PT_ARM_ARCHEXT_ARCHv5TE		0x00000004
#define	PT_ARM_ARCHEXT_ARCHv5TEJ	0x00000005
#define	PT_ARM_ARCHEXT_ARCHv6		0x00000006
#define	PT_ARM_ARCHEXT_ARCHv6KZ		0x00000007
#define	PT_ARM_ARCHEXT_ARCHv6T2		0x00000008
#define	PT_ARM_ARCHEXT_ARCHv6K		0x00000009
#define	PT_ARM_ARCHEXT_ARCHv7		0x0000000a

/*
 * ARM-specific dynamic array tags
 */
#define	DT_ARM_RESERVED1	0x70000000
#define	DT_ARM_SYMTABSZ		0x70000001
#define	DT_ARM_PREEMPTMAP	0x70000002
#define	DT_ARM_RESERVED2	0x70000003

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ELF_ARM_H */
