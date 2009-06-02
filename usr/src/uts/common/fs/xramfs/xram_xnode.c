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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/stat.h>
#include <sys/debug.h>
#include <sys/policy.h>
#include <sys/fs/xnode.h>
#include <fs/xramfs/xram.h>
#include <sys/vtrace.h>
#include <sys/mode.h>

/* you MUST call after xvt is _filled by zero_ */
void
xram_xvt_init(struct xram_xnvntable *xvt)
{
	int i;

	for (i = 0; i < XVT_LIST_MUTEX_SIZE; i++)
		mutex_init(&xvt->xvt_list_mutex[i], NULL, MUTEX_DEFAULT, NULL);
}

void
xram_xvt_destroy(struct xram_xnvntable *xvt)
{
	int i;

	for (i = 0; i < XVT_LIST_MUTEX_SIZE; i++)
		mutex_destroy(&xvt->xvt_list_mutex[i]);
}

/* you must have a exclusive lock */
static int
xram_xvt_insert(struct xmount *xm, struct xram_xnvntable *xvt, xram_xno_t xno,
		struct xnode *xn)
{
	int hval = XRAM_XVT_HASH(xno);

	ASSERT(xno >= XRAMMNODE_XNO_ROOT);

	xn->xn_pprev = &xvt->xvt_hash[hval];
	xn->xn_next = xvt->xvt_hash[hval];
	if (xn->xn_next != NULL)
		xn->xn_next->xn_pprev = &xn->xn_next;
	xvt->xvt_hash[hval] = xn;
	return (0);
}

/* you must have a exclusive lock */
static void
xram_xvt_remove(struct xmount *xm, struct xram_xnvntable *xvt,
		struct xnode *xp)
{
	/*
	 * previous node should be exist
	 * - that is xnode or  anchor in hash entry
	 */
	ASSERT(xp->xn_pprev != NULL);
	*xp->xn_pprev = xp->xn_next;
	if (xp->xn_next != NULL)
		xp->xn_next->xn_pprev = xp->xn_pprev;
	XRAMDBG(("xram_xvt_remove: xno %" PRIuMAX " removing\n",
		 (uintmax_t)XNODE_XNO(xm, xp)));
}

/* you must have a shared lock */
static struct xnode *
xram_xvt_search(struct xmount *xm, struct xram_xnvntable *xvt, xram_xno_t xno)
{
	struct xnode *cur;

	ASSERT(xno >= XRAMMNODE_XNO_ROOT);

	for (cur = xvt->xvt_hash[XRAM_XVT_HASH(xno)]; cur != NULL;
	     cur = cur->xn_next) {
		if (XNODE_XNO(xm, cur) == xno)
			break;
	}
	/* return NULL if not found, return xnode if found; so it is OK */
	return (cur);
}

static kmem_cache_t *xnode_cache;

void
xnode_create_cache(void)
{
	XRAMDBG(("xnode_create_cache: creating cache\n"));
	xnode_cache = kmem_cache_create(
		"xramfs_xnode_cache", sizeof (struct xnode), 0,
		NULL, NULL, NULL, NULL, NULL, 0);

	return;
}

void
xnode_destroy_cache(void)
{
	XRAMDBG(("xnode_destroy_cache: destroying cache\n"));
	kmem_cache_destroy(xnode_cache);
}

/*
 * create xnode for specified xram-media-node xmp.
 * you MUST acquire specified mutex, and call xnode_active before use it.
 */

static int
xnode_init(struct xmount *xm, struct xrammnode *xmp, struct xnode **xpp)
{
	vnode_t *vp;
	struct xnode *xp;
	vtype_t vtype;

	switch (XRAMMNODE_TYPE(xmp)) {
	case XIFMAP:
		vtype = VREG;
		break;
	case XIFIFO:
		vtype = VFIFO;
		break;
	case XIFCHR:
		vtype = VCHR;
		break;
	case XIFDIR:
		vtype = VDIR;
		break;
	case XIFBLK:
		vtype = VBLK;
		break;
	case XIFLNK:
		vtype = VLNK;
		break;
	case XIFPACK: /* no PACKED FILE SUPPORT */
	default:
		return (EIO);
	}

	xp = kmem_cache_alloc(xnode_cache, KM_SLEEP);
	xp->xn_xmp = xmp;

	vp = vn_alloc(KM_SLEEP);
	vn_setops(vp, xram_vnodeops);
	vp->v_vfsp = xm->xm_vfsp;
	vp->v_type = vtype;

	if (vp->v_type == VBLK || vp->v_type == VCHR)
		vp->v_rdev = makedevice(xp->xn_rdev_major, xp->xn_rdev_minor);
	else
		vp->v_rdev = NODEV;
	vp->v_data = (caddr_t)xp;

	xp->xn_vp = vp;
	*xpp = xp;

	vn_exists(vp);
	return (0);
}

