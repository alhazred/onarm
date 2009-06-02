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
 * Copyright (c) 2006-2009 NEC Corporation
 * All rights reserved.
 */

#ifndef	_SYS_PLATFORM_H
#define	_SYS_PLATFORM_H

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

#ident	"@(#)ne1/sys/platform.h"

#include <sys/int_const.h>
#include <sys/pte.h>
#include <sys/platform_mach.h>

/*
 * NE1 specific definitions
 */

/* Number of physical memory banks */
#define	ARMPF_SDRAM_COUNT	2

#define	ARMPF_SDRAM0_PADDR	ARMMACH_SDRAM0_PADDR
#define	ARMPF_SDRAM0_SIZE	ARMMACH_SDRAM0_SIZE
#define	ARMPF_SDRAM1_PADDR	ARMMACH_SDRAM1_PADDR
#define	ARMPF_SDRAM1_SIZE	ARMMACH_SDRAM1_SIZE

/*
 * ARMPF_MAX_MEMSEG_COUNT represents the maximum number of memory segments.
 */

#if	(ARMPF_SDRAM0_PADDR & L1PT_SECTION_VOFFSET) == 0
#define	ARMPF_SDRAM_MEMSEG_COUNT	ARMPF_SDRAM_COUNT
#else	/* (ARMPF_SDRAM0_PADDR & L1PT_SPSECTION_VOFFSET) != 0 */
/* The kernel is loaded in the middle of SDRAM0. */
#define	ARMPF_SDRAM_MEMSEG_COUNT	(ARMPF_SDRAM_COUNT + 1)
#define	ARMPF_SDRAM0_SPLIT		1
#endif	/* (ARMPF_SDRAM0_PADDR & L1PT_SPSECTION_VOFFSET) == 0 */

#ifdef	XRAMDEV_CONFIG

/*
 * If xramfs device is configured, the system may have 3 memory segments:
 *	- xramfs device pages and pages between xramfs device and
 *	  data section.
 *	- Rest of SDRAM0
 *	- SDRAM1
 */
#define	XRAMDEV_COUNT		1
#define	ARMPF_MAX_MEMSEG_COUNT	(ARMPF_SDRAM_MEMSEG_COUNT + XRAMDEV_COUNT)

#else	/* !XRAMDEV_CONFIG */

/* Number of memseg is equal to number of memory banks. */
#define	ARMPF_MAX_MEMSEG_COUNT	ARMPF_SDRAM_MEMSEG_COUNT

#endif	/* XRAMDEV_CONFIG */

/*
 * Backup DRAM range.
 * Although NE1 has no backup DRAM, we define backup DRAM range
 * for warm boot test.
 */
#define	ARMPF_BACKUP_DRAM_PADDR	ARMPF_SDRAM0_PADDR
#define	ARMPF_BACKUP_DRAM_SIZE	UINT32_C(0x2000000)	/* 32MB */

/*
 * ===================================================================
 * System internal registers
 * ===================================================================
 */
#define	ARMPF_SYS_VADDR		UINT32_C(0xfe000000)
#define	ARMPF_SYS_PADDR		UINT32_C(0x18000000)
#define	ARMPF_SYS_SIZE		UINT32_C(0x01000000)	/* 16MB */
#define	_NE1_SYSADDR(off)	(ARMPF_SYS_VADDR + UINT32_C(off))

/*
 * System Control Unit
 */

/* Reset Control registers */
#define	NE1_SYS_CTRL_MEMO	_NE1_SYSADDR(0x37c00)	/* Memo */
#define	NE1_SYS_CTRL_BOOTID	_NE1_SYSADDR(0x37c04)	/* Boot ID */
#define	NE1_SYS_CTRL_RST	_NE1_SYSADDR(0x37c08)	/* Reset out */
#define	NE1_SYS_CTRL_RST_STAT	_NE1_SYSADDR(0x37c0c)	/* Reset status */
#define	NE1_SYS_CTRL_CPU_SWRST	_NE1_SYSADDR(0x37c10)	/* CPU SW reset */
#define	NE1_SYS_CTRL_PERI_SWRST	_NE1_SYSADDR(0x37c14)	/* Peripheral SW reset*/
#define	NE1_SYS_CTRL_RST_TIMER	_NE1_SYSADDR(0x37c18)	/* Reset out timer */

/* Clock Control registers */
#define	NE1_SYS_CTRL_CLKMASK	_NE1_SYSADDR(0x37c80)	/* Clock mask */
#define	NE1_SYS_CTRL_DIV_DCLKI	_NE1_SYSADDR(0x37c8c)	/* Divide DCLKI */
#define	NE1_SYS_CTRL_DIV_SPDIF	_NE1_SYSADDR(0x37c90)	/* Divide SPDIF_CLKO */
#define	NE1_SYS_CTRL_DIV_I2S	_NE1_SYSADDR(0x37c94)	/* Divide I2S_CLK */

