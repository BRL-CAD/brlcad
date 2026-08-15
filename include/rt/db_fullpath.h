/*                   D B _ F U L L P A T H . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2026 United States Government as represented by
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

#ifndef RT_DB_FULLPATH_H
#define RT_DB_FULLPATH_H

#include "common.h"

#include "vmath.h"

#include "bu/color.h"
#include "bu/vls.h"
#include "rt/defines.h"

__BEGIN_DECLS

struct db_i;      /* forward declaration */
struct directory; /* forward declaration */

/** @addtogroup db_fullpath
 *
 * @brief
 * Structures and routines for collecting and manipulating paths through the database tree.
 *
 */
/** @{ */
/** @file rt/db_fullpath.h */

/**
 * The fp_bool array can optionally hold a boolean flag
 * associated with each corresponding dp in fp_names.  This
 * array must be manually maintained by the client code in
 * order for it to have valid data - many functions using
 * full paths (for example, conversion from strings) don't
 * have knowledge of a specific boolean tree.
 */
struct db_full_path {
    uint32_t		magic;
    size_t		fp_len;
    size_t		fp_maxlen;
    struct directory **	fp_names;	/**< @brief array of dir pointers */
    int	              * fp_bool;	/**< @brief array of boolean flags */
    int	              * fp_cinst;	/**< @brief array of comb tree instance specifiers */
};

#define DB_FULL_PATH_INIT_ZERO {DB_FULL_PATH_MAGIC, 0, 0, NULL, NULL, NULL}

#define RT_CK_FULL_PATH(_p) BU_CKMAG(_p, DB_FULL_PATH_MAGIC, "db_full_path")

#define DB_FULL_PATH_CUR_DIR(_pp) (((_pp)->fp_len > 0) ? (_pp)->fp_names[(_pp)->fp_len-1] : NULL)
#define DB_FULL_PATH_CUR_BOOL(_pp) ((_pp)->fp_bool[(_pp)->fp_len-1])
#define DB_FULL_PATH_SET_CUR_BOOL(_pp, _i) ((_pp)->fp_bool[(_pp)->fp_len-1]) = _i

#define DB_FULL_PATH_CUR_COMB_INST(_pp) ((_pp)->fp_cinst[(_pp)->fp_len-1])
#define DB_FULL_PATH_SET_CUR_COMB_INST(_pp, _i) ((_pp)->fp_cinst[(_pp)->fp_len-1]) = _i

#define DB_FULL_PATH_LEN(_pp) ((_pp)->fp_len)
#define DB_FULL_PATH_POP(_pp) ((_pp)->fp_len > 0) ? (_pp)->fp_len-- : (_pp)->fp_len

#define DB_FULL_PATH_GET(_pp, _i) ((_pp)->fp_names[(_i)])
#define DB_FULL_PATH_GET_BOOL(_pp, _i) ((_pp)->fp_bool[(_i)])
#define DB_FULL_PATH_SET_BOOL(_pp, _i, _j) ((_pp)->fp_bool[(_i)] = _j)
#define DB_FULL_PATH_GET_COMB_INST(_pp, _i) ((_pp)->fp_cinst[(_i)])
#define DB_FULL_PATH_SET_COMB_INST(_pp, _i, _j) ((_pp)->fp_cinst[(_i)] = _j)

#define DB_FULL_PATH_ROOT_DIR(_pp) ((_pp)->fp_names[0])



/* db_fullpath.c */
RT_EXPORT extern void db_full_path_init(struct db_full_path *pathp);

RT_EXPORT extern void db_add_node_to_full_path(struct db_full_path *pp,
					       struct directory *dp);

RT_EXPORT extern void db_dup_full_path(struct db_full_path *newp,
				       const struct db_full_path *oldp);

/**
 * Extend "pathp" so that it can grow from current fp_len by incr more names.
 *
 * This is intended primarily as an internal method.
 */
RT_EXPORT extern void db_extend_full_path(struct db_full_path *pathp,
					  size_t incr);

RT_EXPORT extern void db_append_full_path(struct db_full_path *dest,
					  const struct db_full_path *src);

/**
 * Dup old path from starting index to end.
 */
RT_EXPORT extern void db_dup_path_tail(struct db_full_path *newp,
				       const struct db_full_path *oldp,
				       b_off_t start);


/**
 * Status values returned by db_full_path_decode() and
 * db_full_path_encode().  The legacy db_path_to_*() and
 * db_string_to_path() APIs predate this syntax and do not return these
 * detailed errors.
 */
