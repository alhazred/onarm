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

#pragma ident	"@(#)archdep.c"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/vmparam.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/stack.h>
#include <sys/regset.h>
#include <sys/privregs.h>
#include <sys/frame.h>
#include <sys/proc.h>
#include <sys/siginfo.h>
#include <sys/cpuvar.h>
#include <sys/asm_linkage.h>
#include <sys/kmem.h>
#include <sys/errno.h>
#include <sys/bootconf.h>
#include <sys/archsystm.h>
#include <sys/debug.h>
#include <sys/elf.h>
#include <sys/elf_ARM.h>
#include <sys/spl.h>
#include <sys/time.h>
#include <sys/atomic.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/modctl.h>
#include <sys/kobj.h>
#include <sys/panic.h>
#include <sys/reboot.h>
#include <sys/time.h>
#include <sys/fp.h>
#include <sys/auxv.h>
#include <sys/dtrace.h>
#include <sys/mpcore.h>
#include <sys/clock.h>
#include <sys/fp_impl.h>
#include <asm/cpufunc.h>

#if defined(_SYSCALL32_IMPL)
#error "port me if LP64 is available."
#endif	/* _SYSCALL32_IMPL */

extern int one_sec;
extern int max_hres_adj;

/*
 * Install exception vectors to specified va
 *
 * Assumptions:
 *   - Using high-vectors, Control Register (in CP15 c1) V bit is already set.
 *   - Page table for va is already created.
 */
extern void	exception_vectors(void);
extern size_t	exception_vectors_size;
void
install_exception_vectors(caddr_t va)
{
#ifdef	DEBUG
	uint32_t ctrlreg = READ_CP15(0, c1, c0, 0);
	uint32_t v = ctrlreg & (1 << 13);

	if (va == (caddr_t)ARM_VECTORS_HIGH) {
		if (!v) {
			panic("Invalid location of exception vectors %p", va);
		}
	} else if (va == (caddr_t)ARM_VECTORS_NORMAL) {
		if (v) {
			panic("Invalid location of exception vectors %p", va);
		}
	} else {
		/* va must be ARM_VECTORS_NORMAL or ARM_VECTORS_HIGH */
		panic("Invalid va %p", va);
	}
#endif	/* DEBUG */

	/* copy vector's instructions and data. */
	bcopy((caddr_t)exception_vectors, va, exception_vectors_size);

	sync_icache(va, exception_vectors_size);
}

/*
 * Set floating-point registers from a native fpregset_t.
 */
void
setfpregs(klwp_t *lwp, fpregset_t *fp)
{
	struct fpu_ctx *fpu = &lwp->lwp_pcb.pcb_fpu;

	kpreempt_disable();

	bcopy(fp, &fpu->fpu_regs, sizeof(fpregset_t));
	fpu->fpu_regs.fp_fpexc = FPEXC_EN;
	fpu->fpu_regs.fp_fpinst = 0;
	fpu->fpu_regs.fp_fpinst2 = 0;
	fpu->fpu_flags |= FPU_VALID;

	if (fpu->fpu_flags & FPU_EN) {
		/*
		 * If we are changing the fpu_flags in the current context,
		 * disable floating point unit.
		 */
		if (lwp == ttolwp(curthread)) {
			FP_DISABLE(lwp);
		}
	} else {
		/*
		 * If we are trying to change the FPU state of a thread which
		 * hasn't yet initialized floating point, store the state in
		 * the pcb and indicate that the state is valid.  When the
		 * thread enables floating point, it will use this state instead
		 * of the default state.
		 */
	}

	kpreempt_enable();
}

/*
 * Get floating-point registers into a native fpregset_t.
 */
