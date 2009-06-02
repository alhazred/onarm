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
/*
 * Copyright (c) 2006-2008 NEC Corporation
 */

#ident	"@(#)armpf/io/rootnex.c"

/*
 * ARMPF root nexus driver
 */

#include <sys/sysmacros.h>
#include <sys/conf.h>
#include <sys/autoconf.h>
#include <sys/sysmacros.h>
#include <sys/debug.h>
#include <sys/ddidmareq.h>
#include <sys/promif.h>
#include <sys/devops.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <vm/seg_dev.h>
#include <sys/vmem.h>
#include <sys/mman.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/page.h>
#include <sys/avintr.h>
#include <sys/errno.h>
#include <sys/modctl.h>
#include <sys/ddi_impldefs.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/mach_intr.h>
#include <sys/ontrap.h>
#include <sys/atomic.h>
#include <sys/sdt.h>
#include <sys/rootnex.h>
#include <vm/hat_arm.h>
#include <sys/ddifm.h>
#include <sys/gic.h>
#include <sys/platform.h>
#include <sys/archsystm.h>
#include <sys/mach_dma.h>

/*
 * Convert virtual address into physical address, and determine whether
 * we should use copy buffer.
 */
#define	ROOTNEX_VTOP(hat, vaddr, paddr, use_copybuf)			\
	{								\
		if ((hat) == &hat_kas) {				\
			uint32_t	pa;				\
									\
			pa = VTOP_GET_PADDR(vaddr, 0);			\
			ASSERT(!VTOP_ERROR(pa));			\
			(use_copybuf) =					\
				(VTOP_PADDR_TYPE(pa) == VTOP_PA_TYPE_NORMAL); \
			(paddr) = (uint32_t)VTOP_PADDR(pa);		\
		}							\
		else {							\
			pfn_t	pfn;					\
									\
			(use_copybuf) = 1;				\
			pfn = hat_getpfnum((hat), (vaddr));		\
			ASSERT(pfn != PFN_INVALID);			\
			ASSERT(pfn <= ROOTNEX_MAX_PFN);			\
			(paddr) = ptob(pfn);				\
		}							\
	}

/*
 * enable/disable extra checking of function parameters. Useful for debugging
 * drivers.
 */
#ifdef	DEBUG
int rootnex_alloc_check_parms = 1;
int rootnex_bind_check_parms = 1;
int rootnex_bind_check_inuse = 1;
int rootnex_unbind_verify_buffer = 0;
int rootnex_sync_check_parms = 1;
#else
#define	rootnex_alloc_check_parms	(0)
#define	rootnex_bind_check_parms	(0)
#define	rootnex_bind_check_inuse	(0)
#define	rootnex_unbind_verify_buffer	(0)
#define	rootnex_sync_check_parms	(0)
#endif

/* Master Abort and Target Abort panic flag */
int rootnex_fm_ma_ta_panic_flag = 0;

/* Semi-temporary patchables to phase in bug fixes, test drivers, etc. */
int rootnex_bind_fail = 1;

#ifdef	DEBUG
int rootnex_bind_warn = 1;
uint8_t *rootnex_warn_list;
/* bitmasks for rootnex_warn_list. Up to 8 different warnings with uint8_t */
#define	ROOTNEX_BIND_WARNING	(0x1 << 0)
#else	/* !DEBUG */
#define	rootnex_bind_warn	0
#endif	/* DEBUG */

/*
 * revert back to old broken behavior of always sync'ing entire copy buffer.
 * This is useful if be have a buggy driver which doesn't correctly pass in
 * the offset and size into ddi_dma_sync().
 */
#ifdef	ROOTNEX_SYNC_IGNORE_PARAMS
#undef	ROOTNEX_SYNC_IGNORE_PARAMS
#define	ROOTNEX_SYNC_IGNORE_PARAMS	1
#else	/* !ROOTNEX_SYNC_IGNORE_PARAMS */
#define	ROOTNEX_SYNC_IGNORE_PARAMS	0
#endif	/* ROOTNEX_SYNC_IGNORE_PARAMS */

int rootnex_sync_ignore_params = ROOTNEX_SYNC_IGNORE_PARAMS;

/*
 * maximum size that we will allow for a copy buffer.
 */
size_t rootnex_max_copybuf_size = ROOTNEX_MAX_COPYBUF_SIZE;

/*
 * Pre-allocation size is defined as tunable parameter.
 */
uint_t	rootnex_prealloc_cookies = ROOTNEX_PREALLOC_COOKIES;
uint_t	rootnex_prealloc_windows = ROOTNEX_PREALLOC_WINDOWS;
uint_t	rootnex_prealloc_copybuf = ROOTNEX_PREALLOC_COPYBUF;

#define	ROOTNEX_PREALLOC_SIZE						\
	((ROOTNEX_PREALLOC_COOKIES * sizeof (ddi_dma_cookie_t)) +	\
	 (ROOTNEX_PREALLOC_WINDOWS * sizeof (rootnex_window_t)) +	\
	 (ROOTNEX_PREALLOC_COPYBUF * sizeof (rootnex_pgmap_t)))

/* driver global state */
static rootnex_state_t	rootnex_state;

/* shortcut to rootnex counters */
static uint64_t *rootnex_cnt;

/*
 * XXX - does x86 even need these or are they left over from the SPARC days?
 */
/* statically defined integer/boolean properties for the root node */
static rootnex_intprop_t rootnex_intprp[] = {
	{ "PAGESIZE",			PAGESIZE },
	{ "MMU_PAGESIZE",		MMU_PAGESIZE },
	{ "MMU_PAGEOFFSET",		MMU_PAGEOFFSET },
	{ DDI_RELATIVE_ADDRESSING,	1 },
};
#define	NROOT_INTPROPS	(sizeof (rootnex_intprp) / sizeof (rootnex_intprop_t))


static struct cb_ops rootnex_cb_ops = {
	nodev,		/* open */
	nodev,		/* close */
	nodev,		/* strategy */
	nodev,		/* print */
	nodev,		/* dump */
	nodev,		/* read */
	nodev,		/* write */
	nodev,		/* ioctl */
	nodev,		/* devmap */
	nodev,		/* mmap */
	nodev,		/* segmap */
	nochpoll,	/* chpoll */
	ddi_prop_op,	/* cb_prop_op */
	NULL,		/* struct streamtab */
	D_NEW | D_MP | D_HOTPLUG, /* compatibility flags */
	CB_REV,		/* Rev */
	nodev,		/* cb_aread */
	nodev		/* cb_awrite */
};

static int rootnex_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp,
    off_t offset, off_t len, caddr_t *vaddrp);
static int rootnex_map_fault(dev_info_t *dip, dev_info_t *rdip,
    struct hat *hat, struct seg *seg, caddr_t addr,
    struct devpage *dp, pfn_t pfn, uint_t prot, uint_t lock);
static int rootnex_dma_map(dev_info_t *dip, dev_info_t *rdip,
    struct ddi_dma_req *dmareq, ddi_dma_handle_t *handlep);
static int rootnex_dma_allochdl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_attr_t *attr, int (*waitfp)(caddr_t), caddr_t arg,
    ddi_dma_handle_t *handlep);
static int rootnex_dma_freehdl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle);
static int rootnex_dma_bindhdl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, struct ddi_dma_req *dmareq,
    ddi_dma_cookie_t *cookiep, uint_t *ccountp);
static int rootnex_dma_unbindhdl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle);
static int rootnex_dma_sync(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, off_t off, size_t len, uint_t cache_flags);
static int rootnex_dma_win(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, uint_t win, off_t *offp, size_t *lenp,
    ddi_dma_cookie_t *cookiep, uint_t *ccountp);
static int rootnex_dma_mctl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, enum ddi_dma_ctlops request,
    off_t *offp, size_t *lenp, caddr_t *objp, uint_t cache_flags);
static int rootnex_ctlops(dev_info_t *dip, dev_info_t *rdip,
    ddi_ctl_enum_t ctlop, void *arg, void *result);
static int rootnex_fm_init(dev_info_t *dip, dev_info_t *tdip, int tcap,
    ddi_iblock_cookie_t *ibc);
static int rootnex_intr_ops(dev_info_t *pdip, dev_info_t *rdip,
    ddi_intr_op_t intr_op, ddi_intr_handle_impl_t *hdlp, void *result);

static caddr_t	rootnex_reg_mapin(pfn_t pfn, size_t size, uint_t attr);
static void	rootnex_reg_mapout(caddr_t addr, size_t len);

static struct bus_ops rootnex_bus_ops = {
	BUSO_REV,
	rootnex_map,
	NULL,
	NULL,
	NULL,
	rootnex_map_fault,
	rootnex_dma_map,
	rootnex_dma_allochdl,
	rootnex_dma_freehdl,
	rootnex_dma_bindhdl,
	rootnex_dma_unbindhdl,
	rootnex_dma_sync,
	rootnex_dma_win,
	rootnex_dma_mctl,
	rootnex_ctlops,
	ddi_bus_prop_op,
	i_ddi_rootnex_get_eventcookie,
	i_ddi_rootnex_add_eventcall,
	i_ddi_rootnex_remove_eventcall,
	i_ddi_rootnex_post_event,
	0,			/* bus_intr_ctl */
	0,			/* bus_config */
	0,			/* bus_unconfig */
#ifdef	FMA_ENABLE
	rootnex_fm_init,	/* bus_fm_init */
#else
	NULL,			/* bus_fm_init */
#endif	/* FMA_ENABLE */
	NULL,			/* bus_fm_fini */
	NULL,			/* bus_fm_access_enter */
	NULL,			/* bus_fm_access_exit */
	NULL,			/* bus_powr */
	rootnex_intr_ops	/* bus_intr_op */
};

static int rootnex_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int rootnex_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);

static struct dev_ops rootnex_ops = {
	DEVO_REV,
	0,
	ddi_no_info,
	nulldev,
	nulldev,
	rootnex_attach,
	rootnex_detach,
	nulldev,
	&rootnex_cb_ops,
	&rootnex_bus_ops
};

static struct modldrv rootnex_modldrv = {
	&mod_driverops,
	"ARM Platform root nexus 1.141",
	&rootnex_ops
};

static struct modlinkage rootnex_modlinkage = {
	MODREV_1,
	(void *)&rootnex_modldrv,
	NULL
};


/*
 *  extern hacks
 */
extern struct seg_ops segdev_ops;
extern int ignore_hardware_nodes;	/* force flag from ddi_impl.c */
#ifdef	DDI_MAP_DEBUG
extern int ddi_map_debug_flag;
#define	ddi_map_debug	if (ddi_map_debug_flag) prom_printf
#endif
#define	ptob64(x)	(((uint64_t)(x)) << MMU_PAGESHIFT)
extern void armpf_pp_map(page_t *pp, caddr_t kaddr);
extern void armpf_va_map(caddr_t vaddr, struct as *asp, caddr_t kaddr);
extern int gic_intr_ops(dev_info_t *dip, ddi_intr_handle_impl_t *hdlp,
			gic_intr_op_t intr_op, int *result);
extern int impl_ddi_sunbus_initchild(dev_info_t *dip);
extern void impl_ddi_sunbus_removechild(dev_info_t *dip);
/*
 * Use device arena to use for device control register mappings.
 * Various kernel memory walkers (debugger, dtrace) need to know
 * to avoid this address range to prevent undesired device activity.
 */
extern void *device_arena_alloc(size_t size, int vm_flag);
extern void device_arena_free(void * vaddr, size_t size);


/*
 *  Internal functions
 */
static int rootnex_dma_init();
static void rootnex_add_props(dev_info_t *);
static int rootnex_ctl_reportdev(dev_info_t *dip);
static struct intrspec *rootnex_get_ispec(dev_info_t *rdip, int inum);
static int rootnex_map_regspec(ddi_map_req_t *mp, caddr_t *vaddrp);
static int rootnex_unmap_regspec(ddi_map_req_t *mp, caddr_t *vaddrp);
static int rootnex_map_handle(ddi_map_req_t *mp);
static void rootnex_clean_dmahdl(ddi_dma_impl_t *hp);
static int rootnex_valid_alloc_parms(ddi_dma_attr_t *attr, uint_t maxsegsize);
static int rootnex_valid_bind_parms(ddi_dma_req_t *dmareq,
    ddi_dma_attr_t *attr);
static void rootnex_get_sgl(ddi_dma_obj_t *dmar_object, ddi_dma_cookie_t *sgl,
    rootnex_sglinfo_t *sglinfo);
static int rootnex_bind_slowpath(ddi_dma_impl_t *hp, struct ddi_dma_req *dmareq,
    rootnex_dma_t *dma, ddi_dma_attr_t *attr, int kmflag);
static int rootnex_setup_copybuf(ddi_dma_impl_t *hp, struct ddi_dma_req *dmareq,
    rootnex_dma_t *dma, ddi_dma_attr_t *attr);
static void rootnex_teardown_copybuf(rootnex_dma_t *dma);
static int rootnex_setup_windows(ddi_dma_impl_t *hp, rootnex_dma_t *dma,
    ddi_dma_attr_t *attr, int kmflag);
static void rootnex_teardown_windows(rootnex_dma_t *dma);
static void rootnex_init_win(ddi_dma_impl_t *hp, rootnex_dma_t *dma,
    rootnex_window_t *window, ddi_dma_cookie_t *cookie, off_t cur_offset);
static void rootnex_setup_cookie(ddi_dma_obj_t *dmar_object,
    rootnex_dma_t *dma, ddi_dma_cookie_t *cookie, off_t cur_offset,
    size_t *copybuf_used, page_t **cur_pp);
static int rootnex_sgllen_window_boundary(ddi_dma_impl_t *hp,
    rootnex_dma_t *dma, rootnex_window_t **windowp, ddi_dma_cookie_t *cookie,
    ddi_dma_attr_t *attr, off_t cur_offset);
static int rootnex_copybuf_window_boundary(ddi_dma_impl_t *hp,
    rootnex_dma_t *dma, rootnex_window_t **windowp,
    ddi_dma_cookie_t *cookie, off_t cur_offset, size_t *copybuf_used);
static int rootnex_maxxfer_window_boundary(ddi_dma_impl_t *hp,
    rootnex_dma_t *dma, rootnex_window_t **windowp, ddi_dma_cookie_t *cookie);
static int rootnex_valid_sync_parms(ddi_dma_impl_t *hp, rootnex_window_t *win,
    off_t offset, size_t size, uint_t cache_flags);
static int rootnex_verify_buffer(rootnex_dma_t *dma);
static int rootnex_dma_check(dev_info_t *dip, const void *handle,
    const void *comp_addr, const void *not_used);

/*
 * _init()
 *
 */
int
MODDRV_ENTRY_INIT(void)
{
	return (mod_install(&rootnex_modlinkage));
}

/*
 * _info()
 *
 */
int
MODDRV_ENTRY_INFO(struct modinfo *modinfop)
{
	return (mod_info(&rootnex_modlinkage, modinfop));
}

#ifndef	STATIC_DRIVER
/*
 * _fini()
 *
 */
int
MODDRV_ENTRY_FINI(void)
{
	return (EBUSY);
}
#endif	/* !STATIC_DRIVER */

/*
 * rootnex_attach()
 *
 */
static int
rootnex_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int fmcap;
	int e;


	switch (cmd) {
	case DDI_ATTACH:
		break;
	case DDI_RESUME:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	/*
	 * We should only have one instance of rootnex. Save it away since we
	 * don't have an easy way to get it back later.
	 */
	rootnex_state.r_dip = dip;
	rootnex_state.r_err_ibc = (ddi_iblock_cookie_t)ipltospl(15);
	rootnex_state.r_reserved_msg_printed = B_FALSE;
	rootnex_cnt = &rootnex_state.r_counters[0];

#ifdef	FMA_ENABLE
	/*
	 * Set minimum fm capability level for arm platforms and then
	 * initialize error handling. Since we're the rootnex, we don't
	 * care what's returned in the fmcap field.
	 */
	ddi_system_fmcap = DDI_FM_EREPORT_CAPABLE | DDI_FM_ERRCB_CAPABLE |
	    DDI_FM_ACCCHK_CAPABLE | DDI_FM_DMACHK_CAPABLE;
	fmcap = ddi_system_fmcap;
	ddi_fm_init(dip, &fmcap, &rootnex_state.r_err_ibc);
#endif	/* FMA_ENABLE */

	/* initialize DMA related state */
	e = rootnex_dma_init();
	if (e != DDI_SUCCESS) {
		return (DDI_FAILURE);
	}

	/* Add static root node properties */
	rootnex_add_props(dip);

	/* since we can't call ddi_report_dev() */
	cmn_err(CE_CONT, "?root nexus = %s\n", ddi_get_name(dip));

	/* Initialize rootnex event handle */
	i_ddi_rootnex_init_events(dip);

	return (DDI_SUCCESS);
}


/*
 * rootnex_detach()
 *
 */
/*ARGSUSED*/
static int
rootnex_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_SUSPEND:
		break;
	default:
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}


/*
 * rootnex_dma_init()
 *
 */
/*ARGSUSED*/
static int
rootnex_dma_init()
{
	size_t		bufsize;
	const size_t	dbufsz = sizeof(rootnex_dmabuf_t);

	/*
	 * size of our cookie/window/copybuf state needed in dma bind that we
	 * pre-alloc in dma_alloc_handle
	 */
	rootnex_state.r_prealloc_size =
	    (ROOTNEX_PREALLOC_COOKIES * sizeof (ddi_dma_cookie_t)) +
	    (ROOTNEX_PREALLOC_WINDOWS * sizeof (rootnex_window_t)) +
	    (ROOTNEX_PREALLOC_COPYBUF * sizeof (rootnex_pgmap_t));

	/*
	 * setup DDI DMA handle kmem cache, align each handle on 64 bytes,
	 * allocate 8 extra bytes for struct pointer alignment
	 * (dma->dp_prealloc_buffer)
	 */
	ASSERT(ISP2(ROOTNEX_BUF_ALIGN));
	ASSERT(ISP2(ROOTNEX_STRUCT_ALIGN));
	bufsize = dbufsz + ROOTNEX_PREALLOC_SIZE + ROOTNEX_STRUCT_ALIGN;
	bufsize = P2ROUNDUP(bufsize, ROOTNEX_BUF_ALIGN);
	rootnex_state.r_dmahdl_cache = kmem_cache_create("rootnex_dmahdl",
	    bufsize, ROOTNEX_BUF_ALIGN, NULL, NULL, NULL, NULL, NULL, 0);
	if (rootnex_state.r_dmahdl_cache == NULL) {
		return (DDI_FAILURE);
	}

	/*
	 * size of our cookie/window/copybuf state needed in dma bind that we
	 * pre-alloc in dma_alloc_handle
	 */
	rootnex_state.r_prealloc_size =
		bufsize - P2ROUNDUP(dbufsz, ROOTNEX_STRUCT_ALIGN);

#ifdef	DEBUG
	/*
	 * allocate array to track which major numbers we have printed warnings
	 * for.
	 */
	rootnex_warn_list = kmem_zalloc(devcnt * sizeof (*rootnex_warn_list),
	    KM_SLEEP);
#endif	/* DEBUG */

	return (DDI_SUCCESS);
}


/*
 * rootnex_add_props()
 *
 */
static void
rootnex_add_props(dev_info_t *dip)
{
	rootnex_intprop_t *rpp;
	int i;

	/* Add static integer/boolean properties to the root node */
	rpp = rootnex_intprp;
	for (i = 0; i < NROOT_INTPROPS; i++) {
		(void) e_ddi_prop_update_int(DDI_DEV_T_NONE, dip,
		    rpp[i].prop_name, rpp[i].prop_value);
	}
}



/*
 * *************************
 *  ctlops related routines
 * *************************
 */

/*
 * rootnex_ctlops()
 *
 */
