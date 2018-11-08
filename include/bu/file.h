/*                         F I L E . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2018 United States Government as represented by
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

#ifndef BU_FILE_H
#define BU_FILE_H

#include "common.h"

#include <stdio.h> /* for FILE */
#include <sys/types.h> /* for off_t */
#include <stddef.h> /* for size_t */
#include <stdlib.h> /* for getenv */

#include "bu/defines.h"
#include "bu/magic.h"
#include "bu/vls.h"


__BEGIN_DECLS


/** @addtogroup bu_file
 *
 * @brief Routines for handling, inspecting, and comparing files and
 * directories.
 *
 * These routines involve filesystem interaction such as reporting
 * whether a file exists or is of a particular type.
 *
 */
/** @{ */
/** @file bu/file.h */

/**
 * Returns truthfully whether the given file path exists or not.  An
 * empty or NULL path name is treated as a non-existent file and, as
 * such, will return false.  If fd is non-NULL, it will be set to a
 * read-only open file descriptor for the provided path.
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
BU_EXPORT extern int bu_file_same(const char *fn1, const char *fn2);

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


#if 0
/**
 * TODO - currently unimplemented
 *
 * matches a filepath pattern to directory entries.  if non-NULL,
 * matching paths are dynamically allocated, stored into the provided
 * 'matches' array, and followed by a terminating NULL entry.
 *
 * If '*matches' is NULL, the caller is expected to free the matches
 * array with bu_argv_free() If '*matches' is non-NULL (i.e., string
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

#endif


/**
 * Returns the number of directory entries for a given path matching
 * an optional glob pattern.  If the caller provides a pointer to an
 * argv-style 'files' array, this function will dynamically allocate
 * an array of strings, filled with the sorted listing of matching
 * file(s).
 *
 * It is the caller's responsibility to free a non-NULL array with
 * bu_argv_free().
 */
BU_EXPORT extern size_t bu_file_list(const char *path, const char *pattern, char ***files);


/**
 * This routine expands a path to a resolved canonical full path.
 *
 * Returns a pointer to the canonical path. If resolved_path is NULL,
 * caller is responsible for freeing the returned path via bu_free.
 * If supplying a non-NULL resolved_path buffer, it must hold at least
 * MAXPATHLEN characters.
 */
BU_EXPORT extern char *bu_file_realpath(const char *path, char *resolved_path);


/** NEW: Do not use. */
BU_EXPORT extern int bu_fseek(FILE *stream, off_t offset, int origin);
/** NEW: Do not use. */
BU_EXPORT extern off_t bu_ftell(FILE *stream);

/** @} */

__END_DECLS

#endif  /* BU_FILE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
