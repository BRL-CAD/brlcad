/*                            B U . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** @file bu.h
 *
 * Main header file for the BRL-CAD Utility Library, LIBBU.
 *
 * The two letters "BU" stand for "BRL-CAD" and "Utility".  This
 * library provides several layers of low-level utility routines,
 * providing features that make cross-platform coding easier.
 *
 * Parallel processing support:  threads, semaphores, parallel-malloc.
 * Consolidated logging support:  bu_log(), bu_exit(), and bu_bomb().
 *
 * The intention is that these routines are general extensions to the
 * data types offered by the C language itself, and to the basic C
 * runtime support provided by the system LIBC.  All routines in LIBBU
 * are designed to be "parallel-safe" (sometimes called "mp-safe" or
 * "thread-safe" if parallelism is via threading) to greatly ease code
 * development for multiprocessor systems.
 *
 * All of the data types provided by this library are defined in bu.h
 * or appropriate included files from the ./bu subdirectory; none of
 * the routines in this library will depend on data types defined in
 * other BRL-CAD header files, such as vmath.h.  Look for those
 * routines in LIBBN.
 *
 * All truly fatal errors detected by the library use bu_bomb() to
 * exit with a status of 12.  The LIBBU variants of system calls
 * (e.g., bu_malloc()) do not return to the caller (unless there's a
 * bomb hook defined) unless they succeed, thus sparing the programmer
 * from constantly having to check for NULL return codes.
 *
 */
#ifndef BU_H
#define BU_H

#include "common.h"

#include <stdlib.h>
#include <sys/types.h>
#include <stdarg.h>

__BEGIN_DECLS

#include "./bu/defines.h"



/* system interface headers */
#include <setjmp.h> /* for bu_setjmp */
#include <stddef.h> /* for size_t */
#include <limits.h> /* for CHAR_BIT */

#ifdef HAVE_STDINT_H
#  include <stdint.h> /* for [u]int[16|32|64]_t */
#endif

#ifdef HAVE_DLFCN_H
#  include <dlfcn.h>	/* for RTLD_* */
#endif

/* common interface headers */
#include "tcl.h"	/* Included for Tcl_Interp definition */
#include "bu/magic.h"

/* FIXME Temporary global interp.  Remove me.  */
BU_EXPORT extern Tcl_Interp *brlcad_interp;


/** @file libbu/vers.c
 *
 * version information about LIBBU
 *
 */

/**
 * returns the compile-time version of libbu
 */
BU_EXPORT extern const char *bu_version(void);

/**
 * MAX_PSW - The maximum number of processors that can be expected on
 * this hardware.  Used to allocate application-specific per-processor
 * tables at compile-time and represent a hard limit on the number of
 * processors/threads that may be spawned. The actual number of
 * available processors is found at runtime by calling bu_avail_cpus()
 */
#define MAX_PSW 1024


/*----------------------------------------------------------------------*/

#include "./bu/cv.h"

/*----------------------------------------------------------------------*/

#include "./bu/endian.h"

/*----------------------------------------------------------------------*/

#include "./bu/list.h"

/*----------------------------------------------------------------------*/

#include "./bu/bitv.h"

/*----------------------------------------------------------------------*/

#include "./bu/hist.h"

/*----------------------------------------------------------------------*/

#include "./bu/ptbl.h"

/*----------------------------------------------------------------------*/

#include "./bu/mapped_file.h"

/*----------------------------------------------------------------------*/

#include "./bu/vls.h"

/*----------------------------------------------------------------------*/

#include "./bu/avs.h"


/*----------------------------------------------------------------------*/

#include "./bu/vlb.h"

/*----------------------------------------------------------------------*/

#include "./bu/debug.h"

/*----------------------------------------------------------------------*/

#include "bu/parse.h"

/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/

#include "bu/color.h"

/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/

#include "bu/rb.h"

/*----------------------------------------------------------------------*/

/**
 * TBD
 */
struct bu_observer {
    struct bu_list l;
    struct bu_vls observer;
    struct bu_vls cmd;
};
typedef struct bu_observer bu_observer_t;
#define BU_OBSERVER_NULL ((struct bu_observer *)0)

/**
 * asserts the integrity of a non-head node bu_observer struct.
 */
#define BU_CK_OBSERVER(_op) BU_CKMAG(_op, BU_OBSERVER_MAGIC, "bu_observer magic")

/**
 * initializes a bu_observer struct without allocating any memory.
 */
#define BU_OBSERVER_INIT(_op) { \
	BU_LIST_INIT_MAGIC(&(_op)->l, BU_OBSERVER_MAGIC); \
	BU_VLS_INIT(&(_op)->observer); \
	BU_VLS_INIT(&(_op)->cmd); \
    }

/**
 * macro suitable for declaration statement initialization of a bu_observer
 * struct.  does not allocate memory.  not suitable for a head node.
 */
#define BU_OBSERVER_INIT_ZERO { {BU_OBSERVER_MAGIC, BU_LIST_NULL, BU_LIST_NULL}, BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO }

/**
 * returns truthfully whether a bu_observer has been initialized.
 */
#define BU_OBSERVER_IS_INITIALIZED(_op) (((struct bu_observer *)(_op) != BU_OBSERVER_NULL) && LIKELY((_op)->magic == BU_OBSERVER_MAGIC))

/*----------------------------------------------------------------------*/

#include "./bu/log.h"

/*----------------------------------------------------------------------*/

/** @addtogroup file */
/** @{ */

/** @file libbu/file.c
 *
 * Support routines for identifying properties of files and
 * directories such as whether they exist or are the same as another
 * given file.
 *
 */

