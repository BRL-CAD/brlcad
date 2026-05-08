/*                         G L O B . H
 * BRL-CAD
 *
 * Copyright (c) 2015-2026 United States Government as represented by
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


/** @addtogroup bu_glob
 *
 *  @brief Routines and structures for getting a list of entities that
 *  match a given pattern.
 *
 *  The bu_glob API is a portable, callback-based glob implementation
 *  modelled on POSIX glob(3) and inspired by OpenBSD's glob.c.  By
 *  supplying custom opendir/readdir/closedir/lstat/stat callbacks any
 *  hierarchical namespace (filesystem, BRL-CAD .g database, etc.) can
 *  be searched.  The default callbacks use the system filesystem.
 *
 *  See also: src/librt/db_glob.c for the geometry-database backend
 *  (db_path_glob) which plugs into this API.
 *
 */
/** @{ */
/** @file bu/glob.h */


/**
 * Representation of a single directory entry returned by gl_readdir.
 * The caller allocates bu_dirent and sets @a name to point at a caller-owned
 * bu_vls; gl_readdir fills that vls with the entry name on each call.
 */
struct bu_dirent {
    struct bu_vls *name;   /**< entry name (caller-supplied vls, filled by gl_readdir) */
    void *data;            /**< backend-specific per-entry data */
};

/**
 * Stat information about a path element, filled by gl_lstat / gl_stat.
 */
struct bu_stat {
    b_off_t size;    /**< object size (meaning is backend-defined) */
    int is_dir;      /**< non-zero if path is a container (directory / combination) */
    void *data;      /**< backend-specific data */
};


/**
 * Main context structure used by bu_glob() to specify behavior,
 * supply custom callbacks, and accumulate results.
 *
 * Initialise with bu_glob_ctx_create() and release with bu_glob_ctx_destroy().
 */
/** Opaque implementation type; defined in glob.c.  API consumers must not
 *  access this structure directly.
 */
struct bu_glob_ctx_impl;
struct bu_glob_context {

#define BU_GLOB_APPEND     0x0001  /**< Append to output from previous call. */
#define BU_GLOB_NOSORT     0x0020  /**< Do not sort results. */
#define BU_GLOB_NOESCAPE   0x2000  /**< Treat backslash as ordinary character. */
    int gl_flags;                  /**< flags customising globbing behaviour */

    /* --- Return values (filled by bu_glob) --- */

    int gl_pathc;                  /**< total number of matched paths */
    int gl_matchc;                 /**< number of paths matched by this call */
    struct bu_vls **gl_pathv;      /**< NULL-terminated array of matched paths */

    /* --- Optional callback functions ---
     *
     * When NULL the default POSIX filesystem implementation is used.
     * The @a data member is passed as the last argument to lstat, stat,
     * errfunc, and as the second argument to opendir, allowing backends
     * to carry private state without globals.
     */

    /**
     * Open the directory at @a path using backend-specific data @a data.
     * Returns an opaque handle passed back to gl_readdir / gl_closedir,
     * or NULL on failure.
     */
    void *(*gl_opendir)(const char *path, void *data);

    /**
     * Read the next entry from a directory handle obtained via gl_opendir.
     * Fills de->name with the entry name.  Returns 0 on success, non-zero
     * when there are no more entries.
     */
    int (*gl_readdir)(struct bu_dirent *de, void *dirhandle);

    /**
     * Close and free a directory handle obtained via gl_opendir.
     */
    void (*gl_closedir)(void *dirhandle);

    /**
     * Stat a path (do not follow symlinks, i.e. lstat semantics).
     * Returns 0 on success, -1 on failure.
     */
    int (*gl_lstat)(const char *path, struct bu_stat *sb, void *data);

    /**
     * Stat a path (follow symlinks, i.e. stat semantics).
     * Returns 0 on success, -1 on failure.
     */
    int (*gl_stat)(const char *path, struct bu_stat *sb, void *data);

#define BU_GLOB_NOMATCH (-1)       /**< Returned by bu_glob when there are no matches. */
#define BU_GLOB_ABORTED (-2)       /**< Returned by bu_glob after an unignored error. */

    /**
     * Called on directory-open errors.  Receives the path, errno value,
     * and the context data pointer.  Return non-zero to abort the glob.
     */
    int (*gl_errfunc)(const char *path, int errnum, void *data);

    /* --- For caller use --- */

    void *data;   /**< Passed verbatim to every callback as described above. */

    /* --- Private --- */

    struct bu_glob_ctx_impl *i;  /**< Implementation details; do not use directly. */
};
typedef struct bu_glob_context bu_glob_t;


/**
 * Create a globbing context
 */
BU_EXPORT struct bu_glob_context *bu_glob_ctx_create(void);


/**
 * release any resources allocated during bu_glob(), including any
 * returned paths
 */
BU_EXPORT extern void bu_glob_ctx_destroy(struct bu_glob_context *);


/**
 * Match a pattern against a set of elements.
 *
 * This interface is a somewhat simplified and abstracted version of
 * UNIX glob matching, based loosely on the interface specified in
 * POSIX.2.  It supports user specified callback functions allowing
 * callers to glob nearly any named storage structure.  By default,
 * globbing will map to the local filesystem.
 *
 * Function takes an input pattern, a set of flags, and a globbing
 * context from bu_glob_ctx_create().
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
