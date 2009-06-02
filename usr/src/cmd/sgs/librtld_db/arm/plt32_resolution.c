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
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2007 NEC Corporation
 */

#pragma ident	"@(#)plt32_resolution.c	1.14	05/06/08 SMI"

#include	<libelf.h>
#include	<sys/regset.h>
#include	<rtld_db.h>
#include	<_rtld_db.h>
#include	<msg.h>
#include	<stdio.h>

#define	M_LDR_U		0x00800000

static uint_t
immediate_data(uint_t inst)
{
	uint_t  rotate_imm, immed_8, immed;

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

static uint_t
immediate_offset(uint_t inst)
{
	uint_t	offset;

	offset = inst & 0xfff;
	if ((inst & M_LDR_U) == 0) {
		offset *= -1;
	}
	return offset;
}

/*
 * PLT entry for foo:
 *	add	ip, pc, #X
 *	add	ip, ip, #Y
 *	ldr	pc, [ip, #Z]!	; ip = jump table address of GOT
 */
/* ARGSUSED 2 */
rd_err_e
plt32_resolution(rd_agent_t *rap, psaddr_t pc, lwpid_t lwpid,
	psaddr_t pltbase, rd_plt_info_t *rpi)
{
	uint_t		instr[3];
	uint_t		addr;
	psaddr_t	pltoff, pltaddr;

	pltoff = pc - pltbase;
	pltaddr = pltbase +
		((pltoff / M_PLT_ENTSIZE) * M_PLT_ENTSIZE);

	if (ps_pread(rap->rd_psp, pltaddr, (char *)instr,
	    M_PLT_ENTSIZE) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_READFAIL_2), EC_ADDR(pltaddr)));
		return (RD_ERR);
	}

	if (rtld_db_version >= RD_VERSION3) {
		rpi->pi_flags = 0;
		rpi->pi_baddr = 0;
	}

	addr = (pltaddr + 8) + immediate_data(instr[0])
	    + immediate_data(instr[1]) + immediate_offset(instr[2]);

	if (ps_pread(rap->rd_psp, addr, (char *)&addr,
	    sizeof (uint_t)) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_READFAIL_2), EC_ADDR(addr)));
		return (RD_ERR);
	}

	if (addr == (pltbase - M_PLT_RESERVSZ)) {
		rd_err_e	rerr;
		/*
		 * Unbound PLT
		 */
		if ((rerr = rd_binder_exit_addr(rap, MSG_ORIG(MSG_SYM_RTBIND),
		    &(rpi->pi_target))) != RD_OK) {
			return (rerr);
		}
		rpi->pi_skip_method = RD_RESOLVE_TARGET_STEP;
		rpi->pi_nstep = 3;
	} else {
		/*
		 * Bound PLT
		 */
		rpi->pi_skip_method = RD_RESOLVE_STEP;
		rpi->pi_nstep = 3;
		rpi->pi_target = 0;
		if (rtld_db_version >= RD_VERSION3) {
			rpi->pi_flags |= RD_FLG_PI_PLTBOUND;
			rpi->pi_baddr = addr;
		}
	}

	return (RD_OK);
}