/**
 * Returns truthfully whether the given file path exists or not.  An
 * empty or NULL path name is treated as a non-existent file and, as
 * such, will return false.  If fd is non-NULL, it will be set to an
 * open file descriptor for the provided path.
 *
 * @return >0 The given filename exists.
 * @return 0 The given filename does not exist.
 */
BU_EXPORT extern int bu_file_exists(const char *path, int *fd);

/**
 * Returns truthfully as to whether the two provided filenames are the
 * same file.  If either file does not exist, the result is false.  If
 * either filename is empty or NULL, it is treated as non-existent
 * and, as such, will also return false.
 */
BU_EXPORT extern int bu_same_file(const char *fn1, const char *fn2);

/**
 * returns truthfully as to whether or not the two provided file
 * descriptors are the same file.  if either file does not exist, the
 * result is false.
 */
BU_EXPORT extern int bu_same_fd(int fd1, int fd2);

/**
 * returns truthfully if current user can read the specified file or
 * directory.
 */
BU_EXPORT extern int bu_file_readable(const char *path);

/**
 * returns truthfully if current user can write to the specified file
 * or directory.
 */
BU_EXPORT extern int bu_file_writable(const char *path);

/**
 * returns truthfully if current user can run the specified file or
 * directory.
 */
BU_EXPORT extern int bu_file_executable(const char *path);

/**
 * Returns truthfully whether the given file path is a directory.  An
 * empty or NULL path name is treated as a non-existent directory and,
 * as such, will return false.
 *
 * @return >0 The given filename is a directory
 * @return 0 The given filename is not a directory.
 */
BU_EXPORT extern int bu_file_directory(const char *path);

/**
 * Returns truthfully whether the given file path is a symbolic link.
 * An empty or NULL path name is treated as a non-existent link and,
 * as such, will return false.
 *
 * @return >0 The given filename is a symbolic link
 * @return 0 The given filename is not a symbolic link.
 */
BU_EXPORT extern int bu_file_symbolic(const char *path);

/**
 * forcibly attempts to delete a specified file.  if the file can be
 * deleted by relaxing file permissions, they will be changed in order
 * to delete the file.
 *
 * returns truthfully if the specified file was deleted.
 */
BU_EXPORT extern int bu_file_delete(const char *path);

/**
 * matches a filepath pattern to directory entries.  if non-NULL,
 * matching paths are dynamically allocated, stored into the provided
 * 'matches' array, and followed by a terminating NULL entry.
 *
 * If '*matches' is NULL, the caller is expected to free the matches
 * array with bu_free_argv() If '*matches' is non-NULL (i.e., string
 * array is already allocated or on the stack), the caller is expected
 * to ensure adequate entries are allocated and call bu_free_array()
 * to clean up.  If 'matches' is NULL, no entries will be allocated or
 * stored, but the number of matches will still be returned.
 *
 * Example:
 *
 * char **my_matches = NULL;
 * bu_file_glob("src/libbu/[a-e]*.c", &my_matches);
 *
 * This will allocate an array for storing glob matches, filling in
 * the array with all of the directory entries starting with 'a'
 * through 'e' and ending with a '.c' suffix in the src/libbu
 * directory.
 *
 * returns the number of matches
 */
BU_EXPORT extern size_t bu_file_glob(const char *pattern, char ***matches);

/**
 * Call canonicalization routines to both expand and validate
 * a path name.
 *
 * returns a pointer to the canonical path.  Caller must free
 * the path.
 */
BU_EXPORT extern char * bu_file_path_canonicalize(const char *path);

/** @file libbu/fnmatch.c
 *
 */

#define BU_FNMATCH_NOESCAPE    0x01 /**< bu_fnmatch() flag.  Backslash escaping. */
#define BU_FNMATCH_PATHNAME    0x02 /**< bu_fnmatch() flag.  Slash must be matched by slash. */
#define BU_FNMATCH_PERIOD      0x04 /**< bu_fnmatch() flag.  Period must be matched by period. */
#define BU_FNMATCH_LEADING_DIR 0x08 /**< bu_fnmatch() flag.  Ignore /<tail> after Imatch. */
#define BU_FNMATCH_CASEFOLD    0x10 /**< bu_fnmatch() flag.  Case-insensitive searching. */

/**
 * bu_fnmatch() return value when no match is found (0 if found)
 */
#define BU_FNMATCH_NOMATCH 1       /* Match failed. */

/**
 * Function fnmatch() as specified in POSIX 1003.2-1992, section B.6.
 * Compares a string filename or pathname to a pattern.
 *
 * Returns 0 if a match is found or BU_FNMATCH_NOMATCH otherwise.
 *
 */
BU_EXPORT extern int bu_fnmatch(const char *pattern, const char *pathname, int flags);


/** @file libbu/dirent.c
 *
 * Functionality for accessing all files in a directory.
 *
 */

/**
 * Returns the number of directory entries for a given path matching
 * an optional glob pattern.  If the caller provides a pointer to an
 * argv-style 'files' array, this function will allocate the array
 * with dynamically allocated strings for any matching file(s).
 *
 * It is the caller's responsibility to free a non-NULL 'files' array
 * with bu_free_argv().
 */
BU_EXPORT extern size_t bu_dir_list(const char *path, const char *pattern, char ***files);


/** @file libbu/realpath.c
 *
 */

/**
 * Call canonicalization routines to both expand and validate
 * a path name.
 *
 * Returns a pointer to the canonical path. If resolved_path is
 * NULL, caller is responsible for freeing the returned path
 * via bu_free.  If supplying a result string, the string must hold
 * at least MAXPATHLEN characters.
 */
