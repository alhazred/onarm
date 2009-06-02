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
 * Copyright (c) 2008-2009 NEC Corporation
 * All rights reserved.
 */

#ifndef	_SYS_MACH_BOOT_IMPL_H
#define	_SYS_MACH_BOOT_IMPL_H

#ident	"@(#)ne1/sys/mach_boot_impl.h"

/*
 * NE1-specific definitions only for bootstrap code.
 * Kernel build tree private.
 */

#ifndef	_SYS_MACH_BOOT_H
#error	Do NOT include mach_boot_impl.h directly.
#endif	/* !_SYS_MACH_BOOT_H */

/*
 * Physical address range of temporary memory area used by bootstrap code.
 */
#ifndef	_ASM
#define	KERNEL_BOOTTMP_PADDR	ARMPF_SDRAM1_PADDR
#endif	/* !_ASM */
#define	KERNEL_BOOTTMP_SIZE	UINT32_C(0x100000)	/* 1M */

#ifdef	_ASM

#if	(ARMPF_SYS_SIZE & MMU_PAGEOFFSET_SPSECTION) != 0
#error	ARMPF_SYS_SIZE must be supersection size aligned.
#endif	/* (ARMPF_SYS_SIZE & MMU_PAGEOFFSET_SPSECTION) != 0 */

/*
 * STARTUP_RAM_MAPSIZE, which is defined in sys/machparam.h, is used to
 * determine size of initial mapping.
 *
 * The following virtual address mappings will be created in startup
 * page table.
 *
 *	VA: [ARMMACH_SDRAM0_PADDR, ARMMACH_SDRAM0_PADDR + STARTUP_RAM_MAPSIZE)
 *	PA: [ARMMACH_SDRAM0_PADDR, ARMMACH_SDRAM0_PADDR + STARTUP_RAM_MAPSIZE)
 *
 *	VA: [KERNEL_BOOTTMP_PADDR, KERNEL_BOOTTMP_PADDR + KERNEL_BOOTTMP_SIZE)
 *	PA: [KERNEL_BOOTTMP_PADDR, KERNEL_BOOTTMP_PADDR + KERNEL_BOOTTMP_SIZE)
 *
 *	VA: [ARMPF_SYS_VADDR, ARMPF_SYS_VADDR + ARMPF_SYS_SIZE)
 *	PA: [ARMPF_SYS_PADDR, ARMPF_SYS_PADDR + ARMPF_SYS_SIZE)
 *
 *	VA: [ARMPF_SCU_VADDR, ARMPF_SCU_VADDR + ARMPF_SCU_SIZE)
 *	PA: [ARMPF_SCU_PADDR, ARMPF_SCU_PADDR + ARMPF_SCU_SIZE)
 *
 * The following mapping will also be created If XRAMDEV_CONFIG is
 * not defined.
 *
 *	VA: [KERNELBASE, KERNELBASE + STARTUP_RAM_MAPSIZE)
 *	PA: [KERNELPHYSBASE, KERNELPHYSBASE + STARTUP_RAM_MAPSIZE)
 */

/*
 * STARTUP_MAP_DECL()
 *	Declare initial mapping data to create initial mapping.
 */
#define	STARTUP_MAP_DECL()						\
	/* Map SDRAM0. */						\
	STARTUP_EARLYMAP_DECL(ARMMACH_SDRAM0_PADDR,			\
			      ARMMACH_SDRAM0_PADDR,			\
			      STARTUP_RAM_MAPSIZE, 0);			\
									\
	/* Map boottime temporary memory. */				\
	STARTUP_EARLYMAP_DECL(KERNEL_BOOTTMP_PADDR,			\
			      KERNEL_BOOTTMP_PADDR,			\
			      KERNEL_BOOTTMP_SIZE, 0);			\
									\
									\
	/* Map kernel text and data. */					\
	STARTUP_KERNEL_MAP_DECL();					\
									\
	/* Map system internal registers */				\
	STARTUP_EARLYMAP_DECL(ARMPF_SYS_VADDR, ARMPF_SYS_PADDR,		\
			      ARMPF_SYS_SIZE,				\
			      EMAP_DEVICE|EMAP_NOEXEC);			\
									\
	/* Map MPCore SCU. */						\
	STARTUP_EARLYMAP_DECL(ARMPF_SCU_VADDR, ARMPF_SCU_PADDR,		\
			      ARMPF_SCU_SIZE,				\
			      EMAP_DEVICE|EMAP_NOEXEC);			\
									\
	STARTUP_EARLYMAP_END()		/* end of table */

#else	/* !_ASM */

/*
 * PLAT_BOOTPROP_INIT(prop, propsz, lvaluep)
 *	Platform-specific initialization using boot properties.
 *	"lvaluep" must be a u_longlong_t pointer and it must point
 *	valid memory.
 */
#define	PLAT_BOOTPROP_INIT(prop, propsz, lvaluep)	/* nothing to do */

/*
 * PLAT_SECONDARY_CPU_ENTRY_INIT()
 *	Install entry point for secondary CPUs.
 */
#define	PLAT_SECONDARY_CPU_ENTRY_INIT()					\
	do {								\
		extern void	secondary_start(void);			\
									\
		/*							\
		 * Set physical address of secondary boot up code	\
		 * to Memo Register in System Control Unit.		\
		 */							\
		writel(KVTOP(secondary_start), NE1_SYS_CTRL_MEMO);	\
	} while (0)

#endif	/* _ASM */

#endif	/* !_SYS_MACH_BOOT_IMPL_H */
