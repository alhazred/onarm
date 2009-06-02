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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
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
#include <sys/sdt.h>
#include <sys/fs/xnode.h>
#include <fs/xramfs/xram.h>
#include <sys/vtrace.h>
#include <sys/dirent.h>

/*
 * at first, we check xno is equal or greater than XRAMMNODE_XNO_ROOT;
 * and _if passed_, we check (xno - XRAMMNODE_XNO_ROOT) is smaller than
 * number of xrammnodes.
 * if first check is failed, second check is not running;
 * so we don't need to care underflow of (xno - XRAMMNODE_XNO_ROOT)
 * because xno must be equal or greater than XRAMMNODE_XNO_ROOT.
 */

#define IS_INVALID_XNO(xm, xno)						     \
	((xno) < XRAMMNODE_XNO_ROOT					     \
	 || ((xno) - XRAMMNODE_XNO_ROOT) >= XMOUNT_XRAMHEADER(xm)->xh_files)
#define CHK_AND_SET_XNO(xm, rvalp, xno)		\
	(IS_INVALID_XNO(xm, xno)		\
	? (ENXIO) : (*(rvalp) = (xno), 0))


static int
xramsearchdir(struct xmount *xm, struct xnode *dir, char *name,
	      xram_xno_t *xnop)
{
	struct xramdirent *xde;
	struct xramdircheader *xdh;
	caddr_t dnamehead;
	char *candname;
	uint32_t dnameshift, start, candlen;
	size_t namelen;

	xdh = XNODE_DIR(xm, dir);
	ASSERT(name != NULL);

	/* check xdh is in correct region */
	if ((void *)&xdh[1] > xm->xm_kasaddr_end
	    || (void *)xdh < xm->xm_kasaddr) {
		return (ENXIO);
	}

	/* unknown directory types */
	if (xdh->xdh_type != XRAM_DTYPE_SORTED
	    && xdh->xdh_type != XRAM_DTYPE_LINEAR)
		return (ENOTSUP);

	/* special rules for ".", ".." */
	if (name[0] == '.') {
		if (name[1] == '\0')
			return (CHK_AND_SET_XNO(xm, xnop, xdh->xdh_xnodot));
		else if (name[1] == '.' && name[2] == '\0')
			return (CHK_AND_SET_XNO(xm, xnop, xdh->xdh_xnodotdot));
	}

	/* first entry is following directory common header */
	xde = (struct xramdirent *)(&xdh[1]);
	/* check pointer region error */
	if (XRAM_SIZE_PTR2PTR(xde, xm->xm_kasaddr_end)
	    / sizeof(struct xramdirent) < xdh->xdh_nent) {
		XRAMDBG(("xramsearchdir: out of range: 0x%p - 0x%p / %u < "
			 "%" PRIu32 "\n", xm->xm_kasaddr_end, xde,
			 sizeof(struct xramdirent), xdh->xdh_nent));
		return (ENXIO);
	}
	namelen = strlen(name);

	dnamehead = xm->xm_dname;
	dnameshift = XMOUNT_XRAMHEADER(xm)->xh_dname_shift;
	XRAMDBG(("xramsearchdir: target '%s', len %" PRIuMAX "\n", name,
		 (uintmax_t)namelen));

	if (xdh->xdh_type == XRAM_DTYPE_SORTED) {
		uint32_t mini = 0, maxi = xdh->xdh_nent - 1, badcount = 0;
		int res, difflen;

		/* special case: no candidates (maxi may be 0xffffffff) */
		if (xdh->xdh_nent == 0)
			return (ENOENT);

		/*
		 *  consider array [mini, maxi].
		 *  (note that the candidate includes ${maxi}th element)
		 *  and take mid as center value ((mini + maxi) / 2).
		 *  if strcmp returns minus value,
		 *  candidate is shrank to [mini, mid - 1];
		 *  if it returns plus value,
		 *  candidate is shrank to [mid + 1, maxi].
		 *  if it returns zero, of course,
		 *  mid points specified target.
		 *
		 *  repeated above operation several times,
		 *  candidate area is [n, n]. so center value (mid) is
		 *  n. if comparison with name of element n is failed,
		 *  candidate area is vanished; both [n, n - 1] and [n + 1, n]
		 *  are "start and end position is reversed, so they are
		 *  invalid".
		 */
		while (mini <= maxi) {
			uint32_t mid;

			XDTRACE_PROBE2(sortloop, uint32_t, mini,
				       uint32_t, maxi);

			mid = mini + ((maxi - mini) >> 1);
			/*
			 * skip bad entry - ([...] is floor function)
			 * 1st candidate: [(mini+maxi)/2]
			 * 2nd candidate: [(mini+maxi)/2] + 1
			 * 3rd candidate: [(mini+maxi)/2] - 1
			 * 4th candidate: [(mini+maxi)/2] + 2
			 * 5th candidate: [(mini+maxi)/2] - 2
			 * ...
			 * (2n  )th candidate: [(mini+maxi)/2] + n
			 * (2n+1)th candidate: [(mini+maxi)/2] - n
			 *
			 */
			if (badcount > 0) {
				if (badcount & 1)
					mid += (badcount >> 1) + 1;
				else
					mid -= badcount >> 1;
			}

			candlen = XRAMDIRENT_LENGTH(&xde[mid]);
			start = XRAMDIRENT_START(&xde[mid]);
			candname = dnamehead
				+ ((size_t)start << dnameshift);
			/*
			 * check [candname, candname + candlen] is in
			 * xramfs image region
			 */
			if ((char *)xm->xm_kasaddr_end < candname + candlen
			    || (char *)xm->xm_kasaddr > candname) {
				/* ignore this entry */
				/* no more candidates */
				if (maxi - mini <= badcount)
					break;
				badcount++;
				continue;
			}
			/* clear badcount */
			badcount = 0;

			/* name length should be <= 255, so it is OK */
			difflen = ((signed)namelen - (signed)candlen);
			res = memcmp(name, candname,
				     difflen > 0 ? candlen : namelen);
			if (res == 0) {
				if (difflen == 0) {
					XRAMDBG(("xramsearchdir: "
						 "target '%s' is xno %"
						 PRIu32 "\n",
						 name, xde[mid].xde_xno));
					return (CHK_AND_SET_XNO(
							xm, xnop,
							xde[mid].xde_xno));
				}
				res = difflen;
			}
			if (res > 0) {
				/* target is after xde[mid] */
				/*
				 * we don't care of integer overflow of 'mini'
				 *
				 * because 'maximum' $mid is $maxi,
				 * and 'maximum' $maxi is (UINT32_MAX - 1)
				 * (xdh->xdh_nent is 32bit unsigned integer);
				 * so 'maximum' $mid is 0xfffffffe.
				 * $mini may be 0xffffffff, but 'maximum' $maxi
				 * is 0xfffffffe, so next while loop condition
				 * will be failed and exit from loop.
				 */
				mini = mid + 1;
			} else {
				/* target is before xde[mid] */
				/* To avoid integer underflow */
				if (mid == 0)
					break;
				maxi = mid - 1;
			}
		}
	} else if (xdh->xdh_type == XRAM_DTYPE_LINEAR) {
		uint32_t entleft;
		for (entleft = xdh->xdh_nent; entleft != 0; entleft--, xde++) {
			XDTRACE_PROBE2(loop,
				       struct xramdirent *, xde,
				       uint32_t, entleft);

			candlen = XRAMDIRENT_LENGTH(xde);

			if (candlen != namelen)
				continue;

			/* same as candname = XRAMDIRENT_DNAME(xm, xde); */
			start = XRAMDIRENT_START(xde);
			candname = dnamehead
				+ ((size_t)start << dnameshift);
			if ((char *)xm->xm_kasaddr_end < candname + candlen
			    || (char *)xm->xm_kasaddr > candname) {
				continue;
			}
			if (memcmp(candname, name, namelen) == 0) {
				/* found */
				return (CHK_AND_SET_XNO(
						xm, xnop, xde->xde_xno));
			}
		}
	}
	/* not found */
	/* not reached if xdh_type is neither 0 (linear) nor 1 (sorted) */
	return (ENOENT);
}

