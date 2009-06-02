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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*
 * Copyright (c) 2006-2008 NEC Corporation
 */

#pragma ident	"@(#)prmachdep.c	1.55	06/02/03 SMI"

#include <sys/regset.h>
#include <sys/privregs.h>
#include <sys/systm.h>
#include <sys/cpuvar.h>
#include <sys/procfs.h>
#include <sys/mman.h>
#include <sys/fp.h>
#include <sys/vmsystm.h>
#include <vm/seg_kmem.h>
#include <sys/trap.h>

int	prnwatch = 10000;	/* maximum number of watched areas */


/* Instruction Define
 *
 * BKPT(ARM) for /proc single-step
 *    1110 0001 0010 0000 0000 0001 0111 0001
 *  immediate field : 
 *   bit [0:3] 0001            : /proc
 *   bit[19:8] 0000 0000 0001  : single step
 *             -> 0xe1200171 (BKPT)
 *
 * BKPT(THUMB) for /proc single-step
 *    1011 1110 0001 0001
 *  immediate field : 
 *   bit [0:3] 0001            : /proc
 *   bit [7:4] 0001            : single step
 *             -> 0xbe11 (BKPT)
 */
#define BREAKINSTR_ARM	 (ARM_BKPT_INSTR | ARM_BKPT_PROC | ARM_BKPT_PROC_STEP)
#define BREAKINSTR_THUMB 0xbe11

/*
 * Ope code
 * [TST], [TEQ], [CMP], [CMN] are not use Rd(pc) operand.
 */
#define	OP_SHIFT	21
#define	OP_MASK		0xf
#define	OP_AND		0x0
#define	OP_EOR		0x1
#define	OP_SUB		0x2
#define	OP_RSB		0x3
#define	OP_ADD		0x4
#define	OP_ADC		0x5
#define	OP_SBC		0x6
#define	OP_RSC		0x7
#define	OP_ORR		0xc
#define	OP_MOV		0xd
#define	OP_BIC		0xe
#define	OP_MVN		0xf

/*
 * condition code
 */
#define	COND_SHIFT	28
#define	COND_MASK	0xf
#define	COND_EQ		0x0
#define	COND_NE		0x1
#define	COND_CS		0x2
#define	COND_CC		0x3
#define	COND_MI		0x4
#define	COND_PL		0x5
#define	COND_VS		0x6
#define	COND_VC		0x7
#define	COND_HI		0x8
#define	COND_LS		0x9
#define	COND_GE		0xa
#define	COND_LT		0xb
#define	COND_GT		0xc
#define	COND_LE		0xd
#define	COND_AL		0xe


/* utility */
static uint32_t hamming_weight16(uint32_t w);

/* calculate branch address functions */
static int pr_get_user_reg(klwp_t *lwp, int offset);
static uint32_t pr_getrn(klwp_t *lwp, uint32_t instr);
static uint32_t pr_getaluop2(klwp_t *lwp, uint32_t instr);
static uint32_t pr_getldrop2(klwp_t *lwp, uint32_t instr);
static uint32_t pr_get_next_address(klwp_t *lwp, uint32_t pc,
	uint32_t instr);

/* set or clear breakpoints functions */
static int  pr_set_breakpoint(klwp_t *lwp, caddr_t addr);
static void pr_clear_breakpoint(klwp_t *lwp);


/*
 * hamming weight
 */
static uint32_t
hamming_weight16(uint32_t w)
{
	uint32_t res = (w & 0x5555) + ((w >> 1) & 0x5555);
	res = (res & 0x3333) + ((res >> 2) & 0x3333);
	res = (res & 0x0F0F) + ((res >> 4) & 0x0F0F);
	return (res & 0x00FF) + ((res >> 8) & 0x00FF);
}

/*
 * get user register (use offset)
 */