void
getfpregs(klwp_t *lwp, fpregset_t *fp)
{
	struct fpu_ctx *fpu = &lwp->lwp_pcb.pcb_fpu;

	kpreempt_disable();
	if (fpu->fpu_flags & FPU_EN) {
		/*
		 * If we have FPU hw and the thread's pcb doesn't have
		 * a valid FPU state then get the state from the hw.
		 */
		if (fpu_exists && ttolwp(curthread) == lwp &&
		    !(fpu->fpu_flags & FPU_VALID)) {
			/* get the current FPU state */
			if (FP_SAVE_USER(fp) == B_TRUE) {
				kpreempt_enable();
				return;
			}
		}
	}

	/*
	 * There are 3 possible cases we have to be aware of here:
	 *
	 * 1. FPU is enabled.  FPU state is stored in the current LWP.
	 *
	 * 2. FPU is not enabled, and there have been no intervening /proc
	 *    modifications.  Return initial FPU state.
	 *
	 * 3. FPU is not enabled, but a /proc consumer has modified FPU state.
	 *    FPU state is stored in the current LWP.
	 */
	if ((fpu->fpu_flags & FPU_EN) || (fpu->fpu_flags & FPU_VALID)) {
		/*
		 * Cases 1 and 3.
		 */
		bcopy(&fpu->fpu_regs, fp, sizeof(fpregset_t));
	} else {
		/*
		 * Case 2.
		 */
		bzero(fp, sizeof(fpregset_t));
		fp->fp_u.fp_vfp.fp_fpscr = FPSCR_INIT;
	}
	kpreempt_enable();
}

#if defined(_SYSCALL32_IMPL)

/*
 * Set floating-point registers from an fpregset32_t.
 */
void
setfpregs32(klwp_t *lwp, fpregset32_t *fp)
{
	fpregset_t fpregs;

	/* fpregset_32ton(fp, &fpregs); */
	setfpregs(lwp, &fpregs);
}

/*
 * Get floating-point registers into an fpregset32_t.
 */
void
getfpregs32(klwp_t *lwp, fpregset32_t *fp)
{
	fpregset_t fpregs;

	getfpregs(lwp, &fpregs);
	/* fpregset_nto32(&fpregs, fp); */
}

#endif	/* _SYSCALL32_IMPL */

/*
 * Return the general registers
 */
void
getgregs(klwp_t *lwp, gregset_t grp)
{
	struct regs *rp = lwptoregs(lwp);
	bcopy(rp, grp, sizeof(greg_t) * 15);	/* copy r0 - r14 */
	grp[REG_R15]  = rp->r_r15;
	grp[REG_CPSR] = rp->r_cpsr;
	if (lwp == ttolwp(curthread)) {
		grp[REG_TP] = READ_CP15(0, c13, c0, 2);
	} else {
		grp[REG_TP] = lwp->lwp_pcb.pcb_usertp;
	}
}

#if defined(_SYSCALL32_IMPL)

void
getgregs32(klwp_t *lwp, gregset32_t grp)
{
	struct regs *rp = lwptoregs(lwp);

	grp[REG_R0]   = (greg32_t)rp->r_r0;
	grp[REG_R1]   = (greg32_t)rp->r_r1;
	grp[REG_R2]   = (greg32_t)rp->r_r2;
	grp[REG_R3]   = (greg32_t)rp->r_r3;
	grp[REG_R4]   = (greg32_t)rp->r_r4;
	grp[REG_R5]   = (greg32_t)rp->r_r5;
	grp[REG_R6]   = (greg32_t)rp->r_r6;
	grp[REG_R7]   = (greg32_t)rp->r_r7;
	grp[REG_R8]   = (greg32_t)rp->r_r8;
	grp[REG_R9]   = (greg32_t)rp->r_r9;
	grp[REG_R10]  = (greg32_t)rp->r_r10;
	grp[REG_R11]  = (greg32_t)rp->r_r11;
	grp[REG_R12]  = (greg32_t)rp->r_r12;
	grp[REG_R13]  = (greg32_t)rp->r_r13;
	grp[REG_R14]  = (greg32_t)rp->r_r14;
	grp[REG_R15]  = (greg32_t)rp->r_r15;
	grp[REG_CPSR] = (greg32_t)rp->r_cpsr;
	if (lwp == ttolwp(curthread)) {
		grp[REG_TP] = READ_CP15(0, c13, c0, 2);
	} else {
		grp[REG_TP] = lwp->lwp_pcb.pcb_usertp;
	}
}

