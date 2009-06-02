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
 * Copyright (c) 2006-2007 NEC Corporation
 * All rights reserved.
 */


#ident	"@(#)instr_size.c"

#include <sys/proc.h>
#include <sys/privregs.h>


#define	INSTR_TYPE(instr)	(((instr) >> 25) & 0x07) /* bit[27:25] */

#define	INSTR_b0to3(instr)	((instr) & 0x0f)         /* bit[3:0]   */
#define	INSTR_b4to7(instr)	(((instr) >> 4) & 0x0f)  /* bit[7:4]   */
#define	INSTR_b8to11(instr)	(((instr) >> 8) & 0x0f)  /* bit[11:8]  */
#define	INSTR_b16to19(instr)	(((instr) >> 12) & 0x0f) /* bit[19:16] */
#define	INSTR_b20to27(instr)	(((instr) >> 20) & 0xff) /* bit[27:20] */
#define	INSTR_b20to21(instr)	(((instr) >> 20) & 0x03) /* bit[21:20] */
#define	INSTR_b21to22(instr)	(((instr) >> 21) & 0x03) /* bit[22:21] */
#define	INSTR_b21to27(instr)	(((instr) >> 21) & 0x7f) /* bit[27:21] */
#define	INSTR_b24to27(instr)	(((instr) >> 24) & 0x0f) /* bit[27:24] */
#define	INSTR_b28to31(instr)	(((instr) >> 28) & 0x0f) /* bit[28:31] */

#define	INSTR_b21and24(instr)	(((instr) >> 21) & 0x09) /* bit[24] bit[21] */

#define	INSTR_b4(instr)		(((instr) >> 4) & 0x01)  /* bit[4]  */
#define	INSTR_b15(instr)	(((instr) >> 15) & 0x01) /* bit[15] */
#define	INSTR_b20(instr)	(((instr) >> 20) & 0x01) /* bit[20] */
#define	INSTR_b24(instr)	(((instr) >> 24) & 0x01) /* bit[24] */
#define	INSTR_L(instr)		(((instr) >> 20) & 0x01) /* bit[20] */
#define	INSTR_W(instr)		(((instr) >> 21) & 0x01) /* bit[21] */
#define	INSTR_S(instr)		(((instr) >> 22) & 0x01) /* bit[22] */
#define	INSTR_P(instr)		(((instr) >> 24) & 0x01) /* bit[24] */
#define	INSTR_B(instr)		(((instr) >> 22) & 0x01) /* bit[22] */
#define	INSTR_N(instr)		(((instr) >> 22) & 0x01) /* bit[22] */
#define	INSTR_D(instr)		(((instr) >> 22) & 0x01) /* bit[22] */

#define	INSTR_RN(instr)		(((instr) >> 16) & 0x0f) /* Rn */
#define	INSTR_RM(instr)		((instr) & 0x0f)         /* Rm */

#define	INSTR_SHIFT(instr)	(((instr) >> 5) & 0x03)  /* bit[6:5]  */
#define	INSTR_SHIFT_IMM(instr)	(((instr) >> 7) & 0x1f)  /* bit[11:7] */

#define	INSTR_CPNUM(instr)	(((instr) >> 8) & 0x0f)  /* bit[11:8] */
#define	INSTR_OFFSET(instr)	((instr) & 0xff)         /* bit[7:0]  */

#define	INSTR_LDM_P	0x01000000
#define	INSTR_LDM_U	0x00800000
#define	INSTR_LDM_IA	INSTR_LDM_U
#define	INSTR_LDM_IB	(INSTR_LDM_P|INSTR_LDM_U)
#define	INSTR_LDM_DA	0
#define	INSTR_LDM_DB	INSTR_LDM_P
#define	INSTR_LDR_U	0x00800000

#define	INSTR_VFP_MODE(instr)	(((instr) >> 21) & 0x0d)


/* RFE flag */
#define	INSTR_TYPE_RFE	B_FALSE
#define	INSTR_TYPE_LDMSTM	B_TRUE

