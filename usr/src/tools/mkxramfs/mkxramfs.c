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
 * mkxramfs.c
 *
 * Design note:
 *
 * The main strategy is divided into three parts:
 *  (1) scan whole source file tree with composing xrammnode and related infos.
 *  (2) calculate block number offset for all nodes, build complete directory
 *      entries.
 *  (3) dump all out (node-list, directories, symbolic links, file contents).
 *
 * To complete (2), all functions (especially (3)) traverse directory tree
 * with same way; if not, we would write data incorrectly position.
 *
 * in this program, all functions ((1), (2), (3)) are following way
 * to traverse (or build) directory tree.
 *
 * 1. walk entries list to do something for entries.
 * 2. do something for its directory (if needed).
 * 3. walk entries list to enter subdirectories.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <dirent.h>
#include <assert.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <sys/fs/xnode.h>
#include <sys/mkdev.h>

#include <sys/ccompile.h>
#include <sys/inttypes.h>
#include "utils.h"

/* === xramfs parameters === */

#define MKX_MAXPATHLEN	(MAXPATHLEN + 2)	/* 1024 + './' */

/* default parameters */
#define DEFSHIFT_DNAME	(0U)	/* no alignment */
#define DEFSHIFT_SYM	(0U)	/* no alignment */
#define DEFSHIFT_PFILE	(0U)	/* no alignment */
#define DEFSHIFT_PAGE	(12U)	/* 4096 bytes */

/* these are NOT a default parameter - fixed parameter */
#define XNDIRENT_SHIFT_SIZE		((unsigned)XRAMDIR_ALIGN_SHIFT)
#define XRNODE_SHIFT_SIZE		(5U)	/* 32 bytes */

#define XNDIRINFO_MINIMUM_ENTRY		(0)

#define XNO_ROOT	((xram_xno_t)XRAMMNODE_XNO_ROOT)
#define XNO_INVAL	((xram_xno_t)XRAMMNODE_XNO_INVAL)

#define XRAM_XNO_MIN		(1U)		/* minimum: root directory */

/* === parameters, structures for this program === */

/* Define if value which area is filled by zero does not means NULL */
#undef CARE_ZERO_FIELD_IS_NOT_EQUAL_TO_NULL

typedef uintmax_t xstorsize_t;

/*
 * temporary directory entry
 *
 *  nd_next:    pointer of next entry in same directory
 *  nd_xno:     xram node number;
 *              set in scanning stage, represent as xramdirent->xde_xno
 *  nd_nameoff: name field position in directory name entry block size;
 *              set in scanning stage, represent as xramdirent->xde_start
 *  nd_name:    name of node (= filename), NUL terminated string;
 *              set in scanning stage, writing to directory packed name block;
 *              dynamically allocated by strdup()
 *  nd_symtgt:  symbolic link target, non-NUL terminated string
 *              set in scanning stage, writing to symbolic link target block;
 *              dynamically allocated by strdup() if entry is symbolic link,
 *              set to NULL if entry is not a symbolic link.
 *  nd_content: content of directory, pointer to directory information
 *              set to NULL if entry is not a directory.
 *  nd_isfirst: set 1 if copy contents to image,
 *              set 0 if entry is not a regular file (mappable or packed),
 *              or don't copy contents even if it is a regular file
 *              (hard-linked node).
 *  nd_mtime:   if entry is a regular file, last modified time of source file;
 *  		otherwise, set to 0.
 *  nd_ctime:   if entry is a regular file, inode change time of source file;
 *  		otherwise, set to 0.
 */

struct ndirinfo;

struct ndirent {
	struct ndirent	*nd_next;
	xram_xno_t	nd_xno;
	off_t		nd_nameoff;
	char		*nd_name;
	char		*nd_symtgt;
	struct ndirinfo *nd_content;
	int		nd_isfirst;
	time_t		nd_mtime;
	time_t		nd_ctime;
};

/*
 * A directory information
 *
 *   ni_parent:  xram node number of parent directory;
 *               if directory is the root directory,
 *               ni_parent == ni_mine == XNO_ROOT
 *   ni_mine:    xram node number of this directory
 *   ni_nentry:  number of entries
 *   ni_entry:   first entry of directory contents
 *   ni_lastent: pointer of last_entry->nd_next
 *                if scanner option is "don't sort entries";
 *               pointer of ni_entry member if otherwise.
 */

struct ndirinfo {
	xram_xno_t	ni_parent;
	xram_xno_t	ni_mine;
	uint32_t	ni_nentry;
	struct ndirent  *ni_entry;
	struct ndirent  **ni_lastent;
};

/*
 * source inode number <-> xramfs node number conversion table entry
 *
 *   nn_next:		pointer of next entry
 *   nn_sino:		source inode number
 *   nn_xno:		xramfs node number
 */

struct nncnvent {
	struct nncnvent	*nn_next;
	ino_t		nn_sino;
	xram_xno_t	nn_xno;
};

/*
 * source inode number <-> xramfs node number conversion table
 * ($n is hash number generated from nn_sino)
 *
 * nt_first[n]:
 *   first entry of suitable source i-node
 */

#define NODECONVTBL_SIZE	(4096)	/* 32/64KBytes (32/64bit system) */

struct nncnvtbl {
	struct nncnvent	*nt_first[NODECONVTBL_SIZE];
};

/* -- xsd_flags -- */
/* user scan options */
#define SCANOPT_NOSYMLINKS		(0x01)
#define SCANOPT_FIX_MAXNODES		(0x02)
#define SCANOPT_OUT_STATS		(0x04)
#define SCANOPT_DONT_SORT_DIR		(0x08)

/* scan flags */
#define SCANF_IGNORE_DSTCHECK		(0x10)

/* how long do I expand xrammnode array at once */
#define NODE_EXPANDSIZE		(4096)

struct xrscandata {
	int			xsd_flags;
	dev_t			xsd_rootdev;
	dev_t			xsd_dstdev;
	ino_t			xsd_dstino;
	xram_xno_t		xsd_last_assigned;
	struct xramheader	xsd_header;
	struct xrammnode	*xsd_node;
	struct ndirinfo		*xsd_rootdir;
	struct nncnvtbl		xsd_inotbl;
	xram_xno_t		xsd_node_alloced;
	xram_xno_t		xsd_node_max;

	xstorsize_t		xsd_ndentry_unit;
	xstorsize_t		xsd_ndentry_dnunit;
	xstorsize_t		xsd_ndname_unit;
	xstorsize_t		xsd_nsym_unit;
	xstorsize_t		xsd_npack_unit; /* no PACKED_SUPPORT */
	xstorsize_t		xsd_nmap_unit;

	/* following items are for generate statistics */
	/* only number of nodes is counted in scanner */
	xstorsize_t		xsd_nnodes;
	/* following 4 members are counted in decide_position */
	xstorsize_t		xsd_ndirs;
	xstorsize_t		xsd_nsymlinks;
	xstorsize_t		xsd_npackfiles;
	xstorsize_t		xsd_nmapfiles;

	xstorsize_t		xsd_dentry_padsize;
	xstorsize_t		xsd_dname_padsize;
	xstorsize_t		xsd_sym_padsize;
	xstorsize_t		xsd_pack_padsize;
	xstorsize_t		xsd_map_padsize;
};

/*
 * table entry for parsing string argument
 *
 *   sd_name: target string, NUL terminated
 *            if this value is NULL, this entry is last entry of a table.
 *   sd_value: converted value; don't use -1, str2value returns -1
 *             if entry is not found.
 *
 */

struct strdesc {
	const char *sd_name;
	int sd_value;
};

#define MEMBLKSIZ	(4096) /* memory buffer size */
#define MEMEXPSIZ	(4096)

static char blkbuf[MEMBLKSIZ];
static char zerobuf[MEMBLKSIZ]; /* we can use it without initialization */


/*
 *  Macros
 */

#define SIZ2BLK(size, shift) \
	(((uintmax_t)(size) + (1ULL << (shift)) - 1ULL) >> (shift))
/* overflow detectors */
#define ADD_OVERFLOW_HASMAX(cur, add, max) \
	((uintmax_t)(max) < (uintmax_t)(add) || (cur) > (max) - (add))
#define ADD_OVERFLOW_SIMPLE(cur, add) \
	((cur) + (add) < (cur))

/*
 * hash table for i-node <-> xram-node
 */

#define INOTBL_HASH(n)	(int)((n) % (uintmax_t)NODECONVTBL_SIZE)


static void
append_ino(struct nncnvtbl *nt, ino_t ino, xram_xno_t xno)
{
	struct nncnvent *newent;

	newent = malloc(sizeof (struct nncnvent));
	if (newent == NULL)
		die(EXIT_FAILURE, "out of memory\n");

	newent->nn_sino = ino;
	newent->nn_xno = xno;

	trace(3, "inotbl: (new) ino %" PRIuMAX " <-> xno %" PRIuMAX "\n",
	      (uintmax_t)ino, (uintmax_t)xno);

	/* prepend to first of list */
	newent->nn_next = nt->nt_first[INOTBL_HASH(ino)];
	nt->nt_first[INOTBL_HASH(ino)] = newent;
}


static int
search_ino(const struct nncnvtbl *nt, xram_xno_t *res, ino_t ino)
{
	struct nncnvent *cur;

	cur = nt->nt_first[INOTBL_HASH(ino)];
	while (cur != NULL) {
		if (cur->nn_sino == ino) {
			assert(res != NULL);
			*res = cur->nn_xno;
			trace(3, "inotbl: (get) ino %" PRIuMAX " -> xno %"
			      PRIuMAX "\n", (uintmax_t)ino,
			      (uintmax_t)cur->nn_xno);
			return (1);
		}
		cur = cur->nn_next;
	}
	trace(3, "inotbl: (get) ino %" PRIuMAX " is not found\n",
	      (uintmax_t)ino);
	return (0);
}



/*
 *
 *  building directory structure
 *
 */

