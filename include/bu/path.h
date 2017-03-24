/*                         P A T H . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2016 United States Government as represented by
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

#ifndef BU_PATH_H
#define BU_PATH_H

#include "common.h"

#include <stddef.h> /* for size_t */
#include <stdlib.h> /* for getenv */
#include <limits.h> /* for NAME_MAX */

#ifdef HAVE_DLFCN_H
#  include <dlfcn.h>   /* for RTLD_* */
#endif

#include "bu/defines.h"
#include "bu/magic.h"
#include "bu/vls.h"

__BEGIN_DECLS

/** @addtogroup bu_path
 *
 *  @brief
 *  Functionality for processing operating system and geometry database path
 *  strings.
 *
 *  Routines in this header have no knowledge of the file system,
 *  beyond an awareness of the path separator in use in the working operating
 *  system environment. They will not check for the presence or absence of
 *  objects on disk or in a geometry database - they operate only on the the
 *  path string itself.  Any validation of the path is the responsibility of
 *  the caller.
 */
/** @{ */
/** @file bu/path.h */

/**
 * Given a string containing a hierarchical path, return a dynamic
 * string to the parent path.
 *
 * This function is similar if not identical to most dirname() BSD
 * system function implementations; but that system function cannot be
 * used due to significantly inconsistent behavior across platforms.
 *
 * This function always recognizes paths separated by a '/' (i.e.,
 * geometry paths) as well as whatever the native platform directory
 * separator may be.  It is assumed that all file and directory names
 * in the path will not contain a path separator, even if escaped.
 *
 * It is the caller's responsibility to bu_free() the pointer returned
 * from this routine.
 *
 * Examples of strings returned:
 *
 * Input String  | Output String
 * ------------- | -------------
 * /usr/dir/file | /usr/dir
 * /usr/dir/     | /usr
 * /usr/file     | /usr
 * /usr/         | /
 * /usr          | /
 * /             | /
 * .             | .
 * ..            | .
 * usr           | .
 * a/b           | a
 * a/            | .
 * ../a/b        | ../a
 *
 * This routine will return "." if other valid results are not available
 * but should never return NULL.
 */
BU_EXPORT extern char *bu_dirname(const char *path);


/**
 * A libbu re-implementation of basename() for cross-platform
 * compatibility.
 *
 * Given a string containing a hierarchically delimited path (e.g.,
 * /path/to/file), determine the file name portion after the last path
 * separator.
 *
 * This function is similar to most basename() BSD system function
 * implementations; but that system function cannot be used due to
 * significantly inconsistent behavior across platforms, particularly
 * with respect to memory allocation and threading behavior.
 *
 * This function always recognizes paths separated by a '/' (i.e.,
 * geometry paths) as well as whatever the native platform directory
 * separator may be.  It is assumed that all file and directory names
 * in the path do not contain a path separator.  That is, there is
 * currently no support for escaped characters.
 *
 * It is the caller's responsibility to provide a basename buffer with
 * enough memory for a string with length at least strlen(path) + 1
 * (for terminating nul) characters as that is the maximum possible
 * basename size.  If the basename output argument is NULL, then a
 * dynamic memory will be returned and the caller is responsible for
 * releasing memory (via bu_free()), otherwise the caller-provided
 * basename pointer is returned and managed by the caller accordingly.
 *
 * Examples of strings returned:
 *
 * Input String  | Output String
 * ------------- | -------------
 * /usr/dir/file | file
 * /usr/dir/     | dir
 * /usr/         | usr
 * /usr          | usr
 * /             | /
 * .             | .
 * ..            | ..
 * usr           | usr
 * a/b           | b
 * a/            | a
 * ///           | /
 *
 */
BU_EXPORT extern char *bu_basename(const char *path, char *basename);

/**
 * Normalize a path according to rules used for realpath, but
 * without filesystem (or database object) validation.
 *
 * @return
 * A STATIC buffer is returned.  It is the caller's responsibility to
 * call bu_strdup() or make other provisions to save the returned
 * string, before calling again.
  */
BU_EXPORT extern const char *bu_normalize(const char *path);

/**
 * Components of a path recognized by libbu, identified below in the
 * context of the example path:
 *
 *     /dir1/dir2/file.ext
 */
typedef enum {
    BU_PATH_DIRNAME,           /*!< /dir1/dir2 */
    BU_PATH_BASENAME,          /*!< file.ext */
    BU_PATH_BASENAME_EXTLESS,  /*!< file */
    BU_PATH_EXT,               /*!< ext */
    BU_PATH_EXTLESS,           /*!< /dir1/dir2/file */
    BU_PATH_UNKNOWN            /*!< marks end of bu_path_component_t enums */
} bu_path_component_t;

/**
 * Attempt to extract a component from a file path.
 *
 * returns 0 if the specified component was not found, 1 if it was.  If the
 * bu_vls pointer component is not NULL, the component will be written to the
 * vls.
 */

BU_EXPORT extern int bu_path_component(struct bu_vls *component,
	                               const char *path,
	                               bu_path_component_t type);


/**
 * Generate an argv array from a path
 *
 * Given a path string, separate the path elements into a dynamically
 * allocated argv array with the path separators removed.  It is the
 * caller's responsibility to free the array that is returned as well
 * as all elements in the array using bu_argv_free() or manually
 * calling bu_free().
 */
BU_EXPORT extern char **bu_argv_from_path(const char *path, int *ac);