static int
pr_get_user_reg(klwp_t *lwp, int offset)
{
	uint32_t add = 0;
	struct regs *r = lwptoregs(lwp);

	if (USERMODE(r->r_cpsr)) {
		if (offset >= 15) {
			offset = offset + 2;
		}
	} else {
		if (offset >= 13) {
			offset = offset + 2;
		}

		if (offset == 15) {
			add = 8;
		}
	}

	return (*(&r->r_r0 + offset) + add);
}

/*
 * Get value of register `rn' (in the instruction)
 */
static uint32_t
pr_getrn(klwp_t *lwp, uint32_t instr)
{
	uint32_t reg = (instr >> 16) & 0xf;
	uint32_t val = 0;

	val = pr_get_user_reg(lwp, reg);

	return val;
}

/*
 * Get value of operand2 (in an ALU instruction)
 */
static uint32_t
pr_getaluop2(klwp_t *lwp, uint32_t instr)
{
	uint32_t val = 0;
	int shift;
	int type;

	if (instr & (1 << 25)) {
		/* immediate */
		val = instr & 0xff;
		shift = (instr >> 8) & 0xf;
		type = 3;
	} else {
		/* register */
		val = pr_get_user_reg(lwp, instr & 0xf);

		if (instr & (1 << 4)) {
			/* register shift */
			shift = (int)pr_get_user_reg(lwp, (instr >> 8) & 0xf);
		} else {
			/* immediate shift */
			shift = (instr >> 7) & 0x1f;
		}

		type = (instr >> 5) & 0x3;
	}

	switch (type) {
	case 0:
		val <<= shift;
		break;
	case 1:
		val >>= shift;
		break;
	case 2:
		val = (((signed int)val) >> shift);
		break;
	case 3:
 		val = (val >> shift) | (val << (32 - shift));
		break;
	}
	return val;
}

/*
 * Get value of operand2 (in a LDR instruction)
 */
static uint32_t
pr_getldrop2(klwp_t *lwp, uint32_t instr)
{
	uint32_t val = 0;
	int shift;
	int type;

	val = pr_get_user_reg(lwp, instr & 0xf);
	shift = (instr >> 7) & 0x1f;
	type = (instr >> 5) & 0x3;

	switch (type) {
	case 0:
		val <<= shift;
		break;
	case 1:
		val >>= shift;
		break;
	case 2:
		val = (((signed int)val) >> shift);
		break;
	case 3:
 		val = (val >> shift) | (val << (32 - shift));
		break;
	}
	return val;
}

/*
 * calculate next address
 *   Return
 *    addr : next instruction's address
 */
