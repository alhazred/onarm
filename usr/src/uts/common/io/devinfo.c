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
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2006-2008 NEC Corporation
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * driver for accessing kernel devinfo tree.
 */
#include <sys/types.h>
#include <sys/pathname.h>
#include <sys/debug.h>
#include <sys/autoconf.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/kmem.h>
#include <sys/modctl.h>
#include <sys/stat.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunldi_impl.h>
#include <sys/sunndi.h>
#include <sys/esunddi.h>
#include <sys/sunmdi.h>
#include <sys/ddi_impldefs.h>
#include <sys/ndi_impldefs.h>
#include <sys/mdi_impldefs.h>
#include <sys/devinfo_impl.h>
#include <sys/thread.h>
#include <sys/modhash.h>
#include <sys/bitmap.h>
#include <util/qsort.h>
#include <sys/disp.h>
#include <sys/kobj.h>
#include <sys/crc32.h>


#ifdef DEBUG
static int di_debug;
#define	dcmn_err(args) if (di_debug >= 1) cmn_err args
#define	dcmn_err2(args) if (di_debug >= 2) cmn_err args
#define	dcmn_err3(args) if (di_debug >= 3) cmn_err args
#else
#define	dcmn_err(args) /* nothing */
#define	dcmn_err2(args) /* nothing */
#define	dcmn_err3(args) /* nothing */
#endif

/*
 * We partition the space of devinfo minor nodes equally between the full and
 * unprivileged versions of the driver.  The even-numbered minor nodes are the
 * full version, while the odd-numbered ones are the read-only version.
 */
static int di_max_opens = 32;

#define	DI_FULL_PARENT		0
#define	DI_READONLY_PARENT	1
#define	DI_NODE_SPECIES		2
#define	DI_UNPRIVILEGED_NODE(x)	(((x) % 2) != 0)

#define	IOC_IDLE	0	/* snapshot ioctl states */
#define	IOC_SNAP	1	/* snapshot in progress */
#define	IOC_DONE	2	/* snapshot done, but not copied out */
#define	IOC_COPY	3	/* copyout in progress */

/*
 * Keep max alignment so we can move snapshot to different platforms
 */
#define	DI_ALIGN(addr)	((addr + 7l) & ~7l)

/*
 * To avoid wasting memory, make a linked list of memory chunks.
 * Size of each chunk is buf_size.
 */
struct di_mem {
	struct di_mem *next;	/* link to next chunk */
	char *buf;		/* contiguous kernel memory */
	size_t buf_size;	/* size of buf in bytes */
	devmap_cookie_t cook;	/* cookie from ddi_umem_alloc */
};

/*
 * This is a stack for walking the tree without using recursion.
 * When the devinfo tree height is above some small size, one
 * gets watchdog resets on sun4m.
 */
struct di_stack {
	void		*offset[MAX_TREE_DEPTH];
	struct dev_info *dip[MAX_TREE_DEPTH];
	int		circ[MAX_TREE_DEPTH];
	int		depth;	/* depth of current node to be copied */
};

#define	TOP_OFFSET(stack)	\
	((di_off_t *)(stack)->offset[(stack)->depth - 1])
#define	TOP_NODE(stack)		\
	((stack)->dip[(stack)->depth - 1])
#define	PARENT_OFFSET(stack)	\
	((di_off_t *)(stack)->offset[(stack)->depth - 2])
#define	EMPTY_STACK(stack)	((stack)->depth == 0)
#define	POP_STACK(stack)	{ \
	ndi_devi_exit((dev_info_t *)TOP_NODE(stack), \
		(stack)->circ[(stack)->depth - 1]); \
	((stack)->depth--); \
}
#define	PUSH_STACK(stack, node, offp)	{ \
	ASSERT(node != NULL); \
	ndi_devi_enter((dev_info_t *)node, &(stack)->circ[(stack)->depth]); \
	(stack)->dip[(stack)->depth] = (node); \
	(stack)->offset[(stack)->depth] = (void *)(offp); \
	((stack)->depth)++; \
}

#define	DI_ALL_PTR(s)	((struct di_all *)(intptr_t)di_mem_addr((s), 0))

/*
 * With devfs, the device tree has no global locks. The device tree is
 * dynamic and dips may come and go if they are not locked locally. Under
 * these conditions, pointers are no longer reliable as unique IDs.
 * Specifically, these pointers cannot be used as keys for hash tables
 * as the same devinfo structure may be freed in one part of the tree only
 * to be allocated as the structure for a different device in another
 * part of the tree. This can happen if DR and the snapshot are
 * happening concurrently.
 * The following data structures act as keys for devinfo nodes and
 * pathinfo nodes.
 */

enum di_ktype {
	DI_DKEY = 1,
	DI_PKEY = 2
};

struct di_dkey {
	dev_info_t	*dk_dip;
	major_t		dk_major;
	int		dk_inst;
	pnode_t		dk_nodeid;
};

struct di_pkey {
	mdi_pathinfo_t	*pk_pip;
	char		*pk_path_addr;
	dev_info_t	*pk_client;
	dev_info_t	*pk_phci;
};

struct di_key {
	enum di_ktype	k_type;
	union {
		struct di_dkey dkey;
		struct di_pkey pkey;
	} k_u;
};


struct i_lnode;

typedef struct i_link {
	/*
	 * If a di_link struct representing this i_link struct makes it
	 * into the snapshot, then self will point to the offset of
	 * the di_link struct in the snapshot
	 */
	di_off_t	self;

	int		spec_type;	/* block or char access type */
	struct i_lnode	*src_lnode;	/* src i_lnode */
	struct i_lnode	*tgt_lnode;	/* tgt i_lnode */
	struct i_link	*src_link_next;	/* next src i_link /w same i_lnode */
	struct i_link	*tgt_link_next;	/* next tgt i_link /w same i_lnode */
} i_link_t;

typedef struct i_lnode {
	/*
	 * If a di_lnode struct representing this i_lnode struct makes it
	 * into the snapshot, then self will point to the offset of
	 * the di_lnode struct in the snapshot
	 */
	di_off_t	self;

	/*
	 * used for hashing and comparing i_lnodes
	 */
	int		modid;

	/*
	 * public information describing a link endpoint
	 */
	struct di_node	*di_node;	/* di_node in snapshot */
	dev_t		devt;		/* devt */

	/*
	 * i_link ptr to links coming into this i_lnode node
	 * (this i_lnode is the target of these i_links)
	 */
	i_link_t	*link_in;

	/*
	 * i_link ptr to links going out of this i_lnode node
	 * (this i_lnode is the source of these i_links)
	 */
	i_link_t	*link_out;
} i_lnode_t;

/*
 * Soft state associated with each instance of driver open.
 */
static struct di_state {
	di_off_t mem_size;	/* total # bytes in memlist	*/
	struct di_mem *memlist;	/* head of memlist		*/
	uint_t command;		/* command from ioctl		*/
	int di_iocstate;	/* snapshot ioctl state		*/
	mod_hash_t *reg_dip_hash;
	mod_hash_t *reg_pip_hash;
	int lnode_count;
	int link_count;

	mod_hash_t *lnode_hash;
	mod_hash_t *link_hash;
} **di_states;

static kmutex_t di_lock;	/* serialize instance assignment */

typedef enum {
	DI_QUIET = 0,	/* DI_QUIET must always be 0 */
	DI_ERR,
	DI_INFO,
	DI_TRACE,
	DI_TRACE1,
	DI_TRACE2
} di_cache_debug_t;

static uint_t	di_chunk = 32;		/* I/O chunk size in pages */

#define	DI_CACHE_LOCK(c)	(mutex_enter(&(c).cache_lock))
#define	DI_CACHE_UNLOCK(c)	(mutex_exit(&(c).cache_lock))
#define	DI_CACHE_LOCKED(c)	(mutex_owned(&(c).cache_lock))

/*
 * Check that whole device tree is being configured as a pre-condition for
 * cleaning up /etc/devices files.
 */
#define	DEVICES_FILES_CLEANABLE(st)	\
	(((st)->command & DINFOSUBTREE) && ((st)->command & DINFOFORCE) && \
	strcmp(DI_ALL_PTR(st)->root_path, "/") == 0)

#define	CACHE_DEBUG(args)	\
	{ if (di_cache_debug != DI_QUIET) di_cache_print args; }

typedef struct phci_walk_arg {
	di_off_t	off;
	struct di_state	*st;
} phci_walk_arg_t;

static int di_open(dev_t *, int, int, cred_t *);
static int di_ioctl(dev_t, int, intptr_t, int, cred_t *, int *);
static int di_close(dev_t, int, int, cred_t *);
static int di_info(dev_info_t *, ddi_info_cmd_t, void *, void **);
static int di_attach(dev_info_t *, ddi_attach_cmd_t);
static int di_detach(dev_info_t *, ddi_detach_cmd_t);

static di_off_t di_copyformat(di_off_t, struct di_state *, intptr_t, int);
static di_off_t di_snapshot_and_clean(struct di_state *);
static di_off_t di_copydevnm(di_off_t *, struct di_state *);
static di_off_t di_copytree(struct dev_info *, di_off_t *, struct di_state *);
static di_off_t di_copynode(struct di_stack *, struct di_state *);
static di_off_t di_getmdata(struct ddi_minor_data *, di_off_t *, di_off_t,
    struct di_state *);
static di_off_t di_getppdata(struct dev_info *, di_off_t *, struct di_state *);
static di_off_t di_getdpdata(struct dev_info *, di_off_t *, struct di_state *);
static di_off_t di_getprop(struct ddi_prop *, di_off_t *,
    struct di_state *, struct dev_info *, int);
static void di_allocmem(struct di_state *, size_t);
static void di_freemem(struct di_state *);
static void di_copymem(struct di_state *st, caddr_t buf, size_t bufsiz);
static di_off_t di_checkmem(struct di_state *, di_off_t, size_t);
static caddr_t di_mem_addr(struct di_state *, di_off_t);
static int di_setstate(struct di_state *, int);
static void di_register_dip(struct di_state *, dev_info_t *, di_off_t);
static void di_register_pip(struct di_state *, mdi_pathinfo_t *, di_off_t);
static di_off_t di_getpath_data(dev_info_t *, di_off_t *, di_off_t,
    struct di_state *, int);
static di_off_t di_getlink_data(di_off_t, struct di_state *);
static int di_dip_find(struct di_state *st, dev_info_t *node, di_off_t *off_p);

static int cache_args_valid(struct di_state *st, int *error);
static int snapshot_is_cacheable(struct di_state *st);
static int di_cache_lookup(struct di_state *st);
static int di_cache_update(struct di_state *st);
static void di_cache_print(di_cache_debug_t msglevel, char *fmt, ...);
int build_vhci_list(dev_info_t *vh_devinfo, void *arg);
int build_phci_list(dev_info_t *ph_devinfo, void *arg);

static struct cb_ops di_cb_ops = {
	di_open,		/* open */
	di_close,		/* close */
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	nodev,			/* read */
	nodev,			/* write */
	di_ioctl,		/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev,			/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* prop_op */
	NULL,			/* streamtab  */
	D_NEW | D_MP		/* Driver compatibility flag */
};

static struct dev_ops di_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	di_info,		/* info */
	nulldev,		/* identify */
	nulldev,		/* probe */
	di_attach,		/* attach */
	di_detach,		/* detach */
	nodev,			/* reset */
	&di_cb_ops,		/* driver operations */
	NULL			/* bus operations */
};

/*
 * Module linkage information for the kernel.
 */
static struct modldrv modldrv = {
	&mod_driverops,
	"DEVINFO Driver %I%",
	&di_ops
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
	NULL
};

int
MODDRV_ENTRY_INIT(void)
{
	int	error;

	mutex_init(&di_lock, NULL, MUTEX_DRIVER, NULL);

	error = mod_install(&modlinkage);
	if (error != 0) {
		mutex_destroy(&di_lock);
		return (error);
	}

	return (0);
}

int
MODDRV_ENTRY_INFO(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

#ifndef	STATIC_DRIVER
int
MODDRV_ENTRY_FINI(void)
{
	int	error;

	error = mod_remove(&modlinkage);
	if (error != 0) {
		return (error);
	}

	mutex_destroy(&di_lock);
	return (0);
}
#endif	/* !STATIC_DRIVER */

static dev_info_t *di_dip;

/*ARGSUSED*/
static int
di_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	int error = DDI_FAILURE;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*result = (void *)di_dip;
		error = DDI_SUCCESS;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		/*
		 * All dev_t's map to the same, single instance.
		 */
		*result = (void *)0;
		error = DDI_SUCCESS;
		break;
	default:
		break;
	}

	return (error);
}

static int
di_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int error = DDI_FAILURE;

	switch (cmd) {
	case DDI_ATTACH:
		di_states = kmem_zalloc(
		    di_max_opens * sizeof (struct di_state *), KM_SLEEP);

		if (ddi_create_minor_node(dip, "devinfo", S_IFCHR,
		    DI_FULL_PARENT, DDI_PSEUDO, NULL) == DDI_FAILURE ||
		    ddi_create_minor_node(dip, "devinfo,ro", S_IFCHR,
		    DI_READONLY_PARENT, DDI_PSEUDO, NULL) == DDI_FAILURE) {
			kmem_free(di_states,
			    di_max_opens * sizeof (struct di_state *));
			ddi_remove_minor_node(dip, NULL);
			error = DDI_FAILURE;
		} else {
			di_dip = dip;
			ddi_report_dev(dip);

			error = DDI_SUCCESS;
		}
		break;
	default:
		error = DDI_FAILURE;
		break;
	}

	return (error);
}

static int
di_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	int error = DDI_FAILURE;

	switch (cmd) {
	case DDI_DETACH:
		ddi_remove_minor_node(dip, NULL);
		di_dip = NULL;
		kmem_free(di_states, di_max_opens * sizeof (struct di_state *));

		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
		break;
	}

	return (error);
}

/*
 * Allow multiple opens by tweaking the dev_t such that it looks like each
 * open is getting a different minor device.  Each minor gets a separate
 * entry in the di_states[] table.  Based on the original minor number, we
 * discriminate opens of the full and read-only nodes.  If all of the instances
 * of the selected minor node are currently open, we return EAGAIN.
 */
/*ARGSUSED*/
static int
di_open(dev_t *devp, int flag, int otyp, cred_t *credp)
{
	int m;
	minor_t minor_parent = getminor(*devp);

	if (minor_parent != DI_FULL_PARENT &&
	    minor_parent != DI_READONLY_PARENT)
		return (ENXIO);

	mutex_enter(&di_lock);

	for (m = minor_parent; m < di_max_opens; m += DI_NODE_SPECIES) {
		if (di_states[m] != NULL)
			continue;

		di_states[m] = kmem_zalloc(sizeof (struct di_state), KM_SLEEP);
		break;	/* It's ours. */
	}

	if (m >= di_max_opens) {
		/*
		 * maximum open instance for device reached
		 */
		mutex_exit(&di_lock);
		dcmn_err((CE_WARN, "devinfo: maximum devinfo open reached"));
		return (EAGAIN);
	}
	mutex_exit(&di_lock);

	ASSERT(m < di_max_opens);
	*devp = makedevice(getmajor(*devp), (minor_t)(m + DI_NODE_SPECIES));

	dcmn_err((CE_CONT, "di_open: thread = %p, assigned minor = %d\n",
	    (void *)curthread, m + DI_NODE_SPECIES));

	return (0);
}