/*ARGSUSED*/
static int
rootnex_ctlops(dev_info_t *dip, dev_info_t *rdip, ddi_ctl_enum_t ctlop,
    void *arg, void *result)
{
	int n, *ptr;
	struct ddi_parent_private_data *pdp;

	switch (ctlop) {
	case DDI_CTLOPS_DMAPMAPC:
		/*
		 * Return 'partial' to indicate that dma mapping
		 * has to be done in the main MMU.
		 */
		return (DDI_DMA_PARTIAL);

	case DDI_CTLOPS_BTOP:
		/*
		 * Convert byte count input to physical page units.
		 * (byte counts that are not a page-size multiple
		 * are rounded down)
		 */
		*(ulong_t *)result = btop(*(ulong_t *)arg);
		return (DDI_SUCCESS);

	case DDI_CTLOPS_PTOB:
		/*
		 * Convert size in physical pages to bytes
		 */
		*(ulong_t *)result = ptob(*(ulong_t *)arg);
		return (DDI_SUCCESS);

	case DDI_CTLOPS_BTOPR:
		/*
		 * Convert byte count input to physical page units
		 * (byte counts that are not a page-size multiple
		 * are rounded up)
		 */
		*(ulong_t *)result = btopr(*(ulong_t *)arg);
		return (DDI_SUCCESS);

	case DDI_CTLOPS_INITCHILD:
		return (impl_ddi_sunbus_initchild(arg));

	case DDI_CTLOPS_UNINITCHILD:
		impl_ddi_sunbus_removechild(arg);
		return (DDI_SUCCESS);

	case DDI_CTLOPS_REPORTDEV:
		return (rootnex_ctl_reportdev(rdip));

	case DDI_CTLOPS_IOMIN:
		/*
		 * Nothing to do here but reflect back..
		 */
		return (DDI_SUCCESS);

	case DDI_CTLOPS_REGSIZE:
	case DDI_CTLOPS_NREGS:
		break;

	case DDI_CTLOPS_SIDDEV:
		if (ndi_dev_is_prom_node(rdip))
			return (DDI_SUCCESS);
		if (ndi_dev_is_persistent_node(rdip))
			return (DDI_SUCCESS);
		return (DDI_FAILURE);

	case DDI_CTLOPS_POWER:
		return ((*pm_platform_power)((power_req_t *)arg));

	case DDI_CTLOPS_RESERVED0: /* Was DDI_CTLOPS_NINTRS, obsolete */
	case DDI_CTLOPS_RESERVED1: /* Was DDI_CTLOPS_POKE_INIT, obsolete */
	case DDI_CTLOPS_RESERVED2: /* Was DDI_CTLOPS_POKE_FLUSH, obsolete */
	case DDI_CTLOPS_RESERVED3: /* Was DDI_CTLOPS_POKE_FINI, obsolete */
	case DDI_CTLOPS_RESERVED4: /* Was DDI_CTLOPS_INTR_HILEVEL, obsolete */
	case DDI_CTLOPS_RESERVED5: /* Was DDI_CTLOPS_XLATE_INTRS, obsolete */
		if (!rootnex_state.r_reserved_msg_printed) {
			rootnex_state.r_reserved_msg_printed = B_TRUE;
			cmn_err(CE_WARN, "Failing ddi_ctlops call(s) for "
			    "1 or more reserved/obsolete operations.");
		}
		return (DDI_FAILURE);

	default:
		return (DDI_FAILURE);
	}
	/*
	 * The rest are for "hardware" properties
	 */
	if ((pdp = ddi_get_parent_data(rdip)) == NULL)
		return (DDI_FAILURE);

	if (ctlop == DDI_CTLOPS_NREGS) {
		ptr = (int *)result;
		*ptr = pdp->par_nreg;
	} else {
		off_t *size = (off_t *)result;

		ptr = (int *)arg;
		n = *ptr;
		if (n >= pdp->par_nreg) {
			return (DDI_FAILURE);
		}
		*size = (off_t)pdp->par_reg[n].regspec_size;
	}
	return (DDI_SUCCESS);
}


/*
 * rootnex_ctl_reportdev()
 *
 */
static int
rootnex_ctl_reportdev(dev_info_t *dev)
{
	int i, n, len, f_len = 0;
	char *buf;

	buf = kmem_alloc(REPORTDEV_BUFSIZE, KM_SLEEP);
	f_len += snprintf(buf, REPORTDEV_BUFSIZE,
	    "%s%d at root", ddi_driver_name(dev), ddi_get_instance(dev));
	len = strlen(buf);

	for (i = 0; i < sparc_pd_getnreg(dev); i++) {

		struct regspec *rp = sparc_pd_getreg(dev, i);

		if (i == 0)
			f_len += snprintf(buf + len, REPORTDEV_BUFSIZE - len,
			    ": ");
		else
			f_len += snprintf(buf + len, REPORTDEV_BUFSIZE - len,
			    " and ");
		len = strlen(buf);

		f_len += snprintf(buf + len, REPORTDEV_BUFSIZE - len,
				  "space %x offset %x",
				  rp->regspec_bustype, rp->regspec_addr);

		len = strlen(buf);
	}
	for (i = 0, n = sparc_pd_getnintr(dev); i < n; i++) {
		int pri;

		if (i != 0) {
			f_len += snprintf(buf + len, REPORTDEV_BUFSIZE - len,
			    ",");
			len = strlen(buf);
		}
		pri = INT_IPL(sparc_pd_getintr(dev, i)->intrspec_pri);
		f_len += snprintf(buf + len, REPORTDEV_BUFSIZE - len,
		    " sparc ipl %d", pri);
		len = strlen(buf);
	}
#ifdef DEBUG
	if (f_len + 1 >= REPORTDEV_BUFSIZE) {
		cmn_err(CE_NOTE, "next message is truncated: "
		    "printed length 1024, real length %d", f_len);
	}
#endif /* DEBUG */
	cmn_err(CE_CONT, "?%s\n", buf);
	kmem_free(buf, REPORTDEV_BUFSIZE);
	return (DDI_SUCCESS);
}


/*
 * ******************
 *  map related code
 * ******************
 */

/*
 * rootnex_map()
 *
 */
static int
rootnex_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp, off_t offset,
    off_t len, caddr_t *vaddrp)
{
	struct regspec *rp, tmp_reg;
	ddi_map_req_t mr = *mp;		/* Get private copy of request */
	int error;

	mp = &mr;

	switch (mp->map_op)  {
	case DDI_MO_MAP_LOCKED:
	case DDI_MO_UNMAP:
	case DDI_MO_MAP_HANDLE:
		break;
	default:
#ifdef	DDI_MAP_DEBUG
		cmn_err(CE_WARN, "rootnex_map: unimplemented map op %d.",
		    mp->map_op);
#endif	/* DDI_MAP_DEBUG */
		return (DDI_ME_UNIMPLEMENTED);
	}

	if (mp->map_flags & DDI_MF_USER_MAPPING)  {
#ifdef	DDI_MAP_DEBUG
		cmn_err(CE_WARN, "rootnex_map: unimplemented map type: user.");
#endif	/* DDI_MAP_DEBUG */
		return (DDI_ME_UNIMPLEMENTED);
	}

	/*
	 * First, if given an rnumber, convert it to a regspec...
	 * (Presumably, this is on behalf of a child of the root node?)
	 */

	if (mp->map_type == DDI_MT_RNUMBER)  {

		int rnumber = mp->map_obj.rnumber;
#ifdef	DDI_MAP_DEBUG
		static char *out_of_range =
		    "rootnex_map: Out of range rnumber <%d>, device <%s>";
#endif	/* DDI_MAP_DEBUG */

		rp = i_ddi_rnumber_to_regspec(rdip, rnumber);
		if (rp == NULL)  {
#ifdef	DDI_MAP_DEBUG
			cmn_err(CE_WARN, out_of_range, rnumber,
			    ddi_get_name(rdip));
#endif	/* DDI_MAP_DEBUG */
			return (DDI_ME_RNUMBER_RANGE);
		}

		/*
		 * Convert the given ddi_map_req_t from rnumber to regspec...
		 */

		mp->map_type = DDI_MT_REGSPEC;
		mp->map_obj.rp = rp;
	}

	/*
	 * Adjust offset and length correspnding to called values...
	 * XXX: A non-zero length means override the one in the regspec
	 * XXX: (regardless of what's in the parent's range?)
	 */

	tmp_reg = *(mp->map_obj.rp);		/* Preserve underlying data */
	rp = mp->map_obj.rp = &tmp_reg;		/* Use tmp_reg in request */

#ifdef	DDI_MAP_DEBUG
	cmn_err(CE_CONT,
		"rootnex: <%s,%s> <0x%x, 0x%x, 0x%d>"
		" offset %d len %d handle 0x%x\n",
		ddi_get_name(dip), ddi_get_name(rdip),
		rp->regspec_bustype, rp->regspec_addr, rp->regspec_size,
		offset, len, mp->map_handlep);
#endif	/* DDI_MAP_DEBUG */

	/*
	 * I/O or memory mapping:
	 *
	 *	<bustype=0, addr=x, len=x>: memory
	 *	<bustype=1, addr=x, len=x>: I/O
	 */
	if (rp->regspec_bustype > 1) {
		cmn_err(CE_WARN, "<%s,%s> invalid register spec"
		    " <0x%x, 0x%x, 0x%x>", ddi_get_name(dip),
		    ddi_get_name(rdip), rp->regspec_bustype,
		    rp->regspec_addr, rp->regspec_size);
		return (DDI_ME_INVAL);
	}

	rp->regspec_addr += (uint_t)offset;

	if (len != 0)
		rp->regspec_size = (uint_t)len;

#ifdef	DDI_MAP_DEBUG
	cmn_err(CE_CONT,
		"             <%s,%s> <0x%x, 0x%x, 0x%d>"
		" offset %d len %d handle 0x%x\n",
		ddi_get_name(dip), ddi_get_name(rdip),
		rp->regspec_bustype, rp->regspec_addr, rp->regspec_size,
		offset, len, mp->map_handlep);
#endif	/* DDI_MAP_DEBUG */

	/*
	 * Apply any parent ranges at this level, if applicable.
	 * (This is where nexus specific regspec translation takes place.
	 * Use of this function is implicit agreement that translation is
	 * provided via ddi_apply_range.)
	 */

#ifdef	DDI_MAP_DEBUG
	ddi_map_debug("applying range of parent <%s> to child <%s>...\n",
	    ddi_get_name(dip), ddi_get_name(rdip));
#endif	/* DDI_MAP_DEBUG */

	if ((error = i_ddi_apply_range(dip, rdip, mp->map_obj.rp)) != 0)
		return (error);

	switch (mp->map_op)  {
	case DDI_MO_MAP_LOCKED:

		/*
		 * Set up the locked down kernel mapping to the regspec...
		 */

		return (rootnex_map_regspec(mp, vaddrp));

	case DDI_MO_UNMAP:

		/*
		 * Release mapping...
		 */

		return (rootnex_unmap_regspec(mp, vaddrp));

	case DDI_MO_MAP_HANDLE:

		return (rootnex_map_handle(mp));

	default:
		return (DDI_ME_UNIMPLEMENTED);
	}
}


/*
 * rootnex_map_fault()
 *
 *	fault in mappings for requestors
 */
/*ARGSUSED*/
static int
rootnex_map_fault(dev_info_t *dip, dev_info_t *rdip, struct hat *hat,
    struct seg *seg, caddr_t addr, struct devpage *dp, pfn_t pfn, uint_t prot,
    uint_t lock)
{

#ifdef	DDI_MAP_DEBUG
	ddi_map_debug("rootnex_map_fault: address <%x> pfn <%x>", addr, pfn);
	ddi_map_debug(" Seg <%s>\n",
	    seg->s_ops == &segdev_ops ? "segdev" :
	    seg == &kvseg ? "segkmem" : "NONE!");
#endif	/* DDI_MAP_DEBUG */

	/*
	 * This is all terribly broken, but it is a start
	 *
	 * XXX	Note that this test means that segdev_ops
	 *	must be exported from seg_dev.c.
	 * XXX	What about devices with their own segment drivers?
	 */
	if (seg->s_ops == &segdev_ops) {
		struct segdev_data *sdp =
			(struct segdev_data *)seg->s_data;

		if (hat == NULL) {
			/*
			 * This is one plausible interpretation of
			 * a null hat i.e. use the first hat on the
			 * address space hat list which by convention is
			 * the hat of the system MMU.  At alternative
			 * would be to panic .. this might well be better ..
			 */
			ASSERT(AS_READ_HELD(seg->s_as, &seg->s_as->a_lock));
			hat = seg->s_as->a_hat;
			cmn_err(CE_NOTE, "rootnex_map_fault: nil hat");
		}
		hat_devload(hat, addr, MMU_PAGESIZE, pfn, prot | sdp->hat_attr,
		    (lock ? HAT_LOAD_LOCK : HAT_LOAD));
	} else if (seg == &kvseg && dp == NULL) {
		hat_devload(kas.a_hat, addr, MMU_PAGESIZE, pfn, prot,
		    HAT_LOAD_LOCK);
	} else
		return (DDI_FAILURE);
	return (DDI_SUCCESS);
}


/*
 * rootnex_map_regspec()
 *     we don't support mapping of I/O cards above 4Gb
 */
static int
rootnex_map_regspec(ddi_map_req_t *mp, caddr_t *vaddrp)
{
	ulong_t base;
	void *cvaddr;
	uint_t npages, pgoffset;
	struct regspec *rp;
	ddi_acc_hdl_t *hp;
	ddi_acc_impl_t *ap;
	uint_t	hat_acc_flags;

	rp = mp->map_obj.rp;
	hp = mp->map_handlep;

#ifdef	DDI_MAP_DEBUG
	ddi_map_debug(
	    "rootnex_map_regspec: <0x%x 0x%x 0x%x> handle 0x%x\n",
	    rp->regspec_bustype, rp->regspec_addr,
	    rp->regspec_size, mp->map_handlep);
#endif	/* DDI_MAP_DEBUG */

	/*
	 * I/O or memory mapping
	 *
	 *	<bustype=0, addr=x, len=x>: memory
	 *	<bustype=1, addr=x, len=x>: I/O
	 */
	if (rp->regspec_bustype > 1) {
		cmn_err(CE_WARN, "rootnex: invalid register spec"
		    " <0x%x, 0x%x, 0x%x>", rp->regspec_bustype,
		    rp->regspec_addr, rp->regspec_size);
		return (DDI_FAILURE);
	}

	if (rp->regspec_bustype != 0) {
		/*
		 * ARM architecture has no I/O space, 
		 * so we treat this like memory space.
		 */
		if (hp == NULL) {
			return (DDI_FAILURE);
		}
	}

	/*
	 * Memory space
	 */
	if (hp != NULL) {
		/*
		 * hat layer ignores
		 * hp->ah_acc.devacc_attr_endian_flags.
		 */
		switch (hp->ah_acc.devacc_attr_dataorder) {
		case DDI_STRICTORDER_ACC:
			hat_acc_flags = HAT_STRICTORDER;
			break;
		case DDI_UNORDERED_OK_ACC:
			hat_acc_flags = HAT_UNORDERED_OK;
			break;
		case DDI_MERGING_OK_ACC:
			hat_acc_flags = HAT_MERGING_OK;
			break;
		case DDI_LOADCACHING_OK_ACC:
			hat_acc_flags = HAT_LOADCACHING_OK;
			break;
		case DDI_STORECACHING_OK_ACC:
			hat_acc_flags = HAT_STORECACHING_OK;
			break;
		}

		if (rp->regspec_bustype == 1 &&
		    hat_acc_flags == HAT_STORECACHING_OK) {
			/* Invalid cache attribute. */
			return DDI_ME_INVAL;
		}
		ap = (ddi_acc_impl_t *)hp->ah_platform_private;
		ap->ahi_acc_attr |= DDI_ACCATTR_CPU_VADDR;
		impl_acc_hdl_init(hp);
		hp->ah_hat_flags = hat_acc_flags;
	} else {
		hat_acc_flags = HAT_STRICTORDER;
	}

	base = (ulong_t)rp->regspec_addr & (~MMU_PAGEOFFSET); /* base addr */
	pgoffset = (ulong_t)rp->regspec_addr & MMU_PAGEOFFSET; /* offset */

	if (rp->regspec_size == 0) {
#ifdef  DDI_MAP_DEBUG
		ddi_map_debug("rootnex_map_regspec: zero regspec_size\n");
#endif  /* DDI_MAP_DEBUG */
		return (DDI_ME_INVAL);
	}

	if (mp->map_flags & DDI_MF_DEVICE_MAPPING) {
		*vaddrp = (caddr_t)mmu_btop(base);
	} else {
		npages = mmu_btopr(rp->regspec_size + pgoffset);

#ifdef	DDI_MAP_DEBUG
		ddi_map_debug("rootnex_map_regspec: Mapping %d pages \
physical %x ",
		    npages, base);
#endif	/* DDI_MAP_DEBUG */

		cvaddr = rootnex_reg_mapin(mmu_btop(base), mmu_ptob(npages),
					   mp->map_prot|hat_acc_flags);
		if (cvaddr == NULL)
			return (DDI_ME_NORESOURCES);

		*vaddrp = (caddr_t)cvaddr + pgoffset;

		/* save away pfn and npages for FMA */
		hp = mp->map_handlep;
		if (hp) {
			hp->ah_pfn = mmu_btop(base);
			hp->ah_pnum = npages;
		}
	}

#ifdef	DDI_MAP_DEBUG
	ddi_map_debug("at virtual 0x%x\n", *vaddrp);
#endif	/* DDI_MAP_DEBUG */
	return (DDI_SUCCESS);
}


/*
 * rootnex_unmap_regspec()
 *
 */
static int
rootnex_unmap_regspec(ddi_map_req_t *mp, caddr_t *vaddrp)
{
	caddr_t addr = (caddr_t)*vaddrp;
	uint_t pgoffset;
	struct regspec *rp;

	if (mp->map_flags & DDI_MF_DEVICE_MAPPING)
		return (0);

	rp = mp->map_obj.rp;

	if (rp->regspec_size == 0) {
#ifdef  DDI_MAP_DEBUG
		ddi_map_debug("rootnex_unmap_regspec: zero regspec_size\n");
#endif  /* DDI_MAP_DEBUG */
		return (DDI_ME_INVAL);
	}

	/*
	 * I/O or memory mapping:
	 *
	 *	<bustype=0, addr=x, len=x>: memory
	 *	<bustype=1, addr=x, len=x>: I/O
	 */
	pgoffset = (uintptr_t)addr & MMU_PAGEOFFSET;
	rootnex_reg_mapout(addr - pgoffset,
			   PAGE_ROUNDUP(rp->regspec_size + pgoffset));

	/*
	 * Destroy the pointer - the mapping has logically gone
	 */
	*vaddrp = NULL;

	return (DDI_SUCCESS);
}


/*
 * rootnex_map_handle()
 *
 */