static uint32_t
pr_get_next_address(klwp_t *lwp, uint32_t pc, uint32_t instr)
{
	uint32_t addr = pc + 4;
	struct regs *r = lwptoregs(lwp);
	int aluop1, aluop2, ccbit;
	uint32_t base;
	uint32_t offset;
	signed int diff;

	switch ((instr >> 25) & 0x7) {
	case 0:
	case 1:
		/*
		 * data processing
		 */
		if ((instr & 0x0000f000) != 0x0000f000) {
			/* Rd is not PC. */
			break;
		}
		if ((instr & 0x0fffffd0) == 0x012fff10) {
			/* bx, blx(2) */
			addr = pr_get_user_reg(lwp, instr & 0xf);
			break;
		}

		aluop1 = pr_getrn(lwp, instr);
		aluop2 = pr_getaluop2(lwp, instr);
		ccbit  = (r->r_cpsr & PSR_C_BIT) ? 1 : 0;

		switch ((instr >> OP_SHIFT) & OP_MASK) {
		case OP_AND:
			addr = aluop1 & aluop2;
			break;
		case OP_EOR:
			addr = aluop1 ^ aluop2;
			break;
		case OP_SUB:
			addr = aluop1 - aluop2;
			break;
		case OP_RSB:
			addr = aluop2 - aluop1;
			break;
		case OP_ADD:
			addr = aluop1 + aluop2;
			break;
		case OP_ADC:
			addr = aluop1 + aluop2 + ccbit;
			break;
		case OP_SBC:
			addr = aluop1 - aluop2 - !ccbit;
			break;
		case OP_RSC:
			addr = aluop2 - aluop1 - !ccbit;
			break;
		case OP_ORR:
			addr = aluop1 | aluop2;
			break;
		case OP_MOV:
			addr = aluop2;
			break;
		case OP_BIC:
			addr = aluop1 & ~aluop2;
			break;
		case OP_MVN:
			addr = ~aluop2;
			break;
		}
		break;

	case 2:
	case 3:
		/*
		 * ldr
		 */
		if ((instr & 0x0010f000) == 0x0010f000) {
			/* Operation is load and Rd is PC. */
			base = pr_getrn(lwp, instr);
			if (instr & (1 << 24)) {
				/* not post index */
				if (instr & (1 << 25)) {
					/* register offset */
					aluop2 = pr_getldrop2(lwp, instr);
				} else {
					/* immediate offset */
					aluop2 = instr & 0xfff;
				}
				if (instr & (1 << 23)) {
					base += aluop2;
				} else {
					base -= aluop2;
				}
			}
			/* Check whether we read user space */
			fuword32_nowatch((void *)base, &addr);
		}
		break;

	case 4:
		/*
		 * ldm
		 */
		if ((instr & 0x00108000) == 0x00108000) {
			/* Operation is load and PC is included. */
			base = pr_getrn(lwp, instr);
			if (instr & (1 << 23)) {
				offset = hamming_weight16(instr & 0xffff) << 2;

				if (!(instr & (1 << 24))) {
					offset -= 4;
				}
			} else {
				if (instr & (1 << 24)) {
					offset = -4;
				} else {
					offset = 0;
				}
			}
			/* Check whether we read user space */
			fuword32_nowatch((void *)(base + offset), &addr);
			break;
		}
		break;

	case 5:
		/*
		 * b, bl
		 */
		diff = (instr & 0x00ffffff) << 8;
		diff = (diff >> 6) + 8;
		addr = pc + diff;
		break;
	}

	if (addr == pc) {
		/* infinite loop */
		addr = pc + 4;
	}

	return addr;
}

/*
 * Check status register value. N == V.
 *   Return
 *      1  : N == V
 *      0  : N != V
 */
static int
pr_check_cpsr_n_eq_v(greg_t cpsr)
{
	int nbit = (cpsr & PSR_N_BIT) ? 1 : 0;
	int vbit = (cpsr & PSR_V_BIT) ? 1 : 0;
	return (nbit == vbit);
}

/*
 * Check condition code.
 *   Return
 *      1  : execute this code
 *      0  : not execute
 */
static int
pr_check_condition(klwp_t *lwp, uint32_t instr)
{
	uint32_t cond;
	struct regs *r;
	greg_t cpsr;
	int ret;

	cond = (instr >> COND_SHIFT) & COND_MASK;
	if (cond >= COND_AL) {
		/* ALways || NeVer */
		return 1;
	}

	r = lwptoregs(lwp);
	cpsr = r->r_cpsr;

	switch (cond) {
	case COND_EQ:
		ret = (cpsr & PSR_Z_BIT) ? 1 : 0;
		break;
	case COND_NE:
		ret = (cpsr & PSR_Z_BIT) ? 0 : 1;
		break;
	case COND_CS:
		ret = (cpsr & PSR_C_BIT) ? 1 : 0;
		break;
	case COND_CC:
		ret = (cpsr & PSR_C_BIT) ? 0 : 1;
		break;
	case COND_MI:
		ret = (cpsr & PSR_N_BIT) ? 1 : 0;
		break;
	case COND_PL:
		ret = (cpsr & PSR_N_BIT) ? 0 : 1;
		break;
	case COND_VS:
		ret = (cpsr & PSR_V_BIT) ? 1 : 0;
		break;
	case COND_VC:
		ret = (cpsr & PSR_V_BIT) ? 0 : 1;
		break;
	case COND_HI:
		ret = ((cpsr & PSR_C_BIT) && !(cpsr & PSR_Z_BIT)) ? 1 : 0;
		break;
	case COND_LS:
		ret = (!(cpsr & PSR_C_BIT) || (cpsr & PSR_Z_BIT)) ? 1 : 0;
		break;
	case COND_GE:
		ret = pr_check_cpsr_n_eq_v(cpsr) ? 1 : 0;
		break;
	case COND_LT:
		ret = pr_check_cpsr_n_eq_v(cpsr) ? 0 : 1;
		break;
	case COND_GT:
		ret = (!(cpsr & PSR_Z_BIT) && pr_check_cpsr_n_eq_v(cpsr)) ? 1 : 0;
		break;
	case COND_LE:
		ret = ((cpsr & PSR_Z_BIT) || !pr_check_cpsr_n_eq_v(cpsr)) ? 1 : 0;
		break;
	}

	return ret;
}

