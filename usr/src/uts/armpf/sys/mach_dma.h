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

#ifndef	_SYS_MACH_DMA_H
#define	_SYS_MACH_DMA_H

#ident	"@(#)armpf/sys/mach_dma.h"

/*
 * ARMPF-specific definitions for DMA.
 * Kernel build tree private.
 */

#include <sys/types.h>
#include <sys/vmem.h>
#include <vm/hat.h>
#include <vm/hat_arm.h>

/*
 * Declare new storage for ddi_dma_attr to update DMA attributes.
 * Do nothing on typical ARMPF implementation.
 */
#define	ARMPF_DMA_ATTR_DECL(attrbuf)

/*
 * Update ddi_dma_attr for DMA buffer allocation.
 * This macro may change attrp.
 *
 * Do nothing on typical ARMPF implementation.
 */
#define	ARMPF_DMA_ATTR_UPDATE(attrp, attrbuf, econtig_paddr)

/*
 * HAT flags used to map DMA buffer.
 */
#define	ARMPF_DMA_HAT_FLAGS	(HAT_NOSYNC|HAT_STRICTORDER|HAT_PLAT_NOCACHE)

/*
 * Determine whether DMA buffer sync is always required or not.
 */
#define	ARMPF_DMA_READ_SYNC_REQUIRED(handle)		(0)
#define	ARMPF_DMA_WRITE_SYNC_REQUIRED(handle)		(0)
#define	ARMPF_DMA_SYNC_REQUIRED(handle)			\
	(ARMPF_DMA_READ_SYNC_REQUIRED(handle) ||	\
	 ARMPF_DMA_WRITE_SYNC_REQUIRED(handle))

/*
 * Synchronize DMA handle.
 * Currently there is nothing to do.
 */
#define	ARMPF_DMA_SYNC_HANDLE(handle, off, len, type)
#define	ARMPF_DMA_COPYBUF_SYNC_INIT(handle)
#define	ARMPF_DMA_COPYBUF_SYNC_FINI(handle)

/* Prototypes */
extern void	*dma_page_create(vmem_t *vmp, size_t size, int vmflag);
extern void	dma_page_free(vmem_t *vmp, void *addr, size_t size);

#ifdef	_MACH_DMA_PLAT
/* Apply platform specific DMA definitions. */
#include <sys/mach_dma_plat.h>
#endif	/* _MACH_DMA_PLAT */

#endif	/* !_SYS_MACH_DMA_H */
