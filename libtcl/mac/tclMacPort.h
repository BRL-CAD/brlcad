/*
 * tclMacPort.h --
 *
 *	This header file handles porting issues that occur because of
 *	differences between the Mac and Unix. It should be the only
 *	file that contains #ifdefs to handle different flavors of OS.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclMacPort.h 1.75 97/08/11 10:18:07
 */

#ifndef _MACPORT
#define _MACPORT

#ifndef _TCL
#include "tcl.h"
#endif

#include "tclErrno.h"
#include <float.h>

/* Includes */
#ifdef THINK_C
	/*
	 * The Symantic C code has not been tested
	 * and probably will not work.
	 */
#   include <pascal.h>
#   include <posix.h>
#   include <string.h>
#   include <fcntl.h>
#   include <pwd.h>
#   include <sys/param.h>
#   include <sys/types.h>
#   include <sys/stat.h>
#   include <unistd.h>
#elif defined(__MWERKS__)
#   include <time.h>
#   include <unistd.h>
/*
 * The following definitions are usually found if fcntl.h.
 * However, MetroWerks has screwed that file up a couple of times
 * and all we need are the defines.
 */
#define O_RDWR	  0x0		/* open the file in read/write mode */
#define O_RDONLY  0x1		/* open the file in read only mode */
#define O_WRONLY  0x2		/* open the file in write only mode */
#define O_APPEND  0x0100	/* open the file in append mode */
#define O_CREAT	  0x0200	/* create the file if it doesn't exist */
#define O_EXCL	  0x0400	/* if the file exists don't create it again */
#define O_TRUNC	  0x0800	/* truncate the file after opening it */

/*
 * MetroWerks stat.h file is rather weak.  The defines
 * after the include are needed to fill in the missing
 * defines.
 */
#   include <stat.h>
#   ifndef S_IFIFO
#	define   S_IFIFO	0x0100
#   endif
#   ifndef S_IFBLK
#	define   S_IFBLK	0x0600
#   endif
#   ifndef S_ISLNK
#	define   S_ISLNK(m)	(((m)&(S_IFMT)) == (S_IFLNK))
#   endif
#   ifndef S_ISSOCK
#	define   S_ISSOCK(m)	(((m)&(S_IFMT)) == (S_IFSOCK))
#   endif
#   ifndef S_IRWXU
#	define	S_IRWXU	00007		/* read, write, execute: owner */
#   	define	S_IRUSR	00004		/* read permission: owner */
#   	define	S_IWUSR	00002		/* write permission: owner */
#   	define	S_IXUSR	00001		/* execute permission: owner */
#   	define	S_IRWXG	00007		/* read, write, execute: group */
#   	define	S_IRGRP	00004		/* read permission: group */
#   	define	S_IWGRP	00002		/* write permission: group */
#   	define	S_IXGRP	00001		/* execute permission: group */
#   	define	S_IRWXO	00007		/* read, write, execute: other */
#   	define	S_IROTH	00004		/* read permission: other */
#   	define	S_IWOTH	00002		/* write permission: other */
#   	define	S_IXOTH	00001		/* execute permission: other */
#   endif

#   define isatty(arg) 1

/* 
 * Defines used by access function.  This function is provided
 * by Mac Tcl as the function TclMacAccess.
 */
 
#   define	F_OK		0	/* test for existence of file */
#   define	X_OK		0x01	/* test for execute or search permission */
#   define	W_OK		0x02	/* test for write permission */
#   define	R_OK		0x04	/* test for read permission */

#endif

/*
 * waitpid doesn't work on a Mac - the following makes
 * Tcl compile without errors.  These would normally
 * be defined in sys/wait.h on UNIX systems.
 */

#define WNOHANG 1
#define WIFSTOPPED(stat) (1)
#define WIFSIGNALED(stat) (1)
#define WIFEXITED(stat) (1)
#define WIFSTOPSIG(stat) (1)
#define WIFTERMSIG(stat) (1)
#define WIFEXITSTATUS(stat) (1)
#define WEXITSTATUS(stat) (1)
#define WTERMSIG(status) (1)
#define WSTOPSIG(status) (1)

/*
 * Define "NBBY" (number of bits per byte) if it's not already defined.
 */

#ifndef NBBY
#   define NBBY 8
#endif

