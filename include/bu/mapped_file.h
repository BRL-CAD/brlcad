/*                    M A P P E D _ F I L E . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2025 United States Government as represented by
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

#ifndef BU_MAPPED_FILE_H
#define BU_MAPPED_FILE_H

#include "common.h"

#include <sys/types.h> /* for time_t */
#include <stddef.h> /* for size_t */

#include "bu/defines.h"
#include "bu/list.h"


__BEGIN_DECLS

/** @addtogroup bu_mf
 *
 * @brief
 * Routines for sharing large read-only data files.
 *
 * Typical use cases include height fields,
 * bit map solids, texture maps, etc.  Uses memory mapped files where
 * available.
 *
 * Each instance of the file has the raw data available as element
 * "buf".  If a particular application needs to transform the raw data
 * in a manner that is identical across all uses of that application
 * (e.g. height fields, EBMs, etc.), then the application should
 * provide a non-null "appl" string, to tag the format of the "apbuf".
 * This will keep different applications from sharing that instance of
 * the file.
 *
 * Thus, if the same filename is opened for interpretation as both an
 * EBM and a height field, they will be assigned different mapped file
 * structures, so that the "apbuf" pointers are distinct.
 *
 */

/** @{ */
/** @file bu/mapped_file.h */

/**
 * @struct bu_mapped_file bu/mapped_file.h
 *
 * Structure for opening a mapped file.
 *
 * Each file is opened and mapped only once (per application, as
 * tagged by the string in "appl" field).  Subsequent opens require an
 * exact match on both strings.
 *
 * Before allocating apbuf and performing data conversion into it,
 * openers should check to see if the file has already been opened and
 * converted previously.
 *
 * When used in RT, the mapped files are not closed at the end of a
 * frame, so that subsequent frames may take advantage of the large
 * data files having already been read and converted.  Examples
 * include EBMs, texture maps, and height fields.
 *
 * For appl == "db_i", file is a ".g" database & apbuf is (struct db_i *).
 */
struct bu_mapped_file {
    char *name;		/**< bu_strdup() of file name */
    void *buf;	/**< In-memory copy of file (may be mmapped)  */
    size_t buflen;	/**< # bytes in 'buf'  */
    int is_mapped;	/**< 1=mmap() used, 0=bu_malloc/fread */
    char *appl;		/**< bu_strdup() of tag for application using 'apbuf'  */
    void *apbuf;	/**< opt: application-specific buffer */
    size_t apbuflen;	/**< opt: application-specific buflen */
    time_t modtime;	/**< date stamp, in case file is modified */
    int uses;		/**< # ptrs to this struct handed out */
    void *handle;       /**< PRIVATE - for internal file-specific implementation data */
};
typedef struct bu_mapped_file bu_mapped_file_t;
#define BU_MAPPED_FILE_NULL ((struct bu_mapped_file *)0)

/**
 * macro suitable for declaration statement initialization of a
 * bu_mapped_file struct.  does not allocate memory.
 */
#define BU_MAPPED_FILE_INIT_ZERO { NULL, NULL, 0, 0, NULL, NULL, 0, 0, 0, NULL }

/**
 * Provides a standardized interface for acquiring the entire contents
 * of an existing file mapped into the current address space, using
 * the virtual memory capabilities of the operating system (such as
 * mmap()) where available, or by allocating sufficient dynamic memory
 * and reading the entire file.
 *
 * If the file can not be opened, as descriptive an error message as
 * possible will be printed, to simplify code handling in the caller.
 *
 * Mapped files are always opened read-only.
 *
 * If the system does not support mapped files, the data is read into
 * memory.
 */
BU_EXPORT extern struct bu_mapped_file *bu_open_mapped_file(const char *name,
							    const char *appl);

/**
 * Release a use of a mapped file.  Because it may be re-used shortly,
 * e.g. by the next frame of an animation, don't release the memory
 * even on final close, so that it's available when next needed.
 *
 * Call bu_free_mapped_files() after final close to reclaim space.
 * But only do that if you're SURE that ALL these files will never
 * again need to be mapped by this process.  Such as when running
 * multi-frame animations.
 */
BU_EXPORT extern void bu_close_mapped_file(struct bu_mapped_file *mp);

BU_EXPORT extern void bu_pr_mapped_file(const char *title,
					const struct bu_mapped_file *mp);

/**
 * Release storage being used by mapped files with no remaining users.  This
 * will slow subsequent re-opening of those files (since files with no users
 * will be unmapped as part of the freeing process, they will have to be
 * re-mapped on a subsequent reopen.) Use cases where there is a possibility of
 * reopening such files in the future will generally want to postpone calling
 * this routine unless they need to free up memory.
 *
 * This entire routine runs inside a critical section, for parallel protection.
 */
BU_EXPORT extern void bu_free_mapped_files(int verbose);

/**
 * A wrapper for bu_open_mapped_file() which uses a search path to
 * locate the file.
 *
 * The search path is specified as a normal C argv array, terminated
 * by a null string pointer.  If the file name begins with a slash
 * ('/') the path is not used.
 */
BU_EXPORT extern struct bu_mapped_file *bu_open_mapped_file_with_path(char * const *path,
								      const char *name,
								      const char *appl);

/** @} */

__END_DECLS

#endif  /* BU_MAPPED_FILE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