/*
 * Set break point and save instruction and address.
 *   Return
 *     -1  : error
 *      0  : OK (Set break point and save instruction and address)
 */
static int
pr_set_breakpoint(klwp_t *lwp, caddr_t addr)
{
	uint32_t new_instr;
	int size = sizeof(uint32_t);
	int res;

	res = copyin_nowatch(addr, &(lwp->lwp_pcb.pcb_bpinfo.instr.arm), size);
	if (res != 0) {
		/* instruction save error */
		return -1;
	}
	new_instr = BREAKINSTR_ARM;
	res = uwrite(lwp->lwp_procp, &new_instr, size, (uintptr_t)addr);
	if (res != 0) {
		/* instruction change error */
		return -1;
	}
	lwp->lwp_pcb.pcb_bpinfo.addr = addr;
	return 0;
}

/*
 * Clear break point and Restore instruction.
 */
static void
pr_clear_breakpoint(klwp_t *lwp)
{
	caddr_t addr = lwp->lwp_pcb.pcb_bpinfo.addr;
	if (addr == 0) {
		return;
	}
	lwp->lwp_pcb.pcb_bpinfo.addr = 0;
	uwrite(lwp->lwp_procp, &(lwp->lwp_pcb.pcb_bpinfo.instr.arm),
					sizeof(uint32_t), (uintptr_t)addr);
}

/*
 * Force a thread into the kernel if it is not already there.
 * This is a no-op on uniprocessors.
 */
/* ARGSUSED */
void
prpokethread(kthread_t *t)
{
	if (t->t_state == TS_ONPROC && t->t_cpu != CPU_GLOBAL)
		poke_cpu(t->t_cpu->cpu_id);
}

/*
 * Return general registers.
 */
void
prgetprregs(klwp_t *lwp, prgregset_t prp)
{
	ASSERT(MUTEX_NOT_HELD(&lwptoproc(lwp)->p_lock));

	getgregs(lwp, prp);
}

/*
 * Set general registers.
 * (Note: This can be an alias to setgregs().)
 */
void
prsetprregs(klwp_t *lwp, prgregset_t prp, int initial)
{
	if (initial) {		/* set initial values */
		lwptoregs(lwp)->r_cpsr = PSR_USERINIT;
	}
	(void) setgregs(lwp, prp);
}

#ifdef _SYSCALL32_IMPL
#error "port me"
/*
 * Convert prgregset32 to native prgregset
 */
void
prgregset_32ton(klwp_t *lwp, prgregset32_t src, prgregset_t dst)
{
}

/*
 * Return 32-bit general registers
 */
void
prgetprregs32(klwp_t *lwp, prgregset32_t prp)
{
}

#endif	/* _SYSCALL32_IMPL */

/*
 * Get the syscall return values for the lwp.
 */
