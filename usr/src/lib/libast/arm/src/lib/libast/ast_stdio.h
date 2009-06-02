
/* : : generated by proto : : */
/* : : generated from /home/gisburn/ksh93/ast_ksh_20070418/build_i386_32bit/src/lib/libast/features/stdio by iffe version 2007-04-04 : : */
                  
#ifndef _SFSTDIO_H
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

#define _SFSTDIO_H	1
#define _sys_types	1	/* #include <sys/types.h> ok */
#define __FILE_typedef	1
#define _FILE_DEFINED	1
#define _FILE_defined	1
#define _FILEDEFED	1

#ifndef __FILE_TAG
#define __FILE_TAG	_sfio_s
#endif

#undef	FILE
#undef	_FILE
#undef	fpos_t
#undef	fpos64_t

typedef struct _sfio_s _sfio_FILE;

#define FILE		_sfio_FILE
#define _FILE		FILE

#if !defined(__FILE) && !__CYGWIN__
#undef	__FILE
#define __FILE		FILE
#endif

#if defined(_AST_H) || defined(_SFIO_H)

#define BUFSIZ		SF_BUFSIZE

#else

#ifndef BUFSIZ
#define BUFSIZ		8192
#endif

#ifndef EOF
#define EOF		(-1)
#endif

#ifndef NULL
#define NULL		0
#endif

#ifndef SEEK_SET
#define SEEK_SET	0
#define SEEK_CUR	1
#define SEEK_END	2
#endif

#include <ast_std.h>

#include <sfio_s.h>

#if __cplusplus
#define _sf_(f)		(f)
#else
#define _sf_(f)		((struct _sfio_s*)(f))
#endif

#define _SF_EOF		0000200
#define _SF_ERROR	0000400

#endif

#ifdef _NO_LARGEFILE64_SOURCE
#undef _LARGEFILE64_SOURCE
#endif

#ifdef _LARGEFILE64_SOURCE
#undef	off_t
#endif

#define fpos_t		_ast_fpos_t
#if _typ_int64_t
#define fpos64_t	_ast_fpos_t
#endif

typedef struct _ast_fpos_s
{
	intmax_t	_sf_offset;
	unsigned char	_sf_state[64 - sizeof(intmax_t)];
} _ast_fpos_t;

#define _base		_data
#define _ptr		_next
#define _IOFBF		0
#define _IONBF		1
#define _IOLBF		2

#if defined(__cplusplus) && defined(__THROW) && !defined(_UWIN)

#undef	FILE
#define FILE            FILE
typedef struct _sfio_s FILE;

#undef	strerror
extern __MANGLE__ char*	strerror(int) __THROW;

