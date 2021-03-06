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
 *
 * Copyright (c) 2002-2006 Neterion, Inc.
 */

#include "xge-os-pal.h"
#include "xgehal-mm.h"
#include "xge-debug.h"

/*
 * __hal_mempool_grow
 *
 * Will resize mempool up to %num_allocate value.
 */
xge_hal_status_e
__hal_mempool_grow(xge_hal_mempool_t *mempool, int num_allocate,
		int *num_allocated)
{
	int i, first_time = mempool->memblocks_allocated == 0 ? 1 : 0;
	int n_items = mempool->items_per_memblock;

	*num_allocated = 0;

	if ((mempool->memblocks_allocated + num_allocate) >
						mempool->memblocks_max) {
		xge_debug_mm(XGE_ERR, "%s",
			      "__hal_mempool_grow: can grow anymore");
		return XGE_HAL_ERR_OUT_OF_MEMORY;
	}

	for (i = mempool->memblocks_allocated;
	     i < mempool->memblocks_allocated + num_allocate; i++) {
		int j;
		int is_last =
			((mempool->memblocks_allocated+num_allocate-1) == i);
		xge_hal_mempool_dma_t *dma_object =
			mempool->memblocks_dma_arr + i;
		void *the_memblock;
		int dma_flags;

		dma_flags = XGE_OS_DMA_CACHELINE_ALIGNED;
#ifdef XGE_HAL_DMA_DTR_CONSISTENT
		dma_flags |= XGE_OS_DMA_CONSISTENT;
#else
		dma_flags |= XGE_OS_DMA_STREAMING;
#endif

		/* allocate DMA-capable memblock */
		mempool->memblocks_arr[i] = xge_os_dma_malloc(mempool->pdev,
					        mempool->memblock_size,
						dma_flags,
					        &dma_object->handle,
					        &dma_object->acc_handle);
		if (mempool->memblocks_arr[i] == NULL) {
			xge_debug_mm(XGE_ERR,
				      "memblock[%d]: out of DMA memory", i);
			return XGE_HAL_ERR_OUT_OF_MEMORY;
		}
		xge_os_memzero(mempool->memblocks_arr[i],
		mempool->memblock_size);
		the_memblock = mempool->memblocks_arr[i];

		/* allocate memblock's private part. Each DMA memblock
		 * has a space allocated for item's private usage upon
		 * mempool's user request. Each time mempool grows, it will
		 * allocate new memblock and its private part at once.
		 * This helps to minimize memory usage a lot. */
		mempool->memblocks_priv_arr[i] = xge_os_malloc(mempool->pdev,
					mempool->items_priv_size * n_items);
		if (mempool->memblocks_priv_arr[i] == NULL) {
			xge_os_dma_free(mempool->pdev,
				      the_memblock,
				      mempool->memblock_size,
				      &dma_object->acc_handle,
				      &dma_object->handle);
			xge_debug_mm(XGE_ERR,
			        "memblock_priv[%d]: out of virtual memory, "
			        "requested %d(%d:%d) bytes", i,
				mempool->items_priv_size * n_items,
				mempool->items_priv_size, n_items);
			return XGE_HAL_ERR_OUT_OF_MEMORY;
		}
		xge_os_memzero(mempool->memblocks_priv_arr[i],
			     mempool->items_priv_size * n_items);

		/* map memblock to physical memory */
		dma_object->addr = xge_os_dma_map(mempool->pdev,
		                                dma_object->handle,
						the_memblock,
						mempool->memblock_size,
						XGE_OS_DMA_DIR_BIDIRECTIONAL,
#ifdef XGE_HAL_DMA_DTR_CONSISTENT
					        XGE_OS_DMA_CONSISTENT
#else
					        XGE_OS_DMA_STREAMING
#endif
                                                );
		if (dma_object->addr == XGE_OS_INVALID_DMA_ADDR) {
			xge_os_free(mempool->pdev, mempool->memblocks_priv_arr[i],
				  mempool->items_priv_size *
					n_items);
			xge_os_dma_free(mempool->pdev,
				      the_memblock,
				      mempool->memblock_size,
				      &dma_object->acc_handle,
				      &dma_object->handle);
			return XGE_HAL_ERR_OUT_OF_MAPPING;
		}

		/* fill the items hash array */
		for (j=0; j<n_items; j++) {
			int index = i*n_items + j;

			if (first_time && index >= mempool->items_initial) {
				break;
			}

			mempool->items_arr[index] =
				((char *)the_memblock + j*mempool->item_size);

			/* let caller to do more job on each item */
			if (mempool->item_func_alloc != NULL) {
				xge_hal_status_e status;

				if ((status = mempool->item_func_alloc(
					mempool,
					the_memblock,
					i,
					dma_object,
					mempool->items_arr[index],
					index,
					is_last,
					mempool->userdata)) != XGE_HAL_OK) {

					if (mempool->item_func_free != NULL) {
						int k;

						for (k=0; k<j; k++) {

						    index =i*n_items + k;

						  (void)mempool->item_func_free(
						     mempool, the_memblock,
						     i, dma_object,
						     mempool->items_arr[index],
						     index, is_last,
						     mempool->userdata);
						}
					}

					xge_os_free(mempool->pdev,
					     mempool->memblocks_priv_arr[i],
					     mempool->items_priv_size *
					     n_items);
					xge_os_dma_unmap(mempool->pdev,
					     dma_object->handle,
					     dma_object->addr,
					     mempool->memblock_size,
					     XGE_OS_DMA_DIR_BIDIRECTIONAL);
					xge_os_dma_free(mempool->pdev,
					     the_memblock,
					     mempool->memblock_size,
					     &dma_object->acc_handle,
					     &dma_object->handle);
					return status;
				}
			}

			mempool->items_current = index + 1;
		}

		xge_debug_mm(XGE_TRACE,
			"memblock%d: allocated %dk, vaddr 0x"XGE_OS_LLXFMT", "
			"dma_addr 0x"XGE_OS_LLXFMT, i, mempool->memblock_size / 1024,
			(unsigned long long)(ulong_t)mempool->memblocks_arr[i],
			(unsigned long long)dma_object->addr);

		(*num_allocated)++;

		if (first_time && mempool->items_current ==
						mempool->items_initial) {
			break;
		}
	}

	/* increment actual number of allocated memblocks */
	mempool->memblocks_allocated += *num_allocated;

	return XGE_HAL_OK;
}