/* RN/RM flag */
#define	INSTR_TYPE_RN		B_TRUE
#define	INSTR_TYPE_RM		B_FALSE

/* (USERMODE)  offset of (struct)regs */
#define	INSTR_RN_OFFSET_U(instr)	\
	((INSTR_RN(instr) >= 15) ? (INSTR_RN(instr) + 2) : INSTR_RN(instr))

#define	INSTR_RM_OFFSET_U(instr)	\
	((INSTR_RM(instr) >= 15) ? (INSTR_RM(instr) + 2) : INSTR_RM(instr))

/* (!USERMODE) offset of (struct)regs */
#define	INSTR_RN_OFFSET(instr)	\
	((INSTR_RN(instr) >= 13) ? (INSTR_RN(instr) + 2) : INSTR_RN(instr))

#define	INSTR_RM_OFFSET(instr)	\
	((INSTR_RM(instr) >= 13) ? (INSTR_RM(instr) + 2) : INSTR_RM(instr))

/* Rotate Right */
#define	ROTATE_RIGHT(x, shift)	(((x) >> (shift)) | ((x) << (32 - (shift))))


/* Decode instruction to get length of memory */
static int
instr_decode_mem_size(struct regs *rp, uint32_t instr, caddr_t *addr);

/* Decode LDM/STM */
static int
instr_decode_ldmstm(struct regs *rp, uint32_t instr,
	caddr_t *addr, boolean_t type);

/* Decode LDREX/STREX */
static int
instr_decode_ldrexstrex(struct regs *rp, uint32_t instr, caddr_t *addr);

/* Decode VFP */
static int
instr_decode_vfp(struct regs *rp, uint32_t instr, caddr_t *addr);

/* analyze (Addressing mode) */
static int
instr_addressing_second(struct regs *rp, uint32_t instr, caddr_t *addr);
static boolean_t
instr_addressing_third(struct regs *rp, uint32_t instr, caddr_t *addr);

/* analyze (Addressing mode VFP) */
static int
instr_addressing_fifth_vfp(struct regs *rp, uint32_t instr, caddr_t *addr);

/* Compute address */
static uint32_t
instr_compute_addr(struct regs *rp, uint32_t instr, boolean_t reg_type);



/*
 * static caddr_t
 * instr_compute_addr(struct regs *rp, uint32_t instr, boolean_t reg_type)
 *
 * Compute address from Rn or Rm.
 *
 * Return : address of memory referenced.
 */
static uint32_t
instr_compute_addr(struct regs *rp, uint32_t instr, boolean_t reg_type)
{
	caddr_t	ret = 0;
	uint32_t	add = 0;
	uint32_t	offset = 0;

	if (reg_type) {
		/* RN */
		if (USERMODE(rp->r_cpsr)) {
			/* USER MODE */
			offset = INSTR_RN_OFFSET_U(instr);
		} else {
			offset = INSTR_RN_OFFSET(instr);

			if (offset == 15) {
				/* r_sp_svc */
				add = 8;
			}
		}

	} else {
		/* RM */
		if (USERMODE(rp->r_cpsr)) {
			offset = INSTR_RM_OFFSET_U(instr);
		} else {
			offset = INSTR_RM_OFFSET(instr);

			if (offset == 15) {
				/* r_sp_svc */
				add = 8;
			}
		}
	}

	return (*(&rp->r_r0 + offset) + add);
}


/*
 * Addressing mode(2)
 *	LDR|STR{<cond>}{B}{T} <Rd>, <addressing_mode>
 *
 * This Addressing mode contains 9 options.
 *
 * Following option is excluded. 
 *	immediate offset
 *	immediate post-indexed
 *	immediate pre-indexed
 *
 * Return
 *	success : size of memory referenced.
 *	failure : -1
 */
