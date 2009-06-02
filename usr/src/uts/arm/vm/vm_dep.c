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

/* Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T */
/*	All Rights Reserved   */

/*
 * Copyright (c) 2006-2008 NEC Corporation
 */

#ident	"@(#)arm/vm/vm_dep.c"

/*
 * ARM dependent virtual memory support.
 */

#include <sys/types.h>
#include <sys/vmsystm.h>
#include <sys/cache_l220.h>
#include <sys/pte.h>
#include <sys/archsystm.h>
#include <sys/sysmacros.h>
#include <sys/mutex.h>
#include <sys/elf_ARM.h>
#include <sys/stack.h>
#include <vm/page.h>
#include <vm/as.h>
#include <vm/vm_dep.h>
#include <vm/hat_arm.h>
#include <vm/seg_vn.h>
#include <asm/tlb.h>

/* Internal prototypes */
static int	pp_finalize(void *addr, void *cpup, void *notused);

/*
 * int
 * valid_usr_range(caddr_t addr, size_t len, uint_t prot, struct as *as,
 *		   caddr_t userlimit)
 *	Determine whether [addr, addr+len] are valid user addresses.
 */
/*ARGSUSED*/
int
valid_usr_range(caddr_t addr, size_t len, uint_t prot, struct as *as,
		caddr_t userlimit)
{
	caddr_t eaddr = addr + len;

	if (eaddr <= addr || addr >= userlimit || eaddr > userlimit) {
		return RANGE_BADADDR;
	}

	return RANGE_OKAY;
}

int	valid_va_range_aligned_wraparound;

/*
 * int
 * valid_va_range_aligned(caddr_t *basep, size_t *lenp, size_t minlen, int dir,
 *			  size_t align, size_t redzone, size_t off)
 *	Determine whether [*basep, *basep + *lenp) contains a mappable range of
 *	addresses at least "minlen" long, where the base of the range is at
 *	"off" phase from an "align" boundary and there is space for a
 *	"redzone"-sized redzone on either side of the range.
 *
 * Calling/Exit State:
 *	On success, 1 is returned and *basep and *lenp are adjusted to
 *	describe the acceptable range (including the redzone).
 *	On failure, 0 is returned.
 */
 int
valid_va_range_aligned(caddr_t *basep, size_t *lenp, size_t minlen, int dir,
		       size_t align, size_t redzone, size_t off)
{
	uintptr_t	hi, lo;
	size_t		tot_len;

	ASSERT(align == 0 ? off == 0 : off < align);
	ASSERT(ISP2(align));
	ASSERT(align == 0 || align >= PAGESIZE);

	lo = (uintptr_t)*basep;
	hi = lo + *lenp;
	tot_len = minlen + 2 * redzone;	/* need at least this much space */

	/*
	 * If hi rolled over the top, try cutting back.
	 */
	if (hi < lo) {
		*lenp = 0UL - (uintptr_t)lo - 1UL;

		/* Trying to see if this really happens, and then if so, why */
		valid_va_range_aligned_wraparound++;
		hi = lo + *lenp;
	}
	if (*lenp < tot_len) {
                return (0);
        }
        if (hi - lo < tot_len) {
                return (0);
	}

	if (align > 1) {
		uintptr_t tlo = lo + redzone;
		uintptr_t thi = hi - redzone;

		tlo = (uintptr_t)P2PHASEUP(tlo, align, off);
		if (tlo < lo + redzone) {
			return (0);
		}
		if (thi < tlo || thi - tlo < minlen) {
			return (0);
		}
	}

	*basep = (caddr_t)lo;
	*lenp = hi - lo;
	return (1);
}

/*
 * int
 * valid_va_range(caddr_t *basep, size_t *lenp, size_t minlen, int dir)
 *		   caddr_t userlimit)
 *	Determine whether [*basep, *basep + *lenp) contains a mappable range of
 *	addresses at least "minlen" long.
 *
 * Calling/Exit State:
 *	On success, 1 is returned and *basep and *lenp are adjusted to
 *	describe the acceptable range.
 *	On failure, 0 is returned.
 */