static int
rootnex_map_handle(ddi_map_req_t *mp)
{
	ddi_acc_hdl_t *hp;
	ulong_t base;
	uint_t pgoffset;
	struct regspec *rp;

	rp = mp->map_obj.rp;

#ifdef	DDI_MAP_DEBUG
	ddi_map_debug(
	    "rootnex_map_handle: <0x%x 0x%x 0x%x> handle 0x%x\n",
	    rp->regspec_bustype, rp->regspec_addr,
	    rp->regspec_size, mp->map_handlep);
#endif	/* DDI_MAP_DEBUG */

	/*
	 * I/O or memory mapping:
	 *
	 *	<bustype=0, addr=x, len=x>: memory
	 *	<bustype=1, addr=x, len=x>: I/O
	 */

	/*
	 * Set up the hat_flags for the mapping.
	 */
	hp = mp->map_handlep;

	switch (hp->ah_acc.devacc_attr_endian_flags) {
	case DDI_NEVERSWAP_ACC:
		hp->ah_hat_flags = HAT_NEVERSWAP | HAT_STRICTORDER;
		break;
	case DDI_STRUCTURE_LE_ACC:
		hp->ah_hat_flags = HAT_STRUCTURE_LE;
		break;
	case DDI_STRUCTURE_BE_ACC:
		return (DDI_FAILURE);
	default:
		return (DDI_REGS_ACC_CONFLICT);
	}

	switch (hp->ah_acc.devacc_attr_dataorder) {
	case DDI_STRICTORDER_ACC:
		break;
	case DDI_UNORDERED_OK_ACC:
		hp->ah_hat_flags |= HAT_UNORDERED_OK;
		break;
	case DDI_MERGING_OK_ACC:
		hp->ah_hat_flags |= HAT_MERGING_OK;
		break;
	case DDI_LOADCACHING_OK_ACC:
		hp->ah_hat_flags |= HAT_LOADCACHING_OK;
		break;
	case DDI_STORECACHING_OK_ACC:
		hp->ah_hat_flags |= HAT_STORECACHING_OK;
		break;
	default:
		return (DDI_FAILURE);
	}

	if (rp->regspec_bustype == 1 &&
	    (hp->ah_hat_flags & HAT_STORECACHING_OK)) {
		/* Invalid cache attribute. */
		return DDI_ME_INVAL;
	}

	base = (ulong_t)rp->regspec_addr & (~MMU_PAGEOFFSET); /* base addr */
	pgoffset = (ulong_t)rp->regspec_addr & MMU_PAGEOFFSET; /* offset */

	if (rp->regspec_size == 0)
		return (DDI_ME_INVAL);

	hp->ah_pfn = mmu_btop(base);
	hp->ah_pnum = mmu_btopr(rp->regspec_size + pgoffset);

	return (DDI_SUCCESS);
}



/*
 * ************************
 *  interrupt related code
 * ************************
 */

/*
 * rootnex_intr_ops()
 *	bus_intr_op() function for interrupt support
 */
/* ARGSUSED */
static int
rootnex_intr_ops(dev_info_t *pdip, dev_info_t *rdip, ddi_intr_op_t intr_op,
    ddi_intr_handle_impl_t *hdlp, void *result)
{
	struct intrspec			*ispec;
	struct ddi_parent_private_data	*pdp;

	DDI_INTR_NEXDBG((CE_CONT,
	    "rootnex_intr_ops: pdip = %p, rdip = %p, intr_op = %x, hdlp = %p\n",
	    (void *)pdip, (void *)rdip, intr_op, (void *)hdlp));

	/* Process the interrupt operation */
	switch (intr_op) {
	case DDI_INTROP_GETCAP:
		/* First check with gic */
		if (gic_intr_ops(rdip, hdlp, GIC_INTR_OP_GET_CAP, result)) {
			*(int *)result = 0;
			return (DDI_FAILURE);
		}
		break;
	case DDI_INTROP_SETCAP:
		if (gic_intr_ops(rdip, hdlp, GIC_INTR_OP_SET_CAP, result))
			return (DDI_FAILURE);

		return DDI_FAILURE;
		break;
	case DDI_INTROP_ALLOC:
		if ((ispec = rootnex_get_ispec(rdip, hdlp->ih_inum)) == NULL)
			return (DDI_FAILURE);
		hdlp->ih_pri = ispec->intrspec_pri;
		*(int *)result = hdlp->ih_scratch1;
		break;
	case DDI_INTROP_FREE:
		pdp = ddi_get_parent_data(rdip);
		/*
		 * Special case for 'pcic' driver' only.
		 * If an intrspec was created for it, clean it up here
		 * See detailed comments on this in the function
		 * rootnex_get_ispec().
		 */
		if (pdp->par_intr && strcmp(ddi_get_name(rdip), "pcic") == 0) {
			kmem_free(pdp->par_intr, sizeof (struct intrspec) *
			    pdp->par_nintr);
			/*
			 * Set it to zero; so that
			 * DDI framework doesn't free it again
			 */
			pdp->par_intr = NULL;
			pdp->par_nintr = 0;
		}
		break;
	case DDI_INTROP_GETPRI:
		if ((ispec = rootnex_get_ispec(rdip, hdlp->ih_inum)) == NULL)
			return (DDI_FAILURE);
		*(int *)result = ispec->intrspec_pri;
		break;
	case DDI_INTROP_SETPRI:
		/* Validate the interrupt priority passed to us */
		if (*(int *)result > LOCK_LEVEL)
			return (DDI_FAILURE);

		/* Ensure that ispec is ok */
		if ((ispec = rootnex_get_ispec(rdip, hdlp->ih_inum)) == NULL)
			return (DDI_FAILURE);

		/* Change the priority */
		if (gic_intr_ops(rdip, hdlp, GIC_INTR_OP_SET_PRI, result) ==
		    GIC_FAILURE)
			return (DDI_FAILURE);

		/* update the ispec with the new priority */
		ispec->intrspec_pri =  *(int *)result;
		break;
	case DDI_INTROP_ADDISR:
		if ((ispec = rootnex_get_ispec(rdip, hdlp->ih_inum)) == NULL)
			return (DDI_FAILURE);
		ispec->intrspec_func = hdlp->ih_cb_func;
		break;
	case DDI_INTROP_REMISR:
		if ((ispec = rootnex_get_ispec(rdip, hdlp->ih_inum)) == NULL)
			return (DDI_FAILURE);
		ispec->intrspec_func = (uint_t (*)()) 0;
		break;
	case DDI_INTROP_ENABLE:
		if ((ispec = rootnex_get_ispec(rdip, hdlp->ih_inum)) == NULL)
			return (DDI_FAILURE);

		/* Add the interrupt handler */
		if (!add_avintr((void *)hdlp, ispec->intrspec_pri,
				hdlp->ih_cb_func, DEVI(rdip)->devi_name,
				ispec->intrspec_vec, hdlp->ih_cb_arg1,
				hdlp->ih_cb_arg2, NULL, rdip)) {
			return (DDI_FAILURE);
		}
		break;
	case DDI_INTROP_DISABLE:
		if ((ispec = rootnex_get_ispec(rdip, hdlp->ih_inum)) == NULL)
			return (DDI_FAILURE);

		/* Remove the interrupt handler */
		rem_avintr((void *)hdlp, ispec->intrspec_pri,
		    hdlp->ih_cb_func, ispec->intrspec_vec);
		break;
	case DDI_INTROP_SETMASK:
		if (gic_intr_ops(rdip, hdlp, GIC_INTR_OP_SET_MASK, NULL))
			return (DDI_FAILURE);
		break;
	case DDI_INTROP_CLRMASK:
		if (gic_intr_ops(rdip, hdlp, GIC_INTR_OP_CLEAR_MASK, NULL))
			return (DDI_FAILURE);
		break;
	case DDI_INTROP_GETPENDING:
		if (gic_intr_ops(rdip, hdlp, GIC_INTR_OP_GET_PENDING,
		    result)) {
			*(int *)result = 0;
			return (DDI_FAILURE);
		}
		break;
	case DDI_INTROP_NAVAIL:
	case DDI_INTROP_NINTRS:
		*(int *)result = i_ddi_get_intx_nintrs(rdip);
		if (*(int *)result == 0) {
			/*
			 * Special case for 'pcic' driver' only. This driver
			 * driver is a child of 'isa' and 'rootnex' drivers.
			 *
			 * See detailed comments on this in the function
			 * rootnex_get_ispec().
			 *
			 * Children of 'pcic' send 'NINITR' request all the
			 * way to rootnex driver. But, the 'pdp->par_nintr'
			 * field may not initialized. So, we fake it here
			 * to return 1 (a la what PCMCIA nexus does).
			 */
			if (strcmp(ddi_get_name(rdip), "pcic") == 0)
				*(int *)result = 1;
			else
				return (DDI_FAILURE);
		}
		break;
	case DDI_INTROP_SUPPORTED_TYPES:
		*(int *)result = DDI_INTR_TYPE_FIXED;	/* Always ... */
		break;
	default:
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}


/*
 * rootnex_get_ispec()
 *	convert an interrupt number to an interrupt specification.
 *	The interrupt number determines which interrupt spec will be
 *	returned if more than one exists.
 *
 *	Look into the parent private data area of the 'rdip' to find out
 *	the interrupt specification.  First check to make sure there is
 *	one that matchs "inumber" and then return a pointer to it.
 *
 *	Return NULL if one could not be found.
 *
 *	NOTE: This is needed for rootnex_intr_ops()
 */
static struct intrspec *
rootnex_get_ispec(dev_info_t *rdip, int inum)
{
	struct ddi_parent_private_data *pdp = ddi_get_parent_data(rdip);

	/*
	 * Special case handling for drivers that provide their own
	 * intrspec structures instead of relying on the DDI framework.
	 *
	 * A broken hardware driver in ON could potentially provide its
	 * own intrspec structure, instead of relying on the hardware.
	 * If these drivers are children of 'rootnex' then we need to
	 * continue to provide backward compatibility to them here.
	 *
	 * Following check is a special case for 'pcic' driver which
	 * was found to have broken hardwre andby provides its own intrspec.
	 *
	 * Verbatim comments from this driver are shown here:
	 * "Don't use the ddi_add_intr since we don't have a
	 * default intrspec in all cases."
	 *
	 * Since an 'ispec' may not be always created for it,
	 * check for that and create one if so.
	 *
	 * NOTE: Currently 'pcic' is the only driver found to do this.
	 */
	if (!pdp->par_intr && strcmp(ddi_get_name(rdip), "pcic") == 0) {
		pdp->par_nintr = 1;
		pdp->par_intr = kmem_zalloc(sizeof (struct intrspec) *
		    pdp->par_nintr, KM_SLEEP);
	}

	/* Validate the interrupt number */
	if (inum >= pdp->par_nintr)
		return (NULL);

	/* Get the interrupt structure pointer and return that */
	return ((struct intrspec *)&pdp->par_intr[inum]);
}


/*
 * ******************
 *  dma related code
 * ******************
 */

/*
 * rootnex_dma_allochdl()
 *    called from ddi_dma_alloc_handle().
 */
/*ARGSUSED*/
static int
rootnex_dma_allochdl(dev_info_t *dip, dev_info_t *rdip, ddi_dma_attr_t *attr,
    int (*waitfp)(caddr_t), caddr_t arg, ddi_dma_handle_t *handlep)
{
	uint_t maxsegmentsize;
	ddi_dma_impl_t *hp;
	rootnex_dma_t *dma;
	uint32_t count_max;
	uint32_t seg;
	int kmflag;
	int e;
	uchar_t	*pbuf;
	const uint32_t	maxxfer  = (uint32_t)MIN(attr->dma_attr_maxxfer,
						 ROOTNEX_MAX_PADDR);
	const uint32_t	attr_seg = (uint32_t)MIN(attr->dma_attr_seg,
						 ROOTNEX_MAX_PADDR);

	/* convert our sleep flags */
	if (waitfp == DDI_DMA_SLEEP) {
		kmflag = KM_SLEEP;
	} else {
		kmflag = KM_NOSLEEP;
	}

	/*
	 * We try to do only one memory allocation here. We'll do a little
	 * pointer manipulation later. If the bind ends up taking more than
	 * our prealloc's space, we'll have to allocate more memory in the
	 * bind operation. Not great, but much better than before and the
	 * best we can do with the current bind interfaces.
	 */
	hp = kmem_cache_alloc(rootnex_state.r_dmahdl_cache, kmflag);
	if (hp == NULL) {
		if (waitfp != DDI_DMA_DONTWAIT) {
			ddi_set_callback(waitfp, arg,
			    &rootnex_state.r_dvma_call_list_id);
		}
		return (DDI_DMA_NORESOURCES);
	}

	/* Do our pointer manipulation now, align the structures */
	dma = ROOTNEX_DMABUF_TO_DMA(hp);
	hp->dmai_private = dma;
	pbuf = (uchar_t *)dma + sizeof(rootnex_dma_t);
	dma->dp_prealloc_buffer = (uchar_t *)
		P2ROUNDUP_TYPED(pbuf, ROOTNEX_STRUCT_ALIGN, uintptr_t);

	/* setup the handle */
	rootnex_clean_dmahdl(hp);
	dma->dp_dip = rdip;
	dma->dp_sglinfo.si_min_addr = (uint32_t)attr->dma_attr_addr_lo;
	dma->dp_sglinfo.si_max_addr =
		(uint32_t)MIN(attr->dma_attr_addr_hi, ROOTNEX_MAX_PADDR);
	hp->dmai_minxfer = attr->dma_attr_minxfer;
	hp->dmai_burstsizes = attr->dma_attr_burstsizes;
	hp->dmai_rdip = rdip;
	hp->dmai_attr = *attr;

	/* we don't need to worry about the SPL since we do a tryenter */
	mutex_init(&dma->dp_mutex, NULL, MUTEX_DRIVER, NULL);

	/*
	 * Figure out our maximum segment size. If the segment size is greater
	 * than 4G, we will limit it to (4G - 1) since the max size of a dma
	 * object (ddi_dma_obj_t.dmao_size) is 32 bits. dma_attr_seg and
	 * dma_attr_count_max are size-1 type values.
	 *
	 * Maximum segment size is the largest physically contiguous chunk of
	 * memory that we can return from a bind (i.e. the maximum size of a
	 * single cookie).
	 */

	/* handle the rollover cases */
	seg = attr_seg + 1;
	if (seg < attr_seg) {
		seg = attr_seg;
	}
	if (attr->dma_attr_count_max > ROOTNEX_MAX_PADDR) {
		count_max = (uint32_t)ROOTNEX_MAX_PADDR;
	}
	else {
		count_max = attr->dma_attr_count_max + 1;
		if (count_max < attr->dma_attr_count_max) {
			count_max = attr->dma_attr_count_max;
		}
	}
	ASSERT(count_max);

	/*
	 * granularity may or may not be a power of two. If it isn't, we can't
	 * use a simple mask.
	 */
	if (attr->dma_attr_granular & (attr->dma_attr_granular - 1)) {
		dma->dp_granularity_power_2 = RB_FALSE;
	} else {
		dma->dp_granularity_power_2 = RB_TRUE;
	}

	/*
	 * maxxfer should be a whole multiple of granularity. If we're going to
	 * break up a window because we're greater than maxxfer, we might as
	 * well make sure it's maxxfer is a whole multiple so we don't have to
	 * worry about triming the window later on for this case.
	 */
	if (attr->dma_attr_granular > 1) {
		if (dma->dp_granularity_power_2) {
			dma->dp_maxxfer = maxxfer -
			    (maxxfer & (attr->dma_attr_granular - 1));
		} else {
			dma->dp_maxxfer = maxxfer -
			    (maxxfer % attr->dma_attr_granular);
		}
	} else {
		dma->dp_maxxfer = maxxfer;
	}

	maxsegmentsize = MIN(seg, dma->dp_maxxfer);
	maxsegmentsize = MIN(maxsegmentsize, count_max);
	if (maxsegmentsize == 0) {
		maxsegmentsize = (uint32_t)ROOTNEX_MAX_PADDR;
	}
	dma->dp_sglinfo.si_max_cookie_size = maxsegmentsize;
	dma->dp_sglinfo.si_segmask = attr_seg;

	/* check the ddi_dma_attr arg to make sure it makes a little sense */
	if (rootnex_alloc_check_parms) {
		e = rootnex_valid_alloc_parms(attr, maxsegmentsize);
		if (e != DDI_SUCCESS) {
			ROOTNEX_PROF_INC(&rootnex_cnt[ROOTNEX_CNT_ALLOC_FAIL]);
			(void) rootnex_dma_freehdl(dip, rdip,
			    (ddi_dma_handle_t)hp);
			return (e);
		}
	}

	*handlep = (ddi_dma_handle_t)hp;

	ROOTNEX_PROF_INC(&rootnex_cnt[ROOTNEX_CNT_ACTIVE_HDLS]);
	DTRACE_PROBE1(rootnex__alloc__handle, uint64_t,
	    rootnex_cnt[ROOTNEX_CNT_ACTIVE_HDLS]);

	return (DDI_SUCCESS);
}


/*
 * rootnex_dma_freehdl()
 *    called from ddi_dma_free_handle().
 */
/*ARGSUSED*/
static int
rootnex_dma_freehdl(dev_info_t *dip, dev_info_t *rdip, ddi_dma_handle_t handle)
{
	ddi_dma_impl_t *hp;
	rootnex_dma_t *dma;


	hp = (ddi_dma_impl_t *)handle;
	dma = (rootnex_dma_t *)hp->dmai_private;

	/* unbind should have been called first */
	ASSERT(!dma->dp_inuse);

	mutex_destroy(&dma->dp_mutex);
	kmem_cache_free(rootnex_state.r_dmahdl_cache, hp);

	ROOTNEX_PROF_DEC(&rootnex_cnt[ROOTNEX_CNT_ACTIVE_HDLS]);
	DTRACE_PROBE1(rootnex__free__handle, uint64_t,
	    rootnex_cnt[ROOTNEX_CNT_ACTIVE_HDLS]);

	if (rootnex_state.r_dvma_call_list_id)
		ddi_run_callback(&rootnex_state.r_dvma_call_list_id);

	return (DDI_SUCCESS);
}


/*
 * rootnex_dma_bindhdl()
 *    called from ddi_dma_addr_bind_handle() and ddi_dma_buf_bind_handle().
 */
/*ARGSUSED*/
static int
rootnex_dma_bindhdl(dev_info_t *dip, dev_info_t *rdip, ddi_dma_handle_t handle,
    struct ddi_dma_req *dmareq, ddi_dma_cookie_t *cookiep, uint_t *ccountp)
{
	rootnex_sglinfo_t *sinfo;
	ddi_dma_attr_t *attr;
	ddi_dma_impl_t *hp;
	rootnex_dma_t *dma;
	int kmflag;
	int e;


	hp = (ddi_dma_impl_t *)handle;
	dma = (rootnex_dma_t *)hp->dmai_private;
	sinfo = &dma->dp_sglinfo;
	attr = &hp->dmai_attr;

	hp->dmai_rflags = dmareq->dmar_flags & DMP_DDIFLAGS;

	/*
	 * This is useful for debugging a driver. Not as useful in a production
	 * system. The only time this will fail is if you have a driver bug.
	 */
	if (rootnex_bind_check_inuse) {
		/*
		 * No one else should ever have this lock unless someone else
		 * is trying to use this handle. So contention on the lock
		 * is the same as inuse being set.
		 */
		e = mutex_tryenter(&dma->dp_mutex);
		if (e == 0) {
			ROOTNEX_PROF_INC(&rootnex_cnt[ROOTNEX_CNT_BIND_FAIL]);
			return (DDI_DMA_INUSE);
		}
		if (dma->dp_inuse) {
			mutex_exit(&dma->dp_mutex);
			ROOTNEX_PROF_INC(&rootnex_cnt[ROOTNEX_CNT_BIND_FAIL]);
			return (DDI_DMA_INUSE);
		}
		dma->dp_inuse = RB_TRUE;
		mutex_exit(&dma->dp_mutex);
	}

	/* check the ddi_dma_attr arg to make sure it makes a little sense */
	if (rootnex_bind_check_parms) {
		e = rootnex_valid_bind_parms(dmareq, attr);
		if (e != DDI_SUCCESS) {
			ROOTNEX_PROF_INC(&rootnex_cnt[ROOTNEX_CNT_BIND_FAIL]);
			rootnex_clean_dmahdl(hp);
			return (e);
		}
	}

	/* save away the original bind info */
	dma->dp_dma = dmareq->dmar_object;

	/*
	 * Figure out a rough estimate of what maximum number of pages this
	 * buffer could use (a high estimate of course).
	 */
	sinfo->si_max_pages = mmu_btopr(dma->dp_dma.dmao_size) + 1;

	/*
	 * We'll use the pre-allocated cookies for any bind that will *always*
	 * fit (more important to be consistent, we don't want to create
	 * additional degenerate cases).
	 */
	if (sinfo->si_max_pages <= ROOTNEX_PREALLOC_COOKIES) {
		dma->dp_cookies = (ddi_dma_cookie_t *)dma->dp_prealloc_buffer;
		dma->dp_need_to_free_cookie = RB_FALSE;
		DTRACE_PROBE2(rootnex__bind__prealloc, dev_info_t *, rdip,
		    uint_t, sinfo->si_max_pages);

	/*
	 * For anything larger than that, we'll go ahead and allocate the
	 * maximum number of pages we expect to see. Hopefuly, we won't be
	 * seeing this path in the fast path for high performance devices very
	 * frequently.
	 *
	 * a ddi bind interface that allowed the driver to provide storage to
	 * the bind interface would speed this case up.
	 */
	} else {
		/* convert the sleep flags */
		if (dmareq->dmar_fp == DDI_DMA_SLEEP) {
			kmflag =  KM_SLEEP;
		} else {
			kmflag =  KM_NOSLEEP;
		}

		/*
		 * Save away how much memory we allocated. If we're doing a
		 * nosleep, the alloc could fail...
		 */
		dma->dp_cookie_size = sinfo->si_max_pages *
		    sizeof (ddi_dma_cookie_t);
		dma->dp_cookies = kmem_alloc(dma->dp_cookie_size, kmflag);
		if (dma->dp_cookies == NULL) {
			ROOTNEX_PROF_INC(&rootnex_cnt[ROOTNEX_CNT_BIND_FAIL]);
			rootnex_clean_dmahdl(hp);
			return (DDI_DMA_NORESOURCES);
		}
		dma->dp_need_to_free_cookie = RB_TRUE;
		DTRACE_PROBE2(rootnex__bind__alloc, dev_info_t *, rdip, uint_t,
		    sinfo->si_max_pages);
	}
	hp->dmai_cookie = dma->dp_cookies;

	/*
	 * Get the real sgl. rootnex_get_sgl will fill in cookie array while
	 * looking at the contraints in the dma structure. It will then put some
	 * additional state about the sgl in the dma struct (i.e. is the sgl
	 * clean, or do we need to do some munging; how many pages need to be
	 * copied, etc.)
	 */
	rootnex_get_sgl(&dmareq->dmar_object, dma->dp_cookies,
	    &dma->dp_sglinfo);
	ASSERT(sinfo->si_sgl_size <= sinfo->si_max_pages);

	if (!ARMPF_DMA_SYNC_REQUIRED(handle) && sinfo->si_copybuf_req == 0) {
		/* We don't need to sync DMA buffer. */
		hp->dmai_rflags |= DMP_NOSYNC;
	}

	/*
	 * if we don't need the copybuf and we don't need to do a partial,  we
	 * hit the fast path. All the high performance devices should be trying
	 * to hit this path. To hit this path, a device should be able to reach
	 * all of memory, shouldn't try to bind more than it can transfer, and
	 * the buffer shouldn't require more cookies than the driver/device can
	 * handle [sgllen]).
	 */
	if ((sinfo->si_copybuf_req == 0) &&
	    (sinfo->si_sgl_size <= attr->dma_attr_sgllen) &&
	    (dma->dp_dma.dmao_size < dma->dp_maxxfer)) {
#ifdef	FMA_ENABLE
		/*
		 * If the driver supports FMA, insert the handle in the FMA DMA
		 * handle cache.
		 */
		if (attr->dma_attr_flags & DDI_DMA_FLAGERR) {
			hp->dmai_error.err_cf = rootnex_dma_check;
			(void) ndi_fmc_insert(rdip, DMA_HANDLE, hp, NULL);
		}
#endif	/* FMA_ENABLE */

		/*
		 * copy out the first cookie and ccountp, set the cookie
		 * pointer to the second cookie. The first cookie is passed
		 * back on the stack. Additional cookies are accessed via
		 * ddi_dma_nextcookie()
		 */
		*cookiep = dma->dp_cookies[0];
		*ccountp = sinfo->si_sgl_size;
		hp->dmai_cookie++;
		hp->dmai_rflags &= ~DDI_DMA_PARTIAL;
		hp->dmai_nwin = 1;
		ROOTNEX_PROF_INC(&rootnex_cnt[ROOTNEX_CNT_ACTIVE_BINDS]);
		DTRACE_PROBE3(rootnex__bind__fast, dev_info_t *, rdip, uint64_t,
		    rootnex_cnt[ROOTNEX_CNT_ACTIVE_BINDS], uint_t,
		    dma->dp_dma.dmao_size);
		return (DDI_DMA_MAPPED);
	}

	/*
	 * go to the slow path, we may need to alloc more memory, create
	 * multiple windows, and munge up a sgl to make the device happy.
	 */
	e = rootnex_bind_slowpath(hp, dmareq, dma, attr, kmflag);
	if ((e != DDI_DMA_MAPPED) && (e != DDI_DMA_PARTIAL_MAP)) {
		if (dma->dp_need_to_free_cookie) {
			kmem_free(dma->dp_cookies, dma->dp_cookie_size);
		}
		ROOTNEX_PROF_INC(&rootnex_cnt[ROOTNEX_CNT_BIND_FAIL]);
		rootnex_clean_dmahdl(hp); /* must be after free cookie */
		return (e);
	}

#ifdef	FMA_ENABLE
	/*
	 * If the driver supports FMA, insert the handle in the FMA DMA handle
	 * cache.
	 */
	if (attr->dma_attr_flags & DDI_DMA_FLAGERR) {
		hp->dmai_error.err_cf = rootnex_dma_check;
		(void) ndi_fmc_insert(rdip, DMA_HANDLE, hp, NULL);
	}
#endif	/* FMA_ENABLE */

	/* if the first window uses the copy buffer, sync it for the device */
	if (ARMPF_DMA_WRITE_SYNC_REQUIRED(handle) ||
	    ((dma->dp_window[dma->dp_current_win].wd_dosync) &&
	     (hp->dmai_rflags & DDI_DMA_WRITE))) {
		(void) rootnex_dma_sync(dip, rdip, handle, 0, 0,
		    DDI_DMA_SYNC_FORDEV);
	}

	/*
	 * copy out the first cookie and ccountp, set the cookie pointer to the
	 * second cookie. Make sure the partial flag is set/cleared correctly.
	 * If we have a partial map (i.e. multiple windows), the number of
	 * cookies we return is the number of cookies in the first window.
	 */
	if (e == DDI_DMA_MAPPED) {
		hp->dmai_rflags &= ~DDI_DMA_PARTIAL;
		*ccountp = sinfo->si_sgl_size;
	} else {
		hp->dmai_rflags |= DDI_DMA_PARTIAL;
		*ccountp = dma->dp_window[dma->dp_current_win].wd_cookie_cnt;
		ASSERT(hp->dmai_nwin <= dma->dp_max_win);
	}
	*cookiep = dma->dp_cookies[0];
	hp->dmai_cookie++;

	ROOTNEX_PROF_INC(&rootnex_cnt[ROOTNEX_CNT_ACTIVE_BINDS]);
	DTRACE_PROBE3(rootnex__bind__slow, dev_info_t *, rdip, uint64_t,
	    rootnex_cnt[ROOTNEX_CNT_ACTIVE_BINDS], uint_t,
	    dma->dp_dma.dmao_size);
	return (e);
}


/*
 * rootnex_dma_unbindhdl()
 *    called from ddi_dma_unbind_handle()
 */
/*ARGSUSED*/
static int
rootnex_dma_unbindhdl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle)
{
	ddi_dma_impl_t *hp;
	rootnex_dma_t *dma;
	int e;


	hp = (ddi_dma_impl_t *)handle;
	dma = (rootnex_dma_t *)hp->dmai_private;

	/* make sure the buffer wasn't free'd before calling unbind */
	if (rootnex_unbind_verify_buffer) {
		e = rootnex_verify_buffer(dma);
		if (e != DDI_SUCCESS) {
			ASSERT(0);
			return (DDI_FAILURE);
		}
	}

	/* sync the current window before unbinding the buffer */
	if (ARMPF_DMA_READ_SYNC_REQUIRED(handle) ||
	    (dma->dp_window &&
	     dma->dp_window[dma->dp_current_win].wd_dosync &&
	     (hp->dmai_rflags & DDI_DMA_READ))) {
		(void) rootnex_dma_sync(dip, rdip, handle, 0, 0,
		    DDI_DMA_SYNC_FORCPU);
	}

#ifdef	FMA_ENABLE
	/*
	 * If the driver supports FMA, remove the handle in the FMA DMA handle
	 * cache.
	 */
	if (hp->dmai_attr.dma_attr_flags & DDI_DMA_FLAGERR) {
		if ((DEVI(rdip)->devi_fmhdl != NULL) &&
		    (DDI_FM_DMA_ERR_CAP(DEVI(rdip)->devi_fmhdl->fh_cap))) {
			(void) ndi_fmc_remove(rdip, DMA_HANDLE, hp);
		}
	}
#endif	/* FMA_ENABLE */

	/*
	 * cleanup and copy buffer or window state. if we didn't use the copy
	 * buffer or windows, there won't be much to do :-)
	 */
	rootnex_teardown_copybuf(dma);
	rootnex_teardown_windows(dma);

	/*
	 * If we had to allocate space to for the worse case sgl (it didn't
	 * fit into our pre-allocate buffer), free that up now
	 */
	if (dma->dp_need_to_free_cookie) {
		kmem_free(dma->dp_cookies, dma->dp_cookie_size);
	}

	/*
	 * clean up the handle so it's ready for the next bind (i.e. if the
	 * handle is reused).
	 */
	rootnex_clean_dmahdl(hp);

	if (rootnex_state.r_dvma_call_list_id)
		ddi_run_callback(&rootnex_state.r_dvma_call_list_id);

	ROOTNEX_PROF_DEC(&rootnex_cnt[ROOTNEX_CNT_ACTIVE_BINDS]);
	DTRACE_PROBE1(rootnex__unbind, uint64_t,
	    rootnex_cnt[ROOTNEX_CNT_ACTIVE_BINDS]);

	return (DDI_SUCCESS);
}