int
prgetrvals(klwp_t *lwp, long *rval1, long *rval2)
{
	struct regs *r = lwptoregs(lwp);

	if (r->r_cpsr & PSR_C_BIT) {
		/* Syscall failed. r_r12 is errno. */
		return (r->r_r12);
	}

	if (lwp->lwp_eosys == JUSTRETURN) {
		*rval1 = 0;
		*rval2 = 0;
	} else if (lwp_getdatamodel(lwp) != DATAMODEL_NATIVE) {
		/*
		 * XX64	Not sure we -really- need to do this, because the
		 *	syscall return already masks off the bottom values ..?
		 */
		*rval1 = r->r_r0 & (uint32_t)0xffffffffu;
		*rval2 = r->r_r1 & (uint32_t)0xffffffffu;
	} else {
		*rval1 = r->r_r0;
		*rval2 = r->r_r1;
	}
	return (0);
}

/*
 * Does the system support floating-point, either through hardware
 * or by trapping and emulating floating-point machine instructions?
 */
int
prhasfp(void)
{
	extern int fp_kind;

	return (fp_kind != FP_NO);
}

/*
 * Get floating-point registers.
 */
void
prgetprfpregs(klwp_t *lwp, prfpregset_t *pfp)
{
	bzero(pfp, sizeof (prfpregset_t));
	getfpregs(lwp, pfp);
}

#if defined(_SYSCALL32_IMPL)
#error "port me"
void
prgetprfpregs32(klwp_t *lwp, prfpregset32_t *pfp)
{
}
#endif	/* _SYSCALL32_IMPL */

/*
 * Set floating-point registers.
 * (Note: This can be an alias to setfpregs().)
 */
void
prsetprfpregs(klwp_t *lwp, prfpregset_t *pfp)
{
	setfpregs(lwp, pfp);
}

#if defined(_SYSCALL32_IMPL)
#error "port me"
void
prsetprfpregs32(klwp_t *lwp, prfpregset32_t *pfp)
{
}
#endif	/* _SYSCALL32_IMPL */

/*
 * Does the system support extra register state?
 */
/* ARGSUSED */
int
prhasx(proc_t *p)
{
	return (0);
}

/*
 * Get the size of the extra registers.
 */
/* ARGSUSED */
int
prgetprxregsize(proc_t *p)
{
	return (0);
}

/*
 * Get extra registers.
 */
/*ARGSUSED*/
void
prgetprxregs(klwp_t *lwp, caddr_t prx)
{
	/* no extra registers */
}

/*
 * Set extra registers.
 */
/*ARGSUSED*/
void
prsetprxregs(klwp_t *lwp, caddr_t prx)
{
	/* no extra registers */
}

/*
 * Return the base (lower limit) of the process stack.
 */
caddr_t
prgetstackbase(proc_t *p)
{
	return (p->p_usrstack - p->p_stksize);
}

/*
 * Return the "addr" field for pr_addr in prpsinfo_t.
 * This is a vestige of the past, so whatever we return is OK.
 */
caddr_t
prgetpsaddr(proc_t *p)
{
	return ((caddr_t)p);
}

/*
 * Arrange to single-step the lwp.
 */
void
prstep(klwp_t *lwp, int watchstep)
{
	ASSERT(MUTEX_NOT_HELD(&lwptoproc(lwp)->p_lock));

	if (lwp->lwp_pcb.pcb_step == STEP_ACTIVE) {
		/* already set breakpoint. we must reset. */
		pr_clear_breakpoint(lwp);
	}

	lwp->lwp_pcb.pcb_step = STEP_REQUESTED;

	if (watchstep) {
		lwp->lwp_pcb.pcb_flags |= WATCH_STEP;
	} else { 
		lwp->lwp_pcb.pcb_flags |= NORMAL_STEP;
	}

	aston(lwptot(lwp));	/* let trap() */
}

/*
 * Undo prstep().
 */
void
prnostep(klwp_t *lwp)
{
	ASSERT(ttolwp(curthread) == lwp ||
	    MUTEX_NOT_HELD(&lwptoproc(lwp)->p_lock));

	pr_clear_breakpoint(lwp);
	lwp->lwp_pcb.pcb_step = STEP_NONE;

	lwp->lwp_pcb.pcb_flags &= ~(NORMAL_STEP|WATCH_STEP);
}

