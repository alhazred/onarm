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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2006-2008 NEC Corporation
 */

#ifndef	_SYS_ROOTNEX_H
#define	_SYS_ROOTNEX_H

#pragma ident	"@(#)rootnex.h	1.2	06/02/11 SMI"

/*
 * ARM platform root nexus implementation specific state
 */

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/param.h>
#include <sys/modctl.h>
#include <sys/sunddi.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* Use unsigned char as boolean type to reduce memory usage. */
typedef uint8_t		rootnex_bool_t;

#define	RB_TRUE		1
#define	RB_FALSE	0

/* size of buffer used for ctlop reportdev */
#define	REPORTDEV_BUFSIZE	1024

/* min and max interrupt vectors */
#define	VEC_MIN			1
#define	VEC_MAX			255

/* atomic increment/decrement to keep track of outstanding binds, etc */
#define	ROOTNEX_PROF_INC(addr)		atomic_inc_64(addr)
#define	ROOTNEX_PROF_DEC(addr)		atomic_add_64(addr, -1)

/* set in dmac_type to signify that this cookie uses the copy buffer */
#define	ROOTNEX_USES_COPYBUF		0x80000000

/*
 * The highest physical address for ARM platform.
 * rootnex for ARM platform treats physical address as 32 bit address.
 */
#define	ROOTNEX_MAX_PADDR	0xffffffffULL
#define	ROOTNEX_MAX_PFN		mmu_btop(ROOTNEX_MAX_PADDR)

/* address alignment for DMA handle kmem cache */
#define	ROOTNEX_BUF_ALIGN	64

/* address alignment for each struct in DMA handle. */
#define	ROOTNEX_STRUCT_ALIGN	8

/*
 * integer or boolean property name and value. A few static rootnex properties
 * are created during rootnex attach from an array of rootnex_intprop_t..
 */
typedef struct rootnex_intprop_s {
	char	*prop_name;
	int	prop_value;
} rootnex_intprop_t;

/*
 * sgl related information which is visible to rootnex_get_sgl(). Trying to
 * isolate get_sgl() as much as possible so it can be easily replaced.
 */
typedef struct rootnex_sglinfo_s {
	/*
	 * These are passed into rootnex_get_sgl().
	 *
	 * si_min_addr - the minimum physical address
	 * si_max_addr - the maximum physical address
	 * si_max_cookie_size - the maximum size of a physically contiguous
	 *    piece of memory that we can handle in a sgl.
	 * si_segmask - segment mask to determine if we cross a segment boundary
	 * si_max_pages - max number of pages this sgl could occupy (which
	 *    is also the maximum number of cookies we might see.
	 */
	uint32_t	si_min_addr;
	uint32_t	si_max_addr;
	uint32_t	si_max_cookie_size;
	uint32_t	si_segmask;
	uint_t		si_max_pages;

	/*
	 * these are returned by rootnex_get_sgl()
	 *
	 * si_copybuf_req - amount of copy buffer needed by the buffer.
	 * si_buf_offset - The initial offset into the first page of the buffer.
	 *    It's set in get sgl and used in the bind slow path to help
	 *    calculate the current page index & offset from the current offset
	 *    which is relative to the start of the buffer.
	 * si_asp - address space of buffer passed in.
	 * si_sgl_size - The actual number of cookies in the sgl. This does
	 *    not reflect and sharing that we might do on window boundaries.
	 */
	size_t		si_copybuf_req;
	off_t		si_buf_offset;
	struct as	*si_asp;
	uint_t		si_sgl_size;
} rootnex_sglinfo_t;

/*
 * When we have to use the copy buffer, we allocate one of these structures per
 * buffer page to track which pages need the copy buffer, what the kernel
 * virtual address is (which the device can't reach), and what the copy buffer
 * virtual address is (where the device dma's to/from). For 32-bit kernels,
 * since we can't use seg kpm, we also need to keep the page_t around and state
 * if we've currently mapped in the page into KVA space for buffers which don't
 * have kva already and when we have multiple windows because we used up all our
 * copy buffer space.
 */
typedef struct rootnex_pgmap_s {
	rootnex_bool_t	pm_uses_copybuf;
	rootnex_bool_t	pm_mapped;
	page_t		*pm_pp;
	caddr_t		pm_vaddr;
	caddr_t		pm_kaddr;
	caddr_t		pm_cbaddr;
} rootnex_pgmap_t;