/* CPU Control registers */
#define	NE1_SYS_CTRL_FIQ_MASK	_NE1_SYSADDR(0x37d00)	/* MPCore FIQ mask */
#define	NE1_SYS_CTRL_AXI_PORT	_NE1_SYSADDR(0x37d04)	/* MPCore AXI port */
#define	NE1_SYS_CTRL_STAT	_NE1_SYSADDR(0x37d08)	/* MPCore status */
#define	NE1_SYS_CTRL_STAT_MON	_NE1_SYSADDR(0x37d0c)	/* MPCore stat monitor*/

/*
 * ===================================================================
 * MPCore Private Memory Region
 * ===================================================================
 */
#define	ARMPF_SCU_VADDR		UINT32_C(0xffff2000)
#define	ARMPF_SCU_PADDR		UINT32_C(0xc0000000)
#define	ARMPF_SCU_SIZE		UINT32_C(0x00002000)	/* 8KB */

#define	_NE1_SCUADDR(off)	(ARMPF_SCU_VADDR + UINT32_C(off))

#define	MPCORE_SCU_VADDR	ARMPF_SCU_VADDR

/*
 * ===================================================================
 * On-board peripherals.
 * ===================================================================
 */

/* NOR Flash */
#define	NE1_NOR_FLASH_PADDR	UINT32_C(0x08000000)
#define	NE1_NOR_FLASH_SIZE	UINT32_C(0x8000000)	/* 128M */
#define	_NE1_NOR_PADDR(off)	(NE1_NOR_FLASH_PADDR + UINT32_C(off))

#define ARMPF_NOR_FLASH_PADDR	NE1_NOR_FLASH_PADDR

/*
 * NOR FLASH disk property.
 * - NORF_START_OFFSET
 *    NOR FLASH start address offset.(must be multiples of PAGESIZE)
 * - NORF_DISK_SIZE
 *    NOR FLASH disk size. (must be multiples of block size(512byte))
 */
#define NORF_START_OFFSET	(NE1_NOR_FLASH_SIZE / 2)
#define NORF_DISK_SIZE		(NE1_NOR_FLASH_SIZE / 2)

/* Core FPGA and AV FPGA */
#define	NE1_COREFPGA_PADDR	ARMMACH_COREFPGA_PADDR
#define	NE1_COREFPGA_SIZE	UINT32_C(0x20000)	/* 128K */

/*
 * CORE-FPGA, Ether, PCI Config, and PCI I/O space is mapped by kernel.
 * We choose these addresses to share L2PT with the system vector page.
 */
#define	NE1_PCI_CONFIG_VADDR	UINT32_C(0xfffe0000)
#define	ARMPF_PCI_CONFIG_VADDR	NE1_PCI_CONFIG_VADDR

#define	NE1_COREFPGA_VADDR	UINT32_C(0xfffc0000)
#define	NE1_STATUS_LED_VADDR	(NE1_COREFPGA_VADDR + 0x030)
#define	NE1_CONFIG_CLK_VADDR	(NE1_COREFPGA_VADDR + 0x060)

/*
 * ===================================================================
 * Feature configuration
 * ===================================================================
 */

/* 
 * The following definitions determines whether the specified feature is
 * supported on the target board. True means implemented.
 */
#define	ARMPF_L220_EXIST	0		/* L220 L2 cache */
#define	ARMPF_RTC_EXIST		0		/* RTC */

/*
 * ===================================================================
 * Other memory definitions
 * ===================================================================
 */

/*
 * X Window mapping.
 * It uses the last 9M in SDRAM , and it is not managed by page_t.
 */
#define	ARMPF_XWINDOW_VADDR	UINT32_C(0xff000000)
#define	ARMPF_XWINDOW_SIZE	UINT32_C(0x00900000)
#define	ARMPF_XWINDOW_PADDR					\
	(ARMPF_SDRAM1_PADDR + ARMPF_SDRAM1_SIZE - ARMPF_XWINDOW_SIZE)

/*
 * Ramdisk for root filesystem.
 * It is not managed by page_t.
 */
#define	RAMDISK_ROOT_PADDR	(ARMPF_XWINDOW_PADDR - RAMDISK_ROOT_SIZE)

/* UART register size per one port. */
#define	NE1_UART_SIZE		UINT32_C(0x400)

#define	UART_DEFAULT_BAUDRATE_DLL	UINT32_C(0xd8)
#define	UART_DEFAULT_BAUDRATE_DLH	UINT32_C(0x00)

#define	NE1_UART0_OFFSET	UINT32_C(0x34000)
#define	NE1_UART1_OFFSET	UINT32_C(0x34400)
#define	NE1_UART2_OFFSET	UINT32_C(0x34800)
#define	NE1_UART3_OFFSET	UINT32_C(0x34c00)
#define	NE1_UART4_OFFSET	UINT32_C(0x35000)
#define	NE1_UART5_OFFSET	UINT32_C(0x35400)
#define	NE1_UART6_OFFSET	UINT32_C(0x35800)
#define	NE1_UART7_OFFSET	UINT32_C(0x35c00)

