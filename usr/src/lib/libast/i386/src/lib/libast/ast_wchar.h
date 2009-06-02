
/* : : generated by proto : : */
/* : : generated from /home/gisburn/ksh93/ast_ksh_20070418/build_i386_32bit/src/lib/libast/features/wchar by iffe version 2007-04-04 : : */
                  
#ifndef _def_wchar_ast
#if !defined(__PROTO__)
#  if defined(__STDC__) || defined(__cplusplus) || defined(_proto) || defined(c_plusplus)
#    if defined(__cplusplus)
#      define __LINKAGE__	"C"
#    else
#      define __LINKAGE__
#    endif
#    define __STDARG__
#    define __PROTO__(x)	x
#    define __OTORP__(x)
#    define __PARAM__(n,o)	n
#    if !defined(__STDC__) && !defined(__cplusplus)
#      if !defined(c_plusplus)
#      	define const
#      endif
#      define signed
#      define void		int
#      define volatile
#      define __V_		char
#    else
#      define __V_		void
#    endif
#  else
#    define __PROTO__(x)	()
#    define __OTORP__(x)	x
#    define __PARAM__(n,o)	o
#    define __LINKAGE__
#    define __V_		char
#    define const
#    define signed
#    define void		int
#    define volatile
#  endif
#  define __MANGLE__	__LINKAGE__
#  if defined(__cplusplus) || defined(c_plusplus)
#    define __VARARG__	...
#  else
#    define __VARARG__
#  endif
#  if defined(__STDARG__)
#    define __VA_START__(p,a)	va_start(p,a)
#  else
#    define __VA_START__(p,a)	va_start(p)
#  endif
#  if !defined(__INLINE__)
#    if defined(__cplusplus)
#      define __INLINE__	extern __MANGLE__ inline
#    else
#      if defined(_WIN32) && !defined(__GNUC__)
#      	define __INLINE__	__inline
#      endif
#    endif
#  endif
#endif
#if !defined(__LINKAGE__)
#define __LINKAGE__		/* 2004-08-11 transition */
#endif

#define _def_wchar_ast	1
#define _sys_types	1	/* #include <sys/types.h> ok */
#define _hdr_stdlib	1	/* #include <stdlib.h> ok */
#define _hdr_stdio	1	/* #include <stdio.h> ok */
#define _hdr_wchar	1	/* #include <wchar.h> ok */
#define _lib_mbstowcs	1	/* mbstowcs() in default lib(s) */
#define _lib_wctomb	1	/* wctomb() in default lib(s) */
#define _lib_wcrtomb	1	/* wcrtomb() in default lib(s) */
#define _lib_wcslen	1	/* wcslen() in default lib(s) */
#define _lib_wcstombs	1	/* wcstombs() in default lib(s) */
#define _lib_wcwidth	1	/* wcwidth() in default lib(s) */
#define _lib_towlower	1	/* towlower() in default lib(s) */
#define _lib_towupper	1	/* towupper() in default lib(s) */
#define _hdr_time	1	/* #include <time.h> ok */
#define _sys_time	1	/* #include <sys/time.h> ok */
#define _sys_times	1	/* #include <sys/times.h> ok */
#define _hdr_stddef	1	/* #include <stddef.h> ok */
#define _typ_mbstate_t	1	/* mbstate_t is a type */
#define _nxt_wchar <../include/wchar.h>	/* include path for the native <wchar.h> */
#define _nxt_wchar_str "../include/wchar.h"	/* include string for the native <wchar.h> */
#ifndef _SFSTDIO_H
#include <ast_common.h>
#include <stdio.h>
#endif
#if _hdr_wchar && defined(_nxt_wchar)
#include <../include/wchar.h>	/* the native wchar.h */
#endif
#if _hdr_wctype
#include <wctype.h>
#endif

#ifndef WEOF
#define WEOF		(-1)
#endif

#undef	fgetwc
#undef	fgetws
#undef	fputwc
#undef	fputws
#undef	getwc
#undef	getwchar
#undef	getws
#undef	putwc
#undef	putwchar
#undef	ungetwc

