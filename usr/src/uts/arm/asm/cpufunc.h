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

#ifndef	_ASM_CPUFUNC_H
#define	_ASM_CPUFUNC_H

#ident	"@(#)arm/asm/cpufunc.h"

/*
 * ARM specific functions and utilities.
 */

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

#include <sys/controlregs.h>
#include <sys/int_const.h>

/*
 * PA register bits
 */
#define	VTOP_PA_S		UINT32_C(0x00000100)	/* shared */

/* Memory type */
#define	VTOP_PA_TYPE_MASK	UINT32_C(0x000000c0)
#define	VTOP_PA_TYPE_STRONG	UINT32_C(0x00000000)	/* strong order */
#define	VTOP_PA_TYPE_DEVICE	UINT32_C(0x00000040)	/* device */
#define	VTOP_PA_TYPE_NORMAL	UINT32_C(0x00000080)	/* normal memory */

/* Inner attributes for normal memory (VTOP_PA_TYPE_NORMAL) */
#define	VTOP_PA_INNER_MASK	UINT32_C(0x00000030)
#define	VTOP_PA_INNER_WB_NA	UINT32_C(0x00000030)	/* write back,
							   no alloc on write */
#define	VTOP_PA_INNER_WT	UINT32_C(0x00000020)	/* write through */
#define	VTOP_PA_INNER_WB	UINT32_C(0x00000010)	/* write back,
							   alloc on write */
#define	VTOP_PA_INNER_NC	UINT32_C(0x00000000)	/* uncached */

/* Outer attributes for normal memory (VTOP_PA_TYPE_NORMAL) */
#define	VTOP_PA_OUTER_MASK	UINT32_C(0x0000000c)
#define	VTOP_PA_OUTER_WB_NA	UINT32_C(0x0000000c)	/* write back,
							   no alloc on write */
#define	VTOP_PA_OUTER_WT	UINT32_C(0x00000008)	/* write through */
#define	VTOP_PA_OUTER_WB	UINT32_C(0x00000004)	/* write back,
							   alloc on write */
#define	VTOP_PA_OUTER_NC	UINT32_C(0x00000000)	/* uncached */

#define	VTOP_PA_ERROR		UINT32_C(0x00000001)	/* error indicator */
#define	VTOP_PA_PADDR_MASK	UINT32_C(0xfffff000)	/* paddr mask */

#ifdef	_ASM

/* Concatinate instruction mnemonic and condition modifier. */
#ifdef	__STDC__
#define	ARM_INST(inst, cond)		inst##cond
#else	/* !__STDC__ */
#define	ARM_INST(inst, cond)		inst/**/cond
#endif	/* __STDC__ */

/*
 * Read/Write CP10, CP14, CP15 register.
 * Condition field is not supported.
 */
#define	READ_CP10(op1, cr1, cr2, op2, reg)	\
	mrc	p10, op1, reg, cr1, cr2, op2
#define	READ_CP14(op1, cr1, cr2, op2, reg)	\
	mrc	p14, op1, reg, cr1, cr2, op2
#define	READ_CP15(op1, cr1, cr2, op2, reg)	\
	mrc	p15, op1, reg, cr1, cr2, op2

#define	WRITE_CP10(op1, cr1, cr2, op2, reg)	\
	mcr	p10, op1, reg, cr1, cr2, op2
#define	WRITE_CP14(op1, cr1, cr2, op2, reg)	\
	mcr	p14, op1, reg, cr1, cr2, op2
#define	WRITE_CP15(op1, cr1, cr2, op2, reg)	\
	mcr	p15, op1, reg, cr1, cr2, op2

#define	LOAD_CP11(reg, cr1, num)		\
	ldc	p11, cr1, [reg], {num * 2}
#define	STORE_CP11(reg, cr1, num)		\
	stc	p11, cr1, [reg], {num * 2}

/* Read/Write CP10, CP15 register with condition field. */
#define	READ_CP10_COND(cond, op1, cr1, cr2, op2, reg)		\
	ARM_INST(mrc, cond)	p10, op1, reg, cr1, cr2, op2
#define	READ_CP15_COND(cond, op1, cr1, cr2, op2, reg)		\
	ARM_INST(mrc, cond)	p15, op1, reg, cr1, cr2, op2

#define	WRITE_CP10_COND(cond, op1, cr1, cr2, op2, reg)		\
	ARM_INST(mcr, cond)	p10, op1, reg, cr1, cr2, op2
#define	WRITE_CP15_COND(cond, op1, cr1, cr2, op2, reg)		\
	ARM_INST(mcr, cond)	p15, op1, reg, cr1, cr2, op2