/*ARGSUSED*/
static int
di_close(dev_t dev, int flag, int otype, cred_t *cred_p)
{
	struct di_state *st;
	int m = (int)getminor(dev) - DI_NODE_SPECIES;

	if (m < 0) {
		cmn_err(CE_WARN, "closing non-existent devinfo minor %d",
		    m + DI_NODE_SPECIES);
		return (ENXIO);
	}

	st = di_states[m];
	ASSERT(m < di_max_opens && st != NULL);

	di_freemem(st);
	kmem_free(st, sizeof (struct di_state));

	/*
	 * empty slot in state table
	 */
	mutex_enter(&di_lock);
	di_states[m] = NULL;
	dcmn_err((CE_CONT, "di_close: thread = %p, assigned minor = %d\n",
	    (void *)curthread, m + DI_NODE_SPECIES));
	mutex_exit(&di_lock);

	return (0);
}


/*ARGSUSED*/
static int
di_ioctl(dev_t dev, int cmd, intptr_t arg, int mode, cred_t *credp, int *rvalp)
{
	int rv, error;
	di_off_t off;
	struct di_all *all;
	struct di_state *st;
	int m = (int)getminor(dev) - DI_NODE_SPECIES;

	major_t i;
	char *drv_name;
	size_t map_size, size;
	struct di_mem *dcp;
	int ndi_flags;

	if (m < 0 || m >= di_max_opens) {
		return (ENXIO);
	}

	st = di_states[m];
	ASSERT(st != NULL);

	dcmn_err2((CE_CONT, "di_ioctl: mode = %x, cmd = %x\n", mode, cmd));

	switch (cmd) {
	case DINFOIDENT:
		/*
		 * This is called from di_init to verify that the driver
		 * opened is indeed devinfo. The purpose is to guard against
		 * sending ioctl to an unknown driver in case of an
		 * unresolved major number conflict during bfu.
		 */
		*rvalp = DI_MAGIC;
		return (0);

	case DINFOLODRV:
		/*
		 * Hold an installed driver and return the result
		 */
		if (DI_UNPRIVILEGED_NODE(m)) {
			/*
			 * Only the fully enabled instances may issue
			 * DINFOLDDRV.
			 */
			return (EACCES);
		}

		drv_name = kmem_alloc(MAXNAMELEN, KM_SLEEP);
		if (ddi_copyin((void *)arg, drv_name, MAXNAMELEN, mode) != 0) {
			kmem_free(drv_name, MAXNAMELEN);
			return (EFAULT);
		}

		/*
		 * Some 3rd party driver's _init() walks the device tree,
		 * so we load the driver module before configuring driver.
		 */
		i = ddi_name_to_major(drv_name);
		if (ddi_hold_driver(i) == NULL) {
			kmem_free(drv_name, MAXNAMELEN);
			return (ENXIO);
		}

		ndi_flags = NDI_DEVI_PERSIST | NDI_CONFIG | NDI_NO_EVENT;

		/*
		 * i_ddi_load_drvconf() below will trigger a reprobe
		 * via reset_nexus_flags(). NDI_DRV_CONF_REPROBE isn't
		 * needed here.
		 */
		modunload_disable();
		(void) i_ddi_load_drvconf(i);
		(void) ndi_devi_config_driver(ddi_root_node(), ndi_flags, i);
		kmem_free(drv_name, MAXNAMELEN);
		ddi_rele_driver(i);
		rv = i_ddi_devs_attached(i);
		modunload_enable();

		i_ddi_di_cache_invalidate(KM_SLEEP);

		return ((rv == DDI_SUCCESS)? 0 : ENXIO);

	case DINFOUSRLD:
		/*
		 * The case for copying snapshot to userland
		 */
		if (di_setstate(st, IOC_COPY) == -1)
			return (EBUSY);

		map_size = ((struct di_all *)
		    (intptr_t)di_mem_addr(st, 0))->map_size;
		if (map_size == 0) {
			(void) di_setstate(st, IOC_DONE);
			return (EFAULT);
		}

		/*
		 * copyout the snapshot
		 */
		map_size = (map_size + PAGEOFFSET) & PAGEMASK;

		/*
		 * Return the map size, so caller may do a sanity
		 * check against the return value of snapshot ioctl()
		 */
		*rvalp = (int)map_size;

		/*
		 * Copy one chunk at a time
		 */
		off = 0;
		dcp = st->memlist;
		while (map_size) {
			size = dcp->buf_size;
			if (map_size <= size) {
				size = map_size;
			}

			if (ddi_copyout(di_mem_addr(st, off),
			    (void *)(arg + off), size, mode) != 0) {
				(void) di_setstate(st, IOC_DONE);
				return (EFAULT);
			}

			map_size -= size;
			off += size;
			dcp = dcp->next;
		}

		di_freemem(st);
		(void) di_setstate(st, IOC_IDLE);
		return (0);

	default:
		if ((cmd & ~DIIOC_MASK) != DIIOC) {
			/*
			 * Invalid ioctl command
			 */
			return (ENOTTY);
		}
		/*
		 * take a snapshot
		 */
		st->command = cmd & DIIOC_MASK;
		/*FALLTHROUGH*/
	}

	/*
	 * Obtain enough memory to hold header + rootpath.  We prevent kernel
	 * memory exhaustion by freeing any previously allocated snapshot and
	 * refusing the operation; otherwise we would be allowing ioctl(),
	 * ioctl(), ioctl(), ..., panic.
	 */
	if (di_setstate(st, IOC_SNAP) == -1)
		return (EBUSY);

	size = sizeof (struct di_all) +
	    sizeof (((struct dinfo_io *)(NULL))->root_path);
	if (size < PAGESIZE)
		size = PAGESIZE;
	di_allocmem(st, size);

	all = (struct di_all *)(intptr_t)di_mem_addr(st, 0);
	all->devcnt = devcnt;
	all->command = st->command;
	all->version = DI_SNAPSHOT_VERSION;
	all->top_vhci_devinfo = 0;	/* filled up by build_vhci_list. */

	/*
	 * Note the endianness in case we need to transport snapshot
	 * over the network.
	 */
#if defined(_LITTLE_ENDIAN)
	all->endianness = DI_LITTLE_ENDIAN;
#else
	all->endianness = DI_BIG_ENDIAN;
#endif

	/* Copyin ioctl args, store in the snapshot. */
	if (copyinstr((void *)arg, all->root_path,
	    sizeof (((struct dinfo_io *)(NULL))->root_path), &size) != 0) {
		di_freemem(st);
		(void) di_setstate(st, IOC_IDLE);
		return (EFAULT);
	}

	if ((st->command & DINFOCLEANUP) && !DEVICES_FILES_CLEANABLE(st)) {
		di_freemem(st);
		(void) di_setstate(st, IOC_IDLE);
		return (EINVAL);
	}

	error = 0;
	if ((st->command & DINFOCACHE) && !cache_args_valid(st, &error)) {
		di_freemem(st);
		(void) di_setstate(st, IOC_IDLE);
		return (error);
	}

	off = DI_ALIGN(sizeof (struct di_all) + size);

	/*
	 * Only the fully enabled version may force load drivers or read
	 * the parent private data from a driver.
	 */
	if ((st->command & (DINFOPRIVDATA | DINFOFORCE)) != 0 &&
	    DI_UNPRIVILEGED_NODE(m)) {
		di_freemem(st);
		(void) di_setstate(st, IOC_IDLE);
		return (EACCES);
	}

	/* Do we need private data? */
	if (st->command & DINFOPRIVDATA) {
		arg += sizeof (((struct dinfo_io *)(NULL))->root_path);

#ifdef _MULTI_DATAMODEL
		switch (ddi_model_convert_from(mode & FMODELS)) {
		case DDI_MODEL_ILP32: {
			/*
			 * Cannot copy private data from 64-bit kernel
			 * to 32-bit app
			 */
			di_freemem(st);
			(void) di_setstate(st, IOC_IDLE);
			return (EINVAL);
		}
		case DDI_MODEL_NONE:
			if ((off = di_copyformat(off, st, arg, mode)) == 0) {
				di_freemem(st);
				(void) di_setstate(st, IOC_IDLE);
				return (EFAULT);
			}
			break;
		}
#else /* !_MULTI_DATAMODEL */
		if ((off = di_copyformat(off, st, arg, mode)) == 0) {
			di_freemem(st);
			(void) di_setstate(st, IOC_IDLE);
			return (EFAULT);
		}
#endif /* _MULTI_DATAMODEL */
	}

	all->top_devinfo = DI_ALIGN(off);

	/*
	 * For cache lookups we reallocate memory from scratch,
	 * so the value of "all" is no longer valid.
	 */
	all = NULL;

	if (st->command & DINFOCACHE) {
		*rvalp = di_cache_lookup(st);
	} else if (snapshot_is_cacheable(st)) {
		DI_CACHE_LOCK(di_cache);
		*rvalp = di_cache_update(st);
		DI_CACHE_UNLOCK(di_cache);
	} else
		*rvalp = di_snapshot_and_clean(st);

	if (*rvalp) {
		DI_ALL_PTR(st)->map_size = *rvalp;
		(void) di_setstate(st, IOC_DONE);
	} else {
		di_freemem(st);
		(void) di_setstate(st, IOC_IDLE);
	}

	return (0);
}

/*
 * Get a chunk of memory >= size, for the snapshot
 */
static void
di_allocmem(struct di_state *st, size_t size)
{
	struct di_mem *mem = kmem_zalloc(sizeof (struct di_mem),
	    KM_SLEEP);
	/*
	 * Round up size to nearest power of 2. If it is less
	 * than st->mem_size, set it to st->mem_size (i.e.,
	 * the mem_size is doubled every time) to reduce the
	 * number of memory allocations.
	 */
	size_t tmp = 1;
	while (tmp < size) {
		tmp <<= 1;
	}
	size = (tmp > st->mem_size) ? tmp : st->mem_size;

	mem->buf = ddi_umem_alloc(size, DDI_UMEM_SLEEP, &mem->cook);
	mem->buf_size = size;

	dcmn_err2((CE_CONT, "di_allocmem: mem_size=%x\n", st->mem_size));

	if (st->mem_size == 0) {	/* first chunk */
		st->memlist = mem;
	} else {
		/*
		 * locate end of linked list and add a chunk at the end
		 */
		struct di_mem *dcp = st->memlist;
		while (dcp->next != NULL) {
			dcp = dcp->next;
		}

		dcp->next = mem;
	}

	st->mem_size += size;
}

/*
 * Copy upto bufsiz bytes of the memlist to buf
 */
static void
di_copymem(struct di_state *st, caddr_t buf, size_t bufsiz)
{
	struct di_mem *dcp;
	size_t copysz;

	if (st->mem_size == 0) {
		ASSERT(st->memlist == NULL);
		return;
	}

	copysz = 0;
	for (dcp = st->memlist; dcp; dcp = dcp->next) {

		ASSERT(bufsiz > 0);

		if (bufsiz <= dcp->buf_size)
			copysz = bufsiz;
		else
			copysz = dcp->buf_size;

		bcopy(dcp->buf, buf, copysz);

		buf += copysz;
		bufsiz -= copysz;

		if (bufsiz == 0)
			break;
	}
}

/*
 * Free all memory for the snapshot
 */
static void
di_freemem(struct di_state *st)
{
	struct di_mem *dcp, *tmp;

	dcmn_err2((CE_CONT, "di_freemem\n"));

	if (st->mem_size) {
		dcp = st->memlist;
		while (dcp) {	/* traverse the linked list */
			tmp = dcp;
			dcp = dcp->next;
			ddi_umem_free(tmp->cook);
			kmem_free(tmp, sizeof (struct di_mem));
		}
		st->mem_size = 0;
		st->memlist = NULL;
	}

	ASSERT(st->mem_size == 0);
	ASSERT(st->memlist == NULL);
}

/*
 * Copies cached data to the di_state structure.
 * Returns:
 *	- size of data copied, on SUCCESS
 *	- 0 on failure
 */
static int
di_cache2mem(struct di_cache *cache, struct di_state *st)
{
	caddr_t	pa;

	ASSERT(st->mem_size == 0);
	ASSERT(st->memlist == NULL);
	ASSERT(!servicing_interrupt());
	ASSERT(DI_CACHE_LOCKED(*cache));

	if (cache->cache_size == 0) {
		ASSERT(cache->cache_data == NULL);
		CACHE_DEBUG((DI_ERR, "Empty cache. Skipping copy"));
		return (0);
	}

	ASSERT(cache->cache_data);

	di_allocmem(st, cache->cache_size);

	pa = di_mem_addr(st, 0);

	ASSERT(pa);

	/*
	 * Verify that di_allocmem() allocates contiguous memory,
	 * so that it is safe to do straight bcopy()
	 */
	ASSERT(st->memlist != NULL);
	ASSERT(st->memlist->next == NULL);
	bcopy(cache->cache_data, pa, cache->cache_size);

	return (cache->cache_size);
}

/*
 * Copies a snapshot from di_state to the cache
 * Returns:
 *	- 0 on failure
 *	- size of copied data on success
 */
static size_t
di_mem2cache(struct di_state *st, struct di_cache *cache)
{
	size_t map_size;

	ASSERT(cache->cache_size == 0);
	ASSERT(cache->cache_data == NULL);
	ASSERT(!servicing_interrupt());
	ASSERT(DI_CACHE_LOCKED(*cache));

	if (st->mem_size == 0) {
		ASSERT(st->memlist == NULL);
		CACHE_DEBUG((DI_ERR, "Empty memlist. Skipping copy"));
		return (0);
	}

	ASSERT(st->memlist);

	/*
	 * The size of the memory list may be much larger than the
	 * size of valid data (map_size). Cache only the valid data
	 */
	map_size = DI_ALL_PTR(st)->map_size;
	if (map_size == 0 || map_size < sizeof (struct di_all) ||
	    map_size > st->mem_size) {
		CACHE_DEBUG((DI_ERR, "cannot cache: bad size: 0x%x", map_size));
		return (0);
	}

	cache->cache_data = kmem_alloc(map_size, KM_SLEEP);
	cache->cache_size = map_size;
	di_copymem(st, cache->cache_data, cache->cache_size);

	return (map_size);
}

/*
 * Make sure there is at least "size" bytes memory left before
 * going on. Otherwise, start on a new chunk.
 */
static di_off_t
di_checkmem(struct di_state *st, di_off_t off, size_t size)
{
	dcmn_err3((CE_CONT, "di_checkmem: off=%x size=%x\n",
	    off, (int)size));

	/*
	 * di_checkmem() shouldn't be called with a size of zero.
	 * But in case it is, we want to make sure we return a valid
	 * offset within the memlist and not an offset that points us
	 * at the end of the memlist.
	 */
	if (size == 0) {
		dcmn_err((CE_WARN, "di_checkmem: invalid zero size used"));
		size = 1;
	}

	off = DI_ALIGN(off);
	if ((st->mem_size - off) < size) {
		off = st->mem_size;
		di_allocmem(st, size);
	}

	return (off);
}

/*
 * Copy the private data format from ioctl arg.
 * On success, the ending offset is returned. On error 0 is returned.
 */
