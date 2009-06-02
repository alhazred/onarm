/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2008 NEC Corporation
 * All rights reserved.
 */

/*
 * XRAMFS-prototype:
 *  arm/io/xramdisk_arch.c
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
#include <sys/kmem.h>
#include <vm/page.h>
#include <vm/as.h>
#include <vm/hat.h>
#include <vm/seg_kmem.h>

#include <sys/xramdisk.h>
#include <sys/archsystm.h>

extern struct vnode kvp;

#define XRAMDISK_ARM_DEFAULT_NUMPAGES	((pgcnt_t)4096)

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
		xrdp->xrd_numpages = XRAMDISK_ARM_DEFAULT_NUMPAGES;
	}

	asize = (size_t)XRD_SIZE(xrdp);

	/* Allocate and map physical-continuous memory region. */
	vabase = contig_alloc(asize, &mattr, PAGESIZE, PROT_READ|PROT_WRITE,
			      KM_NOSLEEP);
	if (vabase == NULL) {
		cmn_err(CE_WARN,
			"can't allocate continuous memory by contig_alloc."
			"size=%" PRIu64 "\n", (int64_t)asize);
		return (ENOMEM);
	}

	/* and get page number base. */
	phybase = hat_getpfnum(kas.a_hat, vabase);

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
	void *addr = xrdp->xrd_kasaddr;
	size_t size = XRD_SIZE(xrdp);
	pgcnt_t pgcnt = btopr(XRD_SIZE(xrdp));
	size_t asize = pgcnt * PAGESIZE;
	caddr_t a, ea;
	page_t *pp;

	for (a = addr, ea = a + size; a < ea; a += PAGESIZE) {
		pp = page_exists(&kvp, (u_offset_t)(uintptr_t)a);
		if (pp == NULL) {
			panic("xramdisk_arch_detach: pp not found");
		}
		if (PAGE_LOCKED(pp)) {
			page_unlock(pp);
		}
	}
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

	pp = page_numtopp(pfn, SE_EXCL);
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
