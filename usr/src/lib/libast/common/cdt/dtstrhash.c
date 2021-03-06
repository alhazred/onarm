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
#include	"dthdr.h"

/* Hashing a string into an unsigned integer.
** The basic method is to continuingly accumulate bytes and multiply
** with some given prime. The length n of the string is added last.
** The recurrent equation is like this:
**	h[k] = (h[k-1] + bytes)*prime	for 0 <= k < n
**	h[n] = (h[n-1] + n)*prime
** The prime is chosen to have a good distribution of 1-bits so that
** the multiplication will distribute the bits in the accumulator well.
** The below code accumulates 2 bytes at a time for speed.
**
** Written by Kiem-Phong Vo (02/28/03)
*/

#if __STD_C
uint dtstrhash(reg uint h, Void_t* args, reg int n)
#else
uint dtstrhash(h,args,n)
reg uint	h;
Void_t*		args;
reg int		n;
#endif
{
	reg unsigned char*	s = (unsigned char*)args;

	if(n <= 0)
	{	for(; *s != 0; s += s[1] ? 2 : 1)
			h = (h + (s[0]<<8) + s[1])*DT_PRIME;
		n = s - (unsigned char*)args;
	}
	else
	{	reg unsigned char*	ends;
		for(ends = s+n-1; s < ends; s += 2)
			h = (h + (s[0]<<8) + s[1])*DT_PRIME;
		if(s <= ends)
			h = (h + (s[0]<<8))*DT_PRIME;
	}
	return (h+n)*DT_PRIME;
}
