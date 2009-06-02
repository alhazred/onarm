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

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved					*/

/*	Copyright (c) 1987, 1988 Microsoft Corporation		*/
/*	  All Rights Reserved					*/

/*
 * Copyright (c) 2006-2009 NEC Corporation
 */

#pragma ident	"@(#)locore.s	1.202	06/03/20 SMI"

#include <sys/asm_linkage.h>
#include <sys/cpuvar_impl.h>
#include <sys/regset.h>
#include <sys/privregs.h>
#include <sys/reboot.h>
#include <sys/pte.h>
#include <sys/machparam.h>
#include <asm/cpufunc.h>
#include <asm/tlb.h>
#include <sys/mach_boot.h>

#if defined(__lint)

#include <sys/types.h>
#include <sys/thread.h>
#include <sys/systm.h>
#include <sys/lgrp.h>
#include <sys/regset.h>
#include <sys/link.h>
#include <sys/bootconf.h>
#include <sys/bootsvcs.h>

#else	/* __lint */

#include <sys/pcb.h>
#include <sys/trap.h>
#include <sys/ftrace.h>
#ifdef	TRAPTRACE
#include <sys/traptrace.h>
#endif	/* TRAPTRACE */
#include <sys/clock.h>
#include <sys/cmn_err.h>
#include <sys/pit.h>
#include <sys/panic.h>
#include "assym.h"

/*
 * Start physical address of boot temporary memory area.
 * Note that the first 16K bytes of temporary memory is used as initial L1PT.
 * That's why KERNEL_BOOTTMP_PADDR must be 16K bytes aligned.
 */
#if	(KERNEL_BOOTTMP_PADDR & (L1PT_SIZE - 1)) != 0
#error	KERNEL_BOOTTMP_PADDR must be 16K bytes aligned.
#endif	/* (KERNEL_BOOTTMP_PADDR & (L1PT_SIZE - 1)) != 0 */

#define	BOOTTMP_PADDR	(KERNEL_BOOTTMP_PADDR + L1PT_SIZE)

#ifdef	XRAMDEV_CONFIG
/* _start() must copy kernel data because no boot loader exists. */
#define	KERNEL_DATA_BACKUP	1
#endif	/* XRAMDEV_CONFIG */


#ifdef	KERNEL_DATA_BACKUP

/*
 * BACKUP_KERNEL(orgdata, data, bargs, kpbase, tmp0, tmp1, tmp2)
 *	BACKUP_KERNEL() preserves kernel data and symbol table.
 *	"orgdata" must point base physical address of original data section.
 *	"data" must point base physical address of actual data section.
 *	"kpbase" must contains KERNELPHYSBASE value.
 *
 *	Note that BACKUP_KERNEL() destroys all argument registers except for
 *	kpbase.
 */
#define	BACKUP_KERNEL(orgdata, data, bargs, kpbase, tmp0, tmp1, tmp2)	\
	/*								\
	 * At first, we must copy bootargs into temporary work		\
	 * area because the following kernel data copy may		\
	 * destroy bootargs given by U-boot.				\
	 */								\
	ldr	tmp0, =BOOTTMP_PADDR;					\
	add	tmp1, tmp0, #OBP_MAXPATHLEN;				\
3:									\
	ldrb	tmp2, [bargs], #1;					\
	strb	tmp2, [tmp0], #1;					\
	teq	tmp2, #0;						\
	cmpne	tmp0, tmp1;						\
	bne	3b;							\
	ldr	bargs, =BOOTTMP_PADDR;					\
									\
	/*								\
	 * We need to copy until beginning of BSS.			\
	 * CTF data doesn't need to be copied because it will be	\
	 * done in startup.c.						\
	 */								\
	ldr	tmp0, .Lstart_data;					\
	sub	tmp0, tmp0, #KERNELBASE;				\
	add	tmp0, tmp0, kpbase;					\
									\
	/*								\
	 * We need to copy whole text end boundary page because		\
	 * some valid text may exist in it.				\
	 */								\
	mov	tmp1, #MMU_PAGESIZE;					\
	sub	tmp1, tmp1, #1;						\
	bic	orgdata, orgdata, tmp1;					\
	bic	data, data, tmp1;					\
									\
5:									\
	ldr	tmp2, [orgdata], #CLONGSIZE;				\
	str	tmp2, [data], #CLONGSIZE;				\
	cmp	orgdata, tmp0;						\
	blo	5b

#else	/* !KERNEL_DATA_BACKUP */

#define	BACKUP_KERNEL(orgdata, data, bargs, kpbase, tmp0, tmp1, tmp2)

#endif	/* KERNEL_DATA_BACKUP */

/*
 * GET_BOOTARGS(bparam, bargs, tmp0, tmp1, tmp2)
 *	GET_BOOTARGS() gets pointer to NIL-terminated "bootargs" string.
 *	"bparam" must point base physical address of a list of tagged entries.
 */
#define	GET_BOOTARGS(bparam, bargs, tmp0, tmp1, tmp2)			\
	ldr	tmp2, =0x54410009;		/* ATAG_CMDLINE */	\