/*
 * rootnex_verify_buffer()
 *   verify buffer wasn't free'd
 */
static int
rootnex_verify_buffer(rootnex_dma_t *dma)
{
	page_t **pplist;
	caddr_t vaddr;
	uint_t pcnt;
	uint_t poff;
	page_t *pp;
	char b;
	int i;

	/* Figure out how many pages this buffer occupies */
	if (dma->dp_dma.dmao_type == DMA_OTYP_PAGES) {
		poff = dma->dp_dma.dmao_obj.pp_obj.pp_offset & MMU_PAGEOFFSET;
	} else {
		vaddr = dma->dp_dma.dmao_obj.virt_obj.v_addr;
		poff = (uintptr_t)vaddr & MMU_PAGEOFFSET;
	}
	pcnt = mmu_btopr(dma->dp_dma.dmao_size + poff);

	switch (dma->dp_dma.dmao_type) {
	case DMA_OTYP_PAGES:
		/*
		 * for a linked list of pp's walk through them to make sure
		 * they're locked and not free.
		 */
		pp = dma->dp_dma.dmao_obj.pp_obj.pp_pp;
		for (i = 0; i < pcnt; i++) {
			if (PP_ISFREE(pp) || !PAGE_LOCKED(pp)) {
				return (DDI_FAILURE);
			}
			pp = pp->p_next;
		}
		break;

	case DMA_OTYP_VADDR:
	case DMA_OTYP_BUFVADDR:
		pplist = dma->dp_dma.dmao_obj.virt_obj.v_priv;
		/*
		 * for an array of pp's walk through them to make sure they're
		 * not free. It's possible that they may not be locked.
		 */
		if (pplist) {
			for (i = 0; i < pcnt; i++) {
				if (PP_ISFREE(pplist[i])) {
					return (DDI_FAILURE);
				}
			}

		/* For a virtual address, try to peek at each page */
		} else {
			if (dma->dp_sglinfo.si_asp == &kas) {
				for (i = 0; i < pcnt; i++) {
					if (ddi_peek8(NULL, vaddr, &b) ==
					    DDI_FAILURE)
						return (DDI_FAILURE);
					vaddr += MMU_PAGESIZE;
				}
			}
		}
		break;

	default:
		ASSERT(0);
		break;
	}

	return (DDI_SUCCESS);
}


/*
 * rootnex_clean_dmahdl()
 *    Clean the dma handle. This should be called on a handle alloc and an
 *    unbind handle. Set the handle state to the default settings.
 */
static void
rootnex_clean_dmahdl(ddi_dma_impl_t *hp)
{
	rootnex_dma_t *dma;


	dma = (rootnex_dma_t *)hp->dmai_private;

	hp->dmai_nwin = 0;
	dma->dp_current_cookie = 0;
	dma->dp_copybuf_size = 0;
	dma->dp_window = NULL;
	dma->dp_cbaddr = NULL;
	dma->dp_inuse = RB_FALSE;
	dma->dp_need_to_free_cookie = RB_FALSE;
	dma->dp_need_to_free_window = RB_FALSE;
	dma->dp_partial_required = RB_FALSE;
	dma->dp_trim_required = RB_FALSE;
	dma->dp_sglinfo.si_copybuf_req = 0;
	dma->dp_cb_remaping = RB_FALSE;
	dma->dp_kva = NULL;

	/* FMA related initialization */
	hp->dmai_fault = 0;
	hp->dmai_fault_check = NULL;
	hp->dmai_fault_notify = NULL;
	hp->dmai_error.err_ena = 0;
	hp->dmai_error.err_status = DDI_FM_OK;
	hp->dmai_error.err_expected = DDI_FM_ERR_UNEXPECTED;
	hp->dmai_error.err_ontrap = NULL;
	hp->dmai_error.err_fep = NULL;
	hp->dmai_error.err_cf = NULL;
}


/*
 * rootnex_valid_alloc_parms()
 *    Called in ddi_dma_alloc_handle path to validate its parameters.
 */
static int
rootnex_valid_alloc_parms(ddi_dma_attr_t *attr, uint_t maxsegmentsize)
{
	if ((attr->dma_attr_seg < MMU_PAGEOFFSET) ||
	    (attr->dma_attr_count_max < MMU_PAGEOFFSET) ||
	    (attr->dma_attr_granular > MMU_PAGESIZE) ||
	    (attr->dma_attr_maxxfer < MMU_PAGESIZE)) {
		return (DDI_DMA_BADATTR);
	}

	if (attr->dma_attr_addr_lo > ROOTNEX_MAX_PADDR) {
		return (DDI_DMA_BADATTR);
	}
	if (attr->dma_attr_addr_hi <= attr->dma_attr_addr_lo) {
		return (DDI_DMA_BADATTR);
	}

	if ((attr->dma_attr_seg & MMU_PAGEOFFSET) != MMU_PAGEOFFSET ||
	    MMU_PAGESIZE & (attr->dma_attr_granular - 1) ||
	    attr->dma_attr_sgllen <= 0) {
		return (DDI_DMA_BADATTR);
	}

	/* We should be able to DMA into every byte offset in a page */
	if (maxsegmentsize < MMU_PAGESIZE) {
		return (DDI_DMA_BADATTR);
	}

	return (DDI_SUCCESS);
}


/*
 * rootnex_valid_bind_parms()
 *    Called in ddi_dma_*_bind_handle path to validate its parameters.
 */
/* ARGSUSED */
static int
rootnex_valid_bind_parms(ddi_dma_req_t *dmareq, ddi_dma_attr_t *attr)
{
	/*
	 * we only support up to a 2G-1 transfer size on 32-bit kernels so
	 * we can track the offset for the obsoleted interfaces.
	 */
	if (dmareq->dmar_object.dmao_size > 0x7FFFFFFF) {
		return (DDI_DMA_TOOBIG);
	}

	return (DDI_SUCCESS);
}


/*
 * rootnex_get_sgl()
 *    Called in bind fastpath to get the sgl. Most of this will be replaced
 *    with a call to the vm layer when vm2.0 comes around...
 */
