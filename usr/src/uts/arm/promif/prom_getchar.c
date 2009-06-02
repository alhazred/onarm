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

#pragma ident	"@(#)prom_getchar.c	1.13	05/06/08 SMI"

#include <sys/types.h>
#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/bootsvcs.h>

/*
 * prom_getchar
 *
 * always returns a character; waits for input if none is available.
 */
uchar_t
prom_getchar(void)
{
	while (BSVC_ISCHAR(SYSP) == 0)
		;
	return (BSVC_GETCHAR(SYSP));
}

/*
 * prom_mayget
 *
 * returns a character from the keyboard if one is available,
 * otherwise returns a -1.
 */
int
prom_mayget(void)
{
	if (BSVC_ISCHAR(SYSP) == 0)
		return (-1);
	return (BSVC_GETCHAR(SYSP));
}
