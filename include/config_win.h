#ifndef CONFIG_H
#define CONFIG_H seen

#if defined(WIN32)
/*XXX
  This is temporary (atleast until a
  config.h is auto-generated on windows)
*/
#define __STDC__ 1

#pragma warning( disable : 4244 4305 4018)
/*  4244 conversion from type 1 to type 2 
    4305 truncation
	4018 signed/unsigned mismatch
*/
/* Microsoft VisualC++ 6.0 on WinNT 4.0 */
/*
 * Ensure that Project Settings / Project Options includes
 *	/Za		for ANSI C
 *	/D "WIN32"	to fire this rule
 */
# if !__STDC__
#	error "STDC is not properly set on WIN32 build, add /Za to Project Settings / Project Options"
# endif

#define HAVE_STDLIB_H	1
#define HAVE_STDARG_H	1
#define HAVE_STRING_H	1
#define HAS_OPENGL	1
#define HAVE_DRAND48	1
#define HAVE_FLOAT_H	1
#define HAVE_GETHOSTNAME	1
#define HAVE_LIMITS_H	1
#define HAVE_MATHERR	1
#define HAVE_MEMORY_H	1
#define HAVE_OFF_T	1
#define HAVE_POPEN_DECL	1
#define HAVE_STRCHR	1
#define HAVE_STRDUP	1
#define HAVE_STRDUP_DECL	1
#define HAVE_VARARGS_H	1
#define HAVE_VFORK	1
#define HAVE_VPRINTF	1
#define USE_PROTOTYPES	1
#define HAVE_STRING_H	1
#define HAVE_REGEX_H	1
#define HAVE_STRCHR 1
#define HAVE_TIME 1
#define HAVE_SYS_TIME 1
#define bzero(str,n)		memset( str, 0, n )
#define bcopy(from,to,count)	memcpy( to, from, count )
#define REVERSED_IEEE	yes

#include <windows.h>
#include <io.h>

#if 0
#	define HAVE_STDLIB_H	1
#	define HAVE_STDARG_H	1
#	define HAVE_STRING_H	1
#endif
#define stat _stat
#define hypot _hypot
#define fdopen _fdopen
#define off_t _off_t
#undef rad1
#undef rad2
#define isascii __isascii
#define chmod _chmod
#define popen _popen
#define open _open
#define read _read
#define close _close
#define fstat _fstat
#define O_RDONLY _O_RDONLY
#define O_RDWR _O_RDWR
#define O_CREAT _O_CREAT
#define O_EXCL _O_EXCL
#define MAXPATHLEN _MAX_PATH
#define fileno _fileno
#define write _write
#define getpid _getpid
#define access _access
#define pclose _pclose
#define strdup _strdup
#define	isnan _isnan
#define isatty _isatty
#define creat _creat
#define mktemp _mktemp
#undef OUT
#undef IN
#undef DELETE
#undef complex

#endif /* if defined(WIN32) */

#endif /* CONFIG_H */
