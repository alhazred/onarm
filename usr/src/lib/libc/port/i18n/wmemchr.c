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
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)wmemchr.c	1.4	05/06/08 SMI"

#include "lint.h"
#include <sys/types.h>
#include <wchar.h>
#include "libc.h"

wchar_t *
wmemchr(const wchar_t *ws, wchar_t wc, size_t n)
{
	if (n != 0) {
		do {
			if (*ws++ == wc)
				return ((wchar_t *)--ws);
		} while (--n != 0);
	}
	return (NULL);
}