#define BU_FNMATCH_NOESCAPE    0x01 /**< bu_fnmatch() flag.  Backslash escaping. */
#define BU_FNMATCH_PATHNAME    0x02 /**< bu_fnmatch() flag.  Slash must be matched by slash. */
#define BU_FNMATCH_PERIOD      0x04 /**< bu_fnmatch() flag.  Period must be matched by period. */
#define BU_FNMATCH_LEADING_DIR 0x08 /**< bu_fnmatch() flag.  Ignore `/<tail>` after Imatch. */
#define BU_FNMATCH_CASEFOLD    0x10 /**< bu_fnmatch() flag.  Case-insensitive searching. */

/**
 * Function fnmatch() as specified in POSIX 1003.2-1992, section B.6.
 * Compares a string filename or pathname to a pattern.
 *
 * Returns 0 if a match is found or 1 otherwise.
 *
 */
BU_EXPORT extern int bu_fnmatch(const char *pattern, const char *pathname, int flags);

#if 0
/* NOTE - the glob API below is a work in progress - until this notice is
 * removed it should not be considered functional, much less stable! */
#define BU_GLOB_APPEND     0x0001  /* Append to output from previous call. */
#define BU_GLOB_ERR        0x0004  /* Return on error. */
#define BU_GLOB_NOCHECK    0x0010  /* Return pattern itself if nothing matches. */
#define BU_GLOB_NOSORT     0x0020  /* Don't sort. */
#define BU_GLOB_NOESCAPE   0x1000  /* Disable backslash escaping. */

#define BU_GLOB_NOSPACE    (-1)    /* Malloc call failed. */
#define BU_GLOB_ABORTED    (-2)    /* Unignored error. */
#define BU_GLOB_NOMATCH    (-3)    /* No match and GLOB_NOCHECK not set. */
#define BU_GLOB_NOSYS      (-4)    /* Function not supported. */

#define BU_GLOB_BRACE      0x0080  /* Expand braces ala csh. */
#define BU_GLOB_MAGCHAR    0x0100  /* Pattern had globbing characters. */
#define BU_GLOB_NOMAGIC    0x0200  /* GLOB_NOCHECK without magic chars (csh). */
#define BU_GLOB_QUOTE      0x0400  /* Quote special chars with \. */
#define BU_GLOB_LIMIT      0x0800  /* Limit pattern match output to ARG_MAX */
#define	BU_GLOB_PERIOD	   0x2000  /* Allow metachars to match leading periods. */
#define	BU_GLOB_NO_DOTDIRS 0x4000  /* Make . and .. vanish from wildcards. */
#define	BU_GLOB_STAR       0x0040  /* Use glob ** to recurse w */
#define BU_GLOB_KEEPSTAT   0x8000  /* Retain stat data for paths in gl_statv. */

struct bu_dirent {
    char *d_name;
    void *d_data;
};

struct bu_stat {
    const char *name;
    void *s_data;
};

typedef struct {
    int gl_pathc;           /* Count of total paths so far. */
    int gl_matchc;          /* Count of paths matching pattern. */
    int gl_flags;           /* Copy of flags parameter to glob. */
    char **gl_pathv;        /* List of paths matching pattern. */
    struct bu_stat **gl_statv; /* Stat entries corresponding to gl_pathv */
    /* Optional error handling function. */
    int (*gl_errfunc)(const char *, int, void *dptr);
    /* Access methods for glob */
    void (*gl_closedir)(void *, void *dptr);
    int (*gl_readdir)(struct bu_dirent *, void *, void *dptr);
    void *(*gl_opendir)(const char *, void *dptr);
    int (*gl_lstat)(const char *, struct bu_stat *, void *dptr);
    int (*gl_stat)(const char *, struct bu_stat *, void *dptr);
    /* function to expand '~' */
    int (*e_tilde)(char *, void *dptr);
    /* Functions that know how to free bu_dirent's d_data slot and bu_stat'a s_data slot. */
    int (*d_free)(void *data, void *dptr);
    int (*s_free)(struct bu_stat *, void *dptr);
    /* Void pointer to allow additional data not explicitly specified in
     * bu_glob_t to be passed to various functions */
    void *dptr;
} bu_glob_t;

/**
 * bu_glob is a somewhat simplified and abstracted version of glob matching
 * based on the rules specified in POSIX.2, 3.13.  Unlike operating system
 * implementations of glob, bu_glob does not provide a default implementation
 * of file-system-specific functionality and does not use operating system
 * specific types such as dirent and DIR in its public API.  Some OS
 * implementations support user specified "alternative" directory functions to
 * allow glob to work with user-specified filesystems -  in the case of libbu
 * these functions have been made mandatory instead of optional. *All* callers
 * *must* supply functions, even if the desired behavior is the "typical"
 * interaction with the operating system's filesystems.  This separates I/O
 * logic from the core "globbing" logic.
 *
 * In addition to the "usual" callback functions needed by alternative
 * directory implementations, bu_glob callers must also supply callbacks that
 * handle the interpretation of special characters such as '~' - while this has
 * a standard meaning on Unix-style operating systems, that meaning does not
 * translate directly to (for example) .g database hierarchies.
 *
 * Note that by default, a pattern with no matches will return an empty list
 * rather than the unevaluated wildcard (the current POSIX specified behaviour).
 */
BU_EXPORT extern int bu_glob(const char *, int, bu_glob_t *);

/** Free a bu_glob_t structure, including any returned results */
BU_EXPORT extern void bu_globfree(bu_glob_t *);
/** @} */

/* Possible TODO - add a bu_fs_glob for portable filesystem globbing (a closer
 * analogy to the actual system glob) if there is enough need for cross
 * platform filesystem globbing to warrant it - the primary use case for this
 * right now is the .g database */
#endif

/** @} */

__END_DECLS

#endif  /* BU_PATH_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
