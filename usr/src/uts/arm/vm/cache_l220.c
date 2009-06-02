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

#ident	"@(#)arm/vm/cache_l220.c"

/*
 * MPCore L220 L2 cache management.
 *
 * This file must be linked to machine-specific kernel object because
 * it includes machine-specific header.
 *
 * Remarks:
 *	This file contains code to avoid the following L220 errata:
 *
 *	  425331:	Potential deadlock in certain system configuratinos
 *			if Write Buffer not empty when some maintenance
 *			operations are received
 *	  484863:	The Cache Sync operation does not guarantee that
 *			the Eviction Buffer is empty
 */

#ifndef	_MACHDEP
#error	cache_l220.c must be built under platform dependant environment.
#endif	/* !_MACHDEP */


#include <sys/types.h>
#include <sys/param.h>
#include <sys/cache_l220.h>
#include <sys/sysmacros.h>
#include <asm/cpufunc.h>
#include <sys/mpcore.h>
#include <sys/bootconf.h>
#include <vm/vm_dep.h>
#include <sys/platform.h>

#if	ARMPF_L220_EXIST != 0

/* Line size is fixed to 32byte, Unified cache. */
#define	L220_ROUNDUP(a)		P2ROUNDUP((uint32_t)(a), L220_LINESIZE)
#define	L220_ROUNDDOWN(a)	P2ALIGN((uint32_t)(a), L220_LINESIZE)

/* L220 Auxiliary Contorl Register format */
typedef union l220_auxctl {
	struct {
#ifdef	_LITTLE_ENDIAN
		uint32_t
			/* Cycles of latency for data RAM Reads */
			lat_data_read:3,
			/* Cycles of latency for data RAM Writes */
			lat_data_write:3,
			/* Cycles of latency for tag RAMs */
			lat_tag:3,
			/* Cycles of latency for dirty RAMs */
			lat_dirty:3,
			/* Exclusive cache operation */
			exclusive:1,
			/* Associativity */
			associativity:4,
			/* Way-size */
			way_size:3,
			/* Event Monitor bus enable */
			monitor:1,
			/* Parity enable */
			parity:1,
			/* Shared attribute override enable */
			shared_override:1,
			/* Force write allocate */
			write_allocate:2,
			/* Override security check (SB0) */
			security:1,
			/* Non-secure lockdown enable */
			non_secure_lockdown:1,
			/* Non-secure interrupt access control */
			non_secure_intr_ctl:1,
			/* Reserved */
			pad:4;
#else	/* !_LITTLE_ENDIAN */
		uint32_t
			/* Reserved */
			pad:4,
			/* Non-secure interrupt access control */
			non_secure_intr_ctl:1,
			/* Non-secure lockdown enable */
			non_secure_lockdown:1,
			/* Override security check (SB0) */
			security:1,
			/* Force write allocate */
			write_allocate:2,
			/* Shared attribute override enable */
			shared_override:1,
			/* Parity enable */
			parity:1,
			/* Event Monitor bus enable */
			monitor:1,
			/* Way-size */
			way_size:3,
			/* Associativity */
			associativity:4,
			/* Exclusive cache operation */
			exclusive:1,
			/* Cycles of latency for dirty RAMs */
			lat_dirty:3,
			/* Cycles of latency for tag RAMs */
			lat_tag:3,
			/* Cycles of latency for data RAM Writes */
			lat_data_write:3,
			/* Cycles of latency for data RAM Reads */
			lat_data_read:3;
#endif	/* _LITTLE_ENDIAN */
	} l_u;
	uint32_t	l_value;
} l220_auxctl_t;

#define	l_lat_data_read		l_u.lat_data_read
#define	l_lat_data_write	l_u.lat_data_write
#define	l_lat_tag		l_u.lat_tag
#define	l_lat_dirty		l_u.lat_dirty
#define	l_exclusive		l_u.exclusive
#define	l_associativity		l_u.associativity
#define	l_way_size		l_u.way_size
#define	l_monitor		l_u.monitor
#define	l_parity		l_u.parity
#define	l_shared_override	l_u.shared_override
#define	l_write_allocate	l_u.write_allocate
#define	l_security		l_u.security
#define	l_non_secure_lockdown	l_u.non_secure_lockdown
#define	l_non_secure_intr_ctl	l_u.non_secure_intr_ctl


/* L220 Cache ID Register format */
typedef union l220_id {
	struct {
#ifdef	_LITTLE_ENDIAN
		uint32_t	release:6,	/* RTL release */
				part:4,		/* Part Number */
				id:6,		/* CACHE ID */
				pad:8,		/* SBZ */
				impl:8;		/* Implementor */
#else	/* !_LITTLE_ENDIAN */
		uint32_t	impl:8,		/* Implementor */
				pad:8,		/* SBZ */
				id:6,		/* CACHE ID */
				part:4,		/* Part Number */
				release:6;	/* RTL release */
#endif	/* _LITTLE_ENDIAN */
	} li_u;
	uint32_t	li_value;
} l220_id_t;