static void
sorted_insert(struct ndirinfo *ni, struct ndirent *newent,
	      const char *dispheader, const char *dispname)
{
	struct ndirent *cur, **pprev;
	int rval;

	/* walking a list from first */
	for (cur = ni->ni_entry, pprev = &ni->ni_entry; cur != NULL;
	     pprev = &cur->nd_next, cur = cur->nd_next) {
		rval = strcmp(cur->nd_name, newent->nd_name);
		if (rval > 0)
			/* found the insert position */
			break;
		else if (rval == 0)
			/* same named entry in one directory!? */
			die(EXIT_FAILURE, "%s%s: found same name entry\n",
			    dispheader, &dispname[1]);
	}
	/*
	 * insert between pprev and cur
	 * (if above loop is end, append to last of list)
	 */
	newent->nd_next = cur;
	*pprev = newent;
}


static void
adddirent(struct xrscandata *xsd, struct ndirinfo *ni, const char *name,
	  const char *dispheader, const char *dispname, xram_xno_t xno,
	  const char *symtgt, int is_first, time_t ctime, time_t mtime)
{
	struct ndirent *newent;

	newent = malloc(sizeof (struct ndirent));
	if (newent == NULL)
		die(EXIT_FAILURE, "out of memory\n");

	newent->nd_next = NULL;
	newent->nd_xno = xno;
	newent->nd_nameoff = 0; /* only placeholder, set in decide_position */
	newent->nd_content = NULL;
	newent->nd_ctime = ctime;
	newent->nd_mtime = mtime;

	/* check directory name region, name should not be NULL */
	assert(name != NULL);

	newent->nd_name = strdup(name);
	if (newent->nd_name == NULL)
		die(EXIT_FAILURE, "out of memory\n");

	/*
	 * consider symbolic link, symtgt is NULL if node is not
	 * symbolic link.
	 */
	if (symtgt != NULL) {
		newent->nd_symtgt = strdup(symtgt);
		if (newent->nd_symtgt == NULL)
			die(EXIT_FAILURE, "out of memory\n");
		newent->nd_isfirst = 0;
	} else {
		/* not a symbolic link (directory, regular file, etc.) */
		newent->nd_symtgt = NULL;
		newent->nd_isfirst = is_first ? 1 : 0;
	}

	trace(3, "dirent: add: name \"%s\" / sym %s at xno %" PRIuMAX "\n",
	      newent->nd_name, (newent->nd_symtgt != NULL)
	      ? newent->nd_symtgt : "(nil)", (uintmax_t)xno);

	if (ni->ni_nentry >= XRAM_FILES_MAX)
		die(EXIT_FAILURE, "%s%s: exceeds directory entry limit\n",
		    dispheader, &dispname[1]);

	/* now add to directory list, link to list */
	ni->ni_nentry++;
	xsd->xsd_nnodes++;

	if (xsd->xsd_flags & SCANOPT_DONT_SORT_DIR) {
		/* 'linear' option. append to last of list */
		*ni->ni_lastent = newent;
		ni->ni_lastent = &newent->nd_next;
	} else {
		/* 'sort' option. */
		sorted_insert(ni, newent, dispheader, dispname);
	}
}



static xram_xno_t
newxrnode(struct xrscandata *xsd)
{
	xram_xno_t nextnum = xsd->xsd_last_assigned + 1;

	if ((xsd->xsd_flags & SCANOPT_FIX_MAXNODES)
	    && nextnum >= xsd->xsd_node_max)
		die(EXIT_FAILURE,
		    "exceeds xrammnode limit (%" PRIuMAX ")\n",
		    (uintmax_t)(xsd->xsd_node_max - (uintmax_t)XNO_ROOT));

	/*
	 * allocate / enlarge xrammnode array if required
	 *
	 * in 32bit environment, we can acquire 4 Gibi bytes (by definition);
	 * so maximum number of xrammnode (currently 32 bytes) is (2^27).
	 *
	 * If user set maximum number of xrammnode to over 2^27,
	 * it may be overflowed and they see
	 * "out of memory: array size overflow" message
	 * because size_t is defined as unsigned long (iso/iso_stdlib.h)
	 * and its length is 32bits.
	 *
	 */
	if (xsd->xsd_last_assigned >= xsd->xsd_node_alloced - 1
	    || xsd->xsd_node == NULL) {
		struct xrammnode *newptr;
		size_t newsize, newbytes;

		if (xsd->xsd_node_alloced > XRAM_XNO_MAX - NODE_EXPANDSIZE)
			die(EXIT_FAILURE,
			    "exceeds xrammnode hard limit (%" PRIuMAX ")\n",
			    (uintmax_t)XRAM_XNO_MAX + 1ULL);

		/* calculate new size */
		newsize = xsd->xsd_node_alloced + NODE_EXPANDSIZE;
		if ((xsd->xsd_flags & SCANOPT_FIX_MAXNODES)
		    && newsize > xsd->xsd_node_max)
			newsize = xsd->xsd_node_max;

		/* avoid integer overflow */
		newbytes = newsize * sizeof (struct xrammnode);
		if (newbytes / sizeof (struct xrammnode) != newsize)
			die(EXIT_FAILURE, "out of memory: "
			    "array size overflow\n");

		newptr = realloc(xsd->xsd_node, newbytes);
		trace(3, "node (prev)size %" PRIuMAX ", position %p (new)size "
		      "%" PRIuMAX " (%" PRIuMAX " Bytes), position %p\n",
		      (uintmax_t)xsd->xsd_node_alloced, xsd->xsd_node,
		      (uintmax_t)newsize, (uintmax_t)newbytes, newptr);
		if (newptr == NULL)
			die(EXIT_FAILURE,
			    xsd->xsd_node == NULL ?
			    "out of memory\n" :
			    "out of memory: expand memory\n");
		xsd->xsd_node_alloced = newsize;
		xsd->xsd_node = newptr;
	}
	xsd->xsd_last_assigned = nextnum;
	xsd->xsd_header.xh_files++;
	memset(&xsd->xsd_node[nextnum], 0, sizeof (struct xrammnode));
	return (nextnum);
}


/*
 * initialize xrammnode PARTIALLY with result of stat();
 * return 0 if succeed, return -1 if failed (and set errno)
 *
 * errors:
 *  ENOTSUP - currently type of this node is not supported.
 */
static int
initxrnode(struct xrammnode *xmn, struct stat *st, const char *heading,
	   const char *name)
{
	xmn->xmn_nlink = 1;
	xmn->xmn_uid = st->st_uid;
	xmn->xmn_gid = st->st_gid;
	xmn->xmn_time_sec = st->st_mtime;
#ifdef __sun /* don't use __SunOS, which is not defined in gcc */
	xmn->xmn_time_nsec = st->st_mtim.tv_nsec;
#else
	xmn->xmn_time_nsec = 0;
#endif
	xmn->xmn_typemode = st->st_mode & (~S_IFMT);
	switch (st->st_mode & S_IFMT) {
	case S_IFLNK: /* symlink */
		xmn->xmn_typemode |= XIFLNK;
		break;
	case S_IFREG: /* regular file */
		/*
		 * no PACKED_SUPPORT:
		 * currently, all regular files are mappable file..
		 */
		xmn->xmn_typemode |= XIFMAP;
		break;
	case S_IFCHR: /* character device */
		xmn->xmn_typemode |= XIFCHR;
		break;
	case S_IFBLK: /* block device */
		xmn->xmn_typemode |= XIFBLK;
		break;
	case S_IFDIR: /* directory */
		xmn->xmn_typemode |= XIFDIR;
		break;
	case S_IFIFO: /* FIFO */
		xmn->xmn_typemode |= XIFIFO;
		break;
	case S_IFSOCK: /* socket */
	default:
		errno = ENOTSUP;
		return (-1);
	}
	return (0);
}


static void
freexrnode(struct xrscandata *xsd, xram_xno_t xno)
{
	/* assume "previous assigned XNO = newly assigned XNO - 1" */
	assert(xsd->xsd_last_assigned == xno);
	xsd->xsd_last_assigned--;
	xsd->xsd_header.xh_files--;
}



static void
inc_links(struct xrscandata *xsd, xram_xno_t xno, const char *heading,
	  const char *name)
{
	/* increase nlinks */
	if (xsd->xsd_node[xno].xmn_nlink >= XRAM_LINK_MAX)
		die(EXIT_FAILURE, "%s%s: too many hard links\n",
		    heading, &name[1]);
	xsd->xsd_node[xno].xmn_nlink++;
}



/*
 * allocate buffer with malloc() and read symbolic link target completely.
 * (readlink() may be partially read if buffer is too small to store it)
 */
static char *
xreadlink(const char *name)
{
	char *newptr, *ptr = NULL;
	size_t sz = 0;
	ssize_t rsize;

	trace(3, "readlink: %s\n", name);
	while (1) {
		if (sz + MEMEXPSIZ < sz) {
			if (ptr != NULL)
				free(ptr);
			errno = ENOMEM;
			return (NULL);
		}
		sz = sz + MEMEXPSIZ;
		newptr = realloc(ptr, sz);
		if (newptr == NULL)
			die(EXIT_FAILURE,
			    ptr == NULL ? "out of memory\n" :
			    "out of memory: expand memory\n");
		ptr = newptr;
		trace(3, "readlink: bufsize %" PRIuMAX "\n", (uintmax_t)sz);
		rsize = readlink(name, ptr, sz);
		if (rsize == (ssize_t)-1) {
			free(ptr);
			return (NULL);
		}
		if (rsize < sz) {
			/* readlink don't do NUL termination */
			ptr[rsize] = '\0';
			trace(3, "readlink: result \"%s\" (size %" PRIuMAX
			      ")\n", ptr, (uintmax_t)rsize);
			return (ptr);
		}
	}
}