extern __MANGLE__ int	_doprnt __PROTO__((const char*, va_list, FILE*));
extern __MANGLE__ int	_doscan __PROTO__((FILE*, const char*, va_list));
extern __MANGLE__ int	asprintf __PROTO__((char**, const char*, ...));
extern __MANGLE__ int	clearerr __PROTO__((FILE*));
extern __MANGLE__ int	fclose __PROTO__((FILE*));
extern __MANGLE__ FILE*	fdopen __PROTO__((int, const char*));
extern __MANGLE__ int	feof __PROTO__((FILE*));
extern __MANGLE__ int	ferror __PROTO__((FILE*));
extern __MANGLE__ int	fflush __PROTO__((FILE*));
extern __MANGLE__ int	fgetc __PROTO__((FILE*));
extern __MANGLE__ int	fgetpos __PROTO__((FILE*, fpos_t*));
extern __MANGLE__ char*	fgets __PROTO__((char*, int, FILE*));
extern __MANGLE__ int	fileno __PROTO__((FILE*));
extern __MANGLE__ FILE*	fopen __PROTO__((const char*, const char*));
extern __MANGLE__ int	fprintf __PROTO__((FILE*, const char*, ...));
extern __MANGLE__ int	fpurge __PROTO__((FILE*));
extern __MANGLE__ int	fputc __PROTO__((int, FILE*));
extern __MANGLE__ int	fputs __PROTO__((const char*, FILE*));
extern __MANGLE__ ssize_t	fread __PROTO__((__V_*, size_t, size_t, FILE*));
extern __MANGLE__ FILE*	freopen __PROTO__((const char*, const char*, FILE*));
extern __MANGLE__ int	fscanf __PROTO__((FILE*, const char*, ...));
extern __MANGLE__ int	fseek __PROTO__((FILE*, long, int));
extern __MANGLE__ int	fseeko __PROTO__((FILE*, off_t, int));
extern __MANGLE__ int	fsetpos __PROTO__((FILE*, const fpos_t*));
extern __MANGLE__ long	ftell __PROTO__((FILE*));
extern __MANGLE__ off_t	ftello __PROTO__((FILE*));
extern __MANGLE__ ssize_t	fwrite __PROTO__((const __V_*, size_t, size_t, FILE*));
extern __MANGLE__ int	getc __PROTO__((FILE*));
extern __MANGLE__ int	getchar __PROTO__((void));
extern __MANGLE__ char*	gets __PROTO__((char*));
extern __MANGLE__ int	getw __PROTO__((FILE*));
extern __MANGLE__ int	pclose __PROTO__((FILE*));
extern __MANGLE__ FILE*	popen __PROTO__((const char*, const char*));
extern __MANGLE__ int	printf __PROTO__((const char*, ...));
extern __MANGLE__ int	putc __PROTO__((int, FILE*));
extern __MANGLE__ int	putchar __PROTO__((int));
extern __MANGLE__ int	puts __PROTO__((const char*));
extern __MANGLE__ int	putw __PROTO__((int, FILE*));
extern __MANGLE__ void	rewind __PROTO__((FILE*));
extern __MANGLE__ int	scanf __PROTO__((const char*, ...));
extern __MANGLE__ void	setbuf __PROTO__((FILE*, char*));
extern __MANGLE__ int	setbuffer __PROTO__((FILE*, char*, int));
extern __MANGLE__ int	setlinebuf __PROTO__((FILE*));
extern __MANGLE__ int	setvbuf __PROTO__((FILE*, char*, int, size_t));
extern __MANGLE__ int	snprintf __PROTO__((char*, int, const char*, ...));
extern __MANGLE__ int	sprintf __PROTO__((char*, const char*, ...));
extern __MANGLE__ int	sscanf __PROTO__((const char*, const char*, ...));
extern __MANGLE__ FILE*	tmpfile __PROTO__((void));
extern __MANGLE__ int	ungetc __PROTO__((int, FILE*));
extern __MANGLE__ int	vasprintf __PROTO__((char**, const char*, va_list));
extern __MANGLE__ int	vfprintf __PROTO__((FILE*, const char*, va_list));
extern __MANGLE__ int	vfscanf __PROTO__((FILE*, const char*, va_list));
extern __MANGLE__ int	vprintf __PROTO__((const char*, va_list));
extern __MANGLE__ int	vscanf __PROTO__((const char*, va_list));
extern __MANGLE__ int	vsnprintf __PROTO__((char*, int, const char*, va_list));
extern __MANGLE__ int	vsprintf __PROTO__((char*, const char*, va_list));
extern __MANGLE__ int	vsscanf __PROTO__((const char*, const char*, va_list));

#if _typ_int64_t

extern __MANGLE__ int		fgetpos64 __PROTO__((FILE*, fpos64_t*));
extern __MANGLE__ int		fsetpos64 __PROTO__((FILE*, const fpos64_t*));
extern __MANGLE__ int		fseek64 __PROTO__((FILE*, int64_t, int));
extern __MANGLE__ int		fseeko64 __PROTO__((FILE*, int64_t, int));
extern __MANGLE__ int64_t		ftell64 __PROTO__((FILE*));
extern __MANGLE__ int64_t		ftello64 __PROTO__((FILE*));

#endif