void
ucontext_32ton(const ucontext32_t *src, ucontext_t *dst)
{
}

#endif	/* _SYSCALL32_IMPL */

/*
 * Return the user-level PC.
 * If in a system call, return the address of the syscall trap.
 */
greg_t
getuserpc()
{
	greg_t upc = lwptoregs(ttolwp(curthread))->r_pc;

	if (curthread->t_sysnum == 0) {
		return upc;
	}
	return upc - 4;
}

/*
 * Set general registers.
 */
void
setgregs(klwp_t *lwp, gregset_t grp)
{
	struct regs *rp = lwptoregs(lwp);
	bcopy(grp, rp, sizeof(greg_t) * 15);	/* copy r0 - r14 */
	rp->r_r15  = grp[REG_R15];
	rp->r_cpsr = (rp->r_cpsr & ~PSR_USERMASK) |
			(grp[REG_CPSR] & PSR_USERMASK);
	if (lwp == ttolwp(curthread)) {
		WRITE_CP15(0, c13, c0, 2, grp[REG_TP]);
	} else {
		lwp->lwp_pcb.pcb_usertp = grp[REG_TP];
	}
}

/*
 * Get a pc-only stacktrace.  Used for kmem_alloc() buffer ownership tracking.
 * Returns MIN(current stack depth, pcstack_limit).
 */
int
getpcstack(pc_t *pcstack, int pcstack_limit)
{
	/* This is not supported, because fp register does not exist 
	   on ARM architecture. */
	return 0;
}

/*
 * The following ELF header fields are defined as processor-specific
 * in the V8 ABI:
 *
 *	e_ident[EI_DATA]	encoding of the processor-specific
 *				data in the object file
 *	e_machine		processor identification
 *	e_flags			processor-specific flags associated
 *				with the file
 */

/*
 * The value of at_flags reflects a platform's cpu module support.
 * at_flags is used to check for allowing a binary to execute and
 * is passed as the value of the AT_FLAGS auxiliary vector.
 */
int at_flags = 0;

/* Check ELF flags */
#if	defined(__ARM_EABI__) && !defined(ARM_OABI_USER)
#define	ARMELF_EABI_VALID_VERSION	EF_ARM_EABI_VER4	
#else	/* defined(__ARM_EABI__) && !defined(ARM_OABI_USER) */
#define	ARMELF_EABI_VALID_VERSION	0
#endif	/* defined(__ARM_EABI__) && !defined(ARM_OABI_USER) */

#define	ARMELF_EABI_VERSION_MATCH(flags)				\
	(((flags) & EF_ARM_EABIMASK) == ARMELF_EABI_VALID_VERSION)

#define	ARMELF_INVALID_FLAGS	(EF_ARM_NEW_ABI|EF_ARM_OLD_ABI)

#define	ARMELF_FLAGS_VALID(flags)			\
	(ARMELF_EABI_VERSION_MATCH(flags) &&		\
	 (((flags) & ARMELF_INVALID_FLAGS) == 0))

/*
 * Check the processor-specific fields of an ELF header.
 *
 * returns 1 if the fields are valid, 0 otherwise
 */
/*ARGSUSED2*/
int
elfheadcheck(
	unsigned char e_data,
	Elf32_Half e_machine,
	Elf32_Word e_flags)
{
	if (e_data != ELFDATA2LSB)
		return (0);

	if (!ARMELF_FLAGS_VALID(e_flags)) {
		return 0;
	}

	return (e_machine == EM_ARM);
}

