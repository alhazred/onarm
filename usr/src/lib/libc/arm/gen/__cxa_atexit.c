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

#pragma ident	"@(#)__cxa_atexit.c"

/*
 * Implement dummy __cxa_atexit() that always returns -1.
 * It is required to avoid undefined symbol on C++ programs because
 * ARM EABI mode libstdc++.so always refers __cxa_atexit().
 */

#include <sys/types.h>

int
__cxa_atexit(void (*destructor)(void *), void *object, void *dso_handle)
{
	return -1;
}