/*ARGSUSED3*/
int
valid_va_range(caddr_t *basep, size_t *lenp, size_t minlen, int dir)
{
	return valid_va_range_aligned(basep, lenp, minlen, dir, 0, 0, 0);
}

/*
 * faultcode_t
 * pagefault(caddr_t addr, enum fault_type type, enum seg_rw rw,
 *           int iskernel)
 *     Handle a pagefault.
 */
faultcode_t
pagefault(caddr_t addr, enum fault_type type, enum seg_rw rw, int iskernel)
{
	struct as	*as;
	struct proc	*p;
	faultcode_t	res;
	caddr_t		base;
	size_t		len;
	int		err;
	int		mapped_red;
	uintptr_t	ea;

	ASSERT_STACK_ALIGNED();

	mapped_red = segkp_map_red();

	if (iskernel) {
		as = &kas;
	} else {
		p = curproc;
		as = p->p_as;
	}

	/*
	 * Dispatch pagefault.
	 */
	res = as_fault(as->a_hat, as, addr, 1, type, rw);

	/*
	 * If this isn't a potential unmapped hole in the user's
	 * UNIX data or stack segments, just return status info.
	 */
	if (res != FC_NOMAP || iskernel) {
		goto out;
	}

	/*
	 * Check to see if we happened to faulted on a currently unmapped
	 * part of the UNIX data or stack segments.  If so, create a zfod
	 * mapping there and then try calling the fault routine again.
	 */
	base = p->p_brkbase;
	len = p->p_brksize;

	if (addr < base || addr >= base + len) {		/* data seg? */
		base = (caddr_t)p->p_usrstack - p->p_stksize;
		len = p->p_stksize;
		if (addr < base || addr >= p->p_usrstack) {	/* stack seg? */
			/* not in either UNIX data or stack segments */
			res = FC_NOMAP;
			goto out;
		}
	}

	/*
	 * the rest of this function implements a 3.X 4.X 5.X compatibility
	 * This code is probably not needed anymore
	 */
	if (p->p_model == DATAMODEL_ILP32) {

		/* expand the gap to the page boundaries on each side */
		ea = P2ROUNDUP((uintptr_t)base + len, MMU_PAGESIZE);
		base = (caddr_t)P2ALIGN((uintptr_t)base, MMU_PAGESIZE);
		len = ea - (uintptr_t)base;

		as_rangelock(as);
		if (as_gap(as, MMU_PAGESIZE, &base, &len, AH_CONTAIN, addr) ==
		    0) {
			err = as_map(as, base, len, segvn_create, zfod_argsp);
			as_rangeunlock(as);
			if (err) {
				res = FC_MAKE_ERR(err);
				goto out;
			}
		} else {
			/*
			 * This page is already mapped by another thread after
			 * we returned from as_fault() above.  We just fall
			 * through as_fault() below.
			 */
			as_rangeunlock(as);
		}

		res = as_fault(as->a_hat, as, addr, 1, F_INVAL, rw);
	}

out:
	if (mapped_red)
		segkp_unmap_red();

	return (res);
}

/*
 * void
 * map_addr(caddr_t *addrp, size_t len, offset_t off, int vacalign,
 *	    uint_t flags)
 *	Choose virtual address for the user process.
 */
void
map_addr(caddr_t *addrp, size_t len, offset_t off, int vacalign, uint_t flags)
{
	proc_t	*p = curproc;

	/*
	 * We can ignore _MAP_LOW32 because whole virtual space size
	 * on ARM architecture is 4G.
	 */
	map_addr_proc(addrp, len, off, vacalign, p->p_as->a_userlimit, p,
		      flags);
}

/*
 * Redzone size that located at each side of user segment.
 * Redzone is not mandatory, but it will be useful to detect unexpected
 * access across a segment boundary by badly-mannered program.
 * If you don't want redzone, define this as 0.
 */
