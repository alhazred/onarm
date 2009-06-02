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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2007-2008 NEC Corporation
 */

#pragma ident	"@(#)Pisadep.c"

#include <sys/regset.h>

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

#include "Pcontrol.h"
#include "Pisadep.h"

#define	M_PLT_NRSV	4	/* reserved PLT entries */
#define	M_PLT_ENTSIZE	12	/* size of each PLT entry */

#define	SYSCALL32 0xff000000	/* SWI is used to call systemcall */
#define	SYSCALL32_AL 0xef000000

const char *
Ppltdest(struct ps_prochandle *P, uintptr_t pltaddr)
{
	map_info_t *mp = Paddr2mptr(P, pltaddr);

	uintptr_t r_addr;
	file_info_t *fp;
	Elf32_Rel r;
	size_t i;

	if (mp == NULL || (fp = mp->map_file) == NULL ||
	    fp->file_plt_base == 0 ||
	    pltaddr - fp->file_plt_base >= fp->file_plt_size) {
		errno = EINVAL;
		return (NULL);
	}

	i = (pltaddr - fp->file_plt_base) / M_PLT_ENTSIZE - M_PLT_NRSV;

	r_addr = fp->file_jmp_rel + i * sizeof (Elf32_Rel);

	if (Pread(P, &r, sizeof (Elf32_Rel), r_addr) == sizeof (Elf32_Rel) &&
	    (i = ELF32_R_SYM(r.r_info)) < fp->file_dynsym.sym_symn) {

		Elf_Data *data = fp->file_dynsym.sym_data_pri;
		Elf32_Sym *symp = &(((Elf32_Sym *)data->d_buf)[i]);

		return (fp->file_dynsym.sym_strs + symp->st_name);
	}

	return (NULL);
}

int
Pissyscall(struct ps_prochandle *P, uintptr_t addr)
{
	instr_t sysinstr;
	instr_t instr;

	sysinstr = SYSCALL32;

	if (Pread(P, &instr, sizeof (instr), addr) != sizeof (instr) ||
	    (instr & sysinstr) != SYSCALL32_AL) {
		return (0);
	}

	return (1);
}

int
Pissyscall_prev(struct ps_prochandle *P, uintptr_t addr, uintptr_t *dst)
{
	uintptr_t prevaddr = addr - sizeof (instr_t);

	if (Pissyscall(P, prevaddr)) {
		if (dst) {
			*dst = prevaddr;
		}
		return (1);
	}

	return (0);
}

/* ARGSUSED */
int
Pissyscall_text(struct ps_prochandle *P, const void *buf, size_t buflen)
{
	instr_t sysinstr;

	sysinstr = SYSCALL32;

	if (buflen >= sizeof (instr_t) &&
	    (*(instr_t *)buf & sysinstr) == SYSCALL32_AL) {
		return (1);
	} 

	return (0);
}

static void
ucontext_n_to_prgregs(const ucontext_t *src, prgregset_t dst)
{
	(void) memcpy(dst, src->uc_mcontext.gregs, sizeof (gregset_t));
}

int
Pstack_iter(struct ps_prochandle *P, const prgregset_t regs,
	proc_stack_f *func, void *arg)
{
	/* This is not supported, because fp register does not exist 
	   on ARM architecture. */
	return (-1);
}

uintptr_t
Psyscall_setup(struct ps_prochandle *P, int nargs, int sysindex, uintptr_t sp)
{
	sp -= (nargs > 4)? sizeof (int) * (nargs - 4) : 0;
	sp = PSTACK_ALIGN32(sp);

	P->status.pr_lwp.pr_reg[REG_R12] = sysindex;
	P->status.pr_lwp.pr_reg[R_SP]    = sp;
	P->status.pr_lwp.pr_reg[R_PC]    = P->sysaddr;

	return (sp);
}

int
Psyscall_copyinargs(struct ps_prochandle *P, int nargs, argdes_t *argp,
    uintptr_t ap)
{
	uint32_t arglist[MAXARGS - 4];
	int i;
	argdes_t *adp;

	if (nargs > MAXARGS) {
		return (-1);
	}

	for (i = 0, adp = argp; i < nargs; i++, adp++) {
		if (i < 4) {
			(void) Pputareg(P, i, adp->arg_value);
		} else {
			arglist[i - 4] = adp->arg_value;
		}
	}

	if (nargs > 4 &&
	    Pwrite(P, &arglist[0], sizeof (int) * (nargs - 4),
	    (uintptr_t)ap) != sizeof (int) * (nargs - 4)) {
		return (-1);
	}

	return (0);
}

int
Psyscall_copyoutargs(struct ps_prochandle *P, int nargs, argdes_t *argp,
    uintptr_t ap)
{
	uint32_t arglist[MAXARGS - 4];
	int i;
	argdes_t *adp;

	if (nargs > MAXARGS) {
		return (-1);
	}

	if (nargs > 4) {
		if (Pread(P, &arglist[0], sizeof (int) * (nargs - 4), (uintptr_t)ap)
		    != sizeof (int) * (nargs - 4)) {
			return (-1);
		}
	}

	for (i = 0, adp = argp; i < nargs; i++, adp++) {
		if (i < 4) {
			adp->arg_value = P->status.pr_lwp.pr_reg[i];
		} else {
			adp->arg_value = arglist[i - 4];
		}
	}

	return (0);
}
