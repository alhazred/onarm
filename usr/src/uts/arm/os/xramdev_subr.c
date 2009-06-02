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
 * Copyright (c) 2008 NEC Corporation
 * All rights reserved.
 */

#ident	"@(#)arm/os/xramdev_subr.c"

/*
 * Subroutines for xramfs device management.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/xramdev_impl.h>
#include <sys/nvdram_impl.h>
#include <sys/sunddi.h>
#include <sys/dditypes.h>
#include <sys/dumphdr.h>
#include <vm/vm_dep.h>
#include <vm/seg_kmem.h>
#include <vm/hat.h>
#include <vm/hat_arm.h>

#ifdef	XRAMDEV_CONFIG
extern xdseg_t		xramdev_segs[];
extern const uint_t	xramdev_segcnt;
extern nvdseg_t		nvdram_segs[];
extern const uint_t	nvdram_segcnt;

extern const xmemdev_t	*xmemdev_root;

#define	XRAMDEV_DRIVER_NAME	"xramdev"

/* Internal prototypes */
static void	xramdev_impl_allocnode(dev_info_t *parent, const char *name,
				       int unit);
static size_t	xramdev_choose_pagesize(xdseg_t *xsp);

/*
 * void
 * xramdev_reserve_vaddr(void)
 *	Reserve virtual space to map xramfs devices.
 *
 * Calling/Exit State:
 *	xramdev_reserve_vaddr() allocates virtual space from ekernelheap
 *	to lower address direction. So the caller must initialize ekernelheap
 *	in advance.
 *
 *	xramdev_reserve_vaddr() updates ekernelheap.
 */
void
xramdev_impl_reserve_vaddr(void)
{
	int		i;
	uintptr_t	end = (uintptr_t)ekernelheap;

	if (xramdev_segcnt == 0) {
		/* Nothing to do. */
		return;
	}

	for (i = 0; i < xramdev_segcnt; i++) {
		xdseg_t		*xsp = &xramdev_segs[i];
		uintptr_t	vaddr, base, baseoff, voff;
		size_t		size, pgsz, pgoff, pgmask;

		base = mmu_ptob(xsp->xs_base);
		size = mmu_ptob(xsp->xs_count);
		pgsz = xramdev_choose_pagesize(xsp);
		vaddr = end - size;
		if (pgsz > MMU_PAGESIZE) {
			uintptr_t	pgoff, pgmask, baseoff, voff;

			/* Adjust virtual address alignment. */
			pgoff = pgsz - 1;
			pgmask = ~pgoff;
			baseoff = base & pgoff;

			voff = vaddr & pgoff;
			if (voff < baseoff) {
				vaddr -= pgsz;
			}
			vaddr = (vaddr & pgmask) | baseoff;
		}

		XRAMDEV_PRM_PRINTF("xramdev[%d]: vaddr=0x%lx, paddr=0x%lx\n",
				   i, vaddr, base);
		XRAMDEV_PRM_PRINTF("xramdev[%d]: size=0x%lx, pgsz=0x%lx\n",
				   i, size, pgsz);
		xsp->xs_vaddr = vaddr;

		end = vaddr;
	}

	/* Update ekernelheap. */
	ekernelheap = (caddr_t)(end - MMU_PAGESIZE);
}

/*
 * void
 * xramdev_impl_mapinit(void)
 *	Map xramdev device pages to virtual space reserved by
 *	xramdev_impl_reserve_vaddr().
 */
void
xramdev_impl_mapinit(void)
{
	int	i;
	hat_t	*khat = kas.a_hat;

	for (i = 0; i < xramdev_segcnt; i++) {
		xdseg_t	*xsp = &xramdev_segs[i];

		ASSERT(xsp->xs_vaddr != NULL);
		hat_devload(khat, (caddr_t)xsp->xs_vaddr,
			    mmu_ptob(xsp->xs_count), xsp->xs_base,
			    xsp->xs_attr, HAT_LOAD_LOCK|HAT_LOAD_NOCONSIST);
	}
}