#define	MAPADDR_REDZONE_SIZE	PAGESIZE

/*
 * void
 * map_addr_proc(caddr_t *addrp, size_t len, offset_t off, int vacalign,
 *		 caddr_t userlimit, proc_t *p, uint_t flags)
 *	Choose virtual address for the specified process.
 *	We will pick an address range which is the highest available below
 *	KERNELBASE.
 *
 * Calling/Exit State:
 *	The target address space should be writer locked by the caller.
 *	This function returns with the lock state unchanged.
 *
 * Description:
 *	addrp is a value/result parameter. On input it is a hint from the user
 *	to be used in a completely machine dependent fashion. We decide to
 *	completely ignore this hint.
 *	On output it is NULL if no address can be found in the specified
 *	processes address space or else an address that is currently not
 *	mapped for len bytes with a page of red zone on either side.
 *
 *	On MPCore, vacalign is not needed. It is for virtually indexed caches.
 */
void
map_addr_proc(caddr_t *addrp, size_t len, offset_t off, int vacalign,
	      caddr_t userlimit, proc_t *p, uint_t flags)
{
	struct as	*as = p->p_as;
	caddr_t	base;
	size_t	slen, align_amount;
	int	try = 0;
	uint_t	gapflag = AH_HI;
#ifdef	DEBUG
	size_t	olen;
#endif	/* DEBUG */

	ASSERT(userlimit == as->a_userlimit);

	/* At first, try to preserve virtual space for stack. */
	base = p->p_brkbase;
	slen = userlimit - base - MAXSSIZ;
	len = PAGE_ROUNDUP(len);
#ifdef	DEBUG
	olen = len;
#endif	/* DEBUG */

	/* Reserve size for redzone. */
	len += (MAPADDR_REDZONE_SIZE << 1);

	/* Figure out what the alignment should be. */
	if (len <= ELF_ARM_MAXPGSZ) {
		/*
		 * Align virtual addresses to ensure that ELF shared libraries
		 * are mapped with the appropriate alignment constraints by
		 * the run-time linker.
		 */
		align_amount = ELF_ARM_MAXPGSZ;
	}
	else {
		uint_t	szc;
		size_t	pgsz;

		for (szc = mmu_exported_page_sizes - 1; szc >= 0; szc--) {
			pgsz = MMU_PAGESIZE << PAGE_BSZS_SHIFT(szc);

			if (len >= pgsz) {
				break;
			}
		}
		align_amount = MAX(pgsz, ELF_ARM_MAXPGSZ);
	}

	if ((flags & MAP_ALIGN) && ((uintptr_t)*addrp > align_amount)) {
		/*
		 * If MAP_ALIGN is set, requested address alignment should
		 * be set in *addrp.
		 */
		align_amount = (uintptr_t)*addrp;

		ASSERT(ISP2(align_amount));
	}
	len += align_amount;

 tryagain:
	/*
	 * Look for a large enough hole starting below userlimit.
	 * After finding it, use the upper part.
	 */
	if (as_gap(as, len, &base, &slen, gapflag, NULL) == 0) {
		caddr_t	addr, as_addr;

		if (gapflag == AH_HI) {
			addr = base + slen - len + MAPADDR_REDZONE_SIZE;
		}
		else {
			ASSERT(gapflag == AH_LO);
			addr = base + MAPADDR_REDZONE_SIZE;
		}
		as_addr = addr;

		/*
		 * Round address DOWN to the alignment amount,
		 * add the offset, and if this address is less than
		 * the original address, add alignment amount.
		 */
		addr = (caddr_t)((uintptr_t)addr & (~(align_amount - 1)));
		addr += (uintptr_t)(off & (align_amount - 1));
		if (addr < as_addr) {
			addr += align_amount;
		}

		ASSERT(addr <= (as_addr + align_amount));
		ASSERT(((uintptr_t)addr & (align_amount - 1)) ==
		       ((uintptr_t)(off & (align_amount - 1))));
		ASSERT(addr >= base);
		ASSERT(addr + olen + MAPADDR_REDZONE_SIZE <= base + slen);
		*addrp = addr;
	}
	else if (try == 0) {
		/*
		 * Search hole from space preserved for stack, and try to
		 * allocate the lowest address from the space preserved
		 * for stack.
		 */
		base = userlimit - MAXSSIZ - MAPADDR_REDZONE_SIZE;
		slen = MAXSSIZ + MAPADDR_REDZONE_SIZE;
		gapflag = AH_LO;
		try = 1;
		goto tryagain;
	}
	else if (try == 1) {
		/*
		 * No virtual space is available enough to preserve stack
		 * space. Search whole virtual space.
		 */
		base = p->p_brkbase;
		slen = userlimit - base;
		gapflag = AH_HI;
		try = 2;
		goto tryagain;
	}
	else {
		/* No more virtual space. */
		*addrp = NULL;
	}
}

