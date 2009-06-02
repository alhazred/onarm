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

#ident	"@(#)armpf/os/dma_subr.c"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/mman.h>
#include <sys/vmem.h>
#include <sys/cache_l220.h>
#include <sys/mach_dma.h>
#include <sys/vnode.h>
#include <vm/page.h>
#include <vm/seg_kmem.h>
#include <vm/hat_arm.h>
#include <vm/vm_dep.h>

/*
 * DMA buffer management for ARM architecture.
 *
 * Currently, ARM architecture has no mechanism to keep coherency between
 * CPU cache and DMA buffer. So we must use uncached mapping for DMA buffer
 * on ARM architecture.
 */

#ifndef	_MACH_DMA_PAGE_CREATE
/*
 * void *
 * dma_page_create(vmem_t *vmp, size_t size, int vmflags)
 *	Allocate pages for DMA buffer.
 *	This function creates uncached mapping.
 *
 * Remarks:
 *	dma_page_create() must return address that can be freed using
 *	segkmem_free().
 */
void *
dma_page_create(vmem_t *vmp, size_t size, int vmflag)
{
	caddr_t	addr;
	page_t	*ppl;
	pgcnt_t	npages = btopr(size);
	size_t	rsize = ptob(npages);
	int	pflags = PG_EXCL;

	if ((addr = vmem_alloc(vmp, rsize, vmflag)) == NULL) {
		return NULL;
	}

	ASSERT(((uintptr_t)addr & PAGEOFFSET) == 0);

	if (page_resv(npages, vmflag & VM_KMFLAGS) == 0) {
		vmem_free(vmp, addr, rsize);
		return NULL;
	}

	ppl = segkmem_page_create(addr, rsize, vmflag, NULL);
	if (ppl == NULL) {
		vmem_free(vmp, addr, rsize);
		page_unresv(npages);
		return NULL;
	}

	while (ppl != NULL) {
		page_t *pp = ppl;

		page_sub(&ppl, pp);
		ASSERT(page_iolock_assert(pp));
		ASSERT(PAGE_EXCL(pp));
		page_io_unlock(pp);
		hat_devload(kas.a_hat, (caddr_t)(uintptr_t)pp->p_offset,
			    PAGESIZE, pp->p_pagenum,
			    PROT_READ|PROT_WRITE|ARMPF_DMA_HAT_FLAGS,
			    HAT_LOAD_NOCONSIST|HAT_LOAD_LOCK);
		pp->p_lckcnt = 1;
		PP_ARM_SETDMA(pp);
		page_unlock(pp);
	}

	/* Invalidate cache lines. */
	sync_data_memory(addr, rsize);
	CACHE_L220_FLUSH(addr, rsize, L220_FLUSH);

	return addr;
}
#endif	/* !_MACH_DMA_PAGE_CREATE */

/*
 * void
 * dma_page_free(vmem_t *vmp, void *addr, size_t size)
 *	Release pages allocated by dma_page_create().
 */
void
dma_page_free(vmem_t *vmp, void *addr, size_t size)
{
	pgcnt_t	npages = btopr(size);
	size_t	rsize = ptob(npages);
	caddr_t	vaddr, eaddr;

	ASSERT(((uintptr_t)addr & PAGEOFFSET) == 0);
	ASSERT(vmp != NULL);

	hat_unload(kas.a_hat, addr, rsize, HAT_UNLOAD_UNLOCK);

	eaddr = (caddr_t)addr + rsize;
	for (vaddr = (caddr_t)addr; vaddr < eaddr; vaddr += PAGESIZE) {
		page_t	*pp;

		pp = page_lookup(&kvp, (u_offset_t)(uintptr_t)vaddr, SE_EXCL);
		if (pp == NULL) {
			panic("dma_page_free: page not found");
		}

		pp->p_lckcnt = 0;
		PP_ARM_CLRDMA(pp);
		page_destroy(pp, 0);
	}

	page_unresv(npages);
	vmem_free(vmp, addr, rsize);
}
