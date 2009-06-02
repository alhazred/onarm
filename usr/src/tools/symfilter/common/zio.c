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
 * Copyright (c) 2007-2009 NEC Corporation
 * All rights reserved.
 */

#ident	"@(#)tools/symfilter/common/zio.c"

/*
 * zlib wrapper.
 * The main purpose of this file is to avoid compile error due to redefinition
 * of "Byte".
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "zio.h"
#include "elfutil.h"

#define	ZIODEF_STREAM(zdp)	((z_stream *)((zdp)->z_zlib))

/* Blocksize for data compression */
#define	ZIODEF_BLOCKSIZE	0x20000

/* Internal Prototypes */
static void	zio_grow(ziodef_t *zdp);

/*
 * size_t
 * zio_inflate(const void *src, size_t srcsize, void *dst, size_t dstsize)
 *	Decompress data compressed in zlib format.
 *
 * Calling/Exit State:
 *	zio_inflate() returns size of decompressed data.
 *	Any error is treated as fatal.
 */
size_t
zio_inflate(const void *src, size_t srcsize, void *dst, size_t dstsize)
{
	z_stream	zlib;
	int		zerr;

	zlib.zalloc = NULL;
	zlib.zfree = NULL;
	zlib.opaque = NULL;

	zlib.next_in = (Bytef *)src;
	zlib.avail_in = srcsize;
	zlib.next_out = (Bytef *)dst;
	zlib.avail_out = dstsize;

	if ((zerr = inflateInit(&zlib)) != Z_OK) {
		fatal(0, "infrateInit() failed: %s", zError(zerr));
	}

	if ((zerr = inflate(&zlib, Z_FINISH)) != Z_STREAM_END) {
		fatal(0, "inflate() failed: %s", zError(zerr));
	}

	if ((zerr = inflateEnd(&zlib)) != Z_OK) {
		fatal(0, "inflateEnd() failed; %s", zError(zerr));
	}

	verbose(1, "inflate: %d -> %ld bytes", srcsize, zlib.total_out);
	return zlib.total_out;
}

/*
 * void
 * zio_deflate_init(ziodef_t *zdp, void *header, size_t hsize)
 *	Initialize context for data compression.
 *	If header is not NULL, zio_deflate_init() will append the specified
 *	header at the top of compressed data.
 */
void
zio_deflate_init(ziodef_t *zdp, void *header, size_t hsize)
{
	z_stream	*zlib;
	void		*buf;
	size_t		sz;
	int		zerr;

	zlib = (z_stream *)xmalloc(sizeof(*zlib));
	zlib->zalloc = NULL;
	zlib->zfree = NULL;
	zlib->opaque = NULL;

	if ((zerr = deflateInit(zlib, Z_BEST_COMPRESSION)) != Z_OK) {
		fatal(0, "deflateInit() failed; %s", zError(zerr));
	}

	/* Prepare initial output buffer. */
	if (header == NULL) {
		hsize = 0;
	}
	sz = hsize + ZIODEF_BLOCKSIZE;
	buf = xmalloc(sz);
	if (hsize) {
		(void)memcpy(buf, header, hsize);
	}

	zdp->z_zlib = zlib;
	zdp->z_addr = (caddr_t)buf;
	zdp->z_size = sz;
	zdp->z_cur = zdp->z_addr + hsize;
}

/*
 * void
 * zio_deflate(ziodef_t *zdp, void *data, size_t size)
 *	Compress the specified data, and append compressed data into
 *	output buffer in the specified deflate context.
 */
void
zio_deflate(ziodef_t *zdp, void *data, size_t size)
{
	z_stream	*zlib = ZIODEF_STREAM(zdp);
	size_t		off;

	off = zdp->z_cur - zdp->z_addr;
	zlib->next_out = (Bytef *)zdp->z_cur;
	zlib->avail_out = zdp->z_size - off;
	zlib->next_in = (Bytef *)data;
	zlib->avail_in = size;

	while (zlib->avail_in > 0) {
		int	zerr;

		if (zlib->avail_out == 0) {
			zio_grow(zdp);
		}

		if ((zerr = deflate(zlib, Z_NO_FLUSH)) != Z_OK) {
			fatal(0, "deflate() failed: %s", zError(zerr));
		}
	}

	zdp->z_cur = (caddr_t)zlib->next_out;
}

/*
 * void
 * zio_deflate_flush(ziodef_t *zdp, uint_t flush)
 *	Flush zlib stream.
 */
void
zio_deflate_flush(ziodef_t *zdp, uint_t flush)
{
	z_stream	*zlib = ZIODEF_STREAM(zdp);
	int		ftype;

	ftype = (flush == ZIOFL_FINISH) ? Z_FINISH : Z_FULL_FLUSH;

	for (;;) {
		int	zerr;

		if (zlib->avail_out == 0) {
			zio_grow(zdp);
		}

		zerr = deflate(zlib, ftype);
		if (ftype == Z_FINISH && zerr == Z_STREAM_END) {
			/* Detected EOF. */
			break;
		}
		if (ftype == Z_FULL_FLUSH && zerr == Z_BUF_ERROR) {
			/* No input data is available. */
			break;
		}
		if (zerr != Z_OK) {
			fatal(0, "deflate(flush:%d) failed: %s",
			      ftype, zError(zerr));
		}
	}

	zdp->z_cur = (caddr_t)zlib->next_out;
}

/*
 * void *
 * zio_deflate_fini(ziodef_t *zdp, size_t *sizep)
 *	Finalize compression.
 *
 * Calling/Exit State:
 *	zio_deflate_fini() returns compressed data address, and set its size
 *	to *sizep. Note that returned buffer and size contains header area
 *	specified to zio_deflate_init().
 */
void *
zio_deflate_fini(ziodef_t *zdp, size_t *sizep)
{
	z_stream	*zlib = ZIODEF_STREAM(zdp);
	int		zerr;

	/* Flush zlib stream. */
	zio_deflate_flush(zdp, ZIOFL_FINISH);

	if ((zerr = deflateEnd(zlib)) != Z_OK) {
		fatal(0, "deflateEnd() failed: %s", zError(zerr));
	}

	*sizep = zdp->z_cur - zdp->z_addr;
	return zdp->z_addr;
}

/*
 * static void
 * zio_grow(ziodef_t *zdp)
 *	Grow output buffer.
 */
static void
zio_grow(ziodef_t *zdp)
{
	z_stream	*zlib = ZIODEF_STREAM(zdp);
	size_t		off;

	/* Preserve offset for current output pointer. */
	off = (caddr_t)zlib->next_out - zdp->z_addr;

	zlib->avail_out += ZIODEF_BLOCKSIZE;
	zdp->z_size += ZIODEF_BLOCKSIZE;
	zdp->z_addr = (caddr_t)xrealloc(zdp->z_addr, zdp->z_size);
	zdp->z_cur = zdp->z_addr + off;

	/* Update buffer pointer in zlib stream. */
	zlib->next_out = (Bytef *)zdp->z_cur;
}
