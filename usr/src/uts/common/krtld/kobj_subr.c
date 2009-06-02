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

#pragma ident	"@(#)kobj_subr.c	1.8	07/11/29 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>

#include <sys/bootconf.h>
#include <sys/kobj_impl.h>
#include <sys/cmn_err.h>

#ifndef	STATIC_UNIX
/*
 * Standalone copies of some basic routines.  Note that these routines
 * are transformed via Makefile -D flags into krtld_* routines.  So,
 * this version of strcmp() will become krtld_strcmp() when built.
 *
 * This dubious practice is so that krtld can have its own private
 * versions of these routines suitable for use during early boot,
 * when kernel-based routines might not work.  Make sure to use 'nm'
 * on your krtld to make sure it is calling the appropriate routines.
 */

/*
 * Standalone utility functions for use within krtld.
 * Many platforms implement optimized platmod versions of
 * utilities such as bcopy and any such are not yet available
 * until the kernel is more completely stitched together.
 * These standalones are referenced through vectors
 * kobj_bzero, etc.  Throughout krtld, the usual utility
 * is redefined to reference through the corresponding
 * vector so that krtld may simply refer to bzero etc.
 * as usual.  See kobj_impl.h.
 */

/*ARGSUSED*/
static void
kprintf(void *op, const char *fmt, ...)
{
	va_list adx;

	va_start(adx, fmt);
	vprintf(fmt, adx);
	va_end(adx);
}

static void
stand_bzero(void *p_arg, size_t count)
{
	char zero = 0;
	caddr_t p = p_arg;

	while (count != 0)
		*p++ = zero, count--;
}

static void
stand_bcopy(const void *src_arg, void *dest_arg, size_t count)
{
	caddr_t src = (caddr_t)src_arg;
	caddr_t dest = dest_arg;

	if (src < dest && (src + count) > dest) {
		/* overlap copy */
		while (--count != -1)
			*(dest + count) = *(src + count);
	} else {
		while (--count != -1)
			*dest++ = *src++;
	}
}

static size_t
stand_strlcat(char *dst, const char *src, size_t dstsize)
{
	char *df = dst;
	size_t left = dstsize;
	size_t l1;
	size_t l2 = strlen(src);
	size_t copied;

	while (left-- != 0 && *df != '\0')
		df++;
	l1 = df - dst;
	if (dstsize == l1)
		return (l1 + l2);

	copied = l1 + l2 >= dstsize ? dstsize - l1 - 1 : l2;
	bcopy(src, dst + l1, copied);
	dst[l1+copied] = '\0';
	return (l1 + l2);
}

/*
 * Set up the krtld standalone utilty vectors
 */
void
kobj_setup_standalone_vectors()
{
	_kobj_printf = (void (*)(void *, const char *, ...))bop_printf;
	kobj_bcopy = stand_bcopy;
	kobj_bzero = stand_bzero;
	kobj_strlcat = stand_strlcat;
}

/*
 * Restore the kprintf/bcopy/bzero kobj vectors.
 * We need to undefine the override macros to
 * accomplish this.
 *
 * Do NOT add new code after the point or at least
 * certainly not code using bcopy or bzero which would
 * need to be vectored to the krtld equivalents.
 */
#undef	bcopy
#undef	bzero
#undef	strlcat

void
kobj_restore_vectors()
{
	_kobj_printf = kprintf;
	kobj_bcopy = bcopy;
	kobj_bzero = bzero;
	kobj_strlcat = strlcat;
}
#endif	/* !STATIC_UNIX */