extern __MANGLE__ void	clearerr_unlocked __PROTO__((FILE*));
extern __MANGLE__ int	feof_unlocked __PROTO__((FILE*));
extern __MANGLE__ int	ferror_unlocked __PROTO__((FILE*));
extern __MANGLE__ int	fflush_unlocked __PROTO__((FILE*));
extern __MANGLE__ int	fgetc_unlocked __PROTO__((FILE*));
extern __MANGLE__ char*	fgets_unlocked __PROTO__((char*, int, FILE*));
extern __MANGLE__ int	fileno_unlocked __PROTO__((FILE*));
extern __MANGLE__ int	fputc_unlocked __PROTO__((int, FILE*));
extern __MANGLE__ int	fputs_unlocked __PROTO__((char*, FILE*));
extern __MANGLE__ size_t	fread_unlocked __PROTO__((__V_*, size_t, size_t, FILE*));
extern __MANGLE__ size_t	fwrite_unlocked __PROTO__((__V_*, size_t, size_t, FILE*));
extern __MANGLE__ int	getc_unlocked __PROTO__((FILE*));
extern __MANGLE__ int	getchar_unlocked __PROTO__((void));
extern __MANGLE__ int	putc_unlocked __PROTO__((int, FILE*));
extern __MANGLE__ int	putchar_unlocked __PROTO__((int));

#ifdef _USE_GNU

extern __MANGLE__ int	fcloseall __PROTO__((void));
extern __MANGLE__ FILE*	fmemopen __PROTO__((__V_*, size_t, const char*));
extern __MANGLE__ ssize_t	__getdelim __PROTO__((char**, size_t*, int, FILE*));
extern __MANGLE__ ssize_t	getdelim __PROTO__((char**, size_t*, int, FILE*));
extern __MANGLE__ ssize_t	getline __PROTO__((char**, size_t*, FILE*));

#endif

#endif

#ifndef FILENAME_MAX
#define FILENAME_MAX	1024
#endif
#ifndef FOPEN_MAX
#define FOPEN_MAX	20
#endif
#ifndef TMP_MAX
#define TMP_MAX		17576
#endif

#define _doprnt		_ast_doprnt
#define _doscan		_ast_doscan
#define asprintf	_ast_asprintf
#define clearerr	_ast_clearerr
#define fclose		_ast_fclose
#define fdopen		_ast_fdopen
#define fflush		_ast_fflush
#define fgetc		_ast_fgetc
#define fgetpos		_ast_fgetpos
#define fgetpos64	_ast_fgetpos64
#define fgets		_ast_fgets
#define fopen		_ast_fopen
#define fprintf		_ast_fprintf
#define fpurge		_ast_fpurge
#define fputs		_ast_fputs
#define fread		_ast_fread
#define freopen		_ast_freopen
#define fscanf		_ast_fscanf
#define fseek		_ast_fseek
#define fseek64		_ast_fseek64
#define fseeko		_ast_fseeko
#define fseeko64	_ast_fseeko64
#define fsetpos		_ast_fsetpos
#define fsetpos64	_ast_fsetpos64
#define ftell		_ast_ftell
#define ftell64		_ast_ftell64
#define ftello		_ast_ftello
#define ftello64	_ast_ftello64
#define fwrite		_ast_fwrite
#define gets		_ast_gets
#define getw		_ast_getw
#define pclose		_ast_pclose
#define popen		_ast_popen
#define printf		_ast_printf
#define puts		_ast_puts
#define putw		_ast_putw
#define rewind		_ast_rewind
#define scanf		_ast_scanf
#define setbuf		_ast_setbuf
#undef	setbuffer
#define setbuffer	_ast_setbuffer
#define setlinebuf	_ast_setlinebuf
#define setvbuf		_ast_setvbuf
#define snprintf	_ast_snprintf
#define sprintf		_ast_sprintf
#define sscanf		_ast_sscanf
#define tmpfile		_ast_tmpfile
#define ungetc		_ast_ungetc
#define vasprintf	_ast_vasprintf
#define vfprintf	_ast_vfprintf
#define vfscanf		_ast_vfscanf
#define vprintf		_ast_vprintf
#define vscanf		_ast_vscanf
#define vsnprintf	_ast_vsnprintf
#define vsprintf	_ast_vsprintf
#define vsscanf		_ast_vsscanf
#define fcloseall	_ast_fcloseall
#define fmemopen	_ast_fmemopen
#define __getdelim	_ast___getdelim
#define getdelim	_ast_getdelim
#define getline		_ast_getline
#define clearerr_unlocked _ast_clearerr_unlocked
#define feof_unlocked	_ast_feof_unlocked
#define ferror_unlocked	_ast_ferror_unlocked
#define fflush_unlocked	_ast_fflush_unlocked
#define fgetc_unlocked	_ast_fgetc_unlocked
#define fgets_unlocked	_ast_fgets_unlocked
#define fileno_unlocked	_ast_fileno_unlocked
#define fputc_unlocked	_ast_fputc_unlocked
#define fputs_unlocked	_ast_fputs_unlocked
#define fread_unlocked	_ast_fread_unlocked
#define fwrite_unlocked	_ast_fwrite_unlocked
#define getc_unlocked	_ast_getc_unlocked
#define getchar_unlocked _ast_getchar_unlocked
#define putc_unlocked	_ast_putc_unlocked
#define putchar_unlocked _ast_putchar_unlocked