typedef enum {
    DB_FULL_PATH_OK = 0,
    DB_FULL_PATH_INVALID = -1,  /**< Invalid API argument or path structure. */
    DB_FULL_PATH_SYNTAX = -2,   /**< Malformed escaped path text. */
    DB_FULL_PATH_LOOKUP = -3,   /**< A named directory entry was not found. */
    DB_FULL_PATH_RELATION = -4, /**< Consecutive entries are not parent/child. */
    DB_FULL_PATH_INSTANCE = -5  /**< Invalid or unavailable combination instance. */
} db_full_path_status_t;


/**
 * Encode a db_full_path using the unambiguous database-path syntax.
 *
 * This function replaces the contents of @p out on success and leaves it
 * unchanged on failure.  @p out must be an initialized, valid bu_vls; the
 * function does not initialize or free an uninitialized output object.  It
 * does not impose a fixed path-length limit.
 * Canonical output begins with '/', separates hierarchy components with '/',
 * and escapes literal '\\', '/', and '@' bytes in directory-entry names with
 * a preceding '\\'.  A nonzero fp_cinst value is emitted as an unescaped,
 * zero-based @N suffix on a non-root component.
 *
 * The encoded text preserves path identity (directory entries and combination
 * instance indices), but not fp_bool.  Boolean values are relationship
 * metadata and are re-derived by db_full_path_decode().  The text is a
 * database-path syntax, not shell, Tcl, or argv quoting.
 *
 * Use this routine whenever text will later be interpreted as a database
 * path, including command completion, saved path references, and command
 * results intended for re-input.  The legacy db_path_to_string() and
 * db_path_to_vls() routines intentionally retain their raw slash-separated
 * presentation behavior for compatibility.  They cannot distinguish a name
 * containing '/' from multiple hierarchy components and must not be used for
 * new round-trippable path text.
 */
RT_EXPORT extern int db_full_path_encode(struct bu_vls *out,
				 const struct db_full_path *path);


/**
 * Decode the unambiguous database-path syntax emitted by
 * db_full_path_encode().
 *
 * @p out must be an initialized db_full_path.  On success its previous
 * contents are released and replaced.  On failure it is left unchanged.  The
 * parser accepts an optional leading '/' for command-input convenience, while
 * db_full_path_encode() always emits one.  Empty interior components, a
 * trailing unescaped '/', malformed escapes, and unescaped '@' characters
 * that are not a zero-based decimal instance suffix are rejected.
 *
 * Every decoded component is resolved in @p dbip, and each adjacent pair must
 * be a real combination/member relationship.  The resulting directory
 * pointers therefore remain valid only while @p dbip remains open and
 * unchanged.  Parsing is quiet: callers receive a db_full_path_status_t value
 * and no diagnostic is logged for invalid user input.
 *
 * This routine is the inverse of db_full_path_encode().  It is deliberately
 * separate from legacy db_string_to_path(), whose raw slash syntax is
 * ambiguous for directory-entry names containing '/'.
 */
RT_EXPORT extern int db_full_path_decode(struct db_full_path *out,
				 const struct db_i *dbip,
				 const char *pathstr);


/**
 * Unlike rt_path_str(), this version can be used in parallel.
 * Caller is responsible for freeing the returned buffer.
 */
RT_EXPORT extern char *db_path_to_string(const struct db_full_path *pp);

/**
 * Append a string representation of the path onto the vls.  Must have
 * exactly the same formatting conventions as db_path_to_string().
 */
RT_EXPORT extern void db_path_to_vls(struct bu_vls *str,
				     const struct db_full_path *pp);

/**
 * Append a string representation of the path onto the vls, with
 * options to decorate nodes with additional information.
 */
#define DB_FP_PRINT_BOOL         0x1    /* print boolean operations */
#define DB_FP_PRINT_TYPE         0x2    /* print object types */
#define DB_FP_PRINT_MATRIX       0x4    /* print notice that a matrix is present */
#define DB_FP_PRINT_COMB_INDEX   0x8    /* print non-zero comb tree instance specifiers */
RT_EXPORT extern void db_fullpath_to_vls(struct bu_vls *vls,
					 const struct db_full_path *full_path,
					 const struct db_i *dbip,  /* needed for type determination */
					 int fp_flags);


RT_EXPORT extern void db_pr_full_path(const char *msg,
				      const struct db_full_path *pathp);