static di_off_t
di_copyformat(di_off_t off, struct di_state *st, intptr_t arg, int mode)
{
	di_off_t size;
	struct di_priv_data *priv;
	struct di_all *all = (struct di_all *)(intptr_t)di_mem_addr(st, 0);

	dcmn_err2((CE_CONT, "di_copyformat: off=%x, arg=%p mode=%x\n",
	    off, (void *)arg, mode));

	/*
	 * Copyin data and check version.
	 * We only handle private data version 0.
	 */
	priv = kmem_alloc(sizeof (struct di_priv_data), KM_SLEEP);
	if ((ddi_copyin((void *)arg, priv, sizeof (struct di_priv_data),
	    mode) != 0) || (priv->version != DI_PRIVDATA_VERSION_0)) {
		kmem_free(priv, sizeof (struct di_priv_data));
		return (0);
	}

	/*
	 * Save di_priv_data copied from userland in snapshot.
	 */
	all->pd_version = priv->version;
	all->n_ppdata = priv->n_parent;
	all->n_dpdata = priv->n_driver;

	/*
	 * copyin private data format, modify offset accordingly
	 */
	if (all->n_ppdata) {	/* parent private data format */
		/*
		 * check memory
		 */
		size = all->n_ppdata * sizeof (struct di_priv_format);
		off = di_checkmem(st, off, size);
		all->ppdata_format = off;
		if (ddi_copyin(priv->parent, di_mem_addr(st, off), size,
		    mode) != 0) {
			kmem_free(priv, sizeof (struct di_priv_data));
			return (0);
		}

		off += size;
	}

	if (all->n_dpdata) {	/* driver private data format */
		/*
		 * check memory
		 */
		size = all->n_dpdata * sizeof (struct di_priv_format);
		off = di_checkmem(st, off, size);
		all->dpdata_format = off;
		if (ddi_copyin(priv->driver, di_mem_addr(st, off), size,
		    mode) != 0) {
			kmem_free(priv, sizeof (struct di_priv_data));
			return (0);
		}

		off += size;
	}

	kmem_free(priv, sizeof (struct di_priv_data));
	return (off);
}

/*
 * Return the real address based on the offset (off) within snapshot
 */
static caddr_t
di_mem_addr(struct di_state *st, di_off_t off)
{
	struct di_mem *dcp = st->memlist;

	dcmn_err3((CE_CONT, "di_mem_addr: dcp=%p off=%x\n",
	    (void *)dcp, off));

	ASSERT(off < st->mem_size);

	while (off >= dcp->buf_size) {
		off -= dcp->buf_size;
		dcp = dcp->next;
	}

	dcmn_err3((CE_CONT, "di_mem_addr: new off=%x, return = %p\n",
	    off, (void *)(dcp->buf + off)));

	return (dcp->buf + off);
}

/*
 * Ideally we would use the whole key to derive the hash
 * value. However, the probability that two keys will
 * have the same dip (or pip) is very low, so
 * hashing by dip (or pip) pointer should suffice.
 */
static uint_t
di_hash_byptr(void *arg, mod_hash_key_t key)
{
	struct di_key *dik = key;
	size_t rshift;
	void *ptr;

	ASSERT(arg == NULL);

	switch (dik->k_type) {
	case DI_DKEY:
		ptr = dik->k_u.dkey.dk_dip;
		rshift = highbit(sizeof (struct dev_info));
		break;
	case DI_PKEY:
		ptr = dik->k_u.pkey.pk_pip;
		rshift = highbit(sizeof (struct mdi_pathinfo));
		break;
	default:
		panic("devinfo: unknown key type");
		/*NOTREACHED*/
	}
	return (mod_hash_byptr((void *)rshift, ptr));
}

static void
di_key_dtor(mod_hash_key_t key)
{
	char		*path_addr;
	struct di_key	*dik = key;

	switch (dik->k_type) {
	case DI_DKEY:
		break;
	case DI_PKEY:
		path_addr = dik->k_u.pkey.pk_path_addr;
		if (path_addr)
			kmem_free(path_addr, strlen(path_addr) + 1);
		break;
	default:
		panic("devinfo: unknown key type");
		/*NOTREACHED*/
	}

	kmem_free(dik, sizeof (struct di_key));
}

static int
di_dkey_cmp(struct di_dkey *dk1, struct di_dkey *dk2)
{
	if (dk1->dk_dip !=  dk2->dk_dip)
		return (dk1->dk_dip > dk2->dk_dip ? 1 : -1);

	if (dk1->dk_major != (major_t)-1 && dk2->dk_major != (major_t)-1) {
		if (dk1->dk_major !=  dk2->dk_major)
			return (dk1->dk_major > dk2->dk_major ? 1 : -1);

		if (dk1->dk_inst !=  dk2->dk_inst)
			return (dk1->dk_inst > dk2->dk_inst ? 1 : -1);
	}

	if (dk1->dk_nodeid != dk2->dk_nodeid)
		return (dk1->dk_nodeid > dk2->dk_nodeid ? 1 : -1);

	return (0);
}

static int
di_pkey_cmp(struct di_pkey *pk1, struct di_pkey *pk2)
{
	char *p1, *p2;
	int rv;

	if (pk1->pk_pip !=  pk2->pk_pip)
		return (pk1->pk_pip > pk2->pk_pip ? 1 : -1);

	p1 = pk1->pk_path_addr;
	p2 = pk2->pk_path_addr;

	p1 = p1 ? p1 : "";
	p2 = p2 ? p2 : "";

	rv = strcmp(p1, p2);
	if (rv)
		return (rv > 0  ? 1 : -1);

	if (pk1->pk_client !=  pk2->pk_client)
		return (pk1->pk_client > pk2->pk_client ? 1 : -1);

	if (pk1->pk_phci !=  pk2->pk_phci)
		return (pk1->pk_phci > pk2->pk_phci ? 1 : -1);

	return (0);
}

static int
di_key_cmp(mod_hash_key_t key1, mod_hash_key_t key2)
{
	struct di_key *dik1, *dik2;

	dik1 = key1;
	dik2 = key2;

	if (dik1->k_type != dik2->k_type) {
		panic("devinfo: mismatched keys");
		/*NOTREACHED*/
	}

	switch (dik1->k_type) {
	case DI_DKEY:
		return (di_dkey_cmp(&(dik1->k_u.dkey), &(dik2->k_u.dkey)));
	case DI_PKEY:
		return (di_pkey_cmp(&(dik1->k_u.pkey), &(dik2->k_u.pkey)));
	default:
		panic("devinfo: unknown key type");
		/*NOTREACHED*/
	}
}

/*
 * This is the main function that takes a snapshot
 */
static di_off_t
di_snapshot(struct di_state *st)
{
	di_off_t off;
	struct di_all *all;
	dev_info_t *rootnode;
	char buf[80];
	int plen;
	char *path;
	vnode_t *vp;

	all = (struct di_all *)(intptr_t)di_mem_addr(st, 0);
	dcmn_err((CE_CONT, "Taking a snapshot of devinfo tree...\n"));

	/*
	 * Verify path before entrusting it to e_ddi_hold_devi_by_path because
	 * some platforms have OBP bugs where executing the NDI_PROMNAME code
	 * path against an invalid path results in panic.  The lookupnameat
	 * is done relative to rootdir without a leading '/' on "devices/"
	 * to force the lookup to occur in the global zone.
	 */
	plen = strlen("devices/") + strlen(all->root_path) + 1;
	path = kmem_alloc(plen, KM_SLEEP);
	(void) snprintf(path, plen, "devices/%s", all->root_path);
	if (lookupnameat(path, UIO_SYSSPACE, FOLLOW, NULLVPP, &vp, rootdir)) {
		dcmn_err((CE_CONT, "Devinfo node %s not found\n",
		    all->root_path));
		kmem_free(path, plen);
		return (0);
	}
	kmem_free(path, plen);
	VN_RELE(vp);

	/*
	 * Hold the devinfo node referred by the path.
	 */
	rootnode = e_ddi_hold_devi_by_path(all->root_path, 0);
	if (rootnode == NULL) {
		dcmn_err((CE_CONT, "Devinfo node %s not found\n",
		    all->root_path));
		return (0);
	}

	(void) snprintf(buf, sizeof (buf),
	    "devinfo registered dips (statep=%p)", (void *)st);

	st->reg_dip_hash = mod_hash_create_extended(buf, 64,
	    di_key_dtor, mod_hash_null_valdtor, di_hash_byptr,
	    NULL, di_key_cmp, KM_SLEEP);


	(void) snprintf(buf, sizeof (buf),
	    "devinfo registered pips (statep=%p)", (void *)st);

	st->reg_pip_hash = mod_hash_create_extended(buf, 64,
	    di_key_dtor, mod_hash_null_valdtor, di_hash_byptr,
	    NULL, di_key_cmp, KM_SLEEP);

	/*
	 * copy the device tree
	 */
	off = di_copytree(DEVI(rootnode), &all->top_devinfo, st);

	if (DINFOPATH & st->command) {
		mdi_walk_vhcis(build_vhci_list, st);
	}

	ddi_release_devi(rootnode);

	/*
	 * copy the devnames array
	 */
	all->devnames = off;
	off = di_copydevnm(&all->devnames, st);


	/* initialize the hash tables */
	st->lnode_count = 0;
	st->link_count = 0;

	if (DINFOLYR & st->command) {
		off = di_getlink_data(off, st);
	}

	/*
	 * Free up hash tables
	 */
	mod_hash_destroy_hash(st->reg_dip_hash);
	mod_hash_destroy_hash(st->reg_pip_hash);

	/*
	 * Record the timestamp now that we are done with snapshot.
	 *
	 * We compute the checksum later and then only if we cache
	 * the snapshot, since checksumming adds some overhead.
	 * The checksum is checked later if we read the cache file.
	 * from disk.
	 *
	 * Set checksum field to 0 as CRC is calculated with that
	 * field set to 0.
	 */
	all->snapshot_time = ddi_get_time();
	all->cache_checksum = 0;

	ASSERT(all->snapshot_time != 0);

	return (off);
}

/*
 * Take a snapshot and clean /etc/devices files if DINFOCLEANUP is set
 */
static di_off_t
di_snapshot_and_clean(struct di_state *st)
{
	di_off_t	off;

	modunload_disable();
	off = di_snapshot(st);
	if (off != 0 && (st->command & DINFOCLEANUP)) {
		ASSERT(DEVICES_FILES_CLEANABLE(st));
		/*
		 * Cleanup /etc/devices files:
		 * In order to accurately account for the system configuration
		 * in /etc/devices files, the appropriate drivers must be
		 * fully configured before the cleanup starts.
		 * So enable modunload only after the cleanup.
		 */
		i_ddi_clean_devices_files();
		/*
		 * Remove backing store nodes for unused devices,
		 * which retain past permissions customizations
		 * and may be undesired for newly configured devices.
		 */
		dev_devices_cleanup();
	}
	modunload_enable();

	return (off);
}

/*
 * construct vhci linkage in the snapshot.
 */
int
build_vhci_list(dev_info_t *vh_devinfo, void *arg)
{
	struct di_all *all;
	struct di_node *me;
	struct di_state *st;
	di_off_t off;
	phci_walk_arg_t pwa;

	dcmn_err3((CE_CONT, "build_vhci list\n"));

	dcmn_err3((CE_CONT, "vhci node %s, instance #%d\n",
	    DEVI(vh_devinfo)->devi_node_name,
	    DEVI(vh_devinfo)->devi_instance));

	st = (struct di_state *)arg;
	if (di_dip_find(st, vh_devinfo, &off) != 0) {
		dcmn_err((CE_WARN, "di_dip_find error for the given node\n"));
		return (DDI_WALK_TERMINATE);
	}

	dcmn_err3((CE_CONT, "st->mem_size: %d vh_devinfo off: 0x%x\n",
	    st->mem_size, off));

	all = (struct di_all *)(intptr_t)di_mem_addr(st, 0);
	if (all->top_vhci_devinfo == 0) {
		all->top_vhci_devinfo = off;
	} else {
		me = (struct di_node *)
		    (intptr_t)di_mem_addr(st, all->top_vhci_devinfo);

		while (me->next_vhci != 0) {
			me = (struct di_node *)
			    (intptr_t)di_mem_addr(st, me->next_vhci);
		}

		me->next_vhci = off;
	}

	pwa.off = off;
	pwa.st = st;
	mdi_vhci_walk_phcis(vh_devinfo, build_phci_list, &pwa);

	return (DDI_WALK_CONTINUE);
}

/*
 * construct phci linkage for the given vhci in the snapshot.
 */
int
build_phci_list(dev_info_t *ph_devinfo, void *arg)
{
	struct di_node *vh_di_node;
	struct di_node *me;
	phci_walk_arg_t *pwa;
	di_off_t off;

	pwa = (phci_walk_arg_t *)arg;

	dcmn_err3((CE_CONT, "build_phci list for vhci at offset: 0x%x\n",
	    pwa->off));

	vh_di_node = (struct di_node *)(intptr_t)di_mem_addr(pwa->st, pwa->off);

	if (di_dip_find(pwa->st, ph_devinfo, &off) != 0) {
		dcmn_err((CE_WARN, "di_dip_find error for the given node\n"));
		return (DDI_WALK_TERMINATE);
	}

	dcmn_err3((CE_CONT, "phci node %s, instance #%d, at offset 0x%x\n",
	    DEVI(ph_devinfo)->devi_node_name,
	    DEVI(ph_devinfo)->devi_instance, off));

	if (vh_di_node->top_phci == 0) {
		vh_di_node->top_phci = off;
		return (DDI_WALK_CONTINUE);
	}

	me = (struct di_node *)
	    (intptr_t)di_mem_addr(pwa->st, vh_di_node->top_phci);

	while (me->next_phci != 0) {
		me = (struct di_node *)
		    (intptr_t)di_mem_addr(pwa->st, me->next_phci);
	}
	me->next_phci = off;

	return (DDI_WALK_CONTINUE);
}

/*
 * Assumes all devinfo nodes in device tree have been snapshotted
 */
static void
snap_driver_list(struct di_state *st, struct devnames *dnp, di_off_t *poff_p)
{
	struct dev_info *node;
	struct di_node *me;
	di_off_t off;

	ASSERT(mutex_owned(&dnp->dn_lock));

	node = DEVI(dnp->dn_head);
	for (; node; node = node->devi_next) {
		if (di_dip_find(st, (dev_info_t *)node, &off) != 0)
			continue;

		ASSERT(off > 0);
		me = (struct di_node *)(intptr_t)di_mem_addr(st, off);
		ASSERT(me->next == 0 || me->next == -1);
		/*
		 * Only nodes which were BOUND when they were
		 * snapshotted will be added to per-driver list.
		 */
		if (me->next != -1)
			continue;

		*poff_p = off;
		poff_p = &me->next;
	}

	*poff_p = 0;
}

/*
 * Copy the devnames array, so we have a list of drivers in the snapshot.
 * Also makes it possible to locate the per-driver devinfo nodes.
 */