/*
 * The following macros requires zero as register value.
 */
#define	MEMORY_BARRIER(reg)	WRITE_CP15(0, c7, c10, 5, reg)
#define	SYNC_BARRIER(reg)	WRITE_CP15(0, c7, c10, 4, reg)
#define	ICACHE_INV_ALL(reg)	WRITE_CP15(0, c7, c5, 0, reg)
#define	DCACHE_INV_ALL(reg)	WRITE_CP15(0, c7, c6, 0, reg)
#define	DCACHE_CLEAN_ALL(reg)	WRITE_CP15(0, c7, c10, 0, reg)
#define	DCACHE_FLUSH_ALL(reg)	WRITE_CP15(0, c7, c14, 0, reg)
#define	IDCACHE_INV_ALL(reg)	WRITE_CP15(0, c7, c7, 0, reg)
#define	BTC_FLUSH_ALL(reg)	WRITE_CP15(0, c7, c5, 6, reg)

/* Read Hardware Processor ID */
#define	HARD_PROCESSOR_ID(reg)						\
	READ_CP15(0, c0, c0, 5, reg);					\
	and	reg, reg, #0xf

/* Read CPSR */
#define	READ_CPSR(reg)		mrs	reg, cpsr

#define	DISABLE_IRQ_SAVE(reg)						\
	READ_CPSR(reg);							\
	cpsid	i

#define	DISABLE_INTR_SAVE(reg)						\
	READ_CPSR(reg);							\
	cpsid	if

#define	RESTORE_INTR(reg)	msr	cpsr_c, reg

/* Control Register */
#define	CTRL_READ(reg)		READ_CP15(0, c1, c0, 0, reg)
#define	CTRL_WRITE(reg)		WRITE_CP15(0, c1, c0, 0, reg)

/* Auxiliary Control Register */
#define	AUXCTRL_READ(reg)	READ_CP15(0, c1, c0, 1, reg)
#define	AUXCTRL_WRITE(reg)	WRITE_CP15(0, c1, c0, 1, reg)

/* VFP Register */
#define	READ_FPSCR(reg)		READ_CP10(7, c1, c0, 0, reg)
#define	READ_FPEXC(cond, reg)	READ_CP10_COND(cond, 7, c8, c0, 0, reg)
#define	READ_FPINST(cond, reg)	READ_CP10_COND(cond, 7, c9, c0, 0, reg)
#define	READ_FPINST2(cond, reg)	READ_CP10_COND(cond, 7, c10, c0, 0, reg)
#define	WRITE_FPEXC(cond, reg)	WRITE_CP10_COND(cond, 7, c8, c0, 0, reg)

#else	/* !_ASM */

#include <sys/types.h>

#ifdef	__GNUC__

/* Read/Write CP10, CP14, CP15 register */
#ifdef	__STDC__

#define	READ_CP10(op1, cr1, cr2, op2)					\
	({								\
		uint32_t	value;					\
		__asm__ __volatile__("mrc  p10," #op1 ", %0, " #cr1	\
				     ", " #cr2 ", " #op2		\
				     : "=r"(value));			\
		value;							\
	})

#define	READ_CP14(op1, cr1, cr2, op2)					\
	({								\
		uint32_t	value;					\
		__asm__ __volatile__("mrc  p14," #op1 ", %0, " #cr1	\
				     ", " #cr2 ", " #op2		\
				     : "=r"(value));			\
		value;							\
	})

#define	READ_CP15(op1, cr1, cr2, op2)					\
	({								\
		uint32_t	value;					\
		__asm__ __volatile__("mrc  p15," #op1 ", %0, " #cr1	\
				     ", " #cr2 ", " #op2		\
				     : "=r"(value));			\
		value;							\
	})

#define	WRITE_CP10(op1, cr1, cr2, op2, value)			\
	__asm__ __volatile__("mcr  p10, " #op1 ", %0, " #cr1	\
			     ", " #cr2 ", " #op2		\
			     : : "r"(value))

#define	WRITE_CP14(op1, cr1, cr2, op2, value)			\
	__asm__ __volatile__("mcr  p14, " #op1 ", %0, " #cr1	\
			     ", " #cr2 ", " #op2		\
			     : : "r"(value))

#define	WRITE_CP15(op1, cr1, cr2, op2, value)			\
	__asm__ __volatile__("mcr  p15, " #op1 ", %0, " #cr1	\
			     ", " #cr2 ", " #op2		\
			     : : "r"(value))

#define	LOAD_CP11(ptr, cr1, num)				\
	__asm__ __volatile__("ldc  p11," #cr1 ", [%0], {%1}"	\
				:: "r"(ptr), "i"((num) * 2))