/*
 * xge_hal_mempool_create
 * @memblock_size:
 * @items_initial:
 * @items_max:
 * @item_size:
 * @item_func:
 *
 * This function will create memory pool object. Pool may grow but will
 * never shrink. Pool consists of number of dynamically allocated blocks
 * with size enough to hold %items_initial number of items. Memory is
 * DMA-able but client must map/unmap before interoperating with the device.
 * See also: xge_os_dma_map(), xge_hal_dma_unmap(), xge_hal_status_e{}.
 */
xge_hal_mempool_t*
__hal_mempool_create(pci_dev_h pdev, int memblock_size, int item_size,
		int items_priv_size, int items_initial, int items_max,
		xge_hal_mempool_item_f item_func_alloc,
		xge_hal_mempool_item_f item_func_free, void *userdata)
{
	xge_hal_status_e status;
	int memblocks_to_allocate;
	xge_hal_mempool_t *mempool;
	int allocated;

	if (memblock_size < item_size) {
		xge_debug_mm(XGE_ERR,
			"memblock_size %d < item_size %d: misconfiguration",
			memblock_size, item_size);
		return NULL;
	}

	mempool = (xge_hal_mempool_t *) \
			xge_os_malloc(pdev, sizeof(xge_hal_mempool_t));
	if (mempool == NULL) {
		xge_debug_mm(XGE_ERR, "mempool allocation failure");
		return NULL;
	}
	xge_os_memzero(mempool, sizeof(xge_hal_mempool_t));

	mempool->pdev			= pdev;
	mempool->memblock_size		= memblock_size;
	mempool->items_max		= items_max;
	mempool->items_initial		= items_initial;
	mempool->item_size		= item_size;
	mempool->items_priv_size	= items_priv_size;
	mempool->item_func_alloc	= item_func_alloc;
	mempool->item_func_free		= item_func_free;
	mempool->userdata		= userdata;

	mempool->memblocks_allocated = 0;

	mempool->items_per_memblock = memblock_size / item_size;

	mempool->memblocks_max = (items_max + mempool->items_per_memblock - 1) /
					mempool->items_per_memblock;

	/* allocate array of memblocks */
	mempool->memblocks_arr = (void ** ) xge_os_malloc(mempool->pdev,
					sizeof(void*) * mempool->memblocks_max);
	if (mempool->memblocks_arr == NULL) {
		xge_debug_mm(XGE_ERR, "memblocks_arr allocation failure");
		__hal_mempool_destroy(mempool);
		return NULL;
	}
	xge_os_memzero(mempool->memblocks_arr,
		    sizeof(void*) * mempool->memblocks_max);

	/* allocate array of private parts of items per memblocks */
	mempool->memblocks_priv_arr = (void **) xge_os_malloc(mempool->pdev,
					sizeof(void*) * mempool->memblocks_max);
	if (mempool->memblocks_priv_arr == NULL) {
		xge_debug_mm(XGE_ERR, "memblocks_priv_arr allocation failure");
		__hal_mempool_destroy(mempool);
		return NULL;
	}
	xge_os_memzero(mempool->memblocks_priv_arr,
		    sizeof(void*) * mempool->memblocks_max);

	/* allocate array of memblocks DMA objects */
	mempool->memblocks_dma_arr =
		(xge_hal_mempool_dma_t *) xge_os_malloc(mempool->pdev,
		sizeof(xge_hal_mempool_dma_t) * mempool->memblocks_max);

	if (mempool->memblocks_dma_arr == NULL) {
		xge_debug_mm(XGE_ERR, "memblocks_dma_arr allocation failure");
		__hal_mempool_destroy(mempool);
		return NULL;
	}
	xge_os_memzero(mempool->memblocks_dma_arr,
		     sizeof(xge_hal_mempool_dma_t) * mempool->memblocks_max);

	/* allocate hash array of items */
	mempool->items_arr = (void **) xge_os_malloc(mempool->pdev,
				 sizeof(void*) * mempool->items_max);
	if (mempool->items_arr == NULL) {
		xge_debug_mm(XGE_ERR, "items_arr allocation failure");
		__hal_mempool_destroy(mempool);
		return NULL;
	}
	xge_os_memzero(mempool->items_arr, sizeof(void *) * mempool->items_max);

	mempool->shadow_items_arr = (void **) xge_os_malloc(mempool->pdev,
                                sizeof(void*) *  mempool->items_max);
	if (mempool->shadow_items_arr == NULL) {
		xge_debug_mm(XGE_ERR, "shadow_items_arr allocation failure");
		__hal_mempool_destroy(mempool);
		return NULL;
	}
	xge_os_memzero(mempool->shadow_items_arr,
		     sizeof(void *) * mempool->items_max);

	/* calculate initial number of memblocks */
	memblocks_to_allocate = (mempool->items_initial +
				 mempool->items_per_memblock - 1) /
						mempool->items_per_memblock;

	xge_debug_mm(XGE_TRACE, "allocating %d memblocks, "
			"%d items per memblock", memblocks_to_allocate,
			mempool->items_per_memblock);

	/* pre-allocate the mempool */
	status = __hal_mempool_grow(mempool, memblocks_to_allocate, &allocated);
	xge_os_memcpy(mempool->shadow_items_arr, mempool->items_arr,
		    sizeof(void*) * mempool->items_max);
	if (status != XGE_HAL_OK) {
		xge_debug_mm(XGE_ERR, "mempool_grow failure");
		__hal_mempool_destroy(mempool);
		return NULL;
	}

	xge_debug_mm(XGE_TRACE,
		"total: allocated %dk of DMA-capable memory",
		mempool->memblock_size * allocated / 1024);

	return mempool;
}