#define	NE1_UART0_PADDR		(ARMPF_SYS_PADDR + NE1_UART0_OFFSET)
#define	NE1_UART1_PADDR		(ARMPF_SYS_PADDR + NE1_UART1_OFFSET)
#define	NE1_UART2_PADDR		(ARMPF_SYS_PADDR + NE1_UART2_OFFSET)
#define	NE1_UART3_PADDR		(ARMPF_SYS_PADDR + NE1_UART3_OFFSET)
#define	NE1_UART4_PADDR		(ARMPF_SYS_PADDR + NE1_UART4_OFFSET)
#define	NE1_UART5_PADDR		(ARMPF_SYS_PADDR + NE1_UART5_OFFSET)
#define	NE1_UART6_PADDR		(ARMPF_SYS_PADDR + NE1_UART6_OFFSET)
#define	NE1_UART7_PADDR		(ARMPF_SYS_PADDR + NE1_UART7_OFFSET)

#define	NE1_UART0_VADDR		(ARMPF_SYS_VADDR + NE1_UART0_OFFSET)
#define	NE1_UART1_VADDR		(ARMPF_SYS_VADDR + NE1_UART1_OFFSET)
#define	NE1_UART2_VADDR		(ARMPF_SYS_VADDR + NE1_UART2_OFFSET)
#define	NE1_UART3_VADDR		(ARMPF_SYS_VADDR + NE1_UART3_OFFSET)
#define	NE1_UART4_VADDR		(ARMPF_SYS_VADDR + NE1_UART4_OFFSET)
#define	NE1_UART5_VADDR		(ARMPF_SYS_VADDR + NE1_UART5_OFFSET)
#define	NE1_UART6_VADDR		(ARMPF_SYS_VADDR + NE1_UART6_OFFSET)
#define	NE1_UART7_VADDR		(ARMPF_SYS_VADDR + NE1_UART7_OFFSET)

/* UART addresses */
#define	NE1_UARTOFF_RBR		UINT32_C(0x0000)	/* RBR */
#define	NE1_UARTOFF_THR		UINT32_C(0x0000)	/* THR */
#define	NE1_UARTOFF_DLL		UINT32_C(0x0000)	/* DLL */
#define	NE1_UARTOFF_IER		UINT32_C(0x0004)	/* IER */
#define	NE1_UARTOFF_DLH		UINT32_C(0x0004)	/* DLH */
#define	NE1_UARTOFF_IIR		UINT32_C(0x0008)	/* IIR */
#define	NE1_UARTOFF_FCR		UINT32_C(0x0008)	/* FCR */
#define	NE1_UARTOFF_LCR		UINT32_C(0x000c)	/* LCR */
#define	NE1_UARTOFF_MCR		UINT32_C(0x0010)	/* MCR */
#define	NE1_UARTOFF_LSR		UINT32_C(0x0014)	/* LSR */
#define	NE1_UARTOFF_MSR		UINT32_C(0x0018)	/* MSR */
#define	NE1_UARTOFF_SCR		UINT32_C(0x001c)	/* SCR */
#define	NE1_UARTOFF_FDR		UINT32_C(0x0020)	/* FDR */

/*
 * UART register format
 */

#ifdef	__STDC__
#define	_NE1_UART_VADDR(pkg, off)	(NE1_UART##pkg##_VADDR + (off))
#else	/* !__STDC__ */
#define	_NE1_UART_VADDR(pkg, off)	(NE1_UART/**/pkg/**/_VADDR) + (off))
#endif	/* __STDC__ */

