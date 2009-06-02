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

#pragma ident	"@(#)memcmp.c	1.16	05/06/08 SMI"

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*
 * Copyright (c) 2007 NEC Corporation
 */


#if !defined(_KMDB) && !defined(_BOOT) && !defined(_KERNEL)

#pragma weak memcmp = _memcmp

#include "synonyms.h"
#endif /* !_KMDB && !_BOOT && !_KERNEL */

#include <sys/types.h>
#if defined(_KERNEL)
#include <sys/systm.h>
#else
#include <string.h>
#include <stddef.h>
#endif

#define WMASK	3
#define OPT_LEN	20
#define IS_WORD_ALIGN_SAME(s1, s2)	((unsigned int)s1 & WMASK) == \
					((unsigned int)s2 & WMASK)

extern int __memcmp_asm(const unsigned char *, const unsigned char *, size_t);

/*
 * Compare n bytes:  s1>s2: >0  s1==s2: 0  s1<s2: <0
 */
int
memcmp(const void *s1, const void *s2, size_t n)
{
	if (s1 != s2 && n != 0) {
		const unsigned char *ps1 = s1;
		const unsigned char *ps2 = s2;

		if (n >= OPT_LEN && IS_WORD_ALIGN_SAME(s1, s2))
			return (__memcmp_asm(s1, s2, n));
		else
			do {
				if (*ps1++ != *ps2++)
					return (ps1[-1] - ps2[-1]);
			} while (--n != 0);
	}

	return (0);
}
