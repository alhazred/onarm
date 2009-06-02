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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)memcpy.c	1.17	06/03/21 SMI"
#pragma ident	"@(#)memmove.c	1.14	05/06/08 SMI"

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*
 * Copyright (c) 2007 NEC Corporation
 */

/*
 * The SunStudio compiler may generate calls to _memcpy and so we
 * need to make sure that the correct symbol exists for these calls,
 * whether it be libc (first case below) or the kernel (second case).
 */

#pragma weak memcpy = _memcpy

#pragma weak _private_memcpy = _memcpy
#if defined(__ARM_EABI__)
#pragma weak __aeabi_memcpy8 = _memcpy
#pragma weak __aeabi_memcpy4 = _memcpy
#pragma weak __aeabi_memcpy  = _memcpy
#endif

#include "synonyms.h"

#include <sys/types.h>

#define WMASK	3
#define OPT_LEN	20
#define IS_WORD_ALIGN_SAME(s1, s2)	((unsigned int)s1 & WMASK) == \
					((unsigned int)s2 & WMASK)
#define ENOUGH_DIFFERENCE(a,b)	(((unsigned int)a - (unsigned int)b) > 4)

extern char* __memcpy_asm(char *, const char *, size_t);
extern char* __memcpyr_asm(char *, const char *, size_t);

/*
 * Copy s0 to s, always copy n bytes.
 * Return s
 */
void *
_memcpy(void *s, const void *s0, size_t n)
{
	if (n != 0) {
		char *s1 = s;
		const char *s2 = s0;

		if (n > OPT_LEN && IS_WORD_ALIGN_SAME(s1, s2))
			(void)__memcpy_asm(s1, s2, n);
		else
			do {
				*s1++ = *s2++;
			} while (--n != 0);
	}
	return (s);
}


/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/


#pragma weak memmove = _memmove
#if defined(__ARM_EABI__)
#pragma weak __aeabi_memmove8 = _memmove
#pragma weak __aeabi_memmove4 = _memmove
#pragma weak __aeabi_memmove  = _memmove
#endif

/*
 * Copy s0 to s, always copy n bytes.
 * Return s
 * Copying between objects that overlap will take place correctly
 */
void *
_memmove(void *s, const void *s0, size_t n)
{
	if (n != 0) {
		char *s1 = s;
		const char *s2 = s0;

		if (s1 <= s2) {
			if (n > OPT_LEN && IS_WORD_ALIGN_SAME(s1, s2) &&
			    ENOUGH_DIFFERENCE(s2, s1))
				(void)__memcpy_asm(s1, s2, n);
			else
				do {
					*s1++ = *s2++;
				} while (--n != 0);
		} else {
			if (n > OPT_LEN && IS_WORD_ALIGN_SAME(s1, s2) &&
			    ENOUGH_DIFFERENCE(s1, s2))
				(void)__memcpyr_asm(s1, s2, n);
			else {
				s2 += n;
				s1 += n;
				do {
					*--s1 = *--s2;
				} while (--n != 0);
			}
		}
	}
	return (s);
}
