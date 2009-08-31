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

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/


#pragma weak ldivide = _ldivide
#include	"synonyms.h"
#include	<sys/types.h>
#include	<sys/dl.h>
#include	"libc.h"

dl_t
ldivide(dl_t lop, dl_t rop)
{
	int		cnt;
	dl_t		ans;
	dl_t		tmp;
	dl_t		div;

	if (lsign(lop))
		lop = lsub(lzero, lop);
	if (lsign(rop))
		rop = lsub(lzero, rop);

	ans = lzero;
	div = lzero;

	for (cnt = 0; cnt < 63; cnt++) {
		div = lshiftl(div, 1);
		lop = lshiftl(lop, 1);
		if (lsign(lop))
			div.dl_lop |= 1;
		tmp = lsub(div, rop);
		ans = lshiftl(ans, 1);
		if (lsign(tmp) == 0) {
			ans.dl_lop |= 1;
			div = tmp;
		}
	}

	return (ans);
}