/*
 * Return non-zero if a single-step is in effect.
 */
int
prisstep(klwp_t *lwp)
{
	ASSERT(MUTEX_NOT_HELD(&lwptoproc(lwp)->p_lock));

	return (lwp->lwp_pcb.pcb_step != STEP_NONE);
}

/*
 * Set the PC to the specified virtual address.
 */
void
prsvaddr(klwp_t *lwp, caddr_t vaddr)
{
	struct regs *r = lwptoregs(lwp);

	ASSERT(MUTEX_NOT_HELD(&lwptoproc(lwp)->p_lock));

	r->r_pc = (uintptr_t)vaddr;
}

/*
 * Map address "addr" in address space "as" into a kernel virtual address.
 * The memory is guaranteed to be resident and locked down.
 */
caddr_t
prmapin(struct as *as, caddr_t addr, int writing)
{
	page_t *pp;
	caddr_t kaddr;
	pfn_t pfnum;

	/*
	 * XXX - Because of past mistakes, we have bits being returned
	 * by getpfnum that are actually the page type bits of the pte.
	 * When the object we are trying to map is a memory page with
	 * a page structure everything is ok and we can use the optimal
	 * method, ppmapin.  Otherwise, we have to do something special.
	 */
	pfnum = hat_getpfnum(as->a_hat, addr);
	if (pf_is_memory(pfnum)) {
		pp = page_numtopp_nolock(pfnum);
		if (pp != NULL) {
			ASSERT(PAGE_LOCKED(pp));
			kaddr = ppmapin(pp, writing ?
			    (PROT_READ | PROT_WRITE) : PROT_READ, (caddr_t)-1);
			return (kaddr + ((uintptr_t)addr & PAGEOFFSET));
		}
	}

	/*
	 * Oh well, we didn't have a page struct for the object we were
	 * trying to map in; ppmapin doesn't handle devices, but allocating a
	 * heap address allows ppmapout to free virtual space when done.
	 */
	kaddr = vmem_alloc(heap_arena, PAGESIZE, VM_SLEEP);

	hat_devload(kas.a_hat, kaddr, MMU_PAGESIZE,  pfnum,
	    writing ? (PROT_READ | PROT_WRITE) : PROT_READ, 0);

	return (kaddr + ((uintptr_t)addr & PAGEOFFSET));
}

/*
 * Unmap address "addr" in address space "as"; inverse of prmapin().
 */
/* ARGSUSED */
void
prmapout(struct as *as, caddr_t addr, caddr_t vaddr, int writing)
{
	extern void ppmapout(caddr_t);

	vaddr = (caddr_t)((uintptr_t)vaddr & PAGEMASK);
	ppmapout(vaddr);
}

/*
 * Prepare to single-step the lwp if requested.
 * This is called by the lwp itself just before returning to user level.
 */
void
prdostep(void)
{
	klwp_t *lwp = ttolwp(curthread);
	struct regs *r;
	proc_t *p;
	caddr_t pc;
	uint32_t instr;
	caddr_t addr;
	int ret = -1;

	ASSERT(lwp != NULL);
	r = lwptoregs(lwp);
	p = lwptoproc(lwp);

	ASSERT(r != NULL);

	if (lwp->lwp_pcb.pcb_step == STEP_NONE ||
	    lwp->lwp_pcb.pcb_step == STEP_ACTIVE) {
		return;
	}

	if (p->p_model == DATAMODEL_ILP32) {
		pc = (caddr_t)(uintptr_t)(caddr32_t)r->r_pc;
	} else {
		pc = (caddr_t)r->r_pc;
	}

	if (fuword32_nowatch((void *)pc, &instr) == 0) {
		if (pr_check_condition(lwp, instr) == 0) {
			addr = pc + 4;
		} else {
			addr = (caddr_t)
				pr_get_next_address(lwp, (uint32_t)pc, instr);
		}
		ret = pr_set_breakpoint(lwp, addr);
		if (ret != 0) {
			/* error occured. we can't set break point */
			return;
		}
		lwp->lwp_pcb.pcb_step = STEP_ACTIVE;
	}
}

