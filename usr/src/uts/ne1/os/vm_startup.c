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

#ident	"@(#)ne1/os/vm_startup.c"

/*
 * NE1-specific VM initialization.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/debug.h>
#include <sys/mman.h>
#include <sys/trap.h>
#include <sys/xramdev_impl.h>
#include <sys/prom_debug.h>
#include <sys/platform.h>
#include <vm/hat.h>
#include <vm/hat_arm.h>

/*
 * Kernel's virtual memory layout:
 *
 *
 * 0x00000000   +-----------------------+
 *              |                       |
 *              |                       |
 *              |      User space       |
 *              |                       |
 *              |                       |
 * 0xc0000000   +-----------------------| KERNELBASE
 *              |         N/A           |
 * 0xc0002000   +-----------------------+ KERNELTEXTBASE / s_text
 *              |                       |
 *              |      Kernel text      |
 *              |                       |
 *              +-----------------------+ e_text / s_data (floating)
 *              |                       | (econtig if XRAMDEV_CONFIG)
 *              |    Kernel data/BSS    |
 *              |                       |
 *              +-----------------------+ e_data (floating)
 *              |                       |
 *              |   Kernel static data  |
 *              | page structures/hash, |
 *              | kernel HAT resources, |
 *              | ...                   |
 *              +-----------------------+ econtig / heaptext_base (floating)
 *              |                       | (e_kstatic if XRAMDEV_CONFIG)
 *              |      Module text      |
 *              |                       |
 * 0xc2000000   +-----------------------+ heaptext_base + heaptext_size
 *              |      Reserved for     |
 *              |      Kernel L2PT      |
 *              +-----------------------+
 *              | Reserved for          |
 *              |   ppcopy()/pagezero() |
 *              +-----------------------+ segkmap_start (floating)
 *              |        Segkmap        |
 *              +-----------------------+ segkmap_start + segmapsize (floating)
 *              |       Red zone        |
 *              +-----------------------+ kernelheap (floating)
 *              |                       | (segkp is an arena under the heap
 *              |                       |  if SEGKP_SIZE is zero.)
 *              |                       |
 *              |         kvseg         |
 *              |                       |
 *              |                       |
 *              |                       |
 *              +-----------------------+ ekernelheap (floating)
 *              |  Red zone if required |
 *              +-----------------------+
 *              |                       |
 *              |    xramfs devices     |
 *              |   if XRAMDEV_CONFIG   |
 *              |                       |
 *              +-----------------------+
 *              |  Red zone if required |
 *              +-----------------------+ segzio_base (floating)
 *              |        Segzio         |
 *              |  if SEGZIO_SIZE != 0  |
 *              +-----------------------+ segzio_base + segzio_size
 *              |  Red zone if required |
 *              +-----------------------+ segkp_base (floating)
 *              |         Segkp         |
 *              |   if SEGKP_SIZE != 0  |
 *              +-----------------------+ segkp_base + segkp_size
 *              |  Red zone if required |
 * 0xfe000000   +-----------------------+ ARMPF_SYS_VADDR
 *              |    NE1 internal regs  |
 * 0xff000000   +-----------------------+ ARMPF_XWINDOW_VADDR
 *              |       X Window        |
 * 0xff900000   +-----------------------+ SEGDEBUGBASE
 *              | Reserved for debugger |
 * 0xffe00000   +-----------------------+
 *              |        Unused         |
 * 0xfffc0000   +-----------------------+ NE1_COREFPGA_VADDR
 *              |       Core FPGA       |
 * 0xfffd0000   +-----------------------+
 *              |        AV FPGA        |
 * 0xfffe0000   +-----------------------+ ARMPF_PCI_CONFIG_VADDR
 *              |   PCI configuration   |
 * 0xffff0000   +-----------------------+ ARM_VECTORS_HIGH
 *              |  ARM trap/intr vector |
 * 0xffff1000   +-----------------------+ ARM_VECTORS_HIGH + MMU_PAGESIZE
 *              |        Unused         |
 * 0xffff2000   +-----------------------+ ARMPF_SCU_VADDR
 *              |  MPCore SCU, specific |
 * 0xffff4000   +-----------------------+ ARMPF_SCU_VADDR + ARMPF_SCU_SIZE
 *              |        Unused         |
 *              +-----------------------+
 *                  
 * Floating values:
 *
 * econtig:
 *	End of kernel static data. [KERNELBASE, econtig) maps contiguous
 *	physicall address [KERNELPHYSBASE, econtig - KERNELBASE).
 *
 * heaptext_base:
 *	Start of heaptext_arena. This region is used specifically for
 *	module text.
 *
 * segkmap_start:
 *	Start of segmap. The length of segmap can be modified
 *	by changing segmapsize via bootargs.
 *	The default length is 16MB.
 * 
 * kernelheap:
 *	Start of kernel heap. 
 *
 * ekernelheap:
 *	End of kernel heap.
 */