static void
scan_onedir(struct xrscandata *xsd, struct ndirinfo *ni, const char *dirname,
	    const char *heading)
{
	struct dirent *dent;
	struct ndirent *nd;
	char pathbuf[MAXPATHLEN], *pbf;
	struct stat st;
	struct xrammnode *xmn;
	size_t dnlen, entlen, pathleft;
	xram_xno_t xno;
	int rval;
	DIR *dp;

	dnlen = strlen(dirname);
	/* 2 == "/\0" (to append slash and NUL termination) */
	if (dnlen + 2 > sizeof(pathbuf)) {
		errno = ENAMETOOLONG;
		die(EXIT_FAILURE, "%s%s: failed to scan", heading,
		    &dirname[1]);
	}
	pathleft = sizeof(pathbuf) - dnlen - 2;
	if (dnlen > 0)
		memcpy(pathbuf, dirname, dnlen);

	/* append '/' */
	pbf = &pathbuf[dnlen + 1];
	pbf[-1] = '/';
	pbf[0] = '\0';

	trace(3, "scan: heading \"%s\" pathbuf \"%s\"\n", heading, pathbuf);
	trace(1, "scanning %s%s\n", heading, &dirname[1]);
	dp = opendir(dirname);
	if (dp == NULL)
		die(EXIT_FAILURE, "%s%s: failed to scan",
		    heading, &dirname[1]);

	/* scan contents of target directory */
	errno = 0;
	for (; (dent = readdir(dp)) != NULL; errno = 0) {
		trace(3, "scan: found entry: %s\n", dent->d_name);

		/* skip empty-named node ("") for safe */
		if (dent->d_name[0] == '\0') {
			/* To display only target directory, insert NUL */
			pbf[-1] = '\0';
			warnin("WARNING: %s%s: contains an empty-named node\n",
			       heading, &pathbuf[1]);
			pbf[-1] = '/';
			continue;
		}

		/* skip "." and ".." */
		if (dent->d_name[0] == '.'
		    && (dent->d_name[1] == '\0'
			|| (dent->d_name[1] == '.'
			    && dent->d_name[2] == '\0')))
			continue;

		/* verify name and path length */
		entlen = strlen(dent->d_name);
		if (entlen > pathleft || entlen > XRAM_XDE_LEN_MAX) {
			errno = ENAMETOOLONG;
			die(EXIT_FAILURE, "%s%s: failed to scan %s",
			    heading, &dirname[1], dent->d_name);
		}

		memcpy(pbf, dent->d_name, entlen);
		pbf[entlen] = '\0';
		trace(3, "scan: ... target=%s\n", pathbuf);

		if (xsd->xsd_flags & SCANOPT_NOSYMLINKS)
			rval = stat(pathbuf, &st);
		else
			rval = lstat(pathbuf, &st);

		if (rval < 0)
			die(EXIT_FAILURE, "%s%s: failed to stat",
			    heading, &pathbuf[1]);

		if (xsd->xsd_rootdev != st.st_dev) {
			warnin("WARNING: %s%s: not on same device, skipped\n",
			       heading, &pathbuf[1]);
			continue;
		}

		trace(3, "scan: ... inode: %" PRIuMAX "\n",
		      (uintmax_t)st.st_ino);

		if (!(xsd->xsd_flags & SCANF_IGNORE_DSTCHECK)
		    && xsd->xsd_dstdev == st.st_dev
		    && xsd->xsd_dstino == st.st_ino) {
			warnin("WARNING: %s%s: detect the target file, "
			       "skipped\n", heading, &pathbuf[1]);
			continue;
		}

		if (search_ino(&xsd->xsd_inotbl, &xno, st.st_ino) != 0) {
			/* hard-linked. save name and entry only */
			/* increase nlinks */
			trace(3, "scan: ... is hard-linked\n");
			inc_links(xsd, xno, heading, pathbuf);
			adddirent(xsd, ni, dent->d_name, heading,
				  pathbuf, xno, NULL,
				  0 /* do not copy content */, st.st_ctime,
				  st.st_mtime);
			continue;
		}

		/* new entry - allocate new node number */
		xno = newxrnode(xsd);
		xmn = &xsd->xsd_node[xno];
		trace(3, "scan: ... allocate xno %" PRIuMAX "\n",
		      (uintmax_t)xno);
		/* compose xrammnode from stat */
		if (initxrnode(xmn, &st, heading, pathbuf) < 0) {
			if (errno == ENOTSUP) {
				warnin("WARNING: "
				       "%s%s: this node type (0x%" PRIxMAX ")"
				       " is not supported, skipped\n",
				       heading, &pathbuf[1],
				       (uintmax_t)(st.st_mode & S_IFMT));
				freexrnode(xsd, xno);
				continue;
			} else {
				die(EXIT_FAILURE,
				    "%s%s: failed to initialize xrammnode",
				    heading, &pathbuf[1]);
			}
		}

		/* append dirent */
		trace(3, "scan: ... type/mode 0%06" PRIo16 "\n",
		      xmn->xmn_typemode);
		if ((xmn->xmn_typemode & XIFMT) == XIFLNK) {
			char *cnt;

			cnt = xreadlink(pathbuf);
			if (cnt == NULL)
				die(EXIT_FAILURE,
				    "%s%s: failed to readlink",
				    heading, &pathbuf[1]);
			if (strlen(cnt) > XRAM_XMN_SIZE_MAX)
				die(EXIT_FAILURE,
				    "%s%s: content is too large\n",
				    heading, &pathbuf[1]);
			adddirent(xsd, ni, dent->d_name, heading,
				  pathbuf, xno, cnt, 0, (time_t)0, (time_t)0);
			free(cnt);
		} else if ((xmn->xmn_typemode & XIFMT) == XIFMAP) {
			if ((uintmax_t)st.st_size
			    > (uintmax_t)XRAM_XMN_SIZE_MAX)
				die(EXIT_FAILURE,
				    "%s%s: file is too large\n",
				    heading, &pathbuf[1]);
			adddirent(xsd, ni, dent->d_name, heading,
				  pathbuf, xno, NULL, 1, st.st_ctime,
				  st.st_mtime);
#if 0 /* no PACKED_SUPPORT */
		} else if ((xmn->xmn_typemode & XIFMT) == XIFPACK) {
			if ((uintmax_t)st.st_size
			    > (uintmax_t)XRAM_XMN_SIZE_MAX)
				die(EXIT_FAILURE,
				    "%s%s: file is too large\n",
				    heading, &pathbuf[1]);
			adddirent(xsd, ni, dent->d_name, heading,
				  pathbuf, xno, NULL, 1,s t.st_ctime,
				  st.st_mtime);
#endif
		} else {
			adddirent(xsd, ni, dent->d_name, heading,
				  pathbuf, xno, NULL, 0, (time_t)0, (time_t)0);
		}
		append_ino(&xsd->xsd_inotbl, st.st_ino, xno);

		/* node type specific operation */
		switch ((xmn->xmn_typemode & XIFMT)) {
		case XIFPACK:
#if 0 /* no PACKED_SUPPORT */
			xmn->xmn_size = st.st_size;
#else
			assert(0);
#endif
			break;
		case XIFMAP:
			xmn->xmn_size = st.st_size;
			break;
		case XIFBLK:
		case XIFCHR:
			xmn->xmn_rdev_major = major(st.st_rdev);
			xmn->xmn_rdev_minor = minor(st.st_rdev);
			break;
		case XIFLNK:
		case XIFDIR:
			xmn->xmn_start = 0;
			xmn->xmn_size = 0;
			break;
		case XIFIFO:
		default:
			memset(&(xmn->xmn_union), 0, sizeof(xmn->xmn_union));
			break;
		}
	}
	if (errno != 0)
		die(EXIT_FAILURE, "%s%s: failed to read entries",
		    heading, &dirname[1]);
	closedir(dp);
	/* from here, do NOT REORDER ni->ni_first LIST */

	/* recurse subdirectories */
	for (nd = ni->ni_entry; nd != NULL; nd = nd->nd_next) {
		struct ndirinfo *newdir;

		if ((xsd->xsd_node[nd->nd_xno].xmn_typemode
		     & XIFMT) != XIFDIR)
			continue;
		strcpy(pbf, nd->nd_name);
		newdir = malloc(sizeof (struct ndirinfo));
		if (newdir == NULL)
			die(EXIT_FAILURE, "out of memory\n");

		newdir->ni_parent = ni->ni_mine;
		newdir->ni_mine = nd->nd_xno;
		newdir->ni_entry = NULL;
		newdir->ni_lastent = &newdir->ni_entry;
		newdir->ni_nentry = XNDIRINFO_MINIMUM_ENTRY;
		inc_links(xsd, nd->nd_xno, heading, pathbuf);
		scan_onedir(xsd, newdir, pathbuf, heading);
		inc_links(xsd, ni->ni_mine, heading, dirname);
		nd->nd_content = newdir;
	}
	trace(3, "scan: %s[%" PRIuMAX "]: link %" PRIuMAX "\n",
	      dirname, (uintmax_t)ni->ni_mine,
	      (uintmax_t)xsd->xsd_node[ni->ni_mine].xmn_nlink);
}



static void
run_scanner(struct xrscandata *xsd, const char *name)
{
	struct ndirinfo *newdir;
	const char *pseudoname = "\0";
	const char *myname = ".";
	struct stat st;
	xram_xno_t xno;

	if (stat(myname, &st) < 0)
		die(EXIT_FAILURE, "%s: failed to stat", name);
	if (!S_ISDIR(st.st_mode)) {
		errno = ENOTDIR;
		die(EXIT_FAILURE, "%s: failed to scan", name);
	}
	xno = newxrnode(xsd);
	assert(xno == XNO_ROOT);
	xsd->xsd_rootdev = st.st_dev;
	trace(3, "scan: %s is on (%" PRIuMAX ", %" PRIuMAX ")\n", name,
	      (uintmax_t)major(st.st_dev),
	      (uintmax_t)minor(st.st_dev));
	/*
	 * initxrnode should not be failed because node is directory
	 * (checking "if (!S_ISDIR(...))", up to about 10 lines);
	 * but for further use of initxrnode, check error or not
	 */
	if (initxrnode(&xsd->xsd_node[xno], &st, name, pseudoname) < 0)
		die(EXIT_FAILURE, "failed to initialize xrammnode for %s",
		    name);
	inc_links(xsd, xno, name, pseudoname);
	trace(3, "links: %" PRIuMAX "\n",
	      (uintmax_t)xsd->xsd_node[xno].xmn_nlink);
	newdir = malloc(sizeof (struct ndirinfo));
	if (newdir == NULL)
		die(EXIT_FAILURE, "out of memory\n");

	newdir->ni_parent = XNO_ROOT;
	newdir->ni_mine = XNO_ROOT;
	newdir->ni_nentry = XNDIRINFO_MINIMUM_ENTRY;
	newdir->ni_entry = NULL;
	newdir->ni_lastent = &newdir->ni_entry;
	scan_onedir(xsd, newdir, myname, name);
	xsd->xsd_rootdir = newdir;
}


