/*
 * tclUnixPort.h --
 *
 *	This header file handles porting issues that occur because
 *	of differences between systems.  It reads in UNIX-related
 *	header files and sets up UNIX-related macros for Tcl's UNIX
 *	core.  It should be the only file that contains #ifdefs to
 *	handle different flavors of UNIX.  This file sets up the
 *	union of all UNIX-related things needed by any of the Tcl
 *	core files.  This file depends on configuration #defines such
 *	as NO_DIRENT_H that are set up by the "configure" script.
 *
 *	Much of the material in this file was originally contributed
 *	by Karl Lehenbauer, Mark Diekhans and Peter da Silva.
 *
 * Copyright (c) 1991-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#ifndef _TCLUNIXPORT
#define _TCLUNIXPORT

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef _TCLINT
#   include "tclInt.h"
#endif

/*
 *---------------------------------------------------------------------------
 * The following sets of #includes and #ifdefs are required to get Tcl to
 * compile under the various flavors of unix.
 *---------------------------------------------------------------------------
 */

#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_NET_ERRNO_H
#   include <net/errno.h>
#endif
#include <pwd.h>
#include <signal.h>
#ifdef HAVE_SYS_PARAM_H
#   include <sys/param.h>
#endif
#include <sys/types.h>
#ifdef USE_DIRENT2_H
#   include "../compat/dirent2.h"
#else
#ifdef NO_DIRENT_H
#   include "../compat/dirent.h"
#else
#   include <dirent.h>
#endif
#endif

#ifdef HAVE_STRUCT_DIRENT64
typedef struct dirent64	Tcl_DirEntry;
#   define TclOSreaddir		readdir64
#   define TclOSreaddir_r	readdir64_r
#else
typedef struct dirent	Tcl_DirEntry;
#   define TclOSreaddir		readdir
#   define TclOSreaddir_r	readdir_r
#endif

#ifdef HAVE_TYPE_OFF64_T
typedef off64_t		Tcl_SeekOffset;
#   define TclOSseek		lseek64
#   define TclOSopen		open64
#else
typedef off_t		Tcl_SeekOffset;
#   define TclOSseek		lseek
#   define TclOSopen		open
#endif

#ifdef HAVE_STRUCT_STAT64
#   define TclOSstat		stat64
#   define TclOSlstat		lstat64
#else
#   define TclOSstat		stat
#   define TclOSlstat		lstat
#endif

#if !HAVE_STRTOLL && defined(TCL_WIDE_INT_TYPE) && !TCL_WIDE_INT_IS_LONG
EXTERN Tcl_WideInt	strtoll _ANSI_ARGS_((CONST char *string,
					     char **endPtr, int base));
EXTERN Tcl_WideUInt	strtoull _ANSI_ARGS_((CONST char *string,
					      char **endPtr, int base));
#endif

#include <sys/file.h>
#ifdef HAVE_SYS_SELECT_H
#   undef __APPLE_API_PRIVATE
#   include <sys/select.h>
#endif
#include <sys/stat.h>
#if TIME_WITH_SYS_TIME
#   include <sys/time.h>
#   include <time.h>
#else
#if HAVE_SYS_TIME_H
#   include <sys/time.h>
#else
#   include <time.h>
#endif
#endif
#ifndef NO_SYS_WAIT_H
#   include <sys/wait.h>
#endif
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#else
#   include "../compat/unistd.h"
#endif
#ifdef	USE_FIONBIO
    /*
     * Not using the Posix fcntl(...,O_NONBLOCK,...) interface, instead
     * we are using ioctl(..,FIONBIO,..).
     */

#   ifdef HAVE_SYS_FILIO_H
#	include	<sys/filio.h>	/* For FIONBIO. */
#   endif

#   ifdef HAVE_SYS_IOCTL_H
#	include	<sys/ioctl.h>	/* For FIONBIO. */
#   endif
#endif	/* USE_FIONBIO */
#include <utime.h>

/*
 * Socket support stuff: This likely needs more work to parameterize for
 * each system.
 */

#include <sys/socket.h>		/* struct sockaddr, SOCK_STREAM, ... */
#ifndef NO_UNAME
#   include <sys/utsname.h>	/* uname system call. */
#endif
#include <netinet/in.h>		/* struct in_addr, struct sockaddr_in */
#include <arpa/inet.h>		/* inet_ntoa() */
#include <netdb.h>		/* gethostbyname() */