static di_off_t
di_copydevnm(di_off_t *off_p, struct di_state *st)
{
	int i;
	di_off_t off;
	size_t size;
	struct di_devnm *dnp;

	dcmn_err2((CE_CONT, "di_copydevnm: *off_p = %p\n", (void *)off_p));

	/*
	 * make sure there is some allocated memory
	 */
	size = devcnt * sizeof (struct di_devnm);
	off = di_checkmem(st, *off_p, size);
	*off_p = off;

	dcmn_err((CE_CONT, "Start copying devnamesp[%d] at offset 0x%x\n",
	    devcnt, off));

	dnp = (struct di_devnm *)(intptr_t)di_mem_addr(st, off);
	off += size;

	for (i = 0; i < devcnt; i++) {
		if (devnamesp[i].dn_name == NULL) {
			continue;
		}

		/*
		 * dn_name is not freed during driver unload or removal.
		 *
		 * There is a race condition when make_devname() changes
		 * dn_name during our strcpy. This should be rare since
		 * only add_drv does this. At any rate, we never had a
		 * problem with ddi_name_to_major(), which should have
		 * the same problem.
		 */
		dcmn_err2((CE_CONT, "di_copydevnm: %s%d, off=%x\n",
		    devnamesp[i].dn_name, devnamesp[i].dn_instance,
		    off));

		off = di_checkmem(st, off, strlen(devnamesp[i].dn_name) + 1);
		dnp[i].name = off;
		(void) strcpy((char *)di_mem_addr(st, off),
		    devnamesp[i].dn_name);
		off += DI_ALIGN(strlen(devnamesp[i].dn_name) + 1);

		mutex_enter(&devnamesp[i].dn_lock);

		/*
		 * Snapshot per-driver node list
		 */
		snap_driver_list(st, &devnamesp[i], &dnp[i].head);

		/*
		 * This is not used by libdevinfo, leave it for now
		 */
		dnp[i].flags = devnamesp[i].dn_flags;
		dnp[i].instance = devnamesp[i].dn_instance;

		/*
		 * get global properties
		 */
		if ((DINFOPROP & st->command) &&
		    devnamesp[i].dn_global_prop_ptr) {
			dnp[i].global_prop = off;
			off = di_getprop(
			    devnamesp[i].dn_global_prop_ptr->prop_list,
			    &dnp[i].global_prop, st, NULL, DI_PROP_GLB_LIST);
		}

		/*
		 * Bit encode driver ops: & bus_ops, cb_ops, & cb_ops->cb_str
		 */
		if (CB_DRV_INSTALLED(devopsp[i])) {
			if (devopsp[i]->devo_cb_ops) {
				dnp[i].ops |= DI_CB_OPS;
				if (devopsp[i]->devo_cb_ops->cb_str)
					dnp[i].ops |= DI_STREAM_OPS;
			}
			if (NEXUS_DRV(devopsp[i])) {
				dnp[i].ops |= DI_BUS_OPS;
			}
		}

		mutex_exit(&devnamesp[i].dn_lock);
	}

	dcmn_err((CE_CONT, "End copying devnamesp at offset 0x%x\n", off));

	return (off);
}

/*
 * Copy the kernel devinfo tree. The tree and the devnames array forms
 * the entire snapshot (see also di_copydevnm).
 */
static di_off_t
di_copytree(struct dev_info *root, di_off_t *off_p, struct di_state *st)
{
	di_off_t off;
	struct di_stack *dsp = kmem_zalloc(sizeof (struct di_stack), KM_SLEEP);

	dcmn_err((CE_CONT, "di_copytree: root = %p, *off_p = %x\n",
	    (void *)root, *off_p));

	/* force attach drivers */
	if (i_ddi_devi_attached((dev_info_t *)root) &&
	    (st->command & DINFOSUBTREE) && (st->command & DINFOFORCE)) {
		(void) ndi_devi_config((dev_info_t *)root,
		    NDI_CONFIG | NDI_DEVI_PERSIST | NDI_NO_EVENT |
		    NDI_DRV_CONF_REPROBE);
	}

	/*
	 * Push top_devinfo onto a stack
	 *
	 * The stack is necessary to avoid recursion, which can overrun
	 * the kernel stack.
	 */
	PUSH_STACK(dsp, root, off_p);

	/*
	 * As long as there is a node on the stack, copy the node.
	 * di_copynode() is responsible for pushing and popping
	 * child and sibling nodes on the stack.
	 */
	while (!EMPTY_STACK(dsp)) {
		off = di_copynode(dsp, st);
	}

	/*
	 * Free the stack structure
	 */
	kmem_free(dsp, sizeof (struct di_stack));

	return (off);
}

/*
 * This is the core function, which copies all data associated with a single
 * node into the snapshot. The amount of information is determined by the
 * ioctl command.
 */
static di_off_t
di_copynode(struct di_stack *dsp, struct di_state *st)
{
	di_off_t off;
	struct di_node *me;
	struct dev_info *node;

	dcmn_err2((CE_CONT, "di_copynode: depth = %x\n", dsp->depth));

	node = TOP_NODE(dsp);

	ASSERT(node != NULL);

	/*
	 * check memory usage, and fix offsets accordingly.
	 */
	off = di_checkmem(st, *(TOP_OFFSET(dsp)), sizeof (struct di_node));
	*(TOP_OFFSET(dsp)) = off;
	me = DI_NODE(di_mem_addr(st, off));

	dcmn_err((CE_CONT, "copy node %s, instance #%d, at offset 0x%x\n",
	    node->devi_node_name, node->devi_instance, off));

	/*
	 * Node parameters:
	 * self		-- offset of current node within snapshot
	 * nodeid	-- pointer to PROM node (tri-valued)
	 * state	-- hot plugging device state
	 * node_state	-- devinfo node state (CF1, CF2, etc.)
	 */
	me->self = off;
	me->instance = node->devi_instance;
	me->nodeid = node->devi_nodeid;
	me->node_class = node->devi_node_class;
	me->attributes = node->devi_node_attributes;
	me->state = node->devi_state;
	me->flags = node->devi_flags;
	me->node_state = node->devi_node_state;
	me->next_vhci = 0;		/* Filled up by build_vhci_list. */
	me->top_phci = 0;		/* Filled up by build_phci_list. */
	me->next_phci = 0;		/* Filled up by build_phci_list. */
	me->multipath_component = MULTIPATH_COMPONENT_NONE; /* set default. */
	me->user_private_data = NULL;

	/*
	 * Get parent's offset in snapshot from the stack
	 * and store it in the current node
	 */
	if (dsp->depth > 1) {
		me->parent = *(PARENT_OFFSET(dsp));
	}

	/*
	 * Save the offset of this di_node in a hash table.
	 * This is used later to resolve references to this
	 * dip from other parts of the tree (per-driver list,
	 * multipathing linkages, layered usage linkages).
	 * The key used for the hash table is derived from
	 * information in the dip.
	 */
	di_register_dip(st, (dev_info_t *)node, me->self);

	/*
	 * increment offset
	 */
	off += sizeof (struct di_node);

#ifdef	DEVID_COMPATIBILITY
	/* check for devid as property marker */
	if (node->devi_devid) {
		ddi_devid_t	devid;
		char 		*devidstr;
		int		devid_size;

		/*
		 * The devid is now represented as a property.
		 * For micro release compatibility with di_devid interface
		 * in libdevinfo we must return it as a binary structure in'
		 * the snapshot.  When di_devid is removed from libdevinfo
		 * in a future release (and devi_devid is deleted) then
		 * code related to DEVID_COMPATIBILITY can be removed.
		 */
		ASSERT(node->devi_devid == DEVID_COMPATIBILITY);
/* XXX should be DDI_DEV_T_NONE! */
		if (ddi_prop_lookup_string(DDI_DEV_T_ANY, (dev_info_t *)node,
		    DDI_PROP_DONTPASS, DEVID_PROP_NAME, &devidstr) ==
		    DDI_PROP_SUCCESS) {
			if (ddi_devid_str_decode(devidstr, &devid, NULL) ==
			    DDI_SUCCESS) {
				devid_size = ddi_devid_sizeof(devid);
				off = di_checkmem(st, off, devid_size);
				me->devid = off;
				bcopy(devid,
				    di_mem_addr(st, off), devid_size);
				off += devid_size;
				ddi_devid_free(devid);
			}
			ddi_prop_free(devidstr);
		}
	}
#endif	/* DEVID_COMPATIBILITY */

	if (node->devi_node_name) {
		off = di_checkmem(st, off, strlen(node->devi_node_name) + 1);
		me->node_name = off;
		(void) strcpy(di_mem_addr(st, off), node->devi_node_name);
		off += strlen(node->devi_node_name) + 1;
	}

	if (node->devi_compat_names && (node->devi_compat_length > 1)) {
		off = di_checkmem(st, off, node->devi_compat_length);
		me->compat_names = off;
		me->compat_length = node->devi_compat_length;
		bcopy(node->devi_compat_names, di_mem_addr(st, off),
		    node->devi_compat_length);
		off += node->devi_compat_length;
	}

	if (node->devi_addr) {
		off = di_checkmem(st, off, strlen(node->devi_addr) + 1);
		me->address = off;
		(void) strcpy(di_mem_addr(st, off), node->devi_addr);
		off += strlen(node->devi_addr) + 1;
	}

	if (node->devi_binding_name) {
		off = di_checkmem(st, off, strlen(node->devi_binding_name) + 1);
		me->bind_name = off;
		(void) strcpy(di_mem_addr(st, off), node->devi_binding_name);
		off += strlen(node->devi_binding_name) + 1;
	}

	me->drv_major = node->devi_major;

	/*
	 * If the dip is BOUND, set the next pointer of the
	 * per-instance list to -1, indicating that it is yet to be resolved.
	 * This will be resolved later in snap_driver_list().
	 */
	if (me->drv_major != -1) {
		me->next = -1;
	} else {
		me->next = 0;
	}

	/*
	 * An optimization to skip mutex_enter when not needed.
	 */
	if (!((DINFOMINOR | DINFOPROP | DINFOPATH) & st->command)) {
		goto priv_data;
	}

	/*
	 * Grab current per dev_info node lock to
	 * get minor data and properties.
	 */
	mutex_enter(&(node->devi_lock));

	if (!(DINFOMINOR & st->command)) {
		goto path;
	}

	if (node->devi_minor) {		/* minor data */
		me->minor_data = DI_ALIGN(off);
		off = di_getmdata(node->devi_minor, &me->minor_data,
		    me->self, st);
	}

path:
	if (!(DINFOPATH & st->command)) {
		goto property;
	}

	if (MDI_VHCI(node)) {
		me->multipath_component = MULTIPATH_COMPONENT_VHCI;
	}

	if (MDI_CLIENT(node)) {
		me->multipath_component = MULTIPATH_COMPONENT_CLIENT;
		me->multipath_client = DI_ALIGN(off);
		off = di_getpath_data((dev_info_t *)node, &me->multipath_client,
		    me->self, st, 1);
		dcmn_err((CE_WARN, "me->multipath_client = %x for node %p "
		    "component type = %d.  off=%d",
		    me->multipath_client,
		    (void *)node, node->devi_mdi_component, off));
	}

	if (MDI_PHCI(node)) {
		me->multipath_component = MULTIPATH_COMPONENT_PHCI;
		me->multipath_phci = DI_ALIGN(off);
		off = di_getpath_data((dev_info_t *)node, &me->multipath_phci,
		    me->self, st, 0);
		dcmn_err((CE_WARN, "me->multipath_phci = %x for node %p "
		    "component type = %d.  off=%d",
		    me->multipath_phci,
		    (void *)node, node->devi_mdi_component, off));
	}

property:
	if (!(DINFOPROP & st->command)) {
		goto unlock;
	}

	if (node->devi_drv_prop_ptr) {	/* driver property list */
		me->drv_prop = DI_ALIGN(off);
		off = di_getprop(node->devi_drv_prop_ptr, &me->drv_prop, st,
		    node, DI_PROP_DRV_LIST);
	}

	if (node->devi_sys_prop_ptr) {	/* system property list */
		me->sys_prop = DI_ALIGN(off);
		off = di_getprop(node->devi_sys_prop_ptr, &me->sys_prop, st,
		    node, DI_PROP_SYS_LIST);
	}

	if (node->devi_hw_prop_ptr) {	/* hardware property list */
		me->hw_prop = DI_ALIGN(off);
		off = di_getprop(node->devi_hw_prop_ptr, &me->hw_prop, st,
		    node, DI_PROP_HW_LIST);
	}

	if (node->devi_global_prop_list == NULL) {
		me->glob_prop = (di_off_t)-1;	/* not global property */
	} else {
		/*
		 * Make copy of global property list if this devinfo refers
		 * global properties different from what's on the devnames
		 * array. It can happen if there has been a forced
		 * driver.conf update. See mod_drv(1M).
		 */
		ASSERT(me->drv_major != -1);
		if (node->devi_global_prop_list !=
		    devnamesp[me->drv_major].dn_global_prop_ptr) {
			me->glob_prop = DI_ALIGN(off);
			off = di_getprop(node->devi_global_prop_list->prop_list,
			    &me->glob_prop, st, node, DI_PROP_GLB_LIST);
		}
	}

unlock:
	/*
	 * release current per dev_info node lock
	 */
	mutex_exit(&(node->devi_lock));

priv_data:
	if (!(DINFOPRIVDATA & st->command)) {
		goto pm_info;
	}

	if (ddi_get_parent_data((dev_info_t *)node) != NULL) {
		me->parent_data = DI_ALIGN(off);
		off = di_getppdata(node, &me->parent_data, st);
	}

	if (ddi_get_driver_private((dev_info_t *)node) != NULL) {
		me->driver_data = DI_ALIGN(off);
		off = di_getdpdata(node, &me->driver_data, st);
	}

pm_info: /* NOT implemented */

subtree:
	if (!(DINFOSUBTREE & st->command)) {
		POP_STACK(dsp);
		return (DI_ALIGN(off));
	}

child:
	/*
	 * If there is a child--push child onto stack.
	 * Hold the parent busy while doing so.
	 */
	if (node->devi_child) {
		me->child = DI_ALIGN(off);
		PUSH_STACK(dsp, node->devi_child, &me->child);
		return (me->child);
	}

sibling:
	/*
	 * no child node, unroll the stack till a sibling of
	 * a parent node is found or root node is reached
	 */
	POP_STACK(dsp);
	while (!EMPTY_STACK(dsp) && (node->devi_sibling == NULL)) {
		node = TOP_NODE(dsp);
		me = DI_NODE(di_mem_addr(st, *(TOP_OFFSET(dsp))));
		POP_STACK(dsp);
	}

	if (!EMPTY_STACK(dsp)) {
		/*
		 * a sibling is found, replace top of stack by its sibling
		 */
		me->sibling = DI_ALIGN(off);
		PUSH_STACK(dsp, node->devi_sibling, &me->sibling);
		return (me->sibling);
	}

	/*
	 * DONE with all nodes
	 */
	return (DI_ALIGN(off));
}

static i_lnode_t *
i_lnode_alloc(int modid)
{
	i_lnode_t	*i_lnode;

	i_lnode = kmem_zalloc(sizeof (i_lnode_t), KM_SLEEP);

	ASSERT(modid != -1);
	i_lnode->modid = modid;

	return (i_lnode);
}

static void
i_lnode_free(i_lnode_t *i_lnode)
{
	kmem_free(i_lnode, sizeof (i_lnode_t));
}

static void
i_lnode_check_free(i_lnode_t *i_lnode)
{
	/* This lnode and its dip must have been snapshotted */
	ASSERT(i_lnode->self > 0);
	ASSERT(i_lnode->di_node->self > 0);

	/* at least 1 link (in or out) must exist for this lnode */
	ASSERT(i_lnode->link_in || i_lnode->link_out);

	i_lnode_free(i_lnode);
}

static i_link_t *
i_link_alloc(int spec_type)
{
	i_link_t *i_link;

	i_link = kmem_zalloc(sizeof (i_link_t), KM_SLEEP);
	i_link->spec_type = spec_type;

	return (i_link);
}

