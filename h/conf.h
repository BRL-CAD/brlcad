#ifndef CONF_H
#define CONF_H seen

/*
 *			C O N F . H
 *
 * Should be included before any system headers are included (it tells us
 * which are available.)
 *
 * Right now, this include file has predetermined answers for every define.
 * Someday, it will be automatically generated.
 *
 *	PRODUCTION
 *		When defined with -D build option, this will enable mods to
 *		parts of code that are gratuitous in a production 
 *		environment.  For example, magic number checking is removed
 *		as well as other macros that bu_bomb on a fatal error.  Note
 *		that this option should not be used if remote debugging is
 *		potentially desired.
 *		
 *		If defined, other flags are defined to do specific
 *		optimizations.
 *		
 *		Another note, this assumes a bug-free build and should not
 *		be set unless bug-free confidence is high.
 *		
 *
 *	USE_PROTOTYPES -
 *		When defined, this compiler will accept ANSI-C function
 *		prototypes, even if it isn't a full ANSI compiler.
 *		Used mostly to build more sophisticated macros.
 *
 *	USE_STRING_H -
 *		When defined, use <string.h>, not <strings.h>
 *
 *	HAVE_STDLIB_H		Most __STDC__ systems have
 *	HAVE_STDARG_H		these.
 *
 *	HAVE_VARARGS_H		Most systems have this.  (!CRAY)?
 *	
 *	HAVE_STRING_H		Usually the preferred include (for strdup and
 *				strtok).  Historically, SYSV territory, but
 *				nowadays it looks like everyone's got it.
 *	HAVE_GETOPT_H
 *
 *	HAVE_XOSDEFS_H		Has <X11/Xosdefs.h> and <X11/Xfuncproto.h>
 *
 *	USE_SYS_TIME_H		XXX Unnecessary
 *
 *	USE_STRING_H		If your string.h is sufficient
 *
 *	HAVE_DRAND48		Most everyone? except Convex
 *
 *	HAVE_GETHOSTNAME	A BSD thang.
 *
 *	HAVE_GETOPT
 *
 *	HAVE_MATHERR		XXX ?
 *
 *	HAVE_REGEX
 *
 *	HAVE_POSIX_REGEXEC	POSIX regexec(), regcomp(), regfree(), regerror(), and regex.h
 *
 *	USE_REGCOMP             Use system regcomp() and regexec()
 *
 *	HAVE_STRCHR		Usually for folks who have strings.h
 *
 *	HAVE_VFORK		BSD, non-SYSV systems.
 *
 *	HAVE_VPRINTF		Modern (ANSI) systems.
 *
 *	HAVE_WRITEV
 *
 *	HAVE_SHELL_ESCAPE	Typically UNIX-only functions.
 *	HAVE_UNIX_IO
 *	HAVE_SBRK_DECL		Set if unistd.h declares sbrk().
 *	HAVE_UNIX_DOMAIN_SOCKETS
 *
 *	HAVE_UNISTD_H		Set if unistd.h exists
 *
 *	HAVE_DLOPEN		Dynamic library loading support.
 *
 *  $Header$
 */

#if __STDC__ || (defined(__sgi) && defined(__mips)) || defined(__convex__) \
    || defined(__cplusplus)
#	define USE_PROTOTYPES	1
#endif

#if defined(_POSIX_C_SOURCE) && !defined(_POSIX_SOURCE)
#	define _POSIX_SOURCE	1
#endif

#if defined(__alpha)
#	define HAVE_GETOPT_H	1
#	define HAVE_GETOPT	1
#	define HAVE_STDLIB_H	1
#	define HAVE_UNISTD_H	1
#	define HAVE_TERMIOS_H	1
#	define HAVE_STRING_H	1
#	define HAVE_SYS_SELECT_H	1 	/* For fd_set */
#	define HAVE_SBRK_DECL		1
#	define HAVE_SYS_MMAN_H	1
#	define HAVE_GETHOSTNAME	1
#	define HAVE_TERMIOS_H	1
#endif