/*
 * xge_hal_mempool_destroy
 */
void
__hal_mempool_destroy(xge_hal_mempool_t *mempool)
{
	int i, j;

	for (i=0; i<mempool->memblocks_allocated; i++) {
		xge_hal_mempool_dma_t *dma_object;

		xge_assert(mempool->memblocks_arr[i]);
		xge_assert(mempool->memblocks_dma_arr + i);

		dma_object = mempool->memblocks_dma_arr + i;

		for (j=0; j<mempool->items_per_memblock; j++) {
			int index = i*mempool->items_per_memblock + j;

			/* to skip last partially filled(if any) memblock */
			if (index >= mempool->items_current) {
				break;
			}

			/* let caller to do more job on each item */
			if (mempool->item_func_free != NULL) {

				mempool->item_func_free(mempool,
					mempool->memblocks_arr[i],
					i, dma_object,
					mempool->shadow_items_arr[index],
					index, /* unused */ -1,
					mempool->userdata);
			}
		}

		xge_os_dma_unmap(mempool->pdev,
	               dma_object->handle, dma_object->addr,
		       mempool->memblock_size, XGE_OS_DMA_DIR_BIDIRECTIONAL);

		xge_os_free(mempool->pdev, mempool->memblocks_priv_arr[i],
			mempool->items_priv_size * mempool->items_per_memblock);

		xge_os_dma_free(mempool->pdev, mempool->memblocks_arr[i],
			      mempool->memblock_size, &dma_object->acc_handle,
			      &dma_object->handle);
	}

	if (mempool->items_arr) {
		xge_os_free(mempool->pdev, mempool->items_arr, sizeof(void*) *
		          mempool->items_max);
	}

	if (mempool->shadow_items_arr) {
		xge_os_free(mempool->pdev, mempool->shadow_items_arr,
			  sizeof(void*) * mempool->items_max);
	}

	if (mempool->memblocks_dma_arr) {
		xge_os_free(mempool->pdev, mempool->memblocks_dma_arr,
		          sizeof(xge_hal_mempool_dma_t) *
			     mempool->memblocks_max);
	}

	if (mempool->memblocks_priv_arr) {
		xge_os_free(mempool->pdev, mempool->memblocks_priv_arr,
		          sizeof(void*) * mempool->memblocks_max);
	}

	if (mempool->memblocks_arr) {
		xge_os_free(mempool->pdev, mempool->memblocks_arr,
		          sizeof(void*) * mempool->memblocks_max);
	}

	xge_os_free(mempool->pdev, mempool, sizeof(xge_hal_mempool_t));
}

