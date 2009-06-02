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
 *  FS image dumper.
 */
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/fs/xnode.h>
#include <sys/vnode.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

int strictmode = 0;
int isdevice = 0;
uint64_t filesize;
struct xramheader xh;
struct xrammnode *xm;

#define SUPPORT_MAX_SHIFT 31
#define SUPPORT_PAGE_SHIFT 12

#define XIFDNAMER   0130000
#define XIFEOF      0177777

typedef struct xrammnode_sort {
	struct xrammnode *xm;
	xram_xno_t xno;
} *xrammnode_sort_t;

void
errprintf(char *format, ...)
{
	va_list alist;
	(void) fprintf(stderr, "*** [Error]: ");
	va_start(alist, format);
	(void) vfprintf(stderr, format, alist);
	va_end(alist);
}

void
usage(void)
{
	(void) fprintf(stderr, "Usage: dumpxramfsimage [-D] [-C] [-s] image\n");
	exit(1);
}

char *
xrammnode_typestr(int type)
{
	switch (type) {
	case XIFIFO:
		return ("fifo");
	case XIFCHR:
		return ("chrdev");
	case XIFDIR:
		return ("directory");
	case XIFBLK:
		return ("blkdev");
	case XIFPACK:
		return ("packed");
	case XIFMAP:
		return ("map");
	case XIFLNK:
		return ("symlink");
	}
	return ("UNKNOWN");
}

char *
convert_time(time_t *t)
{
	struct tm *tm;
	static char tbuf[1024];
	size_t bytes;

	tm = localtime(t);
	bytes = strftime(tbuf, sizeof(tbuf) - 1, NULL, tm);
	if (bytes == 0) {
		/* this requires less than 10 bytes */
		(void) snprintf(tbuf, sizeof(tbuf), "[%u]", (uint32_t)*t);
	}

	return (tbuf);
}

int
image_read(int fd, char *buf, unsigned int len)
{
	int ret;

	ret = read(fd, buf, len);
	if (ret == -1) {
		perror("read");
		return (-1);
	}
	if (ret != len) {
		errprintf("read %dbytes. (assumed %dbytes)\n", ret, len);
		return (-1);
	}
	return (0);
}

int
dump_xramheader(int fd)
{
	int ret, i;
	time_t create_time;

	create_time = xh.xh_ctime;

	printf("XRAMHEADER\n");
	printf("         magic: 0x%x\n", xh.xh_magic);
	printf("       version: %d\n", xh.xh_version);
	printf("         ctime: %u (%s)\n", xh.xh_ctime,
	       convert_time(&create_time));
	printf("        blocks: %u\n", xh.xh_blocks);
	printf("         files: %u\n", xh.xh_files);
	printf("   dname usize: 2^%d (%u)\n", xh.xh_dname_shift,
	       1 << xh.xh_dname_shift);
	printf(" symlink usize: 2^%d (%u)\n", xh.xh_symlink_shift,
	       1 << xh.xh_symlink_shift);
	printf("packfile usize: 2^%d (%u)\n", xh.xh_pfile_shift,
	       1 << xh.xh_pfile_shift);
	printf("    block size: 2^%d (%u)\n", xh.xh_pageblk_shift,
	       1 << xh.xh_pageblk_shift);
	printf("       dir off: %u\n", xh.xh_off_dir);
	printf("   symlink off: %u\n", xh.xh_off_symlink);
	printf("     pfile off: %u\n", xh.xh_off_pfile);
	printf("     xfile off: %u\n", xh.xh_off_xfile);
	printf("ext header off: %u\n", xh.xh_extheader);
	printf("   dirname off: %u\n", xh.xh_off_dnamergn);

	printf("      reserved: ");
	for (i = 0; i < sizeof(xh.xh_reserved) / 2; i ++)
		printf("0x%02x ", (uint8_t)xh.xh_reserved[i]);
	printf("\n");
	printf("                ");
	for (; i < sizeof(xh.xh_reserved); i ++)
		printf("0x%02x ", (uint8_t)xh.xh_reserved[i]);
	printf("\n");

	printf("\n");
	return (0);
}

void *
dumpxramfsimage_malloc(size_t size, uint32_t num)
{
	size_t bytes;
	void *ret;

	if (size == 0 || num == 0) {
		fprintf(stderr, "malloc size is zero");
		return (NULL);
	}

	bytes = size * num;
	if (bytes / num != size) {
		fprintf(stderr, "malloc size over");
		return (NULL);
	}

	ret = malloc(bytes);
	if (ret == NULL) {
		perror("malloc");
		return (NULL);
	}

	return (ret);
}

void *
dumpxramfsimage_realloc(void *mem, size_t size, uint32_t num)
{
	size_t bytes;
	void *ret;

	if (size == 0 || num == 0) {
		fprintf(stderr, "realloc size is zero");
		return (NULL);
	}

	bytes = size * num;
	if (bytes / num != size) {
		fprintf(stderr, "realloc size over");
		return (NULL);
	}

	ret = realloc(mem, bytes);
	if (ret == NULL) {
		perror("realloc");
		return (NULL);
	}

	return (ret);
}

int
dump_xrammnode(int fd)
{
	uint32_t xrammnode_count;
	xram_xno_t xno;
	struct xrammnode *xmp;
	int ret;
	uint16_t type;
	time_t t;
	uint8_t *un;

	printf("XRAMMNODE: num=%u\n", xh.xh_files);

	xrammnode_count = XRAMMNODE_XNO_ROOT + xh.xh_files;
	for (xno = XRAMMNODE_XNO_ROOT; xno < xrammnode_count; xno++) {
		xmp = &xm[xno];

		type = XRAMMNODE_TYPE(xmp);
		t = xmp->xmn_time_sec;

		printf("  xno: %u\n", xno);
		printf("      typemode: 0x%x type=%s mode=0%o\n",
		       xmp->xmn_typemode,
		       xrammnode_typestr(type), XRAMMNODE_MODE(xmp));
		printf("         nlink: %u\n", xmp->xmn_nlink);
		printf("           uid: %u\n", xmp->xmn_uid);
		printf("           gid: %u\n", xmp->xmn_gid);
		if (type == XIFPACK || type == XIFMAP || type == XIFLNK)
			printf("         union: startblk=%u size=%u\n",
			       xmp->xmn_start, xmp->xmn_size);
		else if (type == XIFDIR)
			printf("         union: startoff=%u size=%u\n",
			       xmp->xmn_start, xmp->xmn_size);
		else if (type == XIFCHR || type == XIFBLK)
			printf("         union: dev=0x%x:0x%x\n",
			       xmp->xmn_rdev_major, xmp->xmn_rdev_minor);
		else {
			un = (uint8_t *)&xmp->_xmn_un;
			printf("         union: %02x %02x %02x %02x "
			       "%02x %02x %02x %02x\n",
			       un[0], un[1], un[2], un[3],
			       un[4], un[5], un[6], un[7]);
		}
		printf("     time(sec): %u (%s)\n",
		       xmp->xmn_time_sec, convert_time(&t));
		printf("    time(nsec): %u\n", xmp->xmn_time_nsec);
		printf("      reserved: 0x%x\n", xmp->_xmn_reserved);
	}

	printf("\n");

	return (0);
}