/* Windows */
#if defined(_WIN32)
/* Microsoft VisualC++ 6.0 on WinNT 4.0
 *
 * Ensure that Project Settings / Project Options includes
 *	/Za		for ANSI C
 */

/* XXX - compiler hack; need to remove later */
# define __STDC__ 1

/* Quell the MSVC++ compiler
 *  4244 conversion from type 1 to type 2 
 *  4305 truncation
 *  4018 signed/unsigned mismatch
 * XXX should remove these eventually (and fix the warnings)
 */
# pragma warning( disable : 4244 4305 4018)

# if !__STDC__
#	 error "STDC is not properly set on WIN32 build, add /Za to Project Settings / Project Options"
# endif
#	define HAVE_STDLIB_H	1
#	define HAVE_STDARG_H	1
#	define HAVE_STRING_H	1
#	define HAS_OPENGL	1
//#	define HAS_POSIX_THREADS	0
//#	define HAS_SGIGL	0
//#	define HAVE_BSDGETTIMEOFDAY	0
//#	define HAVE_BZERO	0
//#	define HAVE_CALTECH_MALLOC	0
//#	define HAVE_DLOPEN	0
#	define HAVE_DRAND48	1
#	define HAVE_FLOAT_H	1
#	define HAVE_GETHOSTNAME	1
//#	define HAVE_GETOPT	0
//#	define HAVE_GETOPT_DECL	0
//#	define HAVE_GETOPT_H	0
//#	define HAVE_IOCTL_COMPAT_H	0
#	define HAVE_LIMITS_H	1
#	define HAVE_MATHERR	1
#	define HAVE_MEMORY_H	1
#	define HAVE_OFF_T	1
#	define HAVE_POPEN_DECL	1
//#	define HAVE_POSIX_REGEXEC	0
//#	define HAVE_REGEX	0
//#	define HAVE_REGEX_DECL	0
//#	define HAVE_SBRK	0
//#	define HAVE_SBRK_DECL	0
//#	define HAVE_SHELL_ESCAPE	0
#	define HAVE_STRCHR	1
#	define HAVE_STRDUP	1
#	define HAVE_STRDUP_DECL	1
//#	define HAVE_SYS_ERRLIST_DECL	0
//#	define HAVE_SYS_MACHD_H	0
//#	define HAVE_SYS_MMAN_H	0
//#	define HAVE_SYS_SELECT_H	0
//#	define HAVE_SYS_SOCKET_H	0
//#	define HAVE_TERMIOS_H	0
//#	define HAVE_UNISTD_H	0
//#	define HAVE_UNIX_DOMAIN_SOCKETS	0
//#	define HAVE_UNIX_IO	0
#	define HAVE_VARARGS_H	1
#	define HAVE_VFORK	1
#	define HAVE_VPRINTF	1
//#	define HAVE_WRITEV	0
//#	define HAVE_XOPEN	0
//#	define HAVE_XOSDEFS_H	0
#	define USE_PROTOTYPES	1
#	define USE_REGCOMP	1
#	define USE_STRING_H	1
//#	define USE_SYS_TIME_H	0
#	define HAVE_STRCHR 1
//#	define USE_SYSV_RE	0
//#	define SPRINTF_NOT_PARALLEL	0
#	define bzero(str,n)		memset( str, 0, n )
#	define bcopy(from,to,count)	memcpy( to, from, count )
#	define REVERSED_IEEE	yes

# include <windows.h>
# include <io.h>

/* use compatibility library calls for standard unixyness */
# define stat _stat
# define hypot _hypot
# define fdopen _fdopen
# define off_t _off_t
# undef rad1
# undef rad2
# define isascii __isascii
# define chmod _chmod
# define popen _popen
# define open _open
# define read _read
# define close _close
# define fstat _fstat
# define O_RDONLY _O_RDONLY
# define O_RDWR _O_RDWR
# define O_CREAT _O_CREAT
# define O_EXCL _O_EXCL
# define MAXPATHLEN _MAX_PATH
# define fileno _fileno
# define write _write
# define getpid _getpid
# define access _access
# define pclose _pclose
# define strdup _strdup
# define	isnan _isnan
# define isatty _isatty
# define creat _creat
# define mktemp _mktemp
# undef OUT
# undef IN
# undef DELETE
# undef complex

