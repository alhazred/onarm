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
 * Copyright (c) 2008-2009 NEC Corporation
 * All rights reserved.
 */

#ident	"@(#)tools/symfilter/libelfutil/verbose.c"

#include <stdio.h>
#include <stdarg.h>
#include "elfutil.h"

int	Verbose;

/*
 * Print verbose message.
 * Current verbose level will be passed by global variable, Verbose.
 */
void
verbose(int level, const char *fmt, ...)
{
	int	i;
	va_list	ap;

	if (Verbose < level) {
		return;
	}

	(void)fputc(' ', stderr);
	for (i = 0; i < level; i++) {
		(void)fputc('+', stderr);
	}
	(void)fputc(' ', stderr);

	va_start(ap, fmt);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
}