static void
i_link_check_free(i_link_t *i_link)
{
	/* This link must have been snapshotted */
	ASSERT(i_link->self > 0);

	/* Both endpoint lnodes must exist for this link */
	ASSERT(i_link->src_lnode);
	ASSERT(i_link->tgt_lnode);

	kmem_free(i_link, sizeof (i_link_t));
}

/*ARGSUSED*/
static uint_t
i_lnode_hashfunc(void *arg, mod_hash_key_t key)
{
	i_lnode_t	*i_lnode = (i_lnode_t *)key;
	struct di_node	*ptr;
	dev_t		dev;

	dev = i_lnode->devt;
	if (dev != DDI_DEV_T_NONE)
		return (i_lnode->modid + getminor(dev) + getmajor(dev));

	ptr = i_lnode->di_node;
	ASSERT(ptr->self > 0);
	if (ptr) {
		uintptr_t k = (uintptr_t)ptr;
		k >>= (int)highbit(sizeof (struct di_node));
		return ((uint_t)k);
	}

	return (i_lnode->modid);
}

static int
i_lnode_cmp(void *arg1, void *arg2)
{
	i_lnode_t	*i_lnode1 = (i_lnode_t *)arg1;
	i_lnode_t	*i_lnode2 = (i_lnode_t *)arg2;

	if (i_lnode1->modid != i_lnode2->modid) {
		return ((i_lnode1->modid < i_lnode2->modid) ? -1 : 1);
	}

	if (i_lnode1->di_node != i_lnode2->di_node)
		return ((i_lnode1->di_node < i_lnode2->di_node) ? -1 : 1);

	if (i_lnode1->devt != i_lnode2->devt)
		return ((i_lnode1->devt < i_lnode2->devt) ? -1 : 1);

	return (0);
}

/*
 * An lnode represents a {dip, dev_t} tuple. A link represents a
 * {src_lnode, tgt_lnode, spec_type} tuple.
 * The following callback assumes that LDI framework ref-counts the
 * src_dip and tgt_dip while invoking this callback.
 */
static int
di_ldi_callback(const ldi_usage_t *ldi_usage, void *arg)
{
	struct di_state	*st = (struct di_state *)arg;
	i_lnode_t	*src_lnode, *tgt_lnode, *i_lnode;
	i_link_t	**i_link_next, *i_link;
	di_off_t	soff, toff;
	mod_hash_val_t	nodep = NULL;
	int		res;

	/*
	 * if the source or target of this device usage information doesn't
	 * corrospond to a device node then we don't report it via
	 * libdevinfo so return.
	 */
	if ((ldi_usage->src_dip == NULL) || (ldi_usage->tgt_dip == NULL))
		return (LDI_USAGE_CONTINUE);

	ASSERT(e_ddi_devi_holdcnt(ldi_usage->src_dip));
	ASSERT(e_ddi_devi_holdcnt(ldi_usage->tgt_dip));

	/*
	 * Skip the ldi_usage if either src or tgt dip is not in the
	 * snapshot. This saves us from pruning bad lnodes/links later.
	 */
	if (di_dip_find(st, ldi_usage->src_dip, &soff) != 0)
		return (LDI_USAGE_CONTINUE);
	if (di_dip_find(st, ldi_usage->tgt_dip, &toff) != 0)
		return (LDI_USAGE_CONTINUE);

	ASSERT(soff > 0);
	ASSERT(toff > 0);

	/*
	 * allocate an i_lnode and add it to the lnode hash
	 * if it is not already present. For this particular
	 * link the lnode is a source, but it may
	 * participate as tgt or src in any number of layered
	 * operations - so it may already be in the hash.
	 */
	i_lnode = i_lnode_alloc(ldi_usage->src_modid);
	i_lnode->di_node = (struct di_node *)(intptr_t)di_mem_addr(st, soff);
	i_lnode->devt = ldi_usage->src_devt;

	res = mod_hash_find(st->lnode_hash, i_lnode, &nodep);
	if (res == MH_ERR_NOTFOUND) {
		/*
		 * new i_lnode
		 * add it to the hash and increment the lnode count
		 */
		res = mod_hash_insert(st->lnode_hash, i_lnode, i_lnode);
		ASSERT(res == 0);
		st->lnode_count++;
		src_lnode = i_lnode;
	} else {
		/* this i_lnode already exists in the lnode_hash */
		i_lnode_free(i_lnode);
		src_lnode = (i_lnode_t *)nodep;
	}

	/*
	 * allocate a tgt i_lnode and add it to the lnode hash
	 */
	i_lnode = i_lnode_alloc(ldi_usage->tgt_modid);
	i_lnode->di_node = (struct di_node *)(intptr_t)di_mem_addr(st, toff);
	i_lnode->devt = ldi_usage->tgt_devt;

	res = mod_hash_find(st->lnode_hash, i_lnode, &nodep);
	if (res == MH_ERR_NOTFOUND) {
		/*
		 * new i_lnode
		 * add it to the hash and increment the lnode count
		 */
		res = mod_hash_insert(st->lnode_hash, i_lnode, i_lnode);
		ASSERT(res == 0);
		st->lnode_count++;
		tgt_lnode = i_lnode;
	} else {
		/* this i_lnode already exists in the lnode_hash */
		i_lnode_free(i_lnode);
		tgt_lnode = (i_lnode_t *)nodep;
	}

	/*
	 * allocate a i_link
	 */
	i_link = i_link_alloc(ldi_usage->tgt_spec_type);
	i_link->src_lnode = src_lnode;
	i_link->tgt_lnode = tgt_lnode;

	/*
	 * add this link onto the src i_lnodes outbound i_link list
	 */
	i_link_next = &(src_lnode->link_out);
	while (*i_link_next != NULL) {
		if ((i_lnode_cmp(tgt_lnode, (*i_link_next)->tgt_lnode) == 0) &&
		    (i_link->spec_type == (*i_link_next)->spec_type)) {
			/* this link already exists */
			kmem_free(i_link, sizeof (i_link_t));
			return (LDI_USAGE_CONTINUE);
		}
		i_link_next = &((*i_link_next)->src_link_next);
	}
	*i_link_next = i_link;

	/*
	 * add this link onto the tgt i_lnodes inbound i_link list
	 */
	i_link_next = &(tgt_lnode->link_in);
	while (*i_link_next != NULL) {
		ASSERT(i_lnode_cmp(src_lnode, (*i_link_next)->src_lnode) != 0);
		i_link_next = &((*i_link_next)->tgt_link_next);
	}
	*i_link_next = i_link;

	/*
	 * add this i_link to the link hash
	 */
	res = mod_hash_insert(st->link_hash, i_link, i_link);
	ASSERT(res == 0);
	st->link_count++;

	return (LDI_USAGE_CONTINUE);
}

struct i_layer_data {
	struct di_state	*st;
	int		lnode_count;
	int		link_count;
	di_off_t	lnode_off;
	di_off_t 	link_off;
};

/*ARGSUSED*/
static uint_t
i_link_walker(mod_hash_key_t key, mod_hash_val_t *val, void *arg)
{
	i_link_t		*i_link  = (i_link_t *)key;
	struct i_layer_data	*data = arg;
	struct di_link		*me;
	struct di_lnode		*melnode;
	struct di_node		*medinode;

	ASSERT(i_link->self == 0);

	i_link->self = data->link_off +
	    (data->link_count * sizeof (struct di_link));
	data->link_count++;

	ASSERT(data->link_off > 0 && data->link_count > 0);
	ASSERT(data->lnode_count == data->st->lnode_count); /* lnodes done */
	ASSERT(data->link_count <= data->st->link_count);

	/* fill in fields for the di_link snapshot */
	me = (struct di_link *)(intptr_t)di_mem_addr(data->st, i_link->self);
	me->self = i_link->self;
	me->spec_type = i_link->spec_type;

	/*
	 * The src_lnode and tgt_lnode i_lnode_t for this i_link_t
	 * are created during the LDI table walk. Since we are
	 * walking the link hash, the lnode hash has already been
	 * walked and the lnodes have been snapshotted. Save lnode
	 * offsets.
	 */
	me->src_lnode = i_link->src_lnode->self;
	me->tgt_lnode = i_link->tgt_lnode->self;

	/*
	 * Save this link's offset in the src_lnode snapshot's link_out
	 * field
	 */
	melnode = (struct di_lnode *)
	    (intptr_t)di_mem_addr(data->st, me->src_lnode);
	me->src_link_next = melnode->link_out;
	melnode->link_out = me->self;

	/*
	 * Put this link on the tgt_lnode's link_in field
	 */
	melnode = (struct di_lnode *)
	    (intptr_t)di_mem_addr(data->st, me->tgt_lnode);
	me->tgt_link_next = melnode->link_in;
	melnode->link_in = me->self;

	/*
	 * An i_lnode_t is only created if the corresponding dip exists
	 * in the snapshot. A pointer to the di_node is saved in the
	 * i_lnode_t when it is allocated. For this link, get the di_node
	 * for the source lnode. Then put the link on the di_node's list
	 * of src links
	 */
	medinode = i_link->src_lnode->di_node;
	me->src_node_next = medinode->src_links;
	medinode->src_links = me->self;

	/*
	 * Put this link on the tgt_links list of the target
	 * dip.
	 */
	medinode = i_link->tgt_lnode->di_node;
	me->tgt_node_next = medinode->tgt_links;
	medinode->tgt_links = me->self;

	return (MH_WALK_CONTINUE);
}

/*ARGSUSED*/
static uint_t
i_lnode_walker(mod_hash_key_t key, mod_hash_val_t *val, void *arg)
{
	i_lnode_t		*i_lnode = (i_lnode_t *)key;
	struct i_layer_data	*data = arg;
	struct di_lnode		*me;
	struct di_node		*medinode;

	ASSERT(i_lnode->self == 0);

	i_lnode->self = data->lnode_off +
	    (data->lnode_count * sizeof (struct di_lnode));
	data->lnode_count++;

	ASSERT(data->lnode_off > 0 && data->lnode_count > 0);
	ASSERT(data->link_count == 0); /* links not done yet */
	ASSERT(data->lnode_count <= data->st->lnode_count);

	/* fill in fields for the di_lnode snapshot */
	me = (struct di_lnode *)(intptr_t)di_mem_addr(data->st, i_lnode->self);
	me->self = i_lnode->self;

	if (i_lnode->devt == DDI_DEV_T_NONE) {
		me->dev_major = (major_t)-1;
		me->dev_minor = (minor_t)-1;
	} else {
		me->dev_major = getmajor(i_lnode->devt);
		me->dev_minor = getminor(i_lnode->devt);
	}

	/*
	 * The dip corresponding to this lnode must exist in
	 * the snapshot or we wouldn't have created the i_lnode_t
	 * during LDI walk. Save the offset of the dip.
	 */
	ASSERT(i_lnode->di_node && i_lnode->di_node->self > 0);
	me->node = i_lnode->di_node->self;

	/*
	 * There must be at least one link in or out of this lnode
	 * or we wouldn't have created it. These fields will be set
	 * during the link hash walk.
	 */
	ASSERT((i_lnode->link_in != NULL) || (i_lnode->link_out != NULL));

	/*
	 * set the offset of the devinfo node associated with this
	 * lnode. Also update the node_next next pointer.  this pointer
	 * is set if there are multiple lnodes associated with the same
	 * devinfo node.  (could occure when multiple minor nodes
	 * are open for one device, etc.)
	 */
	medinode = i_lnode->di_node;
	me->node_next = medinode->lnodes;
	medinode->lnodes = me->self;

	return (MH_WALK_CONTINUE);
}

static di_off_t
di_getlink_data(di_off_t off, struct di_state *st)
{
	struct i_layer_data data = {0};
	size_t size;

	dcmn_err2((CE_CONT, "di_copylyr: off = %x\n", off));

	st->lnode_hash = mod_hash_create_extended("di_lnode_hash", 32,
	    mod_hash_null_keydtor, (void (*)(mod_hash_val_t))i_lnode_check_free,
	    i_lnode_hashfunc, NULL, i_lnode_cmp, KM_SLEEP);

	st->link_hash = mod_hash_create_ptrhash("di_link_hash", 32,
	    (void (*)(mod_hash_val_t))i_link_check_free, sizeof (i_link_t));

	/* get driver layering information */
	(void) ldi_usage_walker(st, di_ldi_callback);

	/* check if there is any link data to include in the snapshot */
	if (st->lnode_count == 0) {
		ASSERT(st->link_count == 0);
		goto out;
	}

	ASSERT(st->link_count != 0);

	/* get a pointer to snapshot memory for all the di_lnodes */
	size = sizeof (struct di_lnode) * st->lnode_count;
	data.lnode_off = off = di_checkmem(st, off, size);
	off += DI_ALIGN(size);

	/* get a pointer to snapshot memory for all the di_links */
	size = sizeof (struct di_link) * st->link_count;
	data.link_off = off = di_checkmem(st, off, size);
	off += DI_ALIGN(size);

	data.lnode_count = data.link_count = 0;
	data.st = st;

	/*
	 * We have lnodes and links that will go into the
	 * snapshot, so let's walk the respective hashes
	 * and snapshot them. The various linkages are
	 * also set up during the walk.
	 */
	mod_hash_walk(st->lnode_hash, i_lnode_walker, (void *)&data);
	ASSERT(data.lnode_count == st->lnode_count);

	mod_hash_walk(st->link_hash, i_link_walker, (void *)&data);
	ASSERT(data.link_count == st->link_count);

out:
	/* free up the i_lnodes and i_links used to create the snapshot */
	mod_hash_destroy_hash(st->lnode_hash);
	mod_hash_destroy_hash(st->link_hash);
	st->lnode_count = 0;
	st->link_count = 0;

	return (off);
}


/*
 * Copy all minor data nodes attached to a devinfo node into the snapshot.
 * It is called from di_copynode with devi_lock held.
 */
static di_off_t
di_getmdata(struct ddi_minor_data *mnode, di_off_t *off_p, di_off_t node,
	struct di_state *st)
{
	di_off_t off;
	struct di_minor *me;

	dcmn_err2((CE_CONT, "di_getmdata:\n"));

	/*
	 * check memory first
	 */
	off = di_checkmem(st, *off_p, sizeof (struct di_minor));
	*off_p = off;

	do {
		me = (struct di_minor *)(intptr_t)di_mem_addr(st, off);
		me->self = off;
		me->type = mnode->type;
		me->node = node;
		me->user_private_data = NULL;

		off += DI_ALIGN(sizeof (struct di_minor));

		/*
		 * Split dev_t to major/minor, so it works for
		 * both ILP32 and LP64 model
		 */
		me->dev_major = getmajor(mnode->ddm_dev);
		me->dev_minor = getminor(mnode->ddm_dev);
		me->spec_type = mnode->ddm_spec_type;

		if (mnode->ddm_name) {
			off = di_checkmem(st, off,
			    strlen(mnode->ddm_name) + 1);
			me->name = off;
			(void) strcpy(di_mem_addr(st, off), mnode->ddm_name);
			off += DI_ALIGN(strlen(mnode->ddm_name) + 1);
		}

		if (mnode->ddm_node_type) {
			off = di_checkmem(st, off,
			    strlen(mnode->ddm_node_type) + 1);
			me->node_type = off;
			(void) strcpy(di_mem_addr(st, off),
			    mnode->ddm_node_type);
			off += DI_ALIGN(strlen(mnode->ddm_node_type) + 1);
		}

		off = di_checkmem(st, off, sizeof (struct di_minor));
		me->next = off;
		mnode = mnode->next;
	} while (mnode);

