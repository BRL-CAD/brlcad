/*                          T R E E . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2022 United States Government as represented by
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
/** @file rt/tree.h
 *
 */

#ifndef RT_TREE_H
#define RT_TREE_H

#include "common.h"
#include "vmath.h"
#include "bu/avs.h"
#include "bu/magic.h"
#include "bu/malloc.h"
#include "bn/tol.h"
#include "rt/geom.h"
#include "rt/defines.h"
#include "rt/db_instance.h"
#include "rt/directory.h"
#include "rt/mater.h"
#include "rt/op.h"
#include "rt/region.h"
#include "rt/soltab.h"
#include "rt/tol.h"
#include "nmg.h"

__BEGIN_DECLS

union tree;       /* forward declaration */
struct resource;  /* forward declaration */
struct rt_i;      /* forward declaration */
struct rt_comb_internal;      /* forward declaration */
struct rt_db_internal;      /* forward declaration */

/**
 * State for database tree walker db_walk_tree() and related
 * user-provided handler routines.
 */
struct db_tree_state {
    uint32_t            magic;
    struct db_i *       ts_dbip;
    int                 ts_sofar;               /**< @brief Flag bits */

    int                 ts_regionid;    /**< @brief GIFT compat region ID code*/
    int                 ts_aircode;     /**< @brief GIFT compat air code */
    int                 ts_gmater;      /**< @brief GIFT compat material code */
    int                 ts_los;         /**< @brief equivalent LOS estimate */
    struct mater_info   ts_mater;       /**< @brief material properties */

    /* FIXME: ts_mat should be a matrix pointer, not a matrix */
    mat_t               ts_mat;         /**< @brief transform matrix */
    int                 ts_is_fastgen;  /**< @brief REGION_NON_FASTGEN/_PLATE/_VOLUME */
    struct bu_attribute_value_set       ts_attrs;       /**< @brief attribute/value structure */

    int                 ts_stop_at_regions;     /**< @brief else stop at solids */
    int                 (*ts_region_start_func)(struct db_tree_state *tsp,
						const struct db_full_path *pathp,
						const struct rt_comb_internal *comb,
						void *client_data
					       ); /**< @brief callback during DAG downward traversal called on region nodes */
    union tree *        (*ts_region_end_func)(struct db_tree_state *tsp,
					      const struct db_full_path *pathp,
					      union tree *curtree,
					      void *client_data
					     ); /**< @brief callback during DAG upward traversal called on region nodes */
    union tree *        (*ts_leaf_func)(struct db_tree_state *tsp,
					const struct db_full_path *pathp,
					struct rt_db_internal *ip,
					void *client_data
				       ); /**< @brief callback during DAG traversal called on leaf primitive nodes */
    const struct bg_tess_tol *  ts_ttol;        /**< @brief  Tessellation tolerance */
    const struct bn_tol *       ts_tol;         /**< @brief  Math tolerance */
    struct model **             ts_m;           /**< @brief  ptr to ptr to NMG "model" */
    struct rt_i *               ts_rtip;        /**< @brief  Helper for rt_gettrees() */
    struct resource *           ts_resp;        /**< @brief  Per-CPU data */
};
#define RT_DBTS_INIT_ZERO { RT_DBTS_MAGIC, NULL, 0, 0, 0, 0, 0, RT_MATER_INFO_INIT_ZERO, MAT_INIT_ZERO, 0, BU_AVS_INIT_ZERO, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }
/* from mged_initial_tree_state */
#define RT_DBTS_INIT_IDN { RT_DBTS_MAGIC, NULL, 0, 0, 0, 0, 100, RT_MATER_INFO_INIT_IDN, MAT_INIT_IDN, 0, BU_AVS_INIT_ZERO, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }

#define TS_SOFAR_MINUS  1       /**< @brief  Subtraction encountered above */
#define TS_SOFAR_INTER  2       /**< @brief  Intersection encountered above */
#define TS_SOFAR_REGION 4       /**< @brief  Region encountered above */