/*
 * Wrap up single stepping of the lwp.
 * This is called by the lwp itself just after it has taken
 * the Break point.  We fix up the PC and instr to have their
 * proper values after the step.  We return 1 to indicate that
 * this fault really is the one we are expecting, else 0.
 *
 * This is also called from syscall() and stop() to reset PC
 * and instr to their proper values for debugger visibility.
 */
int
prundostep(void)
{
	klwp_t *lwp = ttolwp(curthread);
	proc_t *p = ttoproc(curthread);
	struct regs *r;
	int rc = 0;
	int i;
	caddr_t pc;

	ASSERT(lwp != NULL);

	if (lwp->lwp_pcb.pcb_step == STEP_ACTIVE) {
		r = lwptoregs(lwp);

		ASSERT(r != NULL);

		if (p->p_model == DATAMODEL_ILP32) {
			pc = (caddr_t)(uintptr_t)(caddr32_t)r->r_pc;
		} else {
			pc = (caddr_t)r->r_pc;
		}

		if (pc == lwp->lwp_pcb.pcb_bpinfo.addr) {
			rc = 1; /* expecting */
		}
		pr_clear_breakpoint(lwp);
		lwp->lwp_pcb.pcb_step = STEP_WASACTIVE;
	}

	return (rc);
}

/*
 * Make sure the lwp is in an orderly state
 * for inspection by a debugger through /proc.
 * Called from stop() and from syslwp_create().
 */
/* ARGSUSED */
void
prstop(int why, int what)
{
	klwp_t *lwp = ttolwp(curthread);
	struct regs *r = lwptoregs(lwp);

	/*
	 * Make sure we don't deadlock on a recursive call
	 * to prstop().  stop() tests the lwp_nostop flag.
	 */
	ASSERT(lwp->lwp_nostop == 0);
	lwp->lwp_nostop = 1;

	if (copyin_nowatch((caddr_t)r->r_pc, &lwp->lwp_pcb.pcb_instr,
		    sizeof (lwp->lwp_pcb.pcb_instr)) == 0)
		lwp->lwp_pcb.pcb_flags |= INSTR_VALID;
	else {
		lwp->lwp_pcb.pcb_flags &= ~INSTR_VALID;
		lwp->lwp_pcb.pcb_instr = 0;
	}

	(void) save_syscall_args();
	ASSERT(lwp->lwp_nostop == 1);
	lwp->lwp_nostop = 0;
}

/*
 * Fetch the user-level instruction on which the lwp is stopped.
 * It was saved by the lwp itself, in prstop().
 * Return non-zero if the instruction is valid.
 */
int
prfetchinstr(klwp_t *lwp, ulong_t *ip)
{
	*ip = (ulong_t)lwp->lwp_pcb.pcb_instr;
	return (lwp->lwp_pcb.pcb_flags & INSTR_VALID);
}

/*
 * Called from trap() when a load or store instruction
 * falls in a watched page but is not a watchpoint.
 * We emulate the instruction in the kernel.
 */
/* ARGSUSED */
int
pr_watch_emul(struct regs *rp, caddr_t addr, enum seg_rw rw)
{
#ifdef SOMEDAY
	int res;
	proc_t *p = curproc;
	char *badaddr = (caddr_t)(-1);
	int mapped;

	/* prevent recursive calls to pr_watch_emul() */
	ASSERT(!(curthread->t_flag & T_WATCHPT));
	curthread->t_flag |= T_WATCHPT;

	watch_disable_addr(addr, 8, rw);
	res = do_unaligned(rp, &badaddr);
	watch_enable_addr(addr, 8, rw);

	curthread->t_flag &= ~T_WATCHPT;
	if (res == SIMU_SUCCESS) {
		/* adjust the pc */
		return (1);
	}
#endif
	return (0);
}
