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

/*      Copyright (c) 1984 AT&T */
/*        All Rights Reserved   */

#pragma ident	"@(#)execle.c	1.10	05/09/30 SMI"

#include <stdarg.h>

/*
 *	execle(name, arg0, arg1, ..., argn, (char *)0, envp)
 */
int
execle(char *name, ...)
{
	va_list	args;
	char	**first;
	char	**environmentp;

	va_start(args, name);
	first = (char **)args;
	/* traverse argument list to NULL */
	while (va_arg(args, char *) != (char *)0)
		;
	/* environment is next arg */
	environmentp = va_arg(args, char **);
	va_end(args);

	return (execve(name, first, environmentp));
}