#define RT_CK_DBTS(_p) BU_CKMAG(_p, RT_DBTS_MAGIC, "db_tree_state")

/**
 * initial tree start for db tree walkers.
 *
 * Also used by converters in conv/ directory.  Don't forget to
 * initialize ts_dbip before use.
 */
RT_EXPORT extern const struct db_tree_state rt_initial_tree_state;

/**
 * State for database traversal functions.
 */
struct db_traverse
{
    uint32_t magic;
    struct db_i *dbip;
    void (*comb_enter_func) (
	struct db_i *,
	struct directory *,
	void *);
    void (*comb_exit_func) (
	struct db_i *,
	struct directory *,
	void *);
    void (*leaf_func) (
	struct db_i *,
	struct directory *,
	void *);
    struct resource *resp;
    void *client_data;
};
#define RT_DB_TRAVERSE_INIT(_p) {(_p)->magic = RT_DB_TRAVERSE_MAGIC;   \
	(_p)->dbip = ((void *)0); (_p)->comb_enter_func = ((void *)0); \
	(_p)->comb_exit_func = ((void *)0); (_p)->leaf_func = ((void *)0); \
	(_p)->resp = ((void *)0); (_p)->client_data = ((void *)0);}
#define RT_CK_DB_TRAVERSE(_p) BU_CKMAG(_p, RT_DB_TRAVERSE_MAGIC, "db_traverse")

struct combined_tree_state {
    uint32_t magic;
    struct db_tree_state        cts_s;
    struct db_full_path         cts_p;
};
#define RT_CK_CTS(_p) BU_CKMAG(_p, RT_CTS_MAGIC, "combined_tree_state")

union tree {
    uint32_t magic;             /**< @brief  First word: magic number */
    /* Second word is always OP code */
    struct tree_node {
	uint32_t magic;
	int tb_op;                      /**< @brief  non-leaf */
	struct region *tb_regionp;      /**< @brief  ptr to containing region */
	union tree *tb_left;
	union tree *tb_right;
    } tr_b;
    struct tree_leaf {
	uint32_t magic;
	int tu_op;                      /**< @brief  leaf, OP_SOLID */
	struct region *tu_regionp;      /**< @brief  ptr to containing region */
	struct soltab *tu_stp;
    } tr_a;
    struct tree_cts {
	uint32_t magic;
	int tc_op;                      /**< @brief  leaf, OP_REGION */
	struct region *tc_pad;          /**< @brief  unused */
	struct combined_tree_state *tc_ctsp;
    } tr_c;
    struct tree_nmgregion {
	uint32_t magic;
	int td_op;                      /**< @brief  leaf, OP_NMG_TESS */
	const char *td_name;            /**< @brief  If non-null, dynamic string describing heritage of this region */
	struct nmgregion *td_r;         /**< @brief  ptr to NMG region */
    } tr_d;
    struct tree_db_leaf {
	uint32_t magic;
	int tl_op;                      /**< @brief  leaf, OP_DB_LEAF */
	matp_t tl_mat;                  /**< @brief  xform matp, NULL ==> identity */
	char *tl_name;                  /**< @brief  Name of this leaf (bu_strdup'ed) */
    } tr_l;
};
/* Things which are in the same place in both A & B structures */
#define tr_op           tr_a.tu_op
#define tr_regionp      tr_a.tu_regionp

#define TREE_NULL       ((union tree *)0)
#define RT_CK_TREE(_p) BU_CKMAG(_p, RT_TREE_MAGIC, "union tree")

/**
 * initialize a union tree to zero without a node operation set.  Use
 * the largest union so all values are effectively zero except for the
 * magic number.
 */
#define RT_TREE_INIT(_p) {                 \
	(_p)->magic = RT_TREE_MAGIC;       \
	(_p)->tr_b.tb_op = 0;              \
	(_p)->tr_b.tb_regionp = NULL;      \
	(_p)->tr_b.tb_left = NULL;         \
	(_p)->tr_b.tb_right = NULL;        \
    }

