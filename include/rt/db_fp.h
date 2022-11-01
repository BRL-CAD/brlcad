/*                         D B _ F P . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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

#ifndef RT_DB_FP_H
#define RT_DB_FP_H

#include "common.h"

#include "vmath.h"

#include "bu/vls.h"
#include "rt/defines.h"
#include "rt/op.h"

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
/** @file rt/db_fp.h */

/**
 * A db_fp pool holds path information - calling code may optionally initialize
 * a common pool to be used for multiple paths, or forgo use of a pool and rely
 * on internal db_fp management. In cases where many path nodes are the same,
 * across many paths there can be saving in using a pool.  dp_fp paths using a
 * pool will share matching nodes.
 */
struct db_fp_pool_impl;
struct db_fp_pool {
    struct db_fp_pool_impl *i;
};

/* Create a pool for use in db_fp operations */
int db_fp_pool_init(struct db_fp_pool *p);
int db_fp_pool_create(struct db_fp_pool **p);

/* Garbage collect all nodes in a pool which aren't currently referenced by at
 * least one db_fp. A geometry editor maintaining a set of active full paths,
 * for example, will want to do this periodically to clear out stale nodes that
 * were created by paths subsequently removed in user editing.  It will avoid
 * the need to completely rebuild the path set, and instead free up just the
 * memory that is no longer needed. */
void db_fp_pool_gc(struct db_fp_pool *p);

/* Completely eliminate a pool and all its db_fp_nodes.  If this operation is
 * performed and  there are db_fp paths using the pool those paths will point
 * to invalid storage, so callers should use db_fp_pool_gc unless they are sure
 * they are done with all db_fp instances using the pool. */
void db_fp_pool_clear(struct db_fp_pool *p);
void db_fp_pool_destroy(struct db_fp_pool *p);

/* The container which holds a database path */
struct db_fp_impl;
struct db_fp {
    struct db_fp_impl *i;
};

/* All other db_fp routines operate on a db_fp created with db_fp_create.  The
 * p parameter specifying a shared node pool is optional - if NULL, the full path
 * in question will manage its own memory independently. */
struct db_fp * db_fp_create(struct db_fp_pool *p);

/* A db_fp instance should always be removed with db_fp_destroy. */
void db_fp_destroy(struct db_fp *fp);

/* Number of nodes in path */
size_t db_fp_len(struct db_fp *fp);

/* "Pop" the leaf node off the path */
void db_fp_pop(struct db_fp *fp);

/* "Push" the full path in src onto the end of dest.  The leaf of src is the
 * new leaf in dest. Note that the root node of src will be replaced by a new
 * node (or possibly a pre-existing matching node if a pool is in use) in the
 * dest fp, as its parent will no longer be NULL.  Caller will need to manage
 * any user data associated with the root node that they wish to preserve
 * manually, as it will not be copied by this routine. */
void db_fp_push(struct db_fp *dest, const struct db_fp *src, size_t start_offset);

/* Add a directory dp + boolean op (and optionally matrix mat) as the new leaf
 * node of fp.  If a pool is in use, will either append an existing node that
 * matches the specification or (if no such node is found) create a new node.
 * Will return 1 if assignment would create a circular path or -1 if node creation
 * fails.  Returns 0 on success. */
int db_fp_append(struct db_fp *fp, struct directory *dp, db_op_t op, mat_t mat);

/* Set the path node at index i to the node associated with dp and boolean op
 * (and matrix information if specified). If a pool is in use, will either
 * append an existing node that matches the specification or (if no such node
 * is found) create a new node.  Will return 2 if assignment would create a
 * circular path, 1 if the index is out of range for fp, or -1 if node creation
 * fails, and 0 on success.
 *
 * Note that it is the caller's responsibility to manage any user data that
 * might have been associated with the previous node - if a pre-existing node
 * is found to insert that node's user data will not be altered, and if a new
 * node is created its user data queue will be empty. The caller should retrieve
 * any necessary data pointers *before* calling db_fp_set. */
int db_fp_set(struct db_fp *fp, int i, struct directory *dp, db_op_t op, mat_t mat);

