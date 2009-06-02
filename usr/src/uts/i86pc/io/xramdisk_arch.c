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

/*
 * XRAMFS-prototype:
 *  i86pc/io/xramdisk_arch.c
 *  architecture dependent memory driver for XRAMDISK.
 */

#pragma ident	"xramdisk_arch.c"

#if 0
#define XRAMDISK_TRACE
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/errno.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ddidmareq.h>
#include <sys/vnode.h>
#include <vm/page.h>
#include <vm/as.h>
#include <vm/hat.h>
#include <vm/seg_kmem.h>

#include <sys/xramdisk.h>

extern struct vnode kvp;

void *contig_alloc(size_t, ddi_dma_attr_t *, uintptr_t, int);

/*
 * contig_free is copied from i86pc/os/ddi_impl.c
 * should make it global(non-static)
 */
static void
contig_free(void *addr, size_t size)
{
	pgcnt_t	pgcnt = btopr(size);
	size_t	asize = pgcnt * PAGESIZE;
	caddr_t	a, ea;
	page_t	*pp;

	hat_unload(kas.a_hat, addr, asize, HAT_UNLOAD_UNLOCK);

	for (a = addr, ea = a + asize; a < ea; a += PAGESIZE) {
		pp = page_find(&kvp,
				(u_offset_t)(uintptr_t)a);
		if (!pp)
			panic("contig_free: contig pp not found");

		if (!page_tryupgrade(pp)) {
			page_unlock(pp);
			pp = page_lookup(&kvp,
				(u_offset_t)(uintptr_t)a, SE_EXCL);
			if (pp == NULL)
				panic("contig_free: page freed");
		}
		page_destroy(pp, 0);
	}

	page_unresv(pgcnt);
	vmem_free(heap_arena, addr, asize);
}


/* for test: allocate 16Mbytes (4k * 4kpages) as default. */
#define XRAMDISK_I86PC_DEFAULT_NUMPAGES	((pgcnt_t)4096)

static void xramdisk_arch_detach(xramdisk_t *xrdp);
static int xramdisk_arch_getapage(xramdisk_t *xrdp, pfn_t pfn, page_t **ppp);

/*
 * Attach volume memory.
 */
int
xramdisk_arch_attach(xramdisk_t *xrdp)
{
	caddr_t	vabase;
	pfn_t	phybase;
	size_t	asize;

	static ddi_dma_attr_t	mattr = {
		DMA_ATTR_V0,		/* version */
		0x0ULL,			/* addr_lo */
		0xffffffffffffffffULL,	/* addr_hi */
		0xffffffffffffffffULL,	/* count_max */
		0,			/* align: set after. */
		1,			/* burstsizes */
		1,			/* minxfer */
		0xffffffffULL,		/* maxxfer */
		0xffffffffULL,		/* seg */
		1,			/* sgllen: == 1, must be continuous. */
		1,			/* granular */
		0			/* flags */
	};

	mattr.dma_attr_align = PAGESIZE;

	/* Check page location and counts of input parameters. */
	if (xrdp->xrd_numpages == 0) {
		/* Set default size of a volume. */
		xrdp->xrd_numpages = XRAMDISK_I86PC_DEFAULT_NUMPAGES;
	}

	asize = (size_t)XRD_SIZE(xrdp);

	/* fixed image mapping is not supported yet for i86pc. */

	/* Allocate and map physical-continuous memory region. */
	vabase = contig_alloc(asize, &mattr, PAGESIZE, 0);
	if (vabase == NULL) {
		cmn_err(CE_WARN,
			"can't allocate continuous memory by contig_alloc."
			"size=%" PRIu64 "\n", (int64_t)asize);
		return (ENOMEM);
	}

	/* and get page number base. */
	phybase = hat_getpfnum(kas.a_hat, vabase);

	/* Make it as no-consist mapping. */
	hat_unload(kas.a_hat, vabase, asize, HAT_UNLOAD_UNLOCK);
	hat_devload(kas.a_hat, vabase, asize, phybase, PROT_READ|PROT_WRITE,
		    HAT_LOAD_LOCK|HAT_LOAD_NOCONSIST);

	/* Insert result info into xramdisk info. */
	xrdp->xrd_basepage = phybase;
	xrdp->xrd_kasaddr = vabase;
	xrdp->xrd_arch_detach = &xramdisk_arch_detach;
	xrdp->xrd_arch_getapage = &xramdisk_arch_getapage;

	return (0);
}

/*
 * Detach volume memory.
 */
static void
xramdisk_arch_detach(xramdisk_t *xrdp)
{
	/* NOTE: all pages must be share-locked and be in kvp. */
	contig_free(xrdp->xrd_kasaddr, XRD_SIZE(xrdp));
}


/*
 * Get page in the volume.
 * Returned page is SE_EXCL locked.
 */
static int
xramdisk_arch_getapage(xramdisk_t *xrdp, pfn_t pfn, page_t **ppp)
{
	page_t	*pp;

	/* get page. */
#ifdef XRAMDISK_TRACE
	printf("xramdisk_arch_getapage: %s: trying to get a page for pfn %u\n",
	       xrdp->xrd_name, (unsigned int)pfn);
#endif
	pfn += xrdp->xrd_basepage;

retry:
	pp = page_numtopp_nolock(pfn);
	if (pp != NULL) {
		if (!page_tryupgrade(pp)) {
			page_t *npp;
#ifdef XRAMDISK_TRACE
			printf("   cannot upgrade lock for page %p\n", pp);
#endif
			page_unlock(pp);
			npp = page_numtopp(pfn, SE_EXCL);
			if (npp != pp) {
				page_unlock(npp);
				goto retry;
			}
		}
	}

#ifdef XRAMDISK_TRACE
	printf("   got a page %p\n", pp);
#endif
	if (pp == NULL) {
		cmn_err(CE_CONT,
			"xramdisk_arch_getapage: %s: "
			"cannot get a page for pfn %u\n",
			xrdp->xrd_name, (unsigned int)pfn);
		return (ENXIO);
	}

	*ppp = pp;
	return (0);
}
