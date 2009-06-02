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
 * Copyright (c) 2008 NEC Corporation
 * All rights reserved.
 */

#pragma ident   "@(#)cpu_intr_thread.c"

#include <sys/cpuvar.h>

void
cpu_intr_init(cpu_t *cp, int n)
{
	cpu_intr_alloc(cp , n);
}

void
cpu_intr_mp_init(cpu_t *cp)
{
	cpu_intr_alloc(cp , NINTR_THREADS);
}

void
cpu_intr_append(int pri_level)
{
}