char *
xramdirent_name(int fd, struct xramdirent *xde)
{
	static char name[256];
	off_t off, seekret;
	uint32_t start, length;
	int ret;

	start = XRAMDIRENT_START(xde);
	length = XRAMDIRENT_LENGTH(xde);
	off = ((off_t)start << xh.xh_dname_shift) +
	      (xh.xh_off_dir << xh.xh_pageblk_shift) +
	      (xh.xh_off_dnamergn << xh.xh_dname_shift);

	if (off + length >= filesize) {
		return (NULL);
	}

	seekret = lseek(fd, off, SEEK_SET);
	if (seekret == (off_t)-1) {
		perror("lseek");
		return (NULL);
	}

	ret = image_read(fd, name, length);
	if (ret) {
		return (NULL);
	}

	name[length] = '\0';

	return (name);
}

int
xrammnode_getdircheader(int fd, struct xrammnode *xmp,
                        struct xramdircheader *xdh)
{
	off_t diroff, seekret;
	int ret;

	diroff = ((off_t)xmp->xmn_start << XRAMDIR_ALIGN_SHIFT) +
	         (xh.xh_off_dir << xh.xh_pageblk_shift);
	seekret = lseek(fd, diroff, SEEK_SET);
	if (seekret == (off_t)-1) {
		perror("lseek");
		return (-1);
	}
	ret = image_read(fd, (char *)xdh, sizeof(*xdh));
	return (ret);
}

int
is_valid_xno(xram_xno_t xno)
{
	return ((XRAMFS_NUM_VIRTUAL_NODES <= xno) &&
	        (xno < XRAMMNODE_XNO_ROOT + xh.xh_files));
}

typedef union {
	struct {
		int offset;
	} dump;
	struct {
		int fd;
		int *error;
	} corrupt;
	struct {
		uint32_t *nlinks;
	} countup;
} xramdirent_traverse_union;

struct xramdirent_traverse_argument {
	int fd;
	xramdirent_traverse_union cdata;
	int (*prepare)(struct xramdirent_traverse_argument *,
	               struct xramdircheader *,
	               struct xramdirent **, void **);
	int (*doit)(struct xramdirent_traverse_argument *,
	            void *, xram_xno_t, struct xramdirent *);
	void (*cleanup)(struct xramdirent_traverse_argument *,
	                struct xramdirent *, void *);
	uint32_t size_of_array;
	uint32_t num_of_array;
	xram_xno_t *xno_array;
};

int
xramdirent_traverse_xno_exists(struct xramdirent_traverse_argument *arg,
                               xram_xno_t cxno)
{
	uint32_t i;

	for (i = 0; i < arg->num_of_array; i++) {
		if (cxno == arg->xno_array[i]) {
			return (1);
		}
	}
	return (0);
}

int
xramdirent_traverse(struct xramdirent_traverse_argument *arg,
                    xram_xno_t caller_xno, xram_xno_t xno)
{
	struct xrammnode *xmp;
	struct xramdircheader xdh;
	int ret;
	struct xramdirent *xde;
	void *opt = 0;
	uint32_t i;
	xram_xno_t cxno;

	if (!is_valid_xno(xno)) {
		errprintf("invalid xno");
		return (-1);
	}
	xmp = &xm[xno];

	/* get common header */
	if (xrammnode_getdircheader(arg->fd, xmp, &xdh) != 0) {
		return (-1);
	}

	/* prepare */
	ret = arg->prepare(arg, &xdh, &xde, &opt);
	if (ret != 0) {
		return (ret);
	}

	/* for loop-detect */
	if (arg->size_of_array <= arg->num_of_array) {
		arg->size_of_array += 4096;
		arg->xno_array = dumpxramfsimage_realloc(arg->xno_array,
		                                         sizeof(xram_xno_t),
		                                         arg->size_of_array);
		if (arg->xno_array == NULL) {
			arg->cleanup(arg, xde, opt);
			return (-1);
		}
	}
	arg->xno_array[arg->num_of_array++] = xno;

	/* children */
	for (i = 0; i < xdh.xdh_nent; i++) {
		/* doit */
		ret = arg->doit(arg, opt, xdh.xdh_xnodot, &xde[i]);
		if (ret != 0) {
			arg->cleanup(arg, xde, opt);
			return (ret);
		}

		cxno = xde[i].xde_xno;
		if (!is_valid_xno(cxno)) {
			continue;
		}
		/* detect loop */
		if (xramdirent_traverse_xno_exists(arg, cxno) != 0) {
			errprintf("(loop detected xno=%u)", cxno);
			continue;
		}
		/* traverse */
		if (XRAMMNODE_TYPE(&xm[cxno]) != XIFDIR) {
			continue;
		}
		ret = xramdirent_traverse(arg, xdh.xdh_xnodot, cxno);
		if (ret != 0) {
			arg->cleanup(arg, xde, opt);
			return (ret);
		}
	}

	arg->cleanup(arg, xde, opt);

	return (0);
}

int
xramdirent_common_prepare_nocheck(struct xramdirent_traverse_argument *arg,
				  struct xramdircheader *xdh,
				  struct xramdirent **xde, void **opt)
{
	uint32_t numentries;
	int ret;

	/* valid type */
	if (xdh->xdh_type != XRAM_DTYPE_LINEAR &&
	    xdh->xdh_type != XRAM_DTYPE_SORTED) {
		errprintf("invalid xdh_type. type=%d\n", xdh->xdh_type);
		return (-1);
	}