	me->next = 0;

	return (off);
}

/*
 * di_register_dip(), di_find_dip(): The dip must be protected
 * from deallocation when using these routines - this can either
 * be a reference count, a busy hold or a per-driver lock.
 */

static void
di_register_dip(struct di_state *st, dev_info_t *dip, di_off_t off)
{
	struct dev_info *node = DEVI(dip);
	struct di_key *key = kmem_zalloc(sizeof (*key), KM_SLEEP);
	struct di_dkey *dk;

	ASSERT(dip);
	ASSERT(off > 0);

	key->k_type = DI_DKEY;
	dk = &(key->k_u.dkey);

	dk->dk_dip = dip;
	dk->dk_major = node->devi_major;
	dk->dk_inst = node->devi_instance;
	dk->dk_nodeid = node->devi_nodeid;

	if (mod_hash_insert(st->reg_dip_hash, (mod_hash_key_t)key,
	    (mod_hash_val_t)(uintptr_t)off) != 0) {
		panic(
		    "duplicate devinfo (%p) registered during device "
		    "tree walk", (void *)dip);
	}
}


static int
di_dip_find(struct di_state *st, dev_info_t *dip, di_off_t *off_p)
{
	/*
	 * uintptr_t must be used because it matches the size of void *;
	 * mod_hash expects clients to place results into pointer-size
	 * containers; since di_off_t is always a 32-bit offset, alignment
	 * would otherwise be broken on 64-bit kernels.
	 */
	uintptr_t	offset;
	struct		di_key key = {0};
	struct		di_dkey *dk;

	ASSERT(st->reg_dip_hash);
	ASSERT(dip);
	ASSERT(off_p);


	key.k_type = DI_DKEY;
	dk = &(key.k_u.dkey);

	dk->dk_dip = dip;
	dk->dk_major = DEVI(dip)->devi_major;
	dk->dk_inst = DEVI(dip)->devi_instance;
	dk->dk_nodeid = DEVI(dip)->devi_nodeid;

	if (mod_hash_find(st->reg_dip_hash, (mod_hash_key_t)&key,
	    (mod_hash_val_t *)&offset) == 0) {
		*off_p = (di_off_t)offset;
		return (0);
	} else {
		return (-1);
	}
}

/*
 * di_register_pip(), di_find_pip(): The pip must be protected from deallocation
 * when using these routines. The caller must do this by protecting the
 * client(or phci)<->pip linkage while traversing the list and then holding the
 * pip when it is found in the list.
 */

static void
di_register_pip(struct di_state *st, mdi_pathinfo_t *pip, di_off_t off)
{
	struct di_key	*key = kmem_zalloc(sizeof (*key), KM_SLEEP);
	char		*path_addr;
	struct di_pkey	*pk;

	ASSERT(pip);
	ASSERT(off > 0);

	key->k_type = DI_PKEY;
	pk = &(key->k_u.pkey);

	pk->pk_pip = pip;
	path_addr = mdi_pi_get_addr(pip);
	if (path_addr)
		pk->pk_path_addr = i_ddi_strdup(path_addr, KM_SLEEP);
	pk->pk_client = mdi_pi_get_client(pip);
	pk->pk_phci = mdi_pi_get_phci(pip);

	if (mod_hash_insert(st->reg_pip_hash, (mod_hash_key_t)key,
	    (mod_hash_val_t)(uintptr_t)off) != 0) {
		panic(
		    "duplicate pathinfo (%p) registered during device "
		    "tree walk", (void *)pip);
	}
}

/*
 * As with di_register_pip, the caller must hold or lock the pip
 */
static int
di_pip_find(struct di_state *st, mdi_pathinfo_t *pip, di_off_t *off_p)
{
	/*
	 * uintptr_t must be used because it matches the size of void *;
	 * mod_hash expects clients to place results into pointer-size
	 * containers; since di_off_t is always a 32-bit offset, alignment
	 * would otherwise be broken on 64-bit kernels.
	 */
	uintptr_t	offset;
	struct di_key	key = {0};
	struct di_pkey	*pk;

	ASSERT(st->reg_pip_hash);
	ASSERT(off_p);

	if (pip == NULL) {
		*off_p = 0;
		return (0);
	}

	key.k_type = DI_PKEY;
	pk = &(key.k_u.pkey);

	pk->pk_pip = pip;
	pk->pk_path_addr = mdi_pi_get_addr(pip);
	pk->pk_client = mdi_pi_get_client(pip);
	pk->pk_phci = mdi_pi_get_phci(pip);

	if (mod_hash_find(st->reg_pip_hash, (mod_hash_key_t)&key,
	    (mod_hash_val_t *)&offset) == 0) {
		*off_p = (di_off_t)offset;
		return (0);
	} else {
		return (-1);
	}
}

static di_path_state_t
path_state_convert(mdi_pathinfo_state_t st)
{
	switch (st) {
	case MDI_PATHINFO_STATE_ONLINE:
		return (DI_PATH_STATE_ONLINE);
	case MDI_PATHINFO_STATE_STANDBY:
		return (DI_PATH_STATE_STANDBY);
	case MDI_PATHINFO_STATE_OFFLINE:
		return (DI_PATH_STATE_OFFLINE);
	case MDI_PATHINFO_STATE_FAULT:
		return (DI_PATH_STATE_FAULT);
	default:
		return (DI_PATH_STATE_UNKNOWN);
	}
}


static di_off_t
di_path_getprop(mdi_pathinfo_t *pip, di_off_t off, di_off_t *off_p,
    struct di_state *st)
{
	nvpair_t *prop = NULL;
	struct di_path_prop *me;

	if (mdi_pi_get_next_prop(pip, NULL) == NULL) {
		*off_p = 0;
		return (off);
	}

	off = di_checkmem(st, off, sizeof (struct di_path_prop));
	*off_p = off;

	while (prop = mdi_pi_get_next_prop(pip, prop)) {
		int delta = 0;

		me = (struct di_path_prop *)(intptr_t)di_mem_addr(st, off);
		me->self = off;
		off += sizeof (struct di_path_prop);

		/*
		 * property name
		 */
		off = di_checkmem(st, off, strlen(nvpair_name(prop)) + 1);
		me->prop_name = off;
		(void) strcpy(di_mem_addr(st, off), nvpair_name(prop));
		off += strlen(nvpair_name(prop)) + 1;

		switch (nvpair_type(prop)) {
		case DATA_TYPE_BYTE:
		case DATA_TYPE_INT16:
		case DATA_TYPE_UINT16:
		case DATA_TYPE_INT32:
		case DATA_TYPE_UINT32:
			delta = sizeof (int32_t);
			me->prop_type = DDI_PROP_TYPE_INT;
			off = di_checkmem(st, off, delta);
			(void) nvpair_value_int32(prop,
			    (int32_t *)(intptr_t)di_mem_addr(st, off));
			break;

		case DATA_TYPE_INT64:
		case DATA_TYPE_UINT64:
			delta = sizeof (int64_t);
			me->prop_type = DDI_PROP_TYPE_INT64;
			off = di_checkmem(st, off, delta);
			(void) nvpair_value_int64(prop,
			    (int64_t *)(intptr_t)di_mem_addr(st, off));
			break;

		case DATA_TYPE_STRING:
		{
			char *str;
			(void) nvpair_value_string(prop, &str);
			delta = strlen(str) + 1;
			me->prop_type = DDI_PROP_TYPE_STRING;
			off = di_checkmem(st, off, delta);
			(void) strcpy(di_mem_addr(st, off), str);
			break;
		}
		case DATA_TYPE_BYTE_ARRAY:
		case DATA_TYPE_INT16_ARRAY:
		case DATA_TYPE_UINT16_ARRAY:
		case DATA_TYPE_INT32_ARRAY:
		case DATA_TYPE_UINT32_ARRAY:
		case DATA_TYPE_INT64_ARRAY:
		case DATA_TYPE_UINT64_ARRAY:
		{
			uchar_t *buf;
			uint_t nelems;
			(void) nvpair_value_byte_array(prop, &buf, &nelems);
			delta = nelems;
			me->prop_type = DDI_PROP_TYPE_BYTE;
			if (nelems != 0) {
				off = di_checkmem(st, off, delta);
				bcopy(buf, di_mem_addr(st, off), nelems);
			}
			break;
		}

		default:	/* Unknown or unhandled type; skip it */
			delta = 0;
			break;
		}

		if (delta > 0) {
			me->prop_data = off;
		}

		me->prop_len = delta;
		off += delta;

		off = di_checkmem(st, off, sizeof (struct di_path_prop));
		me->prop_next = off;
	}

	me->prop_next = 0;
	return (off);
}


static void
di_path_one_endpoint(struct di_path *me, di_off_t noff, di_off_t **off_pp,
    int get_client)
{
	if (get_client) {
		ASSERT(me->path_client == 0);
		me->path_client = noff;
		ASSERT(me->path_c_link == 0);
		*off_pp = &me->path_c_link;
		me->path_snap_state &=
		    ~(DI_PATH_SNAP_NOCLIENT | DI_PATH_SNAP_NOCLINK);
	} else {
		ASSERT(me->path_phci == 0);
		me->path_phci = noff;
		ASSERT(me->path_p_link == 0);
		*off_pp = &me->path_p_link;
		me->path_snap_state &=
		    ~(DI_PATH_SNAP_NOPHCI | DI_PATH_SNAP_NOPLINK);
	}
}

/*
 * poff_p: pointer to the linkage field. This links pips along the client|phci
 *	   linkage list.
 * noff  : Offset for the endpoint dip snapshot.
 */
static di_off_t
di_getpath_data(dev_info_t *dip, di_off_t *poff_p, di_off_t noff,
    struct di_state *st, int get_client)
{
	di_off_t off;
	mdi_pathinfo_t *pip;
	struct di_path *me;
	mdi_pathinfo_t *(*next_pip)(dev_info_t *, mdi_pathinfo_t *);

	dcmn_err2((CE_WARN, "di_getpath_data: client = %d", get_client));

	/*
	 * The naming of the following mdi_xyz() is unfortunately
	 * non-intuitive. mdi_get_next_phci_path() follows the
	 * client_link i.e. the list of pip's belonging to the
	 * given client dip.
	 */
	if (get_client)
		next_pip = &mdi_get_next_phci_path;
	else
		next_pip = &mdi_get_next_client_path;

	off = *poff_p;

	pip = NULL;
	while (pip = (*next_pip)(dip, pip)) {
		mdi_pathinfo_state_t state;
		di_off_t stored_offset;

		dcmn_err((CE_WARN, "marshalling pip = %p", (void *)pip));

		mdi_pi_lock(pip);

		if (di_pip_find(st, pip, &stored_offset) != -1) {
			/*
			 * We've already seen this pathinfo node so we need to
			 * take care not to snap it again; However, one endpoint
			 * and linkage will be set here. The other endpoint
			 * and linkage has already been set when the pip was
			 * first snapshotted i.e. when the other endpoint dip
			 * was snapshotted.
			 */
			me = (struct di_path *)(intptr_t)
			    di_mem_addr(st, stored_offset);

			*poff_p = stored_offset;

			di_path_one_endpoint(me, noff, &poff_p, get_client);

			/*
			 * The other endpoint and linkage were set when this
			 * pip was snapshotted. So we are done with both
			 * endpoints and linkages.
			 */
			ASSERT(!(me->path_snap_state &
			    (DI_PATH_SNAP_NOCLIENT|DI_PATH_SNAP_NOPHCI)));
			ASSERT(!(me->path_snap_state &
			    (DI_PATH_SNAP_NOCLINK|DI_PATH_SNAP_NOPLINK)));

			mdi_pi_unlock(pip);
			continue;
		}

		/*
		 * Now that we need to snapshot this pip, check memory
		 */
		off = di_checkmem(st, off, sizeof (struct di_path));
		me = (struct di_path *)(intptr_t)di_mem_addr(st, off);
		me->self = off;
		*poff_p = off;
		off += sizeof (struct di_path);

		me->path_snap_state =
		    DI_PATH_SNAP_NOCLINK | DI_PATH_SNAP_NOPLINK;
		me->path_snap_state |=
		    DI_PATH_SNAP_NOCLIENT | DI_PATH_SNAP_NOPHCI;

		/*
		 * Zero out fields as di_checkmem() doesn't guarantee
		 * zero-filled memory
		 */
		me->path_client = me->path_phci = 0;
		me->path_c_link = me->path_p_link = 0;

		di_path_one_endpoint(me, noff, &poff_p, get_client);

		/*
		 * Note the existence of this pathinfo
		 */
		di_register_pip(st, pip, me->self);

		state = mdi_pi_get_state(pip);
		me->path_state = path_state_convert(state);

		/*
		 * Get intermediate addressing info.
		 */
		off = di_checkmem(st, off, strlen(mdi_pi_get_addr(pip)) + 1);
		me->path_addr = off;
		(void) strcpy(di_mem_addr(st, off), mdi_pi_get_addr(pip));
		off += strlen(mdi_pi_get_addr(pip)) + 1;

		/*
		 * Get path properties if props are to be included in the
		 * snapshot
		 */
		if (DINFOPROP & st->command) {
			off = di_path_getprop(pip, off, &me->path_prop, st);
		} else {
			me->path_prop = 0;
		}

		mdi_pi_unlock(pip);
	}

	*poff_p = 0;

	return (off);
}

/*
 * Copy a list of properties attached to a devinfo node. Called from
 * di_copynode with devi_lock held. The major number is passed in case
 * we need to call driver's prop_op entry. The value of list indicates
 * which list we are copying. Possible values are:
 * DI_PROP_DRV_LIST, DI_PROP_SYS_LIST, DI_PROP_GLB_LIST, DI_PROP_HW_LIST
 */
static di_off_t
di_getprop(struct ddi_prop *prop, di_off_t *off_p, struct di_state *st,
	struct dev_info *dip, int list)
{
	dev_t dev;
	int (*prop_op)();
	int off, need_prop_op = 0;
	int prop_op_fail = 0;
	ddi_prop_t *propp = NULL;
	struct di_prop *pp;
	struct dev_ops *ops = NULL;
	int prop_len;
	caddr_t prop_val;


	dcmn_err2((CE_CONT, "di_getprop:\n"));

	ASSERT(st != NULL);

	dcmn_err((CE_CONT, "copy property list at addr %p\n", (void *)prop));

	/*
	 * Figure out if we need to call driver's prop_op entry point.
	 * The conditions are:
	 *	-- driver property list
	 *	-- driver must be attached and held
	 *	-- driver's cb_prop_op != ddi_prop_op
	 *		or parent's bus_prop_op != ddi_bus_prop_op
	 */

	if (list != DI_PROP_DRV_LIST) {
		goto getprop;
	}

	/*
	 * If driver is not attached or if major is -1, we ignore
	 * the driver property list. No one should rely on such
	 * properties.
	 */
	if (!i_ddi_devi_attached((dev_info_t *)dip)) {
		off = *off_p;
		*off_p = 0;
		return (off);
	}

	/*
	 * Now we have a driver which is held. We can examine entry points
	 * and check the condition listed above.
	 */
	ops = dip->devi_ops;

