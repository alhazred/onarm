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
 * Copyright 2003 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SA_STDLIB_H
#define	_SA_STDLIB_H

#pragma ident	"@(#)stdlib.h	1.2	05/06/08 SMI"


/*
 * Exported interfaces for standalone's subset of libc's <stdlib.h>.
 * All standalone code *must* use this header rather than libc's.
 */

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void *malloc(size_t);
extern void *calloc(size_t, size_t);
extern void *realloc(void *, size_t);
extern void free(void *);
extern void abort(void);
extern char *getenv(const char *);
extern void qsort(void *, size_t, size_t, int (*)(const void *, const void *));
extern void *bsearch(const void *, const void *, size_t, size_t,
    int (*)(const void *, const void *));

#ifdef __cplusplus
}
#endif

#endif /* _SA_STDLIB_H */