BU_EXPORT extern char * bu_realpath(const char *path, char *resolved_path);


/** @file libbu/brlcad_path.c
 *
 * @brief
 * A support routine to provide the executable code with the path
 * to where the BRL-CAD programs and libraries are installed.
 *
 */

/**
 * DEPRECATED: This routine is replaced by bu_argv0_full_path().
 *             Do not use.
 *
 * this routine is used by the brlcad-path-finding routines when
 * attempting to locate binaries, libraries, and resources.  This
 * routine will set argv0 if path is provided and should generally be
 * set early on by bu_setprogname().
 *
 * this routine will return "(BRL-CAD)" if argv[0] cannot be
 * identified but should never return NULL.
 */
DEPRECATED BU_EXPORT extern const char *bu_argv0(void);

/**
 * DEPRECATED: This routine is replaced by bu_getcwd().
 *             Do not use.
 *
 * returns the full path to argv0, regardless of how the application
 * was invoked.
 *
 * this routine will return "(BRL-CAD)" if argv[0] cannot be
 * identified but should never return NULL.  this routine is not
 * thread-safe.
 */
BU_EXPORT extern const char *bu_argv0_full_path(void);

/**
 * Get the name of the running application.  application codes should
 * call bu_setprogname() first to ensure that the program name is
 * stored appropriately on platforms that do not have an intrinsic
 * method for tracking the program name automatically.
 *
 * while this routine is thread-safe and reentrant, the static string
 * returned is shared amongst all threads.
 */
BU_EXPORT extern const char *bu_getprogname(void);

/**
 * Set the name of the running application.  This isn't strictly
 * necessary on platforms that have an intrinsic method for tracking
 * the program name automatically, but is still recommended for
 * portability and is necessary on some strict modes of compilation.
 *
 * while the implementation relies on a static string shared across
 * all threads, this routine is thread-safe and reentrant.
 */
BU_EXPORT extern void bu_setprogname(const char *path);

/**
 * returns the pathname for the current working directory.
 */
BU_EXPORT extern char *bu_getcwd(char *buf, size_t size);

/**
 * Report the relative paths being used to hold BRL-CAD applications,
 * libraries, and data.
 *
 * Recognized keys include:
 *
 *   bin     - Directory containing binary applications
 *   lib     - Directory containing libraries
 *   include - Directory containing headers
 *   data    - Directory containing shared data
 *   share   - Directory containing shared data
 *   doc     - Directory containing documentation
 *   man     - Directory containing Unix man pages
 *
 * @return
 * A STATIC buffer is returned.  It is the caller's responsibility to
 * call bu_strdup() or make other provisions to save the returned
 * string, before calling again.
 */
BU_EXPORT extern const char *bu_brlcad_dir(const char *dirkey, int fail_quietly);

/**
 * Locate where the BRL-CAD applications and libraries are installed.
 *
 * The BRL-CAD root is searched for in the following order of
 * precedence by testing for the rhs existence if provided or the
 * directory existence otherwise:
 *
 *   BRLCAD_ROOT environment variable if set
 *   BRLCAD_ROOT compile-time path
 *   run-time path identification
 *   /usr/brlcad static path
 *   current directory
 *
 * @return
 * A STATIC buffer is returned.  It is the caller's responsibility to
 * call bu_strdup() or make other provisions to save the returned
 * string, before calling again.
 */
BU_EXPORT extern const char *bu_brlcad_root(const char *rhs, int fail_quietly);

/**
 * Locate where the BRL-CAD data resources are installed.
 *
 * The BRL-CAD data resources are searched for in the following order
 * of precedence by testing for the existence of rhs if provided or
 * the directory existence otherwise:
 *
 *   BRLCAD_DATA environment variable if set
 *   BRLCAD_DATA compile-time path
 *   bu_brlcad_root/DATA_DIR path
 *   bu_brlcad_root/share path
 *   current directory
 *
 * A STATIC buffer is returned.  It is the caller's responsibility to
 * call bu_strdup() or make other provisions to save the returned
 * string, before calling again.
 */
BU_EXPORT extern const char *bu_brlcad_data(const char *rhs, int fail_quietly);

/**
 * returns the first USER path match to a given executable name.
 *
 * Routine to provide BSD "which" functionality, locating binaries of
 * specified programs from the user's PATH. This is useful to locate
 * binaries and resources at run-time.
 *
 * caller should not free the result, though it will not be preserved
 * between calls either.  the caller should strdup the result if they
 * need to keep it around.
 *
 * routine will return NULL if the executable command cannot be found.
 */
BU_EXPORT extern const char *bu_which(const char *cmd);

/**
 * returns the first SYSTEM path match to a given executable cmd name.
 *
 * Routine to provide BSD "whereis" functionality, locating binaries
 * of specified programs from the SYSTEM path.  This is useful to
 * locate binaries and resources at run-time.
 *
 * caller should not free the result, though it will not be preserved
 * between calls either.  the caller should strdup the result if they
 * need to keep it around.
 *
 * routine will return NULL if the executable command cannot be found.
 */
BU_EXPORT extern const char *bu_whereis(const char *cmd);

/** @file libbu/temp.c
 *
 * Routine to open a temporary file.
 *
 */

