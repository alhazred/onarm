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
 * Copyright (c) 1995-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)tgetnum.c	1.6	05/06/08 SMI"

/* LINTLIBRARY */

/*
 * tgetnum.c
 *
 * XCurses Library
 *
 * Copyright 1990, 1995 by Mortice Kern Systems Inc.  All rights reserved.
 *
 */

#ifdef M_RCSID
#ifndef lint
static char rcsID[] =
"$Header: /team/ps/sun_xcurses/archive/local_changes/xcurses/src/lib/"
"libxcurses/src/libc/xcurses/rcs/tgetnum.c 1.3 1998/05/28 17:10:22 "
"cbates Exp $";
#endif
#endif

#include <private.h>
#include <string.h>

int
tgetnum(char *cap)
{
	const char	**p;
	int	value = -2;

	for (p = numcodes; *p != NULL; ++p) {
		if (memcmp(*p, cap, 2) == 0) {
			/*
			 * (p - numcodes) should be enough small
			 * to fit in 'int'.
			 */
			value = cur_term->_num[(int)(p - numcodes)];
			break;
		}
	}

	return (value);
}