/**
 * RT_GET_TREE returns a union tree pointer with the magic number is
 * set to RT_TREE_MAGIC.
 *
 * DEPRECATED, use BU_GET()+RT_TREE_INIT()
 */
#define RT_GET_TREE(_tp, _res) { \
	BU_GET((_tp), union tree); \
	RT_TREE_INIT((_tp));                             \
    }


/**
 * RT_FREE_TREE deinitializes a tree union pointer.
 *
 * DEPRECATED, use BU_PUT()
 */
#define RT_FREE_TREE(_tp, _res) { \
	BU_PUT((_tp), union tree); \
    }


/**
 * flattened version of the union tree
 */
struct rt_tree_array
{
    union tree *tl_tree;
    int         tl_op;
};

#define TREE_LIST_NULL  ((struct tree_list *)0)

#ifdef USE_OPENCL
/**
 * Flattened version of the infix union tree.
 */
#define UOP_UNION        1         /**< @brief  Binary: L union R */
#define UOP_INTERSECT    2         /**< @brief  Binary: L intersect R */
#define UOP_SUBTRACT     3         /**< @brief  Binary: L subtract R */
#define UOP_XOR          4         /**< @brief  Binary: L xor R, not both*/
#define UOP_NOT          5         /**< @brief  Unary:  not L */
#define UOP_GUARD        6         /**< @brief  Unary:  not L, or else! */
#define UOP_XNOP         7         /**< @brief  Unary:  L, mark region */

#define UOP_SOLID        0         /**< @brief  Leaf:  tr_stp -> solid */

/**
 * bit expr tree representation
 *
 * node:
 *      uint uop : 3
 *      uint right_child : 29
 *
 * leaf:
 *      uint uop : 3
 *      uint st_bit : 29
 */
struct bit_tree {
    unsigned val;
};

struct cl_tree_bit {
    cl_uint val;
};

/* Print a bit expr tree */
RT_EXPORT extern void rt_pr_bit_tree(const struct bit_tree *btp,
				     int idx,
				     int lvl);

RT_EXPORT extern void rt_bit_tree(struct bit_tree *btp,
				  const union tree *tp,
				  size_t *len);
#endif

/* Print an expr tree */
RT_EXPORT extern void rt_pr_tree(const union tree *tp,
				 int lvl);
RT_EXPORT extern void rt_pr_tree_vls(struct bu_vls *vls,
				     const union tree *tp);
RT_EXPORT extern char *rt_pr_tree_str(const union tree *tree);

struct partition; /* forward declaration */
RT_EXPORT extern void rt_pr_tree_val(const union tree *tp,
				     const struct partition *partp,
				     int pr_name,
				     int lvl);

/**
 * Duplicate the contents of a db_tree_state structure, including a
 * private copy of the ts_mater field(s) and the attribute/value set.
 */
RT_EXPORT extern void db_dup_db_tree_state(struct db_tree_state *otsp,
					   const struct db_tree_state *itsp);

/**
 * Release dynamic fields inside the structure, but not the structure
 * itself.
 */
RT_EXPORT extern void db_free_db_tree_state(struct db_tree_state *tsp);

/**
 * In most cases, you don't want to use this routine, you want to
 * struct copy mged_initial_tree_state or rt_initial_tree_state, and
 * then set ts_dbip in your copy.
 */
RT_EXPORT extern void db_init_db_tree_state(struct db_tree_state *tsp,
					    struct db_i *dbip,
					    struct resource *resp);
RT_EXPORT extern struct combined_tree_state *db_new_combined_tree_state(const struct db_tree_state *tsp,
									const struct db_full_path *pathp);
RT_EXPORT extern struct combined_tree_state *db_dup_combined_tree_state(const struct combined_tree_state *old);
RT_EXPORT extern void db_free_combined_tree_state(struct combined_tree_state *ctsp);
RT_EXPORT extern void db_pr_tree_state(const struct db_tree_state *tsp);
RT_EXPORT extern void db_pr_combined_tree_state(const struct combined_tree_state *ctsp);