#if defined(__STDPP__directive) && defined(__STDPP__initial)
__STDPP__directive pragma pp:initial
#endif
#ifndef P_tmpdir
#define P_tmpdir  "/var/tmp/" /*NOCATLITERAL*/
#endif
#ifndef L_ctermid
#define L_ctermid  9
#endif
#ifndef L_tmpnam
#define L_tmpnam  25
#endif
#if defined(__STDPP__directive) && defined(__STDPP__initial)
__STDPP__directive pragma pp:noinitial
#endif
#if defined(__cplusplus) && defined(__THROW)
extern __MANGLE__ char*	ctermid(char*) __THROW;
#else
extern __MANGLE__ char*	ctermid __PROTO__((char*));
#endif
extern __MANGLE__ char*	tmpnam __PROTO__((char*));
extern __MANGLE__ char*	tempnam __PROTO__((const char*, const char*));
extern __MANGLE__ void	perror __PROTO__((const char*));
#ifndef _AST_STD_H
#ifndef remove
extern __MANGLE__ int	remove __PROTO__((const char*));
#endif
#ifndef rename
extern __MANGLE__ int	rename __PROTO__((const char*, const char*));
#endif
#endif

#undef __MANGLE__
#define __MANGLE__ __LINKAGE__

#if _BLD_ast && defined(__EXPORT__)
#undef __MANGLE__
#define __MANGLE__ __LINKAGE__		__EXPORT__
#endif

