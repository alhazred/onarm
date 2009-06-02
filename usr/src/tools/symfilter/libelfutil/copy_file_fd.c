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

#ident	"@(#)tools/symfilter/libelfutil/copy_file.c"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "elfutil.h"

#define	IO_BUFSIZE	0x2000

/*
 * Copy file specified by the given file descriptor.
 */
void
copy_file_fd(char *srcfile, int sfd, char *dstfile, int dfd)
{
	/* CONSTANTCONDITION */
	while (1) {
		ssize_t	nbytes, remain;
		char	buf[IO_BUFSIZE], *bufp;

		if ((nbytes = read(sfd, buf, sizeof(buf))) == (ssize_t)-1) {
			fatal(errno, "Can't read \"%s\"", srcfile);
			/* NOTREACHED */
		}
		if (nbytes == 0) {
			break;
		}
		remain = nbytes;
		bufp = buf;

		while (remain > 0) {
			ssize_t	sz;

			if ((sz = write(dfd, bufp, remain)) == (ssize_t)-1) {
				fatal(errno, "Can't write to \"%s\"", dstfile);
				/* NOTREACHED */
			}
			if (sz > remain) {
				fatal(0, "write(%d) returns %d", remain, sz);
				/* NOTREACHED */
			}
			remain -= sz;
			bufp += sz;
		}
	}
}