/**
 * Handle inheritance of material property found in combination
 * record.  Color and the material property have separate inheritance
 * interlocks.
 *
 * Returns -
 * -1 failure
 * 0 success
 * 1 success, this is the top of a new region.
 */
RT_EXPORT extern int db_apply_state_from_comb(struct db_tree_state *tsp,
					      const struct db_full_path *pathp,
					      const struct rt_comb_internal *comb);

/**
 * The search stops on the first match.
 *
 * Returns -
 * tp if found
 * TREE_NULL if not found in this tree
 */
RT_EXPORT extern union tree *db_find_named_leaf(union tree *tp, const char *cp);

/**
 * The search stops on the first match.
 *
 * Returns -
 * TREE_NULL if not found in this tree
 * tp if found
 * *side == 1 if leaf is on lhs.
 * *side == 2 if leaf is on rhs.
 *
 */
RT_EXPORT extern union tree *db_find_named_leafs_parent(int *side,
							union tree *tp,
							const char *cp);
RT_EXPORT extern void db_tree_del_lhs(union tree *tp,
				      struct resource *resp);
RT_EXPORT extern void db_tree_del_rhs(union tree *tp,
				      struct resource *resp);

/**
 * Given a name presumably referenced in a OP_DB_LEAF node, delete
 * that node, and the operation node that references it.  Not that
 * this may not produce an equivalent tree, for example when rewriting
 * (A - subtree) as (subtree), but that will be up to the caller/user
 * to adjust.  This routine gets rid of exactly two nodes in the tree:
 * leaf, and op.  Use some other routine if you wish to kill the
 * entire rhs below "-" and "intersect" nodes.
 *
 * The two nodes deleted will have their memory freed.
 *
 * If the tree is a single OP_DB_LEAF node, the leaf is freed and *tp
 * is set to NULL.
 *
 * Returns -
 * -3 Internal error
 * -2 Tree is empty
 * -1 Unable to find OP_DB_LEAF node specified by 'cp'.
 * 0 OK
 */
RT_EXPORT extern int db_tree_del_dbleaf(union tree **tp,
					const char *cp,
					struct resource *resp,
					int nflag);

/**
 * Multiply on the left every matrix found in a DB_LEAF node in a
 * tree.
 */
RT_EXPORT extern void db_tree_mul_dbleaf(union tree *tp,
					 const mat_t mat);

/**
 * This routine traverses a combination (union tree) in LNR order and
 * calls the provided function for each OP_DB_LEAF node.  Note that
 * this routine does not go outside this one combination!!!!
 *
 * was previously named comb_functree()
 */
RT_EXPORT extern void db_tree_funcleaf(struct db_i              *dbip,
				       struct rt_comb_internal  *comb,
				       union tree               *comb_tree,
				       void (*leaf_func)(struct db_i *, struct rt_comb_internal *, union tree *,
							 void *, void *, void *, void *),
				       void *           user_ptr1,
				       void *           user_ptr2,
				       void *           user_ptr3,
				       void *           user_ptr4);

/**
 * Starting with possible prior partial path and corresponding
 * accumulated state, follow the path given by "new_path", updating
 * *tsp and *total_path with full state information along the way.  In
 * a better world, there would have been a "combined_tree_state" arg.
 *
 * Parameter 'depth' controls how much of 'new_path' is used:
 *
 * 0 use all of new_path
 * >0 use only this many of the first elements of the path
 * <0 use all but this many path elements.
 *
 * A much more complete version of rt_plookup() and pathHmat().  There
 * is also a TCL interface.
 *
 * Returns -
 * 0 success (plus *tsp is updated)
 * -1 error (*tsp values are not useful)
 */
RT_EXPORT extern int db_follow_path(struct db_tree_state *tsp,
				    struct db_full_path *total_path,
				    const struct db_full_path *new_path,
				    int noisy,
				    long pdepth);