/*
 * caddr_t
 * vm_startup(void)
 *	Platform-specific initialization for VM layer.
 *
 * Calling/Exit State:
 *	vm_startup() returns the end address boundary of virtual address
 *	space that can be managed by kernel segments, such as kernelheap.
 */
caddr_t
vm_startup(void)
{
	caddr_t	ksegend;
	extern caddr_t	econtig;
#ifdef	XRAMDEV_CONFIG
	extern caddr_t	e_kstatic;
#endif	/* XRAMDEV_CONFIG */

	/*
	 * Create initial kernel mappings.
	 */

	/* Kernel text/data, and static data */
	hat_boot_mapin((caddr_t)KERNELBASE, KERNELPHYSBASE,
		       (uintptr_t)econtig - KERNELBASE, HAT_STORECACHING_OK);
#ifdef	XRAMDEV_CONFIG
	/* Kernel data, BSS, and static data. */
	PRM_PRINTF("e_kstatic = 0x%p, static size = 0x%lx\n",
		   e_kstatic, (size_t)(e_kstatic - econtig));
	hat_boot_mapin((caddr_t)econtig, PAGE_ROUNDDOWN(data_paddr),
		       (size_t)(e_kstatic - econtig), HAT_STORECACHING_OK);
#endif	/* XRAMDEV_CONFIG */

	/* System registers. */
	hat_boot_mapin((caddr_t)ARMPF_SYS_VADDR, ARMPF_SYS_PADDR,
		       ARMPF_SYS_SIZE,
		       PROT_READ|PROT_WRITE|HAT_LOADCACHING_OK);

	/* MPCore SCU registers. */
	hat_boot_mapin((caddr_t)ARMPF_SCU_VADDR, ARMPF_SCU_PADDR,
		       ARMPF_SCU_SIZE,
		       PROT_READ|PROT_WRITE|HAT_LOADCACHING_OK);

	/* Vector page. */
	hat_boot_mapin((caddr_t)ARM_VECTORS_HIGH, VECTOR_PAGE_PADDR,
		       MMU_PAGESIZE, HAT_STORECACHING_OK);

	/* PCI config */
	hat_boot_mapin((caddr_t)ARMPF_PCI_CONFIG_VADDR, ARMPF_PCI_CONFIG_PADDR,
		       ARMPF_PCI_CONFIG_SIZE,
		       PROT_READ|PROT_WRITE|HAT_LOADCACHING_OK);

	/* Core FPGA and AV FPGA */
	hat_boot_mapin((caddr_t)NE1_COREFPGA_VADDR, NE1_COREFPGA_PADDR,
		       NE1_COREFPGA_SIZE,
		       PROT_READ|PROT_WRITE|HAT_LOADCACHING_OK);

	/* X Window */
	hat_boot_mapin((caddr_t)ARMPF_XWINDOW_VADDR, ARMPF_XWINDOW_PADDR,
		       ARMPF_XWINDOW_SIZE,
		       PROT_READ|PROT_WRITE|HAT_LOADCACHING_OK);

	/*
	 * Kernel segments are located below the internal system registers.
	 */
	ksegend = (caddr_t)(ARMPF_SYS_VADDR - MMU_PAGESIZE);

	return ksegend;
}
