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
#include	"sfhdr.h"

/*	Read a record delineated by a character.
**	The record length can be accessed via sfvalue(f).
**
**	Written by Kiem-Phong Vo
*/

#if __STD_C
char* sfgetr(reg Sfio_t *f, reg int rc, int type)
#else
char* sfgetr(f,rc,type)
reg Sfio_t*	f;	/* stream to read from	*/
reg int		rc;	/* record separator	*/
int		type;
#endif
{
	reg ssize_t	n, un;
	reg uchar	*s, *ends, *us;
	reg int		found;
	reg Sfrsrv_t*	rsrv;

	SFMTXSTART(f, NIL(char*));

	if(rc < 0 || (f->mode != SF_READ && _sfmode(f,SF_READ,0) < 0) )
		SFMTXRETURN(f, NIL(char*));
	SFLOCK(f,0);

	/* buffer to be returned */
	rsrv = NIL(Sfrsrv_t*);
	us = NIL(uchar*);
	un = 0;
	found = 0;

	/* compatibility mode */
	type = type < 0 ? SF_LASTR : type == 1 ? SF_STRING : type;

	if(type&SF_LASTR) /* return the broken record */
	{	if((rsrv = f->rsrv) && (un = -rsrv->slen) > 0)
		{	us = rsrv->data;
			found = 1;
		}
		goto done;
	}

	while(!found)
	{	/* fill buffer if necessary */
		if((n = (ends = f->endb) - (s = f->next)) <= 0)
		{	/* for unseekable devices, peek-read 1 record */
			f->getr = rc;
			f->mode |= SF_RC;

			/* fill buffer the conventional way */
			if(SFRPEEK(f,s,n) <= 0)
			{	us = NIL(uchar*);
				goto done;
			}
			else
			{	ends = s+n;
				if(f->mode&SF_RC)
				{	s = ends[-1] == rc ? ends-1 : ends;
					goto do_copy;
				}
			}
		}

#if _lib_memchr
		if(!(s = (uchar*)memchr((char*)s,rc,n)))
			s = ends;
#else
		while(*s != rc)
			if((s += 1) == ends)
				break;
#endif
	do_copy:
		if(s < ends) /* found separator */
		{	s += 1;		/* include the separator */
			found = 1;

			if(!us &&
			   (!(type&SF_STRING) || !(f->flags&SF_STRING) ||
			    ((f->flags&SF_STRING) && (f->bits&SF_BOTH) ) ) )
			{	/* returning data in buffer */
				us = f->next;
				un = s - f->next;
				f->next = s;
				goto done;
			}
		}

		/* amount to be read */
		n = s - f->next;

		if(!found && _Sfmaxr > 0 && un+n+1 >= _Sfmaxr) /* already exceed limit */
		{	us = NIL(uchar*);
			goto done;
		}

		/* get internal buffer */
		if(!rsrv || rsrv->size < un+n+1)
		{	if(rsrv)
				rsrv->slen = un;
			if((rsrv = _sfrsrv(f,un+n+1)) != NIL(Sfrsrv_t*))
				us = rsrv->data;
			else
			{	us = NIL(uchar*);
				goto done;
			}
		}

		/* now copy data */
		s = us+un;
		un += n;
		ends = f->next;
		f->next += n;
		MEMCPY(s,ends,n);
	}

done:
	_Sfi = f->val = un;
	f->getr = 0;
	if(found && rc != 0 && (type&SF_STRING) )
	{	us[un-1] = '\0';
		if(us >= f->data && us < f->endb)
		{	f->getr = rc;
			f->mode |= SF_GETR;
		}
	}

	/* prepare for a call to get the broken record */
	if(rsrv)
		rsrv->slen = found ? 0 : -un;

	SFOPEN(f,0);

	if(us && (type&SF_LOCKR) )
	{	f->mode |= SF_PEEK|SF_GETR;
		f->endr = f->data;
	}

	SFMTXRETURN(f, (char*)us);
}