/**
 * Parse the legacy raw slash-separated path presentation used by
 * db_path_to_string().  This format is retained for compatibility, but it is
 * ambiguous when directory-entry names contain '/', '\\', or instance-like
 * '@' suffixes.  New code requiring round-trippable path text should use
 * db_full_path_decode() and db_full_path_encode().
 *
 * The db_full_path structure will be initialized.  If it was already
 * in use, user should call db_free_full_path() first.
 *
 * Returns -
 * -1 One or more components of path did not exist in the directory.
 * 0 OK
 */
RT_EXPORT extern int db_string_to_path(struct db_full_path *pp,
				       const struct db_i *dbip,
				       const char *str);


/**
 * Treat elements from argv[0] to argv[argc-1] as a path specification.
 *
 * The path structure will be fully initialized.  If it was already in
 * use, user should call db_free_full_path() first.
 *
 * Returns -
 * -1 One or more components of path did not exist in the directory.
 * 0 OK
 */
RT_EXPORT extern int db_argv_to_path(struct db_full_path	*pp,
				     struct db_i			*dbip,
				     int				argc,
				     const char			*const*argv);


/**
 * Free the contents of the db_full_path structure, but not the
 * structure itself, which might be automatic.
 */
RT_EXPORT extern void db_free_full_path(struct db_full_path *pp);


/**
 * Returns -
 * 1 match
 * 0 different
 */
RT_EXPORT extern int db_identical_full_paths(const struct db_full_path *a,
					     const struct db_full_path *b);


/**
 * Returns -
 * 1 if 'b' is a proper subset of 'a'
 * 0 if not.
 */
RT_EXPORT extern int db_full_path_subset(const struct db_full_path	*a,
					 const struct db_full_path	*b,
					 const int			skip_first);

/**
 * Returns -
 * 1 if 'a' matches the top part of 'b'
 * 0 if not.
 *
 * For example, /a/b matches both the top part of /a/b/c and /a/b.
 */
RT_EXPORT extern int db_full_path_match_top(const struct db_full_path	*a,
					    const struct db_full_path	*b);


/**
 * Returns -
 * 1 'dp' is found on this path
 * 0 not found
 */
RT_EXPORT extern int db_full_path_search(const struct db_full_path *a,
					 const struct directory *dp);


/**
 * Function to test whether a path has a cyclic entry in it.
 *
 * @param fp [i] Full path to test
 * @param lname [i] String to use when checking path (optional).  If NULL, use the name of the current directory pointer in fp.
 * @param full_check [i] Flag to instruct the cyclic test to check using all directory pointers in fp, not just the lname/current dp test.
 * @return 1 if the path is cyclic, 0 if it is not.
 *
 * By default, only the leaf (or test_name if supplied) will be used to test if
 * the path is cyclic.  If full_check is set, all object names in the path will
 * be checked, as well as lname if set.  This more expensive check is not
 * necessary if calling code is checking for cyclic paths as paths are being
 * built, but is necessary to fully "clear" a db_full_path from an arbitrary
 * source.  Calling code must use its knowledge of the origins of the full path
 * (or lack thereof) to determine how much validation work is needed.
 */
RT_EXPORT extern int db_full_path_cyclic(const struct db_full_path *fp, const char *lname, int full_check);

/**
 * Build the transformation matrix obtained when traversing the path
 * to the specified depth.
 *
 * Returns -
 * 1 OK, path matrix written into 'mat'.
 * 0 FAIL
 *
 * Called in librt/db_tree.c, mged/dodraw.c, and mged/animedit.c
 */
RT_EXPORT extern int db_path_to_mat(struct db_i *dbip, struct db_full_path *pathp, mat_t mat, int depth); /* number of arcs */

/**
 * For a given path, return the "net" boolean operation of the
 * path.  If a subtraction is found along the path, overall path
 * is regarded as a subtraction.  Else, if an intersection is found
 * op is reported as an intersection.  Else, union is reported.
 */
RT_EXPORT extern int
db_fp_op(const struct db_full_path *pathp, struct db_i *dbip, int depth); /* number of arcs - 0 == all */

/**
 * Determine the color operative at the current directory pointer (the leaf
 * node) of the path according to available information and rules.
 *
 * If nothing can be determined default color is set.
 */
RT_EXPORT extern void db_full_path_color(struct bu_color *c, struct db_full_path *pathp, struct db_i *dbip);


/** @} */

__END_DECLS

#endif /*RT_DB_FULLPATH_H*/

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