static int
instr_addressing_second(struct regs *rp, uint32_t instr, caddr_t *addr)
{
	int		size = -1;
	uint32_t	index = 0;
	uint32_t	shift_imm = 0;
	int		carry = 0;
	uint32_t	u_rm = 0;
	int		s_rm = 0;


	/* for logical shift */
	u_rm = instr_compute_addr(rp, instr, INSTR_TYPE_RM);

	/* for arithmetic shift  */
	s_rm = (int) instr_compute_addr(rp, instr, INSTR_TYPE_RM);

	shift_imm = INSTR_SHIFT_IMM(instr);

	switch (INSTR_SHIFT(instr)) {
	case 0:
		/* LSL */
		index = u_rm << shift_imm;
		break;
	case 1:
		/* LSR */
		if (shift_imm == 0) {
			index = 0;
		} else {
			index = u_rm >> shift_imm;
		}
		break;
	case 2:
		/* ASR */
		if (shift_imm == 0) {
			if (s_rm & 0x80000000) {
				index = 0xffffffff;
			} else {
				index = 0;
			}
		} else {
			index = s_rm >> shift_imm;
		}
		break;
	case 3:
		if (shift_imm == 0) {
			/* RRX (with Carry bit) */
			carry = ((rp->r_cpsr >> 29) & 0x01);
			index = (carry << 31) | (u_rm >> 1);
		} else {
			/* ROR */
			index = ROTATE_RIGHT(u_rm, shift_imm);
		}
		break;
	}

	if (instr & INSTR_LDR_U) {
		/* address = Rn + index */
		*addr = (caddr_t) (instr_compute_addr(rp, instr, INSTR_TYPE_RN)
		    + index);
	} else {
		/* address = Rn - index */
		*addr = (caddr_t) (instr_compute_addr(rp, instr, INSTR_TYPE_RN)
		    - index);
	}

	if (INSTR_B(instr)) {
		/* byte access */
		size = 1;
	} else {
		/* word access */
		size = 4;
	}

	return size;
}

/*
 * Addressing mode(3)
 *	LDR|STR{<cond>}H|SH|SB|D <Rd>, <addressing_mode>
 *
 * This Addressing mode contains 6 options.
 *
 * Return
 *	success : B_TRUE
 *	failure : B_FALSE
 */
static boolean_t
instr_addressing_third(struct regs *rp, uint32_t instr, caddr_t *addr)
{
	boolean_t	ret = B_FALSE;
	uint32_t	offset_8 = 0;
	uint32_t	add = 0;


	switch (INSTR_b21to22(instr)) {
	case 0:
	case 1:
		if (INSTR_RN(instr) == 15) {
			/* Rn is PC */
			add = 8;
		}

		if (instr & INSTR_LDR_U) {
			/* address = (Rn+add) + Rm */
			*addr = 
			    (caddr_t) (instr_compute_addr(rp, instr,
					INSTR_TYPE_RN) + add
					+ instr_compute_addr(rp, instr,
					INSTR_TYPE_RM));
		} else {
			/* address = (Rn+add) - Rm */
			*addr =
			    (caddr_t) (instr_compute_addr(rp, instr,
					INSTR_TYPE_RN) + add
					- instr_compute_addr(rp, instr,
					INSTR_TYPE_RM));
		}

		ret = B_TRUE;
		break;

	case 2:
		if (INSTR_P(instr)) {
			if (INSTR_RN(instr) == 15) {
				/* Rn is PC */
				add = 8;
			}
		}

		offset_8 = ((INSTR_b8to11(instr) << 4) 
		    | INSTR_b0to3(instr));

		if (instr & INSTR_LDR_U) {
			/* address = (Rn+add) + offset_8 */
			*addr = (caddr_t) (instr_compute_addr(rp, instr,
			    INSTR_TYPE_RN) + add + offset_8);

		} else {
			/* address = (Rn+add) - offset_8 */
			*addr = (caddr_t) (instr_compute_addr(rp, instr,
			    INSTR_TYPE_RN) + add - offset_8);
		}
		ret = B_TRUE;
		break;

	case 3:
		offset_8 = ((INSTR_b8to11(instr) << 4)
		    | INSTR_b0to3(instr));

		if (instr & INSTR_LDR_U) {
			/* address = Rn + offset_8 */
			*addr = (caddr_t) (instr_compute_addr(rp, instr,
			    INSTR_TYPE_RN) + offset_8);
		} else {
			/* address = Rn - offset_8 */
			*addr = (caddr_t) (instr_compute_addr(rp, instr,
			    INSTR_TYPE_RN) - offset_8);
		}
		ret = B_TRUE;
		break;
	}

	return ret;
}


