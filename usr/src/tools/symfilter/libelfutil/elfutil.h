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
 * Copyright (c) 2007-2009 NEC Corporation
 * All rights reserved.
 */

/* Common utilities. */

#ifndef	_TOOLS_SYMFILTER_LIBELFUTIL_ELFUTIL_H
#define	_TOOLS_SYMFILTER_LIBELFUTIL_ELFUTIL_H

#include <sys/ccompile.h>
#include <sys/types.h>
#include <errno.h>

/*
 * Global variables
 */
extern int	Verbose;
extern int	Exiting;

/* File access mode bits and setuid/setgid/sticky bits. */
#define	S_IAMB_ALL	0xfff


/*
 * Prototypes
 */

/* PRINTFLIKE2 */
extern void	verbose(int level, const char *fmt, ...) __PRINTFLIKE(2);
/* PRINTFLIKE2 */
extern void	fatal(int err, const char *fmt, ...) __PRINTFLIKE(2);
/* PRINTFLIKE1 */
extern void	warning(const char *fmt, ...) __PRINTFLIKE(1);
extern void	elfdie(char *file, const char *fmt, ...);
extern void	*xmalloc(size_t size);
extern void	*xrealloc(void *buf, size_t size);
extern char	*xstrdup(char *str);
extern void	xfree(void *buf);
extern void	move_file(char *srcfile, char *dstfile);
extern void	copy_file(char *srcfile, char *dstfile);
extern void	copy_file_fd(char *srcfile, int sfd, char *dstfile, int dfd);

#endif	/* !_TOOLS_SYMFILTER_LIBELFUTIL_ELFUTIL_H */
