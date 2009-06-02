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
 * Copyright (c) 2006-2008 NEC Corporation
 * All rights reserved.
 */

#ifndef	_SYS_MPCORE_H
#define	_SYS_MPCORE_H

#include <sys/feature_tests.h>
#include <sys/int_const.h>
#if	defined(_KERNEL) && defined(_KERNEL_BUILD_TREE) && defined(_MACHDEP)
#include <sys/platform.h>	/* For MPCORE_SCU_VADDR */
#endif	/* _KERNEL && _KERNEL_BUILD_TREE && _MACHDEP */

#ifdef __cplusplus
extern "C" {
#endif

/* Address offset of SCU-specific registers. */
#define	MPCORE_SCU_CTRL		0x00	/* SCU Control Register */
#define	MPCORE_SCU_CONFIG	0x04	/* SCU Configuration Register */
#define	MPCORE_SCU_CPUSTAT	0x08	/* SCU CPU Status Register */
#define	MPCORE_SCU_INVALL	0x0c	/* SCU Invalidate All Register */
#define	MPCORE_SCU_PERFMON_CTRL	0x10	/* Performance Monitor Control Reg. */
#define	MPCORE_SCU_MONCNT_EV0	0x14	/* Performance Monitor Event 0 Reg. */
#define	MPCORE_SCU_MONCNT_EV1	0x18	/* Performance Monitor Event 1 Reg. */
#define MPCORE_SCU_MONCNT0	0x1C	/* Performance Monitor Counter 0. */

/* Bits in SCU Control Register */
#define	MPCORE_SCU_CTRL_EN		0x0001	/* Enable */
#define	MPCORE_SCU_CTRL_ACCESS_MASK	0x001e	/* SCU regs access ctrl */
#define	MPCORE_SCU_CTRL_CPUIF_MASK	0x01e0	/* CPU IF aliasing ctrl */
#define	MPCORE_SCU_CTRL_PERI_MASK	0x1e00	/* Peripheral IF aliasing ctrl*/

/* Performance Monitor Event Number.*/
#define	MPCORE_SCU_MONCNT_EVN0          0x00	/* Counter disabled.*/
#define	MPCORE_SCU_MONCNT_EVN31         0x1f	/* The counter increments on each cycle.*/

/* Bits in Performance Monitor Control Register.*/
#define MPCORE_SCU_PERFMON_CTRL_EN      0x1	/* Enable.*/
#define MPCORE_SCU_PERFMON_CTRL_RST     0x2	/* Reset counter.*/
#define MPCORE_SCU_PERFMON_CTRL_INTEN0  0x100	/* Interrupt Enable for MN0.*/
#define MPCORE_SCU_PERFMON_CTRL_INTCLR0 0x10000 /* Interrupt flag clear for MN0.*/

#if	defined(_KERNEL) && defined(_KERNEL_BUILD_TREE) && defined(_MACHDEP)
/*
 * Convert address offset of SCU-specific registers into virtual address.
 * MPCORE_SCUREG_VADDR() appends offset prefix automatically. If you need
 * address of MPCORE_SCU_CTRL, you can use MPCORE_SCUREG_VADDR(CTRL).
 */
#define	MPCORE_SCUREG_VBASE	MPCORE_SCU_VADDR

#ifdef	__STDC__
#define	MPCORE_SCUREG_VADDR(off)			\
	(MPCORE_SCUREG_VBASE + MPCORE_SCU_##off)
#else	/* !__STDC__ */
#define	MPCORE_SCUREG_VADDR(off)			\
	(MPCORE_SCUREG_VBASE + MPCORE_SCU_/**/off)
#endif	/* __STDC__ */
#endif	/* _KERNEL && _KERNEL_BUILD_TREE && _MACHDEP */

/* Address offset of CPU interface registers */
#define	MPCORE_CPUIF_CTRL	0x00	/* Control Register */
#define	MPCORE_CPUIF_PRIMASK	0x04	/* Priority Mask Register */
#define	MPCORE_CPUIF_BINPOINT	0x08	/* Binary Point Register */
#define	MPCORE_CPUIF_INTACK	0x0C	/* Interrupt Acknowledge Register */
#define	MPCORE_CPUIF_EOI	0x10	/* End of Interrupt Register */
#define	MPCORE_CPUIF_RUNPRI	0x14	/* Running Priority Register */
#define	MPCORE_CPUIF_HIGHINTR	0x18	/* Highest Pending Interrupt Register */

#if	defined(_KERNEL) && defined(_KERNEL_BUILD_TREE) && defined(_MACHDEP)
/*
 * Convert address offset of CPU interface registers into virtual address.
 * MPCORE_CPUIF_VADDR() appends offset prefix automatically. If you need
 * address of MPCORE_CPUIF_CTRL, you can use MPCORE_CPUIF_VADDR(CTRL).
 */
#define	MPCORE_CPUIF_OFFSET	UINT32_C(0x0100)
#define	MPCORE_CPUIF_VBASE	(MPCORE_SCU_VADDR + MPCORE_CPUIF_OFFSET)

#ifdef	__STDC__
#define	MPCORE_CPUIF_VADDR(off)	(MPCORE_CPUIF_VBASE + MPCORE_CPUIF_##off)
#else	/* !__STDC__ */
#define	MPCORE_CPUIF_VADDR(off)	(MPCORE_CPUIF_VBASE + MPCORE_CPUIF_/**/off)
#endif	/* __STDC__ */
#endif	/* _KERNEL && _KERNEL_BUILD_TREE && _MACHDEP */

/* Address offset of registers in the Interrupt Distributor. */
#define	MPCORE_DIST_CTRL		0x000   /* Control Reg */
#define	MPCORE_DIST_CTRL_TYPE		0x004   /* Controller Type Reg */
#define	MPCORE_DIST_ENABLE_SET		0x100   /* Intr Set-enable Reg */
#define	MPCORE_DIST_ENABLE_CLR		0x180   /* Intr Clear-enable Reg */
#define	MPCORE_DIST_PENDING_SET		0x200   /* Intr Set-pending Reg */
#define	MPCORE_DIST_PENDING_CLR		0x280   /* Intr Clear-pending Reg */
#define	MPCORE_DIST_ACTIVE_BIT		0x300   /* Intr Active Bit Reg */
#define	MPCORE_DIST_PRI			0x400   /* Intr Priority Reg */
#define	MPCORE_DIST_TARGET		0x800   /* Intr CPU targets Reg */
#define	MPCORE_DIST_CONFIG		0xc00   /* Intr Configuration Reg */
#define	MPCORE_DIST_SOFTINT		0xf00   /* Software Intr Register */

#if	defined(_KERNEL) && defined(_KERNEL_BUILD_TREE) && defined(_MACHDEP)
/*
 * Convert address offset of Interrupt Distributor registers into virtual
 * address.
 * MPCORE_DIST_VADDR() appends offset prefix automatically. If you need
 * address of MPCORE_DIST_CTRL, you can use MPCORE_DIST_VADDR(CTRL).
 */
#define	MPCORE_DIST_OFFSET	UINT32_C(0x1000)
#define	MPCORE_DIST_VBASE	(MPCORE_SCU_VADDR + MPCORE_DIST_OFFSET)

#ifdef	__STDC__
#define	MPCORE_DIST_VADDR(off)	(MPCORE_DIST_VBASE + MPCORE_DIST_##off)
#else	/* !__STDC__ */
#define	MPCORE_DIST_VADDR(off)	(MPCORE_DIST_VBASE + MPCORE_DIST_/**/off)
#endif	/* __STDC__ */
#endif	/* _KERNEL && _KERNEL_BUILD_TREE && _MACHDEP */

/* Address offset of MPCore local timer and watchdog */
#define	MPCORE_TWD_TIMER_LOAD		0x00	/* Timer Load Register */
#define	MPCORE_TWD_TIMER_COUNTER	0x04	/* Timer Counter Register */
#define	MPCORE_TWD_TIMER_CTRL		0x08	/* Timer Control Register */
#define	MPCORE_TWD_TIMER_INTSTAT	0x0c	/* Timer Interrupt Status */
#define	MPCORE_TWD_WD_LOAD		0x20	/* Watchdog Load Register */
#define	MPCORE_TWD_WD_COUNTER		0x24	/* Watchdog Counter Register */
#define	MPCORE_TWD_WD_CTRL		0x28	/* Watchdog Control Register */
#define	MPCORE_TWD_WD_INTSTAT		0x2c	/* Watchdog Interrupt Status */
#define	MPCORE_TWD_WD_RESET		0x30	/* Watchdog Reset Sent Reg */
#define	MPCORE_TWD_WD_DISABLE		0x34	/* Watchdog Disable Register */

/* Bits in Timer Control Register */
#define	MPCORE_TWD_TIMER_CTRL_ENABLE	0x1
#define	MPCORE_TWD_TIMER_CTRL_RELOAD	0x2
#define	MPCORE_TWD_TIMER_CTRL_INTR	0x4

#define	MPCORE_TWD_TIMER_CTRL_PRESCALER_MASK	0x0000ff00
#define	MPCORE_TWD_TIMER_CTRL_PRESCALER_SHIFT	8
#define	MPCORE_TWD_TIMER_CTRL_PRESCALER(prescaler)			\
	(((prescaler) << MPCORE_TWD_TIMER_CTRL_PRESCALER_SHIFT) &	\
	 MPCORE_TWD_TIMER_CTRL_PRESCALER_MASK)

/* Bits in Watchdog Timer Control Register */
#define	MPCORE_TWD_WD_CTRL_ENABLE	0x1
#define	MPCORE_TWD_WD_CTRL_RELOAD	0x2
#define	MPCORE_TWD_WD_CTRL_INTR		0x4

#define	MPCORE_TWD_WD_CTRL_PRESCALER_MASK	0x0000ff00
#define	MPCORE_TWD_WD_CTRL_PRESCALER_SHIFT	8
#define	MPCORE_TWD_WD_CTRL_PRESCALER(prescaler)				\
	(((prescaler) << MPCORE_TWD_WD_CTRL_PRESCALER_SHIFT) &		\
	 MPCORE_TWD_WD_CTRL_PRESCALER_MASK)

#if	defined(_KERNEL) && defined(_KERNEL_BUILD_TREE) && defined(_MACHDEP)
/*
 * Convert address offset of MPCore local timer and watchdog registers into
 * virtual address.
 * MPCORE_TWD_VADDR() appends offset prefix automatically. If you need
 * address of MPCORE_TWD_TIMER_LOAD on CPU 0, you can use
 * MPCORE_TWD_VADDR(TIMER_LOAD, 0).
 *
 * MPCORE_TWD_PRIVATE_VADDR() is same as MPCORE_TWD_VADDR(), except that
 * this macro access CPU private timer and watchdog registers identified
 * by CPU transaction ID. If you need address of MPCORE_TWD_TIMER_LOAD on
 * CPU 0, you can use MPCORE_TWD_PRIVATE_VADDR(TIMER_LOAD) on CPU 0.
 *
 * Remarks:
 *	MPCORE_TWD_VADDR() access aliased timer and watchdog registers.
 *	These access may be disabled by SCU control register.
 *	So you should use MPCORE_TWD_PRIVATE_VADDR() if possible.
 */
#define	MPCORE_TWD_PRIVATE_VBASE	(MPCORE_SCU_VADDR + UINT32_C(0x0600))
#define	MPCORE_TWD_VBASE		(MPCORE_SCU_VADDR + UINT32_C(0x0700))
#define	MPCORE_TWD_SIZE			UINT32_C(0x100)

#ifdef	__STDC__
#define	MPCORE_TWD_VADDR(off, cpu)	\
	(MPCORE_TWD_VBASE + MPCORE_TWD_##off + (MPCORE_TWD_SIZE * (cpu)))
#define	MPCORE_TWD_PRIVATE_VADDR(off)	\
	(MPCORE_TWD_PRIVATE_VBASE + MPCORE_TWD_##off)
#else	/* !__STDC__ */
#define	MPCORE_TWD_VADDR(off, cpu)	\
	(MPCORE_TWD_VBASE + MPCORE_TWD_/**/off + (MPCORE_TWD_SIZE * (cpu)))
#define	MPCORE_TWD_PRIVATE_VADDR(off)	\
	(MPCORE_TWD_PRIVATE_VBASE + MPCORE_TWD_/**/off)
#endif	/* __STDC__ */
#endif	/* _KERNEL && _KERNEL_BUILD_TREE && _MACHDEP */

/* Address offset of L220 cache controller registers */
#define	MPCORE_L220_ID		0x000	/* Cache ID Register */
#define	MPCORE_L220_TYPE	0x004	/* Cache Type Register */
#define	MPCORE_L220_CTRL	0x100	/* Cache Control Register */
#define	MPCORE_L220_AUX_CTRL	0x104	/* Auxiliary Control Register */
#define	MPCORE_L220_EVCNT_CTRL	0x200	/* Event Counter Control Register */
#define	MPCORE_L220_EVCNT1_CFG	0x204	/* Event Counter1 Configuration Reg */
#define	MPCORE_L220_EVCNT0_CFG	0x208	/* Event Counter0 Configuration Reg */
#define	MPCORE_L220_EVCNT1_VAL	0x20c	/* Event Counter1 Value Register */
#define	MPCORE_L220_EVCNT0_VAL	0x210	/* Event Counter0 Value Register */
#define	MPCORE_L220_INTMASK	0x214	/* Interrupt Mask Register */
#define	MPCORE_L220_MSK_INTSTAT	0x218	/* Masked Interrupt Status Register */
#define	MPCORE_L220_RAW_INTSTAT	0x21c	/* Raw Interrupt Status Register */
#define	MPCORE_L220_INT_CLR	0x220	/* Interrupt Clear Register */
#define	MPCORE_L220_SYNC	0x730	/* Cache Sync */
#define	MPCORE_L220_INV_PA	0x770	/* Invalidate Line By PA */
#define	MPCORE_L220_INV_WAY	0x77c	/* Invalidate by Way */
#define	MPCORE_L220_CLEAN_PA	0x7b0	/* Clean Line By PA */
#define	MPCORE_L220_CLEAN_IDX	0x7b8	/* Clean Line By Index/Way */
#define	MPCORE_L220_CLEAN_WAY	0x7bc	/* Clean By Way */
#define	MPCORE_L220_CLINV_PA	0x7f0	/* Clean & Invalidate Line by PA */
#define	MPCORE_L220_CLINV_IDX	0x7f8	/* Clean & Invalidate by Index/Way */
#define	MPCORE_L220_CLINV_WAY	0x7fc	/* Clean & Invalidate by Way */
#define	MPCORE_L220_LOCKDOWN_D	0x900	/* Lockdown by Way (D) */
#define	MPCORE_L220_LOCKDOWN_I	0x904	/* Lockdown by Way (I) */
#define	MPCORE_L220_TEST	0xf00	/* Test Operation */
#define	MPCORE_L220_LINE_DATA	0xf10	/* Line Data */
#define	MPCORE_L220_LINE_TAG	0xf30	/* Line Tag */
#define	MPCORE_L220_DEBUG	0xf40	/* Debug Control Register */

#if	defined(_KERNEL) && defined(_KERNEL_BUILD_TREE) && defined(_MACHDEP)
/*
 * Convert address offset of L220 registers into virtual address.
 * MPCORE_L220_VADDR() appends offset prefix automatically. If you need
 * address of MPCORE_L220_ID, you can use MPCORE_L220_VADDR(ID).
 *
 * MPCORE_L220_VBASE must be defined in sys/platform.h
 */
#ifdef	__STDC__
#define	MPCORE_L220_VADDR(off)	(MPCORE_L220_VBASE + MPCORE_L220_##off)
#else	/* !__STDC__ */
#define	MPCORE_L220_VADDR(off)	(MPCORE_L220_VBASE + MPCORE_L220_/**/off)
#endif	/* __STDC__ */
#endif	/* _KERNEL && _KERNEL_BUILD_TREE && _MACHDEP */

/*
 * Fault status register
 */
/* Fault status [10,3:0] */
#define	MPCORE_FSR_STATUS(fsr)	((((fsr) & 0x400) >> 6) | ((fsr) & 0x0f))
#define	MPCORE_FSR_ALIGN	0x01	/* Alignment */
#define	MPCORE_FSR_ICMAIN	0x04	/* I-cache maintenance operation */
#define	MPCORE_FSR_BUSTRNL1	0x0c	/* External abort on translation(L1) */
#define	MPCORE_FSR_BUSTRNL2	0x0e	/* External abort on translation(L2) */
#define	MPCORE_FSR_TRANS_S	0x05	/* Translation -- Section */
#define	MPCORE_FSR_TRANS_P	0x07	/* Translation -- Page */
#define	MPCORE_FSR_ACCESS_S	0x03	/* Access bit -- Section */
#define	MPCORE_FSR_ACCESS_P	0x06	/* Access bit -- Page */
#define	MPCORE_FSR_DOMAIN_S	0x09	/* Domain -- Section */
#define	MPCORE_FSR_DOMAIN_P	0x0b	/* Domain -- Page */
#define	MPCORE_FSR_PERM_S	0x0d	/* Permission -- Section */
#define	MPCORE_FSR_PERM_P	0x0f	/* Permission -- Page */
#define	MPCORE_FSR_PRECISE	0x08	/* Precise external abort */
#define	MPCORE_FSR_IMPRECISE	0x16	/* Imprecise external abort */
#define	MPCORE_FSR_DBGEVNT	0x02	/* Debug event */

/* Type of access caused the abort (read or write) */
#define	MPCORE_FSR_RW(fsr)	((fsr) & (1 << 11))

/*
 * Debug status and control register
 */
/* Method of entry bits [5:2] */
#define	MPCORE_DSCR_ENTRY(dscr)	(((dscr) >> 2) & 0xf)
#define	MPCORE_DSCR_DBGTRAP	0x00	/* Halt DBGTRAP instruction */
#define	MPCORE_DSCR_BREAKP	0x01	/* Breakpoint */
#define	MPCORE_DSCR_WATCHP	0x02	/* Watchpoint */
#define	MPCORE_DSCR_BKPT	0x03	/* BKPT instruction */
#define	MPCORE_DSCR_EDBGRQ	0x04	/* EDBGRQ signal activation */
#define	MPCORE_DSCR_VECTOR	0x05	/* Vector catch */
#define	MPCORE_DSCR_DABT	0x06	/* Data-side abort */
#define	MPCORE_DSCR_IABT	0x07	/* Instruction-side abort */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MPCORE_H */
