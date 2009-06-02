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
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *
 *	Copyright 2000-2002 Sun Microsystems, Inc.  All rights reserved.
 *	Use is subject to license terms.
 */

/*
 * Copyright (c) 2007-2008 NEC Corporation
 */

	.ident	"@(#)boot_elf.s	1.17	05/06/08 SMI"

#if	defined(lint)

#include	<sys/types.h>
#include	"_rtld.h"
#include	"_audit.h"
#include	"_elf.h"

/* ARGSUSED0 */
int
elf_plt_trace()
{
	return (0);
}
#else

/*
 * PLT functions trace routine invoked if AUDIT PLT enter or exit is enabled
 */
	.file	"boot_elf.s"

#include	<link.h>
#include	<sys/asm_linkage.h>
#include	"_audit.h"

/*
 * On entry the 'glue code' has loaded dyndata pointer to ip register.
 *
 *	ldr	ip, [pc, #0]		  @ load dyndata pointer
 *	b	elf_plt_trace		  @ jump to elf_plt_trace routine
 *	.word	dyndata			  @ word area to hold dyndata
 *
 * dyndata currently contains:
 *	[dyndata]
 *	0x0	uintptr_t	reflmp
 *	0x4	uintptr_t	deflmp
 *	0x8	uint_t		symndx
 *	0xc	uint_t		sb_flags
 *	0x10	Sym		symdef.st_name
 *	0x14			symdef.st_value
 *	0x18			symdef.st_size
 *	0x1c			symdef.st_info
 *	0x1d			symdef.st_other
 *	0x1e			symdef.st_shndx
 */