/*
 *
 * Initialization
 *
 */


static void
initheader(struct xramheader *xh)
{
	memset(xh, 0, sizeof (struct xramheader));
	xh->xh_magic = XRAM_MAGIC;
	xh->xh_version = XRAMFS_IMGVERSION;
	xh->xh_ctime = (uint32_t)time(NULL);
	xh->xh_dname_shift = DEFSHIFT_DNAME;
	xh->xh_symlink_shift = DEFSHIFT_SYM;
	xh->xh_pfile_shift = DEFSHIFT_PFILE;
	xh->xh_pageblk_shift = DEFSHIFT_PAGE;
}



static void
initscandata(struct xrscandata *xsd)
{
#ifdef CARE_ZERO_FIELD_IS_NOT_EQUAL_TO_NULL
	int i;
#endif /* defined(CARE_ZERO_FIELD_IS_NOT_EQUAL_TO_NULL) */

	memset(xsd, 0, sizeof (struct xrscandata));
	xsd->xsd_last_assigned = XNO_ROOT - 1;
	initheader(&xsd->xsd_header);
#ifdef CARE_ZERO_FIELD_IS_NOT_EQUAL_TO_NULL
	xsd->xsd_node = NULL;
	xsd->xsd_rootdir = NULL;
	for (i = 0; i < NODECONVTBL_SIZE; i++)
		xsd->xsd_inotbl.nt_first[i] = NULL;
#endif /* defined(CARE_ZERO_FIELD_IS_NOT_EQUAL_TO_NULL) */
}


/*
 *
 *  Complete size and position
 *
 */


/* recalculate by specified unit with integer overflow checks */
static int
convert_unit(xstorsize_t *newunit, xstorsize_t unit, int shift, int newshift)
{
	xstorsize_t mask, rval;

	if (shift == newshift) {
		*newunit = unit;
		return (0);
	} else if (shift > newshift) {
		rval = unit << (shift - newshift);
		/* check integer overflow */
		if ((rval >> (shift - newshift)) != unit)
			return (-1);
		*newunit = rval;
		return (0);
	} else {
		int shiftsize = newshift - shift;

		if (shiftsize >= sizeof(xstorsize_t) * CHAR_BIT)
			return (-1);
		mask = (xstorsize_t)1ULL << shiftsize;
		mask--;
		rval = unit >> shiftsize;
		if (((xstorsize_t)unit & mask) != 0)
			rval++;
		*newunit = rval;
		return (0);
	}
}


static void
fixup_position_header(struct xrscandata *xsd)
{
	xram_blkno_t used_block = 0;
	uintmax_t dirsize;
	xstorsize_t datablock, fixdirsize;

	/* calculate header + node size in "page" blocks */
	if (convert_unit(
		    &datablock, (uintmax_t)xsd->xsd_last_assigned + 1ULL,
		    XRNODE_SHIFT_SIZE, xsd->xsd_header.xh_pageblk_shift) < 0)
		die(EXIT_FAILURE, "page size is too big or too many nodes\n");

	if ((uintmax_t)datablock > (uintmax_t)XRAM_BLKNO_MAX)
		die(EXIT_FAILURE, "node information block: "
		    "exceeds block count limit\n");

	trace(3, "header used %" PRIuMAX " blocks\n", (uintmax_t)datablock);
	used_block = datablock;

	/* calculate directory block size in "page" blocks */

	/*
	 * at first, check constant directory entry region
	 * and convert unit to dnamesize
	 */
	if (convert_unit(
		    &fixdirsize, xsd->xsd_ndentry_unit,
		    XNDIRENT_SHIFT_SIZE, xsd->xsd_header.xh_dname_shift) < 0)
		die(EXIT_FAILURE, "directory entry name unit size is too big "
		    "or too many directory entries\n");

	xsd->xsd_ndentry_dnunit = fixdirsize;
	dirsize = fixdirsize + xsd->xsd_ndname_unit;
	/* checking integer overflow */
	if (dirsize < fixdirsize)
		die(EXIT_FAILURE, "directory entry name unit size is too big "
		    "or too many directory name entries\n");

	if (xsd->xsd_ndname_unit > 0)
		xsd->xsd_header.xh_off_dnamergn = xsd->xsd_ndentry_dnunit;
	else
		xsd->xsd_header.xh_off_dnamergn = 0;
	trace(3, "directory name region is start from #%" PRIuMAX "\n",
	      (uintmax_t)fixdirsize);

	/* dnamesize -> blocksize */
	if (convert_unit(
		    &datablock, dirsize, xsd->xsd_header.xh_dname_shift,
		    xsd->xsd_header.xh_pageblk_shift) < 0)
		die(EXIT_FAILURE, "directory entry name unit size is too big "
		    "or too much directory information\n");

	/* checking integer overflow */
	if (ADD_OVERFLOW_HASMAX(used_block, datablock, XRAM_BLKNO_MAX))
		die(EXIT_FAILURE, "directory entry block: "
		    "exceeds block count limit\n");

	trace(2, "directory used dentry %" PRIuMAX " units + dname %"
	      PRIuMAX " units = %" PRIuMAX " blocks\n",
	      (uintmax_t)xsd->xsd_ndentry_unit,
	      (uintmax_t)xsd->xsd_header.xh_dname_shift,
	      (uintmax_t)datablock);
	xsd->xsd_header.xh_off_dir = used_block;
	used_block += datablock;

	/* calculate symbolic target size in "page" blocks */
	if (convert_unit(
		    &datablock, xsd->xsd_nsym_unit,
		    xsd->xsd_header.xh_symlink_shift,
		    xsd->xsd_header.xh_pageblk_shift) < 0)
		die(EXIT_FAILURE, "symbolic link target unit size "
		    "or symbolic link data is too big\n");

	if (ADD_OVERFLOW_HASMAX(used_block, datablock, XRAM_BLKNO_MAX))
		die(EXIT_FAILURE, "symbolic link target block: "
		    "exceeds block count limit\n");

	trace(2, "symlink used %" PRIuMAX " units = %" PRIuMAX " blocks\n",
	      (uintmax_t)xsd->xsd_nsym_unit, (uintmax_t)datablock);

	if (datablock > 0)
		xsd->xsd_header.xh_off_symlink = used_block;
	else
		xsd->xsd_header.xh_off_symlink = 0;
	used_block += datablock;

	/* no PACKED_SUPPORT: checking packed files block ... */
	xsd->xsd_header.xh_off_pfile = 0;

	/*
	 * count used blocks of mappable file contents;
	 * do not need to call convert_unit() - unit of mappable file is equal
	 * to block size
	 */

	/* check integer overflow */
	if (ADD_OVERFLOW_HASMAX(used_block, xsd->xsd_nmap_unit,
				XRAM_BLKNO_MAX))
		die(EXIT_FAILURE, "mappable file contents block: "
		    "exceeds block count limit\n");

	trace(3, "mappable used %" PRIuMAX " blocks\n",
	      (uintmax_t)xsd->xsd_nmap_unit);
	if (xsd->xsd_nmap_unit > 0)
		xsd->xsd_header.xh_off_xfile = used_block;
	else
		xsd->xsd_header.xh_off_xfile = 0;
	used_block += xsd->xsd_nmap_unit;

	xsd->xsd_header.xh_blocks = used_block;

	trace(2, "node             : from %10" PRIuMAX " [0x%016" PRIxMAX
	      "], %" PRIuMAX " * 32bytes\n",
	      0ULL, 0ULL, (uintmax_t)xsd->xsd_last_assigned + 1ULL);
	trace(2, "directory (entry): from %10" PRIuMAX " [0x%016" PRIxMAX
	      "], %" PRIuMAX " * 16bytes\n",
	      (uintmax_t)xsd->xsd_header.xh_off_dir,
	      (uintmax_t)xsd->xsd_header.xh_off_dir <<
	      (uintmax_t)xsd->xsd_header.xh_pageblk_shift,
	      (uintmax_t)xsd->xsd_ndentry_unit);
	trace(2, "directory (name) : from .......... "
	      "[..................], %" PRIuMAX " * (2^%u)bytes\n",
	      (uintmax_t)xsd->xsd_ndname_unit,
	      xsd->xsd_header.xh_dname_shift);
	trace(2, "symbolic link    : from %10" PRIuMAX " [0x%016" PRIxMAX
	      "], %" PRIuMAX " * (2^%u)bytes\n",
	      (uintmax_t)xsd->xsd_header.xh_off_symlink,
	      (uintmax_t)xsd->xsd_header.xh_off_symlink <<
	      (uintmax_t)xsd->xsd_header.xh_pageblk_shift,
	      (uintmax_t)xsd->xsd_nsym_unit,
	      xsd->xsd_header.xh_symlink_shift);
	trace(2, "packed data      : from %10" PRIuMAX " [0x%016" PRIxMAX
	      "]\n",
	      (uintmax_t)xsd->xsd_header.xh_off_pfile,
	      (uintmax_t)xsd->xsd_header.xh_off_pfile <<
	      (uintmax_t)xsd->xsd_header.xh_pageblk_shift);
	trace(2, "mappable data    : from %10" PRIuMAX " [0x%016" PRIxMAX
	      "]\n",
	      (uintmax_t)xsd->xsd_header.xh_off_xfile,
	      (uintmax_t)xsd->xsd_header.xh_off_xfile <<
	      (uintmax_t)xsd->xsd_header.xh_pageblk_shift);
}



