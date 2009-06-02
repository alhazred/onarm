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

/*
 * Copy file specified by the given path.
 */
void
copy_file(char *srcfile, char *dstfile)
{
	int		sfd, dfd;
	mode_t		mode;
	struct stat	st;

	if ((sfd = open(srcfile, O_RDONLY)) == -1) {
		fatal(errno, "Can't open \"%s\"", srcfile);
		/* NOTREACHED */
	}
	if (fstat(sfd, &st) == -1) {
		fatal(errno, "stat(\"%s\") failed", srcfile);
		/* NOTREACHED */
	}
	mode = st.st_mode & S_IAMB_ALL;

	if ((dfd = open(dstfile, O_WRONLY|O_CREAT|O_TRUNC, mode)) == -1) {
		fatal(errno, "Can't open \"%s\"", dstfile);
		/* NOTREACHED */
	}

	copy_file_fd(srcfile, sfd, dstfile, dfd);

	if (close(sfd) == -1) {
		fatal(errno, "%s: close(%d) failed.", srcfile, sfd);
		/* NOTREACHED */
	}
	if (close(dfd) == -1) {
		fatal(errno, "%s: close(%d) failed.", dstfile, dfd);
		/* NOTREACHED */
	}
}
