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
 *	HAVE_SBRK		Set if unistd.h declares sbrk().
 *	HAVE_UNIX_DOMAIN_SOCKETS
 *
 *  $Header$
 */

#if __STDC__ || (defined(__sgi) && defined(__mips)) || defined(__convex__)
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
#endif

#if IRIX == 5
#	define HAVE_SYS_SELECT_H	1 	/* For fd_set */
#	define _BSD_TYPES		1
#	define _BSD_TIME		1	/* for good timeval */
#	define HAVE_SBRK_DECL		1
#endif

#if IRIX == 6
#	define HAVE_BSDGETTIMEOFDAY	1	/* For BSDgettimeofday */
#	define HAVE_SYS_SELECT_H	1 	/* For fd_set */
#	define _BSD_TYPES		1
#	define _BSD_TIME		1	/* for good timeval */
#	define HAVE_SBRK_DECL		1
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

#if defined(sun) && !defined(SUNOS)
	/* SunOS 4.X on Sun-3 or Sun-4 */
#	define HAVE_STDLIB_H	1
#	define HAVE_UNISTD_H	1
#	define HAVE_SYS_MMAN_H	1
#	define HAVE_LIMITS_H	1
#endif

#if SUNOS >= 52
#	define HAVE_STDLIB_H	1
#	define HAVE_UNISTD_H	1
#	define HAVE_XOSDEFS_H	1
#	define HAVE_SYS_MMAN_H	1
#	define HAVE_SBRK_DECL	1
#endif

#if defined(__FreeBSD__)
#	define	HAVE_STDLIB_H	1
#	define	HAVE_STDARG_H	1
#	define	HAVE_XOSDEFS_H	1
#	define	HAVE_GETHOSTNAME	1
#	define	HAVE_GETOPT	1
#	define	HAVE_REGEX	1
#	define	HAVE_STRCHR	1
#	define	HAVE_VFORK	1
#	define	HAVE_VPRINTF	1
#	define	HAVE_WRITEV	1
#	define	HAVE_SBRK	1

#	define	HAVE_SYS_ERRLIST_DECL	1
#	define	USE_REGCOMP	1
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
#       define STDIN_FILENO    0
#	define HAVE_GETOPT_H	1
#	define HAVE_XOSDEFS_H	1
#	define TK_READ_DATA_PENDING(f)	((f)->_IO_read_ptr != (f)->_IO_read_end)
#endif

#if defined(__NetBSD__)
#	define USE_PROTOTYPES	1
#	define USE_STRING_H	1
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

#if !defined(CRAY1)			/* XXX ??? */
#	define HAVE_VARARGS_H	1	/* In case we don't HAVE_STDARG_H */
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

#if !defined(__convex__) && !defined(__bsdi__)
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
#	define USE_SYSV_RE 1
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
#	define HAVE_SBRK		1
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
	defined(hpux) || defined(__hppa) || defined(__convex__)

        /*  These systems already operate in
         *  IEEE format internally, using big-endian order.
         */
#       define  NATURAL_IEEE    yes
#endif

#if defined(n16) || defined(i386) || defined(i860) || \
	defined(__i386) || \
	defined(__stardent) || defined(__alpha)

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

#if SUNOS >= 52
	/* For the duration of <sys/types.h>, set this, to get u_short
	 * etc, defined properly.
	 */
#	define	_KERNEL	1
#endif

#ifndef CPP_ONLY
# include <sys/types.h>
#endif

#if SUNOS >= 52
#	undef	_KERNEL
#endif

#if !defined(TK_FILE_COUNT) && !defined(TK_FILE_GPTR) && !defined(TK_FILE_READ_PTR) && !defined(TK_READ_DATA_PENDING)
# define TK_FILE_COUNT _cnt
#endif

#endif /* CONF_H */