#endif

/*
#if defined(_WIN32) && defined(_MSC_VER) && defined(_M_IX86)
#	define USE_PROTOTYPES 1
#	define HAVE_STRING_H 1
#	define HAVE_STDLIB_H 1
#	define HAVE_STDARG_H 1
#	define HAVE_VARARGS_H 1
#	define HAVE_DRAND48 1
#	define HAVE_GETHOSTNAME 1
#	define HAVE_GETOPT 1
#	define HAVE_STRCHR 1
#	define HAVE_UNIX_IO 1
#endif
*/

#if defined(sgi) || defined(__sgi) || defined(IRIX)
#	if IRIX < 6
#		define HAS_SGIGL	1	/* Old GL */
#	else
#		define HAS_OPENGL	1	/* OpenGL */
#	endif
#	define HAVE_GETOPT_H	1
#	define HAVE_STDLIB_H	1
#	define HAVE_UNISTD_H	1
#	define HAVE_GETHOSTNAME	1
#	define HAVE_SYS_MMAN_H	1
#       ifndef __STDC__
#               define __STDC__         1
#               define USE_PROTOTYPES   1
#       endif
#       define HAVE_FLOAT_H     1
#       define HAVE_LIMITS_H    1
#	define HAVE_BZERO	1

#	if IRIX == 5
#		define HAVE_SYS_SELECT_H	1 	/* For fd_set */
#		define _BSD_TYPES		1
#		define _BSD_TIME		1	/* for good timeval */
#		define HAVE_SBRK_DECL		1
#	endif

#	if IRIX == 6
#		define HAVE_BSDGETTIMEOFDAY	1	/* For BSDgettimeofday */
#		define HAVE_SYS_SELECT_H	1 	/* For fd_set */
#		define _BSD_TYPES		1
#		define _BSD_TIME		1	/* for good timeval */
#		define HAVE_SBRK_DECL		1
#		define HAVE_REGEX_DECL		1
#		define HAVE_POSIX_REGEXEC	1
#		define USE_REGCOMP		1
#		define HAVE_DLOPEN		1
#	endif
#endif

#if defined(_AIX) || defined(__AIX)
#	define HAVE_SYS_SELECT_H	1 	/* For fd_set */
#endif

#if defined(__convex__)
#	define HAVE_STRING_H	1
#	define HAVE_STDLIB_H	1
#	define HAVE_UNISTD_H	1
#endif

#if defined(_GNU_SOURCE)
#	define _POSIX_SOURCE	1
#	define HAVE_TERMIOS_H	1
#	define HAVE_MEMORY_H	1				/* XXX */
#endif

#if defined(sun) || defined(SUNOS)
	/* SunOS 4.X on Sun-3 or Sun-4 */
#	define HAVE_STDLIB_H	1
#	define HAVE_UNISTD_H	1
#	define HAVE_SYS_MMAN_H	1
#	define HAVE_LIMITS_H	1

#	if SUNOS > 55
#		define USE_PROTOTYPES   1	
#		define HAVE_SBRK_DECL	1
/* #		define HAVE_GETOPT_DECL	1 -- is not available if __STDC__ */
#		define HAVE_MEMORY_H	1
#		define HAVE_LIMITS_H	1
#		define USE_STRING_H	1
#		undef _KERNEL  /* make sure the kernel calls are not used */
#	endif
#endif


