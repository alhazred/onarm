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
 * Copyright 1997 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*
 * University Copyright- Copyright (c) 1982, 1986, 1988
 * The Regents of the University of California
 * All Rights Reserved
 *
 * University Acknowledgment- Portions of this document are derived from
 * software developed by the University of California, Berkeley, and its
 * contributors.
 */

#pragma ident	"@(#)waddchnstr.c	1.10	05/06/08 SMI"

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

/*
 * Add ncols worth of data to win, using string as input.
 * Return the number of chtypes copied.
 */
int
waddchnstr(WINDOW *win, chtype *string, int ncols)
{
	short		my_x = win->_curx;
	short		my_y = win->_cury;
	int		remcols;
	int		b;
	int		sw;
	int		ew;

	if (ncols < 0) {
		remcols = win->_maxx - my_x;
		while (*string && remcols) {
			sw = mbscrw((int)(_CHAR(*string)));
			ew = mbeucw((int)(_CHAR(*string)));
			if (remcols < sw)
				break;
			for (b = 0; b < ew; b++) {
				if (waddch(win, *string++) == ERR)
					goto out;
			}
			remcols -= sw;
		}
	} else {
		remcols = win->_maxx - my_x;
		while ((*string) && (remcols > 0) && (ncols > 0)) {
			sw = mbscrw((int)(_CHAR(*string)));
			ew = mbeucw((int)(_CHAR(*string)));
			if ((remcols < sw) || (ncols < ew))
				break;
			for (b = 0; b < ew; b++) {
				if (waddch(win, *string++) == ERR)
					goto out;
			}
			remcols -= sw;
			ncols -= ew;
		}
	}
out:
	/* restore cursor position */
	win->_curx = my_x;
	win->_cury = my_y;

	win->_flags |= _WINCHANGED;

	/* sync with ancestor structures */
	if (win->_sync)
		wsyncup(win);

	return (win->_immed ? wrefresh(win) : OK);
}