/**
 * Create a temporary file.  The first readable/writable directory
 * will be used, searching TMPDIR/TEMP/TMP environment variable
 * directories followed by default system temp directories and
 * ultimately trying the current directory.
 *
 * This routine is guaranteed to return a new unique file or return
 * NULL on failure.  The temporary file will be automatically unlinked
 * on application exit.  It is the caller's responsibility to set file
 * access settings, preserve file contents, or destroy file contents
 * if the default behavior is non-optimal.
 *
 * The name of the temporary file will be copied into a user-provided
 * (filepath) buffer if it is a non-NULL pointer and of a sufficient
 * (len) length to contain the filename.
 *
 * This routine is NOT thread-safe.
 *
 * Typical Use:
 @code
  FILE *fp;
  char filename[MAXPATHLEN];
  fp = bu_temp_file(&filename, MAXPATHLEN); // get file name
  ...
  fclose(fp); // close the file when you're done
  ...
  fp = bu_temp_file(NULL, 0); // don't need file name
  bu_fchmod(fileno(fp), 0777);
  ...
  rewind(fp);
  while (fputc(0, fp) == 0);
  fclose(fp);
 @endcode
 */
BU_EXPORT extern FILE *bu_temp_file(char *filepath, size_t len);

/** @} */
/** @addtogroup getopt */
/** @{ */

/** @file libbu/getopt.c
 *
 * @brief
 * Special portable re-entrant version of getopt.
 *
 * Everything is prefixed with bu_, to distinguish it from the various
 * getopt routines found in libc.
 *
 * Important note -
 * If bu_getopt() is going to be used more than once, it is necessary
 * to reinitialize bu_optind=1 before beginning on the next argument
 * list.
 */

/**
 * for bu_getopt().  set to zero to suppress errors.
 */
BU_EXPORT extern int bu_opterr;

/**
 * for bu_getopt().  current index into parent argv vector.
 */
BU_EXPORT extern int bu_optind;

/**
 * for bu_getopt().  current option being checked for validity.
 */
BU_EXPORT extern int bu_optopt;

/**
 * for bu_getopt().  current argument associated with current option.
 */
BU_EXPORT extern char *bu_optarg;

/**
 * Get option letter from argument vector.
 *
 * returns the next known option character in ostr.  If bu_getopt()
 * encounters a character not found in ostr or if it detects a missing
 * option argument, it returns `?' (question mark).  If ostr has a
 * leading `:' then a missing option argument causes `:' to be
 * returned instead of `?'.  In either case, the variable bu_optopt is
 * set to the character that caused the error.  The bu_getopt()
 * function returns -1 when the argument list is exhausted.
 */
BU_EXPORT extern int bu_getopt(int nargc, char * const nargv[], const char *ostr);

/** @} */

/** @addtogroup thread */
/** @{ */
/** @file libbu/parallel.c
 *
 * subroutine to determine if we are multi-threaded
 *
 * This subroutine is separated off from parallel.c so that bu_bomb()
 * and others can call it, without causing either parallel.c or
 * semaphore.c to get referenced and thus causing the loader to drag
 * in all the parallel processing stuff from the vendor library.
 *
 */

/**
 * A clean way for bu_bomb() to tell if this is a parallel
 * application.  If bu_parallel() is active, this routine will return
 * non-zero.
 */
BU_EXPORT extern int bu_is_parallel(void);

/**
 * Used by bu_bomb() to help terminate parallel threads,
 * without dragging in the whole parallel library if it isn't being used.
 */
BU_EXPORT extern void bu_kill_parallel(void);

/**
 * returns the CPU number of the current bu_parallel() invoked thread.
 */
BU_EXPORT extern int bu_parallel_id(void);


/** @} */

#include "bu/malloc.h"

/** @addtogroup thread */
/** @{ */
/** @file libbu/kill.c
 *
 * terminate a given process.
 *
 */

/**
 * terminate a given process.
 *
 * returns truthfully whether the process could be killed.
 */
BU_EXPORT extern int bu_terminate(int process);

/** @file libbu/process.c
 *
 * process management routines
 *
 */

/**
 * returns the process ID of the calling process
 */
BU_EXPORT extern int bu_process_id(void);

/** @file libbu/parallel.c
 *
 * routines for parallel processing
 *
 * Machine-specific routines for portable parallel processing.
 *
 */

/**
 * Without knowing what the current UNIX "nice" value is, change to a
 * new absolute "nice" value.  (The system routine makes a relative
 * change).
 */
BU_EXPORT extern void bu_nice_set(int newnice);

/**
 * Return the maximum number of physical CPUs that are considered to
 * be available to this process now.
 */
BU_EXPORT extern size_t bu_avail_cpus(void);

/**
 * Create 'ncpu' copies of function 'func' all running in parallel,
 * with private stack areas.  Locking and work dispatching are handled
 * by 'func' using a "self-dispatching" paradigm.
 *
 * 'func' is called with one parameter, its thread number.  Threads
 * are given increasing numbers, starting with zero.  Processes may
 * also call bu_parallel_id() to obtain their thread number.
 *
 * Threads created with bu_parallel() automatically set CPU affinity
 * where available for improved performance.  This behavior can be
 * disabled at runtime by setting the LIBBU_AFFINITY environment
 * variable to 0.
 *
 * This function will not return control until all invocations of the
 * subroutine are finished.
 */
BU_EXPORT extern void bu_parallel(void (*func)(int ncpu, genptr_t arg), int ncpu, genptr_t arg);

/** @} */

/** @addtogroup thread */
/** @{ */

/** @file libbu/semaphore.c
 *
 * semaphore implementation
 *
 * Machine-specific routines for parallel processing.  Primarily for
 * handling semaphores to protect critical sections of code.
 *
 * The new paradigm: semaphores are referred to, not by a pointer, but
 * by a small integer.  This module is now responsible for obtaining
 * whatever storage is needed to implement each semaphore.
 *
 * Note that these routines can't use bu_log() for error logging,
 * because bu_log() acquires semaphore #0 (BU_SEM_SYSCALL).
 */