/*
 * Some platforms (e.g. SunOS) don't define FLT_MAX and FLT_MIN, so we
 * look for an alternative definition.  If no other alternative is available
 * we use a reasonable guess.
 */

#ifndef NO_FLOAT_H
#   include <float.h>
#else
#ifndef NO_VALUES_H
#   include <values.h>
#endif
#endif

#ifndef FLT_MAX
#   ifdef MAXFLOAT
#	define FLT_MAX MAXFLOAT
#   else
#	define FLT_MAX 3.402823466E+38F
#   endif
#endif
#ifndef FLT_MIN
#   ifdef MINFLOAT
#	define FLT_MIN MINFLOAT
#   else
#	define FLT_MIN 1.175494351E-38F
#   endif
#endif

/*
 * NeXT doesn't define O_NONBLOCK, so #define it here if necessary.
 */

#ifndef O_NONBLOCK
#   define O_NONBLOCK 0x80
#endif

/*
 * HPUX needs the flag O_NONBLOCK to get the right non-blocking I/O
 * semantics, while most other systems need O_NDELAY.  Define the
 * constant NBIO_FLAG to be one of these
 */

#ifdef HPUX
#  define NBIO_FLAG O_NONBLOCK
#else
#  define NBIO_FLAG O_NDELAY
#endif

/*
 * The type of the status returned by wait varies from UNIX system
 * to UNIX system.  The macro below defines it:
 */

#ifdef _AIX
#   define WAIT_STATUS_TYPE pid_t
#else
#ifndef NO_UNION_WAIT
#   define WAIT_STATUS_TYPE union wait
#else
#   define WAIT_STATUS_TYPE int
#endif
#endif

/*
 * Supply definitions for macros to query wait status, if not already
 * defined in header files above.
 */

#ifndef WIFEXITED
#   define WIFEXITED(stat)  (((*((int *) &(stat))) & 0xff) == 0)
#endif

#ifndef WEXITSTATUS
#   define WEXITSTATUS(stat) (((*((int *) &(stat))) >> 8) & 0xff)
#endif

#ifndef WIFSIGNALED
#   define WIFSIGNALED(stat) (((*((int *) &(stat)))) && ((*((int *) &(stat))) == ((*((int *) &(stat))) & 0x00ff)))
#endif

#ifndef WTERMSIG
#   define WTERMSIG(stat)    ((*((int *) &(stat))) & 0x7f)
#endif

#ifndef WIFSTOPPED
#   define WIFSTOPPED(stat)  (((*((int *) &(stat))) & 0xff) == 0177)
#endif

#ifndef WSTOPSIG
#   define WSTOPSIG(stat)    (((*((int *) &(stat))) >> 8) & 0xff)
#endif

/*
 * Define constants for waitpid() system call if they aren't defined
 * by a system header file.
 */

#ifndef WNOHANG
#   define WNOHANG 1
#endif
#ifndef WUNTRACED
#   define WUNTRACED 2
#endif

/*
 * Supply macros for seek offsets, if they're not already provided by
 * an include file.
 */

#ifndef SEEK_SET
#   define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#   define SEEK_CUR 1
#endif
#ifndef SEEK_END
#   define SEEK_END 2
#endif

/*
 * The stuff below is needed by the "time" command.  If this system has no
 * gettimeofday call, then must use times and the CLK_TCK #define (from
 * sys/param.h) to compute elapsed time.  Unfortunately, some systems only
 * have HZ and no CLK_TCK, and some might not even have HZ.
 */

#ifdef NO_GETTOD
#   include <sys/times.h>
#   include <sys/param.h>
#   ifndef CLK_TCK
#       ifdef HZ
#           define CLK_TCK HZ
#       else
#           define CLK_TCK 60
#       endif
#   endif
#else
#   ifdef HAVE_BSDGETTIMEOFDAY
#	define gettimeofday BSDgettimeofday
#   endif
#endif

#ifdef GETTOD_NOT_DECLARED
EXTERN int		gettimeofday _ANSI_ARGS_((struct timeval *tp,
			    struct timezone *tzp));
#endif