#if defined(__FreeBSD__)
#	define	HAVE_STDLIB_H	1
#	define	HAVE_STDARG_H	1
#	define	HAVE_XOSDEFS_H	1
#	define	HAVE_GETHOSTNAME	1
#	define	HAVE_GETOPT_DECL	1
#	define	HAVE_GETOPT	1
#	define	HAVE_REGEX	1
#	define	HAVE_STRCHR	1
#	define	HAVE_VFORK	1
#	define	HAVE_VPRINTF	1
#	define	HAVE_WRITEV	1
#	define HAVE_UNIX_DOMAIN_SOCKETS	1
#	define HAVE_SHELL_ESCAPE	1
#	define HAVE_UNIX_IO		1
#	define	HAVE_SBRK_DECL	1
#	define HAVE_SYS_SOCKET_H	1
#	define	HAVE_SYS_ERRLIST_DECL	1
#	define HAVE_DRAND48	1
#	define	USE_REGCOMP	1
#	define HAVE_REGEX_DECL	1
#	define HAVE_POSIX_REGEXEC	1
#	define HAVE_STDLIB_H	1
#	define HAVE_UNISTD_H	1
#	define HAVE_TERMIOS_H	1
#	define HAVE_TERMCAP_H	1
#	define HAVE_IOCTL_COMPAT_H 1
#	define HAVE_SYS_MMAN_H	1
#       define HAVE_FLOAT_H     1
#       define HAVE_LIMITS_H    1
#	define HAVE_BZERO	1
#	define HAVE_DLOPEN	1
#	if 0 && (__FreeBSD_cc_version >= 400000 )
/*	Only FreeBSD 4.0 and above have multi-CPU support */
#		define HAS_POSIX_THREADS 1
#	endif
#endif

#if defined(__bsdi__)
#	define HAVE_GETOPT	1
#	define HAVE_GETOPT_DECL	1
#	define HAVE_XOPEN	1
#	define HAVE_TERMIOS_H	1
#	define HAVE_IOCTL_COMPAT_H 1
#	define HAVE_SYS_MMAN_H	1
#	define HAVE_MEMORY_H	1				/* XXX */
#	if !defined(_XOPEN_SOURCE)				/* XXX */
#		define _XOPEN_SOURCE	1
#	endif
#	define HAVE_SYS_ERRLIST_DECL	1
#	define USE_PROTOTYPES	1
#	define HAVE_STDLIB_H	1
#	define HAVE_UNISTD_H	1
#	define HAVE_FLOAT_H	1
#	define HAVE_SBRK_DECL	1
#	define TK_READ_DATA_PENDING(f)	((f)->_flags & __SRD)
#endif

#if defined(linux)
#	define _BSD_SOURCE	1	/* for sys/types.h to define caddr_t */
#       define STDIN_FILENO    0
#	define HAVE_GETOPT	1
#	define HAVE_GETOPT_H	1
#	define HAVE_GETOPT_DECL 1
#	define HAVE_STRDUP_DECL	1
#	define HAVE_XOSDEFS_H	1
#	define HAVE_SYS_ERRLIST_DECL	1
#	define HAVE_POSIX_REGEXEC	1
#	define USE_REGCOMP	1
#	define HAVE_UNISTD_H	1
#	define HAVE_STDLIB_H	1
#	define HAVE_GETOPT_H	1
#	define HAVE_XOSDEFS_H	1
#	define HAVE_STRING_H	1
#	define HAVE_POSIX_REGEXEC	1
#	define HAVE_STRCHR	1
#	define HAVE_VFORK	1
#	define HAVE_VPRINTF	1
#	define HAVE_WRITEV	1
#	define HAVE_SBRK_DECL	1
#	define HAVE_DLOPEN	1
#	define HAVE_UNIX_DOMAIN_SOCKETS 1
#	define HAVE_SHELL_ESCAPE	1
#	define HAVE_UNIX_IO	1
#	define HAVE_DRAND48	1
#	define HAVE_TERMIOS_H	1
#	define HAVE_TERMCAP_H	1
#	define HAVE_SYS_MMAN_H	1
#	define HAVE_LIMITS_H	1
/*#	define HAVE_BZERO	1 -- faster to use memcpy */
#	define TK_READ_DATA_PENDING(f)	((f)->_IO_read_ptr != (f)->_IO_read_end)
#	if defined(__GNUC__)
#		define HAVE_GET_NPROCS	1
#	endif
#endif