/*
 * Section for manifest constants for bu_semaphore_acquire()
 */
#define BU_SEM_SYSCALL 0
#define BU_SEM_LISTS 1
#define BU_SEM_BN_NOISE 2
#define BU_SEM_MAPPEDFILE 3
#define BU_SEM_THREAD 4
#define BU_SEM_MALLOC 5
#define BU_SEM_DATETIME 6
#define BU_SEM_LAST (BU_SEM_DATETIME+1)	/* allocate this many for LIBBU+LIBBN */
/*
 * Automatic restart capability in bu_bomb().  The return from
 * BU_SETJUMP is the return from the setjmp().  It is 0 on the first
 * pass through, and non-zero when re-entered via a longjmp() from
 * bu_bomb().  This is only safe to use in non-parallel applications.
 */
#define BU_SETJUMP setjmp((bu_setjmp_valid=1, bu_jmpbuf))
#define BU_UNSETJUMP (bu_setjmp_valid=0)
/* These are global because BU_SETJUMP must be macro.  Please don't touch. */
BU_EXPORT extern int bu_setjmp_valid;		/* !0 = bu_jmpbuf is valid */
BU_EXPORT extern jmp_buf bu_jmpbuf;			/* for BU_SETJUMP() */


/**
 * Prepare 'nsemaphores' independent critical section semaphores.  Die
 * on error.
 *
 * Takes the place of 'n' separate calls to old RES_INIT().  Start by
 * allocating array of "struct bu_semaphores", which has been arranged
 * to contain whatever this system needs.
 *
 */
BU_EXPORT extern void bu_semaphore_init(unsigned int nsemaphores);

/**
 * Release all initialized semaphores and any associated memory.
 */
BU_EXPORT extern void bu_semaphore_free(void);

/**
 * Prepare 'nsemaphores' independent critical section semaphores.  Die
 * on error.
 */
BU_EXPORT extern void bu_semaphore_reinit(unsigned int nsemaphores);

BU_EXPORT extern void bu_semaphore_acquire(unsigned int i);

BU_EXPORT extern void bu_semaphore_release(unsigned int i);

/** @} */

/** @file libbu/str.c
 *
 * Compatibility routines to various string processing functions
 * including strlcat and strlcpy.
 *
 */

/**
 * concatenate one string onto the end of another, returning the
 * length of the dst string after the concatenation.
 *
 * bu_strlcat() is a macro to bu_strlcatm() so that we can report the
 * file name and line number of any erroneous callers.
 */
BU_EXPORT extern size_t bu_strlcatm(char *dst, const char *src, size_t size, const char *label);
#define bu_strlcat(dst, src, size) bu_strlcatm(dst, src, size, BU_FLSTR)

/**
 * copies one string into another, returning the length of the dst
 * string after the copy.
 *
 * bu_strlcpy() is a macro to bu_strlcpym() so that we can report the
 * file name and line number of any erroneous callers.
 */
BU_EXPORT extern size_t bu_strlcpym(char *dst, const char *src, size_t size, const char *label);
#define bu_strlcpy(dst, src, size) bu_strlcpym(dst, src, size, BU_FLSTR)

/**
 * Given a string, allocate enough memory to hold it using
 * bu_malloc(), duplicate the strings, returns a pointer to the new
 * string.
 *
 * bu_strdup() is a macro that includes the current file name and line
 * number that can be used when bu debugging is enabled.
 */
BU_EXPORT extern char *bu_strdupm(const char *cp, const char *label);
#define bu_strdup(s) bu_strdupm(s, BU_FLSTR)

/**
 * Compares two strings for equality.  It performs the comparison more
 * robustly than the standard library's strcmp() function by defining
 * consistent behavior for NULL and empty strings.  It accepts NULL as
 * valid input values and considers "" and NULL as equal.  Returns 0
 * if the strings match.
 */
BU_EXPORT extern int bu_strcmp(const char *string1, const char *string2);

/**
 * Compares two strings for equality.  No more than n-characters are
 * compared.  It performs the comparison more robustly than the
 * standard library's strncmp() function by defining consistent
 * behavior for NULL and empty strings.  It accepts NULL as valid
 * input values and considers "" and NULL as equal.  Returns 0 if the
 * strings match.
 */
BU_EXPORT extern int bu_strncmp(const char *string1, const char *string2, size_t n);

/**
 * Compares two strings for equality without regard for the case in
 * the string.  It performs the comparison more robustly than the
 * standard strcasecmp()/stricmp() function by defining consistent
 * behavior for NULL and empty strings.  It accepts NULL as valid
 * input values and considers "" and NULL as equal.  Returns 0 if the
 * strings match.
 */
BU_EXPORT extern int bu_strcasecmp(const char *string1, const char *string2);

/**
 * Compares two strings for equality without regard for the case in
 * the string.  No more than n-characters are compared.  It performs
 * the comparison more robustly than the standard
 * strncasecmp()/strnicmp() function by defining consistent behavior
 * for NULL and empty strings.  It accepts NULL as valid input values
 * and considers "" and NULL as equal.  Returns 0 if the strings
 * match.
 */
BU_EXPORT extern int bu_strncasecmp(const char *string1, const char *string2, size_t n);

/**
 * BU_STR_EMPTY() is a convenience macro that tests a string for
 * emptiness, i.e. "" or NULL.
 */
#define BU_STR_EMPTY(s) (bu_strcmp((s), "") == 0)

/**
 * BU_STR_EQUAL() is a convenience macro for testing two
 * null-terminated strings for equality.  It is equivalent to
 * (bu_strcmp(s1, s2) == 0) whereby NULL strings are allowed and
 * equivalent to an empty string.  Evaluates true when the strings
 * match and false if they do not.
 */