/*
 * void
 * xramdev_impl_probe(dev_info_t *parent)
 *	Create device node for xramfs physical device and nvdram device.
 *	All nodes are put under the specified node.
 */
void
xramdev_impl_probe(dev_info_t *parent)
{
	int	i;

	for (i = 0; i < xramdev_segcnt; i++) {
		dev_info_t	*dip;
		int		err;
		struct regspec	reg;

		/* Allocate node for xramfs device. */
		xramdev_impl_allocnode(parent, XRAMDEV_DRIVER_NAME, i);
	}

	for (i = 0; i < nvdram_segcnt; i++) {
		dev_info_t	*dip;
		int		err;
		struct regspec	reg;

		/* Allocate node for nvdram device. */
		xramdev_impl_allocnode(parent, NVDRAM_DRIVER_NAME, i);
	}
}

/*
 * static void
 * xramdev_impl_allocnode(dev_info_t *parent, const char *name, int unit)
 *	Allocate a device node for the specified driver.
 *	The given unit number will be set in regspec_addr.
 */
static void
xramdev_impl_allocnode(dev_info_t *parent, const char *name, int unit)
{
	dev_info_t	*dip;
	int		err;
	struct regspec	reg;

	/* Allocate a node. */
	ndi_devi_alloc_sleep(parent, name, (pnode_t)DEVI_SID_NODEID, &dip);

	/*
	 * Set device unit number in regspec_addr.
	 */
	reg.regspec_bustype = 0;
	reg.regspec_addr = unit;
	reg.regspec_size = 0;
	err = ndi_prop_update_int_array(DDI_DEV_T_NONE, dip, "reg",
					(int *)&reg,
					sizeof(reg) / sizeof(int));
	ASSERT(err == 0);

	/* Bind driver. */
	err = ndi_devi_bind_driver(dip, 0);
	ASSERT(err == 0);
}

/*
 * void
 * xramdev_impl_dump(void)
 *	Dump xramfs device pages into the system dump.
 */
void
xramdev_impl_dump(void)
{
	int	i;

	for (i = 0; i < xramdev_segcnt; i++) {
		xdseg_t	*xsp = &xramdev_segs[i];

		if (xsp->xs_flags & XDSEG_SYSDUMP) {
			pfn_t	pfn;

			for (pfn = xsp->xs_base;
			     pfn < xsp->xs_base + xsp->xs_count; pfn++) {
				dump_page(pfn);
			}
		}
	}

	for (i = 0; i < nvdram_segcnt; i++) {
		nvdseg_t	*nsp = &nvdram_segs[i];

		if (nsp->ns_flags & NVDSEG_SYSDUMP) {
			pfn_t	pfn;

			for (pfn = nsp->ns_base;
			     pfn < nsp->ns_base + nsp->ns_count; pfn++) {
				dump_page(pfn);
			}
		}
	}
}

/*
 * boolean_t
 * xramdev_impl_mapattr(pfn_t pfn, uint_t *attrp)
 *	Determine mapping attribute used to map xramfs and nvdram device page.
 *	"attrp" must point HAT attributes passed to HAT interface,
 *	such as hat_memload().
 *
 * Calling/Exit State:
 *	If the specified pfn is a xramfs device page, xramdev_impl_mapattr()
 *	sets mapping attribute into *attrp, and returns B_TRUE.
 *	Otherwise it returns B_FALSE.
 */