/*
 * Addressing mode(5) VFP (multiple load/store)
 *
 * This Addressing mode contains 3 options.
 *  (no index)
 *	<opcode>IA<precision>{<cond>} <Rn>, <registers>
 *  (increment)
 *	<opcode>IA<precision>{<cond>} <Rn>!, <registers>
 *  (decrement)
 *	<opcode>DB<precision>{<cond>} <Rn>!, <registers>
 *
 * Return
 *	success : size of memory referenced.
 *	failure : -1
 */
static int
instr_addressing_fifth_vfp(struct regs *rp, uint32_t instr, caddr_t *addr)
{
	int		size = -1;
	int		add = 0;
	uint32_t	offset = 0;
	uint32_t	cp_num = 0;


	/* [7:0] */
	offset = INSTR_OFFSET(instr);
	if(offset < 1 || offset > 33) {
		/* offset error */
		return -1;
	}

	/* cp_num */
	cp_num = INSTR_CPNUM(instr);
	if (cp_num != 0x0a && cp_num != 0x0b) {
		return -1;
	}

	/* [24:23] [21] */
	switch (INSTR_VFP_MODE(instr)) {
	case 0x04:
		/*
		 * no index
		 * [24] = 0
		 * [23] = 1
		 * [21] = 0
		 */
		if (INSTR_RN(instr) == 15) {
			add = 8;
		}

		if (cp_num == 0x0a) {
			/* FLDMS or FSTMS */
			size = offset * 4;
		} else if (cp_num == 0x0b) {
			if (instr & 0x01) {
				/* FLDMX or FSTMX */
				offset = offset - 1;
			}
			/* FLDMD or FSTMD */
			size = offset * 4;
		}

		*addr = (caddr_t) (instr_compute_addr(rp, instr,
		    INSTR_TYPE_RN) + add);

		break;

	case 0x05:
		/*
		 * increment
		 * [24] = 0
		 * [23] = 1
		 * [21] = 1
		 */
		if (cp_num == 0x0a) {
			/* FLDMS or FSTMS */
			size = offset * 4;
		} else if (cp_num == 0x0b) {
			if (instr & 0x01) {
				/* FLDMX or FSTMX */
				offset = offset - 1;
			}
			/* FLDMD or FSTMD */
			size = offset * 4;
		}

		*addr = (caddr_t) (instr_compute_addr(rp, instr,
		    INSTR_TYPE_RN));

		break;

	case 0x09:
		/*
		 * decrement
		 * [24] = 1
		 * [23] = 0
		 * [21] = 1
		 */
		if (cp_num == 0x0a) {
			/* FLDMS or FSTMS */
			size = offset * 4;
		} else if (cp_num == 0x0b) {
			if (instr & 0x01) {
				/* FLDMX or FSTMX */
				offset = offset - 1;
			}
			/* FLDMD or FSTMD */
			size = offset * 4;
		}

		*addr = (caddr_t) (instr_compute_addr(rp, instr,
		    INSTR_TYPE_RN) - (offset * 4));

		break;
	}

	return size;
}

/*
 * static int
 * instr_decode_ldmstm(struct regs *rp, uint32_t instr, 
 *	caddr_t *addr, boolean_t type);
 *
 * Decode LDM/STM instructions.
 * Compute size and address of memory referenced.
 *
 * Return
 *	success : size of memory referenced.
 *	failure : -1
 */