#define BU_STR_EQUAL(s1, s2) (bu_strcmp((s1), (s2)) == 0)

/**
 * BU_STR_EQUIV() is a convenience macro that compares two
 * null-terminated strings for equality without regard for case.  Two
 * strings are equivalent if they are a case-insensitive match.  NULL
 * strings are allowed and equivalent to an empty string.  Evaluates
 * true if the strings are similar and false if they are not.
 */
#define BU_STR_EQUIV(s1, s2) (bu_strcasecmp((s1), (s2)) == 0)


/** @file escape.c
 *
 * These routines implement support for escaping and unescaping
 * generalized strings that may represent filesystem paths, URLs,
 * object lists, and more.
 *
 */

/**
 * Escapes an input string with preceding '\'s for any characters
 * defined in the 'expression' string.  The input string is written to the
 * specified output buffer of 'size' capacity.  The input and output
 * pointers may overlap or be the same memory (assuming adequate space
 * is available).  If 'output' is NULL, then dynamic memory will be
 * allocated and returned.
 *
 * The 'expression' parameter is a regular "bracket expression"
 * commonly used in globbing and POSIX regular expression character
 * matching.  An expression can be either a matching list (default) or
 * a non-matching list (starting with a circumflex '^' character).
 * For example, "abc" matches any of the characters 'a', 'b', or 'c'.
 * Specifying a non-matching list expression matches any character
 * except for the ones listed after the circumflex.  For example,
 * "^abc" matches any character except 'a', 'b', or 'c'.
 *
 * Backslash escape sequences are not allowed (e.g., \t or \x01) as
 * '\' will be matched literally.
 *
 * A range expression consists of two characters separated by a hyphen
 * and will match any single character between the two characters.
 * For example, "0-9a-c" is equivalent to "0123456789abc".  To match a
 * '-' dash literally, include it as the last or first (after any '^')
 * character within the expression.
 *
 * The expression may also contain named character classes but only
 * for ASCII input strings:
 *
 * [:alnum:] Alphanumeric characters: a-zA-Z0-9
 * [:alpha:] Alphabetic characters: a-zA-Z
 * [:blank:] Space and TAB characters
 * [:cntrl:] Control characters: ACSII 0x00-0X7F
 * [:digit:] Numeric characters: 0-9
 * [:graph:] Characters that are both printable and visible: ASCII 0x21-0X7E
 * [:lower:] Lowercase alphabetic characters: a-z
 * [:print:] Visible and space characters (not control characters): ASCII 0x20-0X7E
 * [:punct:] Punctuation characters (not letters, digits, control, or space): ][!"#$%&'()*+,./:;<=>?@^_`{|}~-\
 * [:upper:] Uppercase alphabetic characters: A-Z
 * [:xdigit:] Hexadecimal digits: a-fA-F0-9
 * [:word:] (non-POSIX) Alphanumeric plus underscore: a-zA-Z0-9_
 *
 * A non-NULL output string is always returned.  This allows
 * expression chaining and embedding as function arguments but care
 * should be taken to free the dynamic memory being returned when
 * 'output' is NULL.
 *
 * If output 'size' is inadequate for holding the escaped input
 * string, bu_bomb() is called.
 *
 * Example:
 *   char *result;
 *   char buf[128];
 *   result = bu_str_escape("my fair lady", " ", buf, 128);
 *   :: result == buf == "my\ fair\ lady"
 *   result = bu_str_escape(buf, "\", NULL, 0);
 *   :: result == "my\\ fair\\ lady"
 *   :: buf == "my\ fair\ lady"
 *   bu_free(result, "bu_str_escape");
 *   result = bu_str_escape(buf, "a-zA-Z", buf, 128);
 *   :: result == buf == "\m\y\ \f\a\i\r\ \l\a\d\y"
 *
 * This function should be thread safe and re-entrant if the
 * input/output buffers are not shared (and strlen() is threadsafe).
 */
BU_EXPORT extern char *bu_str_escape(const char *input, const char *expression, char *output, size_t size);

/**
 * Removes one level of '\' escapes from an input string.  The input
 * string is written to the specified output buffer of 'size'
 * capacity.  The input and output pointers may overlap or be the same
 * memory.  If 'output' is NULL, then dynamic memory will be allocated
 * and returned.
 *
 * A non-NULL output string is always returned.  This allows
 * expression chaining and embedding as function arguments but care
 * should be taken to free the dynamic memory being returned when
 * 'output' is NULL.
 *
 * If output 'size' is inadequate for holding the unescaped input
 * string, bu_bomb() is called.
 *
 * Example:
 *   char *result;
 *   char buf[128];
 *   result = bu_str_unescape("\m\y\\ \f\a\i\r\\ \l\a\d\y", buf, 128);
 *   :: result == buf == "my\ fair\ lady"
 *   result = bu_str_unescape(buf, NULL, 0);
 *   :: result == "my fair lady"
 *   :: buf == "my\ fair\ lady"
 *   bu_free(result, "bu_str_unescape");
 *
 * This function should be thread safe and re-entrant if the
 * input/output buffers are not shared (and strlen() is threadsafe).
 */
BU_EXPORT extern char *bu_str_unescape(const char *input, char *output, size_t size);


/** @} */


/** @addtogroup hton */
/** @{ */
/** @file libbu/htester.c
 *
 * @brief
 * Test network float conversion.
 *
 * Expected to be used in pipes, or with TTCP links to other machines,
 * or with files RCP'ed between machines.
 *
 */

