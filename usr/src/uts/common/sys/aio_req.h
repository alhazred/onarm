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
 * Copyright (c) 1993, by Sun Microsystems, Inc.
 */

#ifndef _SYS_AIO_REQ_H
#define	_SYS_AIO_REQ_H

#pragma ident	"@(#)aio_req.h	1.8	05/06/08 SMI"

#include <sys/buf.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _KERNEL

/*
 * async I/O request struct exposed to drivers.
 */
struct aio_req {
	struct uio	*aio_uio;		/* UIO for this request */
	void 		*aio_private;
};

extern int aphysio(int (*)(), int (*)(), dev_t, int, void (*)(),
		struct aio_req *);
extern int anocancel(struct buf *);

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_AIO_REQ_H */