static void
decide_position_onedir(struct xrscandata *xsd, struct ndirinfo *ni,
		       const char *dirname, const char *heading)
{
	struct ndirent *nd;
	struct xrammnode *xmn;
	xstorsize_t direntryunit, dentrysize = sizeof (struct xramdircheader);
	uint32_t dnamesize_total = 0, namelen, namelen_unit;

	char pathbuf[MKX_MAXPATHLEN], *namep, *disppath;
	size_t pathlen;

	pathlen = strlen(dirname);
	assert(pathlen > 0);
	memcpy(pathbuf, dirname, pathlen);
	namep = &pathbuf[pathlen + 1];
	namep[-1] = '/';
	namep[0] = '\0'; /* for safe */
	disppath = &pathbuf[1];

	/*
	 * check all directory entry,
	 * and calculate start point and size (in bytes) of
	 * all regular files and symbolic link
	 */
	for (nd = ni->ni_entry; nd != NULL; nd = nd->nd_next) {
		xstorsize_t namelen_withpad;

		/*
		 * at this point, strcpy() is safe:
		 * path length check is already done in scan_onedir
		 * (search with ENAMETOOLONG), so pathbuf is enough to store
		 * name entries.
		 */
		strcpy(namep, nd->nd_name);
		trace(1, "positioning %s%s\n", heading, disppath);

		nd->nd_nameoff = xsd->xsd_ndname_unit;
		namelen = strlen(nd->nd_name);
		namelen_unit = SIZ2BLK(namelen,
				       xsd->xsd_header.xh_dname_shift);
		namelen_withpad = namelen_unit
			<< xsd->xsd_header.xh_dname_shift;
		if (ADD_OVERFLOW_HASMAX(dnamesize_total, namelen_withpad,
					UINT32_MAX))
			die(EXIT_FAILURE,
			    "%s%s: directory name size is too big\n",
			    heading, disppath);
		dnamesize_total += namelen_withpad;
		if (ADD_OVERFLOW_HASMAX(xsd->xsd_ndname_unit, namelen_unit,
					XRAM_XDE_START_MAX))
			die(EXIT_FAILURE,
			    "%s%s: exceeds directory packed name "
			    "region size limit\n", heading, disppath);

		xsd->xsd_ndname_unit += namelen_unit;
		xmn = &xsd->xsd_node[nd->nd_xno];

		/* no PACKED_SUPPORT: don't support packed file... */

		if ((xmn->xmn_typemode & XIFMT) == XIFMAP
		    && nd->nd_isfirst != 0) {
			/* mappable file */
			xstorsize_t fsize_blk;

			fsize_blk = SIZ2BLK(
				xmn->xmn_size,
				xsd->xsd_header.xh_pageblk_shift);
			if (xsd->xsd_nmap_unit
			    > XRAM_XMN_START_MAX - fsize_blk)
				die(EXIT_FAILURE, "%s%s: exceeds mappable file"
				    " contents size limit\n",
				    heading, disppath);
			xmn->xmn_start = xsd->xsd_nmap_unit;
			xsd->xsd_nmap_unit += fsize_blk;
			xsd->xsd_nmapfiles++;
		} else if (nd->nd_symtgt != NULL) {
			/*
			 * symbolic link
			 *
			 * NOTE:
			 * we don't worry about hard-linked symbolic link:
			 * even if there are exists, only one entry have
			 * nn->symtgt (content) and others are only pointed
			 * to same node number.
			 */
			size_t symlen;
			xstorsize_t symlen_unit;

			symlen = strlen(nd->nd_symtgt);
			symlen_unit = SIZ2BLK(
				symlen, xsd->xsd_header.xh_symlink_shift);
			xmn->xmn_size = symlen;
			xmn->xmn_start = xsd->xsd_nsym_unit;
			if (ADD_OVERFLOW_HASMAX(xsd->xsd_nsym_unit,
						symlen_unit,
						XRAM_XMN_START_MAX))
				die(EXIT_FAILURE,
				    "%s%s: exceeds symbolic link target "
				    "block size limit\n", heading, disppath);

			xsd->xsd_nsym_unit += symlen_unit;
			xsd->xsd_nsymlinks++;
		}
		dentrysize += (xstorsize_t)(sizeof (struct xramdirent));
	}
	/* check and set directory size and position */
	namep[-1] = '\0';
	trace(1, "position %s%s\n", heading, disppath);
	direntryunit = SIZ2BLK(dentrysize, XNDIRENT_SHIFT_SIZE);

	/*
	 * dentrysize > 0, so direntryunit > 0, and below condition
	 * should not be occurred... except integer overflow.
	 */
	if (ADD_OVERFLOW_SIMPLE(xsd->xsd_ndentry_unit, direntryunit))
		die(EXIT_FAILURE, "%s%s: exceeds directory entry "
		    "region size limit\n", heading, disppath);
	if (dentrysize + dnamesize_total > XRAM_XMN_SIZE_MAX)
		die(EXIT_FAILURE, "%s%s: directory entry is too big\n",
		    heading, disppath);

	xmn = &xsd->xsd_node[ni->ni_mine];
	xmn->xmn_start = xsd->xsd_ndentry_unit;
	xmn->xmn_size = dentrysize + dnamesize_total;
	xsd->xsd_ndentry_unit += direntryunit;
	xsd->xsd_ndirs++;
	namep[-1] = '/';

	/* last, recurse subdirectories */
	for (nd = ni->ni_entry; nd != NULL; nd = nd->nd_next) {
		xmn = &xsd->xsd_node[nd->nd_xno];
		if ((xmn->xmn_typemode & XIFMT) == XIFDIR) {
			strcpy(namep, nd->nd_name);
			decide_position_onedir(xsd, nd->nd_content, pathbuf,
					       heading);
		}
	}
}



/*
 * Writing header, nodes, directories and contents
 */



static xstorsize_t
add_padding(FILE *outfp, const char *outname, xstorsize_t outsize,
	    int align_log2)
{
	xstorsize_t pad_mask, unitsize, padsize, pad_left;
	size_t writesize, wrotelen;

	if (align_log2 <= 0)
		return (0);
	if (align_log2 >= sizeof(xstorsize_t) * CHAR_BIT)
		die(EXIT_FAILURE,
		    "alignment size 2^%" PRIuMAX " is too big to handle\n",
		    (uintmax_t)align_log2);

	unitsize = (1ULL << align_log2);
	pad_mask = unitsize - 1ULL;
	padsize = (unitsize - (outsize & pad_mask)) & pad_mask;

	trace(3, "padding: size %" PRIuMAX ", align 2^%u, padsize %"
	      PRIuMAX "\n", (uintmax_t)outsize, align_log2,
	      (uintmax_t)padsize);

	/*
	 * considering about underflow of pad_left:
	 *
	 * wrotelen must be smaller than or equal to pad_left,
	 * because wrotelen is equal to writesize, and writesize is
	 * smaller than or equal to pad_left.
	 * so padleft -= wrotelen is not underflowed.
	 */
	for (pad_left = padsize; pad_left > 0; pad_left -= wrotelen) {
		writesize = sizeof(zerobuf) < pad_left ?
			sizeof(zerobuf) : pad_left;
		wrotelen = fwrite(zerobuf, 1, writesize, outfp);
		if (wrotelen != writesize)
			die(EXIT_FAILURE,
			    (ferror(outfp) != 0)
			    ? "%s: failed to write"
			    : "%s: failed to write: unknown error\n",
			    outname);
	}
	return (padsize);
}


static void
write_data(FILE *outfp, const char *outname, const void *data,
	size_t size, const char *desc)
{
	size_t wrotelen;

	/*
	 * According C99, fwrite() returns value which is smaller than nmemb
	 * ONLY if error occurs; so we can ignore 'partial write'.
	 */

	wrotelen = fwrite(data, 1, size, outfp);
	if (wrotelen != size)
		die(EXIT_FAILURE, (ferror(outfp) != 0) ? "%s: failed to write"
		    : "%s: failed to write: unknown error\n", outname);
}



static void
write_header_and_nodes(FILE *outfp, const char *outname,
		       struct xrscandata *xsd)
{
	size_t wrote, num_elems;

	/* this may be configure operation */
	assert(sizeof (struct xrammnode) == (1U << XRNODE_SHIFT_SIZE));
	assert(sizeof (xsd->xsd_header)
	       == sizeof (struct xrammnode) * XNO_ROOT);

	write_data(outfp, outname, &xsd->xsd_header, sizeof(xsd->xsd_header),
		   "xramfs header");

	num_elems = xsd->xsd_last_assigned - XNO_ROOT + 1U;
	trace(3, "wrhead: %" PRIuMAX " node to write\n", (uintmax_t)num_elems);
	wrote = fwrite(&xsd->xsd_node[XNO_ROOT],
		       sizeof (struct xrammnode), num_elems, outfp);
	if (wrote < num_elems)
		die(EXIT_FAILURE,
		    (ferror(outfp) != 0)
		    ? "%s: failed to write node data"
		    : "%s: failed to write node data: unknown error\n",
		    outname);
	(void)add_padding(
		outfp, outname,
		((uintmax_t)xsd->xsd_last_assigned + 1ULL)
		<< XRNODE_SHIFT_SIZE,
		xsd->xsd_header.xh_pageblk_shift);
}



static void
write_dentry_onedir(FILE *outfp, const char *outname,
		    struct xrscandata *xsd, struct ndirinfo *ni)
{
	/* output directory entry */
	struct ndirent *nd;
	struct xrammnode *xmn;
	size_t fragsize;
	xstorsize_t padsize;
	struct xramdircheader xdir;

	memset(&xdir, 0, sizeof(xdir));

	xdir.xdh_nent = ni->ni_nentry;
	xdir.xdh_xnodot = ni->ni_mine;
	xdir.xdh_xnodotdot = ni->ni_parent;
	if (xsd->xsd_flags & SCANOPT_DONT_SORT_DIR)
		xdir.xdh_type = XRAM_DTYPE_LINEAR;
	else
		xdir.xdh_type = XRAM_DTYPE_SORTED;
	write_data(outfp, outname, &xdir, sizeof(xdir), "directory entry");

	fragsize = sizeof(xdir);
	trace(3, "dir: %" PRIuMAX " node(s), xno %" PRIuMAX ", "
	      "parent-xno %" PRIuMAX ", type %" PRIuMAX "\n",
	      (uintmax_t)xdir.xdh_nent, (uintmax_t)xdir.xdh_xnodot,
	      (uintmax_t)xdir.xdh_xnodotdot, (uintmax_t)xdir.xdh_type);