	/* no entry? */
	numentries = xdh->xdh_nent;
	if (numentries == 0) {
		*xde = NULL;
		*opt = NULL;
		return (0);
	}

	/* malloc */
	*xde = dumpxramfsimage_malloc(sizeof(struct xramdirent), numentries);
	if (*xde == NULL) {
		return (-1);
	}

	/* read entries */
	ret = image_read(arg->fd, (char *)*xde,
	                 sizeof(struct xramdirent) * numentries);
	if (ret != 0) {
		free(*xde);
		return (ret);
	}

	return (0);
}

int
xramdirent_common_prepare(struct xramdirent_traverse_argument *arg,
                          struct xramdircheader *xdh,
                          struct xramdirent **xde, void **opt)
{
	/* valid xno? */
	if (!is_valid_xno(xdh->xdh_xnodot)) {
		errprintf("invalid xdh_xnodot");
		return (-1);
	}
	if (!is_valid_xno(xdh->xdh_xnodotdot)) {
		errprintf("invalid xdh_xnodotdot");
		return (-1);
	}

	return xramdirent_common_prepare_nocheck(arg, xdh, xde, opt);
}

int
xramdirent_null_doit(struct xramdirent_traverse_argument *arg, void *opt,
                     xram_xno_t caller_xno, struct xramdirent *xde)
{
	return (0);
}

void
xramdirent_common_cleanup(struct xramdirent_traverse_argument *arg,
                          struct xramdirent *xde, void *opt)
{
	free(xde);
}

void
xramdirent_common_argument(struct xramdirent_traverse_argument *arg, int fd)
{
	arg->fd = fd;
	arg->prepare = xramdirent_common_prepare;
	arg->doit = xramdirent_null_doit;
	arg->cleanup = xramdirent_common_cleanup;
	arg->size_of_array = 0;
	arg->num_of_array = 0;
	arg->xno_array = NULL;
}

void
xramdirent_dispose_argument(struct xramdirent_traverse_argument *arg)
{
	if (arg->xno_array != NULL) {
		free(arg->xno_array);
	}
}

int
xramdirent_dump_prepare(struct xramdirent_traverse_argument *arg,
                        struct xramdircheader *xdh,
                        struct xramdirent **xde, void **opt)
{
	int ret, offset = arg->cdata.dump.offset;
	char *offstr;

	ret = xramdirent_common_prepare_nocheck(arg, xdh, xde, opt);
	if (ret != 0) {
		return (ret);
	}

	offstr = dumpxramfsimage_malloc(1, offset + 1);
	if (offstr == NULL) {
		xramdirent_common_cleanup(arg, *xde, opt);
		return (-1);
	}
	memset(offstr, ' ', offset);
	offstr[offset] = '\0';

	*opt = offstr;

	/* dump directory common header */
	printf("%snent=%u, xnodot=%u, xnodotdot=%u, type=%u\n",
	       offstr, xdh->xdh_nent, xdh->xdh_xnodot, xdh->xdh_xnodotdot,
	       xdh->xdh_type);

	arg->cdata.dump.offset = offset + 4;

	return (0);
}

int
xramdirent_dump_doit(struct xramdirent_traverse_argument *arg, void *opt,
                     xram_xno_t caller_xno, struct xramdirent *xde)
{
	char *name;
	int ret;

	/* show name */
	name = xramdirent_name(arg->fd, xde);
	if (name == NULL) {
		name = "*INVALID_NAME*";
	}
	printf("%s [xno=%u,start=%u,length=%u,name=%s]\n",
	       (char *)opt, xde->xde_xno, XRAMDIRENT_START(xde),
	       XRAMDIRENT_LENGTH(xde), name);

	return (0);
}

void
xramdirent_dump_cleanup(struct xramdirent_traverse_argument *arg,
                        struct xramdirent *xde, void *opt)
{
	arg->cdata.dump.offset -= 4;

	free(opt);
	xramdirent_common_cleanup(arg, xde, opt);
}

int
xramdirent_corrupt_name_prepare(struct xramdirent_traverse_argument *arg,
                                struct xramdircheader *xdh,
                                struct xramdirent **xde, void **opt)
{
	int ret, *error = arg->cdata.corrupt.error;
	char **names, *name;
	uint32_t numentries = xdh->xdh_nent, i, j, name_cnt;

	/* prepare */
	ret = xramdirent_common_prepare_nocheck(arg, xdh, xde, opt);
	if (ret != 0) {
		return (ret);
	}

	if (numentries == 0) {
		return (0);
	}

	/* copy names */
	names = dumpxramfsimage_malloc(sizeof(char *), numentries);
	if (names == NULL) {
		xramdirent_common_cleanup(arg, *xde, opt);
		goto error_out;
	}
	for (i = 0, name_cnt = 0; i < numentries; i++) {
		name = xramdirent_name(arg->fd, &(*xde)[i]);
		if (name == NULL) {
			continue;
		}
		names[name_cnt] = strdup(name);
		if (names[name_cnt] == NULL) {
			perror("strdup");
			for (j = 0; j < name_cnt; j++) {
				free(names[j]);
			}
			free(names);
			goto error_out;
		}
		name_cnt++;
	}

	/* check names */
	for (i = 0; i < name_cnt; i++) {
		for (j = i + 1; j < name_cnt; j++) {
			if (strcmp(names[i], names[j]) != 0) {
				continue;
			}
			if (!*error) {
				*error = 1;
				printf("[NG] xno=");
			}
			printf("%u,", (*xde)[i].xde_xno);
		}
	}

	for (i = 0; i < name_cnt; i++) {
		free(names[i]);
	}
	free(names);

	return (0);

error_out:
	*error = 1;
	return (-1);
}

int
xramdirent_countup_prepare(struct xramdirent_traverse_argument *arg,
                           struct xramdircheader *xdh,
                           struct xramdirent **xde, void **opt)
{
	int ret;
	uint32_t *nlinks = arg->cdata.countup.nlinks;

	ret = xramdirent_common_prepare_nocheck(arg, xdh, xde, opt);
	if (ret != 0) {
		return (ret);
	}

	nlinks[xdh->xdh_xnodot    - XRAMMNODE_XNO_ROOT]++;
	nlinks[xdh->xdh_xnodotdot - XRAMMNODE_XNO_ROOT]++;