static void
rootnex_get_sgl(ddi_dma_obj_t *dmar_object, ddi_dma_cookie_t *sgl,
    rootnex_sglinfo_t *sglinfo)
{
	ddi_dma_atyp_t buftype;
	uint32_t last_page;
	uint32_t offset;
	uint32_t addrhi;
	uint32_t addrlo;
	uint32_t maxseg;
	page_t **pplist;
	uint32_t paddr;
	uint32_t psize;
	uint32_t size;
	caddr_t vaddr;
	uint_t pcnt;
	page_t *pp;
	uint_t cnt;
	int use_copybuf;

	/* shortcuts */
	pplist = dmar_object->dmao_obj.virt_obj.v_priv;
	vaddr = dmar_object->dmao_obj.virt_obj.v_addr;
	maxseg = sglinfo->si_max_cookie_size;
	buftype = dmar_object->dmao_type;
	addrhi = sglinfo->si_max_addr;
	addrlo = sglinfo->si_min_addr;
	size = dmar_object->dmao_size;

	pcnt = 0;
	cnt = 0;

	/*
	 * if we were passed down a linked list of pages, i.e. pointer to
	 * page_t, use this to get our physical address and buf offset.
	 */
	if (buftype == DMA_OTYP_PAGES) {
		pp = dmar_object->dmao_obj.pp_obj.pp_pp;
		ASSERT(!PP_ISFREE(pp) && PAGE_LOCKED(pp));
		offset =  dmar_object->dmao_obj.pp_obj.pp_offset &
		    MMU_PAGEOFFSET;
		ASSERT(pp->p_pagenum <= ROOTNEX_MAX_PFN);
		paddr = ptob(pp->p_pagenum) + offset;
		psize = MIN(size, (MMU_PAGESIZE - offset));
		pp = pp->p_next;
		sglinfo->si_asp = NULL;
		use_copybuf = 1;

	/*
	 * We weren't passed down a linked list of pages, but if we were passed
	 * down an array of pages, use this to get our physical address and buf
	 * offset.
	 */
	} else if (pplist != NULL) {
		ASSERT((buftype == DMA_OTYP_VADDR) ||
		    (buftype == DMA_OTYP_BUFVADDR));

		offset = (uintptr_t)vaddr & MMU_PAGEOFFSET;
		sglinfo->si_asp = dmar_object->dmao_obj.virt_obj.v_as;
		if (sglinfo->si_asp == NULL) {
			sglinfo->si_asp = &kas;
		}

		ASSERT(!PP_ISFREE(pplist[pcnt]));
		ASSERT(pplist[pcnt]->p_pagenum <= ROOTNEX_MAX_PFN);
		paddr = ptob(pplist[pcnt]->p_pagenum);
		paddr += offset;
		psize = MIN(size, (MMU_PAGESIZE - offset));
		pcnt++;
		use_copybuf = 1;
	/*
	 * All we have is a virtual address, we'll need to call into the VM
	 * to get the physical address.
	 */
	} else {
		ASSERT((buftype == DMA_OTYP_VADDR) ||
		    (buftype == DMA_OTYP_BUFVADDR));

		offset = (uintptr_t)vaddr & MMU_PAGEOFFSET;
		sglinfo->si_asp = dmar_object->dmao_obj.virt_obj.v_as;
		if (sglinfo->si_asp == NULL) {
			sglinfo->si_asp = &kas;
		}

		ROOTNEX_VTOP(sglinfo->si_asp->a_hat, vaddr, paddr,
			     use_copybuf);
		paddr += offset;
		psize = MIN(size, (MMU_PAGESIZE - offset));
		vaddr += psize;
	}

	/*
	 * Setup the first cookie with the physical address of the page and the
	 * size of the page (which takes into account the initial offset into
	 * the page.
	 */
	sgl[cnt].dmac_laddress = paddr;
	sgl[cnt].dmac_size = psize;
	sgl[cnt].dmac_type = 0;

	/*
	 * Save away the buffer offset into the page. We'll need this later in
	 * the copy buffer code to help figure out the page index within the
	 * buffer and the offset into the current page.
	 */
	sglinfo->si_buf_offset = offset;

	/*
	 * If the DMA engine can't reach the physical address, increase how
	 * much copy buffer we need. We always increase by pagesize so we don't
	 * have to worry about converting offsets. Set a flag in the cookies
	 * dmac_type to indicate that it uses the copy buffer. If this isn't the
	 * last cookie, go to the next cookie (since we separate each page which
	 * uses the copy buffer in case the copy buffer is not physically
	 * contiguous.
	 */
	if (use_copybuf || (paddr < addrlo) || (paddr > (addrhi - psize))) {
		sglinfo->si_copybuf_req += MMU_PAGESIZE;
		sgl[cnt].dmac_type = ROOTNEX_USES_COPYBUF;
		if ((cnt + 1) < sglinfo->si_max_pages) {
			cnt++;
			sgl[cnt].dmac_laddress = 0;
			sgl[cnt].dmac_size = 0;
			sgl[cnt].dmac_type = 0;
		}
	}

	/*
	 * save this page's physical address so we can figure out if the next
	 * page is physically contiguous. Keep decrementing size until we are
	 * done with the buffer.
	 */
	last_page = paddr & MMU_PAGEMASK;
	size -= psize;

	while (size > 0) {
		/* Get the size for this page (i.e. partial or full page) */
		psize = MIN(size, MMU_PAGESIZE);

		if (buftype == DMA_OTYP_PAGES) {
			/* get the paddr from the page_t */
			ASSERT(!PP_ISFREE(pp) && PAGE_LOCKED(pp));
			ASSERT(pp->p_pagenum <= ROOTNEX_MAX_PFN);
			paddr = ptob(pp->p_pagenum);
			pp = pp->p_next;
			use_copybuf = 1;
		} else if (pplist != NULL) {
			/* index into the array of page_t's to get the paddr */
			ASSERT(!PP_ISFREE(pplist[pcnt]));
			ASSERT(pplist[pcnt]->p_pagenum <= ROOTNEX_MAX_PFN);
			paddr = ptob(pplist[pcnt]->p_pagenum);
			pcnt++;
			use_copybuf = 1;
		} else {
			/* call into the VM to get the paddr */
			ROOTNEX_VTOP(sglinfo->si_asp->a_hat, vaddr, paddr,
				     use_copybuf);
			vaddr += psize;
		}

		/* check to see if this page needs the copy buffer */
		if (use_copybuf || (paddr < addrlo) ||
		    (paddr > (addrhi - psize))) {
			sglinfo->si_copybuf_req += MMU_PAGESIZE;

			/*
			 * if there is something in the current cookie, go to
			 * the next one. We only want one page in a cookie which
			 * uses the copybuf since the copybuf doesn't have to
			 * be physically contiguous.
			 */
			if (sgl[cnt].dmac_size != 0) {
				cnt++;
			}
			sgl[cnt].dmac_laddress = paddr;
			sgl[cnt].dmac_size = psize;
			/*
			 * save the buf offset for 32-bit kernel. used in the
			 * obsoleted interfaces.
			 */
			sgl[cnt].dmac_type = ROOTNEX_USES_COPYBUF |
			    (dmar_object->dmao_size - size);
			/* if this isn't the last cookie, go to the next one */
			if ((cnt + 1) < sglinfo->si_max_pages) {
				cnt++;
				sgl[cnt].dmac_laddress = 0;
				sgl[cnt].dmac_size = 0;
				sgl[cnt].dmac_type = 0;
			}

		/*
		 * this page didn't need the copy buffer, if it's not physically
		 * contiguous, or it would put us over a segment boundary, or it
		 * puts us over the max cookie size, or the current sgl doesn't
		 * have anything in it.
		 */
		} else if (((last_page + MMU_PAGESIZE) != paddr) ||
		    !(paddr & sglinfo->si_segmask) ||
		    ((sgl[cnt].dmac_size + psize) > maxseg) ||
		    (sgl[cnt].dmac_size == 0)) {
			/*
			 * if we're not already in a new cookie, go to the next
			 * cookie.
			 */
			if (sgl[cnt].dmac_size != 0) {
				cnt++;
			}

			/* save the cookie information */
			sgl[cnt].dmac_laddress = paddr;
			sgl[cnt].dmac_size = psize;
			/*
			 * save the buf offset for 32-bit kernel. used in the
			 * obsoleted interfaces.
			 */
			sgl[cnt].dmac_type = dmar_object->dmao_size - size;

		/*
		 * this page didn't need the copy buffer, it is physically
		 * contiguous with the last page, and it's <= the max cookie
		 * size.
		 */
		} else {
			sgl[cnt].dmac_size += psize;

			/*
			 * if this exactly ==  the maximum cookie size, and
			 * it isn't the last cookie, go to the next cookie.
			 */
			if (((sgl[cnt].dmac_size + psize) == maxseg) &&
			    ((cnt + 1) < sglinfo->si_max_pages)) {
				cnt++;
				sgl[cnt].dmac_laddress = 0;
				sgl[cnt].dmac_size = 0;
				sgl[cnt].dmac_type = 0;
			}
		}

		/*
		 * save this page's physical address so we can figure out if the
		 * next page is physically contiguous. Keep decrementing size
		 * until we are done with the buffer.
		 */
		last_page = paddr;
		size -= psize;
	}

	/* we're done, save away how many cookies the sgl has */
	if (sgl[cnt].dmac_size == 0) {
		ASSERT(cnt < sglinfo->si_max_pages);
		sglinfo->si_sgl_size = cnt;
	} else {
		sglinfo->si_sgl_size = cnt + 1;
	}
}


/*
 * rootnex_bind_slowpath()
 *    Call in the bind path if the calling driver can't use the sgl without
 *    modifying it. We either need to use the copy buffer and/or we will end up
 *    with a partial bind.
 */
static int
rootnex_bind_slowpath(ddi_dma_impl_t *hp, struct ddi_dma_req *dmareq,
    rootnex_dma_t *dma, ddi_dma_attr_t *attr, int kmflag)
{
	rootnex_sglinfo_t *sinfo;
	rootnex_window_t *window;
	ddi_dma_cookie_t *cookie;
	size_t copybuf_used;
	size_t dmac_size;
	boolean_t partial;
	off_t cur_offset;
	page_t *cur_pp;
	major_t mnum;
	int e;
	int i;


	sinfo = &dma->dp_sglinfo;
	copybuf_used = 0;
	partial = B_FALSE;

	/*
	 * If we're using the copybuf, set the copybuf state in dma struct.
	 * Needs to be first since it sets the copy buffer size.
	 */
	if (sinfo->si_copybuf_req != 0) {
		e = rootnex_setup_copybuf(hp, dmareq, dma, attr);
		if (e != DDI_SUCCESS) {
			return (e);
		}
	} else {
		dma->dp_copybuf_size = 0;
	}

	/*
	 * Figure out if we need to do a partial mapping. If so, figure out
	 * if we need to trim the buffers when we munge the sgl.
	 */
	if ((dma->dp_copybuf_size < sinfo->si_copybuf_req) ||
	    (dma->dp_dma.dmao_size > dma->dp_maxxfer) ||
	    (attr->dma_attr_sgllen < sinfo->si_sgl_size)) {
		dma->dp_partial_required = RB_TRUE;
		if (attr->dma_attr_granular != 1) {
			dma->dp_trim_required = RB_TRUE;
		}
	} else {
		dma->dp_partial_required = RB_FALSE;
		dma->dp_trim_required = RB_FALSE;
	}

	/* If we need to do a partial bind, make sure the driver supports it */
	if (dma->dp_partial_required &&
	    !(dmareq->dmar_flags & DDI_DMA_PARTIAL)) {
#ifdef	DEBUG
		mnum = ddi_driver_major(dma->dp_dip);
		/*
		 * patchable which allows us to print one warning per major
		 * number.
		 */
		if ((rootnex_bind_warn) &&
		    ((rootnex_warn_list[mnum] & ROOTNEX_BIND_WARNING) == 0)) {
			rootnex_warn_list[mnum] |= ROOTNEX_BIND_WARNING;
			cmn_err(CE_WARN, "!%s: coding error detected, the "
			    "driver is using ddi_dma_attr(9S) incorrectly. "
			    "There is a small risk of data corruption in "
			    "particular with large I/Os. The driver should be "
			    "replaced with a corrected version for proper "
			    "system operation. To disable this warning, add "
			    "'set rootnex:rootnex_bind_warn=0' to "
			    "/etc/system(4).", ddi_driver_name(dma->dp_dip));
		}
#endif	/* DEBUG */
		return (DDI_DMA_TOOBIG);
	}

	/*
	 * we might need multiple windows, setup state to handle them. In this
	 * code path, we will have at least one window.
	 */
	e = rootnex_setup_windows(hp, dma, attr, kmflag);
	if (e != DDI_SUCCESS) {
		rootnex_teardown_copybuf(dma);
		return (e);
	}

	window = &dma->dp_window[0];
	cookie = &dma->dp_cookies[0];
	cur_offset = 0;
	rootnex_init_win(hp, dma, window, cookie, cur_offset);
	if (dmareq->dmar_object.dmao_type == DMA_OTYP_PAGES) {
		cur_pp = dmareq->dmar_object.dmao_obj.pp_obj.pp_pp;
	}

	/* loop though all the cookies we got back from get_sgl() */
	for (i = 0; i < sinfo->si_sgl_size; i++) {
		/*
		 * If we're using the copy buffer, check this cookie and setup
		 * its associated copy buffer state. If this cookie uses the
		 * copy buffer, make sure we sync this window during dma_sync.
		 */
		if (dma->dp_copybuf_size > 0) {
			rootnex_setup_cookie(&dmareq->dmar_object, dma, cookie,
			    cur_offset, &copybuf_used, &cur_pp);
			if (cookie->dmac_type & ROOTNEX_USES_COPYBUF) {
				window->wd_dosync = RB_TRUE;
			}
		}

		/*
		 * save away the cookie size, since it could be modified in
		 * the windowing code.
		 */
		dmac_size = cookie->dmac_size;

		/* if we went over max copybuf size */
		if (dma->dp_copybuf_size &&
		    (copybuf_used > dma->dp_copybuf_size)) {
			partial = B_TRUE;
			e = rootnex_copybuf_window_boundary(hp, dma, &window,
			    cookie, cur_offset, &copybuf_used);
			if (e != DDI_SUCCESS) {
				rootnex_teardown_copybuf(dma);
				rootnex_teardown_windows(dma);
				return (e);
			}

			/*
			 * if the coookie uses the copy buffer, make sure the
			 * new window we just moved to is set to sync.
			 */
			if (cookie->dmac_type & ROOTNEX_USES_COPYBUF) {
				window->wd_dosync = RB_TRUE;
			}
			DTRACE_PROBE1(rootnex__copybuf__window, dev_info_t *,
			    dma->dp_dip);

		/* if the cookie cnt == max sgllen, move to the next window */
		} else if (window->wd_cookie_cnt >= attr->dma_attr_sgllen) {
			partial = B_TRUE;
			ASSERT(window->wd_cookie_cnt == attr->dma_attr_sgllen);
			e = rootnex_sgllen_window_boundary(hp, dma, &window,
			    cookie, attr, cur_offset);
			if (e != DDI_SUCCESS) {
				rootnex_teardown_copybuf(dma);
				rootnex_teardown_windows(dma);
				return (e);
			}

			/*
			 * if the coookie uses the copy buffer, make sure the
			 * new window we just moved to is set to sync.
			 */
			if (cookie->dmac_type & ROOTNEX_USES_COPYBUF) {
				window->wd_dosync = RB_TRUE;
			}
			DTRACE_PROBE1(rootnex__sgllen__window, dev_info_t *,
			    dma->dp_dip);

		/* else if we will be over maxxfer */
		} else if ((window->wd_size + dmac_size) >
		    dma->dp_maxxfer) {
			partial = B_TRUE;
			e = rootnex_maxxfer_window_boundary(hp, dma, &window,
			    cookie);
			if (e != DDI_SUCCESS) {
				rootnex_teardown_copybuf(dma);
				rootnex_teardown_windows(dma);
				return (e);
			}

			/*
			 * if the coookie uses the copy buffer, make sure the
			 * new window we just moved to is set to sync.
			 */
			if (cookie->dmac_type & ROOTNEX_USES_COPYBUF) {
				window->wd_dosync = RB_TRUE;
			}
			DTRACE_PROBE1(rootnex__maxxfer__window, dev_info_t *,
			    dma->dp_dip);

		/* else this cookie fits in the current window */
		} else {
			window->wd_cookie_cnt++;
			window->wd_size += dmac_size;
		}

		/* track our offset into the buffer, go to the next cookie */
		ASSERT(dmac_size <= dma->dp_dma.dmao_size);
		ASSERT(cookie->dmac_size <= dmac_size);
		cur_offset += dmac_size;
		cookie++;
	}

	/* if we ended up with a zero sized window in the end, clean it up */
	if (window->wd_size == 0) {
		hp->dmai_nwin--;
		window--;
	}

	ASSERT(window->wd_trim.tr_trim_last == RB_FALSE);

	if (!partial) {
		return (DDI_DMA_MAPPED);
	}

	ASSERT(dma->dp_partial_required);
	return (DDI_DMA_PARTIAL_MAP);
}


/*
 * rootnex_setup_copybuf()
 *    Called in bind slowpath. Figures out if we're going to use the copy
 *    buffer, and if we do, sets up the basic state to handle it.
 */
static int
rootnex_setup_copybuf(ddi_dma_impl_t *hp, struct ddi_dma_req *dmareq,
    rootnex_dma_t *dma, ddi_dma_attr_t *attr)
{
	rootnex_sglinfo_t *sinfo;
	ddi_dma_attr_t lattr;
	size_t max_copybuf;
	int cansleep;
	int e;
	int vmflag;


	sinfo = &dma->dp_sglinfo;

	/*
	 * read this first so it's consistent through the routine so we can
	 * patch it on the fly.
	 */
	max_copybuf = rootnex_max_copybuf_size & MMU_PAGEMASK;

	/* We need to call into the rootnex on ddi_dma_sync() */
	hp->dmai_rflags &= ~DMP_NOSYNC;

	/* make sure the copybuf size <= the max size */
	dma->dp_copybuf_size = MIN(sinfo->si_copybuf_req, max_copybuf);
	ASSERT((dma->dp_copybuf_size & MMU_PAGEOFFSET) == 0);

	/*
	 * if we don't have kva space to copy to/from, allocate the KVA space
	 * now.
	 */
	if ((dmareq->dmar_object.dmao_type == DMA_OTYP_PAGES) ||
	    (dmareq->dmar_object.dmao_obj.virt_obj.v_as != NULL)) {

		/* convert the sleep flags */
		if (dmareq->dmar_fp == DDI_DMA_SLEEP) {
			vmflag = VM_SLEEP;
		} else {
			vmflag = VM_NOSLEEP;
		}

		/* allocate Kernel VA space that we can bcopy to/from */
		dma->dp_kva = vmem_alloc(heap_arena, dma->dp_copybuf_size,
		    vmflag);
		if (dma->dp_kva == NULL) {
			return (DDI_DMA_NORESOURCES);
		}
	}

	/* convert the sleep flags */
	if (dmareq->dmar_fp == DDI_DMA_SLEEP) {
		cansleep = 1;
	} else {
		cansleep = 0;
	}

	/*
	 * Allocated the actual copy buffer. This needs to fit within the DMA
	 * engines limits, so we can't use kmem_alloc...
	 */
	lattr = *attr;
	lattr.dma_attr_align = MMU_PAGESIZE;
	e = i_ddi_mem_alloc(dma->dp_dip, &lattr, dma->dp_copybuf_size, cansleep,
	    0, NULL, &dma->dp_cbaddr, &dma->dp_cbsize, NULL);
	if (e != DDI_SUCCESS) {
		if (dma->dp_kva != NULL) {
			vmem_free(heap_arena, dma->dp_kva,
			    dma->dp_copybuf_size);
		}
		return (DDI_DMA_NORESOURCES);
	}

	DTRACE_PROBE2(rootnex__alloc__copybuf, dev_info_t *, dma->dp_dip,
	    size_t, dma->dp_copybuf_size);

	return (DDI_SUCCESS);
}


/*
 * rootnex_setup_windows()
 *    Called in bind slowpath to setup the window state. We always have windows
 *    in the slowpath. Even if the window count = 1.
 */
static int
rootnex_setup_windows(ddi_dma_impl_t *hp, rootnex_dma_t *dma,
    ddi_dma_attr_t *attr, int kmflag)
{
	rootnex_window_t *windowp;
	rootnex_sglinfo_t *sinfo;
	size_t copy_state_size;
	size_t win_state_size;
	size_t state_available;
	size_t space_needed;
	uint_t copybuf_win;
	uint_t maxxfer_win;
	size_t space_used;
	uint_t sglwin;


	sinfo = &dma->dp_sglinfo;

	dma->dp_current_win = 0;
	hp->dmai_nwin = 0;

	/* If we don't need to do a partial, we only have one window */
	if (!dma->dp_partial_required) {
		dma->dp_max_win = 1;

	/*
	 * we need multiple windows, need to figure out the worse case number
	 * of windows.
	 */
	} else {
		/*
		 * if we need windows because we need more copy buffer that
		 * we allow, the worse case number of windows we could need
		 * here would be (copybuf space required / copybuf space that
		 * we have) plus one for remainder, and plus 2 to handle the
		 * extra pages on the trim for the first and last pages of the
		 * buffer (a page is the minimum window size so under the right
		 * attr settings, you could have a window for each page).
		 * The last page will only be hit here if the size is not a
		 * multiple of the granularity (which theoretically shouldn't
		 * be the case but never has been enforced, so we could have
		 * broken things without it).
		 */
		if (sinfo->si_copybuf_req > dma->dp_copybuf_size) {
			ASSERT(dma->dp_copybuf_size > 0);
			copybuf_win = (sinfo->si_copybuf_req /
			    dma->dp_copybuf_size) + 1 + 2;
		} else {
			copybuf_win = 0;
		}

		/*
		 * if we need windows because we have more cookies than the H/W
		 * can handle, the number of windows we would need here would
		 * be (cookie count / cookies count H/W supports) plus one for
		 * remainder, and plus 2 to handle the extra pages on the trim
		 * (see above comment about trim)
		 */
		if (attr->dma_attr_sgllen < sinfo->si_sgl_size) {
			sglwin = ((sinfo->si_sgl_size / attr->dma_attr_sgllen)
			    + 1) + 2;
		} else {
			sglwin = 0;
		}

		/*
		 * if we need windows because we're binding more memory than the
		 * H/W can transfer at once, the number of windows we would need
		 * here would be (xfer count / max xfer H/W supports) plus one
		 * for remainder, and plus 2 to handle the extra pages on the
		 * trim (see above comment about trim)
		 */
		if (dma->dp_dma.dmao_size > dma->dp_maxxfer) {
			maxxfer_win = (dma->dp_dma.dmao_size /
			    dma->dp_maxxfer) + 1 + 2;
		} else {
			maxxfer_win = 0;
		}
		dma->dp_max_win =  copybuf_win + sglwin + maxxfer_win;
		ASSERT(dma->dp_max_win > 0);
	}
	win_state_size = dma->dp_max_win * sizeof (rootnex_window_t);

	/*
	 * Get space for window and potential copy buffer state. Before we
	 * go and allocate memory, see if we can get away with using what's
	 * left in the pre-allocted state or the dynamically allocated sgl.
	 */
	space_used = (uintptr_t)(sinfo->si_sgl_size *
	    sizeof (ddi_dma_cookie_t));

	/* if we dynamically allocated space for the cookies */
	if (dma->dp_need_to_free_cookie) {
		/* if we have more space in the pre-allocted buffer, use it */
		ASSERT(space_used <= dma->dp_cookie_size);
		if ((dma->dp_cookie_size - space_used) <=
		    rootnex_state.r_prealloc_size) {
			state_available = rootnex_state.r_prealloc_size;
			windowp = (rootnex_window_t *)dma->dp_prealloc_buffer;

		/*
		 * else, we have more free space in the dynamically allocated
		 * buffer, i.e. the buffer wasn't worse case fragmented so we
		 * didn't need a lot of cookies.
		 */
		} else {
			state_available = dma->dp_cookie_size - space_used;
			windowp = (rootnex_window_t *)
			    &dma->dp_cookies[sinfo->si_sgl_size];
		}

	/* we used the pre-alloced buffer */
	} else {
		ASSERT(space_used <= rootnex_state.r_prealloc_size);
		state_available = rootnex_state.r_prealloc_size - space_used;
		windowp = (rootnex_window_t *)
		    &dma->dp_cookies[sinfo->si_sgl_size];
	}

	/*
	 * figure out how much state we need to track the copy buffer. Add an
	 * addition 8 bytes for pointer alignemnt later.
	 */
	if (dma->dp_copybuf_size > 0) {
		copy_state_size = sinfo->si_max_pages *
		    sizeof (rootnex_pgmap_t);
	} else {
		copy_state_size = 0;
	}
	/* add an additional 8 bytes for pointer alignment */
	space_needed = win_state_size + copy_state_size + 0x8;

	/* if we have enough space already, use it */
	if (state_available >= space_needed) {
		dma->dp_window = windowp;
		dma->dp_need_to_free_window = RB_FALSE;

	/* not enough space, need to allocate more. */
	} else {
		dma->dp_window = kmem_alloc(space_needed, kmflag);
		if (dma->dp_window == NULL) {
			return (DDI_DMA_NORESOURCES);
		}
		dma->dp_need_to_free_window = RB_TRUE;
		dma->dp_window_size = space_needed;
		DTRACE_PROBE2(rootnex__bind__sp__alloc, dev_info_t *,
		    dma->dp_dip, size_t, space_needed);
	}

	/*
	 * we allocate copy buffer state and window state at the same time.
	 * setup our copy buffer state pointers. Make sure it's aligned.
	 */
	if (dma->dp_copybuf_size > 0) {
		dma->dp_pgmap = (rootnex_pgmap_t *)(((uintptr_t)
		    &dma->dp_window[dma->dp_max_win] + 0x7) & ~0x7);

		/*
		 * make sure all pm_mapped, pm_vaddr, and pm_pp are set to
		 * false/NULL. Should be quicker to bzero vs loop and set.
		 */
		bzero(dma->dp_pgmap, copy_state_size);
	} else {
		dma->dp_pgmap = NULL;
	}

	return (DDI_SUCCESS);
}