#define	STORE_CP11(ptr, cr1, num)				\
	__asm__ __volatile__("stc  p11," #cr1 ", [%0], {%1}"	\
				:: "r"(ptr), "i"((num) * 2));	\

#else	/* !__STDC__ */

#define	READ_CP10(op1, cr1, cr2, op2)					\
	({								\
		uint32_t	value;					\
		__asm__ __volatile__("mrc  p10, op1, %0, cr1, cr2, op2"	\
				     : "=r"(value));			\
		value;							\
	})

#define	READ_CP14(op1, cr1, cr2, op2)					\
	({								\
		uint32_t	value;					\
		__asm__ __volatile__("mrc  p14, op1, %0, cr1, cr2, op2"	\
				     : "=r"(value));			\
		value;							\
	})

#define	READ_CP15(op1, cr1, cr2, op2)					\
	({								\
		uint32_t	value;					\
		__asm__ __volatile__("mrc  p15, op1, %0, cr1, cr2, op2"	\
				     : "=r"(value));			\
		value;							\
	})

#define	WRITE_CP10(op1, cr1, cr2, op2, value)			\
	__asm__ __volatile__("mcr  p10, op1, %0, cr1, cr2, op2"	\
			     : : "r"(value))

#define	WRITE_CP14(op1, cr1, cr2, op2, value)			\
	__asm__ __volatile__("mcr  p14, op1, %0, cr1, cr2, op2"	\
			     : : "r"(value))

#define	WRITE_CP15(op1, cr1, cr2, op2, value)			\
	__asm__ __volatile__("mcr  p15, op1, %0, cr1, cr2, op2"	\
			     : : "r"(value))

#define	LOAD_CP11(ptr, cr1, num)				\
	__asm__ __volatile__("ldc  p11, cr1, [%0], {%1}"	\
				:: "r"(ptr), "i"((num) * 2))

#define	STORE_CP11(ptr, cr1, num)				\
	__asm__ __volatile__("stc  p11, cr1, [%0], {%1}"	\
				:: "r"(ptr), "i"((num) * 2));	\

#endif	/* __STDC__ */

/* Memory Barrier */
#define	MEMORY_BARRIER()	WRITE_CP15(0, c7, c10, 5, 0)

/* Data Synchronization Barrier, aka Drain Write Buffer. */
#define	SYNC_BARRIER()		WRITE_CP15(0, c7, c10, 4, 0)

#ifdef	_KERNEL

/*
 * Cache Operations
 *   INV:	Invalidate cache line
 *   CLEAN:	Clean cache line
 *   FLUSH:	Clean and invalidate cache line
 */

/* Cache operations (entire cache) */
#define	ICACHE_INV_ALL()	WRITE_CP15(0, c7, c5, 0, 0)
#define	DCACHE_INV_ALL()	WRITE_CP15(0, c7, c6, 0, 0)
#define	DCACHE_CLEAN_ALL()	WRITE_CP15(0, c7, c10, 0, 0)
#define	DCACHE_FLUSH_ALL()	WRITE_CP15(0, c7, c14, 0, 0)
#define	IDCACHE_INV_ALL()	WRITE_CP15(0, c7, c7, 0, 0)

/* Flush branch target cache */
#define	BTC_FLUSH_ALL()		WRITE_CP15(0, c7, c5, 6, 0)

/* Cache operations using virtual address */
#define	DCACHE_CLEAN_VADDR(vaddr)	WRITE_CP15(0, c7, c10, 1, vaddr)
#define	DCACHE_FLUSH_VADDR(vaddr)	WRITE_CP15(0, c7, c14, 1, vaddr)

/* Read Hardware Processor ID */
#define	HARD_PROCESSOR_ID()					\
	({							\
		uint32_t	id = READ_CP15(0, c0, c0, 5);	\
		id = ARM_CPUID_ID(id);				\
	})

/*
 * Disable/Enable IRQ interrupt.
 *
 * Remarks:
 *	This macro never touches FIQ mask.
 */
#define	DISABLE_IRQ()	__asm__ __volatile__("cpsid i" : : : "memory")
#define	ENABLE_IRQ()	__asm__ __volatile__("cpsie i" : : : "memory")

/*
 * Disable/Enable FIQ.
 *
 * Remarks:
 *	This macro never touches IRQ mask.
 */
#define	DISABLE_FIQ()	__asm__ __volatile__("cpsid f" : : : "memory")
#define	ENABLE_FIQ()	__asm__ __volatile__("cpsie f" : : : "memory")

/*
 * Disable/Enable IRQ and FIQ.
 */