.Latag_loop:								\
	ldr	tmp0, [bparam];			/* load size */		\
	ldr	tmp1, [bparam, #4];		/* load tag  */		\
	cmp	tmp1, tmp2;						\
	addeq	bargs, bparam, #8;		/* found bootargs */	\
	beq	.Latag_out;						\
	cmp	tmp1, #0;						\
	addne	bparam, bparam, tmp0, lsl #2;	/* next entry */	\
	bne	.Latag_loop;						\
	mov	bargs, #0;			/* not found */		\
.Latag_out:


/*
 * Our assumptions:
 *	- We are running in real mode.
 *	  So we can't dereference address of global symbol.
 *	- L1 cache state is coherent.
 *	- Interrupts are disabled.
 *	- The kernel's text, initialized data and bss are mapped.
 *	- U-boot "boot parameters" is passed via r2. r2 holds address of
 *	  pointer to a list of tagged entries.
 *	- We can use memory area that starts from KERNEL_BOOTTMP_PADDR
 *	  as temporary work area.
 *	- The kernel is static-linked.
 *	- The kernel is loaded into SDRAM0, and at least SDRAM0 memory in
 *	  [KERNELPHYSBASE, KERNELPHYSBASE + STARTUP_RAM_MAPSIZE) is available.
 *
 * Our actions:
 *	- Save bootargs.
 *	- Initialize our stack pointer to the thread 0 stack (t0stack)
 *	  and leave room for a phony "struct regs".
 *	- Build page tables for bootstrap code using temporary memory area.
 *	- Save bootargs passed by U-boot.
 *	- Save CTF data if present.
 *	- Enable MMU.
 *	- mlsetup() and main() get called.  (NOTE: main() never returns).
 *
 * NOW, the real code!
 */

#define	SDRAM0_END	(ARMPF_SDRAM0_PADDR + ARMPF_SDRAM0_SIZE)
#if	(KERNELPHYSBASE + STARTUP_RAM_MAPSIZE) > SDRAM0_END
#error	The kernel can't be loaded into SDRAM0.
#endif	/* (KERNELPHYSBASE + STARTUP_RAM_MAPSIZE) > SDRAM0_END */

/*
 * Globals:
 */
	.globl	mlsetup
	.globl	main
	.globl	panic

/*
 * Stack and thread for thread 0.
 */
DGDEF3(t0stack, DEFAULTSTKSZ, 5)	/* 32byte align */
	.skip	DEFAULTSTKSZ

DGDEF3(t0, THREAD_SIZE, 5)		/* 32byte align */
	.skip	THREAD_SIZE

/*
 * Storage for bootargs.
 */
DGDEF2(kern_bootargs, OBP_MAXPATHLEN)
	.skip	OBP_MAXPATHLEN

/*
 * BINARY PATCH START:
 *
 * The initial values of the following variables are physical address, and
 * they will be set by build environment.
 */

/*
 * The start and end address of the CTF data for unix module.
 */
DGDEF(ctf_start_address)
	.word	0
DGDEF(ctf_end_address)
	.word	0

/*
 * The start and end address of .symtab section.
 */
DGDEF(symtab_start_address)
	.word	0
DGDEF(symtab_end_address)
	.word	0

/*
 * Number of local symbols in .symtab.
 */
DGDEF(symtab_locals)
	.word	0

/*
 * The start and end address of .strtab section.
 */
DGDEF(strtab_start_address)
	.word	0
DGDEF(strtab_end_address)
	.word	0

/*
 * The start and end address of .hash section.
 */
DGDEF(symhash_start_address)
	.word	0
DGDEF(symhash_end_address)
	.word	0

#ifdef	XRAMDEV_CONFIG

/*
 * Base physical address of data section.
 * The boot loader should copy all kernel image except for text to
 * this address.
 */
DGDEF(data_paddr)
	.word	0

/*
 * Base physical address of original data section.
 * The kernel never modify data in this area.
 */
DGDEF(data_paddr_base)
	.word	0

/*
 * Start address of physical memory reserved for device usage.
 */
DGDEF(xramdev_start_paddr)
	.word	0

/*
 * End address of physical memory reserved for device usage.
 */
DGDEF(xramdev_end_paddr)
	.word	0

/*
 * Start physical address of memory managed by struct page.
 * It is guaranteed that xramdev_pagestart_paddr is larger than or equal
 * xramdev_start_paddr, and smaller than or equal xramdev_end_paddr.
 *
 * Remarks:
 *	Memory in range [xramdev_pagestart_paddr, xramdev_end_paddr) is managed
 *	by struct page. That is, [xramdev_start_paddr, xramdev_end_paddr)
 *	may contain memory that is NOT for xramfs device.
 */
DGDEF(xramdev_pagestart_paddr)
	.word	0

/*
 * Root of B-tree index for memory device node.
 */
DGDEF(xmemdev_root)
	.word	0

#endif	/* XRAMDEV_CONFIG */

/*
 * BINARY PATCH END
 */

#ifdef	ARMPF_SDRAM0_SPLIT
/*
 * Flag to notice unix CTF data exists.
 */
DGDEF(unix_ctf_exists)
	.word	1
#else	/* !ARMPF_SDRAM0_SPLIT */

#ifndef	XRAMDEV_CONFIG

#endif	/* !XRAMDEV_CONFIG */

#define	PLATFORM_PHYSMEM_SIZE	(PLATFORM_MIN_PHYSMEM << MMU_PAGESHIFT)
#define	UNIX_CTF_MAXADDR	(KERNELBASE + PLATFORM_PHYSMEM_SIZE)

#if	PLATFORM_PHYSMEM_SIZE > STARTUP_RAM_MAPSIZE
#error	STARTUP_RAM_MAPSIZE must be enlarged.
#endif	/* PLATFORM_PHYSMEM_SIZE > STARTUP_RAM_MAPSIZE */

#endif	/* ARMPF_SDRAM0_SPLIT */

/*
 * Current allocation pointer in boot temporary memory
 */
DGDEF(boottmp_reserved)
	.word	BOOTTMP_PADDR

#if	(STACK_ALIGN & (STACK_ALIGN - 1)) != 0
#error	STACK_ALIGN must be power of 2.
#endif	/* (STACK_ALIGN & (STACK_ALIGN - 1)) != 0 */

/*
 * Round down value to the specified alignment.
 * Alignment must be the number of low-order zero bits, and be lower than
 * or equal to 256.
 */
#define	LOCORE_P2ALIGN(reg, align)				\
	bic	reg, reg, #((align) - 1);			\

/* Control Register bits to be initialized */
#define	CTRL_CLEAR_BITS							\
	(CTREG_M_BIT|CTREG_A_BIT|CTREG_C_BIT|CTREG_S_BIT|CTREG_R_BIT|	\
	 CTREG_Z_BIT|CTREG_I_BIT|CTREG_V_BIT|CTREG_L4_BIT|CTREG_U_BIT|	\
	 CTREG_XP_BIT|CTREG_AP_BIT|CTREG_SBZ)

/*
 * Control Register bits to be set
 *	- MMU enabled
 *	- L1 data cache enabled
 *	- Program flow prediction enabled
 *	- L1 instruction cache enabled
 *	- Use high vector
 *	- Allow unaligned data access
 *	- Subpage AP bits disabled.
 */
#define	CTRL_SET_BITS							\
	(CTREG_M_BIT|CTREG_C_BIT|CTREG_Z_BIT|CTREG_I_BIT|CTREG_V_BIT|	\
	 CTREG_U_BIT|CTREG_XP_BIT|CTREG_AP_BIT)

/*
 * Initial value of DACR.
 * We must have access permission for domain 0 because initial section
 * PTEs have domain 0.
 */
#define	INITIAL_DACR							\
	(DACR_VALUE(HAT_DOMAIN_KERNEL, DACR_CLIENT)|DACR_VALUE(0, DACR_CLIENT))

#if	0
#ifdef TRAPTRACE
	/*
	 * trap_trace_ctl must agree with the structure definition in
	 * <sys/traptrace.h>
	 */	
	DGDEF3(trap_trace_bufsize, CLONGSIZE, CLONGSIZE)
	.NWORD	TRAP_TSIZE

	DGDEF3(trap_trace_freeze, 4, 4)
	.long	0

	DGDEF3(trap_trace_off, 4, 4)
	.long	0

	DGDEF3(trap_trace_ctl, _MUL(4, CLONGSIZE), 16)
	.NWORD	trap_tr0		/* next record */
	.NWORD	trap_tr0		/* first record */
	.NWORD	trap_tr0 + TRAP_TSIZE	/* limit */
	.NWORD	0			/* pad */

	/*
	 * Enough padding for 31 more CPUs (no .skip on x86 -- grrrr).
	 */
	.NWORD	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	.NWORD	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	.NWORD	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	.NWORD	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	.NWORD	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	.NWORD	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	.NWORD	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	.NWORD	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0


#if NCPU != 32
#error "NCPU != 32, Expand padding for trap_trace_ctl"
#endif


	/*
	 * CPU 0's preallocated TRAPTRACE buffer.
	 */
	.globl	trap_tr0
	.comm	trap_tr0, TRAP_TSIZE

	/*
	 * A dummy TRAPTRACE entry to use after death.
	 */
	.globl	trap_trace_postmort
	.comm	trap_trace_postmort, TRAP_ENT_SIZE
#endif	/* TRAPTRACE */
#endif	/* 0 */

#endif	/* __lint */

#if defined(__lint)

/*ARGSUSED*/
void
_start(const char *uboot_args)
{}

#else	/* __lint */

ENTRY_NP(_start)
	/*
	 * We can use all registers but r2/r5 that keeps bootparams.
	 */
	mov	r9, #0				/* r9 = 0 */

	/* Disable MMU for a while */
	CTRL_READ(r0)
	bic	r0, r0, #CTREG_M_BIT
	CTRL_WRITE(r0)
	nop
	nop
	nop

	/*
	 * Build page table from scratch.
	 * At first, we should zero startup page table to protect board
	 * from unexpected access.
	 */
	ldr	r6, =KERNEL_BOOTTMP_PADDR
	mov	r0, #0
	mov	r1, #0
	mov	r3, #0
	mov	r4, #0
	mov	r10, #0
	mov	r11, #0
	mov	r12, #0
	mov	r7, r6
	add	r8, r6, #L1PT_SIZE
.Lptezero:
	/* Use 8 registers to zero out page table. */
	stmia	r7!, {r0-r1, r3-r4, r9-r12}
	cmp	r7, r8
	blo	.Lptezero

	GET_BOOTARGS(r2, r5, r0, r1, r3)
	/* Now we can use r2. */

	ldr	r4, =KERNELPHYSBASE
#ifdef	XRAMDEV_CONFIG
	ldr	r0, .Ldata_paddr_base
	ldr	r1, .Ldata_paddr
	sub	r0, r0, #KERNELBASE
	add	r0, r0, r4
	sub	r1, r1, #KERNELBASE
	add	r1, r1, r4
	ldr	r0, [r0]		/* r0 = original data section */
	ldr	r1, [r1]		/* r1 = actual data section */
	mov	r10, r0
	mov	r11, r1
#endif	/* XRAMDEV_CONFIG */

	/* Copy data section to data_paddr if needed. */
	BACKUP_KERNEL(r0, r1, r5, r4, r2, r3, ip)

	/*
	 * Copy bootargs into kern_bootargs[].
	 */
	ldr	r7, .Lbargs_ptr
	sub	r7, #KERNELBASE
	add	r7, r4
#ifdef	XRAMDEV_CONFIG
	/* Obtain kern_bootargs address in actual data section. */
	sub	ip, r7, r10
	add	r7, r11, ip
#endif	/* XRAMDEV_CONFIG */
	add	r8, r7, #OBP_MAXPATHLEN
	strb	r9, [r8, #-1]	/* Terminate kern_bootargs[]. */
.Lbargs_loop:
	ldrb	r3, [r5], #1	/* r3 = *bootargs, bootargs++ */
	strb	r3, [r7], #1	/* *kern_bootargs = r3, kern_bootargs++ */
	cmp	r7, r8
	bhs	.Lbargs_out
	teq	r3, #0
	bne	.Lbargs_loop
.Lbargs_out:

	/* Now we can use r5. */

	/* Initialize sp. */
	adr	sp, _start
	sub	sp, sp, #0x10

	/*
	 * Load physical address of boottmp_reserved which keeps the end
	 * address of reserved area in boot temporary memory area.
	 * It is used for L2PT allocation.
	 */
	ldr	r5, .Lboottmp_reserved
	sub	r5, #KERNELBASE
	add	r5, r4

#ifdef	XRAMDEV_CONFIG
	/* Obtain boottmp_reserved address in actual data section. */
	sub	ip, r5, r10
	add	r5, r11, ip

	/*
	 * Create mapping for kernel text,
	 * [KERNELBASE, PAGE_ROUNDDOWN(&__text_end__)).
	 */
	mov	ip, #MMU_PAGESIZE
	sub	ip, ip, #1
	ldr	r2, .Ldata_start
	mov	r0, #KERNELBASE
	mov	r1, r4			/* r1 = KERNELPHYSBASE */
	bic	r2, ip
	str	r9, [sp]		/* flags = 0 */
	mov	r10, r2
	sub	r2, r2, r0
	mov	r3, r5
	bl	early_mapinit

	/* Create mapping for kernel data, bss. */
	mov	ip, #MMU_PAGESIZE
	sub	ip, ip, #1
	mov	r0, r10
	bic	r1, r11, ip
	ldr	r2, =(KERNELBASE + STARTUP_RAM_MAPSIZE)
	str	r9, [sp]		/* flags = 0 */
	sub	r2, r2, r0
	mov	r3, r5
	bl	early_mapinit
#endif	/* XRAMDEV_CONFIG */

	/* Create initial mapping according to the initial data set. */
	adr	r7, .Linitial_map_set
.Lmap_loop:
	ldmia	r7!, {r0-r2, ip}
	teq	r2, #0
	beq	.Lsetup_ttb		/* if (size == 0) break */
	str	ip, [sp]
	mov	r3, r5
	bl	early_mapinit
	b	.Lmap_loop

.Lsetup_ttb:
	/* Setup TTB 0 */
	orr	r6, #TTB_RGN_WALLOC
#if	NCPU != 1
	orr	r6, #TTB_SHARED
#endif	/* NCPU != 1 */

	/*
	 * Initialize TTB and TTB control registers.
	 */
	TTB_BOOT_INIT(r6, r9, r0)
	TLB_FLUSH(r9)				/* Flush all TLB entries */

	/*
	 * Set the Domain Access Control Register.
	 */
	mov	r0, #INITIAL_DACR
	DACR_SET(r0)

	/* Set SMP bit in AUX Control Register. */
	AUXCTRL_READ(r0)
	orr	r0, r0, #AUXCTL_SMP_BIT
	AUXCTRL_WRITE(r0)

	/* Enable MMU, and initialize Control Register. */
	CTRL_READ(r0)
	ldr	r2, .Lmpcore_ctrl_clear
	ldr	r3, .Lmpcore_ctrl_set
	bic	r0, r0, r2		/* Clear bits to be initialized */
	orr	r0, r0, r3		/* Set bits to be set */
	IDCACHE_INV_ALL(r9)		/* Invalidate all cache lines. */
	CTRL_WRITE(r0)
	SYNC_BARRIER(r9)

	/*
	 * If XRAMDEV_CONFIG is defined, CTF data copy will be done
	 * in startup.c.
	 */
#ifndef	XRAMDEV_CONFIG
	/*
	 * If CTF data exists, it is located just after .bss.
	 * In addition, CTF data must be located to pageable area because
	 * it may be released when it is expanded by Dtrace.
	 * So we choose to copy it into the end boundary of startup mapping
	 * on SDRAM where the kernel is loaded.
	 * Zeros CTF start and end addresses if the range of CTF data is
	 * not valid.
	 */
	ldr	r2, .Lctf_saddr
	ldr	r3, .Lctf_eaddr
	ldr	r10, [r2]
	cmp	r10, #0
	streq	r9, [r2]
	streq	r9, [r3]
	beq	.Linit_bss
	ldr	r5, [r3]
	cmp	r5, #0
	streq	r9, [r2]
	streq	r9, [r3]
	beq	.Linit_bss

	subs	r0, r5, r10		/* r0 = CTF size */
	streq	r9, [r2]
	streq	r9, [r3]
	beq	.Linit_bss

	/*
	 * Set destination address to r8.
	 * Note that destination address must be page-aligned.
	 */
#ifdef	ARMPF_SDRAM0_SPLIT
	/*
	 * There is a hole between SDRAM0 start address and KERNELPHYSBASE.
	 * Copy CTF data into the hole.
	 */
	ldr	r8, =ARMPF_SDRAM0_PADDR

	/* Disable CTF data if it is too large. */
	ldr	r7, =KERNELPHYSBASE
	add	r5, r8, r0
	cmp	r5, r7
	ldrhs	r8, .Lctf_exists
	strhs	r9, [r8]
	bhs	.Linit_bss
#else	/* !ARMPF_SDRAM0_SPLIT */
	ldr	r8, =UNIX_CTF_MAXADDR
	sub	r8, r8, r0
	mov	r7, #MMU_PAGESIZE
	sub	r7, r7, #1
	bic	r8, r8, r7
#endif	/* ARMPF_SDRAM0_SPLIT */

	/* Update ctf_XXX_address. */
	add	r7, r8, r0
	str	r8, [r2]
	str	r7, [r3]

	/*
	 * CTF data can be copied using word access because we know that
	 * CTF base address should be word-aligned. Although its size may
	 * not be word-aligned, it's harmless because we can access source
	 * and destination area safely.
	 */
	add	r5, r10, r0		/* r5 = end boundary of CTF data */
.Lctf_wcopy:
	ldr	r2, [r10], #CLONGSIZE
	str	r2, [r8], #CLONGSIZE
	cmp	r10, r5
	blo	.Lctf_wcopy

.Linit_bss:
#endif	/* !XRAMDEV_CONFIG */

	adr	r0, .Lstart_data
	ldmia	r0, {r3-r4, sp}		/* Load bss range and change stack */
	LOCORE_P2ALIGN(sp, STACK_ENTRY_ALIGN)	/* Adjust stack alignment */

	/*
	 * Zero out the bss.
	 * We can zero using two registers because base address and size of
	 * .bss must be 8 bytes aligned.
	 */
	mov	r8, #0

	/* r9 is still 0 */
2:
	stmia	r3!, {r8, r9}
	cmp	r3, r4
	blo	2b

	/* Jump into kernel virtual address space. */
	ldr	r0, .Lvirt_start_addr
	mov	pc, r0

.Lvirt_done:
	/* Program counter is in KERNELBASE range from here. */
	ldr	fp, =KERNELBASE		/* trace back starts here */

	mov	r0, sp			/* r0 = pointer to struct regs */
	bl	mlsetup
	bl	main			/* never returned */

	adr	r0, .Lmainreturned
	bl	panic
.Lpanic_loop:
	b	.Lpanic_loop
	/* NOTREACHED */
	SET_SIZE(_start)

	/* Initial mapping data set. */
	.ltorg
.Linitial_map_set:
	STARTUP_MAP_DECL()

	.global	__bss_start__
	.global	__bss_end__
.Lstart_data:
	.word	__bss_start__		/* BSS start address */
	.word	__bss_end__		/* BSS end address */
	/*
	 * Initial stack top for t0.
	 * We must leave room for a "struct regs" for lwp0.
	 */
	.word	t0stack + DEFAULTSTKSZ - REGSIZE - STACK_BIAS

	.ltorg
#ifndef	XRAMDEV_CONFIG
.Lctf_saddr:
	.word	ctf_start_address
.Lctf_eaddr:
	.word	ctf_end_address
#ifdef	ARMPF_SDRAM0_SPLIT
.Lctf_exists:
	.word	unix_ctf_exists
#endif	/* ARMPF_SDRAM0_SPLIT */
#endif	/* !XRAMDEV_CONFIG */
.Lbargs_ptr:
	.word	kern_bootargs

.Lvirt_start_addr:
	.word	.Lvirt_done

	.align	0
.Lmainreturned:
	.asciz	"main() returned"
.Lmp_startup_returned:
	.asciz	"mp_start() returned"

.Lboottmp_reserved:
	.word	boottmp_reserved

#ifdef	XRAMDEV_CONFIG
	.align	2
	.global	__text_end__
.Ldata_start:
	.word	__text_end__
.Ldata_paddr:
	.word	data_paddr
.Ldata_paddr_base:
	.word	data_paddr_base
.Lxramdev_end_paddr:
	.word	xramdev_end_paddr
#endif	/* XRAMDEV_CONFIG */

	.text
	.align	2

	/* Bits in Control Register to be initialized. */
.Lmpcore_ctrl_clear:
	.word	CTRL_CLEAR_BITS

	/* Bits in Control Register to be set */
.Lmpcore_ctrl_set:
	.word	CTRL_SET_BITS

#endif	/* __lint */

/*
 * static void
 * early_mapinit(uintptr_t vaddr, uintptr_t paddr, size_t size,
 *		 l2pte_t **allocp, uint_t flags)
 *	Establish virtual address mapping for early bootstrap.
 *	"allocp" must be an address of L2PT pointer that keeps free L2PT.
 *	early_mapinit() updates *allocp if it allocates a L2PT.
 *
 *	Valid value for flags are:
 *
 *	EMAP_READONLY
 *		Create read-only mapping.
 *
 *	EMAP_NOEXEC
 *		Disable exec permission of new mapping.
 *
 *	EMAP_DEVICE
 *		Use device attributes for new mapping.
 *		Otherwise, early_mapinit() uses normal page attributes for
 *		new mapping.
 *
 *	early_mapinit() grants any access for new mapping if neigher
 *	EMAP_READONLY nor EMAP_NOEXEC is set in flags.
 */
#ifdef	__lint
static void
early_mapinit(uintptr_t vaddr, uintptr_t paddr, size_t size, l2pte_t **allocp,
	      uint_t flags)
{}
#else	/* !__lint */

	.text
	.align 2
	.type	early_mapinit, %function
early_mapinit:
	stmfd	sp!, {r4-r11}
	ldr	r11, [sp, #(4*8)]		/* r11 = flags */

	/* Adjust arguments to pagesize boundary. */
	mov	ip, #MMU_PAGESIZE
	add	r5, r0, r2
	sub	ip, ip, #1
	bic	r0, r0, ip		/* r0 = P2ALIGN(vaddr, MMU_PAGESIZE) */
	bic	r1, r1, ip		/* r1 = P2ALIGN(paddr, MMU_PAGESIZE) */

	/* size = P2ROUNDUP(vaddr + size) - r0 */
	add	r5, r5, ip
	bic	r4, r5, ip
	sub	r2, r4, r0

	ldr	r8, =KERNEL_BOOTTMP_PADDR	/* r8 = L1PT address */

.Lmaploop:
	mov	r4, r0, lsr #L1PT_VSHIFT	/* r4 = L1PT_INDEX(vaddr) */

	/*
	 * Determine pagesize.
	 * At first, check whether we can use supersection.
	 */
	mov	ip, #L1PT_SPSECTION_VSIZE
	sub	ip, ip, #1
	tst	r0, ip
	bne	.Lsection_check
	tst	r1, ip
	bne	.Lsection_check
	cmp	r2, #L1PT_SPSECTION_VSIZE
	blo	.Lsection_check

	/* Create supersection mapping. */
	ldr	r5, .Lspattr
	mov	ip, #L1PT_SPSECTION_NPTES
	add	r4, r8, r4, lsl #2		/* r4 = &l1pt[r4] */

	/* If EMAP_READONLY is set in flags, revoke write permission. */
	tst	r11, #EMAP_READONLY
	orrne	r5, #L1PT_APX

	/*
	 * If EMAP_READONLY or EMAP_NOEXEC is set in flags, revoke
	 * exec permission.
	 */ 
	tst	r11, #(EMAP_READONLY|EMAP_NOEXEC)
	orrne	r5, #L1PT_XN

	/*
	 * If EMAP_DEVICE is set in flags, use device page attributes for
	 * this mapping.
	 */
	tst	r11, #EMAP_DEVICE
	bicne	r5, #L1PT_TEX_MASK
	bicne	r5, #L1PT_CACHED

	orr	r6, r1, r5			/* r6 = paddr | attr */
10:
	str	r6, [r4], #L1PT_PTE_SIZE	/* Set PTE */
	subs	ip, ip, #1
	bhi	10b

	subs	r2, r2, #L1PT_SPSECTION_VSIZE
	add	r0, r0, #L1PT_SPSECTION_VSIZE
	add	r1, r1, #L1PT_SPSECTION_VSIZE
	bne	.Lmaploop
	b	.Lmap_return

.Lsection_check:
	/* Check whether we can use section. */
	mov	ip, #L1PT_SECTION_VSIZE
	sub	ip, ip, #1
	tst	r0, ip
	bne	.Ll2map
	tst	r1, ip
	bne	.Ll2map
	cmp	r2, #L1PT_SECTION_VSIZE
	blo	.Ll2map

	/* Create section mapping. */
	ldr	r5, .Lscattr

	/* If EMAP_READONLY is set in flags, revoke write permission. */
	tst	r11, #EMAP_READONLY
	orrne	r5, #L1PT_APX

	/*
	 * If EMAP_READONLY or EMAP_NOEXEC is set in flags, revoke
	 * exec permission.
	 */ 
	tst	r11, #(EMAP_READONLY|EMAP_NOEXEC)
	orrne	r5, #L1PT_XN

	/*
	 * If EMAP_DEVICE is set in flags, use device page attributes for
	 * this mapping.
	 */
	tst	r11, #EMAP_DEVICE
	bicne	r5, #L1PT_TEX_MASK
	bicne	r5, #L1PT_CACHED

	orr	r6, r1, r5			/* r6 = paddr | attr */
	str	r6, [r8, r4, lsl #2]		/* Set PTE */

	subs	r2, r2, #L1PT_SECTION_VSIZE
	add	r0, r0, #L1PT_SECTION_VSIZE
	add	r1, r1, #L1PT_SECTION_VSIZE
	bne	.Lmaploop
	b	.Lmap_return

.Ll2map:
	/*
	 * Large and small page mapping requires L2 page table.
	 * So we need to check whether L2PT is already allocated.
	 */
	ldr	r7, [r8, r4, lsl #2]
	cmp	r7, #0
	movne	ip, #L2PT_SIZE
	subne	ip, ip, #1
	bicne	r7, r7, ip
	bne	.Llarge_check		/* L2PT is alredy allocated. */

	/* Allocate a L2PT and zeros out. */
	str	r0, [sp, #-4]!
	ldr	r7, [r3]		/* r7 = address of new L2PT */
	mov	r5, #0
	mov	r6, #0
	mov	r9, #0
	mov	r10, #0
	mov	r0, r7
	add	ip, r7, #L2PT_SIZE
20:
	stmia	r7!, {r5-r6, r9-r10}
	cmp	r7, ip
	blo	20b

	ldr	r9, .Lcoarseattr
	str	ip, [r3]		/* Update for next allocation */

	/* Install L1PT entry. */
	mov	r7, r0
	orr	r10, r0, r9
	str	r10, [r8, r4, lsl #2]
	ldr	r0, [sp], #4

.Llarge_check:
	mov	ip, #L2PT_NPTES
	sub	ip, ip, #1
	mov	r10, r0, lsr #L2PT_VSHIFT
	and	r10, r10, ip			/* r10 = L2PT_INDEX(vaddr) */

	/*
	 * Check whether we can use large page.
	 * Now r7 keeps base address of L2PT.
	 */
	mov	ip, #L2PT_LARGE_VSIZE
	sub	ip, ip, #1
	tst	r0, ip
	bne	.Lsmall
	tst	r1, ip
	bne	.Lsmall
	cmp	r2, #L2PT_LARGE_VSIZE
	blo	.Lsmall

	/* Create large page mapping. */
	ldr	r5, .Llgattr
	mov	ip, #L2PT_LARGE_NPTES
	add	r4, r7, r10, lsl #2		/* r4 = &l2pt[r10] */

	/* If EMAP_READONLY is set in flags, revoke write permission. */
	tst	r11, #EMAP_READONLY
	orrne	r5, #L2PT_APX

	/*
	 * If EMAP_READONLY or EMAP_NOEXEC is set in flags, revoke
	 * exec permission.
	 */ 
	tst	r11, #(EMAP_READONLY|EMAP_NOEXEC)
	orrne	r5, #L2PT_LARGE_XN

	/*
	 * If EMAP_DEVICE is set in flags, use device page attributes for
	 * this mapping.
	 */
	tst	r11, #EMAP_DEVICE
	bicne	r5, #L2PT_LARGE_TEX_MASK
	bicne	r5, #L2PT_CACHED

	orr	r6, r1, r5			/* r6 = paddr | attr */
30:
	str	r6, [r4], #L2PT_PTE_SIZE	/* Set PTE */
	subs	ip, ip, #1
	bhi	30b

	subs	r2, r2, #L2PT_LARGE_VSIZE
	add	r0, r0, #L2PT_LARGE_VSIZE
	add	r1, r1, #L2PT_LARGE_VSIZE
	bne	.Lmaploop
	b	.Lmap_return

.Lsmall:
	/* Create small page mapping. */
	ldr	r5, .Lsmattr

	/* If EMAP_READONLY is set in flags, revoke write permission. */
	tst	r11, #EMAP_READONLY
	orrne	r5, #L2PT_APX

	/*
	 * If EMAP_READONLY or EMAP_NOEXEC is set in flags, revoke
	 * exec permission.
	 */ 
	tst	r11, #(EMAP_READONLY|EMAP_NOEXEC)
	orrne	r5, #L2PT_SMALL_XN

	/*
	 * If EMAP_DEVICE is set in flags, use device page attributes for
	 * this mapping.
	 */
	tst	r11, #EMAP_DEVICE
	bicne	r5, #L2PT_SMALL_TEX_MASK
	bicne	r5, #L2PT_CACHED

	orr	r6, r1, r5			/* r6 = paddr | attr */
	str	r6, [r7, r10, lsl #2]		/* Set PTE */

	subs	r2, r2, #MMU_PAGESIZE
	add	r0, r0, #MMU_PAGESIZE
	add	r1, r1, #MMU_PAGESIZE
	bne	.Lmaploop

.Lmap_return:
	ldmfd	sp!, {r4-r11}
	mov	pc, lr

.Lspattr:
	.word	STARTUP_L1PTATTR_MEM | L1PT_TYPE_SPSECTION
.Lscattr:
	.word	STARTUP_L1PTATTR_MEM | L1PT_TYPE_SECTION
.Llgattr:
	.word	STARTUP_LARGE_ATTR | L2PT_TYPE_LARGE
.Lsmattr:
	.word	STARTUP_SMALL_ATTR | L2PT_TYPE_SMALL
.Lcoarseattr:
	.word	L1PT_DOMAIN(0) | L1PT_TYPE_COARSE
	SET_SIZE(early_mapinit)
#endif	/* __lint */

#if defined(__lint)

/*ARGSUSED*/
void
secondary_start()
{}

#else	/* __lint */

ENTRY_NP(secondary_start)
	/*
	 * We can use all registers.
	 */
	mov	r9, #0				/* r9 = 0 */

	/* Disable MMU for a while */
	CTRL_READ(r2)
	bic	r2, r2, #CTREG_M_BIT
	CTRL_WRITE(r2)
	nop
	nop
	nop

	/*
	 * Physical address of startup page table should be already
	 * set in MP_STARTUP_L1PT_PADDR.
	 */
	ldr	r0, =MP_STARTUP_L1PT_PADDR
	ldr	r6, [r0]

	/* Setup TTB 0 */
	orr	r6, #TTB_RGN_WALLOC
#if	NCPU != 1
	orr	r6, #TTB_SHARED
#endif	/* NCPU != 1 */

	/*
	 * Initialize TTB and TTB control registers.
	 */
	TTB_SECONDARY_INIT(r6, r9, r0)
	TLB_FLUSH(r9)				/* Flush all TLB entries */

	/*
	 * Set the Domain Access Control Register.
	 */
	mov	r0, #INITIAL_DACR
	DACR_SET(r0)

	/* Set zero into Context ID Register. */
	CONTEXT_ID_SET(r9)

	/* Set SMP bit in AUX Control Register. */
	AUXCTRL_READ(r0)
	orr	r0, r0, #AUXCTL_SMP_BIT
	AUXCTRL_WRITE(r0)

	/* Enable MMU, and initialize Control Register. */
	CTRL_READ(r0)
	ldr	r2, .Lmpcore_ctrl_clear
	ldr	r3, .Lmpcore_ctrl_set
	bic	r0, r0, r2		/* Clear bits to be initialized */
	orr	r0, r0, r3		/* Set bits to be set */
	IDCACHE_INV_ALL(r9)		/* Invalidate all cache lines. */
	CTRL_WRITE(r0)
	SYNC_BARRIER(r9)

	/* Program counter will be in KERNELBASE range soon. */
	ldr	fp, =KERNELBASE		/* trace back starts */

	/*
	 * Get pointer to startup thread for this cpu.
	 * Note that we cannot use THREADP(), LOADCPU() yet.
	 */
	HARD_PROCESSOR_ID(r0)		/* Read H/W Processor ID */
	ldr	r2, =cpu
	ldr	r2, [r2, r0, lsl #2]	/* r2 = cpu[cpuid] */
	ldr	r0, [r2, #CPU_THREAD]	/* r0 = cpu->cpu_thread */
	THREADP_SET(r0)

	ldr	sp, [r0, #T_SP]		/* sp = thread->t_sp */
	adr	lr, .Lnotreached
	/* jump into startup function for secondary CPUs. */
	ldr	pc, [r0, #T_PC]		/* pc = thread->t_pc */
	/* Never returned. */

.Lnotreached:
	adr	r0, .Lmp_startup_returned
	bl	panic
.Lpanic_loop2:
	b	.Lpanic_loop2
	/* NOTREACHED */
	SET_SIZE(secondary_start)

#endif	/* __lint */
