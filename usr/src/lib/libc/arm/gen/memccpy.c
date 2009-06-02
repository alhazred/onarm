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

#pragma ident	"@(#)memccpy.c	1.16	05/06/08 SMI"

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*
 * Copyright (c) 2007 NEC Corporation
 */


#ifndef _KMDB
#pragma weak memccpy = _memccpy

#include "synonyms.h"
#endif /* !_KMDB */
#include <sys/types.h>
#include <string.h>
#include <stddef.h>
#include <memory.h>

#define WMASK	3
#define OPT_LEN	20
#define IS_WORD_ALIGN_SAME(s1, s2)	((unsigned int)s1 & WMASK) == \
					((unsigned int)s2 & WMASK)

extern unsigned char *__memccpy_asm(unsigned char *, const unsigned char *, int, size_t);

/*
 * Copy s0 to s, stopping if character c is copied. Copy no more than n bytes.
 * Return a pointer to the byte after character c in the copy,
 * or NULL if c is not found in the first n bytes.
 */
void *
memccpy(void *s, const void *s0, int c, size_t n)
{
	if (n != 0) {
		unsigned char *s1 = s;
		const unsigned char *s2 = s0;
		if (n >= OPT_LEN && IS_WORD_ALIGN_SAME(s1, s2))
			return (__memccpy_asm(s1, s2, (unsigned char)c, n));
		else {
			do {
				if ((*s1++ = *s2++) == (unsigned char)c)
					return (s1);
			} while (--n != 0);
		}
	}
	return (NULL);
}