/*
 * Search directory 'parent' for entry 'name'.
 *
 * The calling thread can't hold the write version
 * of the rwlock for the directory being searched
 *
 * 0 is returned on success and *foundxp points
 * to the found xnode with its vnode held.
 */
int
xram_dirlookup(struct xmount *xm, struct xnode *parent, char *name,
	       struct xnode **foundxp, cred_t *cred)
{
	int error;
	xram_xno_t xno;

	if (XNODE_TYPE(parent) != XIFDIR)
		return (ENOTDIR);

	if ((error = xram_xaccess(xm, parent, VEXEC, cred))) {
		XRAMDBG(("xram_dirlookup: parent-xno %" PRIuMAX
			 " (type/mode 0%06" PRIo16 ") -> ERROR %d\n",
			 (uintmax_t)XNODE_XNO(xm, parent),
			 parent->xn_typemode, error));
		return (error);
	}
	XRAMDBG(("xram_dirlookup: parent-xno %" PRIuMAX " (type/mode 0%06"
		 PRIo16 ")\n", (uintmax_t)XNODE_XNO(xm, parent),
		 parent->xn_typemode));

	error = xramsearchdir(xm, parent, name, &xno);
	if (error != 0) {
		XDTRACE_PROBE1(not_found_name, char *, name);

		/* even if 'directory type is invalid'
		   or 'xno record is invalid', it returns ENOENT */
		return (error);
	}
	XDTRACE_PROBE2(found_name, char *, name, xram_xno_t, xno);
	error = xnode_lookup(xm, xno, foundxp);
	return (error);
}