static int
instr_decode_ldmstm(struct regs *rp, uint32_t instr,
	caddr_t *addr, boolean_t type)
{
	int		regnum = 0;
	int		i;
	uint32_t	instr_tmp = instr;


	if (type) {
		for (i = 0; i < 16; i++) {
			if (instr_tmp & 0x01) {
				regnum++;
			}
			instr_tmp >>= 1;
		}
		if (regnum == 0) {
			return -1;
		}
	} else {
		/* 
		 * RFE : regnum = 2  (PC  and CPSR)
		 * SRS : regnum = 2  (R14 and SPSR)
		 */
		regnum = 2;
	}

	switch (instr & (INSTR_LDM_P|INSTR_LDM_U)) {
	case INSTR_LDM_IA:
		*addr = (caddr_t) (instr_compute_addr(rp, instr,
		    INSTR_TYPE_RN));
		break;
	case INSTR_LDM_IB:
		*addr = (caddr_t) (instr_compute_addr(rp, instr, 
		    INSTR_TYPE_RN) + 4);
		break;
	case INSTR_LDM_DA:
		*addr = (caddr_t) (instr_compute_addr(rp, instr, 
		    INSTR_TYPE_RN) - (regnum * 4) + 4);
		break;
	case INSTR_LDM_DB:
		*addr = (caddr_t) (instr_compute_addr(rp, instr, 
		    INSTR_TYPE_RN) - (regnum * 4));
		break;
	}

	return (regnum * 4);

}

/*
 * static int
 * instr_decode_ldrexstrex(struct regs *rp, uint32_t instr, caddr_t *addr);
 *
 * Decode LDREX/STREX instructions.
 * Compute size and address of memory referenced.
 *
 * Return
 *	success : size of memory referenced.
 *	failure : -1
 */
static int
instr_decode_ldrexstrex(struct regs *rp, uint32_t instr, caddr_t *addr)
{
	int	size = -1;


	switch (INSTR_b21to27(instr)) {
	case 0x0c:
		/*
		 * LDREX, STREX
		 * [27:21] = 0001100
		 * [7:4]   = 1001
		 */
		size = 4;
		break;

	case 0x0d:
		/*
		 * LDREXD, STREXD
		 * [27:21] = 0001101
		 * [7:4]   = 1001
		 */
		size = 8;
		break;

	case 0x0e:
		/*
		 * LDREXB, STREXB
		 * [27:21] = 0001110
		 * [7:4]   = 1001
		 */
		size = 1;
		break;

	case 0x0f:
		/*
		 * LDREXH, STREXH
		 * [27:21] = 0001111
		 * [7:4]   = 1001
		 */
		size = 2;
		break;
	}

	*addr = (caddr_t) (instr_compute_addr(rp, instr,INSTR_TYPE_RN));

	return size;
}


/*
 * static int
 * instr_decode_vfp(struct regs *rp, 
 *	uint32_t instr, caddr_t *addr);
 *
 * Decode VFP instructions.
 * Compute size and address of memory referenced.
 *
 * This function decodes the following instructions
 *	FLDD, FLDMD(FLDMX), FLDMS, FLDS,
 *	FSTD, FSTMD(FSTMX), FSTMS, FSTS
 *	(FLDMX, FSTMX are not recommended in ARMv6)
 *
 * Return
 *	success : size of memory referenced.
 *	failure : -1
 */
