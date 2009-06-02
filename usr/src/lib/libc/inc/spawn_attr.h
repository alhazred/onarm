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
 */

#ifndef	_SPAWN_ATTR_H_
#define	_SPAWN_ATTR_H_

#pragma ident	"@(#)spawn_attr.h"

#include "thr_uberdata.h"
#include <sys/types.h>
#include <signal.h>
#include <sched.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct {
	short		sa_psflags;	/* POSIX_SPAWN_* flags */
	pri_t		sa_priority;
	int		sa_schedpolicy;
	pid_t		sa_pgroup;
	sigset_t	sa_sigdefault;
	sigset_t	sa_sigmask;
} spawn_attr_t;

typedef struct file_attr {
	struct file_attr *fa_next;	/* circular list of file actions */
	struct file_attr *fa_prev;
	enum {FA_OPEN, FA_CLOSE, FA_DUP2} fa_type;
	uint_t		fa_pathsize;	/* size of fa_path[] array */
	char		*fa_path;	/* copied pathname for open() */
	int		fa_oflag;	/* oflag for open() */
	mode_t		fa_mode;	/* mode for open() */
	int		fa_filedes;	/* file descriptor for open()/close() */
	int		fa_newfiledes;	/* new file descriptor for dup2() */
} file_attr_t;

#define	GET_DEFAULT_PATHENV(pathstr, xpg4)				    \
	do {								    \
		/*							    \
		 * XPG4:  pathstr is equivalent to _CS_PATH, except that    \
		 * :/usr/sbin is appended when root, and pathstr must end   \
		 * with a colon when not root.  Keep these paths in sync    \
		 * with _CS_PATH in confstr.c.  Note that pathstr must end  \
		 * with a colon when not root so that when file doesn't	    \
		 * contain '/', the last call to execat() will result in an \
		 * attempt to execv file from the current directory.	    \
		 */							    \
		if (_private_geteuid() == 0 || _private_getuid() == 0) {    \
			if (!xpg4)					    \
				pathstr = "/usr/sbin:/usr/ccs/bin:/usr/bin";\
			else						    \
				pathstr = "/usr/xpg4/bin:/usr/ccs/bin:"     \
				"/usr/bin:/opt/SUNWspro/bin:/usr/sbin";	    \
		} else {						    \
			if (!xpg4)					    \
				pathstr = "/usr/ccs/bin:/usr/bin:";	    \
			else						    \
				pathstr = "/usr/xpg4/bin:/usr/ccs/bin:"	    \
					"/usr/bin:/opt/SUNWspro/bin:";	    \
		}							    \
	} while (0)

#define	GET_DEFAULT_SHELLPATH(xpg4)		\
	(xpg4 ?  "/usr/xpg4/bin/sh":"/bin/sh")

#ifdef	__cplusplus
}
#endif

#endif	/* _SPAWN_ATTR_H_ */