int
xram_listdir(struct xmount *xm, struct xnode *dir, uio_t *uiop, int *eofp)
{
	int bufsize;
	caddr_t outbuf;
	dirent64_t *dp;

	ulong_t outsize = 0, skipcount;
	uint64_t num_ent, offset = 0;
	size_t dnamemaxsize_inunit;

	int error = 0;
	struct xramdircheader *xdh;
	struct xramdirent *xde;
	enum {SP_DOT = 0, SP_DOTDOT, SP_END};
	caddr_t dnamehead;
	uint8_t dnameshift;

	ASSERT(SP_END == XRAMFS_NUM_VIRTUAL_NODES);
	bufsize = uiop->uio_iov->iov_len;
	outbuf = kmem_alloc(bufsize, KM_SLEEP);
	dp = (dirent64_t *)outbuf;

	xdh = XNODE_DIR(xm, dir);
	if ((void *)&xdh[1] > xm->xm_kasaddr_end
	    || (void *)xdh < xm->xm_kasaddr) {
		XRAMDBG(("xram_listdir: out of range: "
			 "[0x%p (0x%p - 0x%p) 0x%p]\n",
			 xm->xm_kasaddr, xdh, &xdh[1], xm->xm_kasaddr_end));
		kmem_free(outbuf, bufsize);
		return (ENXIO);
	}

	switch (xdh->xdh_type) {
	case XRAM_DTYPE_LINEAR:
	case XRAM_DTYPE_SORTED:
		/*
		 * first directory entry follows directory common header
		 *  if xdh->xdh_type is 0 or 1
		 */
		xde = (struct xramdirent *)(&xdh[1]);
		/* check pointer points correct region */
		if (XRAM_SIZE_PTR2PTR(xde, xm->xm_kasaddr_end)
		    / sizeof(struct xramdirent) < xdh->xdh_nent) {
			XRAMDBG(("xram_listdir: out of range: "
				 "0x%p - 0x%p / %u < %" PRIu32 "\n",
				 xm->xm_kasaddr_end, xde,
				 sizeof(struct xramdirent), xdh->xdh_nent));
			kmem_free(outbuf, bufsize);
			return (ENXIO);
		}
		break;
	default:
		/* unknown directory type */
		kmem_free(outbuf, bufsize);
		return (ENOTSUP);
	}
	skipcount = uiop->uio_loffset;

	num_ent = xdh->xdh_nent + XRAMFS_NUM_VIRTUAL_NODES;

	dnamehead = xm->xm_dname;
	dnameshift = XMOUNT_XRAMHEADER(xm)->xh_dname_shift;
	/*
	 * don't use XRAM_BYTE2UNIT_ROUNDUP (we think worst case:
	 * e.g. if SIZE_PTR2PTR(...) returns 1, we should set 0, not 1)
	 */
	dnamemaxsize_inunit =
		XRAM_SIZE_PTR2PTR(xm->xm_dname, xm->xm_kasaddr_end)
			>> dnameshift;

	XDTRACE_PROBE2(dirread, struct xnode *, dir, uio_t *, uiop);

	if (skipcount >= XRAMFS_NUM_VIRTUAL_NODES)
		xde += (skipcount - XRAMFS_NUM_VIRTUAL_NODES);

	for (offset = skipcount; offset < num_ent; offset++) {
		int reclen;
		xram_xno_t curxno;
		uint32_t namelen, nameoff;
		size_t left;
		char *name;

		XDTRACE_PROBE2(dirent_loop, struct xramdirent *, xde,
			       ulong_t, outsize);

		switch (offset) {
		case SP_DOT:
			curxno = xdh->xdh_xnodot;
			namelen = 1;
			name = ".";
			break;
		case SP_DOTDOT:
			curxno = xdh->xdh_xnodotdot;
			namelen = 2;
			name = "..";
			break;
		default:
			curxno = xde->xde_xno;
			/* xde points $name_entry[offset - 2] */
			namelen = XRAMDIRENT_LENGTH(xde);
			/* nameoff is in dnamergn unit */
			nameoff = XRAMDIRENT_START(xde);
			name = XRAMDIRENT_DNAME(xm, xde);

			/*
			 * increment xde if offset >= 2
			 *  (i.e. it points real name entry)
			 */
			xde++;
			/*
			 * check name is in correct range:
			 * namelen is smaller than 0x100,
			 * and nameoff is smaller than 0x1000000,
			 * so right expression isn't overflowed.
			 */
			if (dnamemaxsize_inunit <
			    nameoff + XRAM_BYTE2UNIT_ROUNDUP(
				    namelen, dnameshift)) {
				XRAMDBG(("xram_listdir: out of range: "
					 "%" PRIu32 " + %" PRIu32 " > 0x%p "
					 "- 0x%p (shift %" PRIu8 ") \n",
					 nameoff, namelen,  xm->xm_kasaddr_end,
					 xm->xm_dname,  dnameshift));
				continue;
			}
			break;
		}

		reclen = DIRENT64_RECLEN(namelen);
		if (outsize + reclen > bufsize) {
			if (outsize == 0)
				/* no entry was wrote */
				error = EINVAL;
			break;
		}

		/* don't check curxno is valid or not */

		/*
		 * Note that 'name' is NOT NUL terminated, so memcpy
		 *  and append NUL character
		 */
		memcpy(dp->d_name, name, namelen);
		dp->d_name[namelen] = '\0';

		dp->d_reclen = reclen;
		dp->d_ino = XNO_NODEID(curxno);
		dp->d_off = offset;
		dp = (dirent64_t *)((uintptr_t)dp + dp->d_reclen);
		outsize += reclen;
	}

	if (error == 0) {
		error = uiomove(outbuf, outsize, UIO_READ, uiop);
		XDTRACE_PROBE2(copy, ulong_t, outsize, int, error);
		if (error == 0) {
			XDTRACE_PROBE1(copy_done, uio_t *, uiop);
			uiop->uio_loffset = offset;
			/* reached end of the directory */
			if (eofp != NULL)
				*eofp = (offset == num_ent) ? 1 : 0;
		}
	}

	kmem_free(outbuf, bufsize);
	return (error);
}