static int
instr_decode_vfp(struct regs *rp, uint32_t instr, caddr_t *addr)
{
	int		size = -1;
	uint32_t	offset = 0;


	if (INSTR_CPNUM(instr) == 0x0a) {
		/* CP10 */
		if (INSTR_b24(instr)) {
			if(INSTR_b20to21(instr) == 0x01
			    || INSTR_b20to21(instr) == 0x00) {
				/*
				 * FSTS or FLDS
				 */
				offset = INSTR_OFFSET(instr);
	
				if (instr & INSTR_LDR_U) {
					*addr = (caddr_t)(instr_compute_addr(rp,
					    instr, INSTR_TYPE_RN)
					    + (offset * 4));
				} else {
					*addr = (caddr_t)(instr_compute_addr(rp,
					    instr, INSTR_TYPE_RN)
					    - (offset * 4));
				}

				return 4;
			}
		}

		/* FLDMS or FSTMS */
		size = instr_addressing_fifth_vfp(rp, instr, addr);

	} else if (INSTR_CPNUM(instr) == 0x0b) {
		/* CP11 */
		if ((INSTR_b24(instr)) && (!(INSTR_D(instr)))) {
			/* [24] == 1, [22] = 0 */
			if(INSTR_b20to21(instr) == 0x01
			    || INSTR_b20to21(instr) == 0x00) {
				/*
				 * FSTD or FLDD
				 */
				offset = INSTR_OFFSET(instr);

				if (instr & INSTR_LDR_U) {
					*addr = (caddr_t)(instr_compute_addr(rp,
					    instr, INSTR_TYPE_RN)
					    + (offset * 4));
				} else {
					*addr = (caddr_t) (instr_compute_addr(rp,
					    instr, INSTR_TYPE_RN)
					    - (offset * 4));
				}
				return 8;
			}
		}

		if (!(INSTR_D(instr))) {
			/* 
			 * FLDMD or FSTMD
			 * [22] = 0
			 */
			size = instr_addressing_fifth_vfp(rp, instr, addr);
		}
	}

	return size;
}

/*
 * static int
 * instr_decode_get_mem_size(uint32_t instr, caddr_t *addr);
 *
 * Decode instructions and 
 * Compute size and address of memory referenced.
 *
 * Return
 *	success : size of memory referenced.
 *	failure : -1
 */