#define	NE1_UART0_RBR		_NE1_UART_VADDR(0, NE1_UARTOFF_RBR)
#define	NE1_UART0_THR		_NE1_UART_VADDR(0, NE1_UARTOFF_THR)
#define	NE1_UART0_DLL		_NE1_UART_VADDR(0, NE1_UARTOFF_DLL)
#define	NE1_UART0_IER		_NE1_UART_VADDR(0, NE1_UARTOFF_IER)
#define	NE1_UART0_DLH		_NE1_UART_VADDR(0, NE1_UARTOFF_DLH)
#define	NE1_UART0_IIR		_NE1_UART_VADDR(0, NE1_UARTOFF_IIR)
#define	NE1_UART0_FCR		_NE1_UART_VADDR(0, NE1_UARTOFF_FCR)
#define	NE1_UART0_LCR		_NE1_UART_VADDR(0, NE1_UARTOFF_LCR)
#define	NE1_UART0_MCR		_NE1_UART_VADDR(0, NE1_UARTOFF_MCR)
#define	NE1_UART0_LSR		_NE1_UART_VADDR(0, NE1_UARTOFF_LSR)
#define	NE1_UART0_MSR		_NE1_UART_VADDR(0, NE1_UARTOFF_MSR)
#define	NE1_UART0_SCR		_NE1_UART_VADDR(0, NE1_UARTOFF_SCR)
#define	NE1_UART0_FDR		_NE1_UART_VADDR(0, NE1_UARTOFF_FDR)
#define	NE1_UART1_RBR		_NE1_UART_VADDR(1, NE1_UARTOFF_RBR)
#define	NE1_UART1_THR		_NE1_UART_VADDR(1, NE1_UARTOFF_THR)
#define	NE1_UART1_DLL		_NE1_UART_VADDR(1, NE1_UARTOFF_DLL)
#define	NE1_UART1_IER		_NE1_UART_VADDR(1, NE1_UARTOFF_IER)
#define	NE1_UART1_DLH		_NE1_UART_VADDR(1, NE1_UARTOFF_DLH)
#define	NE1_UART1_IIR		_NE1_UART_VADDR(1, NE1_UARTOFF_IIR)
#define	NE1_UART1_FCR		_NE1_UART_VADDR(1, NE1_UARTOFF_FCR)
#define	NE1_UART1_LCR		_NE1_UART_VADDR(1, NE1_UARTOFF_LCR)
#define	NE1_UART1_MCR		_NE1_UART_VADDR(1, NE1_UARTOFF_MCR)
#define	NE1_UART1_LSR		_NE1_UART_VADDR(1, NE1_UARTOFF_LSR)
#define	NE1_UART1_MSR		_NE1_UART_VADDR(1, NE1_UARTOFF_MSR)
#define	NE1_UART1_SCR		_NE1_UART_VADDR(1, NE1_UARTOFF_SCR)
#define	NE1_UART1_FDR		_NE1_UART_VADDR(1, NE1_UARTOFF_FDR)
#define	NE1_UART2_RBR		_NE1_UART_VADDR(2, NE1_UARTOFF_RBR)
#define	NE1_UART2_THR		_NE1_UART_VADDR(2, NE1_UARTOFF_THR)
#define	NE1_UART2_DLL		_NE1_UART_VADDR(2, NE1_UARTOFF_DLL)
#define	NE1_UART2_IER		_NE1_UART_VADDR(2, NE1_UARTOFF_IER)
#define	NE1_UART2_DLH		_NE1_UART_VADDR(2, NE1_UARTOFF_DLH)
#define	NE1_UART2_IIR		_NE1_UART_VADDR(2, NE1_UARTOFF_IIR)
#define	NE1_UART2_FCR		_NE1_UART_VADDR(2, NE1_UARTOFF_FCR)
#define	NE1_UART2_LCR		_NE1_UART_VADDR(2, NE1_UARTOFF_LCR)
#define	NE1_UART2_MCR		_NE1_UART_VADDR(2, NE1_UARTOFF_MCR)
#define	NE1_UART2_LSR		_NE1_UART_VADDR(2, NE1_UARTOFF_LSR)
#define	NE1_UART2_MSR		_NE1_UART_VADDR(2, NE1_UARTOFF_MSR)
#define	NE1_UART2_SCR		_NE1_UART_VADDR(2, NE1_UARTOFF_SCR)
#define	NE1_UART2_FDR		_NE1_UART_VADDR(2, NE1_UARTOFF_FDR)
#define	NE1_UART3_RBR		_NE1_UART_VADDR(3, NE1_UARTOFF_RBR)
#define	NE1_UART3_THR		_NE1_UART_VADDR(3, NE1_UARTOFF_THR)
#define	NE1_UART3_DLL		_NE1_UART_VADDR(3, NE1_UARTOFF_DLL)
#define	NE1_UART3_IER		_NE1_UART_VADDR(3, NE1_UARTOFF_IER)
#define	NE1_UART3_DLH		_NE1_UART_VADDR(3, NE1_UARTOFF_DLH)
#define	NE1_UART3_IIR		_NE1_UART_VADDR(3, NE1_UARTOFF_IIR)
#define	NE1_UART3_FCR		_NE1_UART_VADDR(3, NE1_UARTOFF_FCR)
#define	NE1_UART3_LCR		_NE1_UART_VADDR(3, NE1_UARTOFF_LCR)
#define	NE1_UART3_MCR		_NE1_UART_VADDR(3, NE1_UARTOFF_MCR)
#define	NE1_UART3_LSR		_NE1_UART_VADDR(3, NE1_UARTOFF_LSR)
#define	NE1_UART3_MSR		_NE1_UART_VADDR(3, NE1_UARTOFF_MSR)
#define	NE1_UART3_SCR		_NE1_UART_VADDR(3, NE1_UARTOFF_SCR)
#define	NE1_UART3_FDR		_NE1_UART_VADDR(3, NE1_UARTOFF_FDR)
#define	NE1_UART4_RBR		_NE1_UART_VADDR(4, NE1_UARTOFF_RBR)
#define	NE1_UART4_THR		_NE1_UART_VADDR(4, NE1_UARTOFF_THR)
#define	NE1_UART4_DLL		_NE1_UART_VADDR(4, NE1_UARTOFF_DLL)
#define	NE1_UART4_IER		_NE1_UART_VADDR(4, NE1_UARTOFF_IER)
#define	NE1_UART4_DLH		_NE1_UART_VADDR(4, NE1_UARTOFF_DLH)
#define	NE1_UART4_IIR		_NE1_UART_VADDR(4, NE1_UARTOFF_IIR)
#define	NE1_UART4_FCR		_NE1_UART_VADDR(4, NE1_UARTOFF_FCR)
#define	NE1_UART4_LCR		_NE1_UART_VADDR(4, NE1_UARTOFF_LCR)
#define	NE1_UART4_MCR		_NE1_UART_VADDR(4, NE1_UARTOFF_MCR)
#define	NE1_UART4_LSR		_NE1_UART_VADDR(4, NE1_UARTOFF_LSR)
#define	NE1_UART4_MSR		_NE1_UART_VADDR(4, NE1_UARTOFF_MSR)
#define	NE1_UART4_SCR		_NE1_UART_VADDR(4, NE1_UARTOFF_SCR)
#define	NE1_UART4_FDR		_NE1_UART_VADDR(4, NE1_UARTOFF_FDR)
#define	NE1_UART5_RBR		_NE1_UART_VADDR(5, NE1_UARTOFF_RBR)
#define	NE1_UART5_THR		_NE1_UART_VADDR(5, NE1_UARTOFF_THR)
#define	NE1_UART5_DLL		_NE1_UART_VADDR(5, NE1_UARTOFF_DLL)
#define	NE1_UART5_IER		_NE1_UART_VADDR(5, NE1_UARTOFF_IER)
#define	NE1_UART5_DLH		_NE1_UART_VADDR(5, NE1_UARTOFF_DLH)
#define	NE1_UART5_IIR		_NE1_UART_VADDR(5, NE1_UARTOFF_IIR)
#define	NE1_UART5_FCR		_NE1_UART_VADDR(5, NE1_UARTOFF_FCR)
#define	NE1_UART5_LCR		_NE1_UART_VADDR(5, NE1_UARTOFF_LCR)
#define	NE1_UART5_MCR		_NE1_UART_VADDR(5, NE1_UARTOFF_MCR)
#define	NE1_UART5_LSR		_NE1_UART_VADDR(5, NE1_UARTOFF_LSR)
#define	NE1_UART5_MSR		_NE1_UART_VADDR(5, NE1_UARTOFF_MSR)
#define	NE1_UART5_SCR		_NE1_UART_VADDR(5, NE1_UARTOFF_SCR)
#define	NE1_UART5_FDR		_NE1_UART_VADDR(5, NE1_UARTOFF_FDR)
#define	NE1_UART6_RBR		_NE1_UART_VADDR(6, NE1_UARTOFF_RBR)
#define	NE1_UART6_THR		_NE1_UART_VADDR(6, NE1_UARTOFF_THR)
#define	NE1_UART6_DLL		_NE1_UART_VADDR(6, NE1_UARTOFF_DLL)
#define	NE1_UART6_IER		_NE1_UART_VADDR(6, NE1_UARTOFF_IER)
#define	NE1_UART6_DLH		_NE1_UART_VADDR(6, NE1_UARTOFF_DLH)
#define	NE1_UART6_IIR		_NE1_UART_VADDR(6, NE1_UARTOFF_IIR)
#define	NE1_UART6_FCR		_NE1_UART_VADDR(6, NE1_UARTOFF_FCR)
#define	NE1_UART6_LCR		_NE1_UART_VADDR(6, NE1_UARTOFF_LCR)
#define	NE1_UART6_MCR		_NE1_UART_VADDR(6, NE1_UARTOFF_MCR)
#define	NE1_UART6_LSR		_NE1_UART_VADDR(6, NE1_UARTOFF_LSR)
#define	NE1_UART6_MSR		_NE1_UART_VADDR(6, NE1_UARTOFF_MSR)
#define	NE1_UART6_SCR		_NE1_UART_VADDR(6, NE1_UARTOFF_SCR)
#define	NE1_UART6_FDR		_NE1_UART_VADDR(6, NE1_UARTOFF_FDR)
#define	NE1_UART7_RBR		_NE1_UART_VADDR(7, NE1_UARTOFF_RBR)
#define	NE1_UART7_THR		_NE1_UART_VADDR(7, NE1_UARTOFF_THR)
#define	NE1_UART7_DLL		_NE1_UART_VADDR(7, NE1_UARTOFF_DLL)
#define	NE1_UART7_IER		_NE1_UART_VADDR(7, NE1_UARTOFF_IER)
#define	NE1_UART7_DLH		_NE1_UART_VADDR(7, NE1_UARTOFF_DLH)
#define	NE1_UART7_IIR		_NE1_UART_VADDR(7, NE1_UARTOFF_IIR)
#define	NE1_UART7_FCR		_NE1_UART_VADDR(7, NE1_UARTOFF_FCR)
#define	NE1_UART7_LCR		_NE1_UART_VADDR(7, NE1_UARTOFF_LCR)
#define	NE1_UART7_MCR		_NE1_UART_VADDR(7, NE1_UARTOFF_MCR)
#define	NE1_UART7_LSR		_NE1_UART_VADDR(7, NE1_UARTOFF_LSR)
#define	NE1_UART7_MSR		_NE1_UART_VADDR(7, NE1_UARTOFF_MSR)
#define	NE1_UART7_SCR		_NE1_UART_VADDR(7, NE1_UARTOFF_SCR)
#define	NE1_UART7_FDR		_NE1_UART_VADDR(7, NE1_UARTOFF_FDR)