/**
 * Follow the slash-separated path given by "cp", and update *tsp and
 * *total_path with full state information along the way.
 *
 * A much more complete version of rt_plookup().
 *
 * TODO - need to extend this to support specifiers orig_str to
 * call out particular instances of combs in a tree...
 *
 * Returns -
 * 0 success (plus *tsp is updated)
 * -1 error (*tsp values are not useful)
 */
RT_EXPORT extern int db_follow_path_for_state(struct db_tree_state *tsp,
					      struct db_full_path *pathp,
					      const char *orig_str, int noisy);

RT_EXPORT extern union tree *db_dup_subtree(const union tree *tp,
					    struct resource *resp);
RT_EXPORT extern void db_ck_tree(const union tree *tp);


/**
 * Release all storage associated with node 'tp', including children
 * nodes.
 */
RT_EXPORT extern void db_free_tree(union tree *tp,
				   struct resource *resp);


/**
 * Re-balance this node to make it left heavy.  Union operators will
 * be moved to left side.  when finished "tp" MUST still point to top
 * node of this subtree.
 */
RT_EXPORT extern void db_left_hvy_node(union tree *tp);


/**
 * If there are non-union operations in the tree, above the region
 * nodes, then rewrite the tree so that the entire tree top is nothing
 * but union operations, and any non-union operations are clustered
 * down near the region nodes.
 */
RT_EXPORT extern void db_non_union_push(union tree *tp,
					struct resource *resp);

/**
 * Return a count of the number of "union tree" nodes below "tp",
 * including tp.
 */
RT_EXPORT extern int db_count_tree_nodes(const union tree *tp,
					 int count);


/**
 * Returns -
 * 1 if this tree contains nothing but union operations.
 * 0 if at least one subtraction or intersection op exists.
 */
RT_EXPORT extern int db_is_tree_all_unions(const union tree *tp);
RT_EXPORT extern int db_count_subtree_regions(const union tree *tp);
RT_EXPORT extern int db_tally_subtree_regions(union tree        *tp,
					      union tree        **reg_trees,
					      int               cur,
					      int               lim,
					      struct resource *resp);

/**
 * This is the top interface to the "tree walker."
 *
 * Parameters:
 *      rtip            rt_i structure to database (open with rt_dirbuild())
 *      argc            # of tree-tops named
 *      argv            names of tree-tops to process
 *      init_state      Input parameter: initial state of the tree.
 *                      For example:  rt_initial_tree_state,
 *                      and mged_initial_tree_state.
 *
 * These parameters are pointers to callback routines.  If NULL, they
 * won't be called.
 *
 *      reg_start_func  Called at beginning of each region, before
 *                      visiting any nodes within the region.  Return
 *                      0 if region should be skipped without
 *                      recursing, otherwise non-zero.  DO NOT USE FOR
 *                      OTHER PURPOSES!  For example, can be used to
 *                      quickly skip air regions.
 *
 *      reg_end_func    Called after all nodes within a region have been
 *                      recursively processed by leaf_func.  If it
 *                      wants to retain 'curtree' then it may steal
 *                      that pointer and return TREE_NULL.  If it
 *                      wants us to clean up some or all of that tree,
 *                      then it returns a non-null (union tree *)
 *                      pointer, and that tree is safely freed in a
 *                      non-parallel section before we return.
 *
 *      leaf_func       Function to process a leaf node.  It is actually
 *                      invoked from db_recurse() from
 *                      _db_walk_subtree().  Returns (union tree *)
 *                      representing the leaf, or TREE_NULL if leaf
 *                      does not exist or has an error.
 *
 *
 * This routine will employ multiple CPUs if asked, but is not
 * multiply-parallel-recursive.  Call this routine with ncpu > 1 from
 * serial code only.  When called from within an existing thread, ncpu
 * must be 1.
 *
 * If ncpu > 1, the caller is responsible for making sure that
 * RTG.rtg_parallel is non-zero.
 *
 * Plucks per-cpu resources out of rtip->rti_resources[].  They need
 * to have been initialized first.
 *
 * Returns -
 * -1 Failure to prepare even a single sub-tree
 * 0 OK
 */