#if defined(__NetBSD__)
#	define USE_PROTOTYPES	1
#	define HAVE_STRING_H	1
#	define HAVE_STDLIB_H	1
#	define HAVE_STDARG_H	1
#	define HAVE_UNISTD_H	1
#	define HAVE_SYS_MMAN_H	1
#	define HAVE_SBRK_DECL	1
#	define HAVE_GETOPT	1
#	define HAVE_GETOPT_DECL 1
#	define HAVE_REGEX_DECL	1
#	define HAVE_SYS_ERRLIST_DECL	1
#	define HAVE_SBRK_DECL		1
#	define HAVE_IOCTL_COMPAT_H	1
#	define HAVE_BZERO	1
#endif

/* Apple Mac OS X (Darwin -- not YellowDog Linux or others where __PPC__ is
 * defined instead of __ppc__ 
 */
#if defined(__ppc__)
#       define USE_PROTOTYPES   1
#       define USE_REGCOMP      1
#       define USE_STRING_H     1
#				define HAVE_STRING_H	1
#       define HAS_POSIX_THREADS        1
#       define HAVE_BZERO       1
#				define HAVE_FLOAT	1
#				define HAVE_GETHOSTNAME	1
#       define HAVE_GETOPT      1
#       define HAVE_GETOPT_DECL 1
#       define HAVE_IOCTL_COMPAT_H      1
#				define HAVE_LIMITS_H	1
#				define HAVE_MEMORY_H	1
#       define HAVE_POSIX_REGEXEC       1
#       define HAVE_REGEX	1
#       define HAVE_REGEX_DECL  1
#       define HAVE_SBRK	1
#       define HAVE_SBRK_DECL   1
#       define HAVE_SHELL_ESCAPE	1
#				define HAVE_STRCHR	1
#       define HAVE_STDLIB_H    1
#       define HAVE_STDARG_H    1
#       define HAVE_SYS_ERRLIST_DECL    1
#       define HAVE_SYS_MMAN_H  1
#       define HAVE_SYS_SOCKET_H	1
#				define HAVE_TERMIOS_H	1
#       define HAVE_UNISTD_H    1
#       define HAVE_UNIX_DOMAIN_SOCKETS 1
#       define HAVE_UNIX_IO             1
#       define HAVE_VFORK                       1
#       define HAVE_VPRINTF             1
#       define HAVE_WRITEV              1
#endif

#if defined(__sp3__)
#	define USE_PROTOTYPES	1
#       define USE_STRING_H     1
#	define HAS_POSIX_THREADS	1
#	define HAVE_FLOAT_H	1
#	define HAVE_GETHOSTNAME	1
#	define HAVE_GETOPT	1
#	define HAVE_GETOPT_DECL	1
#	define HAVE_LIMITS_H	1
#	define HAVE_MEMORY_H	1
#	define HAVE_SBRK	1
#	define HAVE_SBRK_DECL	1
#	define HAVE_STRCHR	1
#	define HAVE_STDLIB_H	1
#	define HAVE_STDARG_H	1
#	define HAVE_STRING_H	1
#	define HAVE_SYS_MMAN_H	1
#	define HAVE_SYS_SOCKET_H	1
#	define HAVE_TERMIOS_H	1
#	define HAVE_UNISTD_H	1
#	define HAVE_UNIX_DOMAIN_SOCKETS	1
#	define HAVE_UNIX_IO	1
#	define HAVE_VFORK	1
#	define HAVE_VPRINTF	1
#	define HAVE_WRITEV	1
#endif

#if defined(__STDC__)
#	define HAVE_STDLIB_H	1
#	define HAVE_STDARG_H	1
#endif

#if 1 	/* defined(SYSV) */
	/* Most people have this. Gives us strtok and strdup and strchr */
#	define HAVE_STRING_H	1
#endif