/*
 * int
 * map_addr_vacalign_check(caddr_t addr, u_offset_t off)
 *	Determine whether the specified address may cause cache alias.
 *
 *	On MPCore, this routine always returns 0, that is, it never
 *	causes cache alias.
 *
 * Remarks:
 *	Although primary instruction cache on MPCore is virtual indexed cache,
 *	it's harmless because instruction cache line never becomes dirty.
 *	But implementation requires to flush all instruction cache lines
 *	when text memory is modified.
 */
int
map_addr_vacalign_check(caddr_t addr, u_offset_t off)
{
	return 0;
}

/*
 * size_t
 * map_pgsz(int maptype, struct proc *p, caddr_t addr, size_t len, int memcntl)
 *	Suggest a page size to be used to map a segment of type maptype and
 *	length len.
 *
 *	Currently, this function always returns PAGESIZE because large
 *	Currently, this function always returns SZC_SMALL because large
 *	page is not supported.
 */
size_t
map_pgsz(int maptype, struct proc *p, caddr_t addr, size_t len, int memcntl)
{
	return PAGESIZE;
}

/*
 * uint_t
 * map_pgszcvec(caddr_t addr, size_t size, uintptr_t off, int flags, int type,
 *		int memcntl)
 *	Return a bit vector of large page size codes that can be used to map
 *	[addr, addr + len) region.
 *
 *	Currently, this function always returns SZC_SMALL because large
 *	page is not supported.
 */
uint_t
map_pgszcvec(caddr_t addr, size_t size, uintptr_t off, int flags, int type,
	     int memcntl)
{
	return SZC_SMALL;
}

/*
 * void
 * pagezero(page_t *pp, uint_t off, uint_t len)
 *	Zero the physical page from off to off + len.
 *
 * Calling/Exit State:
 *	pagezero() doesn't affect reference/modification bits in page
 *	structure. Hence it is up to the caller to update them.
 *
 * Remarks:
 *	pagezero() uses CPU->cpu_caddr1 that was preserved while startup.
 *	It assumes that no one uses either map at interrupt level,
 *	and no one sleeps with an active mapping there.
 */
void
pagezero(page_t *pp, uint_t off, uint_t len)
{
	caddr_t		addr;
	uintptr_t	start;
	l2pte_t		*ptep;
	cpu_t		*cp;
	kmutex_t	*mutex;

	ASSERT(PAGE_LOCKED(pp));
	ASSERT(len > 0 && len <= PAGESIZE);
	ASSERT(off <= PAGESIZE);
	ASSERT(off + len <= PAGESIZE);

	/* Disable preemption so that CPU can't change. */
	kpreempt_disable();
	cp = CPU_GLOBAL;
	addr = cp->cpu_caddr1;
	ptep = cp->cpu_caddr1pte;
	mutex = &(cp->cpu_ppaddr_mutex);
	mutex_enter(mutex);

	/* Map page to CPU private space. */
	hat_mempte_remap(pp->p_pagenum, addr, ptep,
			 PROT_READ|PROT_WRITE|HAT_STORECACHING_OK,
			 HAT_LOAD_NOCONSIST);

	/* Zero page. */
	start = (uintptr_t)addr + off;
	if ((start & (sizeof(int) - 1)) == 0 && (len & FB_BLOCKMASK) == 0) {
		fast_bzero((void *)start, len);
	}
	else {
		bzero((void *)start, len);
	}

	/* Unmap page. */
	hat_mempte_remap(PFN_INVALID, addr, ptep, 0, 0);

	mutex_exit(mutex);
	kpreempt_enable();
}