	/* check all nodes in target directory */
	for (nd = ni->ni_entry; nd != NULL; nd = nd->nd_next) {
		struct xramdirent xde;

		xde.xde_xno = nd->nd_xno;
		xde.xde_start_len
			= (((nd->nd_nameoff << XRAMDIRENT_START_MASK_SHIFT)
			    & XRAMDIRENT_START_MASK)
			   | ((strlen(nd->nd_name)
			       << XRAMDIRENT_LENGTH_MASK_SHIFT)
			      & XRAMDIRENT_LENGTH_MASK));
		write_data(outfp, outname, &xde, sizeof (struct xramdirent),
			   "directory entry");
		trace(3, "   : ... have xno %" PRIuMAX ", "
		      "name %" PRIuMAX " \"%s\" (-> 0x%08" PRIxMAX ")\n",
		      (uintmax_t)nd->nd_xno, (uintmax_t)nd->nd_nameoff,
		      nd->nd_name, (uintmax_t)xde.xde_start_len);
		fragsize += (sizeof (struct xramdirent));
	}
	padsize = add_padding(outfp, outname, fragsize, XNDIRENT_SHIFT_SIZE);
	xsd->xsd_dentry_padsize += padsize;

	/* recurse subdirectories */
	for (nd = ni->ni_entry; nd != NULL; nd = nd->nd_next) {
		xmn = &xsd->xsd_node[nd->nd_xno];
		if ((xmn->xmn_typemode & XIFMT) == XIFDIR) {
			write_dentry_onedir(outfp, outname, xsd,
					    nd->nd_content);
		}
	}
}


static void
write_dentry(FILE *outfp, const char *outname, struct xrscandata *xsd)
{
	write_dentry_onedir(outfp, outname, xsd, xsd->xsd_rootdir);
	/* add padding if required */
	if (xsd->xsd_header.xh_dname_shift > XNDIRENT_SHIFT_SIZE) {
		uint8_t fragunit;
		xstorsize_t fragmask, fragsize;
		xstorsize_t padsize;

		fragunit = xsd->xsd_header.xh_dname_shift
			- XNDIRENT_SHIFT_SIZE;
		assert(fragunit < sizeof(xstorsize_t) * CHAR_BIT);
		fragmask = (1ULL << (fragunit)) - 1ULL;

		fragsize = xsd->xsd_ndentry_unit & fragmask;
		trace(3, "wrdir: %" PRIuMAX " bytes / align 2^%u\n",
		      (uintmax_t)fragsize << XNDIRENT_SHIFT_SIZE,
		      xsd->xsd_header.xh_dname_shift);
		padsize = add_padding(outfp, outname,
				      fragsize << XNDIRENT_SHIFT_SIZE,
				      xsd->xsd_header.xh_dname_shift);
		xsd->xsd_dentry_padsize += padsize;
	}
}



static void
write_dname_onedir(FILE *outfp, const char *outname,
		   struct xrscandata *xsd, struct ndirinfo *ni)
{
	struct ndirent *nd;
	struct xrammnode *xmn;
	size_t namelen;
	xstorsize_t padsize;

	/* output dirname in target directory */
	for (nd = ni->ni_entry; nd != NULL; nd = nd->nd_next) {
		namelen = strlen(nd->nd_name);
		if (namelen == 0)
			continue;
		write_data(outfp, outname, nd->nd_name, namelen,
			   "directory packed name");
		padsize = add_padding(outfp, outname, namelen,
				      xsd->xsd_header.xh_dname_shift);
		xsd->xsd_dname_padsize += padsize;
	}

	/* recurse subdirectories */
	for (nd = ni->ni_entry; nd != NULL; nd = nd->nd_next) {
		xmn = &xsd->xsd_node[nd->nd_xno];
		if ((xmn->xmn_typemode & XIFMT) == XIFDIR)
			write_dname_onedir(outfp, outname, xsd,
					   nd->nd_content);
	}
}


static void
write_dname(FILE *outfp, const char *outname, struct xrscandata *xsd)
{
	xstorsize_t padsize;

	/* we have directory names, so write them */
	if (xsd->xsd_header.xh_off_dnamergn > 0)
		write_dname_onedir(outfp, outname, xsd, xsd->xsd_rootdir);

	/*
	 * add padding if required
	 * (this check cannot be skipped, this padding is not only for
	 * directory names, also for directory entries)
	 */
	if (xsd->xsd_header.xh_pageblk_shift
	    > xsd->xsd_header.xh_dname_shift) {
		xstorsize_t fragmask, fragsize;
		int fragunit;

		fragunit = xsd->xsd_header.xh_pageblk_shift
			- xsd->xsd_header.xh_dname_shift;
		assert(fragunit < sizeof(xstorsize_t) * CHAR_BIT);
		fragmask = (1ULL << (fragunit)) - 1ULL;

		fragsize = (xsd->xsd_ndentry_dnunit + xsd->xsd_ndname_unit)
			& fragmask;

		trace(3, "wrdname: %" PRIuMAX " bytes / align 2^%u\n",
		      (uintmax_t)fragsize << xsd->xsd_header.xh_dname_shift,
		      xsd->xsd_header.xh_pageblk_shift);

		padsize = add_padding(
			outfp, outname,
			fragsize << xsd->xsd_header.xh_dname_shift,
			xsd->xsd_header.xh_pageblk_shift);
		xsd->xsd_dname_padsize += padsize;
	}
}



static void
write_symlink_onedir(FILE *outfp, const char *outname,
		     struct xrscandata *xsd, struct ndirinfo *ni)
{
	struct ndirent *nd;
	struct xrammnode *xmn;
	size_t namelen;
	xstorsize_t padsize;

	/* output symbolic links in target directory */
	for (nd = ni->ni_entry; nd != NULL; nd = nd->nd_next) {
		if (nd->nd_symtgt == NULL)
			continue;
		namelen = strlen(nd->nd_symtgt);
		if (namelen == 0)
			continue;
		write_data(outfp, outname, nd->nd_symtgt, namelen,
			   "symbolic link target");
		padsize = add_padding(outfp, outname, namelen,
				      xsd->xsd_header.xh_symlink_shift);
		xsd->xsd_sym_padsize += padsize;
	}

	/* recurse subdirectories */
	for (nd = ni->ni_entry; nd != NULL; nd = nd->nd_next) {
		xmn = &xsd->xsd_node[nd->nd_xno];
		if ((xmn->xmn_typemode & XIFMT) == XIFDIR)
			write_symlink_onedir(outfp, outname, xsd,
					     nd->nd_content);
	}
}


static void
write_symlinks(FILE *outfp, const char *outname, struct xrscandata *xsd)
{
	/* if we have no symbolic link, skip this function */
	if (xsd->xsd_header.xh_off_symlink == 0)
		return;
	/* write symlink target block */
	write_symlink_onedir(outfp, outname, xsd, xsd->xsd_rootdir);
	/* add padding if required */
	if (xsd->xsd_header.xh_pageblk_shift
	    > xsd->xsd_header.xh_symlink_shift) {
		xstorsize_t fragmask, fragsize;
		int fragunit;
		xstorsize_t padsize;

		fragunit = xsd->xsd_header.xh_pageblk_shift
			- xsd->xsd_header.xh_symlink_shift;
		assert(fragunit < sizeof(xstorsize_t) * CHAR_BIT);
		fragmask = (1ULL << (fragunit)) - 1ULL;

		fragsize = xsd->xsd_nsym_unit & fragmask;

		trace(3, "wrsym: %" PRIuMAX " bytes / align 2^%u\n",
		      (uintmax_t)fragsize << xsd->xsd_header.xh_symlink_shift,
		      xsd->xsd_header.xh_pageblk_shift);
		padsize = add_padding(
			outfp, outname,
			fragsize << xsd->xsd_header.xh_symlink_shift,
			xsd->xsd_header.xh_pageblk_shift);
		xsd->xsd_sym_padsize += padsize;
	}
}

#define FAILEDMSGPFX "%s: failed to build an image: "

static xstorsize_t
copy_one_file(FILE *outfp, const char *outname, FILE *infp,
	      const char *innameheading, const char *inname,
	      xstorsize_t assumed_size)
{
	size_t readlen, wrotelen;
	xstorsize_t filesize = 0;

	while ((readlen = fread(blkbuf, 1, sizeof(blkbuf), infp)) > 0) {
		wrotelen = fwrite(blkbuf, 1, readlen, outfp);
		if (wrotelen != readlen)
			die(EXIT_FAILURE,
			    (ferror(outfp) != 0)
			    ? "%s: failed to write"
			    : "%s: failed to write: unknown error\n", outname);
		filesize += (xstorsize_t)readlen;
		if (filesize > assumed_size) {
			die(EXIT_FAILURE, FAILEDMSGPFX
			    "%s%s: file size changed: assumed %"
			    PRIuMAX " byte(s) but already %" PRIuMAX
			    " byte(s) read\n", outname, innameheading, inname,
			    (uintmax_t)assumed_size, (uintmax_t)filesize);
		}
	}
	if(ferror(infp) != 0)
		die(EXIT_FAILURE, FAILEDMSGPFX
		    "%s%s: failed to read", outname, innameheading,
		    inname);
	return (filesize);
}

static void
format_datetime_ifany(const time_t *timevalp, char *dst, size_t dstsize)
{
	struct tm *tm;

	tm = localtime(timevalp);
	if (tm == NULL || strftime(dst, dstsize, "%Y-%m-%d %T %Z", tm) == 0)
		snprintf(dst, dstsize, "[%" PRIu32 "]",
			 (uint32_t)*timevalp);
}

static void
copy_file_onedir(FILE *outfp, const char *outname, struct xrscandata *xsd,
		 struct ndirinfo *ni, const char *dirname, const char *heading)
{
	struct ndirent *nd;
	struct xrammnode *xmn;
	FILE *fp;
	xstorsize_t filesize, padsize;
	size_t pathlen;
	char pathbuf[MKX_MAXPATHLEN], *namep;
	struct stat st;

	pathlen = strlen(dirname);
	assert(pathlen > 0);
	memcpy(pathbuf, dirname, pathlen);
	namep = &pathbuf[pathlen + 1];
	namep[-1] = '/';
	namep[0] = '\0'; /* for safe */