	return (0);
}

int
xramdirent_countup_doit(struct xramdirent_traverse_argument *arg, void *opt,
                        xram_xno_t caller_xno, struct xramdirent *xde)
{
	xram_xno_t cxno;
	uint32_t *nlinks = arg->cdata.countup.nlinks;
	int ret;

	cxno = xde->xde_xno;
	if (is_valid_xno(cxno)) {
		nlinks[cxno - XRAMMNODE_XNO_ROOT]++;
	}

	return (0);
}

int
dump_xramdirent_fromroot(int fd)
{
	struct xramdirent_traverse_argument arg;
	int ret;

	printf("dump XRAMDIRENT from root directory\n");
	xramdirent_common_argument(&arg, fd);
	arg.cdata.dump.offset = 2;
	arg.prepare = xramdirent_dump_prepare;
	arg.doit = xramdirent_dump_doit;
	arg.cleanup = xramdirent_dump_cleanup;
	ret = xramdirent_traverse(&arg, XRAMMNODE_XNO_ROOT, XRAMMNODE_XNO_ROOT);
	xramdirent_dispose_argument(&arg);
	printf("\n");

	return (ret);
}

xram_blkno_t
xnode_getoffsetofblock(struct xrammnode *xmp)
{
	switch (XRAMMNODE_TYPE(xmp)) {
	case XIFDIR:
		return (xh.xh_off_dir);
	case XIFPACK:
		return (xh.xh_off_pfile);
	case XIFMAP:
		return (xh.xh_off_xfile);
	case XIFLNK:
		return (xh.xh_off_symlink);
	case XIFDNAMER:
		return (xh.xh_off_dnamergn);
	default:
		return (0xffffffff);
	}
}

int
xmsort_compare_start(const void *s1, const void *s2)
{
	xrammnode_sort_t xm1 = (xrammnode_sort_t)s1;
	xrammnode_sort_t xm2 = (xrammnode_sort_t)s2;
	xram_blkno_t b1 = xnode_getoffsetofblock(xm1->xm);
	xram_blkno_t b2 = xnode_getoffsetofblock(xm2->xm);

	if (b1 == b2) {
		return (xm1->xm->xmn_start < xm2->xm->xmn_start ? -1 : 1);
	}
	return (b1 < b2 ? -1 : 1);
}

off_t
xnode_getoffsetofnode(struct xrammnode *xmp)
{
	switch (XRAMMNODE_TYPE(xmp)) {
	case XIFDIR:
		return (((off_t)xh.xh_off_dir << xh.xh_pageblk_shift) +
		        ((off_t)xmp->xmn_start << XRAMDIR_ALIGN_SHIFT));
	case XIFPACK:
		return (((off_t)xh.xh_off_pfile << xh.xh_pageblk_shift) +
		        ((off_t)xmp->xmn_start << xh.xh_pfile_shift));
	case XIFMAP:
		return (((off_t)xh.xh_off_xfile << xh.xh_pageblk_shift) +
		        ((off_t)xmp->xmn_start << xh.xh_pageblk_shift));
	case XIFLNK:
		return (((off_t)xh.xh_off_symlink << xh.xh_pageblk_shift) +
		        ((off_t)xmp->xmn_start << xh.xh_symlink_shift));
	default:
		/* unreachable */
		return ((off_t)-1);
	}
}

typedef struct xramregion_sort {
	uint16_t        type;
	xram_blkno_t    offset;
} *xramregion_sort_t;

#define XMRSET(index, tp, off) \
	(xmr[(index)].type = (tp), xmr[(index)].offset = (off))

int
xmr_compare_offset(const void *s1, const void *s2)
{
	xramregion_sort_t xm1 = (xramregion_sort_t)s1;
	xramregion_sort_t xm2 = (xramregion_sort_t)s2;

	return (xm1->offset < xm2->offset ? -1 : 1);
}

int
check_xramheader(void)
{
	uint64_t off_dir, off_dnamer, blocks;
	struct xramregion_sort xmr[6];
	uint32_t xrammnode_count;
	int error = 0, i;

	/* check magic number, endian check */
	if (xh.xh_magic != XRAM_MAGIC) {
		errprintf("invalid magic number\n");
		return (-1);
	}

	/* version */
	if (xh.xh_version <= 0 || XRAMFS_IMGVERSION < xh.xh_version) {
		errprintf("invalid version\n");
		return (-1);
	}

	/* check image size */
	if (((uint64_t)xh.xh_blocks << xh.xh_pageblk_shift) != filesize) {
		errprintf("invalid image size\n");
		return (-1);
	}

	/* num entries */
	if (xh.xh_files == 0 || xh.xh_files > XRAM_MNODE_MAX) {
		errprintf("invalid xh_files\n");
		return (-1);
	}

	/* check xxx usizes */
	if (SUPPORT_MAX_SHIFT < xh.xh_dname_shift) {
		errprintf("invalid xh_dname_shift\n");
		return (-1);
	}
	if (SUPPORT_MAX_SHIFT < xh.xh_symlink_shift) {
		errprintf("invalid xh_symlink_shift\n");
		return (-1);
	}
	if (SUPPORT_MAX_SHIFT < xh.xh_pfile_shift) {
		errprintf("invalid xh_pfile_shift\n");
		return (-1);
	}

	/* page size must be 4K */
	if (xh.xh_pageblk_shift != SUPPORT_PAGE_SHIFT) {
		errprintf("xh_pageblk_shift must be %d\n", SUPPORT_PAGE_SHIFT);
		return (-1);
	}

	/* offset */
	if (xh.xh_off_dir == 0) {
		errprintf("directory blocks not exist\n");
		return (-1);
	}
	if (xh.xh_off_dir > xh.xh_blocks) {
		errprintf("xh_off_dir too large\n");
		return (-1);
	}
	if (xh.xh_off_symlink > xh.xh_blocks) {
		errprintf("xh_off_symlink too large\n");
		return (-1);
	}
	if (xh.xh_off_pfile > xh.xh_blocks) {
		errprintf("xh_off_pfile too large\n");
		return (-1);
	}
	if (xh.xh_off_xfile > xh.xh_blocks) {
		errprintf("xh_off_xfile too large\n");
		return (-1);
	}

	if (xh.xh_extheader != 0) {
		errprintf("xh_extheader is not supported\n");
		return (-1);
	}

	off_dir = (uint64_t)xh.xh_off_dir << xh.xh_pageblk_shift;
	off_dnamer = (uint64_t)xh.xh_off_dnamergn << xh.xh_dname_shift;
	blocks = (uint64_t)xh.xh_blocks << xh.xh_pageblk_shift;
	if (off_dir + off_dnamer > blocks) {
		errprintf("xh_off_dnamergn too large\n");
		return (-1);
	}

	/* sort region */
	XMRSET(0, XIFDIR, xh.xh_off_dir);
	XMRSET(1, XIFPACK, xh.xh_off_pfile);
	XMRSET(2, XIFMAP, xh.xh_off_xfile);
	XMRSET(3, XIFLNK, xh.xh_off_symlink);
	XMRSET(4, XIFDNAMER, xh.xh_off_dnamergn);
	XMRSET(5, XIFEOF, filesize >> xh.xh_pageblk_shift);
	qsort(xmr, sizeof(xmr) / sizeof(xmr[0]), sizeof(xmr[0]),
	      xmr_compare_offset);

	/* check xnode number and files */
	printf("  check XRAMMNODE number ... ");
	fflush(stdout);
	/* -2 is for xramheader */
	xrammnode_count = ((uint64_t)xmr[0].offset << xh.xh_pageblk_shift) /
	                  sizeof(struct xrammnode) - 2;
	if (xh.xh_files > xrammnode_count) {
		error = 1;
		printf("[NG] header:%u real:%u",
		       xh.xh_files, xrammnode_count);
	} else {
		error = 0;
		printf("[OK]");
	}
	printf("\n");
	if (error) {
		return (-1);
	}

	/* reserved */
	printf("  check XRAMHEADER reserved ... ");
	fflush(stdout);
	for (i = 0; i < sizeof(xh.xh_reserved); i++) {
		if (xh.xh_reserved[i] != 0) {
			if (!error) {
				error = 1;
				printf("[NG] reserved: ");
			}
			printf("%d,", i);
		}
	}
	if (!error) {
		printf("[OK]");
	}
	printf("\n");
	if (error) {
		return (-1);
	}

	return 0;
}