/*
 * ===================================================================
 * Macros to determine builtin I/O space.
 * ===================================================================
 */
#define	_NE1_IN_RANGE(start, end, base, size)				\
	((uint32_t)(start) >= (base) && (uint32_t)(end) <= (base) + (size))
#define	_NE1_BUILTIN_PTOV(paddr, pbase, vbase)	\
	((vbase) + ((uint32_t)(paddr) - (pbase)))

#define	NE1_IS_SYS_VADDR(pstart, pend)					\
	_NE1_IN_RANGE(pstart, pend, ARMPF_SYS_VADDR, ARMPF_SYS_SIZE)
#define	NE1_IS_SYS_PADDR(pstart, pend)					\
	_NE1_IN_RANGE(pstart, pend, ARMPF_SYS_PADDR, ARMPF_SYS_SIZE)
#define	NE1_SYS_PTOV(paddr)					\
	_NE1_BUILTIN_PTOV(paddr, ARMPF_SYS_PADDR, ARMPF_SYS_VADDR)

#define	NE1_IS_PCI_CONFIG_VADDR(pstart, pend)				\
	_NE1_IN_RANGE(pstart, pend, NE1_PCI_CONFIG_VADDR,		\
		      NE1_PCI_CONFIG_SIZE)
#define	NE1_IS_PCI_CONFIG_PADDR(pstart, pend)				\
	_NE1_IN_RANGE(pstart, pend, NE1_PCI_CONFIG_PADDR,		\
		      NE1_PCI_CONFIG_SIZE)
