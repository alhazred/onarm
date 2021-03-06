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

/*
 * Copyright (c) 2006 NEC Corporation
 */

#pragma ident	"@(#)prom_putchar.c	1.18	05/06/08 SMI"

#include <sys/types.h>
#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/bootsvcs.h>

/*ARGSUSED*/
int
prom_mayput(char c)
{
	prom_putchar(c);
	return (1);
}

void
prom_putchar(char c)
{
	static int bhcharpos = 0;
	promif_owrap_t *ow;

	ow = promif_preout();
	switch (c) {
	case '\t':
		do {
			BSVC_PUTCHAR(SYSP, ' ');
		} while (++bhcharpos % 8);
		break;
	case '\n':
	case '\r':
		bhcharpos = 0;
		BSVC_PUTCHAR(SYSP, c);
		break;
	case '\b':
		if (bhcharpos)
			bhcharpos--;
		BSVC_PUTCHAR(SYSP, c);
		break;
	default:
		bhcharpos++;
		BSVC_PUTCHAR(SYSP, c);
		break;
	}
	promif_postout(ow);
}

void
prom_writestr(const char *s, size_t n)
{
	while (n-- != 0)
		prom_putchar(*s++);
}
