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
 * Copyright (c) 2006-2008 NEC Corporation
 * All rights reserved.
 */

#ifndef	_SYS_CONTROLREGS_H
#define	_SYS_CONTROLREGS_H

#pragma ident	"@(#)controlregs.h"

#ifndef _ASM
#include <sys/types.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This file desribes the ARM architecture control registers and
 * coprocessor registers. 
 */

/* Program Status Register */
#define	PSR_MODE	0x0000001f	/* Mode mask */
#define	PSR_MODE_USER	0x00000010	/* User mode */
#define	PSR_MODE_FIQ	0x00000011	/* FIQ mode */
#define	PSR_MODE_IRQ	0x00000012	/* IRQ mode */
#define	PSR_MODE_SVC	0x00000013	/* Supervisor mode */
#define	PSR_MODE_ABT	0x00000017	/* Abort mode */
#define	PSR_MODE_UND	0x0000001b	/* Undefined mode */
#define	PSR_MODE_SYS	0x0000001f	/* System mode */
#define	PSR_T_BIT	0x00000020	/* Thumb status bit */
#define	PSR_F_BIT	0x00000040	/* FIQ disable bit */
#define	PSR_I_BIT	0x00000080	/* IRQ disable bit */
#define	PSR_A_BIT	0x00000100	/* Imprecice abort bit */
#define	PSR_E_BIT	0x00000200	/* Data endianess bit */
#define	PSR_GE_MASK	0x000f0000	/* Greater than or equal to bits */
#define	PSR_GE_SHIFT	16
#define	PSR_J_BIT	0x01000000	/* Java bit */
#define	PSR_Q_BIT	0x08000000	/* Sticky overflow bit */
#define	PSR_V_BIT	0x10000000	/* Overflow bit */
#define	PSR_C_BIT	0x20000000	/* Carry/Borrow/Extend bit */
#define	PSR_Z_BIT	0x40000000	/* Zero bit */
#define	PSR_N_BIT	0x80000000	/* Negative/Less than bit */

#define PSR_USERINIT	PSR_MODE_USER	/* initial user psr */

#define	PSR_COND	\
	(PSR_N_BIT|PSR_Z_BIT|PSR_C_BIT|PSR_V_BIT)  /* condition code flags */

#define	PSR_USERMASK	\
	(PSR_COND|PSR_Q_BIT|PSR_GE_MASK|PSR_E_BIT)  /* user variable bits */

/* CP15 ID Code register */
#define	CP15_ID_IMPL		0xff000000	/* Implementor mask */
#define	CP15_ID_VARIANT		0x00f00000	/* Variant number mask */
#define	CP15_ID_ARCH		0x000f0000	/* Architectural format mask */
#define	CP15_ID_PARTNUM		0x0000fff0	/* Part number mask */
#define	CP15_ID_REVISION	0x0000000f	/* Revision number mask */

#define	CP15_ID_IMPL_SHIFT	24
#define	CP15_ID_VARIANT_SHIFT	20
#define	CP15_ID_ARCH_SHIFT	16
#define	CP15_ID_PARTNUM_SHIFT	4

#define	CP15_ID_ARM11MPCORE	0x410fb02f
#define	CP15_ID_IMPL_ARM	0x41

#define	ARM_IDCODE_IMPL(x)					\
			(((x) & CP15_ID_IMPL) >> CP15_ID_IMPL_SHIFT)
#define	ARM_IDCODE_VARIANT(x)					\
			(((x) & CP15_ID_VARIANT) >> CP15_ID_VARIANT_SHIFT)
#define	ARM_IDCODE_ARCH(x)					\
			(((x) & CP15_ID_ARCH) >> CP15_ID_ARCH_SHIFT)
#define	ARM_IDCODE_PARTNUM(x)					\
			(((x) & CP15_ID_PARTNUM) >> CP15_ID_PARTNUM_SHIFT)
#define	ARM_IDCODE_REVISION(x)	((x) & CP15_ID_REVISION) 

/* CPU ID Register (c0, c0, 5) */
#define	CPU15_CPUID_CPU_MASK	0xf

#define	ARM_CPUID_ID(x)		((x) & CPU15_CPUID_CPU_MASK)

