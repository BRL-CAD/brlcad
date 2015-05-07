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

/** @addtogroup path
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
/** @file libbu/path.c */
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
BU_EXPORT extern void bu_basename(char *basename, const char *path);

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
    PATH_DIRNAME = 0,   /*!< /dir1/dir2 */
    PATH_DIRNAME_CORE,  /*!< /dir1/dir2/file */
    PATH_BASENAME,      /*!< file.ext */
    PATH_BASENAME_CORE, /*!< file */
    PATH_EXTENSION      /*!< ext */
} path_component_t;


/**
 * Attempt to extract a component from a file path.
 *
 * returns 0 if the specified component was not found, 1
 * if it was.  If the bu_vls pointer component is not NULL,
 * the component will be written to the vls.
 */

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
 * Function fnmatch() as specified in POSIX 1003.2-1992, section B.6.
 * Compares a string filename or pathname to a pattern.
 *
 * Returns 0 if a match is found or 1 otherwise.
 *
 */
BU_EXPORT extern int bu_fnmatch(const char *pattern, const char *pathname, int flags);



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