extern __MANGLE__ int	_doprnt __PROTO__((const char*, va_list, FILE*));
extern __MANGLE__ int	_doscan __PROTO__((FILE*, const char*, va_list));
extern __MANGLE__ int	asprintf __PROTO__((char**, const char*, ...));
extern __MANGLE__ int	clearerr __PROTO__((FILE*));
extern __MANGLE__ int	fclose __PROTO__((FILE*));
extern __MANGLE__ FILE*	fdopen __PROTO__((int, const char*));
extern __MANGLE__ int	feof __PROTO__((FILE*));
extern __MANGLE__ int	ferror __PROTO__((FILE*));
extern __MANGLE__ int	fflush __PROTO__((FILE*));
extern __MANGLE__ int	fgetc __PROTO__((FILE*));
extern __MANGLE__ int	fgetpos __PROTO__((FILE*, fpos_t*));
extern __MANGLE__ char*	fgets __PROTO__((char*, int, FILE*));
extern __MANGLE__ int	fileno __PROTO__((FILE*));
extern __MANGLE__ FILE*	fopen __PROTO__((const char*, const char*));
extern __MANGLE__ int	fprintf __PROTO__((FILE*, const char*, ...));
extern __MANGLE__ int	fpurge __PROTO__((FILE*));
extern __MANGLE__ int	fputc __PROTO__((int, FILE*));
extern __MANGLE__ int	fputs __PROTO__((const char*, FILE*));
extern __MANGLE__ ssize_t	fread __PROTO__((__V_*, size_t, size_t, FILE*));
extern __MANGLE__ FILE*	freopen __PROTO__((const char*, const char*, FILE*));
extern __MANGLE__ int	fscanf __PROTO__((FILE*, const char*, ...));
extern __MANGLE__ int	fseek __PROTO__((FILE*, long, int));
extern __MANGLE__ int	fseeko __PROTO__((FILE*, off_t, int));
extern __MANGLE__ int	fsetpos __PROTO__((FILE*, const fpos_t*));
extern __MANGLE__ long	ftell __PROTO__((FILE*));
extern __MANGLE__ off_t	ftello __PROTO__((FILE*));
extern __MANGLE__ ssize_t	fwrite __PROTO__((const __V_*, size_t, size_t, FILE*));
extern __MANGLE__ int	getc __PROTO__((FILE*));
extern __MANGLE__ int	getchar __PROTO__((void));
extern __MANGLE__ char*	gets __PROTO__((char*));
extern __MANGLE__ int	getw __PROTO__((FILE*));
extern __MANGLE__ int	pclose __PROTO__((FILE*));
extern __MANGLE__ FILE*	popen __PROTO__((const char*, const char*));
extern __MANGLE__ int	printf __PROTO__((const char*, ...));
extern __MANGLE__ int	putc __PROTO__((int, FILE*));
extern __MANGLE__ int	putchar __PROTO__((int));
extern __MANGLE__ int	puts __PROTO__((const char*));
extern __MANGLE__ int	putw __PROTO__((int, FILE*));
extern __MANGLE__ void	rewind __PROTO__((FILE*));
extern __MANGLE__ int	scanf __PROTO__((const char*, ...));
extern __MANGLE__ void	setbuf __PROTO__((FILE*, char*));
extern __MANGLE__ int	setbuffer __PROTO__((FILE*, char*, int));
extern __MANGLE__ int	setlinebuf __PROTO__((FILE*));
extern __MANGLE__ int	setvbuf __PROTO__((FILE*, char*, int, size_t));
extern __MANGLE__ int	snprintf __PROTO__((char*, int, const char*, ...));
extern __MANGLE__ int	sprintf __PROTO__((char*, const char*, ...));
extern __MANGLE__ int	sscanf __PROTO__((const char*, const char*, ...));
extern __MANGLE__ FILE*	tmpfile __PROTO__((void));
extern __MANGLE__ int	ungetc __PROTO__((int, FILE*));
extern __MANGLE__ int	vasprintf __PROTO__((char**, const char*, va_list));
extern __MANGLE__ int	vfprintf __PROTO__((FILE*, const char*, va_list));
extern __MANGLE__ int	vfscanf __PROTO__((FILE*, const char*, va_list));
extern __MANGLE__ int	vprintf __PROTO__((const char*, va_list));
extern __MANGLE__ int	vscanf __PROTO__((const char*, va_list));
extern __MANGLE__ int	vsnprintf __PROTO__((char*, int, const char*, va_list));
extern __MANGLE__ int	vsprintf __PROTO__((char*, const char*, va_list));
extern __MANGLE__ int	vsscanf __PROTO__((const char*, const char*, va_list));

#if _typ_int64_t

extern __MANGLE__ int		fgetpos64 __PROTO__((FILE*, fpos64_t*));
extern __MANGLE__ int		fsetpos64 __PROTO__((FILE*, const fpos64_t*));
extern __MANGLE__ int		fseek64 __PROTO__((FILE*, int64_t, int));
extern __MANGLE__ int		fseeko64 __PROTO__((FILE*, int64_t, int));
extern __MANGLE__ int64_t		ftell64 __PROTO__((FILE*));
extern __MANGLE__ int64_t		ftello64 __PROTO__((FILE*));

#ifdef _LARGEFILE64_SOURCE

#undef	fpos_t
#undef	off_t
#undef	fgetpos
#undef	fsetpos
#undef	fseek
#undef	fseeko
#undef	ftell
#undef	ftello

#define	fpos_t		fpos64_t
#if _typ_off64_t
#define	off_t		off64_t
#else
#define	off_t		int64_t
#endif

#define fgetpos		fgetpos64
#define fsetpos		fsetpos64
#define	fseek		fseek64
#define	fseeko		fseeko64
#define ftell		ftell64
#define ftello		ftello64

#endif

#endif