/*
 * remove specified xnode from hash table, so its xnode will be
 * hidden from other threads / processes.
 * you MUST have a mutex for specified xnode.
 *
 * called from:
 *  xram_inactive() at xram_vnops.c
 *
 */

void
xnode_inactive(struct xmount *xm, struct xnode *xp)
{
	xram_xvt_remove(xm, &xm->xm_xvt, xp);
}

/*
 * finalize and free all resources allocated by specified xnode
 *
 * called from:
 *  xram_inactive() at xram_vnops.c
 *
 */
void
xnode_free(struct xmount *xm, struct xnode *xp)
{
	vnode_t *vp = XNTOV(xp);

	vn_invalid(vp);
	vn_free(vp);
	kmem_cache_free(xnode_cache, xp);
}

int
xnode_lookup(struct xmount *xm, xram_xno_t xno, struct xnode **foundxp)
{
	struct xnode *xp, *newxp;
	kmutex_t *kmtx;
	int error;

	kmtx = xnode_getmutex(xm, xno);
	/* lock a hash table to avoid twice vnode creation */
	mutex_enter(kmtx);
	xp = xram_xvt_search(xm, &xm->xm_xvt, xno);
	if (xp != NULL) {
		/* already created, use it */
		xnode_hold(xp);
		mutex_exit(kmtx);
		*foundxp = xp;
		return (0);
	}
	/* release bit of a time, to avoid memory allocation deadlock */
	mutex_exit(kmtx);
	/* create new xnode */
	error = xnode_init(xm, XNO2XMN(xm, xno), &xp);
	if (error != 0)
		return (error);
	/*
	 * lookup one more time to assure only this thread is
	 * creating the new xnode
	 */
	mutex_enter(kmtx);
	newxp = xram_xvt_search(xm, &xm->xm_xvt, xno);
	if (newxp != NULL) {
		/*
		 * somebody already create and register this xnode;
		 * destroy xnode created by this thread, and use
		 * somebody created one.
		 */
		xnode_hold(newxp);
		mutex_exit(kmtx);
		xnode_free(xm, xp);
		*foundxp = newxp;
		return (0);
	}
	/* I am the first one! */
	xram_xvt_insert(xm, &xm->xm_xvt, xno, xp);
	mutex_exit(kmtx);
	*foundxp = xp;
	return (0);
}

/*
 * mutex should be exportable because functions on another file
 * should lock or unlock a list
 */
kmutex_t *
xnode_getmutex(struct xmount *xm, xram_xno_t xno)
{
	return (&xm->xm_xvt.xvt_list_mutex[xno & XVT_LIST_MUTEX_MASK]);
}

/* following 3 functions are only for unmount() */
void
xnode_lock_all(struct xmount *xm)
{
	struct xram_xnvntable *xvt = &xm->xm_xvt;
	int i;

	for (i = 0; i < XVT_LIST_MUTEX_SIZE; i++)
		mutex_enter(&xvt->xvt_list_mutex[i]);
}

int
xnode_isalone(struct xmount *xm)
{
	struct xram_xnvntable *xvt = &xm->xm_xvt;
	int i, is_exist = 0;
	struct xnode *cur;

	for (i = 0; i < XVT_HASH_SIZE; i++) {
		if (xvt->xvt_hash[i] != NULL) {
			/*
			 * already exist in another slot, or
			 *  another node exist in same slot
			 */
			if (is_exist == 1 || xvt->xvt_hash[i]->xn_next != NULL)
				return (0);
			is_exist = 1;
		}
	}
	return (1);
}

void
xnode_unlock_all(struct xmount *xm)
{
	struct xram_xnvntable *xvt = &xm->xm_xvt;
	int i;

	for (i = 0; i < XVT_LIST_MUTEX_SIZE; i++)
		mutex_exit(&xvt->xvt_list_mutex[i]);
}