#ifdef XGEHAL_RNIC

/*
 * __hal_allocate_dma_register
 *
 * Will allocate dmable memory for register.
 */
xge_hal_status_e
__hal_allocate_dma_register(pci_dev_h pdev, int size,
		void **dma_register, xge_hal_mempool_dma_t *dma_object)
{
	int dma_flags;

	dma_flags = XGE_OS_DMA_CACHELINE_ALIGNED;
#ifdef XGE_HAL_DMA_DTR_CONSISTENT
	dma_flags |= XGE_OS_DMA_CONSISTENT;
#else
	dma_flags |= XGE_OS_DMA_STREAMING;
#endif

	xge_os_memzero(dma_object, sizeof(xge_hal_mempool_dma_t));

	/* allocate DMA-capable memblock */
	*dma_register = xge_os_dma_malloc(pdev,
					  size,
					  dma_flags,
					  &dma_object->handle,
					  &dma_object->acc_handle);
	if (*dma_register == NULL) {
		xge_debug_mm(XGE_ERR, "dma_register: out of DMA memory");
		return XGE_HAL_ERR_OUT_OF_MEMORY;
	}

	xge_os_memzero(*dma_register, size);

	/* map memblock to physical memory */
	dma_object->addr = xge_os_dma_map(pdev,
		                          dma_object->handle,
					  *dma_register,
					  size,
					  XGE_OS_DMA_DIR_BIDIRECTIONAL,
#ifdef XGE_HAL_DMA_DTR_CONSISTENT
					  XGE_OS_DMA_CONSISTENT
#else
					  XGE_OS_DMA_STREAMING
#endif
                                          );
	if (dma_object->addr == XGE_OS_INVALID_DMA_ADDR) {
		xge_os_dma_free(pdev,
				*dma_register,
				size,
				&dma_object->acc_handle,
				&dma_object->handle);
		return XGE_HAL_ERR_OUT_OF_MAPPING;
	}

	xge_debug_mm(XGE_TRACE,
		"dmareg: allocated %dk, vaddr 0x"XGE_OS_LLXFMT", "
		"dma_addr 0x"XGE_OS_LLXFMT, size / 1024,
		(unsigned long long)(ulong_t)*dma_register,
		(unsigned long long)dma_object->addr);


	return XGE_HAL_OK;
}

/*
 * __hal_free_dma_register
 */
void
__hal_free_dma_register(pci_dev_h pdev, int size,
			void *dma_register, xge_hal_mempool_dma_t *dma_object)

{

	xge_os_dma_unmap(pdev,
		dma_object->handle, dma_object->addr,
		size, XGE_OS_DMA_DIR_BIDIRECTIONAL);

	xge_os_dma_free(pdev, dma_register, size,
		&dma_object->acc_handle, &dma_object->handle);

}

#endif