/*
 * We only need to trim a buffer when we have multiple windows. Each window has
 * trim state. We might have trimmed the end of the previous window, leaving the
 * first cookie of this window trimmed[tr_trim_first] (which basically means we
 * won't start with a new cookie), or we might need to trim the end of the
 * current window [tr_trim_last] (which basically means we won't end with a
 * complete cookie). We keep the same state for the first & last cookie in a
 * window (a window can have one or more cookies). However, when we trim the
 * last cookie, we keep a pointer to the last cookie in the trim state since we
 * only need this info when we trim. The pointer to the first cookie in the
 * window is in the window state since we need to know what the first cookie in
 * the window is in various places.
 *
 * If we do trim a cookie, we save away the physical address and size of the
 * cookie so that we can over write the cookie when we switch windows (the
 * space for a cookie which is in two windows is shared between the windows.
 * We keep around the same information for the last page in a window.
 *
 * if we happened to trim on a page that uses the copy buffer, and that page
 * is also in the middle of a window boundary because we have filled up the
 * copy buffer, we need to remember the copy buffer address for both windows
 * since the same page will have different copy buffer addresses in the two
 * windows. We need to due the same for kaddr in the 32-bit kernel since we
 * have a limited kva space which we map to.
 */
typedef struct rootnex_trim_s {
	rootnex_bool_t		tr_trim_first;
	rootnex_bool_t		tr_trim_last;
	rootnex_bool_t		tr_first_copybuf_win;
	rootnex_bool_t		tr_last_copybuf_win;
	ddi_dma_cookie_t	*tr_last_cookie;
	uint32_t		tr_first_paddr;
	uint32_t		tr_last_paddr;
	size_t			tr_first_size;
	size_t			tr_last_size;

	uint_t			tr_first_pidx;
	uint_t			tr_last_pidx;
	caddr_t			tr_first_cbaddr;
	caddr_t			tr_last_cbaddr;
	caddr_t			tr_first_kaddr;
	caddr_t			tr_last_kaddr;
} rootnex_trim_t;

/*
 * per window state. A bound DMA handle can have multiple windows. Each window
 * will have the following state. We track if this window needs to sync,
 * the offset into the buffer where the window starts, the size of the window.
 * a pointer to the first cookie in the window, the number of cookies in the
 * window, and the trim state for the window. For the 32-bit kernel, we keep
 * track of if we need to remap the copy buffer when we switch to a this window
 */
typedef struct rootnex_window_s {
	uint_t			wd_cookie_cnt;
	off_t			wd_offset;
	size_t			wd_size;
	ddi_dma_cookie_t	*wd_first_cookie;
	rootnex_trim_t		wd_trim;
	rootnex_bool_t		wd_dosync;
	rootnex_bool_t		wd_remap_copybuf;
} rootnex_window_t;

