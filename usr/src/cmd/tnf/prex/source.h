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
 * Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#ifndef _SOURCE_H
#define	_SOURCE_H

#pragma ident	"@(#)source.h	1.8	05/06/08 SMI"

/*
 * Declarations
 */

#ifdef __cplusplus
extern "C" {
#endif

void			source_init(void);
void			source_file(char *path);
int			 source_input(void);
void			source_unput(int c);
void			source_output(int c);
void			source_nl(void);

void			yyerror(char *s);
void			prompt(void);
void
semantic_err(char *format, ...);

#ifdef __cplusplus
}
#endif

#endif /* _SOURCE_H */