#define	li_impl		li_u.impl
#define	li_id		li_u.id
#define	li_part		li_u.part
#define	li_release	li_u.release

/* L220 RTL release */
#define	L220_RTL_R1P0		0x1

/* Debug Control Register bits */
#define	DBGCTL_DCL		0x1	/* Disable cache linefill */
#define	DBGCTL_DWB		0x2	/* Disable write-back (force WT) */
#define	DBGCTL_SPNIDEN		0x4	/* Pollable read bit for SPNIDEN */

static int	l220_enabled;
static int	l220_wt;

static lock_t	l220_lock;

/*
 * Determine whether STREX instruction is used to avoid L220 erratum 425331.
 * If false, SWP instruction is used.
 */
static int	l220_write_strex;

/*
 * C bit value of Cache Sync register format, physical address, and
 * index way combination.
 */
#define	L220_C_BIT		0x1

#ifdef	__GNUC__
/*
 * CACHE_L220_WRITE(regaddr, value, active, use_strex)
 *	Write the given value to the L220 register.
 *
 *	As described the L220 erratum 425331, the following write accesses
 *	using normal STR instruction may cause deadlock:
 *
 *	  - write to Control Register
 *	  - Cache Sync operation
 *	  - Clean and/or Invalidate by PA operations
 *	  - Clean and/or Invalidate by Index/Way operations
 *
 *	The above accesses must be done using CACHE_L220_WRITE() macro.
 *	CACHE_L220_WRITE() issues write accesses using LDREX/STREX or SWP
 *	to avoid L220 peripheral port deadlock.
 *
 *	CACHE_L220_WRITE() takes the following arguments:
 *	  regaddr	L220 register address.
 *	  value		Value to be written to the specified register.
 *	  active	Active bits that indicates background operation
 *			is active.
 *	  use_strex	The caller must pass value of l220_write_strex.
 *			This argument may help compiler to optimize code.
 *
 *	CACHE_L220_WRITE() writes ("value" | "active") to the specified L220
 *	register. If "active" is not zero, it is treated as background
 *	operation. CACHE_L220_WRITE() waits for completion of background
 *	operation by polling active bits in the specified register.
 */
#define	CACHE_L220_WRITE(regaddr, value, active, use_strex)		\
	do {								\
		uint32_t	__tmp1, __tmp2, __v;			\
									\
		/* Set active bits into the given value. */		\
		__v = (value) | (active);				\
									\
		/*							\
		 * If use_strex is true, write the given value to the	\
		 * L220 register using lDREX/STREX.			\
		 * Note that we must NOT see the result of STREX	\
		 * because L220 peripheral port doesn't	have exclusive	\
		 * monitor. So STREX to the L220 register is always	\
		 * treated as normal STR.				\
		 *							\
		 * Otherwise, use SWP for write access to the L220	\
		 * register.						\
		 */							\
		__asm__ __volatile__					\
			("teq		%4, #0\n"			\
			 "ldrexne	%0, [%2]\n"			\
			 "strexne	%1, %3, [%2]\n"			\
			 "swpeq		%0, %3, [%2]"			\
			 : "=&r"(__tmp1), "=&r"(__tmp2)			\
			 : "r"(regaddr), "r"(__v), "r"(use_strex)	\
			 : "memory", "cc");				\
									\
		if (active != 0) {					\
			/*						\
			 * Spin here until all active bits are		\
			 * cleared.					\
			 */						\
			while ((readl(regaddr) & (active)) != 0);	\
		}							\
	} while (0)

/*
 * CACHE_L220_SYNC()
 *	Wait for completion of L220 operation.
 *
 *	To avoid L220 erratum 484863, we use dummy SWP instruction to cached
 *	memory instead of the Cache Sync operation. SWP	issues AXI locked
 *	transaction that drains all L220 buffers, including the Eviction
 *	Buffer.
 *
 * Remarks:
 *	Although ARM Errata Notice describes that this erratum can be avoided
 *	by the Cache Sync operation launched by SWP instruction, we should
 *	NOT use this workaround because we know another erratum that
 *	SWP instruction to the L220 register causes deadlock.
 */
#define	CACHE_L220_SYNC()				\
	do {						\
		uint32_t	__tmp1, __tmp2;		\
							\
		__asm__ __volatile__			\
			("swp	%0, %1, [%2]"		\
			 : "=&r"(__tmp1)		\
			 : "r"(0), "r"(&__tmp2)		\
			 : "memory");			\
	} while (0)
#else	/* !__GNUC__ */
#error	CACHE_L220_WRITE() and CACHE_L220_SYNC() must be written in \
	assembly language.
#endif	/* __GNUC__ */

#endif	/* ARMPF_L220_EXIST != 0 */

/*
 * void
 * cache_l220_init(void)
 *	Initialize L220 cache module.
 */