/*
 * Define access mode constants if they aren't already defined.
 */

#ifndef F_OK
#    define F_OK 00
#endif
#ifndef X_OK
#    define X_OK 01
#endif
#ifndef W_OK
#    define W_OK 02
#endif
#ifndef R_OK
#    define R_OK 04
#endif

/*
 * Define FD_CLOEEXEC (the close-on-exec flag bit) if it isn't
 * already defined.
 */

#ifndef FD_CLOEXEC
#   define FD_CLOEXEC 1
#endif

/*
 * On systems without symbolic links (i.e. S_IFLNK isn't defined)
 * define "lstat" to use "stat" instead.
 */

#ifndef S_IFLNK
#   undef TclOSlstat
#   define lstat	stat
#   define lstat64	stat64
#   define TclOSlstat	TclOSstat
#endif

/*
 * Define macros to query file type bits, if they're not already
 * defined.
 */

#ifndef S_ISREG
#   ifdef S_IFREG
#       define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#   else
#       define S_ISREG(m) 0
#   endif
#endif /* !S_ISREG */
#ifndef S_ISDIR
#   ifdef S_IFDIR
#       define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#   else
#       define S_ISDIR(m) 0
#   endif
#endif /* !S_ISDIR */
#ifndef S_ISCHR
#   ifdef S_IFCHR
#       define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#   else
#       define S_ISCHR(m) 0
#   endif
#endif /* !S_ISCHR */
#ifndef S_ISBLK
#   ifdef S_IFBLK
#       define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)
#   else
#       define S_ISBLK(m) 0
#   endif
#endif /* !S_ISBLK */
#ifndef S_ISFIFO
#   ifdef S_IFIFO
#       define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#   else
#       define S_ISFIFO(m) 0
#   endif
#endif /* !S_ISFIFO */
#ifndef S_ISLNK
#   ifdef S_IFLNK
#       define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#   else
#       define S_ISLNK(m) 0
#   endif
#endif /* !S_ISLNK */
#ifndef S_ISSOCK
#   ifdef S_IFSOCK
#       define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)
#   else
#       define S_ISSOCK(m) 0
#   endif
#endif /* !S_ISSOCK */

/*
 * Make sure that MAXPATHLEN and MAXNAMLEN are defined.
 */

#ifndef MAXPATHLEN
#   ifdef PATH_MAX
#       define MAXPATHLEN PATH_MAX
#   else
#       define MAXPATHLEN 2048
#   endif
#endif

#ifndef MAXNAMLEN
#   ifdef NAME_MAX
#	define MAXNAMLEN NAME_MAX
#   else
#	define MAXNAMLEN 255
#   endif
#endif

/*
 * Make sure that L_tmpnam is defined.
 */

#ifndef L_tmpnam
#   define L_tmpnam 100
#endif

/*
 * The following macro defines the type of the mask arguments to
 * select:
 */

#ifndef NO_FD_SET
#   define SELECT_MASK fd_set
#else /* NO_FD_SET */
#   ifndef _AIX
	typedef long fd_mask;
#   endif /* !AIX */
#   if defined(_IBMR2)
#	define SELECT_MASK void
#   else /* !defined(_IBMR2) */
#	define SELECT_MASK int
#   endif /* defined(_IBMR2) */
#endif /* !NO_FD_SET */

/*
 * Define "NBBY" (number of bits per byte) if it's not already defined.
 */

#ifndef NBBY
#   define NBBY 8
#endif

/*
 * The following macro defines the number of fd_masks in an fd_set:
 */

#ifndef FD_SETSIZE
#   ifdef OPEN_MAX
#	define FD_SETSIZE OPEN_MAX
#   else
#	define FD_SETSIZE 256
#   endif
#endif /* FD_SETSIZE */
#if !defined(howmany)
#   define howmany(x, y) (((x)+((y)-1))/(y))
#endif /* !defined(howmany) */
#ifndef NFDBITS
#   define NFDBITS NBBY*sizeof(fd_mask)
#endif /* NFDBITS */
#define MASK_SIZE howmany(FD_SETSIZE, NFDBITS)

/*
 * Not all systems declare the errno variable in errno.h. so this
 * file does it explicitly.  The list of system error messages also
 * isn't generally declared in a header file anywhere.
 */