	/*
	 * Some nexus drivers incorrectly set cb_prop_op to nodev,
	 * nulldev or even NULL.
	 */
	if (ops && ops->devo_cb_ops &&
	    (ops->devo_cb_ops->cb_prop_op != ddi_prop_op) &&
	    (ops->devo_cb_ops->cb_prop_op != nodev) &&
	    (ops->devo_cb_ops->cb_prop_op != nulldev) &&
	    (ops->devo_cb_ops->cb_prop_op != NULL)) {
		need_prop_op = 1;
	}

getprop:
	/*
	 * check memory availability
	 */
	off = di_checkmem(st, *off_p, sizeof (struct di_prop));
	*off_p = off;
	/*
	 * Now copy properties
	 */
	do {
		pp = (struct di_prop *)(intptr_t)di_mem_addr(st, off);
		pp->self = off;
		/*
		 * Split dev_t to major/minor, so it works for
		 * both ILP32 and LP64 model
		 */
		pp->dev_major = getmajor(prop->prop_dev);
		pp->dev_minor = getminor(prop->prop_dev);
		pp->prop_flags = prop->prop_flags;
		pp->prop_list = list;

		/*
		 * property name
		 */
		off += sizeof (struct di_prop);
		if (prop->prop_name) {
			off = di_checkmem(st, off, strlen(prop->prop_name)
			    + 1);
			pp->prop_name = off;
			(void) strcpy(di_mem_addr(st, off), prop->prop_name);
			off += strlen(prop->prop_name) + 1;
		}

		/*
		 * Set prop_len here. This may change later
		 * if cb_prop_op returns a different length.
		 */
		pp->prop_len = prop->prop_len;
		if (!need_prop_op) {
			if (prop->prop_val == NULL) {
				dcmn_err((CE_WARN,
				    "devinfo: property fault at %p",
				    (void *)prop));
				pp->prop_data = -1;
			} else if (prop->prop_len != 0) {
				off = di_checkmem(st, off, prop->prop_len);
				pp->prop_data = off;
				bcopy(prop->prop_val, di_mem_addr(st, off),
				    prop->prop_len);
				off += DI_ALIGN(pp->prop_len);
			}
		}

		off = di_checkmem(st, off, sizeof (struct di_prop));
		pp->next = off;
		prop = prop->prop_next;
	} while (prop);

	pp->next = 0;

	if (!need_prop_op) {
		dcmn_err((CE_CONT, "finished property "
		    "list at offset 0x%x\n", off));
		return (off);
	}

	/*
	 * If there is a need to call driver's prop_op entry,
	 * we must release driver's devi_lock, because the
	 * cb_prop_op entry point will grab it.
	 *
	 * The snapshot memory has already been allocated above,
	 * which means the length of an active property should
	 * remain fixed for this implementation to work.
	 */


	prop_op = ops->devo_cb_ops->cb_prop_op;
	pp = (struct di_prop *)(intptr_t)di_mem_addr(st, *off_p);

	mutex_exit(&dip->devi_lock);

	do {
		int err;
		struct di_prop *tmp;

		if (pp->next) {
			tmp = (struct di_prop *)
			    (intptr_t)di_mem_addr(st, pp->next);
		} else {
			tmp = NULL;
		}

		/*
		 * call into driver's prop_op entry point
		 *
		 * Must search DDI_DEV_T_NONE with DDI_DEV_T_ANY
		 */
		dev = makedevice(pp->dev_major, pp->dev_minor);
		if (dev == DDI_DEV_T_NONE)
			dev = DDI_DEV_T_ANY;

		dcmn_err((CE_CONT, "call prop_op"
		    "(%lx, %p, PROP_LEN_AND_VAL_BUF, "
		    "DDI_PROP_DONTPASS, \"%s\", %p, &%d)\n",
		    dev,
		    (void *)dip,
		    (char *)di_mem_addr(st, pp->prop_name),
		    (void *)di_mem_addr(st, pp->prop_data),
		    pp->prop_len));

		if ((err = (*prop_op)(dev, (dev_info_t)dip,
		    PROP_LEN_AND_VAL_ALLOC, DDI_PROP_DONTPASS,
		    (char *)di_mem_addr(st, pp->prop_name),
		    &prop_val, &prop_len)) != DDI_PROP_SUCCESS) {
			if ((propp = i_ddi_prop_search(dev,
			    (char *)di_mem_addr(st, pp->prop_name),
			    (uint_t)pp->prop_flags,
			    &(DEVI(dip)->devi_drv_prop_ptr))) != NULL) {
				pp->prop_len = propp->prop_len;
				if (pp->prop_len != 0) {
					off = di_checkmem(st, off,
					    pp->prop_len);
					pp->prop_data = off;
					bcopy(propp->prop_val, di_mem_addr(st,
					    pp->prop_data), propp->prop_len);
					off += DI_ALIGN(pp->prop_len);
				}
			} else {
				prop_op_fail = 1;
			}
		} else if (prop_len != 0) {
			pp->prop_len = prop_len;
			off = di_checkmem(st, off, prop_len);
			pp->prop_data = off;
			bcopy(prop_val, di_mem_addr(st, off), prop_len);
			off += DI_ALIGN(prop_len);
			kmem_free(prop_val, prop_len);
		}

		if (prop_op_fail) {
			pp->prop_data = -1;
			dcmn_err((CE_WARN, "devinfo: prop_op failure "
			    "for \"%s\" err %d",
			    di_mem_addr(st, pp->prop_name), err));
		}

		pp = tmp;

	} while (pp);

	mutex_enter(&dip->devi_lock);
	dcmn_err((CE_CONT, "finished property list at offset 0x%x\n", off));
	return (off);
}

/*
 * find private data format attached to a dip
 * parent = 1 to match driver name of parent dip (for parent private data)
 *	0 to match driver name of current dip (for driver private data)
 */
#define	DI_MATCH_DRIVER	0
#define	DI_MATCH_PARENT	1

struct di_priv_format *
di_match_drv_name(struct dev_info *node, struct di_state *st, int match)
{
	int i, count, len;
	char *drv_name;
	major_t major;
	struct di_all *all;
	struct di_priv_format *form;

	dcmn_err2((CE_CONT, "di_match_drv_name: node = %s, match = %x\n",
	    node->devi_node_name, match));

	if (match == DI_MATCH_PARENT) {
		node = DEVI(node->devi_parent);
	}

	if (node == NULL) {
		return (NULL);
	}

	major = ddi_name_to_major(node->devi_binding_name);
	if (major == (major_t)(-1)) {
		return (NULL);
	}

	/*
	 * Match the driver name.
	 */
	drv_name = ddi_major_to_name(major);
	if ((drv_name == NULL) || *drv_name == '\0') {
		return (NULL);
	}

	/* Now get the di_priv_format array */
	all = (struct di_all *)(intptr_t)di_mem_addr(st, 0);

	if (match == DI_MATCH_PARENT) {
		count = all->n_ppdata;
		form = (struct di_priv_format *)
		    (intptr_t)(di_mem_addr(st, 0) + all->ppdata_format);
	} else {
		count = all->n_dpdata;
		form = (struct di_priv_format *)
		    (intptr_t)((caddr_t)all + all->dpdata_format);
	}

	len = strlen(drv_name);
	for (i = 0; i < count; i++) {
		char *tmp;

		tmp = form[i].drv_name;
		while (tmp && (*tmp != '\0')) {
			if (strncmp(drv_name, tmp, len) == 0) {
				return (&form[i]);
			}
			/*
			 * Move to next driver name, skipping a white space
			 */
			if (tmp = strchr(tmp, ' ')) {
				tmp++;
			}
		}
	}

	return (NULL);
}

/*
 * The following functions copy data as specified by the format passed in.
 * To prevent invalid format from panicing the system, we call on_fault().
 * A return value of 0 indicates an error. Otherwise, the total offset
 * is returned.
 */
#define	DI_MAX_PRIVDATA	(PAGESIZE >> 1)	/* max private data size */

static di_off_t
di_getprvdata(struct di_priv_format *pdp, struct dev_info *node,
    void *data, di_off_t *off_p, struct di_state *st)
{
	caddr_t pa;
	void *ptr;
	int i, size, repeat;
	di_off_t off, off0, *tmp;
	char *path;

	label_t ljb;

	dcmn_err2((CE_CONT, "di_getprvdata:\n"));

	/*
	 * check memory availability. Private data size is
	 * limited to DI_MAX_PRIVDATA.
	 */
	off = di_checkmem(st, *off_p, DI_MAX_PRIVDATA);

	if ((pdp->bytes == 0) || pdp->bytes > DI_MAX_PRIVDATA) {
		goto failure;
	}

	if (!on_fault(&ljb)) {
		/* copy the struct */
		bcopy(data, di_mem_addr(st, off), pdp->bytes);
		off0 = DI_ALIGN(pdp->bytes);

		/* dereferencing pointers */
		for (i = 0; i < MAX_PTR_IN_PRV; i++) {

			if (pdp->ptr[i].size == 0) {
				goto success;	/* no more ptrs */
			}

			/*
			 * first, get the pointer content
			 */
			if ((pdp->ptr[i].offset < 0) ||
			    (pdp->ptr[i].offset >
			    pdp->bytes - sizeof (char *)))
				goto failure;	/* wrong offset */

			pa = di_mem_addr(st, off + pdp->ptr[i].offset);

			/* save a tmp ptr to store off_t later */
			tmp = (di_off_t *)(intptr_t)pa;

			/* get pointer value, if NULL continue */
			ptr = *((void **) (intptr_t)pa);
			if (ptr == NULL) {
				continue;
			}

			/*
			 * next, find the repeat count (array dimension)
			 */
			repeat = pdp->ptr[i].len_offset;

			/*
			 * Positive value indicates a fixed sized array.
			 * 0 or negative value indicates variable sized array.
			 *
			 * For variable sized array, the variable must be
			 * an int member of the structure, with an offset
			 * equal to the absolution value of struct member.
			 */
			if (repeat > pdp->bytes - sizeof (int)) {
				goto failure;	/* wrong offset */
			}

			if (repeat >= 0) {
				repeat = *((int *)
				    (intptr_t)((caddr_t)data + repeat));
			} else {
				repeat = -repeat;
			}

			/*
			 * next, get the size of the object to be copied
			 */
			size = pdp->ptr[i].size * repeat;

			/*
			 * Arbitrarily limit the total size of object to be
			 * copied (1 byte to 1/4 page).
			 */
			if ((size <= 0) || (size > (DI_MAX_PRIVDATA - off0))) {
				goto failure;	/* wrong size or too big */
			}

			/*
			 * Now copy the data
			 */
			*tmp = off0;
			bcopy(ptr, di_mem_addr(st, off + off0), size);
			off0 += DI_ALIGN(size);
		}
	} else {
		goto failure;
	}

success:
	/*
	 * success if reached here
	 */
	no_fault();
	*off_p = off;

	return (off + off0);
	/*NOTREACHED*/

failure:
	/*
	 * fault occurred
	 */
	no_fault();
	path = kmem_alloc(MAXPATHLEN, KM_SLEEP);
	cmn_err(CE_WARN, "devinfo: fault on private data for '%s' at %p",
	    ddi_pathname((dev_info_t *)node, path), data);
	kmem_free(path, MAXPATHLEN);
	*off_p = -1;	/* set private data to indicate error */

	return (off);
}

/*
 * get parent private data; on error, returns original offset
 */
static di_off_t
di_getppdata(struct dev_info *node, di_off_t *off_p, struct di_state *st)
{
	int off;
	struct di_priv_format *ppdp;

	dcmn_err2((CE_CONT, "di_getppdata:\n"));

	/* find the parent data format */
	if ((ppdp = di_match_drv_name(node, st, DI_MATCH_PARENT)) == NULL) {
		off = *off_p;
		*off_p = 0;	/* set parent data to none */
		return (off);
	}

	return (di_getprvdata(ppdp, node,
	    ddi_get_parent_data((dev_info_t *)node), off_p, st));
}

/*
 * get parent private data; returns original offset
 */
static di_off_t
di_getdpdata(struct dev_info *node, di_off_t *off_p, struct di_state *st)
{
	int off;
	struct di_priv_format *dpdp;

	dcmn_err2((CE_CONT, "di_getdpdata:"));

	/* find the parent data format */
	if ((dpdp = di_match_drv_name(node, st, DI_MATCH_DRIVER)) == NULL) {
		off = *off_p;
		*off_p = 0;	/* set driver data to none */
		return (off);
	}

	return (di_getprvdata(dpdp, node,
	    ddi_get_driver_private((dev_info_t *)node), off_p, st));
}

/*
 * The driver is stateful across DINFOCPYALL and DINFOUSRLD.
 * This function encapsulates the state machine:
 *
 *	-> IOC_IDLE -> IOC_SNAP -> IOC_DONE -> IOC_COPY ->
 *	|		SNAPSHOT		USRLD	 |
 *	--------------------------------------------------
 *
 * Returns 0 on success and -1 on failure
 */
static int
di_setstate(struct di_state *st, int new_state)
{
	int ret = 0;

	mutex_enter(&di_lock);
	switch (new_state) {
	case IOC_IDLE:
	case IOC_DONE:
		break;
	case IOC_SNAP:
		if (st->di_iocstate != IOC_IDLE)
			ret = -1;
		break;
	case IOC_COPY:
		if (st->di_iocstate != IOC_DONE)
			ret = -1;
		break;
	default:
		ret = -1;
	}

	if (ret == 0)
		st->di_iocstate = new_state;
	else
		cmn_err(CE_NOTE, "incorrect state transition from %d to %d",
		    st->di_iocstate, new_state);
	mutex_exit(&di_lock);
	return (ret);
}

/*
 * We cannot assume the presence of the entire
 * snapshot in this routine. All we are guaranteed
 * is the di_all struct + 1 byte (for root_path)
 */
static int
header_plus_one_ok(struct di_all *all)
{
	/*
	 * Refuse to read old versions
	 */
	if (all->version != DI_SNAPSHOT_VERSION) {
		CACHE_DEBUG((DI_ERR, "bad version: 0x%x", all->version));
		return (0);
	}

	if (all->cache_magic != DI_CACHE_MAGIC) {
		CACHE_DEBUG((DI_ERR, "bad magic #: 0x%x", all->cache_magic));
		return (0);
	}

	if (all->snapshot_time == 0) {
		CACHE_DEBUG((DI_ERR, "bad timestamp: %ld", all->snapshot_time));
		return (0);
	}

	if (all->top_devinfo == 0) {
		CACHE_DEBUG((DI_ERR, "NULL top devinfo"));
		return (0);
	}

	if (all->map_size < sizeof (*all) + 1) {
		CACHE_DEBUG((DI_ERR, "bad map size: %u", all->map_size));
		return (0);
	}

	if (all->root_path[0] != '/' || all->root_path[1] != '\0') {
		CACHE_DEBUG((DI_ERR, "bad rootpath: %c%c",
		    all->root_path[0], all->root_path[1]));
		return (0);
	}

	/*
	 * We can't check checksum here as we just have the header
	 */

	return (1);
}