#define	NE1_PCI_CONFIG_PTOV(paddr)					\
	_NE1_BUILTIN_PTOV(paddr, NE1_PCI_CONFIG_PADDR, NE1_PCI_CONFIG_VADDR)

#define	ARMPF_BUILTIN_IOSPACE_PTOV(pstart, pend)			\
	((NE1_IS_SYS_PADDR(pstart, pend)) ? NE1_SYS_PTOV(pstart)	\
	 : (NE1_IS_PCI_CONFIG_PADDR(pstart, pend)) ? 			\
		NE1_PCI_CONFIG_PTOV(pstart)				\
	 : NULL)
#define	ARMPF_IS_BUILTIN_IOSPACE(pstart, pend)		\
	(NE1_IS_SYS_VADDR(pstart, pend) ||		\
	 NE1_IS_PCI_CONFIG_VADDR(pstart, pend))

/*
 * ===================================================================
 * IRQ definitions
 * ===================================================================
 */
/* Inter Processer Interrupt */
#define	IRQ_IPI_LO	0
#define	IRQ_IPI_HI	1
#define	IRQ_IPI_CPUPOKE	2
#define	IRQ_IPI_CBE	3

#define	IRQ_LOCALTIMER	29
#define	IRQ_LOCALWDT	30
#define	IRQ_GIC_START	32

/* GIC interrupts */
#define	GICINT_EXBUS		0
#define	GICINT_I2C		1
#define	GICINT_CSI0		2
#define	GICINT_CSI1		3
#define	GICINT_TIMER0		4
#define	GICINT_TIMER1		5
#define	GICINT_TIMER2		6
#define	GICINT_TIMER3		7
#define	GICINT_TIMER4		8
#define	GICINT_TIMER5		9
#define	GICINT_PWM		10
#define	GICINT_SD0		11
#define	GICINT_SD1		12
#define	GICINT_CF		13
#define	GICINT_NAND		14
#define	GICINT_MIF		15
#define	GICINT_DTV		16
#define	GICINT_SGX		17
#define	GICINT_DISP0		18
#define	GICINT_DISP1		19
#define	GICINT_DISP2		20
#define	GICINT_VIDEO		21
#define	GICINT_SPDIF0		22
#define	GICINT_SPDIF1		23
#define	GICINT_I2S0		24
#define	GICINT_I2S1		25
#define	GICINT_I2S2		26
#define	GICINT_I2S3		27
#define	GICINT_APB		28
#define	GICINT_AHB_BRIDGE0	29
#define	GICINT_AHB_BRIDGE1	30
#define	GICINT_AHB_BRIDGE2	31
#define	GICINT_AXI		32
#define	GICINT_PCI_INT		33
#define	GICINT_PCI_SERRB	34
#define	GICINT_PCI_PERRB	35
#define	GICINT_EXPCI_INT	36
#define	GICINT_EXPCI_SERRB	37
#define	GICINT_EXPCI_PERRB	38
#define	GICINT_USBH_INTA	39
#define	GICINT_USBH_INTB	40
#define	GICINT_USBH_SMI		41
#define	GICINT_USBH_PME		42
#define	GICINT_ATA6		43
#define	GICINT_DMAC32_0END	44
#define	GICINT_DMAC32_0ERR	45
#define	GICINT_DMAC32_1END	46
#define	GICINT_DMAC32_1ERR	47
#define	GICINT_DMAC32_2END	48
#define	GICINT_DMAC32_2ERR	49
#define	GICINT_DMAC32_3END	50
#define	GICINT_DMAC32_3ERR	51
#define	GICINT_DMAC32_4END	52
#define	GICINT_DMAC32_4ERR	53
#define	GICINT_UART0		54
#define	GICINT_UART1		55
#define	GICINT_UART2		56
#define	GICINT_UART3		57
#define	GICINT_UART4		58
#define	GICINT_UART5		59
#define	GICINT_UART6		60
#define	GICINT_UART7		61
#define	GICINT_EWDT		63
#define	GICINT_DMAC_AXI_END	65
#define	GICINT_PMUIRQ0		68
#define	GICINT_PMUIRQ1		69
#define	GICINT_PMUIRQ2		70
#define	GICINT_PMUIRQ3		71
#define	GICINT_PMUIRQ4		72
#define	GICINT_PMUIRQ5		73
#define	GICINT_PMUIRQ6		74
#define	GICINT_PMUIRQ7		75
#define	GICINT_PMUIRQ8		76
#define	GICINT_PMUIRQ9		77
#define	GICINT_PMUIRQ10		78
#define	GICINT_PMUIRQ11		79
#define	GICINT_COMMRX0		80
#define	GICINT_COMMRX1		81
#define	GICINT_COMMRX2		82
#define	GICINT_COMMRX3		83
#define	GICINT_COMMTX0		84
#define	GICINT_COMMTX1		85
#define	GICINT_COMMTX2		86
#define	GICINT_COMMTX3		87
#define	GICINT_PWRCTLO0		88
#define	GICINT_PWRCTLO1		89
#define	GICINT_PWRCTLO2		90
#define	GICINT_PWRCTLO3		91
#define	GICINT_DMAC_EXBUS_END	92
#define	GICINT_DMAC_EXBUS_ERR	93
#define	GICINT_AHB_BRIDDGE3	94
#define	GICINT_TEST		95

