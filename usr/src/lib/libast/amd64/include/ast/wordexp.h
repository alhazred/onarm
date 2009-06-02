
/* : : generated by proto : : */
/***********************************************************************
*                                                                      *
*               This software is part of the ast package               *
*           Copyright (c) 1985-2007 AT&T Knowledge Ventures            *
*                      and is licensed under the                       *
*                  Common Public License, Version 1.0                  *
*                      by AT&T Knowledge Ventures                      *
*                                                                      *
*                A copy of the License is available at                 *
*            http://www.opensource.org/licenses/cpl1.0.txt             *
*         (with md5 checksum 059e8cd6165cb4c31e351f2b69388fd9)         *
*                                                                      *
*              Information and Software Systems Research               *
*                            AT&T Research                             *
*                           Florham Park NJ                            *
*                                                                      *
*                 Glenn Fowler <gsf@research.att.com>                  *
*                  David Korn <dgk@research.att.com>                   *
*                   Phong Vo <kpv@research.att.com>                    *
*                                                                      *
***********************************************************************/
                  
/*
 * posix wordexp interface definitions
 */

#ifndef _WORDEXP_H
#if !defined(__PROTO__)
#include <prototyped.h>
#endif
#if !defined(__LINKAGE__)
#define __LINKAGE__		/* 2004-08-11 transition */
#endif

#define _WORDEXP_H

#include <ast_common.h>

#define WRDE_APPEND	01
#define WRDE_DOOFFS	02
#define WRDE_NOCMD	04
#define WRDE_NOSYS	0100
#define WRDE_REUSE	010
#define WRDE_SHOWERR	020
#define WRDE_UNDEF	040

#define WRDE_BADCHAR	1
#define WRDE_BADVAL	2
#define WRDE_CMDSUB	3
#define WRDE_NOSPACE	4
#define WRDE_SYNTAX	5
#define WRDE_NOSHELL	6

typedef struct _wdarg
{
	size_t	we_wordc;
	char	**we_wordv;
	size_t	we_offs;
} wordexp_t;

#if _BLD_ast && defined(__EXPORT__)
#undef __MANGLE__
#define __MANGLE__ __LINKAGE__		__EXPORT__
#endif

extern __MANGLE__ int wordexp __PROTO__((const char*, wordexp_t*, int));
extern __MANGLE__ int wordfree __PROTO__((wordexp_t*));

#undef __MANGLE__
#define __MANGLE__ __LINKAGE__

#endif /* _WORDEXP_H */