uint_t auxv_hwcap_include = 0;	/* patch to enable unrecognized features */
uint_t auxv_hwcap_exclude = 0;	/* patch for broken cpus, debugging */
#if defined(_SYSCALL32_IMPL)
uint_t auxv_hwcap32_include = 0;	/* ditto for 32-bit apps */
uint_t auxv_hwcap32_exclude = 0;	/* ditto for 32-bit apps */
#endif

/*
 * Gather information about the processor and place it into auxv_hwcap
 * so that it can be exported to the linker via the aux vector.
 *
 * We use this seemingly complicated mechanism so that we can ensure
 * that /etc/system can be used to override what the system can or
 * cannot discover for itself.
 */
void
bind_hwcap(void)
{
	uint_t cpu_hwcap_flags = 0;

	auxv_hwcap = (auxv_hwcap_include | cpu_hwcap_flags) &
	    ~auxv_hwcap_exclude;

#if 0
	if (auxv_hwcap_include || auxv_hwcap_exclude)
		cmn_err(CE_CONT, "?user ABI extensions: %b\n",
		    auxv_hwcap, FMT_AV_386);
#endif

#if defined(_SYSCALL32_IMPL)
	auxv_hwcap32 = (auxv_hwcap32_include | cpu_hwcap_flags) &
		~auxv_hwcap32_exclude;

#if 0
	if (auxv_hwcap32_include || auxv_hwcap32_exclude)
		cmn_err(CE_CONT, "?32-bit user ABI extensions: %b\n",
		    auxv_hwcap32, FMT_AV_386);
#endif
#endif
}

int
__ipltospl(int ipl)
{
	return (ipltospl(ipl));
}

/*
 * The panic code invokes panic_saveregs() to record the contents of a
 * regs structure into the specified panic_data structure for debuggers.
 */
void
panic_saveregs(panic_data_t *pdp, struct regs *rp)
{
	panic_nv_t *pnv = PANICNVGET(pdp);

	PANICNVADD(pnv, "r0", (uint32_t)rp->r_r0);
	PANICNVADD(pnv, "r1", (uint32_t)rp->r_r1);
	PANICNVADD(pnv, "r2", (uint32_t)rp->r_r2);
	PANICNVADD(pnv, "r3", (uint32_t)rp->r_r3);
	PANICNVADD(pnv, "r4", (uint32_t)rp->r_r4);
	PANICNVADD(pnv, "r5", (uint32_t)rp->r_r5);
	PANICNVADD(pnv, "r6", (uint32_t)rp->r_r6);
	PANICNVADD(pnv, "r7", (uint32_t)rp->r_r7);
	PANICNVADD(pnv, "r8", (uint32_t)rp->r_r8);
	PANICNVADD(pnv, "r9", (uint32_t)rp->r_r9);
	PANICNVADD(pnv, "r10", (uint32_t)rp->r_r10);
	PANICNVADD(pnv, "r11", (uint32_t)rp->r_r11);
	PANICNVADD(pnv, "r12", (uint32_t)rp->r_r12);
	PANICNVADD(pnv, "r13", (uint32_t)rp->r_r13);
	PANICNVADD(pnv, "r14", (uint32_t)rp->r_r14);
	PANICNVADD(pnv, "r15", (uint32_t)rp->r_r15);
	PANICNVADD(pnv, "cpsr", (uint32_t)rp->r_cpsr);

	/* CP15 registers */
	PANICNVADD(pnv, "control", READ_CP15(0, c1,  c0, 0));
	PANICNVADD(pnv, "auxctrl", READ_CP15(0, c1,  c0, 1));
	PANICNVADD(pnv, "cpacl",   READ_CP15(0, c1,  c0, 2));
	PANICNVADD(pnv, "tidusr",  READ_CP15(0, c13, c0, 2));
	PANICNVADD(pnv, "tidknl",  READ_CP15(0, c13, c0, 4));

	PANICNVSET(pdp, pnv);
}

/*
 * Determine whether pc is the end of stack.
 * We do this by comparing the address to the addresses of several
 * well-known routines.
 */