#define	DISABLE_INTR()	__asm__ __volatile__("cpsid if" : : : "memory")
#define	ENABLE_INTR()	__asm__ __volatile__("cpsie if" : : : "memory")

/*
 * Read CPSR value.
 */
#define	READ_CPSR()					\
	({						\
		uint32_t	value;			\
		__asm__ __volatile__("mrs   %0, cpsr"	\
				     : "=r"(value));	\
		value;					\
	})

/*
 * Disable/Enable IRQ and FIQ saving current CPSR value.
 */
#define	DISABLE_INTR_SAVE()			\
	({					\
		uint32_t	cpsr;		\
		cpsr = READ_CPSR();		\
		DISABLE_INTR();			\
		cpsr;				\
	})

#define	DISABLE_IRQ_SAVE()			\
	({					\
		uint32_t	cpsr;		\
		cpsr = READ_CPSR();		\
		DISABLE_IRQ();			\
		cpsr;				\
	})

#define	DISABLE_FIQ_SAVE()			\
	({					\
		uint32_t	cpsr;		\
		cpsr = READ_CPSR();		\
		DISABLE_FIQ();			\
		cpsr;				\
	})

#define	RESTORE_INTR(cpsr)					\
	__asm__ __volatile__ ("msr  cpsr_c, %0" : : "r"(cpsr))

/*
 * Macros used to translate virtual address into physical address.
 *
 * Remarks:
 *	You should disable IRQs before you use these macros.
 */

/* Kernel space read */
#define	VTOP_SET_VADDR_READ(vaddr)	WRITE_CP15(0, c7, c8, 0, vaddr)

/* Kernel space write */
#define	VTOP_SET_VADDR_WRITE(vaddr)	WRITE_CP15(0, c7, c8, 1, vaddr)

/* User space read */
#define	VTOP_SET_UVADDR_READ(vaddr)	WRITE_CP15(0, c7, c8, 2, vaddr)

/* User space write */
#define	VTOP_SET_UVADDR_WRITE(vaddr)	WRITE_CP15(0, c7, c8, 3, vaddr)

/* Read physical address */
#define	VTOP_READ_PADDR()		READ_CP15(0, c7, c4, 0)

/* Macros to check results of VTOP_READ_PADDR() */
#define	VTOP_ERROR(pa)		((pa) & VTOP_PA_ERROR)
#define	VTOP_PADDR(pa)		((pa) & VTOP_PA_PADDR_MASK)
#define	VTOP_PADDR_TYPE(pa)	((pa) & VTOP_PA_TYPE_MASK)

/*
 * Return physical address currently mapped to the specified virual address.
 * This macro uses VA to PA register instead of page table.
 * If write is true, check whether we can write to the given virtual address.
 * Otherwise check for read access.
 *
 * You can use VTOP_PADDR() macro to obtain physical address, and
 * VTOP_ERROR() to detect unresolved mapping.
 */
#define	VTOP_GET_PADDR(vaddr, write)				\
	({							\
		uint32_t	__x = DISABLE_IRQ_SAVE();	\
		uintptr_t	__paddr;			\
								\
		if (write) {					\
			VTOP_SET_VADDR_WRITE(vaddr);		\
		}						\
		else {						\
			VTOP_SET_VADDR_READ(vaddr);		\
		}						\
		__paddr = VTOP_READ_PADDR();			\
		RESTORE_INTR(__x);				\
		__paddr;					\
	})

/*
 * Macros for accessing VFP registers.
 */
#define	READ_FPSID()		READ_CP10(7,  c0, c0, 0)
#define	READ_FPSCR()		READ_CP10(7,  c1, c0, 0)
#define	READ_FPEXC()		READ_CP10(7,  c8, c0, 0)
#define	READ_FPINST()		READ_CP10(7,  c9, c0, 0)
#define	READ_FPINST2()		READ_CP10(7, c10, c0, 0)
#define	WRITE_FPSID(value)	WRITE_CP10(7,  c0, c0, 0, value)
#define	WRITE_FPSCR(value)	WRITE_CP10(7,  c1, c0, 0, value)
#define	WRITE_FPEXC(value)	WRITE_CP10(7,  c8, c0, 0, value)
#define	WRITE_FPINST(value)	WRITE_CP10(7,  c9, c0, 0, value)
#define	WRITE_FPINST2(value)	WRITE_CP10(7, c10, c0, 0, value)

/* 
 * Wait for interrupt
 */
#define	ARM_WFI()	__asm__ __volatile__("wfi")

#endif	/* _KERNEL */

#endif	/* __GNUC__ */

#endif	/* !_ASM */

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* !_ASM_CPUFUNC_H */