int
check_rootnode(struct xrammnode *root_xramnode)
{
	int error = 0;

	/* root node check */
	printf("  check root XRAMMNODE is XIFDIR ... ");
	fflush(stdout);
	if (XRAMMNODE_TYPE(root_xramnode) != XIFDIR) {
		error = 1;
		printf("[NG] %s",
		       xrammnode_typestr(XRAMMNODE_TYPE(root_xramnode)));
	} else {
		printf("[OK]");
	}
	printf("\n");

	return (error);
}

int
check_xrammnodes(const char *str, void (*doit)(xram_xno_t xno, int *error))
{
	uint32_t i;
	xram_xno_t xno;
	int error = 0;

	printf(str);
	fflush(stdout);
	for (i = 0; i < xh.xh_files; i++) {
		xno = XRAMMNODE_XNO_ROOT + i;
		(*doit)(xno, &error);
	}
	if (!error) {
		printf("[OK]");
	}
	printf("\n");

	return (error);
}

void
check_xrammnode_name_is_in_range(int fd)
{
	uint32_t i, j;
	xram_xno_t xno;
	struct xrammnode *xmp;
	struct xramdircheader xdh;
	struct xramdirent *xde;
	off_t off, seekret;
	uint32_t start, length;
	int ret, error = 0;

	printf("  check name is in range ... ");
	fflush(stdout);
	for (i = 0; i < xh.xh_files; i++) {
		xno = XRAMMNODE_XNO_ROOT + i;
		xmp = &xm[xno];
		if (XRAMMNODE_TYPE(xmp) != XIFDIR) {
			continue;
		}
		if (xrammnode_getdircheader(fd, xmp, &xdh) != 0) {
			continue;
		}
		if (xdh.xdh_nent == 0) {
			continue;
		}
		xde = dumpxramfsimage_malloc(sizeof(*xde), xdh.xdh_nent);
		if (xde == NULL) {
			error = 1;
			printf("[NG]");
			break;
		}
		ret = image_read(fd, (char *)xde,
		                 sizeof(*xde) * xdh.xdh_nent);
		if (ret) {
			error = 1;
			printf("[NG]");
			free(xde);
			break;
		}

		/* check name */
		for (j = 0; j < xdh.xdh_nent; j++) {
			start = XRAMDIRENT_START(&xde[j]);
			length = XRAMDIRENT_LENGTH(&xde[j]);
			off = ((off_t)start << xh.xh_dname_shift) +
			    (xh.xh_off_dir << xh.xh_pageblk_shift) +
			    (xh.xh_off_dnamergn << xh.xh_dname_shift);

			if (off + length < filesize) {
			    continue;
			}
			if (!error) {
				error = 1;
				printf("[NG] name is out of range, xno=");
			}
			printf("%u,", xde[j].xde_xno);
		}
		free(xde);
	}
	if (!error) {
	    printf("[OK]");
	}
	printf("\n");
}

void
check_xrammnode_xno_is_valid(int fd)
{
	uint32_t i, j;
	xram_xno_t xno;
	struct xrammnode *xmp;
	struct xramdircheader xdh;
	struct xramdirent *xde;
	int ret, error = 0;

	printf("  check xnos ... ");
	fflush(stdout);
	for (i = 0; i < xh.xh_files; i++) {
		xno = XRAMMNODE_XNO_ROOT + i;
		xmp = &xm[xno];
		if (XRAMMNODE_TYPE(xmp) != XIFDIR) {
			continue;
		}
		if (xrammnode_getdircheader(fd, xmp, &xdh) != 0) {
			continue;
		}
		if (xdh.xdh_nent == 0) {
			continue;
		}
		xde = dumpxramfsimage_malloc(sizeof(*xde), xdh.xdh_nent);
		if (xde == NULL) {
			error = 1;
			printf("[NG]");
			break;
		}
		ret = image_read(fd, (char *)xde,
		                 sizeof(*xde) * xdh.xdh_nent);
		if (ret) {
			error = 1;
			printf("[NG]");
			free(xde);
			break;
		}

		/* check xno */
		for (j = 0; j < xdh.xdh_nent; j++) {
			if (is_valid_xno(xde[j].xde_xno)) {
			    continue;
			}
			if (!error) {
				error = 1;
				printf("[NG] xno is invalid, xno=");
			}
			printf("%u,", xde[j].xde_xno);
		}
		free(xde);
	}
	if (!error) {
	    printf("[OK]");
	}
	printf("\n");
}