/*
 * rootnex_teardown_copybuf()
 *    cleans up after rootnex_setup_copybuf()
 */
static void
rootnex_teardown_copybuf(rootnex_dma_t *dma)
{
	int i;

	/*
	 * if we allocated kernel heap VMEM space, go through all the pages and
	 * map out any of the ones that we're mapped into the kernel heap VMEM
	 * arena. Then free the VMEM space.
	 */
	if (dma->dp_kva != NULL) {
		for (i = 0; i < dma->dp_sglinfo.si_max_pages; i++) {
			if (dma->dp_pgmap[i].pm_mapped) {
				hat_unload(kas.a_hat, dma->dp_pgmap[i].pm_kaddr,
				    MMU_PAGESIZE, HAT_UNLOAD);
				dma->dp_pgmap[i].pm_mapped = RB_FALSE;
			}
		}

		vmem_free(heap_arena, dma->dp_kva, dma->dp_copybuf_size);
	}


	/* if we allocated a copy buffer, free it */
	if (dma->dp_cbaddr != NULL) {
		i_ddi_mem_free(dma->dp_cbaddr, NULL);
	}
}


/*
 * rootnex_teardown_windows()
 *    cleans up after rootnex_setup_windows()
 */
static void
rootnex_teardown_windows(rootnex_dma_t *dma)
{
	/*
	 * if we had to allocate window state on the last bind (because we
	 * didn't have enough pre-allocated space in the handle), free it.
	 */
	if (dma->dp_need_to_free_window) {
		kmem_free(dma->dp_window, dma->dp_window_size);
	}
}


/*
 * rootnex_init_win()
 *    Called in bind slow path during creation of a new window. Initializes
 *    window state to default values.
 */
/*ARGSUSED*/
static void
rootnex_init_win(ddi_dma_impl_t *hp, rootnex_dma_t *dma,
    rootnex_window_t *window, ddi_dma_cookie_t *cookie, off_t cur_offset)
{
	hp->dmai_nwin++;
	window->wd_dosync = RB_FALSE;
	window->wd_offset = cur_offset;
	window->wd_size = 0;
	window->wd_first_cookie = cookie;
	window->wd_cookie_cnt = 0;
	window->wd_trim.tr_trim_first = RB_FALSE;
	window->wd_trim.tr_trim_last = RB_FALSE;
	window->wd_trim.tr_first_copybuf_win = RB_FALSE;
	window->wd_trim.tr_last_copybuf_win = RB_FALSE;
	window->wd_remap_copybuf = dma->dp_cb_remaping;
}


/*
 * rootnex_setup_cookie()
 *    Called in the bind slow path when the sgl uses the copy buffer. If any of
 *    the sgl uses the copy buffer, we need to go through each cookie, figure
 *    out if it uses the copy buffer, and if it does, save away everything we'll
 *    need during sync.
 */
static void
rootnex_setup_cookie(ddi_dma_obj_t *dmar_object, rootnex_dma_t *dma,
    ddi_dma_cookie_t *cookie, off_t cur_offset, size_t *copybuf_used,
    page_t **cur_pp)
{
	boolean_t copybuf_sz_power_2;
	rootnex_sglinfo_t *sinfo;
	uint_t pidx;
	uint_t pcnt;
	off_t poff;
	page_t **pplist;
	pfn_t pfn;

	sinfo = &dma->dp_sglinfo;

	/*
	 * Calculate the page index relative to the start of the buffer. The
	 * index to the current page for our buffer is the offset into the
	 * first page of the buffer plus our current offset into the buffer
	 * itself, shifted of course...
	 */
	pidx = (sinfo->si_buf_offset + cur_offset) >> MMU_PAGESHIFT;
	ASSERT(pidx < sinfo->si_max_pages);

	/* if this cookie uses the copy buffer */
	if (cookie->dmac_type & ROOTNEX_USES_COPYBUF) {
		/*
		 * NOTE: we know that since this cookie uses the copy buffer, it
		 * is <= MMU_PAGESIZE.
		 */

		/*
		 * get the offset into the page.
		 */
		poff = cookie->dmac_laddress & MMU_PAGEOFFSET;

		/* figure out if the copybuf size is a power of 2 */
		if (dma->dp_copybuf_size & (dma->dp_copybuf_size - 1)) {
			copybuf_sz_power_2 = B_FALSE;
		} else {
			copybuf_sz_power_2 = B_TRUE;
		}

		/* This page uses the copy buffer */
		dma->dp_pgmap[pidx].pm_uses_copybuf = RB_TRUE;

		/*
		 * save the copy buffer KVA that we'll use with this page.
		 * if we still fit within the copybuf, it's a simple add.
		 * otherwise, we need to wrap over using & or % accordingly.
		 */
		if ((*copybuf_used + MMU_PAGESIZE) <= dma->dp_copybuf_size) {
			dma->dp_pgmap[pidx].pm_cbaddr = dma->dp_cbaddr +
			    *copybuf_used;
		} else {
			if (copybuf_sz_power_2) {
				dma->dp_pgmap[pidx].pm_cbaddr = (caddr_t)(
				    (uintptr_t)dma->dp_cbaddr +
				    (*copybuf_used &
				    (dma->dp_copybuf_size - 1)));
			} else {
				dma->dp_pgmap[pidx].pm_cbaddr = (caddr_t)(
				    (uintptr_t)dma->dp_cbaddr +
				    (*copybuf_used % dma->dp_copybuf_size));
			}
		}

		/*
		 * over write the cookie physical address with the address of
		 * the physical address of the copy buffer page that we will
		 * use.
		 */
		pfn = hat_getpfnum(kas.a_hat, dma->dp_pgmap[pidx].pm_cbaddr);
		ASSERT(pfn <= ROOTNEX_MAX_PFN);
		cookie->dmac_laddress = ptob64(pfn) + poff;

		/* if we have a kernel VA, it's easy, just save that address */
		if ((dmar_object->dmao_type != DMA_OTYP_PAGES) &&
		    (sinfo->si_asp == &kas)) {
			/*
			 * save away the page aligned virtual address of the
			 * driver buffer. Offsets are handled in the sync code.
			 */
			dma->dp_pgmap[pidx].pm_kaddr = (caddr_t)(((uintptr_t)
			    dmar_object->dmao_obj.virt_obj.v_addr + cur_offset)
			    & MMU_PAGEMASK);
			/*
			 * we didn't need to, and will never need to map this
			 * page.
			 */
			dma->dp_pgmap[pidx].pm_mapped = RB_FALSE;

		/* we don't have a kernel VA. We need one for the bcopy. */
		} else {
			/*
			 * for the 32-bit kernel, this is a pain. First we'll
			 * save away the page_t or user VA for this page. This
			 * is needed in rootnex_dma_win() when we switch to a
			 * new window which requires us to re-map the copy
			 * buffer.
			 */
			pplist = dmar_object->dmao_obj.virt_obj.v_priv;
			if (dmar_object->dmao_type == DMA_OTYP_PAGES) {
				dma->dp_pgmap[pidx].pm_pp = *cur_pp;
				dma->dp_pgmap[pidx].pm_vaddr = NULL;
			} else if (pplist != NULL) {
				dma->dp_pgmap[pidx].pm_pp = pplist[pidx];
				dma->dp_pgmap[pidx].pm_vaddr = NULL;
			} else {
				dma->dp_pgmap[pidx].pm_pp = NULL;
				dma->dp_pgmap[pidx].pm_vaddr = (caddr_t)
				    (((uintptr_t)
				    dmar_object->dmao_obj.virt_obj.v_addr +
				    cur_offset) & MMU_PAGEMASK);
			}

			/*
			 * save away the page aligned virtual address which was
			 * allocated from the kernel heap arena (taking into
			 * account if we need more copy buffer than we alloced
			 * and use multiple windows to handle this, i.e. &,%).
			 * NOTE: there isn't and physical memory backing up this
			 * virtual address space currently.
			 */
			if ((*copybuf_used + MMU_PAGESIZE) <=
			    dma->dp_copybuf_size) {
				dma->dp_pgmap[pidx].pm_kaddr = (caddr_t)
				    (((uintptr_t)dma->dp_kva + *copybuf_used) &
				    MMU_PAGEMASK);
			} else {
				if (copybuf_sz_power_2) {
					dma->dp_pgmap[pidx].pm_kaddr = (caddr_t)
					    (((uintptr_t)dma->dp_kva +
					    (*copybuf_used &
					    (dma->dp_copybuf_size - 1))) &
					    MMU_PAGEMASK);
				} else {
					dma->dp_pgmap[pidx].pm_kaddr = (caddr_t)
					    (((uintptr_t)dma->dp_kva +
					    (*copybuf_used %
					    dma->dp_copybuf_size)) &
					    MMU_PAGEMASK);
				}
			}

			/*
			 * if we haven't used up the available copy buffer yet,
			 * map the kva to the physical page.
			 */
			if (!dma->dp_cb_remaping && ((*copybuf_used +
			    MMU_PAGESIZE) <= dma->dp_copybuf_size)) {
				dma->dp_pgmap[pidx].pm_mapped = RB_TRUE;
				if (dma->dp_pgmap[pidx].pm_pp != NULL) {
					armpf_pp_map(dma->dp_pgmap[pidx].pm_pp,
					    dma->dp_pgmap[pidx].pm_kaddr);
				} else {
					armpf_va_map(dma->dp_pgmap[pidx].pm_vaddr,
					    sinfo->si_asp,
					    dma->dp_pgmap[pidx].pm_kaddr);
				}

			/*
			 * we've used up the available copy buffer, this page
			 * will have to be mapped during rootnex_dma_win() when
			 * we switch to a new window which requires a re-map
			 * the copy buffer. (32-bit kernel only)
			 */
			} else {
				dma->dp_pgmap[pidx].pm_mapped = RB_FALSE;
			}
			/* go to the next page_t */
			if (dmar_object->dmao_type == DMA_OTYP_PAGES) {
				*cur_pp = (*cur_pp)->p_next;
			}
		}

		/* add to the copy buffer count */
		*copybuf_used += MMU_PAGESIZE;

	/*
	 * This cookie doesn't use the copy buffer. Walk through the pages this
	 * cookie occupies to reflect this.
	 */
	} else {
		/*
		 * figure out how many pages the cookie occupies. We need to
		 * use the original page offset of the buffer and the cookies
		 * offset in the buffer to do this.
		 */
		poff = (sinfo->si_buf_offset + cur_offset) & MMU_PAGEOFFSET;
		pcnt = mmu_btopr(cookie->dmac_size + poff);

		while (pcnt > 0) {
			/*
			 * the 32-bit kernel doesn't have seg kpm, so we need
			 * to map in the driver buffer (if it didn't come down
			 * with a kernel VA) on the fly. Since this page doesn't
			 * use the copy buffer, it's not, or will it ever, have
			 * to be mapped in.
			 */
			dma->dp_pgmap[pidx].pm_mapped = RB_FALSE;
			dma->dp_pgmap[pidx].pm_uses_copybuf = RB_FALSE;

			/*
			 * we need to update pidx and cur_pp or we'll loose
			 * track of where we are.
			 */
			if (dmar_object->dmao_type == DMA_OTYP_PAGES) {
				*cur_pp = (*cur_pp)->p_next;
			}
			pidx++;
			pcnt--;
		}
	}
}


/*
 * rootnex_sgllen_window_boundary()
 *    Called in the bind slow path when the next cookie causes us to exceed (in
 *    this case == since we start at 0 and sgllen starts at 1) the maximum sgl
 *    length supported by the DMA H/W.
 */
static int
rootnex_sgllen_window_boundary(ddi_dma_impl_t *hp, rootnex_dma_t *dma,
    rootnex_window_t **windowp, ddi_dma_cookie_t *cookie, ddi_dma_attr_t *attr,
    off_t cur_offset)
{
	off_t new_offset;
	size_t trim_sz;
	off_t coffset;


	/*
	 * if we know we'll never have to trim, it's pretty easy. Just move to
	 * the next window and init it. We're done.
	 */
	if (!dma->dp_trim_required) {
		(*windowp)++;
		rootnex_init_win(hp, dma, *windowp, cookie, cur_offset);
		(*windowp)->wd_cookie_cnt++;
		(*windowp)->wd_size = cookie->dmac_size;
		return (DDI_SUCCESS);
	}

	/* figure out how much we need to trim from the window */
	ASSERT(attr->dma_attr_granular != 0);
	if (dma->dp_granularity_power_2) {
		trim_sz = (*windowp)->wd_size & (attr->dma_attr_granular - 1);
	} else {
		trim_sz = (*windowp)->wd_size % attr->dma_attr_granular;
	}

	/* The window's a whole multiple of granularity. We're done */
	if (trim_sz == 0) {
		(*windowp)++;
		rootnex_init_win(hp, dma, *windowp, cookie, cur_offset);
		(*windowp)->wd_cookie_cnt++;
		(*windowp)->wd_size = cookie->dmac_size;
		return (DDI_SUCCESS);
	}

	/*
	 * The window's not a whole multiple of granularity, since we know this
	 * is due to the sgllen, we need to go back to the last cookie and trim
	 * that one, add the left over part of the old cookie into the new
	 * window, and then add in the new cookie into the new window.
	 */

	/*
	 * make sure the driver isn't making us do something bad... Trimming and
	 * sgllen == 1 don't go together.
	 */
	if (attr->dma_attr_sgllen == 1) {
		return (DDI_DMA_NOMAPPING);
	}

	/*
	 * first, setup the current window to account for the trim. Need to go
	 * back to the last cookie for this.
	 */
	cookie--;
	(*windowp)->wd_trim.tr_trim_last = RB_TRUE;
	(*windowp)->wd_trim.tr_last_cookie = cookie;
	(*windowp)->wd_trim.tr_last_paddr = (uint32_t)cookie->dmac_laddress;
	ASSERT(cookie->dmac_size > trim_sz);
	(*windowp)->wd_trim.tr_last_size = cookie->dmac_size - trim_sz;
	(*windowp)->wd_size -= trim_sz;

	/* save the buffer offsets for the next window */
	coffset = cookie->dmac_size - trim_sz;
	new_offset = (*windowp)->wd_offset + (*windowp)->wd_size;

	/*
	 * set this now in case this is the first window. all other cases are
	 * set in dma_win()
	 */
	cookie->dmac_size = (*windowp)->wd_trim.tr_last_size;

	/*
	 * initialize the next window using what's left over in the previous
	 * cookie.
	 */
	(*windowp)++;
	rootnex_init_win(hp, dma, *windowp, cookie, new_offset);
	(*windowp)->wd_cookie_cnt++;
	(*windowp)->wd_trim.tr_trim_first = RB_TRUE;
	(*windowp)->wd_trim.tr_first_paddr =
		(uint32_t)cookie->dmac_laddress + coffset;
	(*windowp)->wd_trim.tr_first_size = trim_sz;
	if (cookie->dmac_type & ROOTNEX_USES_COPYBUF) {
		(*windowp)->wd_dosync = RB_TRUE;
	}

	/*
	 * now go back to the current cookie and add it to the new window. set
	 * the new window size to the what was left over from the previous
	 * cookie and what's in the current cookie.
	 */
	cookie++;
	(*windowp)->wd_cookie_cnt++;
	(*windowp)->wd_size = trim_sz + cookie->dmac_size;

	/*
	 * trim plus the next cookie could put us over maxxfer (a cookie can be
	 * a max size of maxxfer). Handle that case.
	 */
	if ((*windowp)->wd_size > dma->dp_maxxfer) {
		/*
		 * maxxfer is already a whole multiple of granularity, and this
		 * trim will be <= the previous trim (since a cookie can't be
		 * larger than maxxfer). Make things simple here.
		 */
		trim_sz = (*windowp)->wd_size - dma->dp_maxxfer;
		(*windowp)->wd_trim.tr_trim_last = RB_TRUE;
		(*windowp)->wd_trim.tr_last_cookie = cookie;
		(*windowp)->wd_trim.tr_last_paddr =
			(uint32_t)cookie->dmac_laddress;
		(*windowp)->wd_trim.tr_last_size = cookie->dmac_size - trim_sz;
		(*windowp)->wd_size -= trim_sz;
		ASSERT((*windowp)->wd_size == dma->dp_maxxfer);

		/* save the buffer offsets for the next window */
		coffset = cookie->dmac_size - trim_sz;
		new_offset = (*windowp)->wd_offset + (*windowp)->wd_size;

		/* setup the next window */
		(*windowp)++;
		rootnex_init_win(hp, dma, *windowp, cookie, new_offset);
		(*windowp)->wd_cookie_cnt++;
		(*windowp)->wd_trim.tr_trim_first = RB_TRUE;
		(*windowp)->wd_trim.tr_first_paddr =
			(uint32_t)cookie->dmac_laddress +
		    coffset;
		(*windowp)->wd_trim.tr_first_size = trim_sz;
	}

	return (DDI_SUCCESS);
}


/*
 * rootnex_copybuf_window_boundary()
 *    Called in bind slowpath when we get to a window boundary because we used
 *    up all the copy buffer that we have.
 */
