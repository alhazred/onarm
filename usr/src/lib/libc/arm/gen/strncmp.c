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

#pragma ident	"@(#)strncmp.c	1.13	05/06/08 SMI"

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*
 * Copyright (c) 2007 NEC Corporation
 */


#include "synonyms.h"
#include <string.h>
#include <sys/types.h>

#define WMASK	3
#define OPT_LEN	20
#define IS_WORD_ALIGN_SAME(s1, s2)	((unsigned int)s1 & WMASK) == \
					((unsigned int)s2 & WMASK)
#define EFFECTIVE_LENGTH(a, b)	(a < b ? a : b)

extern int __memcmp_asm(const char *, const char *, size_t);

/*
 * Compare strings (at most n bytes)
 *	returns: s1>s2; >0  s1==s2; 0  s1<s2; <0
 */
int
strncmp(const char *s1, const char *s2, size_t n)
{
	size_t len, elen;

	if (s1 == s2)
		return (0);
	len = strlen(s1) + 1;
	elen = EFFECTIVE_LENGTH(n, len);
	if (elen >= OPT_LEN && IS_WORD_ALIGN_SAME(s1, s2))
		return (__memcmp_asm(s1, s2, elen));
	else {
		n++;
		while (--n != 0 && *s1 == *s2++)
			if (*s1++ == '\0')
				return (0);
		return (n == 0 ? 0 : *(unsigned char *)s1 - *(unsigned char *)--s2);
	}
}