#ifdef NO_ERRNO
extern int errno;
#endif

/*
 * Not all systems declare all the errors that Tcl uses!  Provide some
 * work-arounds...
 */

#ifndef EOVERFLOW
#   ifdef EFBIG
#	define EOVERFLOW EFBIG
#   else /* !EFBIG */
#	define EOVERFLOW EINVAL
#   endif /* EFBIG */
#endif /* EOVERFLOW */

/*
 * Variables provided by the C library:
 */

#if defined(_sgi) || defined(__sgi) || (defined(__APPLE__) && defined(__DYNAMIC__))
#   define environ _environ
#endif
extern char **environ;

/*
 * At present (12/91) not all stdlib.h implementations declare strtod.
 * The declaration below is here to ensure that it's declared, so that
 * the compiler won't take the default approach of assuming it returns
 * an int.  There's no ANSI prototype for it because there would end
 * up being too many conflicts with slightly-different prototypes.
 */

extern double strtod();

/*
 * There is no platform-specific panic routine for Unix in the Tcl internals.
 */

#define TclpPanic ((Tcl_PanicProc *) NULL)

/*
 *---------------------------------------------------------------------------
 * The following macros and declarations represent the interface between 
 * generic and unix-specific parts of Tcl.  Some of the macros may override 
 * functions declared in tclInt.h.
 *---------------------------------------------------------------------------
 */

/*
 * The default platform eol translation on Unix is TCL_TRANSLATE_LF.
 */

#ifdef DJGPP
#define	TCL_PLATFORM_TRANSLATION	TCL_TRANSLATE_CRLF
#else
#define	TCL_PLATFORM_TRANSLATION	TCL_TRANSLATE_LF
#endif

/*
 * The following macros have trivial definitions, allowing generic code to 
 * address platform-specific issues.
 */

#define TclpGetPid(pid)		((unsigned long) (pid))
#define TclpReleaseFile(file)	/* Nothing. */

/*
 * The following defines wrap the system memory allocation routines for
 * use by tclAlloc.c.  By default off unused on Unix.
 */

#if USE_TCLALLOC
#   define TclpSysAlloc(size, isBin)	malloc((size_t)size)
#   define TclpSysFree(ptr)		free((char*)ptr)
#   define TclpSysRealloc(ptr, size)	realloc((char*)ptr, (size_t)size)
#endif

/*
 * The following macros and declaration wrap the C runtime library
 * functions.
 */

#define TclpExit		exit

/*
 * Platform specific mutex definition used by memory allocators.
 * These mutexes are statically allocated and explicitly initialized.
 * Most modules do not use this, but instead use Tcl_Mutex types and
 * Tcl_MutexLock and Tcl_MutexUnlock that are self-initializing.
 */

#ifdef TCL_THREADS
#include <pthread.h>
typedef pthread_mutex_t TclpMutex;
EXTERN void	TclpMutexInit _ANSI_ARGS_((TclpMutex *mPtr));
EXTERN void	TclpMutexLock _ANSI_ARGS_((TclpMutex *mPtr));
EXTERN void	TclpMutexUnlock _ANSI_ARGS_((TclpMutex *mPtr));
EXTERN Tcl_DirEntry * 	TclpReaddir(DIR *);
EXTERN struct tm *     	TclpLocaltime(time_t *);
EXTERN struct tm *     	TclpGmtime(time_t *);
EXTERN char *          	TclpInetNtoa(struct in_addr);
#define readdir(x)	TclpReaddir(x)
#define localtime(x)	TclpLocaltime(x)
#define gmtime(x)	TclpGmtime(x)
#define inet_ntoa(x)	TclpInetNtoa(x)
#undef TclOSreaddir
#define TclOSreaddir(x) TclpReaddir(x)
#ifdef MAC_OSX_TCL
/* 
 * On Mac OS X, realpath is currently not
 * thread safe, c.f. SF bug # 711232.
 */
#define NO_REALPATH
#endif
#else
typedef int TclpMutex;
#define	TclpMutexInit(a)
#define	TclpMutexLock(a)
#define	TclpMutexUnlock(a)
#endif /* TCL_THREADS */

#include "tclPlatDecls.h"
#include "tclIntPlatDecls.h"

#endif /* _TCLUNIXPORT */
