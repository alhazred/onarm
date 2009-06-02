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
 * Copyright (c) 2007 NEC Corporation
 * All rights reserved.
 */

#pragma ident	"@(#)spawn_fast.c"

#include <spawn.h>

#pragma weak posix_spawn = __posix_spawn
static int
__posix_spawn(
	pid_t *pidp,
	const char *path,
	const posix_spawn_file_actions_t *file_actions,
	const posix_spawnattr_t *attrp,
	char *const argv[],
	char *const envp[])
{
	return (_posix_spawn(pidp, path, file_actions, attrp, argv, envp));
}

#pragma weak posix_spawnp = __posix_spawnp
static int
__posix_spawnp(
	pid_t *pidp,
	const char *file,
	const posix_spawn_file_actions_t *file_actions,
	const posix_spawnattr_t *attrp,
	char *const argv[],
	char *const envp[])
{
	return (_posix_spawnp(pidp, file, file_actions, attrp, argv, envp));
}