#ifdef	__STDC__
#define	GIC_IRQ(intr)		(IRQ_GIC_START + GICINT_##intr)
#else	/* !__STDC__ */
#define	GIC_IRQ(intr)		(IRQ_GIC_START + GICINT_/**/intr)
#endif	/* __STDC__ */

#define	IRQ_TIMER0	GIC_IRQ(TIMER0)
#define	IRQ_TIMER1	GIC_IRQ(TIMER1)
#define	IRQ_TIMER2	GIC_IRQ(TIMER2)
#define	IRQ_TIMER3	GIC_IRQ(TIMER3)
#define	IRQ_TIMER4	GIC_IRQ(TIMER4)
#define	IRQ_TIMER5	GIC_IRQ(TIMER5)
#define	IRQ_PCIINTR	GIC_IRQ(PCI_INT)
#define	IRQ_PCISERRB	GIC_IRQ(PCI_SERRB)
#define	IRQ_PCIPERRB	GIC_IRQ(PCI_PERRB)
#define	IRQ_EXPCIINTR	GIC_IRQ(EXPCI_INT)
#define	IRQ_EXPCISERRB	GIC_IRQ(EXPCI_SERRB)
#define	IRQ_EXPCIPERRB	GIC_IRQ(EXPCI_PERRB)
#define	IRQ_USBHINTA	GIC_IRQ(USBH_INTA)
#define	IRQ_USBHINTB	GIC_IRQ(USBH_INTB)
#define	IRQ_USBHSMI	GIC_IRQ(USBH_SMI)
#define	IRQ_USBHPME	GIC_IRQ(USBH_PME)
#define	IRQ_UART0	GIC_IRQ(UART0)
#define	IRQ_UART1	GIC_IRQ(UART1)
#define	IRQ_UART2	GIC_IRQ(UART2)
#define	IRQ_UART3	GIC_IRQ(UART3)
#define	IRQ_UART4	GIC_IRQ(UART4)
#define	IRQ_UART5	GIC_IRQ(UART5)
#define	IRQ_UART6	GIC_IRQ(UART6)
#define	IRQ_UART7	GIC_IRQ(UART7)
#define	IRQ_PMU_CPU0	GIC_IRQ(PMUIRQ0)
#define	IRQ_PMU_CPU1	GIC_IRQ(PMUIRQ1)
#define	IRQ_PMU_CPU2	GIC_IRQ(PMUIRQ2)
#define	IRQ_PMU_CPU3	GIC_IRQ(PMUIRQ3)
#define	IRQ_PMU_SCU0	GIC_IRQ(PMUIRQ4)
#define	IRQ_I2S0	GIC_IRQ(I2S0)
#define	IRQ_I2S1	GIC_IRQ(I2S1)
#define	IRQ_I2S2	GIC_IRQ(I2S2)
#define	IRQ_I2S3	GIC_IRQ(I2S3)
#define	IRQ_DMAC32_0END	GIC_IRQ(DMAC32_0END)
#define	IRQ_DMAC32_0ERR	GIC_IRQ(DMAC32_0ERR)
#define	IRQ_DMAC32_1END	GIC_IRQ(DMAC32_1END)
#define	IRQ_DMAC32_1ERR	GIC_IRQ(DMAC32_1ERR)
#define	IRQ_DMAC32_2END	GIC_IRQ(DMAC32_2END)
#define	IRQ_DMAC32_2ERR	GIC_IRQ(DMAC32_2ERR)
#define	IRQ_DMAC32_3END	GIC_IRQ(DMAC32_3END)
#define	IRQ_DMAC32_3ERR	GIC_IRQ(DMAC32_3ERR)
#define	IRQ_DMAC32_4END	GIC_IRQ(DMAC32_4END)
#define	IRQ_DMAC32_4ERR	GIC_IRQ(DMAC32_4ERR)