#if defined(_POSIX_SOURCE)
#	define HAVE_UNISTD_H	1
#endif

#ifdef CRAY1
#	define HAVE_SYS_MACHD_H	1	/* XMP only, for HZ */
#endif

#if BSD >= 43
#	define HAVE_SYS_SOCKET_H	1
#endif

#if (defined(BSD) && !defined(SYSV)) || defined(CRAY)
#	define USE_SYS_TIME_H	1
#endif

#if defined(HAVE_STRING_H)		/*  && defined(SYSV) */
#	define USE_STRING_H	1
#endif

#if !defined(__convex__) && !defined(__bsdi__) && !defined(__ppc__)
#	define HAVE_DRAND48	1
#endif

#if defined(BSD) || defined(linux) || defined(sgi) || defined(_BSD_SOURCE)
#	define HAVE_GETHOSTNAME	1
#endif

#if defined(SYSV) || defined(__NetBSD__) || defined(__stardent)
#	define HAVE_GETOPT	1
#endif

#if defined(__NetBSD__) || defined(CRAY1) || defined(_HPUX_SOURCE)
#	define HAVE_GETOPT_DECL	1
#endif


#if (defined(SYSV) || BSD >= 43) && !__STDC__
#	define HAVE_MATHERR	1
#endif

#if defined(__convex__)
#	define HAVE_POPEN_DECL	1
#	define HAVE_STRDUP_DECL 1
#endif

#ifdef CRAY2
#	define HAVE_POPEN_DECL 1
#endif

#if !defined(__bsdi__) && (defined(BSD) || defined(__GNUC__) )
#	define HAVE_REGEX	1
#endif

/* For those systems without regex, pick the alternative */
#if defined(__bsdi__)
#	define USE_REGCOMP 1	/* This is the POSIX way */
#else
/*
 * XXX This hack was put in to prevent both USE_REGCOMP and USE_SYSV_RE
 * being defined at the same time.
 */
#if !defined(USE_REGCOMP)
#	define USE_SYSV_RE 1
#endif
#endif

#if !defined(SYSV)
#	define HAVE_VFORK	1
#endif

#if defined(__STDC__) || (defined(alliant) && defined(i860)) || defined(sun)
#	define HAVE_VPRINTF	1
#endif

#if defined(BSD) && !defined(CRAY) && !defined(mips) && !defined(n16)
#	define HAVE_WRITEV	1
#endif

#if defined(HAVE_STRING_H)
#	define HAVE_STRCHR	1
#endif

#if defined(unix) || defined(__unix) || defined(__unix__) || defined(BSD) || \
		defined(SUNOS) || defined(linux)
#	define HAVE_SHELL_ESCAPE	1
#	define HAVE_UNIX_IO		1
#	define HAVE_SBRK_DECL		1
#endif

#if defined(BSD) && !defined(sgi)
#	define HAVE_UNIX_DOMAIN_SOCKETS	1
#endif

/*
 *	Posix and XOpen terminal-handling features.
 *
 *	HAVE_XOPEN
 *	HAVE_TERMIOS_H		Has <termios.h>, tcgetattr(), tcsetattr()
 *	HAVE_MEMORY_H		Has <memory.h> for memset(), memcpy(), etc.
 *	_XOPEN_SOURCE
 */

#if ( defined(_POSIX_SOURCE) || defined(linux) ) && !defined(__convex__)
#	define HAVE_XOPEN	1
#	define HAVE_TERMIOS_H	1
#	define HAVE_MEMORY_H	1				/* XXX */
#	if !defined(_XOPEN_SOURCE)				/* XXX */
#		define _XOPEN_SOURCE	1
#	endif
#endif

#if !defined(SYSV)			/* XXXXXXXXX */
#	define HAVE_CALTECH_MALLOC 1   /* XXX Bleh. See librt/storage.c */
#endif

#ifdef SYSV
#	define SPRINTF_NOT_PARALLEL	1
#endif