static int
chunk_write(struct vnode *vp, offset_t off, caddr_t buf, size_t len)
{
	rlim64_t	rlimit;
	ssize_t		resid;
	int		error = 0;


	rlimit = RLIM64_INFINITY;

	while (len) {
		resid = 0;
		error = vn_rdwr(UIO_WRITE, vp, buf, len, off,
		    UIO_SYSSPACE, FSYNC, rlimit, kcred, &resid);

		if (error || resid < 0) {
			error = error ? error : EIO;
			CACHE_DEBUG((DI_ERR, "write error: %d", error));
			break;
		}

		/*
		 * Check if we are making progress
		 */
		if (resid >= len) {
			error = ENOSPC;
			break;
		}
		buf += len - resid;
		off += len - resid;
		len = resid;
	}

	return (error);
}

extern int modrootloaded;
extern void mdi_walk_vhcis(int (*)(dev_info_t *, void *), void *);
extern void mdi_vhci_walk_phcis(dev_info_t *,
	int (*)(dev_info_t *, void *), void *);

static void
di_cache_write(struct di_cache *cache)
{
	struct di_all	*all;
	struct vnode	*vp;
	int		oflags;
	size_t		map_size;
	size_t		chunk;
	offset_t	off;
	int		error;
	char		*buf;

	ASSERT(DI_CACHE_LOCKED(*cache));
	ASSERT(!servicing_interrupt());

	if (cache->cache_size == 0) {
		ASSERT(cache->cache_data == NULL);
		CACHE_DEBUG((DI_ERR, "Empty cache. Skipping write"));
		return;
	}

	ASSERT(cache->cache_size > 0);
	ASSERT(cache->cache_data);

	if (!modrootloaded || rootvp == NULL || vn_is_readonly(rootvp)) {
		CACHE_DEBUG((DI_ERR, "Can't write to rootFS. Skipping write"));
		return;
	}

	all = (struct di_all *)cache->cache_data;

	if (!header_plus_one_ok(all)) {
		CACHE_DEBUG((DI_ERR, "Invalid header. Skipping write"));
		return;
	}

	ASSERT(strcmp(all->root_path, "/") == 0);

	/*
	 * The cache_size is the total allocated memory for the cache.
	 * The map_size is the actual size of valid data in the cache.
	 * map_size may be smaller than cache_size but cannot exceed
	 * cache_size.
	 */
	if (all->map_size > cache->cache_size) {
		CACHE_DEBUG((DI_ERR, "map_size (0x%x) > cache_size (0x%x)."
		    " Skipping write", all->map_size, cache->cache_size));
		return;
	}

	/*
	 * First unlink the temp file
	 */
	error = vn_remove(DI_CACHE_TEMP, UIO_SYSSPACE, RMFILE);
	if (error && error != ENOENT) {
		CACHE_DEBUG((DI_ERR, "%s: unlink failed: %d",
		    DI_CACHE_TEMP, error));
	}

	if (error == EROFS) {
		CACHE_DEBUG((DI_ERR, "RDONLY FS. Skipping write"));
		return;
	}

	vp = NULL;
	oflags = (FCREAT|FWRITE);
	if (error = vn_open(DI_CACHE_TEMP, UIO_SYSSPACE, oflags,
	    DI_CACHE_PERMS, &vp, CRCREAT, 0)) {
		CACHE_DEBUG((DI_ERR, "%s: create failed: %d",
		    DI_CACHE_TEMP, error));
		return;
	}

	ASSERT(vp);

	/*
	 * Paranoid: Check if the file is on a read-only FS
	 */
	if (vn_is_readonly(vp)) {
		CACHE_DEBUG((DI_ERR, "cannot write: readonly FS"));
		goto fail;
	}

	/*
	 * Note that we only write map_size bytes to disk - this saves
	 * space as the actual cache size may be larger than size of
	 * valid data in the cache.
	 * Another advantage is that it makes verification of size
	 * easier when the file is read later.
	 */
	map_size = all->map_size;
	off = 0;
	buf = cache->cache_data;

	while (map_size) {
		ASSERT(map_size > 0);
		/*
		 * Write in chunks so that VM system
		 * is not overwhelmed
		 */
		if (map_size > di_chunk * PAGESIZE)
			chunk = di_chunk * PAGESIZE;
		else
			chunk = map_size;

		error = chunk_write(vp, off, buf, chunk);
		if (error) {
			CACHE_DEBUG((DI_ERR, "write failed: off=0x%x: %d",
			    off, error));
			goto fail;
		}

		off += chunk;
		buf += chunk;
		map_size -= chunk;

		/* Give pageout a chance to run */
		delay(1);
	}

	/*
	 * Now sync the file and close it
	 */
	if (error = VOP_FSYNC(vp, FSYNC, kcred, NULL)) {
		CACHE_DEBUG((DI_ERR, "FSYNC failed: %d", error));
	}

	if (error = VOP_CLOSE(vp, oflags, 1, (offset_t)0, kcred, NULL)) {
		CACHE_DEBUG((DI_ERR, "close() failed: %d", error));
		VN_RELE(vp);
		return;
	}

	VN_RELE(vp);

	/*
	 * Now do the rename
	 */
	if (error = vn_rename(DI_CACHE_TEMP, DI_CACHE_FILE, UIO_SYSSPACE)) {
		CACHE_DEBUG((DI_ERR, "rename failed: %d", error));
		return;
	}

	CACHE_DEBUG((DI_INFO, "Cache write successful."));

	return;

fail:
	(void) VOP_CLOSE(vp, oflags, 1, (offset_t)0, kcred, NULL);
	VN_RELE(vp);
}


/*
 * Since we could be called early in boot,
 * use kobj_read_file()
 */
static void
di_cache_read(struct di_cache *cache)
{
	struct _buf	*file;
	struct di_all	*all;
	int		n;
	size_t		map_size, sz, chunk;
	offset_t	off;
	caddr_t		buf;
	uint32_t	saved_crc, crc;

	ASSERT(modrootloaded);
	ASSERT(DI_CACHE_LOCKED(*cache));
	ASSERT(cache->cache_data == NULL);
	ASSERT(cache->cache_size == 0);
	ASSERT(!servicing_interrupt());

	file = kobj_open_file(DI_CACHE_FILE);
	if (file == (struct _buf *)-1) {
		CACHE_DEBUG((DI_ERR, "%s: open failed: %d",
		    DI_CACHE_FILE, ENOENT));
		return;
	}

	/*
	 * Read in the header+root_path first. The root_path must be "/"
	 */
	all = kmem_zalloc(sizeof (*all) + 1, KM_SLEEP);
	n = kobj_read_file(file, (caddr_t)all, sizeof (*all) + 1, 0);

	if ((n != sizeof (*all) + 1) || !header_plus_one_ok(all)) {
		kmem_free(all, sizeof (*all) + 1);
		kobj_close_file(file);
		CACHE_DEBUG((DI_ERR, "cache header: read error or invalid"));
		return;
	}

	map_size = all->map_size;

	kmem_free(all, sizeof (*all) + 1);

	ASSERT(map_size >= sizeof (*all) + 1);

	buf = di_cache.cache_data = kmem_alloc(map_size, KM_SLEEP);
	sz = map_size;
	off = 0;
	while (sz) {
		/* Don't overload VM with large reads */
		chunk = (sz > di_chunk * PAGESIZE) ? di_chunk * PAGESIZE : sz;
		n = kobj_read_file(file, buf, chunk, off);
		if (n != chunk) {
			CACHE_DEBUG((DI_ERR, "%s: read error at offset: %lld",
			    DI_CACHE_FILE, off));
			goto fail;
		}
		off += chunk;
		buf += chunk;
		sz -= chunk;
	}

	ASSERT(off == map_size);

	/*
	 * Read past expected EOF to verify size.
	 */
	if (kobj_read_file(file, (caddr_t)&sz, 1, off) > 0) {
		CACHE_DEBUG((DI_ERR, "%s: file size changed", DI_CACHE_FILE));
		goto fail;
	}

	all = (struct di_all *)di_cache.cache_data;
	if (!header_plus_one_ok(all)) {
		CACHE_DEBUG((DI_ERR, "%s: file header changed", DI_CACHE_FILE));
		goto fail;
	}

	/*
	 * Compute CRC with checksum field in the cache data set to 0
	 */
	saved_crc = all->cache_checksum;
	all->cache_checksum = 0;
	CRC32(crc, di_cache.cache_data, map_size, -1U, crc32_table);
	all->cache_checksum = saved_crc;

	if (crc != all->cache_checksum) {
		CACHE_DEBUG((DI_ERR,
		    "%s: checksum error: expected=0x%x actual=0x%x",
		    DI_CACHE_FILE, all->cache_checksum, crc));
		goto fail;
	}

	if (all->map_size != map_size) {
		CACHE_DEBUG((DI_ERR, "%s: map size changed", DI_CACHE_FILE));
		goto fail;
	}

	kobj_close_file(file);

	di_cache.cache_size = map_size;

	return;

fail:
	kmem_free(di_cache.cache_data, map_size);
	kobj_close_file(file);
	di_cache.cache_data = NULL;
	di_cache.cache_size = 0;
}


/*
 * Checks if arguments are valid for using the cache.
 */
static int
cache_args_valid(struct di_state *st, int *error)
{
	ASSERT(error);
	ASSERT(st->mem_size > 0);
	ASSERT(st->memlist != NULL);

	if (!modrootloaded || !i_ddi_io_initialized()) {
		CACHE_DEBUG((DI_ERR,
		    "cache lookup failure: I/O subsystem not inited"));
		*error = ENOTACTIVE;
		return (0);
	}

	/*
	 * No other flags allowed with DINFOCACHE
	 */
	if (st->command != (DINFOCACHE & DIIOC_MASK)) {
		CACHE_DEBUG((DI_ERR,
		    "cache lookup failure: bad flags: 0x%x",
		    st->command));
		*error = EINVAL;
		return (0);
	}

	if (strcmp(DI_ALL_PTR(st)->root_path, "/") != 0) {
		CACHE_DEBUG((DI_ERR,
		    "cache lookup failure: bad root: %s",
		    DI_ALL_PTR(st)->root_path));
		*error = EINVAL;
		return (0);
	}

	CACHE_DEBUG((DI_INFO, "cache lookup args ok: 0x%x", st->command));

	*error = 0;

	return (1);
}

static int
snapshot_is_cacheable(struct di_state *st)
{
	ASSERT(st->mem_size > 0);
	ASSERT(st->memlist != NULL);

	if ((st->command & DI_CACHE_SNAPSHOT_FLAGS) !=
	    (DI_CACHE_SNAPSHOT_FLAGS & DIIOC_MASK)) {
		CACHE_DEBUG((DI_INFO,
		    "not cacheable: incompatible flags: 0x%x",
		    st->command));
		return (0);
	}

	if (strcmp(DI_ALL_PTR(st)->root_path, "/") != 0) {
		CACHE_DEBUG((DI_INFO,
		    "not cacheable: incompatible root path: %s",
		    DI_ALL_PTR(st)->root_path));
		return (0);
	}

	CACHE_DEBUG((DI_INFO, "cacheable snapshot request: 0x%x", st->command));

	return (1);
}

static int
di_cache_lookup(struct di_state *st)
{
	size_t	rval;
	int	cache_valid;

	ASSERT(cache_args_valid(st, &cache_valid));
	ASSERT(modrootloaded);

	DI_CACHE_LOCK(di_cache);

	/*
	 * The following assignment determines the validity
	 * of the cache as far as this snapshot is concerned.
	 */
	cache_valid = di_cache.cache_valid;

	if (cache_valid && di_cache.cache_data == NULL) {
		di_cache_read(&di_cache);
		/* check for read or file error */
		if (di_cache.cache_data == NULL)
			cache_valid = 0;
	}

	if (cache_valid) {
		/*
		 * Ok, the cache was valid as of this particular
		 * snapshot. Copy the cached snapshot. This is safe
		 * to do as the cache cannot be freed (we hold the
		 * cache lock). Free the memory allocated in di_state
		 * up until this point - we will simply copy everything
		 * in the cache.
		 */

		ASSERT(di_cache.cache_data != NULL);
		ASSERT(di_cache.cache_size > 0);

		di_freemem(st);

		rval = 0;
		if (di_cache2mem(&di_cache, st) > 0) {

			ASSERT(DI_ALL_PTR(st));

			/*
			 * map_size is size of valid data in the
			 * cached snapshot and may be less than
			 * size of the cache.
			 */
			rval = DI_ALL_PTR(st)->map_size;

			ASSERT(rval >= sizeof (struct di_all));
			ASSERT(rval <= di_cache.cache_size);
		}
	} else {
		/*
		 * The cache isn't valid, we need to take a snapshot.
		 * Set the command flags appropriately
		 */
		ASSERT(st->command == (DINFOCACHE & DIIOC_MASK));
		st->command = (DI_CACHE_SNAPSHOT_FLAGS & DIIOC_MASK);
		rval = di_cache_update(st);
		st->command = (DINFOCACHE & DIIOC_MASK);
	}

	DI_CACHE_UNLOCK(di_cache);

	/*
	 * For cached snapshots, the devinfo driver always returns
	 * a snapshot rooted at "/".
	 */
	ASSERT(rval == 0 || strcmp(DI_ALL_PTR(st)->root_path, "/") == 0);

	return ((int)rval);
}

/*
 * This is a forced update of the cache  - the previous state of the cache
 * may be:
 *	- unpopulated
 *	- populated and invalid
 *	- populated and valid
 */
static int
di_cache_update(struct di_state *st)
{
	int rval;
	uint32_t crc;
	struct di_all *all;

	ASSERT(DI_CACHE_LOCKED(di_cache));
	ASSERT(snapshot_is_cacheable(st));

	/*
	 * Free the in-core cache and the on-disk file (if they exist)
	 */
	i_ddi_di_cache_free(&di_cache);

	/*
	 * Set valid flag before taking the snapshot,
	 * so that any invalidations that arrive
	 * during or after the snapshot are not
	 * removed by us.
	 */
	atomic_or_32(&di_cache.cache_valid, 1);

	rval = di_snapshot_and_clean(st);

	if (rval == 0) {
		CACHE_DEBUG((DI_ERR, "can't update cache: bad snapshot"));
		return (0);
	}

	DI_ALL_PTR(st)->map_size = rval;

	if (di_mem2cache(st, &di_cache) == 0) {
		CACHE_DEBUG((DI_ERR, "can't update cache: copy failed"));
		return (0);
	}

	ASSERT(di_cache.cache_data);
	ASSERT(di_cache.cache_size > 0);

	/*
	 * Now that we have cached the snapshot, compute its checksum.
	 * The checksum is only computed over the valid data in the
	 * cache, not the entire cache.
	 * Also, set all the fields (except checksum) before computing
	 * checksum.
	 */
	all = (struct di_all *)di_cache.cache_data;
	all->cache_magic = DI_CACHE_MAGIC;
	all->map_size = rval;

	ASSERT(all->cache_checksum == 0);
	CRC32(crc, di_cache.cache_data, all->map_size, -1U, crc32_table);
	all->cache_checksum = crc;

	di_cache_write(&di_cache);

	return (rval);
}

static void
di_cache_print(di_cache_debug_t msglevel, char *fmt, ...)
{
	va_list	ap;

	if (di_cache_debug <= DI_QUIET)
		return;

	if (di_cache_debug < msglevel)
		return;

	switch (msglevel) {
		case DI_ERR:
			msglevel = CE_WARN;
			break;
		case DI_INFO:
		case DI_TRACE:
		default:
			msglevel = CE_NOTE;
			break;
	}

	va_start(ap, fmt);
	vcmn_err(msglevel, fmt, ap);
	va_end(ap);
}
