/*                         G L O B . H
 * BRL-CAD
 *
 * Copyright (c) 2015-2025 United States Government as represented by
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

#ifndef BU_GLOB_H
#define BU_GLOB_H

#include "common.h"

#include "bu/defines.h"
#include "bu/vls.h"

__BEGIN_DECLS


/** @addtogroup bu_experimental
 *
 * NOTE - the glob API below is a work in progress - until this notice is
 * removed it should not be considered functional, much less stable!
 *
 *  @brief Routines and structures for getting a list of entities that
 *  match a given pattern.
 *
 *  TODO - grab and adjust implementation from:
 *  https://github.com/openbsd/src/blob/master/lib/libc/gen/glob.c
 *
 */
/** @{ */
/** @file bu/glob.h */


/**
 * Representation of a path element, for use with bu_glob() callbacks.
 */
struct bu_dirent {
    struct bu_vls *name;
    void *data;
};

/**
 * Information about a path element (file, directory, object, etc),
 * for use with bu_glob() callbacks.
 */
struct bu_stat {
    struct bu_vls name;
    b_off_t size;
    void *data;
};


#define BU_GLOB_APPEND   0x0001 /* Append to output from previous call. */
#define BU_GLOB_DOOFFS   0x0002 /* Use gl_offs. */
#define BU_GLOB_ERR      0x0004 /* Return on error. */
#define BU_GLOB_MARK     0x0008 /* Append / to matching directories. */
#define BU_GLOB_NOCHECK  0x0010 /* Return pattern itself if nothing matches. */
#define BU_GLOB_NOSORT   0x0020 /* Don't sort. */
#define BU_GLOB_NOESCAPE 0x1000 /* Disable backslash escaping. */

#define BU_GLOB_NOSPACE (-1) /* Malloc call failed. */
#define BU_GLOB_ABORTED (-2) /* Unignored error. */
#define BU_GLOB_NOMATCH (-3) /* No match and GLOB_NOCHECK not set. */

#define BU_GLOB_BRACE    0x0080 /* Expand braces ala csh. */
#define BU_GLOB_MAGCHAR  0x0100 /* Pattern had globbing characters. */
#define BU_GLOB_NOMAGIC  0x0200 /* GLOB_NOCHECK without magic chars (csh). */
#define BU_GLOB_LIMIT    0x2000 /* Limit pattern match output to ARG_MAX */
#define BU_GLOB_KEEPSTAT 0x4000 /* Retain stat data for paths in gl_statv. */

/**
 * Used by bu_glob() to specify behavior and callbacks.  Also holds returned
 * results.
 */
struct bu_glob_context {

    int gl_flags;                /**< flags customizing globbing behavior */

    /* Return values */
    int gl_pathc;                /**< count of total paths so far */
    int gl_matchc;               /**< count of paths matching pattern */
    struct bu_vls **gl_pathv;    /**< list of paths matching pattern - TODO - should this be a bu_ptbl?*/

    /* Callback functions */
    struct bu_glob_context *(*gl_opendir)(const char *);
    int (*gl_readdir)(struct bu_dirent *, struct bu_glob_context *);
    void (*gl_closedir)(struct bu_glob_context *);

    int (*gl_lstat)(const char *, struct bu_stat *, struct bu_glob_context *);
    int (*gl_stat)(const char *, struct bu_stat *, struct bu_glob_context *);
    int (*gl_errfunc)(const char *, int, struct bu_glob_context *);

    /* For caller use */
    void *data;                  /**< data passed to all callbacks */

    /* Private */
    void *priv;               /**< For internal use only */
};
typedef struct bu_glob_context bu_glob_t;


/**
 * create and initialize a globbing context for use prior to calling bu_glob()
 */
BU_EXPORT struct bu_glob_context *bu_glob_create(void);


/**
 * release any resources allocated during bu_glob(), including any
 * returned paths
 */
BU_EXPORT extern void bu_glob_destroy(struct bu_glob_context *);


/**
 * match a pattern against a set of elements.
 *
 * This interface is a somewhat simplified and abstracted version of
 * UNIX glob matching, based loosely on the interface specified in
 * POSIX.2.  It supports user specified callback functions allowing
 * callers to glob nearly any named storage structure.  By default,
 * globbing will map to the local filesystem.
 *
 * Function takes an input pattern, a set of flags, and a globbing
 * context from bu_glob_alloc().
 *
 * Returns zero on success, non-zero on failure.
 *
 * gl_pathc will contain the total number of paths matched.  This will
 * increment previous glob counts if GLOB_APPEND is specified.
 *
 * gl_matchc will contain the number of matched paths for this
 * invocation of bu_glob().
 *
 * gl_pathv contains a list of matched paths.
 */
BU_EXPORT extern int bu_glob(const char *pattern, int flags, struct bu_glob_context *context);

/** @} */


__END_DECLS

#endif  /* BU_GLOB_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
