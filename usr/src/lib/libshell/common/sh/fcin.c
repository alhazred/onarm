/***********************************************************************
*                                                                      *
*               This software is part of the ast package               *
*           Copyright (c) 1982-2007 AT&T Knowledge Ventures            *
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
*                  David Korn <dgk@research.att.com>                   *
*                                                                      *
***********************************************************************/
#pragma prototyped
/*
 *   Routines to implement fast character input
 *
 *   David Korn
 *   AT&T Labs
 *
 */

#include	<ast.h>
#include	<sfio.h>
#include	<error.h>
#include	<fcin.h>

Fcin_t _Fcin = {0};

/*
 * open stream <f> for fast character input
 */
int	fcfopen(register Sfio_t* f)
{
	register int	n;
	char		*buff;
	Fcin_t		save;
	errno = 0;
	_Fcin.fcbuff = _Fcin.fcptr;
	_Fcin._fcfile = f;
	fcsave(&save);
	if(!(buff=(char*)sfreserve(f,SF_UNBOUND,SF_LOCKR)))
	{
		fcrestore(&save);
		_Fcin.fcchar = 0;
		_Fcin.fcptr = _Fcin.fcbuff = &_Fcin.fcchar;
		_Fcin.fclast = 0;
		_Fcin._fcfile = (Sfio_t*)0;
		return(EOF);
	}
	n = sfvalue(f);
	fcrestore(&save);
	sfread(f,buff,0);
	_Fcin.fcoff = sftell(f);;
	buff = (char*)sfreserve(f,SF_UNBOUND,SF_LOCKR);
	_Fcin.fclast = (_Fcin.fcptr=_Fcin.fcbuff=(unsigned char*)buff)+n;
	if(sffileno(f) >= 0)
		*_Fcin.fclast = 0;
	return(n);
}


/*
 * With _Fcin.fcptr>_Fcin.fcbuff, the stream pointer is advanced and
 * If _Fcin.fclast!=0, performs an sfreserve() for the next buffer.
 * If a notify function has been set, it is called
 * If last is non-zero, and the stream is a file, 0 is returned when
 * the previous character is a 0 byte.
 */
int	fcfill(void)
{
	register int		n;
	register Sfio_t	*f;
	register unsigned char	*last=_Fcin.fclast, *ptr=_Fcin.fcptr;
	if(!(f=fcfile()))
	{
		/* see whether pointer has passed null byte */
		if(ptr>_Fcin.fcbuff && *--ptr==0)
			_Fcin.fcptr=ptr;
		else
			_Fcin.fcoff = 0;
		return(0);
	}
	if(last)
	{
		if( ptr<last && ptr>_Fcin.fcbuff && *(ptr-1)==0)
			return(0);
		if(_Fcin.fcchar)
			*last = _Fcin.fcchar;
		if(ptr > last)
			_Fcin.fcptr = ptr = last;
	}
	if((n = ptr-_Fcin.fcbuff) && _Fcin.fcfun)
		(*_Fcin.fcfun)(f,(const char*)_Fcin.fcbuff,n);
	sfread(f, (char*)_Fcin.fcbuff, n);
	_Fcin.fcoff +=n;
	_Fcin._fcfile = 0;
	if(!last)
		return(0);
	else if(fcfopen(f) < 0)
		return(EOF);
	return(*_Fcin.fcptr++);
}

/*
 * Synchronize and close the current stream
 */
int fcclose(void)
{
	register unsigned char *ptr;
	if(_Fcin.fclast==0)
		return(0);
	if((ptr=_Fcin.fcptr)>_Fcin.fcbuff && *(ptr-1)==0)
		_Fcin.fcptr--;
	if(_Fcin.fcchar)
		*_Fcin.fclast = _Fcin.fcchar;
	_Fcin.fclast = 0;
	_Fcin.fcleft = 0;
	return(fcfill());
}

/*
 * Set the notify function that is called for each fcfill()
 */
void fcnotify(void (*fun)(Sfio_t*,const char*,int))
{
	_Fcin.fcfun = fun;
}

#ifdef __EXPORT__
#   define extern __EXPORT__
#endif

#undef fcsave
extern void fcsave(Fcin_t *fp)
{
	*fp = _Fcin;
}

#undef fcrestore
extern void fcrestore(Fcin_t *fp)
{
	_Fcin = *fp;
}

struct Extra
{
	unsigned char	buff[2*MB_LEN_MAX];
	unsigned char	*next;
};

int fcmbstate(const char *state, int *s, int *len)
{
	static struct Extra	extra;
	register int		i, c, n;
	if(_Fcin.fcleft)
	{
		if((c = mbsize(extra.next)) < 0)
			c = 1;
		if((_Fcin.fcleft -= c) <=0)
		{
			_Fcin.fcptr = (unsigned char*)fcfirst() - _Fcin.fcleft; 
			_Fcin.fcleft = 0;
		}
		*len = c;
		if(c==1)
			*s = state[*extra.next++];
		else if(c==0)
			_Fcin.fcleft = 0;
		else
		{
			c = mbchar(extra.next);
			*s = state['a'];
		}
		return(c);
	}
	switch(*len = mbsize(_Fcin.fcptr))
	{
	    case -1:
		if(_Fcin._fcfile && (n=(_Fcin.fclast-_Fcin.fcptr)) < MB_LEN_MAX)
		{
			memcmp(extra.buff, _Fcin.fcptr, n);
			_Fcin.fcptr = _Fcin.fclast;
			for(i=n; i < MB_LEN_MAX+n; i++)
			{
				if((extra.buff[i] = fcgetc(c))==0)
					break;
			}
			_Fcin.fcleft = n;
			extra.next = extra.buff;
			return(fcmbstate(state,s,len));
		}
		*len = 1;
		/* fall through */
	    case 0:
	    case 1:
		*s = state[c=fcget()];
		break;
	    default:
		c = mbchar(_Fcin.fcptr);
		*s = state['a'];
	}
	return(c);
} 