#define fgetwc		_ast_fgetwc
#define fgetws		_ast_fgetws
#define fputwc		_ast_fputwc
#define fputws		_ast_fputws
#define fwide		_ast_fwide
#define fwprintf	_ast_fwprintf
#define fwscanf		_ast_fwscanf
#define getwc		_ast_getwc
#define getwchar	_ast_getwchar
#define getws		_ast_getws
#define putwc		_ast_putwc
#define putwchar	_ast_putwchar
#define swprintf	_ast_swprintf
#define swscanf		_ast_swscanf
#define ungetwc		_ast_ungetwc
#define vfwprintf	_ast_vfwprintf
#define vfwscanf	_ast_vfwscanf
#define vswprintf	_ast_vswprintf
#define vswscanf	_ast_vswscanf
#define vwprintf	_ast_vwprintf
#define vwscanf		_ast_vwscanf
#define wprintf		_ast_wprintf
#define wscanf		_ast_wscanf

#if !_typ_mbstate_t
#undef	_typ_mbstate_t
#define _typ_mbstate_t	1
typedef char mbstate_t;
#endif

#if _BLD_ast && defined(__EXPORT__)
#undef __MANGLE__
#define __MANGLE__ __LINKAGE__		__EXPORT__
#endif

#if !_lib_mbstowcs
extern __MANGLE__ size_t		mbstowcs __PROTO__((wchar_t*, const char*, size_t));
#endif
#if !_lib_wctomb
extern __MANGLE__ int		wctomb __PROTO__((char*, wchar_t));
#endif
#if !_lib_wcrtomb
extern __MANGLE__ size_t		wcrtomb __PROTO__((char*, wchar_t, mbstate_t*));
#endif
#if !_lib_wcslen
extern __MANGLE__ size_t		wcslen __PROTO__((const wchar_t*));
#endif
#if !_lib_wcstombs
extern __MANGLE__ size_t		wcstombs __PROTO__((char*, const wchar_t*, size_t));
#endif

extern __MANGLE__ int		fwprintf __PROTO__((FILE*, const wchar_t*, ...));
extern __MANGLE__ int		fwscanf __PROTO__((FILE*, const wchar_t*, ...));
extern __MANGLE__ wint_t		fgetwc __PROTO__((FILE*));
extern __MANGLE__ wchar_t*		fgetws __PROTO__((wchar_t*, int, FILE*));
extern __MANGLE__ wint_t		fputwc __PROTO__((wchar_t, FILE*));
extern __MANGLE__ int		fputws __PROTO__((const wchar_t*, FILE*));
extern __MANGLE__ int		fwide __PROTO__((FILE*, int));
extern __MANGLE__ wint_t		getwc __PROTO__((FILE*));
extern __MANGLE__ wint_t		getwchar __PROTO__((void));
extern __MANGLE__ wchar_t*		getws __PROTO__((wchar_t*));
extern __MANGLE__ wint_t		putwc __PROTO__((wchar_t, FILE*));
extern __MANGLE__ wint_t		putwchar __PROTO__((wchar_t));
extern __MANGLE__ int		swprintf __PROTO__((wchar_t*, size_t, const wchar_t*, ...));
extern __MANGLE__ int		swscanf __PROTO__((const wchar_t*, const wchar_t*, ...));
extern __MANGLE__ wint_t		ungetwc __PROTO__((wint_t, FILE*));
extern __MANGLE__ int		vfwprintf __PROTO__((FILE*, const wchar_t*, va_list));
extern __MANGLE__ int		vfwscanf __PROTO__((FILE*, const wchar_t*, va_list));
extern __MANGLE__ int		vwprintf __PROTO__((const wchar_t*, va_list));
extern __MANGLE__ int		vwscanf __PROTO__((const wchar_t*, va_list));
extern __MANGLE__ int		vswprintf __PROTO__((wchar_t*, size_t, const wchar_t*, va_list));
extern __MANGLE__ int		vswscanf __PROTO__((const wchar_t*, const wchar_t*, va_list));
extern __MANGLE__ int		wprintf __PROTO__((const wchar_t*, ...));
extern __MANGLE__ int		wscanf __PROTO__((const wchar_t*, ...));

#undef __MANGLE__
#define __MANGLE__ __LINKAGE__
#endif