void
check_xrammnode_type_is_valid(xram_xno_t xno, int *error)
{
	uint16_t type;

	type = XRAMMNODE_TYPE(&xm[xno]);
	if ((type == XIFIFO)
	    || (type == XIFCHR)
	    || (type == XIFDIR)
	    || (type == XIFBLK)
	    || (type == XIFMAP)
	    || (type == XIFLNK)) {
	    return;
	}
	if (type == XIFPACK) {
	    if (!*error) {
		*error = 1;
		printf("[NG] packed file is not supported, xno=");
	    }
	    printf("%u,", xno);
	    return;
	}
	if (!*error) {
	    *error = 1;
	    printf("[NG] type is invalid, xno=");
	}
	printf("%u,", xno);
}

void
check_xrammnode_reserved(xram_xno_t xno, int *error)
{
	if (xm[xno]._xmn_reserved == 0) {
		return;
	}

	if (!*error) {
		*error = 1;
		printf("[NG] non zero reserved. xno=");
	}
	printf("%u,", xno);
}

void
check_xrammnode_nlink(xram_xno_t xno, int *error)
{
	if (xm[xno].xmn_nlink <= XRAM_LINK_MAX) {
		return;
	}

	if (!*error) {
		*error = 1;
		printf("[NG] xno=");
	}
	printf("%u,", xno);
}

void
check_xrammnode_nsec(xram_xno_t xno, int *error)
{
	if (xm[xno].xmn_time_nsec < 1000 * 1000 * 1000) {
		return;
	}

	if (!*error) {
		*error = 1;
		printf("[NG] xno=");
	}
	printf("%u,", xno);
}

int
is_zero_region(const void *mem, size_t size)
{
	size_t pos;

	for (pos = 0; pos < size; pos++) {
		if ( ((char*)mem)[pos] != 0 ) {
			return (0);
		}
	}
	return (pos);
}

void
check_xrammnode_fifo(xram_xno_t xno, int *error)
{
	struct xrammnode *xmp = &xm[xno];
	int ret;

	if (XRAMMNODE_TYPE(xmp) != XIFIFO) {
		return;
	}

	ret = is_zero_region(&xmp->xmn_union, sizeof(xmp->xmn_union));
	if (ret) {
		return;
	}

	if (!*error) {
		*error = 1;
		printf("[NG] xno=");
	}
	printf("%u,", xno);
}

typedef int (*XMSNODESFUNC)(struct xrammnode *xmp, xram_xno_t xno, int fd,
                            off_t nextxmp_start, off_t currentxmp_end,
                            int *error, int islast);

int
check_xmsnodes(xrammnode_sort_t xms, uint32_t xms_count, const char *str,
               int fd, XMSNODESFUNC func)
{
	uint32_t i, type, regionsize;
	int error = 0, ret, islast;
	struct xrammnode *xmp, *nextxmp;
	off_t nextxmp_start, currentxmp_end;

	printf(str);
	fflush(stdout);
	for (i = 0; i < xms_count; i++) {
		xmp = xms[i].xm;
		type = XRAMMNODE_TYPE(xmp);
		if (i == xms_count - 1) {
			nextxmp_start = filesize;
			islast = 1;
		} else {
			nextxmp = xms[i + 1].xm;
			nextxmp_start = xnode_getoffsetofnode(nextxmp);
			islast = (type != XRAMMNODE_TYPE(nextxmp));
		}
		currentxmp_end = xnode_getoffsetofnode(xmp) + xmp->xmn_size;
		ret = (*func)(xms[i].xm, xms[i].xno, fd, nextxmp_start,
		              currentxmp_end, &error, islast);
		if (ret) {
			printf("\n");
			return (ret);
		}
	}
	if (!error) {
		printf("[OK]");
	}
	printf("\n");

	return (0);
}

#define roundup2(x, y)  (((x) + (1 << (y)) - 1) & (~((1 << (y)) - 1)))

int
check_xrammnode_corrupt(struct xrammnode *xmp, xram_xno_t xno, int fd,
                        off_t nextxmp_start, off_t currentxmp_end,
                        int *error, int islast)
{
	off_t nextblock, nextregion;

	nextblock = xnode_getoffsetofnode(xmp);
	switch (XRAMMNODE_TYPE(xmp)) {
	case XIFMAP:
		nextblock += roundup2(xmp->xmn_size, xh.xh_pageblk_shift);
		break;
	case XIFLNK:
		nextblock += roundup2(xmp->xmn_size, xh.xh_symlink_shift);
		break;
	case XIFPACK:
		nextblock += roundup2(xmp->xmn_size, xh.xh_pfile_shift);
		break;
	default:
		return (0);
	}
	if (currentxmp_end <= nextblock) {
		return (0);
	}

	nextregion = roundup2(nextblock, xh.xh_pageblk_shift);
	if (islast && nextblock <= nextregion) {
		return (0);
	}

	if (!*error) {
		*error = 1;
		printf("[NG] file size corrupted. xno=");
	}
	printf("%u,", xno);
	return (0);
}

int
check_xrammnode_padding(struct xrammnode *xmp, xram_xno_t xno, int fd,
                        off_t next, off_t paddingpos, int *error,
			int islast)
{
	off_t paddingbytes, seekret;
	int ret;
	void *padding;

	if (next <= paddingpos) {
		return (0);
	}

	paddingbytes = next - paddingpos;
	seekret = lseek(fd, paddingpos, SEEK_SET);
	if (seekret == (off_t)-1) {
		perror("lseek");
		return (1);
	}
	padding = dumpxramfsimage_malloc(1, paddingbytes);
	if (padding == NULL) {
		return (1);
	}
	ret = image_read(fd, padding, paddingbytes);
	if (ret) {
		free(padding);
		return (1);
	}
	ret = is_zero_region(padding, paddingbytes);
	free(padding);
	if (!ret) {
		if (!*error) {
			*error = 1;
			printf("[NG] non zero padding, xno=");
		}
		printf("%u,", xno);
	}

	return (0);
}