extern void _sys_rtt();
extern void user_rtt();
extern void svc_rtt();
extern void trap();
extern void switrap();
extern void fiqtrap();
extern void irqtrap();
extern void dosoftint();
extern void intr_thread();
extern void thread_start();

static uintptr_t tracestopfunc[] = {
	(uintptr_t)_sys_rtt,
	(uintptr_t)user_rtt,
	(uintptr_t)svc_rtt,
	(uintptr_t)trap,
	(uintptr_t)switrap,
	(uintptr_t)fiqtrap,
	(uintptr_t)irqtrap,
	(uintptr_t)dosoftint,
	(uintptr_t)intr_thread,
	(uintptr_t)thread_start
};

#define	TRACE_DEPTH	10
#define	DECODE_INSTR	10

/*
 * Print a stack backtrace using the specified stack pointer.  We delay two
 * seconds before continuing, unless this is the panic traceback.
 */
void
traceback(caddr_t sp, uintptr_t pc, uintptr_t lr)
{
	caddr_t minsp, stacktop;
	int on_intr;
	cpu_t *cpu;
	int depth = 0;
	uintptr_t sympc;

	if (!panicstr) {
		printf("traceback: %%sp = %p\n", (void *)sp);
	}

	/*
	 * If we are panicking, the high-level interrupt information in
	 * CPU was overwritten.  panic_cpu has the correct values.
	 */
	kpreempt_disable();			/* prevent migration */

	cpu = (panicstr && CPU->cpu_id == panic_cpu.cpu_id)? &panic_cpu : CPU;

	if ((on_intr = CPU_ON_INTR(cpu)) != 0) {
		stacktop = (cpu->cpu_intr_stack + SA(MINFRAME));
	} else {
		stacktop = curthread->t_stk;
	}

	kpreempt_enable();

	sympc = pc;
	minsp = sp;

	while ((uintptr_t)sp >= KERNELBASE && depth < TRACE_DEPTH) {
		ulong_t off;
		char *sym;
		uintptr_t instaddr;
		int spsize, lroff, i;

		if (sp < minsp || sp > stacktop) {
			if (on_intr) {
				/*
				 * Hop from interrupt stack to thread stack.
				 */
				/* This is not supported, because fp register 
				   does not exist on ARM architecture. */
				printf("  >> end of interrupt stack\n");
				on_intr = 0;
				continue;
			}
			printf("  >> out of range (sp = %p)\n", (void *)sp);
			break;	/* we're outside of the expected range */
		}

		if ((uintptr_t)sp & (STACK_ALIGN - 1)) {
			printf("  >> mis-aligned %%sp = %p\n", (void *)sp);
			break;
		}

		if ((sym = kobj_getsymname(sympc, &off)) != NULL) {
			printf("%08lx %s:%s+%lx ()\n", (uintptr_t)sp,
				mod_containing_pc((caddr_t)sympc), sym, off);
		} else {
			printf("%08lx ???:%p ()\n", (uintptr_t)sp, (void *)pc);
			break;
		}
		instaddr = sympc - off;	/* start address of the symbol */

		/* Check stack end */
		for (i = 0; i < sizeof(tracestopfunc)/sizeof(uintptr_t); i++) {
			if (tracestopfunc[i] == instaddr) {
				printf("  >> detect stack-end function\n");
				goto out;
			}
		}
		if (sp >= stacktop) {
			printf("  >> out of range (sp = %p)\n", (void *)sp);
			break;	/* we're outside of the expected range */
		}

		/*
		 * Decode instruction in order to determine stack size
		 * and offset saved lr.
		 */
		spsize = 0;
		lroff = -1;
		for (i = 0; i < DECODE_INSTR; i++) {
			uint32_t inst;
			int	off = lroff, spsz = spsize;

			if (instaddr >= pc) {
				break;
			}
			/* fetch instruction */
			inst = *(uint32_t *)instaddr;
			/* decode instruction */
			dis_stacktrace(inst, &spsz, &off);
			if (off > lroff) {
				lroff = off;
			}
			if (spsz > spsize) {
				spsize = spsz;
			}
			instaddr += 4;
		}

		if (lroff == -1) {
			pc = lr;
			lr = -1;	/* invalidate */
		} else {
			/* load lr saved on stack */
			pc = *(uintptr_t *)(sp + lroff);
		}
		sympc = pc - 4;
		minsp = sp;
		sp += spsize;
		depth++;
	}

out:
	if (!panicstr) {
		printf("end of traceback\n");
		DELAY(2 * MICROSEC);
	}
}

