/*                          T R E E . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2015 United States Government as represented by
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
    const struct rt_tess_tol *  ts_ttol;        /**< @brief  Tessellation tolerance */
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
 * RT_GET_TREE returns a new initialized tree union pointer.  The
 * magic number is set to RT_TREE_MAGIC and all other members are
 * zero-initialized.
 *
 * This is a malloc-efficient BU_ALLOC(tp, union tree) replacement.
 * Previously used tree nodes are stored in the provided resource
 * pointer (during RT_FREE_TREE) as a single-linked list using the
 * tb_left field.  Requests for new nodes are pulled first from that
 * list or allocated fresh if needed.
 *
 * DEPRECATED, use BU_GET()
 */
#define RT_GET_TREE(_tp, _res) { \
        if (((_tp) = (_res)->re_tree_hd) != TREE_NULL) { \
            (_res)->re_tree_hd = (_tp)->tr_b.tb_left;    \
            (_tp)->tr_b.tb_left = TREE_NULL;             \
            (_res)->re_tree_get++;                       \
        } else {                                         \
            BU_ALLOC(_tp, union tree);                   \
            (_res)->re_tree_malloc++;                    \
        }                                                \
        RT_TREE_INIT((_tp));                             \
    }

/**
 * RT_FREE_TREE deinitializes a tree union pointer.
 *
 * This is a malloc-efficient replacement for bu_free(tp).  Instead of
 * actually freeing the nodes, they are added to a single-linked list
 * in rt_tree_hd down the tb_left field.  Requests for new nodes (via
 * RT_GET_TREE()) pull from this list instead of allocating new nodes.
 *
 * DEPRECATED, use BU_PUT()
 */
#define RT_FREE_TREE(_tp, _res) { \
        (_tp)->magic = 0;                         \
        (_tp)->tr_b.tb_left = (_res)->re_tree_hd; \
        (_tp)->tr_b.tb_right = TREE_NULL;         \
        (_res)->re_tree_hd = (_tp);               \
        (_tp)->tr_b.tb_op = OP_FREE;              \
        (_res)->re_tree_free++;                   \
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

/* Print an expr tree */
RT_EXPORT extern void rt_pr_tree(const union tree *tp,
	                         int lvl);

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
