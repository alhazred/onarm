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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

#pragma ident	"@(#)line.c	1.10	05/06/08 SMI"

/*
 *	This program reads a single line from the standard input
 *	and writes it on the standard output. It is probably most useful
 *	in conjunction with the shell.
 */

#include <limits.h>
#include <unistd.h>

#define	LSIZE	LINE_MAX		/* POSIX.2 */

static char	readc(void);

static int EOF;
static char nl = '\n';

/*ARGSUSED*/
int
main(int argc, char **argv)
{
	char c;
	char line[LSIZE];
	char *linep, *linend;

	EOF = 0;
	linep = line;
	linend = line + LSIZE;

	while ((c = readc()) != nl) {
		if (linep == linend) {
			(void) write(1, line, LSIZE);
			linep = line;
		}
		*linep++ = c;
	}

	/* LINTED  E_PTRDIFF_T_OVERFLOW */
	(void) write(1, line, linep-line);
	(void) write(1, &nl, 1);
	if (EOF == 1)
		return (1);
	return (0);
}

static char
readc(void)
{
	char c;

	if (read(0, &c, 1) != 1) {
		EOF = 1;
		return (nl);
	}
	else
		return (c);
}