static int
rootnex_copybuf_window_boundary(ddi_dma_impl_t *hp, rootnex_dma_t *dma,
    rootnex_window_t **windowp, ddi_dma_cookie_t *cookie, off_t cur_offset,
    size_t *copybuf_used)
{
	rootnex_sglinfo_t *sinfo;
	off_t new_offset;
	size_t trim_sz;
	off_t coffset;
	uint_t pidx;
	off_t poff;


	sinfo = &dma->dp_sglinfo;

	/*
	 * the copy buffer should be a whole multiple of page size. We know that
	 * this cookie is <= MMU_PAGESIZE.
	 */
	ASSERT(cookie->dmac_size <= MMU_PAGESIZE);

	/*
	 * from now on, all new windows in this bind need to be re-mapped during
	 * ddi_dma_getwin() (32-bit kernel only). i.e. we ran out out copybuf
	 * space...
	 */
	dma->dp_cb_remaping = RB_TRUE;

	/* reset copybuf used */
	*copybuf_used = 0;

	/*
	 * if we don't have to trim (since granularity is set to 1), go to the
	 * next window and add the current cookie to it. We know the current
	 * cookie uses the copy buffer since we're in this code path.
	 */
	if (!dma->dp_trim_required) {
		(*windowp)++;
		rootnex_init_win(hp, dma, *windowp, cookie, cur_offset);

		/* Add this cookie to the new window */
		(*windowp)->wd_cookie_cnt++;
		(*windowp)->wd_size += cookie->dmac_size;
		*copybuf_used += MMU_PAGESIZE;
		return (DDI_SUCCESS);
	}

	/*
	 * *** may need to trim, figure it out.
	 */

	/* figure out how much we need to trim from the window */
	if (dma->dp_granularity_power_2) {
		trim_sz = (*windowp)->wd_size &
		    (hp->dmai_attr.dma_attr_granular - 1);
	} else {
		trim_sz = (*windowp)->wd_size % hp->dmai_attr.dma_attr_granular;
	}

	/*
	 * if the window's a whole multiple of granularity, go to the next
	 * window, init it, then add in the current cookie. We know the current
	 * cookie uses the copy buffer since we're in this code path.
	 */
	if (trim_sz == 0) {
		(*windowp)++;
		rootnex_init_win(hp, dma, *windowp, cookie, cur_offset);

		/* Add this cookie to the new window */
		(*windowp)->wd_cookie_cnt++;
		(*windowp)->wd_size += cookie->dmac_size;
		*copybuf_used += MMU_PAGESIZE;
		return (DDI_SUCCESS);
	}

	/*
	 * *** We figured it out, we definitly need to trim
	 */

	/*
	 * make sure the driver isn't making us do something bad...
	 * Trimming and sgllen == 1 don't go together.
	 */
	if (hp->dmai_attr.dma_attr_sgllen == 1) {
		return (DDI_DMA_NOMAPPING);
	}

	/*
	 * first, setup the current window to account for the trim. Need to go
	 * back to the last cookie for this. Some of the last cookie will be in
	 * the current window, and some of the last cookie will be in the new
	 * window. All of the current cookie will be in the new window.
	 */
	cookie--;
	(*windowp)->wd_trim.tr_trim_last = RB_TRUE;
	(*windowp)->wd_trim.tr_last_cookie = cookie;
	(*windowp)->wd_trim.tr_last_paddr = (uint32_t)cookie->dmac_laddress;
	ASSERT(cookie->dmac_size > trim_sz);
	(*windowp)->wd_trim.tr_last_size = cookie->dmac_size - trim_sz;
	(*windowp)->wd_size -= trim_sz;

	/*
	 * we're trimming the last cookie (not the current cookie). So that
	 * last cookie may have or may not have been using the copy buffer (
	 * we know the cookie passed in uses the copy buffer since we're in
	 * this code path).
	 *
	 * If the last cookie doesn't use the copy buffer, nothing special to
	 * do. However, if it does uses the copy buffer, it will be both the
	 * last page in the current window and the first page in the next
	 * window. Since we are reusing the copy buffer (and KVA space on the
	 * 32-bit kernel), this page will use the end of the copy buffer in the
	 * current window, and the start of the copy buffer in the next window.
	 * Track that info... The cookie physical address was already set to
	 * the copy buffer physical address in setup_cookie..
	 */
	if (cookie->dmac_type & ROOTNEX_USES_COPYBUF) {
		pidx = (sinfo->si_buf_offset + (*windowp)->wd_offset +
		    (*windowp)->wd_size) >> MMU_PAGESHIFT;
		(*windowp)->wd_trim.tr_last_copybuf_win = RB_TRUE;
		(*windowp)->wd_trim.tr_last_pidx = pidx;
		(*windowp)->wd_trim.tr_last_cbaddr =
		    dma->dp_pgmap[pidx].pm_cbaddr;
		(*windowp)->wd_trim.tr_last_kaddr =
		    dma->dp_pgmap[pidx].pm_kaddr;
	}

	/* save the buffer offsets for the next window */
	coffset = cookie->dmac_size - trim_sz;
	new_offset = (*windowp)->wd_offset + (*windowp)->wd_size;

	/*
	 * set this now in case this is the first window. all other cases are
	 * set in dma_win()
	 */
	cookie->dmac_size = (*windowp)->wd_trim.tr_last_size;

	/*
	 * initialize the next window using what's left over in the previous
	 * cookie.
	 */
	(*windowp)++;
	rootnex_init_win(hp, dma, *windowp, cookie, new_offset);
	(*windowp)->wd_cookie_cnt++;
	(*windowp)->wd_trim.tr_trim_first = RB_TRUE;
	(*windowp)->wd_trim.tr_first_paddr =
		(uint32_t)cookie->dmac_laddress + coffset;
	(*windowp)->wd_trim.tr_first_size = trim_sz;

	/*
	 * again, we're tracking if the last cookie uses the copy buffer.
	 * read the comment above for more info on why we need to track
	 * additional state.
	 *
	 * For the first cookie in the new window, we need reset the physical
	 * address to DMA into to the start of the copy buffer plus any
	 * initial page offset which may be present.
	 */
	if (cookie->dmac_type & ROOTNEX_USES_COPYBUF) {
		pfn_t	pfn;

		(*windowp)->wd_dosync = RB_TRUE;
		(*windowp)->wd_trim.tr_first_copybuf_win = RB_TRUE;
		(*windowp)->wd_trim.tr_first_pidx = pidx;
		(*windowp)->wd_trim.tr_first_cbaddr = dma->dp_cbaddr;
		poff = (*windowp)->wd_trim.tr_first_paddr & MMU_PAGEOFFSET;
		pfn = hat_getpfnum(kas.a_hat, dma->dp_cbaddr);
		ASSERT(pfn <= ROOTNEX_MAX_PFN);
		(*windowp)->wd_trim.tr_first_paddr = ptob(pfn) + poff;
		(*windowp)->wd_trim.tr_first_kaddr = dma->dp_kva;
		/* account for the cookie copybuf usage in the new window */
		*copybuf_used += MMU_PAGESIZE;

		/*
		 * every piece of code has to have a hack, and here is this
		 * ones :-)
		 *
		 * There is a complex interaction between setup_cookie and the
		 * copybuf window boundary. The complexity had to be in either
		 * the maxxfer window, or the copybuf window, and I chose the
		 * copybuf code.
		 *
		 * So in this code path, we have taken the last cookie,
		 * virtually broken it in half due to the trim, and it happens
		 * to use the copybuf which further complicates life. At the
		 * same time, we have already setup the current cookie, which
		 * is now wrong. More background info: the current cookie uses
		 * the copybuf, so it is only a page long max. So we need to
		 * fix the current cookies copy buffer address, physical
		 * address, and kva for the 32-bit kernel. We due this by
		 * bumping them by page size (of course, we can't due this on
		 * the physical address since the copy buffer may not be
		 * physically contiguous).
		 */
		cookie++;
		dma->dp_pgmap[pidx + 1].pm_cbaddr += MMU_PAGESIZE;
		poff = cookie->dmac_laddress & MMU_PAGEOFFSET;
		pfn = hat_getpfnum(kas.a_hat,
				   dma->dp_pgmap[pidx + 1].pm_cbaddr);
		ASSERT(pfn <= ROOTNEX_MAX_PFN);
		cookie->dmac_laddress = ptob64(pfn) + poff;
		ASSERT(dma->dp_pgmap[pidx + 1].pm_mapped == RB_FALSE);
		dma->dp_pgmap[pidx + 1].pm_kaddr += MMU_PAGESIZE;
	} else {
		/* go back to the current cookie */
		cookie++;
	}

	/*
	 * add the current cookie to the new window. set the new window size to
	 * the what was left over from the previous cookie and what's in the
	 * current cookie.
	 */
	(*windowp)->wd_cookie_cnt++;
	(*windowp)->wd_size = trim_sz + cookie->dmac_size;
	ASSERT((*windowp)->wd_size < dma->dp_maxxfer);

	/*
	 * we know that the cookie passed in always uses the copy buffer. We
	 * wouldn't be here if it didn't.
	 */
	*copybuf_used += MMU_PAGESIZE;

	return (DDI_SUCCESS);
}


/*
 * rootnex_maxxfer_window_boundary()
 *    Called in bind slowpath when we get to a window boundary because we will
 *    go over maxxfer.
 */
static int
rootnex_maxxfer_window_boundary(ddi_dma_impl_t *hp, rootnex_dma_t *dma,
    rootnex_window_t **windowp, ddi_dma_cookie_t *cookie)
{
	size_t dmac_size;
	off_t new_offset;
	size_t trim_sz;
	off_t coffset;


	/*
	 * calculate how much we have to trim off of the current cookie to equal
	 * maxxfer. We don't have to account for granularity here since our
	 * maxxfer already takes that into account.
	 */
	trim_sz = ((*windowp)->wd_size + cookie->dmac_size) - dma->dp_maxxfer;
	ASSERT(trim_sz <= cookie->dmac_size);
	ASSERT(trim_sz <= dma->dp_maxxfer);

	/* save cookie size since we need it later and we might change it */
	dmac_size = cookie->dmac_size;

	/*
	 * if we're not trimming the entire cookie, setup the current window to
	 * account for the trim.
	 */
	if (trim_sz < cookie->dmac_size) {
		(*windowp)->wd_cookie_cnt++;
		(*windowp)->wd_trim.tr_trim_last = RB_TRUE;
		(*windowp)->wd_trim.tr_last_cookie = cookie;
		(*windowp)->wd_trim.tr_last_paddr =
			(uint32_t)cookie->dmac_laddress;
		(*windowp)->wd_trim.tr_last_size = cookie->dmac_size - trim_sz;
		(*windowp)->wd_size = dma->dp_maxxfer;

		/*
		 * set the adjusted cookie size now in case this is the first
		 * window. All other windows are taken care of in get win
		 */
		cookie->dmac_size = (*windowp)->wd_trim.tr_last_size;
	}

	/*
	 * coffset is the current offset within the cookie, new_offset is the
	 * current offset with the entire buffer.
	 */
	coffset = dmac_size - trim_sz;
	new_offset = (*windowp)->wd_offset + (*windowp)->wd_size;

	/* initialize the next window */
	(*windowp)++;
	rootnex_init_win(hp, dma, *windowp, cookie, new_offset);
	(*windowp)->wd_cookie_cnt++;
	(*windowp)->wd_size = trim_sz;
	if (trim_sz < dmac_size) {
		(*windowp)->wd_trim.tr_trim_first = RB_TRUE;
		(*windowp)->wd_trim.tr_first_paddr =
			(uint32_t)cookie->dmac_laddress + coffset;
		(*windowp)->wd_trim.tr_first_size = trim_sz;
	}

	return (DDI_SUCCESS);
}


/*
 * rootnex_dma_sync()
 *    called from ddi_dma_sync() if DMP_NOSYNC is not set in hp->dmai_rflags.
 *    We set DMP_NOSYNC if we're not using the copy buffer. If DMP_NOSYNC
 *    is set, ddi_dma_sync() returns immediately passing back success.
 */
/*ARGSUSED*/
static int
rootnex_dma_sync(dev_info_t *dip, dev_info_t *rdip, ddi_dma_handle_t handle,
    off_t off, size_t len, uint_t cache_flags)
{
	rootnex_sglinfo_t *sinfo;
	rootnex_pgmap_t *cbpage;
	rootnex_window_t *win;
	ddi_dma_impl_t *hp;
	rootnex_dma_t *dma;
	caddr_t fromaddr;
	caddr_t toaddr;
	uint_t psize;
	off_t offset;
	uint_t pidx;
	size_t size;
	off_t poff;
	int e;


	hp = (ddi_dma_impl_t *)handle;
	dma = (rootnex_dma_t *)hp->dmai_private;
	sinfo = &dma->dp_sglinfo;

	/*
	 * if we don't have any windows, we don't need to sync. A copybuf
	 * will cause us to have at least one window.
	 */
	if (dma->dp_window == NULL) {
		ARMPF_DMA_SYNC_HANDLE(handle, off, len, cache_flags);
		return (DDI_SUCCESS);
	}

	/* This window may not need to be sync'd */
	win = &dma->dp_window[dma->dp_current_win];
	if (!win->wd_dosync) {
		ARMPF_DMA_SYNC_HANDLE(handle, off, len, cache_flags);
		return (DDI_SUCCESS);
	}

	/* handle off and len special cases */
	if ((off == 0) || (ROOTNEX_SYNC_IGNORE_PARAMS)) {
		offset = win->wd_offset;
	} else {
		offset = off;
	}
	if ((len == 0) || (ROOTNEX_SYNC_IGNORE_PARAMS)) {
		size = win->wd_size;
	} else {
		size = len;
	}

	/* check the sync args to make sure they make a little sense */
	if (rootnex_sync_check_parms) {
		e = rootnex_valid_sync_parms(hp, win, offset, size,
		    cache_flags);
		if (e != DDI_SUCCESS) {
			ROOTNEX_PROF_INC(&rootnex_cnt[ROOTNEX_CNT_SYNC_FAIL]);
			return (DDI_FAILURE);
		}
	}

	/*
	 * special case the first page to handle the offset into the page. The
	 * offset to the current page for our buffer is the offset into the
	 * first page of the buffer plus our current offset into the buffer
	 * itself, masked of course.
	 */
	poff = (sinfo->si_buf_offset + offset) & MMU_PAGEOFFSET;
	psize = MIN((MMU_PAGESIZE - poff), size);

	ARMPF_DMA_COPYBUF_SYNC_INIT(handle)

	/* go through all the pages that we want to sync */
	while (size > 0) {
		/*
		 * Calculate the page index relative to the start of the buffer.
		 * The index to the current page for our buffer is the offset
		 * into the first page of the buffer plus our current offset
		 * into the buffer itself, shifted of course...
		 */
		pidx = (sinfo->si_buf_offset + offset) >> MMU_PAGESHIFT;
		ASSERT(pidx < sinfo->si_max_pages);

		/*
		 * if this page uses the copy buffer, we need to sync it,
		 * otherwise, go on to the next page.
		 */
		cbpage = &dma->dp_pgmap[pidx];
		ASSERT((cbpage->pm_uses_copybuf == RB_TRUE) ||
		    (cbpage->pm_uses_copybuf == RB_FALSE));
		if (cbpage->pm_uses_copybuf) {
			/* cbaddr and kaddr should be page aligned */
			ASSERT(((uintptr_t)cbpage->pm_cbaddr &
			    MMU_PAGEOFFSET) == 0);
			ASSERT(((uintptr_t)cbpage->pm_kaddr &
			    MMU_PAGEOFFSET) == 0);

			/*
			 * if we're copying for the device, we are going to
			 * copy from the drivers buffer and to the rootnex
			 * allocated copy buffer.
			 */
			if (cache_flags == DDI_DMA_SYNC_FORDEV) {
				fromaddr = cbpage->pm_kaddr + poff;
				toaddr = cbpage->pm_cbaddr + poff;
				DTRACE_PROBE2(rootnex__sync__dev,
				    dev_info_t *, dma->dp_dip, size_t, psize);

			/*
			 * if we're copying for the cpu/kernel, we are going to
			 * copy from the rootnex allocated copy buffer to the
			 * drivers buffer.
			 */
			} else {
				fromaddr = cbpage->pm_cbaddr + poff;
				toaddr = cbpage->pm_kaddr + poff;
				DTRACE_PROBE2(rootnex__sync__cpu,
				    dev_info_t *, dma->dp_dip, size_t, psize);
			}

			FAST_BCOPY(fromaddr, toaddr, psize);
		}

		/*
		 * decrement size until we're done, update our offset into the
		 * buffer, and get the next page size.
		 */
		size -= psize;
		offset += psize;
		psize = MIN(MMU_PAGESIZE, size);

		/* page offset is zero for the rest of this loop */
		poff = 0;
	}

	ARMPF_DMA_COPYBUF_SYNC_FINI(handle)

	return (DDI_SUCCESS);
}


/*
 * rootnex_valid_sync_parms()
 *    checks the parameters passed to sync to verify they are correct.
 */
static int
rootnex_valid_sync_parms(ddi_dma_impl_t *hp, rootnex_window_t *win,
    off_t offset, size_t size, uint_t cache_flags)
{
	off_t woffset;


	/*
	 * the first part of the test to make sure the offset passed in is
	 * within the window.
	 */
	if (offset < win->wd_offset) {
		return (DDI_FAILURE);
	}

	/*
	 * second and last part of the test to make sure the offset and length
	 * passed in is within the window.
	 */
	woffset = offset - win->wd_offset;
	if ((woffset + size) > win->wd_size) {
		return (DDI_FAILURE);
	}

	/*
	 * if we are sync'ing for the device, the DDI_DMA_WRITE flag should
	 * be set too.
	 */
	if ((cache_flags == DDI_DMA_SYNC_FORDEV) &&
	    (hp->dmai_rflags & DDI_DMA_WRITE)) {
		return (DDI_SUCCESS);
	}

	/*
	 * at this point, either DDI_DMA_SYNC_FORCPU or DDI_DMA_SYNC_FORKERNEL
	 * should be set. Also DDI_DMA_READ should be set in the flags.
	 */
	if (((cache_flags == DDI_DMA_SYNC_FORCPU) ||
	    (cache_flags == DDI_DMA_SYNC_FORKERNEL)) &&
	    (hp->dmai_rflags & DDI_DMA_READ)) {
		return (DDI_SUCCESS);
	}

	return (DDI_FAILURE);
}


/*
 * rootnex_dma_win()
 *    called from ddi_dma_getwin()
 */
