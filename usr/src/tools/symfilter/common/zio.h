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
 * Copyright (c) 2007 NEC Corporation
 * All rights reserved.
 */

/* zlib I/O wrapper. */

#ifndef	_TOOLS_SYMFILTER_COMMON_ZIO_H
#define	_TOOLS_SYMFILTER_COMMON_ZIO_H

#include <sys/types.h>

/* Context for deflate */
typedef struct ziodef {
	void	*z_zlib;		/* zlib context */
	caddr_t	z_addr;			/* Output buffer */
	size_t	z_size;			/* Size of output buffer */
	caddr_t	z_cur;			/* Current pointer in output buffer */
} ziodef_t;

/* Flags for zio_deflate_flush() */
#define	ZIOFL_FULL_FLUSH	0x1
#define	ZIOFL_FINISH		0x2

/* Prototypes */
extern size_t	zio_inflate(const void *src, size_t srcsize, void *dst,
			    size_t dstsize);
extern void	zio_deflate_init(ziodef_t *zdp, void *header, size_t hsize);
extern void	zio_deflate(ziodef_t *zdp, void *data, size_t size);
extern void	zio_deflate_flush(ziodef_t *zdp, uint_t flush);
extern void	*zio_deflate_fini(ziodef_t *zdp, size_t *sizep);

#endif	/* !_TOOLS_SYMFILTER_COMMON_ZIO_H */