/* MPCore FIQ Mask Control Register bits */
#define	FIQ0MSK_SET		0
#define	FIQ1MSK_SET		1
#define	FIQ2MSK_SET		2
#define	FIQ3MSK_SET		3
#define	FIQ0MSK_CLR		8
#define	FIQ1MSK_CLR		9
#define	FIQ2MSK_CLR		10
#define	FIQ3MSK_CLR		11
#define	FIQ0_CLR		16
#define	FIQ1_CLR		17
#define	FIQ2_CLR		18
#define	FIQ3_CLR		19
#define	FIQ0_DISP		24
#define	FIQ1_DISP		25
#define	FIQ2_DISP		26
#define	FIQ3_DISP		27

#define	FIQMSK_SET(cpuid)	(FIQ0MSK_SET + (cpuid))
#define	FIQMSK_CLR(cpuid)	(FIQ0MSK_CLR + (cpuid))
#define	FIQ_CLR(cpuid)		(FIQ0_CLR + (cpuid))

/*
 * Console serial port definition.
 * Use UART0 as console device.
 */
#define	NE1_CONSOLE_PADDR	NE1_UART0_PADDR
#define	NE1_CONSOLE_RBR		NE1_UART0_RBR
#define	NE1_CONSOLE_THR		NE1_UART0_THR
#define	NE1_CONSOLE_DLL		NE1_UART0_DLL
#define	NE1_CONSOLE_IER		NE1_UART0_IER
#define	NE1_CONSOLE_DLH		NE1_UART0_DLH
#define	NE1_CONSOLE_IIR		NE1_UART0_IIR
#define	NE1_CONSOLE_FCR		NE1_UART0_FCR
#define	NE1_CONSOLE_LCR		NE1_UART0_LCR
#define	NE1_CONSOLE_MCR		NE1_UART0_MCR
#define	NE1_CONSOLE_LSR		NE1_UART0_LSR
#define	NE1_CONSOLE_MSR		NE1_UART0_MSR
#define	NE1_CONSOLE_SCR		NE1_UART0_SCR
#define	NE1_CONSOLE_FDR		NE1_UART0_FDR
#define	NE1_CONSOLE_DDIPATH	"/ne_uart@1,18034000:a"

#define	ARMPF_CONSOLE_DDIPATH	NE1_CONSOLE_DDIPATH

/* UART_LSR bits */
#define	NE1_UART_LSR_LSR0	UINT32_C(0x01)
#define	NE1_UART_LSR_LSR1	UINT32_C(0x02)
#define	NE1_UART_LSR_LSR2	UINT32_C(0x04)
#define	NE1_UART_LSR_LSR3	UINT32_C(0x08)
#define	NE1_UART_LSR_LSR4	UINT32_C(0x10)
#define	NE1_UART_LSR_LSR5	UINT32_C(0x20)
#define	NE1_UART_LSR_LSR6	UINT32_C(0x40)
#define	NE1_UART_LSR_LSR7	UINT32_C(0x80)

/* UART_IER bits */
#define	NE1_UART_IER_IE0	UINT32_C(0x01)
#define	NE1_UART_IER_IE1	UINT32_C(0x02)
#define	NE1_UART_IER_IE2	UINT32_C(0x04)
#define	NE1_UART_IER_IE3	UINT32_C(0x08)

#define	NE1_PCI_CONFIG_PADDR	UINT32_C(0x18023000)
#define	NE1_PCI_CONFIG_SIZE	UINT32_C(0x1000)	/* 4K */
#define	NE1_PCI_MEM_PADDR	UINT32_C(0x18024000)
#define	NE1_PCI_MEM_SIZE	UINT32_C(0x2000)	/* 8K */

#define	ARMPF_PCI_CONFIG_PADDR	NE1_PCI_CONFIG_PADDR
#define	ARMPF_PCI_CONFIG_SIZE	NE1_PCI_CONFIG_SIZE
#define	ARMPF_PCI_MEM_PADDR	NE1_PCI_MEM_PADDR
#define	ARMPF_PCI_MEM_SIZE	NE1_PCI_MEM_SIZE

/* I2S */
#define	NE1_I2S0_OFFSET		UINT32_C(0x32400)
#define	NE1_I2S0_PADDR		(ARMPF_SYS_PADDR + NE1_I2S0_OFFSET)
#define	NE1_I2S0_SIZE		UINT32_C(0x400)

/*
 * Physical memory range that can be used as DMA buffer.
 * Note that some hole may exist in [ARMPF_DMA_MIN_PADDR, ARMPF_DMA_MAX_PADDR].
 *
 * No restriction on NE1.
 */
#define	ARMPF_DMA_MIN_PADDR	ARMPF_SDRAM0_PADDR
#define	ARMPF_DMA_MAX_PADDR						\
	(ARMPF_SDRAM1_PADDR + ARMPF_SDRAM1_SIZE - ARMPF_XWINDOW_SIZE - 1)

#ifndef	_ASM

#include <sys/types.h>

/* Read and write register. */
#define	readw(addr)		*((volatile uint16_t *)(addr))
#define	writew(value, addr)	*((volatile uint16_t *)(addr)) = (value)
#define	readl(addr)		*((volatile uint32_t *)(addr))
#define	writel(value, addr)	*((volatile uint32_t *)(addr)) = (value)

#endif	/* !_ASM */


#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* !_SYS_PLATFORM_H */