extern __MANGLE__ void	clearerr_unlocked __PROTO__((FILE*));
extern __MANGLE__ int	feof_unlocked __PROTO__((FILE*));
extern __MANGLE__ int	ferror_unlocked __PROTO__((FILE*));
extern __MANGLE__ int	fflush_unlocked __PROTO__((FILE*));
extern __MANGLE__ int	fgetc_unlocked __PROTO__((FILE*));
extern __MANGLE__ char*	fgets_unlocked __PROTO__((char*, int, FILE*));
extern __MANGLE__ int	fileno_unlocked __PROTO__((FILE*));
extern __MANGLE__ int	fputc_unlocked __PROTO__((int, FILE*));
extern __MANGLE__ int	fputs_unlocked __PROTO__((char*, FILE*));
extern __MANGLE__ size_t	fread_unlocked __PROTO__((__V_*, size_t, size_t, FILE*));
extern __MANGLE__ size_t	fwrite_unlocked __PROTO__((__V_*, size_t, size_t, FILE*));
extern __MANGLE__ int	getc_unlocked __PROTO__((FILE*));
extern __MANGLE__ int	getchar_unlocked __PROTO__((void));
extern __MANGLE__ int	putc_unlocked __PROTO__((int, FILE*));
extern __MANGLE__ int	putchar_unlocked __PROTO__((int));

#ifdef _USE_GNU

extern __MANGLE__ int	fcloseall __PROTO__((void));
extern __MANGLE__ FILE*	fmemopen __PROTO__((__V_*, size_t, const char*));
extern __MANGLE__ ssize_t	__getdelim __PROTO__((char**, size_t*, int, FILE*));
extern __MANGLE__ ssize_t	getdelim __PROTO__((char**, size_t*, int, FILE*));
extern __MANGLE__ ssize_t	getline __PROTO__((char**, size_t*, FILE*));

#endif

#undef __MANGLE__
#define __MANGLE__ __LINKAGE__

#if _BLD_DLL && _DLL_INDIRECT_DATA

#define stdin		((FILE*)_ast_dll->_ast_stdin)
#define stdout		((FILE*)_ast_dll->_ast_stdout)
#define stderr		((FILE*)_ast_dll->_ast_stderr)

#else

#define	stdin		(&_Sfstdin)
#define	stdout		(&_Sfstdout)
#define	stderr		(&_Sfstderr)

#endif

#if defined(_AST_H) || defined(_SFIO_H)

#define feof(f)		sfeof(f)
#define ferror(f)	sferror(f)
#define fileno(f)	sffileno(f)
#define fputc(c,f)	sfputc(f,c)
#define getc(f)		sfgetc(f)
#define getchar()	sfgetc(sfstdin)
#define putc(c,f)	sfputc(f,c)
#define putchar(c)	sfputc(sfstdout,c)

#else

#if !_UWIN
#if _BLD_ast && defined(__EXPORT__)
#undef __MANGLE__
#define __MANGLE__ __LINKAGE__ __EXPORT__
#endif
#if !_BLD_ast && defined(__IMPORT__)
#undef __MANGLE__
#define __MANGLE__ __LINKAGE__ __IMPORT__
#endif
#endif

extern __MANGLE__ FILE	_Sfstdin;
extern __MANGLE__ FILE	_Sfstdout;
extern __MANGLE__ FILE	_Sfstderr;

#undef __MANGLE__
#define __MANGLE__ __LINKAGE__

#define feof(f)		(_sf_(f)->_flags&_SF_EOF)
#define ferror(f)	(_sf_(f)->_flags&_SF_ERROR)
#define fileno(f)	(_sf_(f)->_file)
#define fputc(c,f)	(_sf_(f)->_next>=_sf_(f)->_endw?_sfflsbuf(_sf_(f),(int)((unsigned char)(c))):(int)(*_sf_(f)->_next++=(unsigned char)(c)))
#define getc(f)		(_sf_(f)->_next>=_sf_(f)->_endr?_sffilbuf(_sf_(f),0):(int)(*_sf_(f)->_next++))
#define getchar()	getc(stdin)
#define putc(c,f)	fputc(c,f)
#define putchar(c)	fputc(c,stdout)

#if _BLD_ast && defined(__EXPORT__)
#undef __MANGLE__
#define __MANGLE__ __LINKAGE__		__EXPORT__
#endif

extern __MANGLE__ int		_sffilbuf __PROTO__((FILE*, int));
extern __MANGLE__ int		_sfflsbuf __PROTO__((FILE*, int));

#undef __MANGLE__
#define __MANGLE__ __LINKAGE__

#endif

#endif
