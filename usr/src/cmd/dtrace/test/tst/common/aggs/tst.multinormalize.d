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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma	ident	"@(#)tst.multinormalize.d	1.1	06/08/28 SMI"

/*
 * ASSERTION:
 *    Multiple aggregations are supported
 *
 * SECTION: Aggregations/Normalization
 *
 */

#pragma D option quiet
#pragma D option aggrate=1ms
#pragma D option switchrate=50ms

BEGIN
{
	i = 0;
	start = timestamp;
}

tick-100ms
/i < 20/
{
	@func1[i % 5] = sum(i * 100);
	@func2[i % 5 + 1] = sum(i * 200);
	i++;
}

tick-100ms
/i == 20/
{
	printf("normalized data #1:\n");
	normalize(@func1, 5);
	printa(@func1);

	printf("\nnormalized data #2:\n");
	normalize(@func2, 5);
	exit(0);
}