/*ARGSUSED*/
static int
rootnex_dma_win(dev_info_t *dip, dev_info_t *rdip, ddi_dma_handle_t handle,
    uint_t win, off_t *offp, size_t *lenp, ddi_dma_cookie_t *cookiep,
    uint_t *ccountp)
{
	rootnex_window_t *window;
	rootnex_trim_t *trim;
	ddi_dma_impl_t *hp;
	rootnex_dma_t *dma;
	rootnex_sglinfo_t *sinfo;
	rootnex_pgmap_t *pmap;
	uint_t pidx;
	uint_t pcnt;
	off_t poff;
	int i;


	hp = (ddi_dma_impl_t *)handle;
	dma = (rootnex_dma_t *)hp->dmai_private;
	sinfo = &dma->dp_sglinfo;

	/* If we try and get a window which doesn't exist, return failure */
	if (win >= hp->dmai_nwin) {
		ROOTNEX_PROF_INC(&rootnex_cnt[ROOTNEX_CNT_GETWIN_FAIL]);
		return (DDI_FAILURE);
	}

	/*
	 * if we don't have any windows, and they're asking for the first
	 * window, setup the cookie pointer to the first cookie in the bind.
	 * setup our return values, then increment the cookie since we return
	 * the first cookie on the stack.
	 */
	if (dma->dp_window == NULL) {
		if (win != 0) {
			ROOTNEX_PROF_INC(&rootnex_cnt[ROOTNEX_CNT_GETWIN_FAIL]);
			return (DDI_FAILURE);
		}
		hp->dmai_cookie = dma->dp_cookies;
		*offp = 0;
		*lenp = dma->dp_dma.dmao_size;
		*ccountp = dma->dp_sglinfo.si_sgl_size;
		*cookiep = hp->dmai_cookie[0];
		hp->dmai_cookie++;
		return (DDI_SUCCESS);
	}

	/* sync the old window before moving on to the new one */
	window = &dma->dp_window[dma->dp_current_win];
	if ((window->wd_dosync) && (hp->dmai_rflags & DDI_DMA_READ)) {
		(void) rootnex_dma_sync(dip, rdip, handle, 0, 0,
		    DDI_DMA_SYNC_FORCPU);
	}

	/*
	 * before we move to the next window, if we need to re-map, unmap all
	 * the pages in this window.
	 */
	if (dma->dp_cb_remaping) {
		/*
		 * If we switch to this window again, we'll need to map in
		 * on the fly next time.
		 */
		window->wd_remap_copybuf = RB_TRUE;

		/*
		 * calculate the page index into the buffer where this window
		 * starts, and the number of pages this window takes up.
		 */
		pidx = (sinfo->si_buf_offset + window->wd_offset) >>
		    MMU_PAGESHIFT;
		poff = (sinfo->si_buf_offset + window->wd_offset) &
		    MMU_PAGEOFFSET;
		pcnt = mmu_btopr(window->wd_size + poff);
		ASSERT((pidx + pcnt) <= sinfo->si_max_pages);

		/* unmap pages which are currently mapped in this window */
		for (i = 0; i < pcnt; i++) {
			if (dma->dp_pgmap[pidx].pm_mapped) {
				hat_unload(kas.a_hat,
				    dma->dp_pgmap[pidx].pm_kaddr, MMU_PAGESIZE,
				    HAT_UNLOAD);
				dma->dp_pgmap[pidx].pm_mapped = RB_FALSE;
			}
			pidx++;
		}
	}

	/*
	 * Move to the new window.
	 * NOTE: current_win must be set for sync to work right
	 */
	dma->dp_current_win = win;
	window = &dma->dp_window[win];

	/* if needed, adjust the first and/or last cookies for trim */
	trim = &window->wd_trim;
	if (trim->tr_trim_first) {
		window->wd_first_cookie->dmac_laddress =
			(uint64_t)trim->tr_first_paddr;
		window->wd_first_cookie->dmac_size = trim->tr_first_size;
		window->wd_first_cookie->dmac_type =
		    (window->wd_first_cookie->dmac_type &
		    ROOTNEX_USES_COPYBUF) + window->wd_offset;
		if (trim->tr_first_copybuf_win) {
			dma->dp_pgmap[trim->tr_first_pidx].pm_cbaddr =
			    trim->tr_first_cbaddr;
			dma->dp_pgmap[trim->tr_first_pidx].pm_kaddr =
			    trim->tr_first_kaddr;
		}
	}
	if (trim->tr_trim_last) {
		trim->tr_last_cookie->dmac_laddress =
			(uint64_t)trim->tr_last_paddr;
		trim->tr_last_cookie->dmac_size = trim->tr_last_size;
		if (trim->tr_last_copybuf_win) {
			dma->dp_pgmap[trim->tr_last_pidx].pm_cbaddr =
			    trim->tr_last_cbaddr;
			dma->dp_pgmap[trim->tr_last_pidx].pm_kaddr =
			    trim->tr_last_kaddr;
		}
	}

	/*
	 * setup the cookie pointer to the first cookie in the window. setup
	 * our return values, then increment the cookie since we return the
	 * first cookie on the stack.
	 */
	hp->dmai_cookie = window->wd_first_cookie;
	*offp = window->wd_offset;
	*lenp = window->wd_size;
	*ccountp = window->wd_cookie_cnt;
	*cookiep = hp->dmai_cookie[0];
	hp->dmai_cookie++;

	/* re-map copybuf if required for this window */
	if (dma->dp_cb_remaping) {
		/*
		 * calculate the page index into the buffer where this
		 * window starts.
		 */
		pidx = (sinfo->si_buf_offset + window->wd_offset) >>
		    MMU_PAGESHIFT;
		ASSERT(pidx < sinfo->si_max_pages);

		/*
		 * the first page can get unmapped if it's shared with the
		 * previous window. Even if the rest of this window is already
		 * mapped in, we need to still check this one.
		 */
		pmap = &dma->dp_pgmap[pidx];
		if ((pmap->pm_uses_copybuf) && (pmap->pm_mapped == RB_FALSE)) {
			if (pmap->pm_pp != NULL) {
				pmap->pm_mapped = RB_TRUE;
				armpf_pp_map(pmap->pm_pp, pmap->pm_kaddr);
			} else if (pmap->pm_vaddr != NULL) {
				pmap->pm_mapped = RB_TRUE;
				armpf_va_map(pmap->pm_vaddr, sinfo->si_asp,
				    pmap->pm_kaddr);
			}
		}
		pidx++;

		/* map in the rest of the pages if required */
		if (window->wd_remap_copybuf) {
			window->wd_remap_copybuf = RB_FALSE;

			/* figure out many pages this window takes up */
			poff = (sinfo->si_buf_offset + window->wd_offset) &
			    MMU_PAGEOFFSET;
			pcnt = mmu_btopr(window->wd_size + poff);
			ASSERT(((pidx - 1) + pcnt) <= sinfo->si_max_pages);

			/* map pages which require it */
			for (i = 1; i < pcnt; i++) {
				pmap = &dma->dp_pgmap[pidx];
				if (pmap->pm_uses_copybuf) {
					ASSERT(pmap->pm_mapped == RB_FALSE);
					if (pmap->pm_pp != NULL) {
						pmap->pm_mapped = RB_TRUE;
						armpf_pp_map(pmap->pm_pp,
						    pmap->pm_kaddr);
					} else if (pmap->pm_vaddr != NULL) {
						pmap->pm_mapped = RB_TRUE;
						armpf_va_map(pmap->pm_vaddr,
						    sinfo->si_asp,
						    pmap->pm_kaddr);
					}
				}
				pidx++;
			}
		}
	}

	/* if the new window uses the copy buffer, sync it for the device */
	if ((window->wd_dosync) && (hp->dmai_rflags & DDI_DMA_WRITE)) {
		(void) rootnex_dma_sync(dip, rdip, handle, 0, 0,
		    DDI_DMA_SYNC_FORDEV);
	}

	return (DDI_SUCCESS);
}



/*
 * ************************
 *  obsoleted dma routines
 * ************************
 */

/*
 * rootnex_dma_map()
 *    called from ddi_dma_setup()
 */
/* ARGSUSED */
static int
rootnex_dma_map(dev_info_t *dip, dev_info_t *rdip, struct ddi_dma_req *dmareq,
    ddi_dma_handle_t *handlep)
{
	ddi_dma_handle_t *lhandlep;
	ddi_dma_handle_t lhandle;
	ddi_dma_cookie_t cookie;
	ddi_dma_attr_t dma_attr;
	ddi_dma_lim_t *dma_lim;
	uint_t ccnt;
	int e;


	/*
	 * if the driver is just testing to see if it's possible to do the bind,
	 * we'll use local state. Otherwise, use the handle pointer passed in.
	 */
	if (handlep == NULL) {
		lhandlep = &lhandle;
	} else {
		lhandlep = handlep;
	}

	/* convert the limit structure to a dma_attr one */
	dma_lim = dmareq->dmar_limits;
	dma_attr.dma_attr_version = DMA_ATTR_V0;
	dma_attr.dma_attr_addr_lo = dma_lim->dlim_addr_lo;
	dma_attr.dma_attr_addr_hi = dma_lim->dlim_addr_hi;
	dma_attr.dma_attr_minxfer = dma_lim->dlim_minxfer;
	dma_attr.dma_attr_seg = dma_lim->dlim_adreg_max;
	dma_attr.dma_attr_count_max = dma_lim->dlim_ctreg_max;
	dma_attr.dma_attr_granular = dma_lim->dlim_granular;
	dma_attr.dma_attr_sgllen = dma_lim->dlim_sgllen;
	dma_attr.dma_attr_maxxfer = dma_lim->dlim_reqsize;
	dma_attr.dma_attr_burstsizes = dma_lim->dlim_burstsizes;
	dma_attr.dma_attr_align = MMU_PAGESIZE;
	dma_attr.dma_attr_flags = 0;

	e = rootnex_dma_allochdl(dip, rdip, &dma_attr, dmareq->dmar_fp,
	    dmareq->dmar_arg, lhandlep);
	if (e != DDI_SUCCESS) {
		return (e);
	}

	e = rootnex_dma_bindhdl(dip, rdip, *lhandlep, dmareq, &cookie, &ccnt);
	if ((e != DDI_DMA_MAPPED) && (e != DDI_DMA_PARTIAL_MAP)) {
		(void) rootnex_dma_freehdl(dip, rdip, *lhandlep);
		return (e);
	}

	/*
	 * if the driver is just testing to see if it's possible to do the bind,
	 * free up the local state and return the result.
	 */
	if (handlep == NULL) {
		(void) rootnex_dma_unbindhdl(dip, rdip, *lhandlep);
		(void) rootnex_dma_freehdl(dip, rdip, *lhandlep);
		if (e == DDI_DMA_MAPPED) {
			return (DDI_DMA_MAPOK);
		} else {
			return (DDI_DMA_NOMAPPING);
		}
	}

	return (e);
}


/*
 * rootnex_dma_mctl()
 *
 */
/* ARGSUSED */
static int
rootnex_dma_mctl(dev_info_t *dip, dev_info_t *rdip, ddi_dma_handle_t handle,
    enum ddi_dma_ctlops request, off_t *offp, size_t *lenp, caddr_t *objpp,
    uint_t cache_flags)
{
	ddi_dma_cookie_t lcookie;
	ddi_dma_cookie_t *cookie;
	rootnex_window_t *window;
	ddi_dma_impl_t *hp;
	rootnex_dma_t *dma;
	uint_t nwin;
	uint_t ccnt;
	size_t len;
	off_t off;
	int e;


	/*
	 * DDI_DMA_SEGTOC, DDI_DMA_NEXTSEG, and DDI_DMA_NEXTWIN are a little
	 * hacky since were optimizing for the current interfaces and so we can
	 * cleanup the mess in genunix. Hopefully we will remove the this
	 * obsoleted routines someday soon.
	 */

	switch (request) {

	case DDI_DMA_SEGTOC: /* ddi_dma_segtocookie() */
		hp = (ddi_dma_impl_t *)handle;
		cookie = (ddi_dma_cookie_t *)objpp;

		/*
		 * convert segment to cookie. We don't distinguish between the
		 * two :-)
		 */
		*cookie = *hp->dmai_cookie;
		*lenp = cookie->dmac_size;
		*offp = cookie->dmac_type & ~ROOTNEX_USES_COPYBUF;
		return (DDI_SUCCESS);

	case DDI_DMA_NEXTSEG: /* ddi_dma_nextseg() */
		hp = (ddi_dma_impl_t *)handle;
		dma = (rootnex_dma_t *)hp->dmai_private;

		if ((*lenp != NULL) && ((uintptr_t)*lenp != (uintptr_t)hp)) {
			return (DDI_DMA_STALE);
		}

		/* handle the case where we don't have any windows */
		if (dma->dp_window == NULL) {
			/*
			 * if seg == NULL, and we don't have any windows,
			 * return the first cookie in the sgl.
			 */
			if (*lenp == NULL) {
				dma->dp_current_cookie = 0;
				hp->dmai_cookie = dma->dp_cookies;
				*objpp = (caddr_t)handle;
				return (DDI_SUCCESS);

			/* if we have more cookies, go to the next cookie */
			} else {
				if ((dma->dp_current_cookie + 1) >=
				    dma->dp_sglinfo.si_sgl_size) {
					return (DDI_DMA_DONE);
				}
				dma->dp_current_cookie++;
				hp->dmai_cookie++;
				return (DDI_SUCCESS);
			}
		}

		/* We have one or more windows */
		window = &dma->dp_window[dma->dp_current_win];

		/*
		 * if seg == NULL, return the first cookie in the current
		 * window
		 */
		if (*lenp == NULL) {
			dma->dp_current_cookie = 0;
			hp->dmai_cookie = window->wd_first_cookie;

		/*
		 * go to the next cookie in the window then see if we done with
		 * this window.
		 */
		} else {
			if ((dma->dp_current_cookie + 1) >=
			    window->wd_cookie_cnt) {
				return (DDI_DMA_DONE);
			}
			dma->dp_current_cookie++;
			hp->dmai_cookie++;
		}
		*objpp = (caddr_t)handle;
		return (DDI_SUCCESS);

	case DDI_DMA_NEXTWIN: /* ddi_dma_nextwin() */
		hp = (ddi_dma_impl_t *)handle;
		dma = (rootnex_dma_t *)hp->dmai_private;

		if ((*offp != NULL) && ((uintptr_t)*offp != (uintptr_t)hp)) {
			return (DDI_DMA_STALE);
		}

		/* if win == NULL, return the first window in the bind */
		if (*offp == NULL) {
			nwin = 0;

		/*
		 * else, go to the next window then see if we're done with all
		 * the windows.
		 */
		} else {
			nwin = dma->dp_current_win + 1;
			if (nwin >= hp->dmai_nwin) {
				return (DDI_DMA_DONE);
			}
		}

		/* switch to the next window */
		e = rootnex_dma_win(dip, rdip, handle, nwin, &off, &len,
		    &lcookie, &ccnt);
		ASSERT(e == DDI_SUCCESS);
		if (e != DDI_SUCCESS) {
			return (DDI_DMA_STALE);
		}

		/* reset the cookie back to the first cookie in the window */
		if (dma->dp_window != NULL) {
			window = &dma->dp_window[dma->dp_current_win];
			hp->dmai_cookie = window->wd_first_cookie;
		} else {
			hp->dmai_cookie = dma->dp_cookies;
		}

		*objpp = (caddr_t)handle;
		return (DDI_SUCCESS);

	case DDI_DMA_FREE: /* ddi_dma_free() */
		(void) rootnex_dma_unbindhdl(dip, rdip, handle);
		(void) rootnex_dma_freehdl(dip, rdip, handle);
		if (rootnex_state.r_dvma_call_list_id) {
			ddi_run_callback(&rootnex_state.r_dvma_call_list_id);
		}
		return (DDI_SUCCESS);

	case DDI_DMA_IOPB_ALLOC:	/* get contiguous DMA-able memory */
	case DDI_DMA_SMEM_ALLOC:	/* get contiguous DMA-able memory */
		/* should never get here, handled in genunix */
		ASSERT(0);
		return (DDI_FAILURE);

	case DDI_DMA_KVADDR:
	case DDI_DMA_GETERR:
	case DDI_DMA_COFF:
		return (DDI_FAILURE);
	}

	return (DDI_FAILURE);
}


#ifdef	FMA_ENABLE
/*
 * *********
 *  FMA Code
 * *********
 */

/*
 * rootnex_fm_init()
 *    FMA init busop
 */
/* ARGSUSED */
static int
rootnex_fm_init(dev_info_t *dip, dev_info_t *tdip, int tcap,
    ddi_iblock_cookie_t *ibc)
{
	*ibc = rootnex_state.r_err_ibc;

	return (ddi_system_fmcap);
}

/*
 * rootnex_dma_check()
 *    Function called after a dma fault occurred to find out whether the
 *    fault address is associated with a driver that is able to handle faults
 *    and recover from faults.
 */
/* ARGSUSED */
static int
rootnex_dma_check(dev_info_t *dip, const void *handle, const void *addr,
    const void *not_used)
{
	rootnex_window_t *window;
	uint64_t start_addr;
	uint64_t fault_addr;
	ddi_dma_impl_t *hp;
	rootnex_dma_t *dma;
	uint64_t end_addr;
	size_t csize;
	int i;
	int j;


	/* The driver has to set DDI_DMA_FLAGERR to recover from dma faults */
	hp = (ddi_dma_impl_t *)handle;
	ASSERT(hp);

	dma = (rootnex_dma_t *)hp->dmai_private;

	/* Get the address that we need to search for */
	fault_addr = *(uint64_t *)addr;

	/*
	 * if we don't have any windows, we can just walk through all the
	 * cookies.
	 */
	if (dma->dp_window == NULL) {
		/* for each cookie */
		for (i = 0; i < dma->dp_sglinfo.si_sgl_size; i++) {
			/*
			 * if the faulted address is within the physical address
			 * range of the cookie, return DDI_FM_NONFATAL.
			 */
			if ((fault_addr >= dma->dp_cookies[i].dmac_laddress) &&
			    (fault_addr <= (dma->dp_cookies[i].dmac_laddress +
			    dma->dp_cookies[i].dmac_size))) {
				return (DDI_FM_NONFATAL);
			}
		}

		/* fault_addr not within this DMA handle */
		return (DDI_FM_UNKNOWN);
	}

	/* we have mutiple windows, walk through each window */
	for (i = 0; i < hp->dmai_nwin; i++) {
		window = &dma->dp_window[i];

		/* Go through all the cookies in the window */
		for (j = 0; j < window->wd_cookie_cnt; j++) {

			start_addr = window->wd_first_cookie[j].dmac_laddress;
			csize = window->wd_first_cookie[j].dmac_size;

			/*
			 * if we are trimming the first cookie in the window,
			 * and this is the first cookie, adjust the start
			 * address and size of the cookie to account for the
			 * trim.
			 */
			if (window->wd_trim.tr_trim_first && (j == 0)) {
				start_addr = window->wd_trim.tr_first_paddr;
				csize = window->wd_trim.tr_first_size;
			}

			/*
			 * if we are trimming the last cookie in the window,
			 * and this is the last cookie, adjust the start
			 * address and size of the cookie to account for the
			 * trim.
			 */
			if (window->wd_trim.tr_trim_last &&
			    (j == (window->wd_cookie_cnt - 1))) {
				start_addr = window->wd_trim.tr_last_paddr;
				csize = window->wd_trim.tr_last_size;
			}

			end_addr = start_addr + csize;

			/*
			 * if the faulted address is within the physical address
			 * range of the cookie, return DDI_FM_NONFATAL.
			 */
			if ((fault_addr >= start_addr) &&
			    (fault_addr <= end_addr)) {
				return (DDI_FM_NONFATAL);
			}
		}
	}

	/* fault_addr not within this DMA handle */
	return (DDI_FM_UNKNOWN);
}
#endif	/* FMA_ENABLE */

/*
 * static caddr_t
 * rootnex_reg_mapin(pfn_t pfn, size_t len, uint_t attr)
 *	Map the specified physical address into virtual address.
 */
static caddr_t
rootnex_reg_mapin(pfn_t pfn, size_t len, uint_t attr)
{
	caddr_t		addr;
	uintptr_t	start = mmu_ptob(pfn);
	uintptr_t	end = start + len;

	addr = (caddr_t)ARMPF_BUILTIN_IOSPACE_PTOV(start, end);
	if (addr == NULL) {
		/* Create new mapping. */
		addr = device_arena_alloc(len, VM_NOSLEEP);
		if (addr != NULL) {
			hat_devload(&hat_kas, addr, len, pfn, attr,
				    HAT_LOAD_LOCK);
		}
	}

	return addr;
}

/*
 * static void
 * rootnex_reg_mapout(caddr_t addr, size_t len)
 *	Release mapping created by rootnex_reg_mapin().
 */
static void
rootnex_reg_mapout(caddr_t addr, size_t len)
{
	uintptr_t	start = (uintptr_t)addr;
	uintptr_t	end = start + len;

	if (ARMPF_IS_BUILTIN_IOSPACE(start, end)) {
		/* Do NOT unmap builtin I/O space. */
		return;
	}

	/* Unmap and release virtual space. */
	hat_unload(&hat_kas, addr, len, HAT_UNLOAD_UNLOCK);
	device_arena_free(addr, len);
}