	/* first, copy all file in target directory */
	for (nd = ni->ni_entry; nd != NULL; nd = nd->nd_next) {
		/*
		 * if target is not a mappable file,
		 * or not a first found target (hard-linked file), skip it
		 */
		xmn = &xsd->xsd_node[nd->nd_xno];
		if ((xmn->xmn_typemode & XIFMT) != XIFMAP
		    || nd->nd_isfirst == 0)
			continue;

		strcpy(namep, nd->nd_name);
		trace(1, "copying %s%s\n", heading, &pathbuf[1]);
		fp = fopen(pathbuf, "rb");
		if (fp == NULL)
			die(EXIT_FAILURE, FAILEDMSGPFX "%s%s: failed to open",
			    outname, heading, &pathbuf[1]);
		filesize = copy_one_file(outfp, outname, fp, heading,
					 &pathbuf[1], xmn->xmn_size);
		if (fstat(fileno(fp), &st) < 0)
			die(EXIT_FAILURE, FAILEDMSGPFX "%s%s: failed to stat",
			    outname, heading, &pathbuf[1]);
		(void)fclose(fp);

		/* sanity checks */
		if (st.st_mtime != nd->nd_mtime) {
			char exptm[64], realtm[64];

			format_datetime_ifany(&nd->nd_mtime, exptm,
					      sizeof(exptm));
			format_datetime_ifany(&st.st_mtime, realtm,
					      sizeof(realtm));

			die(EXIT_FAILURE, FAILEDMSGPFX
			    "%s%s: file may be modified since directory "
			    "scanning\n    last modified time is changed: "
			    "recorded '%s'\n                                  "
			    "  but now '%s'\n",
			    outname, heading, &pathbuf[1], exptm, realtm);
		}
		if (st.st_ctime != nd->nd_ctime) {
			char exptm[64], realtm[64];

			format_datetime_ifany(&nd->nd_ctime, exptm,
					      sizeof(exptm));
			format_datetime_ifany(&st.st_ctime, realtm,
					      sizeof(realtm));

			die(EXIT_FAILURE, FAILEDMSGPFX
			    "%s%s: file may be modified since directory "
			    "scanning\n    inode change time is changed: "
			    "recorded '%s'\n                                 "
			    "  but now '%s'\n",
			    outname, heading, &pathbuf[1], exptm, realtm);
		}
		if (filesize != xmn->xmn_size)
			die(EXIT_FAILURE, FAILEDMSGPFX
			    "%s%s: %" PRIu64 " bytes read, "
			    "but recorded %" PRIu64 " bytes at header "
			    "(xno=%" PRIu64 ")\n", outname,
			    heading, &pathbuf[1], (uint64_t)filesize,
			    (uint64_t)xmn->xmn_size, (uint64_t)nd->nd_xno);
		padsize = add_padding(outfp, outname, filesize,
				      xsd->xsd_header.xh_pageblk_shift);

		xsd->xsd_map_padsize += padsize;
	}

	/* next, recurse subdirectories */
	for (nd = ni->ni_entry; nd != NULL; nd = nd->nd_next) {
		/* if target is not a directory, skip it */
		xmn = &xsd->xsd_node[nd->nd_xno];
		if ((xmn->xmn_typemode & XIFMT) != XIFDIR)
			continue;
		strcpy(namep, nd->nd_name);
		copy_file_onedir(outfp, outname, xsd,
				 nd->nd_content, pathbuf, heading);
	}
}






static void
output_stats(struct xrscandata *xsd)
{
	xstorsize_t total_size, header_padsize, total_padsize;
	xstorsize_t size_in_blk;
	FILE *outfp = stderr;

	/* heading */
	fputs("Type      |Align     |Num.Entry |"
	      "Actual    |Pad       |Total     |Pad Ratio\n"
	      "----------+----------+----------+"
	      "----------+----------+----------+----------\n", outfp);

	/* header + node information */
	convert_unit(&size_in_blk,
		     ((uintmax_t)xsd->xsd_last_assigned + 1ULL),
		     XRNODE_SHIFT_SIZE,
		     xsd->xsd_header.xh_pageblk_shift);
	total_size = (xstorsize_t)size_in_blk
		<< xsd->xsd_header.xh_pageblk_shift;
	header_padsize = total_size -
		(((xstorsize_t)xsd->xsd_last_assigned + 1ULL)
		 << XRNODE_SHIFT_SIZE);
	fprintf(outfp,
		"%-10s %10" PRIuMAX " %10" PRIuMAX " %10" PRIuMAX
		" %10" PRIuMAX " %10" PRIuMAX " ",
		"Header", 1ULL << XRNODE_SHIFT_SIZE,
		(uintmax_t)xsd->xsd_header.xh_files,
		(uintmax_t)(total_size - header_padsize),
		(uintmax_t)header_padsize, (uintmax_t)total_size);
	if (total_size > 0)
		fprintf(outfp, "%9.5f%%\n",
			(double)(header_padsize) * 100.0 / (double)total_size);
	else
		fputs("    --\n", outfp);

	/* directory blocks */
	convert_unit(&size_in_blk,
		     xsd->xsd_ndentry_dnunit + xsd->xsd_ndname_unit,
		     xsd->xsd_header.xh_dname_shift,
		     xsd->xsd_header.xh_pageblk_shift);
	total_size = (xstorsize_t)size_in_blk
		<< xsd->xsd_header.xh_pageblk_shift;
	fprintf(outfp,
		"%-10s     --         --     %10" PRIuMAX " %10" PRIuMAX
		" %10" PRIuMAX " ", "Directory",
		(uintmax_t)(total_size - xsd->xsd_dentry_padsize
			    - xsd->xsd_dname_padsize),
		(uintmax_t)(xsd->xsd_dentry_padsize + xsd->xsd_dname_padsize),
		(uintmax_t)total_size);
	if (total_size > 0)
		fprintf(outfp, "%9.5f%%\n",
			(double)(xsd->xsd_dentry_padsize
				 + xsd->xsd_dname_padsize)
			* 100.0 / (double)total_size);
	else
		fputs("    --\n", outfp);

	/* directory entry region */
	total_size = (xstorsize_t)xsd->xsd_ndentry_dnunit
		<< xsd->xsd_header.xh_dname_shift;
	fprintf(outfp, "%-10s %10" PRIuMAX " %10" PRIuMAX " %10" PRIuMAX
		" %10" PRIuMAX " %10" PRIuMAX "     --\n",
		"  Entry", (uintmax_t)1ULL << XNDIRENT_SHIFT_SIZE,
		(uintmax_t)xsd->xsd_ndirs,
		(uintmax_t)(total_size - xsd->xsd_dentry_padsize),
		(uintmax_t)xsd->xsd_dentry_padsize, (uintmax_t)total_size);

	/* directory name region */
	total_size = (size_in_blk
		       << xsd->xsd_header.xh_pageblk_shift)
		   - ((xstorsize_t)xsd->xsd_ndentry_dnunit
		       << xsd->xsd_header.xh_dname_shift);
	fprintf(outfp, "%-10s %10" PRIuMAX " %10" PRIuMAX " %10" PRIuMAX
		" %10" PRIuMAX " %10" PRIuMAX "     --\n", "  Name",
		1ULL << xsd->xsd_header.xh_dname_shift,
		(uintmax_t)xsd->xsd_nnodes,
		(uintmax_t)(total_size - xsd->xsd_dname_padsize),
		(uintmax_t)xsd->xsd_dname_padsize,
		(uintmax_t)total_size);

	/* symbolic link target block */
	convert_unit(&size_in_blk, xsd->xsd_nsym_unit,
		     xsd->xsd_header.xh_symlink_shift,
		     xsd->xsd_header.xh_pageblk_shift);
	total_size = (xstorsize_t)size_in_blk
		<< xsd->xsd_header.xh_pageblk_shift;
	fprintf(outfp,
		"%-10s %10" PRIuMAX " %10" PRIuMAX " %10" PRIuMAX
		" %10" PRIuMAX " %10" PRIuMAX " ", "SymLink",
		1ULL << xsd->xsd_header.xh_symlink_shift,
		(uintmax_t)xsd->xsd_nsymlinks,
		(uintmax_t)(total_size - xsd->xsd_sym_padsize),
		(uintmax_t)xsd->xsd_sym_padsize, (uintmax_t)total_size);
	if (total_size > 0)
		fprintf(outfp, "%9.5f%%\n",
			(double)(xsd->xsd_sym_padsize)
			* 100.0 / (double)total_size);
	else
		fputs("    --\n", outfp);

	/* packed file contents block */
	convert_unit(&size_in_blk, xsd->xsd_npack_unit,
		     xsd->xsd_header.xh_pfile_shift,
		     xsd->xsd_header.xh_pageblk_shift);
	total_size = (xstorsize_t)size_in_blk
		<< xsd->xsd_header.xh_pageblk_shift;
	fprintf(outfp, "%-10s %10" PRIuMAX " %10" PRIuMAX " %10" PRIuMAX
		" %10" PRIuMAX " %10" PRIuMAX " ", "Packed",
		1ULL << xsd->xsd_header.xh_pfile_shift,
		(uintmax_t)xsd->xsd_npackfiles,
		total_size - (uintmax_t)xsd->xsd_pack_padsize,
		(uintmax_t)xsd->xsd_pack_padsize,
		total_size);
	if (total_size > 0)
		fprintf(outfp, "%9.5f%%\n",
			(double)(xsd->xsd_pack_padsize)
			* 100.0 / (double)total_size);
	else
		fputs("    --\n", outfp);

	/* mappable file contents block */
	total_size = (uintmax_t)xsd->xsd_nmap_unit
		<< xsd->xsd_header.xh_pageblk_shift;
	fprintf(outfp, "%-10s %10" PRIuMAX " %10" PRIuMAX " %10" PRIuMAX
		" %10" PRIuMAX " %10" PRIuMAX " ", "Mappable",
		1ULL << xsd->xsd_header.xh_pageblk_shift,
		(uintmax_t)xsd->xsd_nmapfiles,
		(uintmax_t)(total_size - xsd->xsd_map_padsize),
		(uintmax_t)xsd->xsd_map_padsize,
		(uintmax_t)total_size);
	if (total_size > 0)
		fprintf(outfp, "%9.5f%%\n",
			(double)(xsd->xsd_map_padsize)
			* 100.0 / (double)total_size);
	else
		fputs("    --\n", outfp);

	/* total */
	fputs("----------+----------+----------+"
	      "----------+----------+----------+----------\n", outfp);

	total_size = (xstorsize_t)xsd->xsd_header.xh_blocks
		<< xsd->xsd_header.xh_pageblk_shift;
	total_padsize = xsd->xsd_dentry_padsize
		+ xsd->xsd_dname_padsize
		+ xsd->xsd_sym_padsize
		+ xsd->xsd_pack_padsize
		+ xsd->xsd_map_padsize
		+ header_padsize;
	fprintf(outfp,
		"%-10s     --         --     %10" PRIuMAX " %10" PRIuMAX
		" %10" PRIuMAX " ", "Total",
		(uintmax_t)(total_size - total_padsize),
		(uintmax_t)total_padsize, (uintmax_t)total_size);
	if (total_size > 0)
		fprintf(outfp, "%9.5f%%\n",
			(double)(total_padsize) * 100.0 / (double)total_size);
	else
		fputs("    --\n", outfp);
}