/* Control Register (c1, c0, 0) */
#define	CTREG_M_BIT	0x00000001	/* MMU enable */
#define	CTREG_A_BIT	0x00000002	/* Data alignment fault enable */
#define	CTREG_C_BIT	0x00000004	/* L1 data cache enable */
#define	CTREG_SBZ1	0x000000f8	/* Should be zero */
#define	CTREG_S_BIT	0x00000100	/* System protection (deprecated) */
#define	CTREG_R_BIT	0x00000200	/* ROM protection (deprecated) */
#define	CTREG_SBZ2	0x00000400	/* Should be zero */
#define	CTREG_Z_BIT	0x00000800	/* Program flow prediction */
#define	CTREG_I_BIT	0x00001000	/* L1 instruction cache enable */
#define	CTREG_V_BIT	0x00002000	/* Location of exception vectors */
#define	CTREG_SBZ3	0x00004000	/* Should be zero */
#define	CTREG_L4_BIT	0x00008000	/* Config if load to PC set T bit */
#define	CTREG_U_BIT	0x00400000	/* Unaligned data access */
#define	CTREG_XP_BIT	0x00800000	/* Subpage AP bits enabled if 0 */
#define	CTREG_SBZ4	0x01000000	/* Should be zero */
#define	CTREG_EE_BIT	0x02000000	/* CPSR E bit on exception */
#define	CTREG_SBZ5	0x04000000	/* Should be zero */
#define	CTREG_NMFI_BIT	0x08000000	/* FIQs behaves NMFIs if 1 */
#define	CTREG_TEX_BIT	0x10000000	/* TEX remap bit */
#define	CTREG_AP_BIT	0x20000000	/* TLB AP[0]used as access bit if 1 */
#define	CTREG_SBZ6	0xc0000000	/* Should be zero */
#define	CTREG_SBZ							\
	(CTREG_SBZ1|CTREG_SBZ2|CTREG_SBZ3|CTREG_SBZ4|CTREG_SBZ5|CTREG_SBZ6)

/* Auxiliary Control Register (c1, c0, 1) */
#define	AUXCTL_RS_BIT	0x00000001	/* Return stack enable */
#define	AUXCTL_DB_BIT	0x00000002	/* Dynamic branch prediction enable */
#define	AUXCTL_SB_BIT	0x00000004	/* Static branch prediction enable */
#define	AUXCTL_F_BIT	0x00000008	/* Instruction folding enable */
#define	AUXCTL_EXCL_BIT	0x00000010	/* Exclusive L1 and L2 */
#define	AUXCTL_SMP_BIT	0x00000020	/* SMP/nAMP enable */

/* Coprocessor Access Control Register (c1, c0, 2) */
#define	CPCTL_CP10_FULL	0x00300000	/* cp10 full access */
#define	CPCTL_CP11_FULL	0x00c00000	/* cp11 full access */

/* Translation Table Base Register (c2, c0, 0/1) */
#define	TTB_SHARED	0x00000002	/* Shared page table walk */
#define	TTB_RGN(x)	((x) << 3)	/* Outer cachable attributes */
#define	TTB_RGN_UNCACHE	TTB_RGN(0)	/* Noncachable */
#define	TTB_RGN_WALLOC	TTB_RGN(1)	/* Write Allocate */
#define	TTB_RGN_WTHRU	TTB_RGN(2)	/* Write Through */
#define	TTB_RGN_NOALLOC	TTB_RGN(3)	/* No Allocate on Write */

/* Domain Access Control Register (c3, c0, 0) */
#define	DACR_FAULT	0		/* No access */
#define	DACR_CLIENT	1		/* Obey permissions in TLB entry */
#define	DACR_MANAGER	3		/* No permission check */

/*
 * Create value for DACR.
 * domain is domain number (0-15), type is DACR_XXX value.
 */
#define	DACR_VALUE(domain, type)	((type) << ((domain) << 1))

/* Context ID Register (c13, c0, 1) */
#define	CONTEXT_ID_ASID_BITS	8
#define	CONTEXT_ID_ASID_MASK	((1 << CONTEXT_ID_ASID_BITS) - 1)

#ifdef __cplusplus
}
#endif

#endif	/* !_SYS_CONTROLREGS_H */
