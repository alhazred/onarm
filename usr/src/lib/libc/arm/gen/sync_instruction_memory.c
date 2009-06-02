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
 * Copyright (c) 2006-2007 NEC Corporation
 */

#pragma ident	"@(#)sync_instruction_memory.c	1.4	05/06/08 SMI"

#include <sys/asm_linkage.h>
#include <sys/sysarm.h>

/*
 * void sync_instruction_memory(caddr_t addr, size_t len)
 *
 * Make the memory at [addr, addr+len) valid for instruction execution.
 */

#define CACHE_LINE_SIZE	4
#define ROUNDUP(x, y)	((((x)+((y)-1))/(y))*(y))

void
sync_instruction_memory(caddr_t addr, size_t len)
{
	uint_t start = ((uint_t)addr / CACHE_LINE_SIZE) * CACHE_LINE_SIZE;
	size_t size = ROUNDUP(len, CACHE_LINE_SIZE);

	(void)sysarm(SARM_DC_FLUSHVADDR,
		(uintptr_t)start,
		(uintptr_t)size,
		(uintptr_t)NULL);
	return;
}