/** @file libbu/xdr.c
 *
 * DEPRECATED.
 *
 * Routines to implement an external data representation (XDR)
 * compatible with the usual InterNet standards, e.g.:
 * big-endian, twos-compliment fixed point, and IEEE floating point.
 *
 * Routines to insert/extract short/long's into char arrays,
 * independent of machine byte order and word-alignment.
 * Uses encoding compatible with routines found in libpkg,
 * and BSD system routines htonl(), htons(), ntohl(), ntohs().
 *
 */

/**
 * DEPRECATED: use ntohll()
 * Macro version of library routine bu_glonglong()
 * The argument is expected to be of type "unsigned char *"
 */
#define BU_GLONGLONG(_cp)	\
    ((((uint64_t)((_cp)[0])) << 56) |	\
     (((uint64_t)((_cp)[1])) << 48) |	\
     (((uint64_t)((_cp)[2])) << 40) |	\
     (((uint64_t)((_cp)[3])) << 32) |	\
     (((uint64_t)((_cp)[4])) << 24) |	\
     (((uint64_t)((_cp)[5])) << 16) |	\
     (((uint64_t)((_cp)[6])) <<  8) |	\
     ((uint64_t)((_cp)[7])))
/**
 * DEPRECATED: use ntohl()
 * Macro version of library routine bu_glong()
 * The argument is expected to be of type "unsigned char *"
 */
#define BU_GLONG(_cp)	\
    ((((uint32_t)((_cp)[0])) << 24) |	\
     (((uint32_t)((_cp)[1])) << 16) |	\
     (((uint32_t)((_cp)[2])) <<  8) |	\
     ((uint32_t)((_cp)[3])))
/**
 * DEPRECATED: use ntohs()
 * Macro version of library routine bu_gshort()
 * The argument is expected to be of type "unsigned char *"
 */
#define BU_GSHORT(_cp)	\
    ((((uint16_t)((_cp)[0])) << 8) | \
     (_cp)[1])

/**
 * DEPRECATED: use ntohs()
 */
DEPRECATED BU_EXPORT extern uint16_t bu_gshort(const unsigned char *msgp);

/**
 * DEPRECATED: use ntohl()
 */
DEPRECATED BU_EXPORT extern uint32_t bu_glong(const unsigned char *msgp);

/**
 * DEPRECATED: use htons()
 */
DEPRECATED BU_EXPORT extern unsigned char *bu_pshort(unsigned char *msgp, uint16_t s);

/**
 * DEPRECATED: use htonl()
 */
DEPRECATED BU_EXPORT extern unsigned char *bu_plong(unsigned char *msgp, uint32_t l);

/**
 * DEPRECATED: use htonll()
 */
DEPRECATED BU_EXPORT extern unsigned char *bu_plonglong(unsigned char *msgp, uint64_t l);

/** @} */

/** @addtogroup tcl */
/** @{ */
/** @file libbu/observer.c
 *
 * @brief
 * Routines for implementing the observer pattern.
 *
 */

/**
 * runs a given command, calling the corresponding observer callback
 * if it matches.
 */
BU_EXPORT extern int bu_observer_cmd(void *clientData, int argc, const char *argv[]);

/**
 * Notify observers.
 */
BU_EXPORT extern void bu_observer_notify(Tcl_Interp *interp, struct bu_observer *headp, char *self);

/**
 * Free observers.
 */
BU_EXPORT extern void bu_observer_free(struct bu_observer *);

/**
 * Bu_Init
 *
 * Allows LIBBU to be dynamically loaded to a vanilla tclsh/wish with
 * "load /usr/brlcad/lib/libbu.so"
 *
 * @param interp	- tcl interpreter in which this command was registered.
 *
 * @return BRLCAD_OK if successful, otherwise, BRLCAD_ERROR.
 */
BU_EXPORT extern int Bu_Init(void *interp);


/** @} */

#include "bu/hash.h"

/** @addtogroup file */
/** @{ */
/** @file libbu/fchmod.c
 *
 * Wrapper around fchmod.
 *
 */

/**
 * Portable wrapper for setting a file descriptor's permissions ala
 * fchmod().
 */
BU_EXPORT extern int bu_fchmod(int fd, unsigned long pmode);


/** @file libbu/argv.c
 *
 * Functions related to argv processing.
 *
 */

/**
 * Build argv[] array from input buffer, by splitting whitespace
 * separated "words" into null terminated strings.
 *
 * 'lim' indicates the maximum number of elements that can be stored
 * in the argv[] array not including a terminating NULL.
 *
 * The 'lp' input buffer is altered by this process.  The argv[] array
 * points into the input buffer.
 *
 * The argv[] array needs to have at least lim+1 pointers allocated
 * for lim items plus a terminating pointer to NULL.  The input buffer
 * should not be freed until argv has been freed or passes out of
 * scope.
 *
 * Returns -
 * 0	no words in input
 * argc	number of words of input, now in argv[]
 */
BU_EXPORT extern size_t bu_argv_from_string(char *argv[],
					    size_t lim,
					    char *lp);

/**
 * Deallocate all strings in a given argv array and the array itself
 *
 * This call presumes the array has been allocated with bu_dup_argv()
 * or bu_argv_from_path().
 */
BU_EXPORT extern void bu_free_argv(int argc, char *argv[]);

/**
 * free up to argc elements of memory allocated to an array without
 * free'ing the array itself.
 */
BU_EXPORT extern void bu_free_array(int argc, char *argv[], const char *str);

/**
 * Dynamically duplicate an argv array and all elements in the array
 *
 * Duplicate an argv array by duplicating all strings and the array
 * itself.  It is the caller's responsibility to free the array
 * returned including all elements in the array by calling bu_free()
 * or bu_free_argv().
 */