/*
 * Generate a stack backtrace from a saved register set.
 */
void
traceregs(struct regs *rp)
{
	traceback((caddr_t)rp->r_sp, rp->r_pc, rp->r_lr);
}

void
exec_set_sp(size_t stksize)
{
	klwp_t *lwp = ttolwp(curthread);

	lwptoregs(lwp)->r_sp = (uintptr_t)curproc->p_usrstack - stksize;
}

hrtime_t
gethrtime_waitfree(void)
{
	return (dtrace_gethrtime());
}

hrtime_t
gethrtime(void)
{
	ASSERT_STACK_ALIGNED();
	return (scucnt_gethrtime());
}

hrtime_t
gethrtime_unscaled(void)
{
	ASSERT_STACK_ALIGNED();
	return (scucnt_gethrtimeunscaled());
}

void
scalehrtime(hrtime_t *hrt)
{
	scucnt_scalehrtime(hrt);
}

void
gethrestime(timespec_t *tp)
{
	gethrestimef(tp);
}

void
__adj_hrestime(void)
{
	long long adj;

	if (hrestime_adj == 0)
		adj = 0;
	else if (hrestime_adj > 0) {
		if (hrestime_adj < max_hres_adj)
			adj = hrestime_adj;
		else
			adj = max_hres_adj;
	} else {
		if (hrestime_adj < -max_hres_adj)
			adj = -max_hres_adj;
		else
			adj = hrestime_adj;
	}

	timedelta -= adj;
	hrestime_adj = timedelta;
	hrestime.tv_nsec += adj;

	while ((unsigned long)hrestime.tv_nsec >= NANOSEC) {
		one_sec++;
		hrestime.tv_sec++;
		hrestime.tv_nsec -= NANOSEC;
	}
}

/*
 * void
 * arm_fiq_init(cpu_t *cp)
 *	Initialize FIQ mode stack for the current CPU.
 *	"cp" must point cpu structure for the current CPU.
 *
 * Remarks:
 *	The caller must set FIQ mode stack address into cp->cpu_fiq_modestack
 *	in advance.
 */
void
arm_fiq_init(cpu_t *cp)
{
	char	*bottom;
	uint_t	*lastp;
	extern void	arm_set_modestack(uint_t mode, void *sp);

	/*
	 * Set zero at the bottom of stack.
	 * It will be used as FIQ nest counter.
	 */
	bottom = cp->cpu_fiq_modestack + FIQ_MODE_STACKSZ - sizeof(long);
	lastp = (uint_t *)((void *)bottom);
	*lastp = 0;

	/* Set FIQ mode stack pointer. */
	arm_set_modestack(PSR_MODE_FIQ, lastp);
}

#ifdef	DEBUG
/*
 * void
 * assert_stack_aligned(uintptr_t addr, size_t align)
 *	Check whether stack is aligned properly.
 *	Verification of stack alignment must be done by function in order to
 *	avoid unexpected optimization.
 */
void
assert_stack_aligned(uintptr_t addr, size_t align)
{
	ASSERT((addr & (align - 1)) == 0);
}
#endif	/* DEBUG */

/*
 * This function is currently unused on arm.
 */
/*ARGSUSED*/
void
lwp_attach_brand_hdlrs(klwp_t *lwp)
{
}