#define	REFLMP_OFF		0x0
#define	DEFLMP_OFF		0x4
#define	SYMNDX_OFF		0x8
#define	SBFLAGS_OFF		0xc
#define	SYMDEF_OFF		0x10
#define	SYMDEF_VALUE_OFF	0x14

	ENTRY_NP(elf_plt_trace)		/* do not need to profile */

	stmdb	sp!, {r0-r9, sl, fp, lr}
	add	fp, sp, #52		/* fp points to start of stack frame */
	sub	sp, sp, #28		/* create some local storage */

	/*
	 * Local stack space storage looks like this :
	 *
	 *	--- start of saved registers area ---------------------
	 *	[fp -4]		store lr
	 *	[fp -8]		store fp
	 *	[fp -12]	store sl
	 *	[fp -16]	store r9
	 *	[fp -20]	store r8
	 *	[fp -24]	store r7
	 *	[fp -28]	store r6
	 *	[fp -32]	store r5
	 *	[fp -36]	store r4
	 *	[fp -40]	store r3
	 *	[fp -44]	store r2
	 *	[fp -48]	store r1
	 *	[fp -52]	store r0
	 *	--- start of local storage ----------------------------
	 *	[fp -56]	(padding so that sp aligns at double word)
	 *		=== ARM regset area for audit_pltenter() ===
	 *	[fp -60]	lr (return address to the caller)
	 *	[fp -64]	sp (stack pointer of caller)
	 *	[fp -68]	r3 (arg4 of caller)
	 *	[fp -72]	r2 (arg3 of caller)
	 *	[fp -76]	r1 (arg2 of caller)
	 *	[fp -80]	r0 (arg1 of caller)
	 *		=== End of ARM regset area =================
	 *	--- end of local storage ------------------------------
	 *
	 * NOTE: According to AAPCS (Procedure Call Standard for the ARM
	 *	 Architecture), r11 register is NOT a role of fp (frame
	 *	 pointer) but a one of variable register.
	 *	 So we CANNOT expect that r11 points stack top of a caller.
	 * NOTE: Only in elf_plt_trace(), we locally use r11 as fp to access
	 *	 our stack frame easily.
	 *
	 * Usage of registers:
	 *	r4 = dyndata pointer
	 *	r5 = address of call destination (default is symdef.st_value)
	 *	fp = frame pointer
	 */

	mov	r4, ip
	ldr	r5, [r4, #SYMDEF_VALUE_OFF]

	/*
	 * Check whether plt enter processing is needed or not.
	 * When one of the following conditions has true, we bypass plt enter 
	 * processing:
	 *   (1) An audit library does NOT provide la_arm_pltenter().
	 *	 (If it is provided, audit_flags contains AF_PLTENTER.)
	 *   (2) Per function tracing is disabled.
	 *       (LA_SYMB_NOPLTENTER flag is able to set per-function basis.)
	 */
	ldrd	r6, .Lgot
.Lref_got:
	add	sl, pc, r6		/* sl = address of ld.so's GOT */
	ldr	r7, [sl, r7]
	ldr	r6, [r7]		/* r6 = audit_flags */
	ldr	r7, [r4, #SBFLAGS_OFF]	/* r7 = dyndata.sb_flags */

	tst	r6, #AF_PLTENTER	/* if (!(audit_flags & AF_PLTENTER)) */
	beq	.Lend_pltenter		/* 	goto .Lend_pltenter;         */

	tst	r7, #LA_SYMB_NOPLTENTER	/* if (sb_flags & LA_SYMB_NOPLTENTER)*/
	bne	.Lend_pltenter		/* 	goto .Lend_pltenter;         */

/*
 * STARTING PLT ENTER PROCESSING
 */
.Lstart_pltenter:
	/*
	 * Save some registers into registers area(regs*) needed for
	 * plt enter auditing.
	 */
	strd	r0, [fp, #-80]		/* copy caller's r0 and r1 */
	strd	r2, [fp, #-72]		/* copy caller's r2 and r3 */
	str	fp, [fp, #-64]		/* copy caller's stack top */
	str	lr, [fp, #-60]		/* copy caller's lr */

	/*
	 * Load arguments for audit_plteneter().
	 * Prototype is:
	 * Addr
	 * audit_pltenter(Rt_map *rlmp, Rt_map *dlmp, Sym *sym, uint_t ndx,
	 *   void *regs, uint_t *flags)
	 */
	ldrd	r0, [r4, #REFLMP_OFF]	/* set arg1 (dyndata.rlmp) and */
					/*     arg2 (dyndata.dlmp) */
	add	r2, r4, #SYMDEF_OFF	/* set arg3 (&dyndata.sym) */
	ldr	r3, [r4, #SYMNDX_OFF]	/* set arg4 (dyndata.symndx) */
	sub	r8, fp, #80		/* r8 = arg5 (ARM regset area) */
	add	r9, r4, #SBFLAGS_OFF	/* r9 = arg6 (&dyndata.sb_flags) */
	strd	r8, [sp, #-8]!		/* set arg5 and arg6 onto stack */
	bl	audit_pltenter		/* invoke the audit_pltenter function */
	add	sp, sp, #8
	mov	r5, r0			/* save the call destination */
					/* audit_pltenter might returns other */
					/* than sym.st_value for intentional */
					/* redirection. */

.Lend_pltenter:
	/*
	 * Check whether plt exit processing is needed or not.
	 * When one of the following conditions has true, we bypass plt exit 
	 * processing:
	 *   (1) An audit library does NOT provide la_pltexit().
	 *	 (If it is provided, audit_flags contains AF_PLTEXIT.)
	 *   (2) Per function tracing is disabled.
	 *       (LA_SYMB_NOPLTEXIT flag is able to set per-function basis.)
	 *   (3) audit_argcnt is less than 0.
	 *       (audit_argcnt is declared in common/globals.c. It is able to
	 *       change by define LD_AUDIT_ARGS enviroment variable.) 
	 */
	tst	r6, #AF_PLTEXIT		/* if (!(audit_flags & AF_PLTEXIT))   */
	beq	.Lbypass_pltexit	/* 	goto .Lbypass_pltexit;        */

	tst	r7, #LA_SYMB_NOPLTEXIT	/* if (sb_flags & LA_SYMB_NOPLTEXIT)  */
	bne	.Lbypass_pltexit	/* 	goto .Lbypass_pltexit;        */

	ldr	r6, .Laudit_argcnt
	ldr	r6, [sl, r6]		/* sl = address of ld.so's GOT */
	ldr	r6, [r6]		/* r6 = audit_argcnt */
	cmp	r6, #0			/* if (audit_argcnt > 0)           */
	bgt	.Lstart_pltexit		/*	goto .Lstart_pltexit;      */

.Lbypass_pltexit:
	/*
	 * No PLTEXIT processing required.
	 * We clean up our stack frame and restore original registers.
	 * And then jump to the final destination.
	 */
	mov	ip, r5
	add	sp, sp, #28		/* Clean up local storage */
	ldmia	sp!, {r0-r9, sl, fp, lr}/* Restore all saved registers */
	bx	ip			/* and jump to the final destination. */
	/* NOTREACHED */

/*
 * STARTING PLT EXIT PROCESSING
 */
.Lstart_pltexit:
	/*
	 * In order to call the destination procedure and then return
	 * to audit_pltexit() for post analysis, we must first grow
	 * our stack frame and then duplicate the original callers
	 * arguments on the callers stack frame.
	 * On ARM EABI, however, we cannot to get caller's stack frame size
	 * easily because we don't use fp(r11) as frame pointer.
	 * So we copy caller's stack with fixed size which is specified
	 * by audit_argcnt.
	 * NOTE: Because of this limitation, there is a possibility that
	 *       stacked arguments of a original caller are copied PARTIALLY.
	 *       To avoid this troublesome situation, you can set the 
	 *       max number (which is assumption number in actual) of stacked
	 *       arguments to LD_AUDIT_ARGS enviroment variable.
	 */
	mov	r6, r6, lsl #2
	add	r6, r6, #7		/* r6 = double word rounded byte size */
	bic	r6, r6, #7		/*      of arguments to copy. */

	/*
	 * Grow the stack and duplicate the arguments of the original caller.
	 */
	sub	sp, sp, r6		/* grow the stack */
	mov	r1, r6
	mov	r2, sp			/* r2 = dest (duplicated stack top) */
	mov	r3, fp			/* r3 = src (caller's stack top) */
1:
	cmp	r1, #0			/* while (size > 0) {     */
	ldrgtd	r8, [r3], #8		/*	*dest++ = *src++; */
	strgtd	r8, [r2], #8		/*	*dest++ = *src++; */
	subgt	r1, r1, #8		/*	size -= 8;        */
	bgt	1b			/* }                      */

	/*
	 * Restore the original destination call arguments
	 * and jump to the destination.
	 */
	sub	r7, fp, #52		/* Get start address of saved regs */
	ldmia	r7, {r0-r3}		/* Load all original register args */
	blx	r5			/* and jump to it. */

	/*
	 * Store return value to saved area.
	 * Please note that we must store all argument registers which are
	 * used for return value greater than 32-bit.
	 */
	strd	r0, [fp, #-52]
	strd	r2, [fp, #-44]

	/*
	 * Clean up duplicated stack.
	 */
	add	sp, sp, r6

	/*
	 * Load arguments for audit_pltexit() and call that function.
	 * Prototype is:
	 * Addr
	 * audit_pltexit(uintptr_t retval, Rt_map *rlmp, Rt_map *dlmp,
	 *   Sym *sym, uint_t ndx)
	 */
					/* set arg1 (r0 of dest. func) */
	ldr	r1, [r4, #REFLMP_OFF]	/* set arg2 (dyndata.rlmp)   */
	ldr	r2, [r4, #DEFLMP_OFF]	/* set arg3 (dyndata.dlmp)   */
	add	r3, r4, #SYMDEF_OFF	/* set arg4 (&dyndata.symp)  */
	ldr	r7, [r4, #SYMNDX_OFF]
	str	r7, [sp, #-8]!		/* set arg5 (dyndata.symndx) */
	bl	audit_pltexit		/* invoke the audit_pltexit function */

	/*
	 * Clean up our stack frame and return to the original caller.
	 */
	add	sp, sp, #36		/* Clean up local storage */
					/* 36 = 28(for local storage) + 8(for */
					/*      stacked arg of la_pltexit) */
	ldmia	sp!, {r0-r9, sl, fp, pc}/* and return to the caller. */
	/* NOTREACHED */

	/*
	 * Area for global symbols.
	 */
	.align	3
.Lgot:
	.word	_GLOBAL_OFFSET_TABLE_ - (.Lref_got + 8)
.Laudit_flags:
	.word	audit_flags(GOT)	/* offset of audit_flags in GOT */
.Laudit_argcnt:
	.word	audit_argcnt(GOT)	/* offset of audit_argcnt in GOT */

	SET_SIZE(elf_plt_trace)

#endif

/*
 * We got here because a call to a function resolved to a procedure
 * linkage table entry.  That entry did a JMPL to the first PLT entry, which
 * in turn did a call to elf_rtbndr.
 *
 * 1st PLT entry (PLT0):
 *
 * str	lr, [sp, #-4]!			@ push the return address (lr)
 * ldr	lr, [pc, #4]			@ load from 3 words ahead
 * add	lr, pc, lr			@ adjust lr to point to GOT base
 * ldr	pc, [lr, #8]!			@ jump to elf_rtbndr. GOT[2] has it.
 * &GOT[0] - .				@ contains distance to GOT base
 *
 * PLT[n+1] (nth PLT entry):
 * add	ip, pc, #0xnn00000
 * add	ip, ip, #0xnn000
 * ldr	pc, [ip, #0xnnn]!		@ jump to contents of the associated
 * 					@ GOT[n+3] entry and set ip = &GOT[n+3]
 *
 * On entry the registers has the values
 * ip = &GOT[n+3]			@ the GOT address where the relocation
 * 					@ has to happen
 * lr = &GOT[2]				@ addrress of third GOT entry
 * stack[0] = return address (lr) of the original function call
 * [r0, r1, r2, r3] are the arguments to the original function call
 */

#if defined(lint)

extern	unsigned long	elf_bndr(Rt_map *, unsigned long, caddr_t);
void
elf_rtbndr(Rt_map * lmp, unsigned long reloc, caddr_t pc)
{
	(void)	elf_bndr(lmp, reloc, pc);
}

#else
	ANSI_PRAGMA_WEAK2(_elf_rtbndr, elf_rtbndr, function)

	ENTRY_NP(elf_rtbndr)		/* Do not need to profile. */

	stmdb	sp!, {r0-r3, fp}	/* push r0-r3 and fp(r11) */
	add	fp, sp, #20
	ldr	r2, [fp]		/* get return address at r2 */
	/*
	 * calculate the index for the entry into the PLT Table
	 */
	sub	r1, ip, lr		/* distance from &GOT[n+3] to &GOT[2] */
	sub	r1, r1, #4		/* assuming indexing starts from 0, */
	mov	r1, r1, lsr #2		/* calculate entry index (distance/4) */
					/* r1 now contains pltgot index */
	/*
	 * put link map address
	 */
	ldr 	r0, [lr, #-4]		/* r0 now contains the link map addr */
	bl 	elf_bndr		/* call elf_bndr(lmp, pltindx, pc); */
	mov 	ip, r0			/* store resolved function address at */
					/* 'pc' saved ip */
	/*
	 * The following statement pops the original args, fp, sp, lr, and pc
	 * This also causes the function to jump to the destination function
	 * with its original arguments and registers.
	 */
	ldmia	sp!, {r0-r3, fp}	/* pop r0-r3 and fp */
	ldr	lr, [sp], #4		/* pop 1st PLT entry */
	mov	pc, ip			/* return value of elf_bndr() is set */

	SET_SIZE(elf_rtbndr)
#endif