boolean_t
xramdev_impl_mapattr(pfn_t pfn, uint_t *attrp)
{
	const xmemdev_t	*xmp;

	/*
	 * We must check xramdev_end_paddr first because we assume that
	 * the most part of pagable memory is located above xramfs and
	 * nvdram device.
	 */
	if (pfn >= mmu_btop(xramdev_end_paddr) ||
	    pfn < mmu_btop(xramdev_start_paddr)) {
		/* Out out device range. */
		return B_FALSE;
	}

	/* Search B-tree index of devices. */
	xmp = xmemdev_root;
	while (xmp != NULL) {
		pfn_t	base = xmp->xd_base;
		uint_t	attr, xattr, prot, chattr;

		if (pfn < base) {
			/* Search lower PFN nodes. */
			xmp = xmp->xd_lower;
			continue;
		}
		if (pfn >= base + xmp->xd_count) {
			/* Search higher PFN nodes. */
			xmp = xmp->xd_higher;
			continue;
		}

		/* Found. */
		attr = *attrp;
		xattr = xmp->xd_attr;
		prot = attr & HAT_PROT_MASK;
		chattr = xattr & ~HAT_PROT_MASK;

		ASSERT(!(attr & PROT_WRITE) || (xattr & PROT_WRITE));
		*attrp = prot | chattr;
		return B_TRUE;
	}

	panic("xmemdev index is corrupted");
	/* NOTREACHED */
}

/*
 * static size_t
 * xramdev_choose_pagesize(xdseg_t *xsp)
 *	Choose pagesize for xramfs device page mapping in kernel space.
 */
static size_t
xramdev_choose_pagesize(xdseg_t *xsp)
{
	pfn_t		spfn, epfn;
	pgcnt_t		count;
	uint_t		szc;

	spfn = xsp->xs_base;
	count = xsp->xs_count;
	epfn = spfn + count;

	/* Try to use larger page size as possible. */
	for (szc = SZC_SPSECTION; szc > 0; szc--) {
		const pgcnt_t	cnt = SZCPAGES(szc);

		if (count >= cnt &&
		    PFN_BASE(spfn + cnt - 1, szc) < PFN_BASE(epfn, szc)) {
			return mmu_ptob(cnt);
		}
	}

	/* Use normal pagesize. */
	return MMU_PAGESIZE;
}

#endif	/* XRAMDEV_CONFIG */

/*
 * The following functions are used by device driver.
 * So they must be defined even if XRAMDEV_CONFIG is not defined.
 */

/*
 * xdseg_t *
 * xramdev_impl_getseg(int unit)
 *	Derive xramfs device information.
 *
 * Calling/Exit State:
 *	Upon successful completion, xramdev_impl_getseg() returns a pointer to
 *	struct xdseg that contains device information corresponding to
 *	the specified unit. It returns NULL on failure.
 */
xdseg_t *
xramdev_impl_getseg(int unit)
{
#ifdef	XRAMDEV_CONFIG
	if (unit < 0 || unit >= xramdev_segcnt) {
		return NULL;
	}

	return &xramdev_segs[unit];
#else	/* !XRAMDEV_CONFIG */
	return NULL;
#endif	/* XRAMDEV_CONFIG */
}

/*
 * uint_t
 * xramdev_impl_segcount(void)
 *	Return number of xramfs devices.
 */
uint_t
xramdev_impl_segcount(void)
{
#ifdef	XRAMDEV_CONFIG
	return xramdev_segcnt;
#else	/* !XRAMDEV_CONFIG */
	return 0;
#endif	/* XRAMDEV_CONFIG */
}

/*
 * nvdseg_t *
 * nvdram_impl_getseg(int unit)
 *	Derive nvdram device information.
 *
 * Calling/Exit State:
 *	Upon successful completion, nvdram_impl_getseg() returns a pointer to
 *	struct nvdram that contains device information corresponding to
 *	the specified unit. It returns NULL on failure.
 */
nvdseg_t *
nvdram_impl_getseg(int unit)
{
#ifdef	XRAMDEV_CONFIG
	if (unit < 0 || unit >= nvdram_segcnt) {
		return NULL;
	}

	return &nvdram_segs[unit];
#else	/* !XRAMDEV_CONFIG */
	return NULL;
#endif	/* XRAMDEV_CONFIG */
}

/*
 * uint_t
 * nvdram_impl_segcount(void)
 *	Return number of nvdram devices.
 */
uint_t
nvdram_impl_segcount(void)
{
#ifdef	XRAMDEV_CONFIG
	return nvdram_segcnt;
#else	/* !XRAMDEV_CONFIG */
	return 0;
#endif	/* XRAMDEV_CONFIG */
}