/* Copy old_fp onto new_fp, destroying pre-existing path information in
 * curr_fp. */
void db_fp_cpy(struct db_fp *curr_fp, const struct db_fp *old_fp);

/* Retrieve a node's directory pointer.  Index of -1 returns the leaf,
 * 0 returns the root. Returns NULL for an out-of-range index value.*/
struct directory *db_fp_get_dir(const struct db_fp *fp, int index);

/* Retrieve a nodes associated boolean operator. Index of -1 returns the leaf's
 * parent op.  0 returns union. Returns DB_OP_NULL for an out-of-range index
 * value. */
db_op_t db_fp_get_bool(const struct db_fp *fp, int index);

/* Retrieve a node's associated matrix (NULL may be interpreted as the identity
 * matrix.) Returns NULL for an out-of-range index value.*/
matp_t db_fp_get_mat(const struct db_fp *fp, int index);

/* Mechanism to allow calling code to associate and retrieve user
 * data associated with a particular full path.*/
void db_fp_push_data(struct db_fp *fp, void *data);
void *db_fp_get_data(struct db_fp *fp);
void db_fp_pop_data(struct db_fp *fp);

/* Mechanism to allow calling code to associate and retrieve user
 * data associated with the nth node in a path. Push is a no-op
 * and get/pop return NULL for index values not present in the path. */
void db_fp_push_node_data(struct db_fp *fp, int index, void *data);
void *db_fp_get_node_data(struct db_fp *fp, int index);
void db_fp_pop_node_data(struct db_fp *fp, int index);

/* Print out paths as strings */
#define DB_FP_STR_BOOL         0x1    /* boolean labels flag */
#define DB_FP_STR_TYPE         0x2    /* object type flag */
#define DB_FP_STR_MATRIX       0x4    /* matrix flag */
char *db_fp_to_str(const struct db_fp *fp, int flags);
void db_fp_to_vls(struct bu_vls *v, const struct db_fp *fp, int flags);

/* Convert string descriptions of paths into path structures.  Pool may be NULL  */
int db_str_to_fp(struct db_fp **fp, const struct db_i *dbip, const char *str, struct db_fp_pool *p);
int db_argv_to_fp(struct db_fp **fp, const struct db_i *dbip, int argc, const char *const*argv, struct db_fp_pool *p);

/* Compare paths, including boolean operations and matrices if defined */
RT_EXPORT extern int db_fp_identical(const struct db_fp *a, const struct db_fp *b);

/* Any two paths that are identical according to db_fp_identical are also true
 * in this test, and in addition a more permissive check is performed for
 * same-solid-different-path conditions if db_fp_identical does not pass.  The
 * latter test is not a full geometric comparison, but will detect cases where
 * the same solid database object is unioned in from multiple paths without
 * being moved by a matrix. */
RT_EXPORT extern int db_fp_identical_solid(const struct db_fp *a, const struct db_fp *b);

/* Returns either the index which denotes the point in a that starts a
 * series of path nodes which fully match the path nodes of b, or -1 if no
 * match found.  By default, the search will try to match the root node of a
 * with the root node of b and work left(root) to right(leaf), if leaf_to_root
 * is set the leaf of a will be matched against the leaf of b and the match
 * will work right to left. skip_first tells the search how many nodes in a to
 * ignore before starting to compare.*/
int db_fp_match(const struct db_fp *a, const struct db_fp *b, unsigned int skip_first, int leaf_to_root);

/* Returns either the first node index whose dp->d_namep matches n, or -1 if no
 * match is found.  Normally the search will be ordered from left(root) to
 * right(leaf), but if leaf_to_root is set the checks will be done in right to
 * left order.  skip_first tells the search how many nodes of p to ignore before
 * starting to compare.*/
int db_fp_search(const struct db_fp *p, const char *n, unsigned int skip_first, int leaf_to_root);

/* Set mat to the evaluated matrix along the path fp, starting from the root node and continuing up
 * until depth d (0 indicates a full path evaluation). */
void db_fp_eval_mat(mat_t mat, const struct db_fp *fp, unsigned int depth);

/** @} */

__END_DECLS

#endif /*RT_DB_FP_H*/

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