/*
 * These functions always return dummy values on Mac.
 */
#ifndef geteuid
#   define geteuid() 1
#endif
#ifndef getpid
#   define getpid() -1
#endif

#define NO_SYS_ERRLIST
#define WAIT_STATUS_TYPE int

/*
 * Make sure that MAXPATHLEN is defined.
 */

#ifndef MAXPATHLEN
#   ifdef PATH_MAX
#       define MAXPATHLEN PATH_MAX
#   else
#       define MAXPATHLEN 2048
#   endif
#endif

/*
 * The following functions are declared in tclInt.h but don't do anything
 * on Macintosh systems.
 */

#define TclSetSystemEnv(a,b)

/*
 * Many signals are not supported on the Mac and are thus not defined in
 * <signal.h>.  They are defined here so that Tcl will compile with less
 * modification.
  */

#ifndef SIGQUIT
#define SIGQUIT 300
#endif

#ifndef SIGPIPE
#define SIGPIPE 13
#endif

#ifndef SIGHUP
#define SIGHUP  100
#endif

extern char **environ;

/*
 * Prototypes needed for compatability
 */

EXTERN int 	TclMacCreateEnv _ANSI_ARGS_((void));
EXTERN int	strncasecmp _ANSI_ARGS_((CONST char *s1,
			    CONST char *s2, size_t n));

/*
 * The following declarations belong in tclInt.h, but depend on platform
 * specific types (e.g. struct tm).
 */

EXTERN struct tm *	TclpGetDate _ANSI_ARGS_((const time_t *tp,
			    int useGMT));
EXTERN size_t		TclStrftime _ANSI_ARGS_((char *s, size_t maxsize,
			    const char *format, const struct tm *t));

#define tzset()
#define TclpGetPid(pid)	    ((unsigned long) (pid))

/*
 * The following prototypes and defines replace the Macintosh version
 * of the POSIX functions "stat" and "access".  The various compilier 
 * vendors don't implement this function well nor consistantly.
 */
EXTERN int TclMacStat _ANSI_ARGS_((char *path, struct stat *buf));
#define stat(path, bufPtr) TclMacStat(path, bufPtr)
#define lstat(path, bufPtr) TclMacStat(path, bufPtr)
EXTERN int TclMacAccess _ANSI_ARGS_((const char *filename, int mode));
#define access(path, mode) TclMacAccess(path, mode)
EXTERN FILE * TclMacFOpenHack _ANSI_ARGS_((const char *path,
	const char *mode));
#define fopen(path, mode) TclMacFOpenHack(path, mode)
EXTERN int TclMacReadlink _ANSI_ARGS_((char *path, char *buf, int size));
#define readlink(fileName, buffer, size) TclMacReadlink(fileName, buffer, size)
#ifdef TCL_TEST
#define chmod(path, mode) TclMacChmod(path, mode)
EXTERN int	TclMacChmod(char *path, int mode);
#endif

/*
 * Defines for Tcl internal commands that aren't really needed on
 * the Macintosh.  They all act as no-ops.
 */
#define TclCreateCommandChannel(out, in, err, num, pidPtr)	NULL
#define TclClosePipeFile(x)

/*
 * These definitions force putenv & company to use the version
 * supplied with Tcl.
 */
#ifndef putenv
#   define unsetenv	TclUnsetEnv
#   define putenv	Tcl_PutEnv
#   define setenv	TclSetEnv
void	TclSetEnv(CONST char *name, CONST char *value);
int	Tcl_PutEnv(CONST char *string);
void	TclUnsetEnv(CONST char *name);
#endif

/*
 * The default platform eol translation on Mac is TCL_TRANSLATE_CR:
 */

#define	TCL_PLATFORM_TRANSLATION	TCL_TRANSLATE_CR

/*
 * Declare dynamic loading extension macro.
 */

#define TCL_SHLIB_EXT ".shlb"

/*
 * The following define should really be in tclInt.h, but tclInt.h does
 * not include tclPort.h, which includes the "struct stat" definition.
 */

EXTERN int              TclpSameFile _ANSI_ARGS_((char *file1, char *file2,
			    struct stat *sourceStatBufPtr, 
		            struct stat *destStatBufPtr)) ;

#endif /* _MACPORT */
