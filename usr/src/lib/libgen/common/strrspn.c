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

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)strrspn.c	1.11	06/01/04 SMI"	/* SVr4.0 1.1.2.2 */

/*LINTLIBRARY*/

#pragma weak strrspn = _strrspn

#include "gen_synonyms.h"
#include <sys/types.h>
#include <string.h>

/*
 *	Trim trailing characters from a string.
 *	Returns pointer to the first character in the string
 *	to be trimmed (tc).
 */

char *
strrspn(const char *string, const char *tc)
{
	char	*p;

	p = (char *)string + strlen(string);
	while (p != (char *)string)
		if (!strchr(tc, *--p))
			return (++p);

	return (p);
}