int
check_xrammnode_contents(struct xrammnode *xmp, xram_xno_t xno, int fd,
			 off_t next, off_t fileend, int *error,
			 int islast)
{
	int ret;
	off_t filebytes, filepos, seekret;
	void *file;
	uint16_t type;

	type = XRAMMNODE_TYPE(xmp);
	if ((type != XIFMAP)
	    && (type != XIFLNK)
	    && (type != XIFPACK)) {
	    return (0);
	}

	filepos = xnode_getoffsetofnode(xmp);
	filebytes = xmp->xmn_size;

	seekret = lseek(fd, filepos, SEEK_SET);
	if (seekret == (off_t)-1) {
		perror("lseek");
		return (1);
	}
	file = dumpxramfsimage_malloc(1, filebytes);
	if (file == NULL) {
		return (1);
	}
	ret = image_read(fd, file, filebytes);
	if (ret) {
		if (!*error) {
			*error = 1;
			printf("[NG] %s file can't be read, xno=",
			       xrammnode_typestr(type));
		}
		printf("%u,", xno);
		free(file);
		return (1);
	}
	free(file);
	return (0);
}

void
check_xrammnode_nlinks(int fd)
{
	uint32_t *nlinks, i;
	struct xramdirent_traverse_argument arg;
	xram_xno_t xno;
	struct xrammnode *xmp;
	int ret, error = 0;

	printf("  check nlinks ... ");
	fflush(stdout);

	/* countup nlinks */
	nlinks = dumpxramfsimage_malloc(sizeof(*nlinks), xh.xh_files);
	if (nlinks == NULL) {
		return;
	}
	for (i = 0; i < xh.xh_files; i++) {
		nlinks[i] = 0;
	}
	xramdirent_common_argument(&arg, fd);
	arg.cdata.countup.nlinks = nlinks;
	arg.prepare = xramdirent_countup_prepare;
	arg.doit = xramdirent_countup_doit;
	ret = xramdirent_traverse(&arg, XRAMMNODE_XNO_ROOT,
	                          XRAMMNODE_XNO_ROOT);
	xramdirent_dispose_argument(&arg);
	if (ret != 0) {
		free(nlinks);
		return;
	}

	/* check */
	for (i = 0; i < xh.xh_files; i++) {
		xno = XRAMMNODE_XNO_ROOT + i;
		xmp = &xm[xno];
		if (xmp->xmn_nlink == nlinks[i]) {
			continue;
		}
		if (!error) {
			error = 1;
			printf("[NG] xno=");
		}
		printf("%u,", xno);
	}
	free(nlinks);

	if (!error) {
		printf("[OK]");
	}
	printf("\n");
}

struct xramdirent_name_sort {
	char *name;
	uint32_t index;
	xram_xno_t xno;
};

int
xdnsort_compare_name(const void *p1, const void *p2)
{
	struct xramdirent_name_sort *s1 = (struct xramdirent_name_sort *)p1;
	struct xramdirent_name_sort *s2 = (struct xramdirent_name_sort *)p2;
	int ret;

	ret = strcmp(s1->name, s2->name);
	return (ret == 0 ? s1->index < s2->index : ret);
}

void
check_xrammnode_dir_sorted(int fd)
{
	uint32_t i, j, k, name_cnt;
	xram_xno_t xno;
	struct xrammnode *xmp;
	struct xramdircheader xdh;
	struct xramdirent *xde;
	int ret, error = 0;
	char *name;
	struct xramdirent_name_sort *xdns;

	printf("  check sorted directories ... ");
	fflush(stdout);
	for (i = 0; i < xh.xh_files; i++) {
		xno = XRAMMNODE_XNO_ROOT + i;
		xmp = &xm[xno];
		if (XRAMMNODE_TYPE(xmp) != XIFDIR) {
			continue;
		}
		if (xrammnode_getdircheader(fd, xmp, &xdh) != 0) {
			continue;
		}
		if (xdh.xdh_nent == 0 ||
		    xdh.xdh_type != XRAM_DTYPE_SORTED) {
			continue;
		}
		xde = dumpxramfsimage_malloc(sizeof(*xde), xdh.xdh_nent);
		if (xde == NULL) {
			error = 1;
			printf("[NG]");
			break;
		}
		xdns = dumpxramfsimage_malloc(sizeof(*xdns), xdh.xdh_nent);
		if (xdns == NULL) {
			error = 1;
			printf("[NG]");
			free(xde);
			break;
		}
		ret = image_read(fd, (char *)xde,
		                 sizeof(*xde) * xdh.xdh_nent);
		if (ret) {
			error = 1;
			printf("[NG]");
			free(xde);
			free(xdns);
			break;
		}

		/* check sorted */
		for (j = 0, name_cnt = 0; j < xdh.xdh_nent; j++) {
			name = xramdirent_name(fd, &xde[j]);
			if (name == NULL) {
			    continue;
			}
			xdns[name_cnt].name = strdup(name);
			xdns[name_cnt].index = name_cnt;
			xdns[name_cnt].xno = xde[j].xde_xno;
			name_cnt++;
		}
		qsort(xdns, name_cnt, sizeof(*xdns),
		      xdnsort_compare_name);

		for (j = 0; j < name_cnt; j++) {
			if (xdns[j].index == j) {
				continue;
			}
			if (!error) {
				error = 1;
				printf("[NG] xno=");
			}
			printf("%u,", xdns[j].xno);
		}

		/* free memory */
		for (j = 0; j < name_cnt; j++) {
			if (xdns[j].name) {
				free(xdns[j].name);
			}
		}
		free(xde);
		free(xdns);
	}

out:
	if (!error) {
		printf("[OK]");
	}
	printf("\n");
}

void
check_xrammnode_same_name(int fd)
{
	struct xramdirent_traverse_argument arg;
	int error = 0, ret;

	printf("  check same names in same directory ... ");
	fflush(stdout);
	xramdirent_common_argument(&arg, fd);
	arg.cdata.corrupt.fd = fd;
	arg.cdata.corrupt.error = &error;
	arg.prepare = xramdirent_corrupt_name_prepare;
	ret = xramdirent_traverse(&arg, XRAMMNODE_XNO_ROOT,
	                          XRAMMNODE_XNO_ROOT);
	xramdirent_dispose_argument(&arg);
	if (error) {
		printf("\n");
		return;
	}
	printf("[OK]\n");
}