/*
 * int
 * ppcopy(page_t *frompp, page_t *topp)
 *	Copy the data from the physical page associated with frompp to
 *	that associated with topp.
 *
 * Calling/Exit State:
 *	ppcopy() always returns 1.
 *
 *	ppcopy() doesn't affect reference/modification bits in page structure.
 *	Hence it is up to the caller to update them.
 *
 * Remarks:
 *	ppcopy() uses CPU->cpu_caddr1 and CPU->cpu_caddr2 that was preserved
 *	while startup. It assumes that no one uses either map at interrupt
 *	level, and no one sleeps with an active mapping there.
 */
int
ppcopy(page_t *frompp, page_t *topp)
{
	caddr_t		fromaddr, toaddr;
	l2pte_t		*fromptep, *toptep;
	cpu_t		*cp;
	kmutex_t	*mutex;

	ASSERT(PAGE_LOCKED(frompp));
	ASSERT(PAGE_LOCKED(topp));

	/* Disable preemption so that CPU can't change. */
	kpreempt_disable();
	cp = CPU_GLOBAL;
	fromaddr = cp->cpu_caddr1;
	toaddr = cp->cpu_caddr2;
	fromptep = cp->cpu_caddr1pte;
	toptep = cp->cpu_caddr2pte;
	mutex = &(cp->cpu_ppaddr_mutex);
	mutex_enter(mutex);

	/* Map pages to CPU private space. */
	hat_mempte_remap(frompp->p_pagenum, fromaddr, fromptep,
			 PROT_READ|HAT_STORECACHING_OK, HAT_LOAD_NOCONSIST);
	hat_mempte_remap(topp->p_pagenum, toaddr, toptep,
			 PROT_READ|PROT_WRITE|HAT_STORECACHING_OK,
			 HAT_LOAD_NOCONSIST);

	/* Copy page. */
	fast_bcopy(fromaddr, toaddr, PAGESIZE);

	/* Unmap pages. */
	hat_mempte_remap(PFN_INVALID, fromaddr, fromptep, 0, 0);
	hat_mempte_remap(PFN_INVALID, toaddr, toptep, 0, 0);

	mutex_exit(mutex);
	kpreempt_enable();

	return 1;
}

/*
 * int
 * bp_color(struct buf *bp)
 *	Return page color to map the specified buffer.
 *
 *	On MPCore, this routine always returns 0, that is, it never
 *	causes cache alias.
 */
/*ARGSUSED*/
int
bp_color(struct buf *bp)
{
	return 0;
}

/*
 * void
 * pageout_init(void (*procedure)(), proc_t *pp, pri_t pri)
 *	Create and initialise pageout scanner thread.
 *	The thread has to start at procedure with process pp and priority pri.
 */
void
pageout_init(void (*procedure)(), proc_t *pp, pri_t pri)
{
	(void)thread_create(NULL, 0, procedure, NULL, 0, pp, TS_RUN, pri);
}

/*
 * size_t
 * exec_get_spslew(void)
 *	Return stack size used to avoid cache thrashing.
 *	On ARMv6 architecture, no need to randomize stack because
 *	ARMv6 L2 data cache is PIPT cache.
 */
size_t
exec_get_spslew(void)
{
	return (0);
}