void
cache_l220_init(void)
{
#if	ARMPF_L220_EXIST != 0
	l220_auxctl_t	aux;
	l220_id_t	id;
	uint_t		val; 
	uint32_t	data;
	int		use_strex;


	/*
	 * Determine whether we should use LDREX/STREX for write access to
	 * the L220 registers.
	 * If the RTL release of L220 cache is r1p0, we use SWP instruction.
	 * Otherwise use LDREX/STREX.
	 */
	id.li_value = readl(MPCORE_L220_VADDR(ID));
	use_strex = l220_write_strex = (id.li_release != L220_RTL_R1P0);

	lock_init(&l220_lock);

	/* Disable L220. */
	CACHE_L220_WRITE(MPCORE_L220_VADDR(CTRL), 0, 0, use_strex);

	if (BOP_GETPROPLEN(bootops, "l220-disable") >= 0) {
		printf("L220 cache disabled\n");
		return;
	}
	if (BOP_GETPROPLEN(bootops, "l220-wt") >= 0) {
		printf("L220 is configured as write through cache.\n");
		l220_wt = 1;
	}

	l220_enabled = 1;

	/* Initialize Auxiliary Control Register. */
	aux.l_value = 0;
	aux.l_lat_data_read = L220_LAT_DATA_READ;
	aux.l_lat_data_write = L220_LAT_DATA_WRITE;
	aux.l_lat_tag = L220_LAT_TAG;
	aux.l_lat_dirty = L220_LAT_DIRTY;
	aux.l_exclusive = 0;
	aux.l_associativity = L220_ASSOCIATIVITY;
	aux.l_way_size = L220_WAY_PARAM;
	aux.l_monitor = 1;
	aux.l_parity = 1;
	aux.l_shared_override = 1;
	aux.l_write_allocate = 0;
	aux.l_security = 1;
	aux.l_non_secure_lockdown = 0;
	aux.l_non_secure_intr_ctl = 0;
	writel(aux.l_value, MPCORE_L220_VADDR(AUX_CTRL));

	/* Invalidate all cache ways. */
	CACHE_L220_WRITE(MPCORE_L220_VADDR(INV_WAY), 0, L220_WAY_BITS,
			 use_strex);

	if (l220_wt) {
		uint32_t	val;

		/* Configure L220 as write through cache. */
		val = readl(MPCORE_L220_VADDR(DEBUG));
		writel(val|DBGCTL_DWB, MPCORE_L220_VADDR(DEBUG));
	}

	/* Enable L220. */
	CACHE_L220_WRITE(MPCORE_L220_VADDR(CTRL), 1, 0, use_strex);
	SYNC_BARRIER();
#endif	/* ARMPF_L220_EXIST != 0 */
}

/*
 * void
 * cache_l220_flush(caddr_t *start, size_t size, l220_flushop_t op);
 *	Flush cache lines in L220 at the specified range.
 *
 *	cache_l220_flush() guarantees that it implies SYNC_BARRIER()
 *	even if L220 cache is disabled.
 */
void
cache_l220_flush(caddr_t vaddr, size_t size, l220_flushop_t op)
{
#if	ARMPF_L220_EXIST != 0
	uintptr_t	addr, last_vpage, paddr, regaddr;
	uint32_t	x;
	const int	use_strex = l220_write_strex;

	if (!l220_enabled || (l220_wt && op == L220_CLEAN)) {
		SYNC_BARRIER();
		return;
	}

	addr = L220_ROUNDDOWN(vaddr);

	last_vpage = (uintptr_t)-1;
	paddr = 0;

	/* op must be a L220 register offset. */
	regaddr = MPCORE_L220_VBASE + (uintptr_t)op;

	x = DISABLE_IRQ_SAVE();
	lock_set(&l220_lock);
	SYNC_BARRIER();

	while (addr < (uintptr_t)vaddr + size) {
		uintptr_t	vpage;

		vpage = P2ALIGN(addr, MMU_PAGESIZE);
		if (vpage != last_vpage) {
			/*
			 * Convert virtual address into physical address.
			 * We don't want to access page table because
			 * this function may be called from hat functions.
			 */
			paddr = VTOP_GET_PADDR(vpage, 0);
			if (VTOP_ERROR(paddr)) {
				/* Unmapped virtual space. Ignore. */
				addr = vpage + MMU_PAGESIZE;
				continue;
			}
			last_vpage = vpage;
			paddr = VTOP_PADDR(paddr) + (addr - vpage);
		}

		/* Do L220 cache operation. */
		CACHE_L220_WRITE(regaddr, paddr, L220_C_BIT, use_strex);

		addr += L220_LINESIZE;
		paddr += L220_LINESIZE;
	}

	/* Wait for completion of L220 operation. */
	CACHE_L220_SYNC();

	SYNC_BARRIER();
	lock_clear(&l220_lock);
	RESTORE_INTR(x);
#endif	/* ARMPF_L220_EXIST != 0 */
}