RT_EXPORT extern int db_walk_tree(struct db_i *dbip,
				  int argc,
				  const char **argv,
				  int ncpu,
				  const struct db_tree_state *init_state,
				  int (*reg_start_func) (struct db_tree_state * /*tsp*/,
							 const struct db_full_path * /*pathp*/,
							 const struct rt_comb_internal * /* combp */,
							 void *client_data),
				  union tree *(*reg_end_func) (struct db_tree_state * /*tsp*/,
							       const struct db_full_path * /*pathp*/,
							       union tree * /*curtree*/,
							       void *client_data),
				  union tree *(*leaf_func) (struct db_tree_state * /*tsp*/,
							    const struct db_full_path * /*pathp*/,
							    struct rt_db_internal * /*ip*/,
							    void *client_data),
				  void *client_data);

/**
 * Fills a bu_vls with a representation of the given tree appropriate
 * for processing by Tcl scripts.
 *
 * A tree 't' is represented in the following manner:
 *
 * t := { l dbobjname { mat } }
 *         | { l dbobjname }
 *         | { u t1 t2 }
 *         | { n t1 t2 }
 *         | { - t1 t2 }
 *         | { ^ t1 t2 }
 *         | { ! t1 }
 *         | { G t1 }
 *         | { X t1 }
 *         | { N }
 *         | {}
 *
 * where 'dbobjname' is a string containing the name of a database object,
 *       'mat'       is the matrix preceding a leaf,
 *       't1', 't2'  are trees (recursively defined).
 *
 * Notice that in most cases, this tree will be grossly unbalanced.
 */
RT_EXPORT extern int db_tree_list(struct bu_vls *vls, const union tree *tp);

/**
 * Take a TCL-style string description of a binary tree, as produced
 * by db_tree_list(), and reconstruct the in-memory form of that tree.
 */
RT_EXPORT extern union tree *db_tree_parse(struct bu_vls *vls, const char *str, struct resource *resp);

/**
 * This subroutine is called for a no-frills tree-walk, with the
 * provided subroutines being called at every combination and leaf
 * (solid) node, respectively.
 *
 * This routine is recursive, so no variables may be declared static.
 */
RT_EXPORT extern void db_functree(struct db_i *dbip,
				  struct directory *dp,
				  void (*comb_func)(struct db_i *,
						    struct directory *,
						    void *),
				  void (*leaf_func)(struct db_i *,
						    struct directory *,
						    void *),
				  struct resource *resp,
				  void *client_data);
/**
 * Ray Tracing library database tree walker.
 *
 * Collect and prepare regions and solids for subsequent ray-tracing.
 *
 */


/**
 * Calculate the bounding RPP of the region whose boolean tree is
 * 'tp'.  The bounding RPP is returned in tree_min and tree_max, which
 * need not have been initialized first.
 *
 * Returns -
 * 0 success
 * -1 failure (tree_min and tree_max may have been altered)
 */
RT_EXPORT extern int rt_bound_tree(const union tree     *tp,
				   vect_t               tree_min,
				   vect_t               tree_max);

/**
 * Eliminate any references to NOP nodes from the tree.  It is safe to
 * use db_free_tree() here, because there will not be any dead solids.
 * They will all have been converted to OP_NOP nodes by
 * _rt_tree_kill_dead_solid_refs(), previously, so there is no need to
 * worry about multiple db_free_tree()'s repeatedly trying to free one
 * solid that has been instanced multiple times.
 *
 * Returns -
 * 0 this node is OK.
 * -1 request caller to kill this node
 */
RT_EXPORT extern int rt_tree_elim_nops(union tree *,
				       struct resource *resp);

/**
 * Return count of number of leaf nodes in this tree.
 */
RT_EXPORT extern size_t db_tree_nleaves(const union tree *tp);