struct check_xrammnodes_arg {
	const char *str;
	void (*func)(xram_xno_t, int*);
};

int
check_xramfsimage(int fd)
{
	struct xrammnode *root_xramnode = &xm[XRAMMNODE_XNO_ROOT];
	int filesize_error, ret, error;
	uint32_t i, xms_count;
	xram_xno_t xno;
	struct xrammnode *xmp;
	uint16_t type;
	xrammnode_sort_t xms;
	struct check_xrammnodes_arg args[] = {
		{ "  check types ... ", check_xrammnode_type_is_valid },
		{ "  check reserved ... ", check_xrammnode_reserved },
		{ "  check xmn_nlink ... ", check_xrammnode_nlink },
		{ "  check xmn_time_nsec ... ", check_xrammnode_nsec },
		{ "  check fifo start and size 0 ... ", check_xrammnode_fifo },
	};

	printf("start verifying filesystem image\n");

	ret = check_xramheader();
	if (ret) {
		return (-1);
	}

	ret = check_rootnode(root_xramnode);
	if (ret) {
		return (-1);
	}

	/* sequential check */
	for (i = 0; i < sizeof(args) / sizeof(args[0]); i++) {
		ret = check_xrammnodes(args[i].str, args[i].func);
		if (ret) {
			return (-1);
		}
	}

	check_xrammnode_name_is_in_range(fd);
	check_xrammnode_nlinks(fd);
	check_xrammnode_xno_is_valid(fd);
	check_xrammnode_dir_sorted(fd);

	check_xrammnode_same_name(fd);

	/* make size sorted xm array */
	xms = dumpxramfsimage_malloc(sizeof(struct xrammnode_sort),
	                             xh.xh_files);
	if (xms == NULL) {
		return (-1);
	}
	for (i = 0, xms_count = 0; i < xh.xh_files; i++) {
		xno = XRAMMNODE_XNO_ROOT + i;
		xmp = &xm[xno];
		type = XRAMMNODE_TYPE(xmp);
		if (type != XIFPACK && type != XIFMAP && type != XIFLNK) {
			continue;
		}
		if (xmp->xmn_size == 0) {
			continue;
		}
		xms[xms_count].xm = xmp;
		xms[xms_count].xno = xno;
		xms_count++;
	}
	qsort(xms, xms_count, sizeof(struct xrammnode_sort),
	      xmsort_compare_start);

	/* file size check */
	ret = check_xmsnodes(xms, xms_count,
	                     "  check size and blocks ... ",
	                     fd, check_xrammnode_corrupt);

	/* check padding */
	ret = check_xmsnodes(xms, xms_count,
	                     "  check padding ... ",
	                     fd, check_xrammnode_padding);

	/* check packed, map, symlink */
	ret = check_xmsnodes(xms, xms_count,
	                     "  check contents ... ",
	                     fd, check_xrammnode_contents);

	free(xms);

	return (0);
}

int
read_xramfsimage(int fd)
{
	int ret;
	uint32_t xrammnode_count;
	xram_xno_t xno;
	struct xrammnode *xmp;

	ret = image_read(fd, (char *)&xh, sizeof(xh));
	if (ret) {
		return (-1);
	}

	if (xh.xh_files > XRAM_MNODE_MAX) {
		fprintf(stderr, "too many xrammnodes to dump\n");
		return (-1);
	}

	if (isdevice) {
		filesize = (uint64_t)xh.xh_blocks << xh.xh_pageblk_shift;
	}

	if (strictmode && filesize !=
	    (uint64_t)xh.xh_blocks << xh.xh_pageblk_shift) {
		fprintf(stderr, "[strict check] invalid fsimage size\n");
		return (-1);
	}

	xrammnode_count = XRAMMNODE_XNO_ROOT + xh.xh_files;

	xm = dumpxramfsimage_malloc(sizeof(struct xrammnode), xrammnode_count);
	if (xm == NULL) {
		return (-1);
	}

	for (xno = XRAMMNODE_XNO_ROOT; xno < xrammnode_count; xno++) {
		xmp = &xm[xno];
		ret = image_read(fd, (char *)xmp, sizeof(*xmp));
		if (ret) {
			return (-1);
		}
	}

	return (0);
}

int
dump_xramfsimage(int fd)
{
	int ret;

	ret = dump_xramheader(fd);
	if (ret == -1) {
		return (-1);
	}

	ret = dump_xrammnode(fd);
	if (ret == -1) {
		fprintf(stderr, "dump_xrammnode error\n");
		return (-1);
	}

	ret = dump_xramdirent_fromroot(fd);
	if (ret == -1) {
		fprintf(stderr, "dump_xramdirent error\n");
		return (-1);
	}

	return (0);
}

int
main(int argc, char **argv)
{
	int optchar, check = 1, dump = 1, fd, ret;
	char *filename = NULL;
	struct stat st;
	uint64_t blocksize;

	while ((optchar = getopt(argc, argv, "CDs")) != -1) {
		switch (optchar) {
		case 'C':
			check = 0;
			break;
		case 'D':
			dump = 0;
			break;
		case 's':
			strictmode = 1;
			break;
		case '?':
		case ':':
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0) {
		fprintf(stderr, "specify fs image\n");
		usage();
	}

	filename = argv[0];
	fd = open(filename, O_NONBLOCK | O_RDONLY);
	if (fd == -1) {
		perror("open");
		exit(1);
	}

	ret = fstat(fd, &st);
	if (ret == -1) {
		perror("fstat");
		exit(1);
	}

	if (S_ISREG(st.st_mode)) {
		filesize = st.st_size;
	} else if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode)) {
		fprintf(stderr, "%s is a device node\n", filename);
		isdevice = 1;
	} else {
		fprintf(stderr, "%s is not a supported file type\n", filename);
		exit(1);
	}

	ret = read_xramfsimage(fd);
	if (ret) {
		exit(1);
	}

	if (isdevice == 0) {
		blocksize = (uint64_t)xh.xh_blocks << xh.xh_pageblk_shift;
		if (filesize < blocksize) {
			fprintf(stderr, "header block size is too big\n");
			filesize = blocksize;
		}
	}

	if (dump) {
		ret = dump_xramfsimage(fd);
		if (ret) {
			exit(1);
		}
	}

	if (check) {
		ret = check_xramfsimage(fd);
		if (ret) {
			exit(1);
		}
	}

	return (0);
}
