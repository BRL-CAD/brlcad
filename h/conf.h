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
 */

#if __STDC__ || (defined(__sgi) && defined(__mips))
#	define USE_PROTOTYPES	1
#endif

#if defined(_POSIX_C_SOURCE) && !defined(_POSIX_SOURCE)
#	define _POSIX_SOURCE	1
#endif

#if defined(sgi) || defined(__sgi) || defined(IRIX)
#	if IRIX < 6
#		define HAS_SGIGL	1	/* Old GL */
#	else
#		define HAS_OPENGL	1	/* OpenGL */
#	endif
#	define HAVE_GETOPT_H	1
#	define HAVE_REGEX_DECL	1
#	define HAVE_SBRK_DECL	1
#	define HAVE_STDLIB_H	1
#	define HAVE_UNISTD_H	1
#endif

#if defined(_GNU_SOURCE)
#	define _POSIX_SOURCE	1
#	define HAVE_TERMIOS_H	1
#	define HAVE_MEMORY_H	1				/* XXX */
#endif


/*
 *	Include handling
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
 */

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

/*
 * XXX SunOS 4.X also has <unistd.h> but I am not sure if including it
 * works since I am only working on SunOS 5.X
 */
#if SUNOS >= 52
#	define HAVE_UNISTD_H	1
#endif

#if !defined(CRAY1)			/* XXX ??? */
#	define HAVE_VARARGS_H	1	/* In case we don't HAVE_STDARG_H */
#endif

#ifdef CRAY1
#	define HAVE_SYS_MACHD_H	1	/* XMP only, for HZ */
#endif

#if (defined(sgi) && defined(mips)) || (defined(__sgi) && defined(__mips))
#	define HAVE_SYS_MMAN_H	1
#endif

#if BSD >= 43
#	define HAVE_SYS_SOCKET_H	1
#endif

/*
 *	USE_SYS_TIME_H		XXX Unnecessary
 *
 *	USE_STRING_H		If your string.h is sufficient
 */

#if (defined(BSD) && !defined(SYSV)) || defined(CRAY)
#	define USE_SYS_TIME_H	1
#endif

#if defined(HAVE_STRING_H)		/*  && defined(SYSV) */
#	define USE_STRING_H	1
#endif



/*	HAVE_DRAND48		Most everyone? except Convex
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
 */

#if !defined(__convex__) && !defined(__bsdi__)
#	define HAVE_DRAND48	1
#endif

#if defined(BSD) || defined(linux) || defined(sgi) || defined(_BSD_SOURCE)
#	define HAVE_GETHOSTNAME	1
#endif

#if defined(linux)
#	define HAVE_GETOPT_H	1
#endif

#if defined(SYSV) || defined(__NetBSD__) || defined(__bsdi__) || defined(__stardent)
#	define HAVE_GETOPT	1
#endif

#if defined(__NetBSD__) || defined(CRAY1) || defined(__bsdi__)
#	define HAVE_GETOPT_DECL	1
#endif

#if (defined(SYSV) || BSD >= 43) && !__STDC__
#	define HAVE_MATHERR	1
#endif

#if (defined(BSD) && !defined(__bsdi__)) || defined(__GNUC__)
#	define HAVE_REGEX	1
#endif

/* For those systems without regex, pick the alternative */
#if defined(__bsdi__)
#	define USE_REGCOMP 1	/* This is the POSIX way */
#else
#	define USE_SYSV_RE 1
#endif

#if defined(__NetBSD__)
#	define HAVE_REGEX_DECL	1
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
 *	HAVE_TERMIOS_H
 *	HAVE_MEMORY_H
 *	_XOPEN_SOURCE
 */

#if (defined(__bsdi__) || defined(_POSIX_SOURCE) ) && !defined(__convex__)	/* XXX */
#	define HAVE_XOPEN	1
#	define HAVE_TERMIOS_H	1
#	define HAVE_MEMORY_H	1				/* XXX */
#	if !defined(_XOPEN_SOURCE)				/* XXX */
#		define _XOPEN_SOURCE	1
#	endif
#endif

/*
 *	NetBSD's pedantic sys_errlist declaration means we don't want to
 *	make our own (it declares it as "const char const *" which collides.)
 */

#ifdef __NetBSD__
#	define HAVE_SYS_ERRLIST_DECL	1
#	define HAVE_SBRK_DECL		1
#endif

#if SUNOS >= 52
#	define HAVE_SBRK_DECL		1
#endif

#if !defined(SYSV)			/* XXXXXXXXX */
#	define HAVE_CALTECH_MALLOC 1   /* XXX Bleh. See librt/storage.c */
#endif

#if IRIX >= 5
#	define HAVE_SYS_SELECT_H	1 	/* For fd_set */
#	define _BSD_TYPES		1
#	define _BSD_TIME		1	/* for good timeval */
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
        defined(_AIX) || defined(NeXT) || defined(hpux)

        /*  These systems already operate in
         *  IEEE format internally, using big-endian order.
         */
#       define  NATURAL_IEEE    yes
#endif

#if defined(n16) || defined(i386) || defined(i860) || \
	defined(__i386) || \
	defined(__stardent)

        /* These systems operate in IEEE format, using little-endian order */
#       define  REVERSED_IEEE   yes
#endif

#if defined(NATURAL_IEEE) && defined(REVERSED_IEEE)
# include "conf.h:  ERROR, two IEEE conversions defined for this machine type"
#endif

#if 1
#	define HAVE_OFF_T	1
#endif

#if !defined(vax) && !defined(gould) && !defined(sun)
#	define VECTORIZE	1
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

#include <sys/types.h>

#if SUNOS >= 52
#	undef	_KERNEL
#endif

#endif /* CONF_H */