#if defined(sun) || \
	(defined(alliant) && !defined(i860)) || \
        defined(ardent) || defined(stellar) || \
	defined(sparc) || defined(__sparc) || defined(__sparc__) || \
	defined(mips) || \
        (defined(sgi) && defined(mips)) || \
        (defined(__sgi) && defined(__mips)) || \
        defined(pyr) || defined(apollo) || defined(aux) || \
        defined(_AIX) || defined(NeXT) || defined(convex) || \
	defined(hpux) || defined(__hppa) || defined(__convex__) || \
	(defined(__ppc__) || defined(__PPC__))

        /*  These systems already operate in
         *  IEEE format internally, using big-endian order.
         */
#       define  NATURAL_IEEE    yes
#endif

#if defined(i386) || defined(__i386) || defined(__alpha) \
    || defined(n16) || defined(i860) ||defined(__stardent) 

        /* These systems operate in IEEE format, using little-endian order */
#       define  REVERSED_IEEE   yes
#endif

#if defined(NATURAL_IEEE) && defined(REVERSED_IEEE)
# include "conf.h:  ERROR, two IEEE conversions defined for this machine type"
#endif

#if 1
#	define HAVE_OFF_T	1
#endif

#if 1
#	define HAVE_STRDUP	1
#endif

/* We need sys/types.h for definitions and prototypes that will appear 
   everywhere. */

#if defined(SUNOS) && SUNOS >= 52
	/* For the duration of <sys/types.h>, set this, to get u_short
	 * etc, defined properly.
	 */
        /* This is no longer needed/recommended with the newer sun libs 
	 */
#	if SUNOS <= 55
#		define	_KERNEL	1
#	endif
#endif

#if !defined(CPP_ONLY)
# include <sys/types.h>
#endif

#if defined(SUNOS) && SUNOS >= 52
#	if SUNOS <= 55
#		undef	_KERNEL
#	endif
#endif

#if !defined(TK_FILE_COUNT) && !defined(TK_FILE_GPTR) && !defined(TK_FILE_READ_PTR) && !defined(TK_READ_DATA_PENDING)
# define TK_FILE_COUNT _cnt
#endif

/*
 * This section is intended to allow for a compile that trims off all non-critical
 * runtime actions in order to give performance a boost.  The PRODUCTION flag should
 * normally be passed in during compilation (e.g. -DPRODUCTION=1) on the command line, 
 * though it could be hard-coded as desired here for any given platform.
 * 
 * Any combination of the different optimizations listed below may individually be 
 * optionally turned off by commenting out the unwanted defines.
 *
 * NO_BOMBING_MACROS
 *	turns off many macros in h/bu.h and h/bn.h that check a condition (such as a
 *	magic number) and bu_bomb on failure.  note that turning this flag on will
 *	remove the debug capability to catch fatal runtime erros (bugs) and exit
 *	gracefully.
 * NO_MAGIC_CHECKING
 *	turns off allocation of (some) variables and calls to check magic numbers.
 *	note that this will hinder memory corrupting bug detection.
 * NO_BADRAY_CHECKING
 *	is a risky optimization to turn off checking in librt/shoot.c for bad rays 
 *	passed in to the expensive shootray() routine.
 * NO_DEBUG_CHECKING
 * 	is an optimization that replaces instances of rt_g.debug in if statements 
 *	where checking for a debug level occurs (in order to output a debug message)
 * 	with the integer constant 0.  note that turning this flag on will remove the
 *	capability to output debug messages and commands that take a debug-level args
 *	will not output as expected.  see h/raytrace.h for the implementation details.
 ****************/
#ifdef PRODUCTION
#  define NO_BOMBING_MACROS	1	/* Don't do anything for macros that only bomb on a fatal error */
#  define NO_MAGIC_CHECKING	1	/* Don't perform magic number checking */
#  define NO_BADRAY_CHECKING	1	/* Don't check for divide by zero in rt (shoot.c/shootray())*/
#  define NO_DEBUG_CHECKING	1	/* Don't use the rt_g.debug facilities */
#endif


#endif /* CONF_H */