static int
instr_decode_mem_size(struct regs *rp, uint32_t instr, caddr_t *addr)
{
	int		size = -1;
	uint32_t	imm_off12;
	int		add = 0;


	switch (INSTR_TYPE(instr)) {
	case 0:
		/*
		 * ARM v6
		 *   LDRD, LDREX, LDRH, LDRSB, LDRSH, 
		 *   STRD, STREX, STRH, SWP, SWPB
		 * MPCore Additional
		 *   LDREXB, LDREXH, LDREXD, STREXB, STREXH, STREXD
		 */
		switch (INSTR_b4to7(instr)) {
		case 0x09:
			/* [7:4] = 1001 */
			if (INSTR_b20to27(instr) == 0x10) {
				/*
				 * SWP
				 * [27:20] = 00010000
				 * [7:4]   = 1001
				 */
				*addr = (caddr_t) (instr_compute_addr(rp, instr,
				    INSTR_TYPE_RN));
				size = 4;
			} else if (INSTR_b20to27(instr) == 0x14) {
				/*
				 * SWPB
				 * [27:20] = 00010100
				 * [7:4]   = 1001
				 */
				*addr = (caddr_t) (instr_compute_addr(rp, instr,
				    INSTR_TYPE_RN));
				size = 1;
			} else {
				/*
				 * LDREX, STREX, LDREXD, STREXD, 
				 * LDREXB, STREXB, LDREXH, STREXH
				 */
				size = instr_decode_ldrexstrex(rp,
				    instr, addr);
			}
			break;

		case 0x0b:
			/*
			 * LDRH, STRH
			 * [7:4] = 1011
			 */
			if (instr_addressing_third(rp, instr, addr)) {
				size = 2;
			}
			break;

		case 0x0d:
			if (INSTR_L(instr)) {
				/*
				 * LDRSB
				 * [7:4] = 1101
				 * [20] = 1
				 */
				if (instr_addressing_third(rp,
				    instr, addr)) {
					size = 1;
				}
			} else {
				/*
				 * LDRD
				 * [7:4] = 1101
				 * [20] = 0
				 */
				if (instr_addressing_third(rp,
				    instr, addr)) {
					size = 8;
				}
			}
			break;

		case 0x0f:
			if (INSTR_L(instr)) {
				/*
				 * LDRSH
				 * [20]  = 1
				 * [7:4] = 1111
				 */
				if (instr_addressing_third(rp,
				    instr, addr)) {
					size = 2;
				}
			} else {
				/*
				 * STRD
				 * [20]  = 0
				 * [7:4] = 1111
				 */
				if (instr_addressing_third(rp,
				    instr, addr)) {
					size = 8;
				}
			}
			break;
		}
		break;

	case 1:
		/* ignore instruction */
		break;

	case 2:
		/* 
		 * Addressing mode(2)
		 *	immediate offset
		 *	immediate post/pre indexed
		 */
		/* offset_12 [11:0] */
		imm_off12 = instr & 0xfff;

		if (instr & INSTR_LDR_U) {
			/* Rn + offset (U==1) */
			*addr = (caddr_t) (instr_compute_addr(rp, instr,
			    INSTR_TYPE_RN) + imm_off12);
		} else {
			/* Rn - offset (U==0) */
			*addr = (caddr_t) (instr_compute_addr(rp, instr,
			    INSTR_TYPE_RN) - imm_off12);
		}

		if (INSTR_B(instr)) {
			/* byte access */
			size = 1;
		} else {
			/* word access */
			size = 4;
		}
		break;

	case 3:
		/*
		 * LDR, LDRB, LDRBT, LDRT, STR, STRB, STRT
		 */
		size = instr_addressing_second(rp, instr, addr);
		break;

	case 4:
		/*
		 * LDM(1), LDM(2), LDM(3), RFE, STM(1), STM(2)
		 */
		if (INSTR_L(instr)) {
			/* Load */
			if (INSTR_S(instr)) {
				if (INSTR_b15(instr)) {
					/* LDM(3) */
					size = instr_decode_ldmstm(rp,
						    instr, addr,
						    INSTR_TYPE_LDMSTM);
				} else {
					if (!INSTR_W(instr)) {
						/* LDM(2) */
						size =
						    instr_decode_ldmstm(rp,
						    instr, addr,
						    INSTR_TYPE_LDMSTM);
					}
				}
			} else {
				if ((INSTR_b28to31(instr) == 0x0f) 
				    && (INSTR_b8to11(instr) == 0x0a)) {
					/* 
					 * RFE
					 * [31:28] = 1111
					 * [11:8]  = 1010
					 */
					size = instr_decode_ldmstm(rp,
						    instr, addr,
						    INSTR_TYPE_RFE);
				} else {
					/* LDM(1) */
					size =
					    instr_decode_ldmstm(rp,
						    instr,addr,
						    INSTR_TYPE_LDMSTM);
				}
			}
		} else {
			/* Store */
			if (INSTR_S(instr)) {
				if (!INSTR_W(instr)) {
					/* STM(2) */
					size = instr_decode_ldmstm(rp, instr,
					    addr, INSTR_TYPE_LDMSTM);
				}
			} else {
				/* STM(1) */
				size = instr_decode_ldmstm(rp, instr,
				    addr, INSTR_TYPE_LDMSTM);
			}
		}
		break;
	case 5:
		/* ignore instruction */
		break;

	case 6:
		/*
		 * VFP instructions
		 */
		if ((INSTR_CPNUM(instr) == 10)
		    || (INSTR_CPNUM(instr) == 11)) {
			/* VFP */
			size = instr_decode_vfp(rp, instr, addr);
		}
		break;

	case 7:
		/* ignore instruction */
		break;
	}

	return size;
}

int
instr_size(struct regs *rp, caddr_t *addrp, enum seg_rw rw)
{
	uint32_t	instr = 0;
	int		res = -1;
	int		mem_size = -1;


	if (rw == S_EXEC) {
		/* Instruction fetch */
		*addrp = (caddr_t)rp->r_pc;
		return 4;
	} 

	/* Read Instruction */
	res = fuword32_nowatch((caddr_t)rp->r_pc, &instr);

	if(res != 0) {
		return -1;
	}

	/* to Compute size of memory */
	mem_size = instr_decode_mem_size(rp, instr, addrp);

	return mem_size;
}
