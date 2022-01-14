/*                         P A T H . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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

#include "bu/defines.h"
#include "bu/magic.h"
#include "bu/vls.h"

__BEGIN_DECLS

/** @addtogroup bu_path
 *
 * @brief Routines for manipulating path strings.
 *
 * operating system and geometry
 * database path strings.
 *
 * Typically being used for operating system and geometry database
 * path strings, routines in this header do NOT have knowledge of or
 * access the filesystem.  They will not check for the presence or
 * absence of objects on disk or in a geometry database - they operate
 * only on the the path string itself.  Any validation of the path is
 * the responsibility of the caller or other routines.
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
BU_EXPORT extern char *bu_path_dirname(const char *path);


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
 * basename size.  If the basename output argument is NULL, then
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
BU_EXPORT extern char *bu_path_basename(const char *path, char *basename);


/**
 * Normalize a path according to rules used for realpath, but
 * without filesystem (or database object) validation.
 *
 * @return
 * A STATIC buffer is returned.  It is the caller's responsibility to
 * call bu_strdup() or make other provisions to save the returned
 * string, before calling again.
 */
BU_EXPORT extern const char *bu_path_normalize(const char *path);


/**
 * Components of a path recognized by bu_path_component(), identified
 * below in the context of the example path:
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
 * Identify components of a file path such as the filename (basename),
 * file extension, directory path (dirname) and more.
 *
 * Returns truthfully (non-zero) if the specified component type was
 * found in the provided path.  A copy of the component will be
 * written to the 'component' vls if non-NULL.
 */

BU_EXPORT extern int bu_path_component(struct bu_vls *component, const char *path, bu_path_component_t type);


/**
 * Generate an argv array from a path
 *
 * Given a path string, separate the path elements into a dynamically
 * allocated argv array with the path separators removed.  It is the
 * caller's responsibility to free the array that is returned as well
 * as all elements in the array using bu_argv_free() or manually
 * calling bu_free().
 */
BU_EXPORT extern char **bu_path_to_argv(const char *path, int *ac);

#define BU_PATH_MATCH_NOMATCH     1

#define BU_PATH_MATCH_NOESCAPE    0x01 /**< bu_path_match() flag:
					* Backslash is ordinary
					* character, not an escape
					* character.
					*/
#define BU_PATH_MATCH_PATHNAME    0x02 /**< bu_path_match() flag:
					* Match a slash in pathname
					* only with a slash in pattern
					* and not by an asterisk or a
					* question mark metacharacter,
					* nor by a bracket expression
					* containing a slash.
					*/
#define BU_PATH_MATCH_PERIOD      0x04 /**< bu_path_match() flag:
					* Leading period in pathname
					* has to be matched exactly by
					* a period in pattern.
					*/

#define BU_PATH_MATCH_LEADING_DIR 0x08 /**< bu_path_match() flag:
					* Ignore /<tail> after Imatch.
					*/

#define BU_PATH_MATCH_CASEFOLD    0x10 /**< bu_path_match() flag:
					* Case-insensitive
					* matching.
					*/
/**
 * Compares a string filename or pathname to a pattern.
 *
 * This is a portable implementation of the fnmatch() function as
 * specified in POSIX 1003.2-1992, section B.6.
 *
 * Returns 0 if a match is found or 1 otherwise.
 *
 */
BU_EXPORT extern int bu_path_match(const char *pattern, const char *pathname, int flags);


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