/**
 * Take a binary tree in "V4-ready" layout (non-unions pushed below
 * unions, left-heavy), and flatten it into an array layout, ready for
 * conversion back to the GIFT-inspired V4 database format.
 *
 * This is done using the db_non_union_push() routine.
 *
 * If argument 'free' is non-zero, then the non-leaf nodes are freed
 * along the way, to prevent memory leaks.  In this case, the caller's
 * copy of 'tp' will be invalid upon return.
 *
 * When invoked at the very top of the tree, the op argument must be
 * OP_UNION.
 */
RT_EXPORT extern struct rt_tree_array *db_flatten_tree(struct rt_tree_array *rt_tree_array, union tree *tp, int op, int avail, struct resource *resp);


/**
 * Produce a GIFT-compatible listing, one "member" per line,
 * regardless of the structure of the tree we've been given.
 */
RT_EXPORT extern void db_tree_flatten_describe(struct bu_vls    *vls,
					       const union tree *tp,
					       int              indented,
					       int              lvl,
					       double           mm2local,
					       struct resource  *resp);

RT_EXPORT extern void db_tree_describe(struct bu_vls    *vls,
				       const union tree *tp,
				       int              indented,
				       int              lvl,
				       double           mm2local);

/**
 * Support routine for db_ck_v4gift_tree().
 * Ensure that the tree below 'tp' is left-heavy, i.e. that there are
 * nothing but solids on the right side of any binary operations.
 *
 * Returns -
 * -1 ERROR
 * 0 OK
 */
RT_EXPORT extern int db_ck_left_heavy_tree(const union tree     *tp,
					   int          no_unions);
/**
 * Look a gift-tree in the mouth.
 *
 * Ensure that this boolean tree conforms to the GIFT convention that
 * union operations must bind the loosest.
 *
 * There are two stages to this check:
 * 1) Ensure that if unions are present they are all at the root of tree,
 * 2) Ensure non-union children of union nodes are all left-heavy
 * (nothing but solid nodes permitted on rhs of binary operators).
 *
 * Returns -
 * -1 ERROR
 * 0 OK
 */
RT_EXPORT extern int db_ck_v4gift_tree(const union tree *tp);

/**
 * Given a rt_tree_array array, build a tree of "union tree" nodes
 * appropriately connected together.  Every element of the
 * rt_tree_array array used is replaced with a TREE_NULL.  Elements
 * which are already TREE_NULL are ignored.  Returns a pointer to the
 * top of the tree.
 */
RT_EXPORT extern union tree *db_mkbool_tree(struct rt_tree_array *rt_tree_array,
					    size_t              howfar,
					    struct resource     *resp);

RT_EXPORT extern union tree *db_mkgift_tree(struct rt_tree_array *trees,
					    size_t subtreecount,
					    struct resource *resp);


RT_EXPORT extern void rt_optim_tree(union tree *tp,
				    struct resource *resp);





/*************************************************************************
 * Deprecated
 *************************************************************************/


/**
 * Updates state via *tsp, pushes member's directory entry on *pathp.
 * (Caller is responsible for popping it).
 *
 * Returns -
 * -1 failure
 * 0 success, member pushed on path
 *
 * DEPRECATED, internal implementation function
 */
RT_EXPORT extern int db_apply_state_from_memb(struct db_tree_state *tsp,
					      struct db_full_path *pathp,
					      const union tree *tp);

/**
 * Returns -
 * -1 found member, failed to apply state
 * 0 unable to find member 'cp'
 * 1 state applied OK
 *
 * DEPRECATED, internal implementation function
 */
RT_EXPORT extern int db_apply_state_from_one_member(struct db_tree_state *tsp,
						    struct db_full_path *pathp,
						    const char *cp,
						    int sofar,
						    const union tree *tp);

/**
 * Recurse down the tree, finding all the leaves (or finding just all
 * the regions).
 *
 * ts_region_start_func() is called to permit regions to be skipped.
 * It is not intended to be used for collecting state.
 *
 * DEPRECATED, internal implementation function
 */
RT_EXPORT extern union tree *db_recurse(struct db_tree_state *tsp,
					struct db_full_path *pathp,
					struct combined_tree_state **region_start_statepp,
					void *client_data);



__END_DECLS

#endif /* RT_TREE_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
