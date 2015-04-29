/*                         P A T H . H
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

/**  @defgroup io Input/Output */
/**   @defgroup path Path Processing */

/** @file path.h
 *
 */
#ifndef BU_PATH_H
#define BU_PATH_H

#include "common.h"

#include <stddef.h> /* for size_t */
#include <stdlib.h> /* for getenv */

#ifdef HAVE_DLFCN_H
#  include <dlfcn.h>   /* for RTLD_* */
#endif

#include "bu/defines.h"
#include "bu/magic.h"
#include "bu/vls.h"

__BEGIN_DECLS

/** @addtogroup path */
/** @{ */
/** @file libbu/path.c */

/**
 * Support routines for working with path strings, both
 * filesystem paths and .g database hierarchy paths
 *
 * Routines in this header have no knowledge of the
 * file system, beyond an awareness of the path separator
 * in use in the working operating system environment.
 */

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
 *      /usr/dir/file   /usr/dir
 * @n   /usr/dir/       /usr
 * @n   /usr/file       /usr
 * @n   /usr/           /
 * @n   /usr            /
 * @n   /               /
 * @n   .               .
 * @n   ..              .
 * @n   usr             .
 * @n   a/b             a
 * @n   a/              .
 * @n   ../a/b          ../a
 *
 * This routine will return "." if other valid results are not available
 * but should never return NULL.
 */
BU_EXPORT extern char *bu_dirname(const char *path);


/**
 * A libbu re-implementation of basename() for cross-platform
 * compatibility.
 *
 * Given a string containing a hierarchical path, return a dynamic
 * string to the portion after the last path separator.
 *
 * This function is similar if not identical to most basename() BSD
 * system function implementations; but that system function cannot be
 * used due to significantly inconsistent behavior across platforms.
 *
 * This function always recognizes paths separated by a '/' (i.e.,
 * geometry paths) as well as whatever the native platform directory
 * separator may be.  It is assumed that all file and directory names
 * in the path will not contain a path separator, even if escaped.
 *
 * It is the caller's responsibility to allocate basename with
 * enough memory to hold a string with length strlen(path), since
 * that is the maximum possible size of bu_basename's output.
 *
 * Examples of strings returned:
 *
 *      /usr/dir/file   file
 * @n   /usr/dir/       dir
 * @n   /usr/           usr
 * @n   /usr            usr
 * @n   /               /
 * @n   .               .
 * @n   ..              ..
 * @n   usr             usr
 * @n   a/b             b
 * @n   a/              a
 * @n   ///             /
 */
BU_EXPORT extern void bu_basename(char *basename, const char *path);

/**
 * Normalize a path according to rules used for realpath, but
 * without filesystem (or database object) validation.
 *
 * @return
 * A STATIC buffer is returned.  It is the caller's responsibility to
 * call bu_strdup() or make other provisions to save the returned
 * string, before calling again.
 *
 * TODO - need to do better with Windows - they can use either back slash
 * or forward slash in paths, so BU_DIR_SEPARATOR isn't enough.  By the same
 * token, back slash is also used as a quote.  How do we handle this?
 */
BU_EXPORT extern const char *bu_normalize(const char *path);

/**
 * Attempts to extract a component from a file path.
 * Supported components are illustrated below with the
 * example path:
 *
 * /dir1/dir2/file.ext
 *
 * PATH_DIRNAME         /dir1/dir2
 * PATH_DIRNAME_CORE    /dir1/dir2/file
 * PATH_BASENAME        file.ext
 * PATH_BASENAME_CORE   file
 * PATH_EXTENSION       ext
 *
 * returns 0 if the specified component was not found, 1
 * if it was.  If the bu_vls pointer component is not NULL,
 * the component will be written to the vls.
 */

typedef enum {
    PATH_DIRNAME,
    PATH_DIRNAME_CORE,
    PATH_BASENAME,
    PATH_BASENAME_CORE,
    PATH_EXTENSION
} path_component_t;

BU_EXPORT extern int bu_path_component(struct bu_vls *component,
	                               const char *path,
	                               path_component_t type);


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



/** @file libbu/fnmatch.c
 *
 */

#define BU_FNMATCH_NOESCAPE    0x01 /**< bu_fnmatch() flag.  Backslash escaping. */
#define BU_FNMATCH_PATHNAME    0x02 /**< bu_fnmatch() flag.  Slash must be matched by slash. */
#define BU_FNMATCH_PERIOD      0x04 /**< bu_fnmatch() flag.  Period must be matched by period. */
#define BU_FNMATCH_LEADING_DIR 0x08 /**< bu_fnmatch() flag.  Ignore `/<tail>` after Imatch. */
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



/*################################*/
/* BRL-CAD specific path routines */
/*################################*/


/**@file libbu/brlcad_path.c
 *
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

/**
 * @brief
 * A support routine to provide the executable code with the path
 * to where the BRL-CAD programs and libraries are installed.
 *
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
