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

#ident	"@(#)tools/symfilter/libelfutil/elfdie.c"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <libelf.h>
#include "elfutil.h"

/*
 * Print error message and die.
 * libelf error message is also printed.
 */
void
elfdie(char *file, const char *fmt, ...)
{
	va_list	ap;

	if (Exiting) {
		return;
	}

	(void)fprintf(stderr, "%s: ", file);
	va_start(ap, fmt);
	(void)vfprintf(stderr, fmt, ap);
	(void)fprintf(stderr, "\nlibelf error = %s\n", elf_errmsg(-1));
	va_end(ap);

	Exiting = 1;
	exit(1);
}