/* per dma handle private state */
typedef struct rootnex_dma_s {
	/*
	 * Declare boolean members here to reduce memory usage.
	 */

	/*
	 * sgl related state used to build and describe the sgl.
	 *
	 * dp_partial_required - used in the bind slow path to identify if we
	 *    need to do a partial mapping or not.
	 * dp_trim_required - used in the bind slow path to identify if we
	 *    need to trim when switching to a new window. This should only be
	 *    set when partial is set.
	 * dp_granularity_power_2 - set in alloc handle and used in bind slow
	 *    path to determine if we & or % to calculate the trim.
	 */
	rootnex_bool_t		dp_partial_required;
	rootnex_bool_t		dp_trim_required;
	rootnex_bool_t		dp_granularity_power_2;

	/*
	 * Copy buffer related state
	 *
	 * dp_cb_remaping - status if this bind causes us to have to remap
	 *    the copybuf when switching to new windows. This is only used in
	 *    the 32-bit kernel since we use seg kpm in the 64-bit kernel for
	 *    this case.
	 */
	rootnex_bool_t		dp_cb_remaping;

	/*
	 * Window related state
	 *
	 * dp_need_to_free_window - determine whether dp_window needs to be
	 *    freed by kmem_free().
	 */
	rootnex_bool_t		dp_need_to_free_window;

	/*
	 * Driver state
	 *
	 * dp_inuse - only used to see if a driver is trying to bind to an
	 *    already bound dma handle.
	 */
	rootnex_bool_t		dp_inuse;

	/*
	 *
	 * Cookie related state
	 *
	 * dp_need_to_free_cookie - determine whether cookies need to be
	 *    freed by kmem_free().
	 */
	rootnex_bool_t		dp_need_to_free_cookie;

	rootnex_bool_t		_dp_pad;

	/*
	 * sgl related state used to build and describe the sgl.
	 *
	 * dp_dma - copy of dma "object" passed in during bind
	 * dp_maxxfer - trimmed dma_attr_maxxfer so that it is a whole
	 *    multiple of granularity
	 * dp_sglinfo - See rootnex_sglinfo_t above.
	 */
	uint32_t		dp_maxxfer;
	ddi_dma_obj_t		dp_dma;
	rootnex_sglinfo_t	dp_sglinfo;

	/*
	 * Copy buffer related state
	 *
	 * dp_copybuf_size - the actual size of the copy buffer that we are
	 *    using. This can be smaller that dp_copybuf_req, i.e. bind size >
	 *    max copy buffer size.
	 * dp_cbaddr - kernel address of copy buffer. Used to determine where
	 *    where to copy to/from.
	 * dp_cbsize - the "real" size returned from the copy buffer alloc.
	 *    Set in the copybuf alloc and used to free copybuf.
	 * dp_pgmap - page map used in sync to determine which pages in the
	 *    buffer use the copy buffer and what addresses to use to copy to/
	 *    from.
	 * dp_kva - kernel heap arena vmem space for mapping to buffers which
	 *    we don't have a kernel VA to bcopy to/from. This is only used in
	 *    the 32-bit kernel since we use seg kpm in the 64-bit kernel for
	 *    this case.
	 */
	size_t			dp_copybuf_size;
	caddr_t			dp_cbaddr;
	size_t			dp_cbsize;
	rootnex_pgmap_t		*dp_pgmap;
	caddr_t			dp_kva;

	/*
	 * window related state. The pointer to the window state array which may
	 * be a pointer into the pre allocated state, or we may have had to
	 * allocate the window array on the fly because it wouldn't fit. If
	 * we allocate it, we'll use dp_need_to_free_window and dp_window_size
	 * during cleanup. dp_current_win keeps track of the current window.
	 * dp_max_win is the maximum number of windows we could have.
	 */
	uint_t			dp_current_win;
	rootnex_window_t	*dp_window;
	uint_t			dp_window_size;
	uint_t			dp_max_win;

	/* dip of driver which "owns" handle. set to rdip in alloc_handle() */
	dev_info_t		*dp_dip;

	/*
	 * dp_mutex is only used to see if a driver is trying to bind to
	 * an already bound dma handle. dp_mutex only used for dp_inuse
	 */
	kmutex_t		dp_mutex;

	/*
	 * cookie related state. The pointer to the cookies (dp_cookies) may
	 * be a pointer into the pre allocated state, or we may have had to
	 * allocate the cookie array on the fly because it wouldn't fit. If
	 * we allocate it, we'll use dp_need_to_free_cookie and dp_cookie_size
	 * during cleanup. dp_current_cookie is only used in the obsoleted
	 * interfaces to determine when we've used up all the cookies in a
	 * window during nextseg()..
	 */
	size_t			dp_cookie_size;
	ddi_dma_cookie_t	*dp_cookies;
	uint_t			dp_current_cookie; /* for obsoleted I/Fs */

	/*
	 * pre allocated space for the bind state, allocated during alloc
	 * handle. For a lot of devices, this will save us from having to do
	 * kmem_alloc's during the bind most of the time. kmem_alloc's can be
	 * expensive on ARM when the cpu count goes up since xcalls are
	 * expensive on ARM.
	 */
	uchar_t			*dp_prealloc_buffer;
} rootnex_dma_t;

/*
 * profile/performance counters. Most things will be dtrace probes, but there
 * are a couple of things we want to keep track all the time. We track the
 * total number of active handles and binds (i.e. an alloc without a free or
 * a bind without an unbind) since rootnex attach. We also track the total
 * number of binds which have failed since rootnex attach.
 */
typedef enum {
	ROOTNEX_CNT_ACTIVE_HDLS = 0,
	ROOTNEX_CNT_ACTIVE_BINDS = 1,
	ROOTNEX_CNT_ALLOC_FAIL = 2,
	ROOTNEX_CNT_BIND_FAIL = 3,
	ROOTNEX_CNT_SYNC_FAIL = 4,
	ROOTNEX_CNT_GETWIN_FAIL = 5,

	/* This one must be last */
	ROOTNEX_CNT_LAST
} rootnex_cnt_t;

/*
 * global driver state.
 *   r_dmahdl_cache - dma_handle kmem_cache
 *   r_dvma_call_list_id - ddi_set_callback() id
 *   r_peekpoke_mutex - serialize peeks and pokes.
 *   r_dip - rootnex dip
 *   r_reserved_msg_printed - ctlops reserve message threshold
 *   r_counters - profile/performance counters
 */
typedef struct rootnex_state_s {
	uint_t			r_prealloc_size;
	kmem_cache_t		*r_dmahdl_cache;
	uintptr_t		r_dvma_call_list_id;
	kmutex_t		r_peekpoke_mutex;
	dev_info_t		*r_dip;
	ddi_iblock_cookie_t	r_err_ibc;
	boolean_t		r_reserved_msg_printed;
	uint64_t		r_counters[ROOTNEX_CNT_LAST];
} rootnex_state_t;

/* struct definition to allocate ddi_dma_impl_t and rootnex_dma_t at once. */
typedef struct rootnex_dmabuf {
	ddi_dma_impl_t		rd_handle;
	rootnex_dma_t		rd_dma;
} rootnex_dmabuf_t;

#define	ROOTNEX_DMABUF_TO_DMA(buf)			\
	(&(((rootnex_dmabuf_t *)(buf))->rd_dma))

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ROOTNEX_H */