static void
build_image(const char *srcdir, const char *dstfile, struct xrscandata *xsd)
{
	FILE *fp;
	char *srcdir_real = strdup(srcdir);
	char *default_name = "(stdout)";
	struct stat st;
	size_t srcdir_len;

	if (srcdir_real == NULL)
		die(EXIT_FAILURE, "out of memory\n");

	/* remove trailing slashs (except "/", it is root directory) */
	for (srcdir_len = strlen(srcdir_real);
	     srcdir_len > 1 && srcdir_real[srcdir_len - 1] == '/';
	     srcdir_len--)
		srcdir_real[srcdir_len - 1] = '\0';

	if (dstfile != NULL) {
		fp = fopen(dstfile, "wb");
	} else {
		dstfile = default_name;
		/* open stdout with Write + Binary */
		fp = fdopen(fileno(stdout), "wb");
	}
	if (fp == NULL)
		die(EXIT_FAILURE, "%s: failed to open for output",
		     dstfile);
	if (fstat(fileno(fp), &st) < 0)
		die(EXIT_FAILURE, "%s: failed to stat", dstfile);
	if (S_ISREG(st.st_mode)) {
		xsd->xsd_dstdev = st.st_dev;
		xsd->xsd_dstino = st.st_ino;
	} else
		xsd->xsd_flags |= SCANF_IGNORE_DSTCHECK;
	if (chdir(srcdir_real) < 0)
		die(EXIT_FAILURE, "%s: failed to chdir", srcdir_real);

	run_scanner(xsd, srcdir_real);

	decide_position_onedir(xsd, xsd->xsd_rootdir, ".", srcdir);
	fixup_position_header(xsd);

	write_header_and_nodes(fp, dstfile, xsd);
	write_dentry(fp, dstfile, xsd);
	write_dname(fp, dstfile, xsd);
	write_symlinks(fp, dstfile, xsd);

	copy_file_onedir(fp, dstfile, xsd, xsd->xsd_rootdir, ".", srcdir_real);

	trace(1, "total %" PRIuMAX " blocks\n",
	      (uintmax_t)xsd->xsd_header.xh_blocks);
	if (fclose(fp) == EOF)
		die(EXIT_FAILURE, "%s: failed to close correctly", dstfile);
	if (xsd->xsd_flags & SCANOPT_OUT_STATS)
		output_stats(xsd);
}


/*
 *
 *  bootstrap and options
 *
 */


static void __NORETURN
usage(void)
{
	fputs("Usage: mkxramfs [-Lsv] [-N maxnodes] [-o image] "
	      "[-D unit] [-S unit] [-P unit] [-B unit] [-d type] "
	      "rootdir\n", stderr);
	exit(EXIT_FAILURE);
}



static void
check_and_set_unit(uint8_t *setto, const char *name, const char *arg)
{
	unsigned long val;
	char *p;

	errno = 0;
	val = strtoul(arg, &p, 0);
	if (p == NULL || *p != '\0' || arg == p)
		die(EXIT_FAILURE, "argument \"%s\" is not a valid number\n",
		    arg);
	if ((val == ULONG_MAX && errno == ERANGE)
	    || val > 0xffU /* == UINT8_MAX */)
		die(EXIT_FAILURE, "%s \"%s\" is out of range\n", name, arg);
	if (val >= sizeof(xstorsize_t) * CHAR_BIT)
		die(EXIT_FAILURE,
		    "%s \"%lu\" is too big to handle (maximum size: %"
		    PRIuMAX ")\n", name, val,
		    (uintmax_t)(sizeof(xstorsize_t) * CHAR_BIT - 1U));
	*setto = val;
	trace(1, "config: %s -> %lu\n", name, val);
}



static int
str2value(const char *str, const struct strdesc *sd)
{
	if (str == NULL)
		return (-1);
	for (; sd->sd_name != NULL; sd++) {
		if (strcmp(sd->sd_name, str) == 0)
			return (sd->sd_value);
	}
	return (-1);
}



int
main(int argc, char *argv[])
{
	int optchar, type;
	unsigned long val;
	char *dstfile = NULL;
	char *p;
	struct xrscandata xsd;
	static const struct strdesc dirtype[] = {
		{"linear", 0},
		{"sort", 1},
		{NULL, -1}
	};

	getpname(argv[0]);
	initscandata(&xsd);
	while ((optchar = getopt(argc, argv, "Lsvd:N:D:S:P:B:o:")) != -1) {
		switch (optchar) {
		case 'v':
			settracelevel(gettracelevel() + 1);
			break;
		case 'L':
			xsd.xsd_flags |= SCANOPT_NOSYMLINKS;
			break;
		case 'd':
			type = str2value(optarg, dirtype);
			if (type < 0)
				die(EXIT_FAILURE,
				    "argument \"%s\" is not known "
				    "directory type\n", optarg);
			if (type == 0)
				xsd.xsd_flags |= SCANOPT_DONT_SORT_DIR;
			else /* type == 1 */
				xsd.xsd_flags &= ~SCANOPT_DONT_SORT_DIR;
			break;
		case 'N':
			if (xsd.xsd_flags & SCANOPT_FIX_MAXNODES)
				die(EXIT_FAILURE,
				    "duplicate 'N' option specified\n");
			errno = 0;
			val = strtoul(optarg, &p, 0);
			if (p == NULL || *p != '\0' || optarg == p)
				die(EXIT_FAILURE,
				    "argument \"%s\" is not a valid number\n",
				    optarg);
			/*
			 * Currently, we don't need minimum check:
			 * -N 0 means 'unlimited' and minimum required number
			 * is 1.
			 */
			if ((errno == ERANGE && val == ULONG_MAX)
			    || val > XRAM_XNO_MAX)
				die(EXIT_FAILURE,
				    "maxnodes \"%s\" is out of range\n",
				    optarg);
			if (val == 0)
				/* remove limitation */
				xsd.xsd_flags &= ~SCANOPT_FIX_MAXNODES;
			else {
				xsd.xsd_flags |= SCANOPT_FIX_MAXNODES;
				/*
				 * put to real storage
				 * (long may be 64bit value)
				 */
				xsd.xsd_node_max = val;
				/*
				 * ... and checking overflow;
				 *
				 * we don't need check xsd.xsd_node_max is
				 * equal to val - val is already checked
				 * '32bit value' in above 'out-of-range'
				 * check.
				 */
				if (ADD_OVERFLOW_SIMPLE(xsd.xsd_node_max,
							   XNO_ROOT))
					die(EXIT_FAILURE,
					    "we cannot handle value %" PRIuMAX
					    " due to integer overflow\n",
					    (uintmax_t)val);
				/* first 2 entries are reserved for header */
				xsd.xsd_node_max += XNO_ROOT;
			}
			break;
		case 'D':
			check_and_set_unit(
				&xsd.xsd_header.xh_dname_shift,
				"directory entry name unit size", optarg);
			break;
		case 'S':
			check_and_set_unit(
				&xsd.xsd_header.xh_symlink_shift,
				"symbolic link target unit size", optarg);
			break;
		case 'P':
			check_and_set_unit(
				&xsd.xsd_header.xh_pfile_shift,
				"packed file contents unit size", optarg);
			break;
		case 'B':
			check_and_set_unit(
				&xsd.xsd_header.xh_pageblk_shift,
				"block size", optarg);
			break;
		case 's':
			xsd.xsd_flags |= SCANOPT_OUT_STATS;
			break;
		case 'o':
			if (optarg == NULL || *optarg == '\0')
				die(EXIT_FAILURE,
				    "empty destination file name specified\n");
			dstfile = optarg;
			break;
		case '?':
		case ':':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	/* check unit sizes */
	if (xsd.xsd_header.xh_dname_shift > xsd.xsd_header.xh_pageblk_shift)
		die(EXIT_FAILURE,
		    "directory entry name unit size (%" PRIu8 ") must be "
		    "less than or equal to block size (%" PRIu8 ")\n",
		    xsd.xsd_header.xh_dname_shift,
		    xsd.xsd_header.xh_pageblk_shift);

	if (xsd.xsd_header.xh_symlink_shift > xsd.xsd_header.xh_pageblk_shift)
		die(EXIT_FAILURE,
		    "symbolic link target unit size (%" PRIu8 ") must be "
		    "less than or equal to block size (%" PRIu8 ")\n",
		    xsd.xsd_header.xh_symlink_shift,
		    xsd.xsd_header.xh_pageblk_shift);

	if (xsd.xsd_header.xh_pfile_shift > xsd.xsd_header.xh_pageblk_shift)
		die(EXIT_FAILURE,
		    "packed file contents unit size (%" PRIu8 ") must be "
		    "less than or equal to block size (%" PRIu8 ")\n",
		    xsd.xsd_header.xh_pfile_shift,
		    xsd.xsd_header.xh_pageblk_shift);
	if (argc != 1)
		usage();
	build_image(argv[0], dstfile, &xsd);
	return (EXIT_SUCCESS);
}
