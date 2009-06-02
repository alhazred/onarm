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

#ident	"@(#)arm/vm/cachectl.c"

/*
 * ARM cache control utilities.
 *
 * Currently, this file support only MPCore:
 *	- Primary cache is Harvard cache.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/cachectl.h>
#include <sys/prom_debug.h>
#include <sys/cpuvar.h>
#include <sys/x_call.h>
#include <vm/hat_arm.h>

/* Cache Type Register format */
typedef union cache_type {
	struct {
#ifdef	_LITTLE_ENDIAN
		uint32_t	i_len:2,
				i_m:1,
				i_assoc:3,
				i_size:3,
				i_pad:2,
				i_p:1,
				d_len:2,
				d_m:1,
				d_assoc:3,
				d_size:3,
				d_pad:2,
				d_p:1,
				s:1,
				ctype:4,
				pad:3;
#else	/* !_LITTLE_ENDIAN */
		uint32_t	pad:3,
				ctype:4,
				s:1,
				d_p:1,
				d_pad:2,
				d_size:3,
				d_assoc:3,
				d_m:1,
				d_len:2,
				i_p:1,
				i_pad:2,
				i_size:3,
				i_assoc:3,
				i_m:1,
				i_len:2;
#endif	/* _LITTLE_ENDIAN */
	} ct_u;
	uint32_t	ct_value;
} cache_type_t;

/* Convert value embedded in Cache Type Register into actual value */
#define	CACHE_TYPE_LINESIZE(len)	(1U << ((len) + 3))
#define	CACHE_TYPE_MULTIPLIER(m)	((m) ? 3 : 2)
#define	CACHE_TYPE_SIZE(sz, mtp)	((mtp) << ((sz) + 8))

#define	ct_ctype	ct_u.ctype
#define	ct_s		ct_u.s
#define	ct_d_p		ct_u.d_p
#define	ct_d_size	ct_u.d_size
#define	ct_d_assoc	ct_u.d_assoc
#define	ct_d_m		ct_u.d_m
#define	ct_d_len	ct_u.d_len
#define	ct_i_p		ct_u.i_p
#define	ct_i_size	ct_u.i_size
#define	ct_i_assoc	ct_u.i_assoc
#define	ct_i_m		ct_u.i_m
#define	ct_i_len	ct_u.i_len

/* Read Cache Type Register */
#define	READ_CACHE_TYPE_REG()	READ_CP15(0, c0, c0, 1)

/* Primary cache attributes */
size_t	arm_picache_linesize;		/* I-cache linesize */
size_t	arm_picache_size;		/* I-cache size */
size_t	arm_pdcache_linesize;		/* D-cache linesize */
size_t	arm_pdcache_size;		/* D-cache size */

/*
 * void
 * cache_init(void)
 *	Initialize ARM cache functionarity.
 *
 * Remarks:
 *	This function treats only primary caches.
 *	If you want to initialize L220 L2 cache on MPCore,
 *	use cache_l220_init() instead.
 */
void
cache_init(void)
{
	cache_type_t	reg;

	/* Check primary cache attributes. */
	reg.ct_value = READ_CACHE_TYPE_REG();

	arm_picache_linesize = CACHE_TYPE_LINESIZE(reg.ct_i_len);
	arm_picache_size = CACHE_TYPE_SIZE(reg.ct_i_size,
					   CACHE_TYPE_MULTIPLIER(reg.ct_i_m));
	arm_pdcache_linesize = CACHE_TYPE_LINESIZE(reg.ct_d_len);
	arm_pdcache_size = CACHE_TYPE_SIZE(reg.ct_d_size,
					   CACHE_TYPE_MULTIPLIER(reg.ct_d_m));
	PRM_DEBUG(arm_picache_linesize);
	PRM_DEBUG(arm_picache_size);
	PRM_DEBUG(arm_pdcache_linesize);
	PRM_DEBUG(arm_pdcache_size);
}

/*
 * void
 * sync_icache(caddr_t addr, uint_t len)
 *	Synchronize modification to text.
 *
 *	MPCore has Harvard style primary cache, so we needs to writeback
 *	data cache, and invalidate instruction cache.
 *
 * Remarks:
 *	The caller must guarantee that the specified virtual space is valid.
 */
void
sync_icache(caddr_t addr, uint_t len)
{
	hat_xcall(&hat_kas, (hat_xcallfunc_t)local_sync_icache,
		  (void *)addr, (void *)len, NULL, NULL);
}

/*
 * int
 * local_sync_icache(uintptr_t vaddr, uint_t size, void *notused)
 *	Synchronize modification to text on the current CPU.
 *
 *	MPCore has Harvard style primary cache, so we needs to writeback
 *	data cache, and invalidate instruction cache.
 *
 *	This function will be called from cross call handler.
 */
int
local_sync_icache(uintptr_t vaddr, uint_t size, void *notused)
{
	uintptr_t	end;

	vaddr = DCACHE_ROUNDDOWN(vaddr);
	size = DCACHE_ROUNDUP(size);
	if (size >= arm_pdcache_size) {
		/* Sync entire cache. */
		DCACHE_CLEAN_ALL();
		ICACHE_INV_ALL();
		SYNC_BARRIER();
		return 0;
	}

	end = vaddr + size;
	for (; vaddr < end; vaddr += arm_pdcache_linesize) {
		/* Writeback data cache line. */
		DCACHE_CLEAN_VADDR(vaddr);
	}

	/*
	 * We need to invalidate all instruction cache lines because
	 * aliased line may exist in instruction cache.
	 */
	ICACHE_INV_ALL();

	/* Guarantee sync operation has done. */
	SYNC_BARRIER();

	return 0;
}