BU_EXPORT extern char **bu_dup_argv(int argc, const char *argv[]);

/**
 * Combine two argv arrays into one new (duplicated) argv array.
 *
 * If insert is negative, the insertArgv array elements will be
 * prepended into the new argv array.  If insert is greater than or
 * equal to argc, the insertArgv array elements will be appended after
 * all duplicated elements in the specified argv array.  Otherwise,
 * the insert argument is the position where the insertArgv array
 * elements will be merged with the specified argv array.
 */
BU_EXPORT extern char **bu_dupinsert_argv(int insert, int insertArgc, const char *insertArgv[], int argc, const char *argv[]);

/**
 * Generate an argv array from a path
 *
 * Given a path string, separate the path elements into a dynamically
 * allocated argv array with the path separators removed.  It is the
 * caller's responsibility to free the array that is returned as well
 * as all elements in the array using bu_free_argv() or manually
 * calling bu_free().
 */
BU_EXPORT extern char **bu_argv_from_path(const char *path, int *ac);


/** @file libbu/interrupt.c
 *
 * Routines for managing signals.  In particular, provide a common
 * means to temporarily buffer processing a signal during critical
 * write operations.
 *
 */

/**
 * Defer signal processing and interrupts before critical sections.
 *
 * Signal processing for a variety of signals that would otherwise
 * disrupt the logic of an application are put on hold until
 * bu_restore_interrupts() is called.
 *
 * If an interrupt signal is received while suspended, it will be
 * raised when/if interrupts are restored.
 *
 * Returns 0 on success.
 * Returns non-zero on error (with perror set if signal() failure).
 */
BU_EXPORT extern int bu_suspend_interrupts(void);

/**
 * Resume signal processing and interrupts after critical sections.
 *
 * If a signal was raised since bu_suspend_interrupts() was called,
 * the previously installed signal handler will be immediately called
 * albeit only once even if multiple signals were received.
 *
 * Returns 0 on success.
 * Returns non-zero on error (with perror set if signal() failure).
 */
BU_EXPORT extern int bu_restore_interrupts(void);

/** @} */

/** @addtogroup file */
/** @{ */
/** @file libbu/simd.c
 * Detect SIMD type at runtime.
 */

#define BU_SIMD_SSE4_2 7
#define BU_SIMD_SSE4_1 6
#define BU_SIMD_SSE3 5
#define BU_SIMD_ALTIVEC 4
#define BU_SIMD_SSE2 3
#define BU_SIMD_SSE 2
#define BU_SIMD_MMX 1
#define BU_SIMD_NONE 0
/**
 * Detect SIMD capabilities at runtime.
 */
BU_EXPORT extern int bu_simd_level(void);

/**
 * Detect if requested SIMD capabilities are available at runtime.
 * Returns 1 if they are, 0 if they are not.
 */
BU_EXPORT extern int bu_simd_supported(int level);

/** @} */

/** @addtogroup file */
/** @{ */
/** @file libbu/timer.c
 * Return microsecond accuracy time information.
 */
BU_EXPORT extern int64_t bu_gettime(void);

/** @} */

/** @addtogroup file */
/** @{ */
/**
 * Evaluate the time_t input as UTC time in ISO format.
 *
 * The UTC time is written into the user-provided bu_vls struct and is
 * also returned and guaranteed to be a non-null result, returning a
 * static "NULL" UTC time if an error is encountered.
 */
BU_EXPORT void bu_utctime(struct bu_vls *utc_result, const int64_t time_val);

/** @} */

/** @addtogroup file */
/** @{ */
/** @file libbu/dlfcn.c
 * Dynamic Library functionality
 */
#ifdef HAVE_DLOPEN
# define BU_RTLD_LAZY RTLD_LAZY
# define BU_RTLD_NOW RTLD_NOW
# define BU_RTLD_GLOBAL RTLD_GLOBAL
# define BU_RTLD_LOCAL RTLD_LOCAL
#else
# define BU_RTLD_LAZY 1
# define BU_RTLD_NOW 2
# define BU_RTLD_GLOBAL 0x100
# define BU_RTLD_LOCAL 0
#endif
BU_EXPORT extern void *bu_dlopen(const char *path, int mode);
BU_EXPORT extern void *bu_dlsym(void *path, const char *symbol);
BU_EXPORT extern int bu_dlclose(void *handle);
BU_EXPORT extern const char *bu_dlerror(void);

/** NEW: Do not use. */
BU_EXPORT extern int bu_fseek(FILE *stream, off_t offset, int origin);
/** NEW: Do not use. */
BU_EXPORT extern off_t bu_ftell(FILE *stream);

/** @} file */


/** @file libbu/ctype.c
 *
 * Routines for checking ctypes.
 *
 */
BU_EXPORT extern int bu_str_isprint(const char *cp);

/**
 * Get the current operating host's name.  This is usually also the
 * network name of the current host.  The name is written into the
 * provided hostname buffer of at least len size.  The hostname is
 * always null-terminated and should be sized accordingly.
 */
BU_EXPORT extern int bu_gethostname(char *hostname, size_t len);


/** @addtogroup file */
/** @{ */
/** @file libbu/sort.c
 * platform-independent re-entrant version of qsort, where the first argument
 * is the array to sort, the second the number of elements inside the array, the
 * third the size of one element, the fourth the comparison-function and the
 * fifth a variable which is handed as a third argument to the comparison-function.
 */
BU_EXPORT extern void bu_sort(void *array, size_t nummemb, size_t sizememb,
            int (*compare)(const void *, const void *, void *), void *context);
/** @} */

__END_DECLS

#endif  /* BU_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
