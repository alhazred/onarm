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

#pragma ident	"@(#)_creat.c	1.5	06/12/02 SMI"

#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h>
#include <sys/param.h>
#include "compat.h"	/* for UTMPX_MAGIC_FLAG */

int
creat_com(char *path, int mode)
{
	int fd;

	if (strcmp(path, "/etc/mtab") == 0 ||
	    strcmp(path, "/etc/fstab") == 0) {
		errno = ENOENT;
		return (-1);
	}
	if (strcmp(path, "/var/adm/wtmp") == 0) {
		if ((fd = _syscall(SYS_creat, "/var/adm/wtmpx", mode)) >= 0)
			fd_add(fd, UTMPX_MAGIC_FLAG);
		return (fd);
	}
	if (strcmp(path, "/etc/utmp") == 0 ||
	    strcmp(path, "/var/adm/utmp") == 0) {
		if ((fd = _syscall(SYS_creat, "/var/adm/utmpx", mode)) >= 0)
			fd_add(fd, UTMPX_MAGIC_FLAG);
		return (fd);
	}
	return (_syscall(SYS_creat, path, mode));
}