/*
 * int
 * sync_user_icache(struct as *as, caddr_t addr, uint_t size)
 *	Synchronize modification to text for user space.
 *
 *	MPCore has Harvard style primary cache, so we needs to writeback
 *	data cache, and invalidate instruction cache.
 */
int
sync_user_icache(struct as *as, caddr_t addr, uint_t size)
{
	cpuset_t cpuset;
	int      error = 0;

	/*
	 *  Now, we ignore appointed address and size.
	 *  We need to invalidate all instruction cache line.
	 *  because aliased line may exist in instruction cache.
	 *
	 *  When you check an appointed parameter, you had better
	 *  use as_segat(). 
	 *  If it was a wrong parameter, return EFAULT.
	 */

	/* set all CPUs */
	CPUSET_ALL(cpuset);
	
	kpreempt_disable();

	/* Every time, we send x_call to all CPUs */
	xc_call(NULL, NULL, NULL, X_CALL_HIPRI, cpuset,
		(xc_func_t)local_sync_user_icache);
	kpreempt_enable();

	return (error);
}

/*
 * int
 * local_sync_user_icache(struct as *as, uintptr_t vaddr, uint_t size)
 *	Synchronize modification to text for user space on the current CPU.
 *
 *	MPCore has Harvard style primary cache, so we needs to writeback
 *	data cache, and invalidate instruction cache.
 *
 *	This function will be called from cross call handler.
 */
int
local_sync_user_icache(struct as *as, uintptr_t vaddr, uint_t size)
{
	/* 
	 * Now, even if an address and size are apointed,
	 * we invalidate all instruction cache line. 
	 * because aliased line may exist in instruction cache.
	 */
	DCACHE_CLEAN_ALL();
	ICACHE_INV_ALL();
	SYNC_BARRIER();

	return 0;
}

/*
 * void
 * sync_data_memory(caddr_t addr, size_t len)
 *	Synchronize data memory modification.
 *
 *	This function cleans and invalidates data cache line.
 *
 * Remarks:
 *	The caller must guarantee that the specified virtual space is valid.
 */
void
sync_data_memory(caddr_t addr, size_t len)
{
	hat_xcall(&hat_kas, (hat_xcallfunc_t)local_sync_data_memory,
		  (void *)addr, (void *)len, NULL, NULL);
}

/*
 * int
 * local_sync_data_memory(uintptr_t vaddr, size_t size, void *notused)
 *	Synchronize data memory modification on the current CPU.
 *
 *	This function cleans and invalidates data cache line.
 *
 *	This function will be called from cross call handler.
 */
int
local_sync_data_memory(uintptr_t vaddr, size_t size, void *notused)
{
	uintptr_t	end;

	vaddr = DCACHE_ROUNDDOWN(vaddr);
	size = DCACHE_ROUNDUP(size);
	if (size >= arm_pdcache_size) {
		/* Flush entire data cache. */
		DCACHE_FLUSH_ALL();
		SYNC_BARRIER();
		return 0;
	}

	end = vaddr + size;
	for (; vaddr < end; vaddr += arm_pdcache_linesize) {
		/* Clean and invalidate data cache line. */
		DCACHE_FLUSH_VADDR(vaddr);
	}

	/* Guarantee sync operation has done. */
	SYNC_BARRIER();

	return 0;
}

/*
 * int
 * sync_user_dcache(struct as *as, caddr_t addr, size_t len)
 *	Synchronize data memory modification for user space on all CPUs.
 *
 *	This function cleans and invalidates data cache line.
 */
int
sync_user_dcache(struct as *as, caddr_t addr, size_t len)
{
	cpuset_t cpuset;
	int      error = 0;

	/*
	 *  Now, we ignore appointed address and size.
	 *  We clean and invalidate all data cache line.
	 *
	 *  When you check an appointed parameter, you had better
	 *  use as_segat(). 
	 *  If it was a wrong parameter, return EFAULT.
	 */

	/* set all CPUs */
	CPUSET_ALL(cpuset);

	kpreempt_disable();

	/* Every time, we send x_call to all CPUs */
	xc_call(NULL, NULL, NULL, X_CALL_HIPRI, cpuset,
		(xc_func_t)local_sync_user_dcache);
	kpreempt_enable();

	return (error);
}

/*
 * int
 * local_sync_user_dcache(struct as *as, uintptr_t vaddr, size_t size)
 *	Synchronize data memory modification for user space on the current CPU.
 *
 *	This function cleans and invalidates data cache line for user space.
 *
 *	This function will be called from cross call handler.
 */
int
local_sync_user_dcache(struct as *as, uintptr_t vaddr, size_t size)
{
	/* 
	 *  Now, even if an address and size are apointed,
	 *  we clean and invalidate all data cache line. 
	 */
	return (local_dcache_flushall(NULL, NULL, NULL));
}

/*
 * void
 * dcache_flushall(void)
 *	Flush all data cache lines.
 */
void
dcache_flushall(void)
{
	hat_xcall(&hat_kas, local_dcache_flushall, NULL, NULL, NULL, NULL);
}

/*
 * int
 * local_dcache_flushall(void *a1, void *a2, void *a3)
 *	Flush all data cache lines on the current CPU.
 *
 *	This function will be called from cross call handler.
 */
int
local_dcache_flushall(void *a1, void *a2, void *a3)
{
	DCACHE_FLUSH_ALL();
	SYNC_BARRIER();

	return 0;
}
