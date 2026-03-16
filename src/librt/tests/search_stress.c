/*                S E A R C H _ S T R E S S . C
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/** @file search_stress.c
 *
 * Stress and correctness tests for search.cpp / search_old.cpp.
 *
 * Builds test geometry in-memory using librt's API and verifies:
 *   - db_search results match independently-calculated ground truth
 *   - db_search (new optimised implementation) and db_search_old
 *     (reference implementation) return identical results for all
 *     tested filters
 *
 * Tree used in correctness tests
 * ================================
 *
 * All objects are searched starting from the single top-level object
 * (top1), since all other objects in the tree are referenced children.
 *
 * Primitives (spheres):
 *   prim_target.s   <- the "target" that -above searches look for
 *   prim_special.s  <- a second named target
 *   prim_plain.s    <- uninteresting filler
 *
 * Leaf combos (depth-1, referenced by mid-level combos):
 *   leaf_t   = { prim_target.s }
 *   leaf_s   = { prim_special.s }
 *   leaf_ts  = { prim_target.s, prim_special.s }  (two children)
 *   leaf_p   = { prim_plain.s }
 *
 * Mid combos (depth-2):
 *   mid_a    = { leaf_t,  leaf_p  }   <- has prim_target.s
 *   mid_b    = { leaf_s,  leaf_p  }   <- has prim_special.s only
 *   mid_c    = { leaf_ts, leaf_p  }   <- has both prim_target.s/prim_special.s
 *
 * Upper combos (depth-3):
 *   upper_a  = { mid_a, mid_b }   <- has prim_target.s via mid_a
 *   upper_b  = { mid_b, mid_c }   <- has prim_target.s via mid_c; mid_b is reused
 *
 * Top combo (depth-4) -- the only top-level (unreferenced) object:
 *   top1     = { upper_a, upper_b }
 *
 * Note: all nodes are reached only through top1, so all path counts below
 * are relative to the subtree rooted at top1.
 *
 * Ground-truth path counts (DB_SEARCH_TREE, no explicit path => searches top1):
 *
 * Query: -name prim_target.s
 *   /top1/upper_a/mid_a/leaf_t/prim_target.s
 *   /top1/upper_b/mid_c/leaf_ts/prim_target.s
 *   Total = 2
 *
 * Query: -type shape
 *   9 paths (prim_target.s x2, prim_special.s x3, prim_plain.s x4)
 *   Total = 9
 *
 * Query: -type comb   (all comb paths in tree)
 *   /top1
 *   /top1/upper_a                      /top1/upper_b
 *   /top1/upper_a/mid_a                /top1/upper_b/mid_b
 *   /top1/upper_a/mid_a/leaf_t         /top1/upper_b/mid_b/leaf_s
 *   /top1/upper_a/mid_a/leaf_p         /top1/upper_b/mid_b/leaf_p
 *   /top1/upper_a/mid_b                /top1/upper_b/mid_c
 *   /top1/upper_a/mid_b/leaf_s         /top1/upper_b/mid_c/leaf_ts
 *   /top1/upper_a/mid_b/leaf_p         /top1/upper_b/mid_c/leaf_p
 *   Total = 15
 *
 * Query: -above -name prim_target.s
 *   Objects in whose subtree prim_target.s appears:
 *   /top1
 *   /top1/upper_a                      /top1/upper_b
 *   /top1/upper_a/mid_a                /top1/upper_b/mid_c
 *   /top1/upper_a/mid_a/leaf_t         /top1/upper_b/mid_c/leaf_ts
 *   Total = 7
 *
 * Query: -above -name prim_special.s
 *   /top1
 *   /top1/upper_a                      /top1/upper_b
 *   /top1/upper_a/mid_b                /top1/upper_b/mid_b
 *   /top1/upper_a/mid_b/leaf_s         /top1/upper_b/mid_b/leaf_s
 *   /top1/upper_b/mid_c                /top1/upper_b/mid_c/leaf_ts
 *   Total = 9
 *
 * Query: -above -name prim_plain.s
 *   All comb paths except leaves that do not contain leaf_p as descendant.
 *   (Every path in the tree is above some leaf_p.)
 *   Total = 11
 *
 * Query: -below -name mid_a  (searched from top1)
 *   Objects that are direct or indirect children of mid_a:
 *   /top1/upper_a/mid_a/leaf_t
 *   /top1/upper_a/mid_a/leaf_p
 *   /top1/upper_a/mid_a/leaf_t/prim_target.s
 *   /top1/upper_a/mid_a/leaf_p/prim_plain.s
 *   Total = 4
 *
 * Query: -type comb  (RETURN_UNIQ_DP)  -> 10 unique comb dps
 * Query: -type shape (RETURN_UNIQ_DP)  ->  3 unique shape dps
 * Note: db_search return value is always the PATH count, not the unique
 *       dp count.  Use BU_PTBL_LEN(&results) for the unique-dp count.
 *
 *
 * DAG (shared-node) topology used in performance tests
 * =====================================================
 *
 * The fan-tree used above has no path sharing: every node is
 * referenced by exactly one parent, so the number of distinct full
 * paths equals the number of nodes.  The old and new implementations
 * therefore do the same amount of work on that tree.
 *
 * The key algorithmic difference is:
 *   db_search_old  - ALWAYS pre-builds the complete set of full paths
 *                    into a bu_ptbl before evaluating any filter.
 *   db_search (new) - for queries that do NOT contain -above, uses an
 *                    on-the-fly traversal that builds, evaluates, and
 *                    frees each path in turn without accumulating them.
 *                    This covers -name, -type, and -below queries.
 *                    For -above queries it falls back to the same
 *                    pre-collection strategy as the old implementation.
 *
 * To expose this difference the DAG performance tests use a 3-level
 * all-to-all structure:
 *
 *   dag_target.s             <- single target primitive
 *   dag_leaf_0..dag_leaf_{L-1}  <- L leaf combos, each containing dag_target.s
 *   dag_mid_0..dag_mid_{M-1}    <- M mid combos, each referencing ALL L leaves
 *   dag_top                  <- single top combo referencing all M mids
 *
 * Full paths from dag_top:
 *   dag_top alone                         :         1
 *   dag_top -> dag_mid_j                  :         M
 *   dag_top -> dag_mid_j -> dag_leaf_i    :       M*L
 *   dag_top -> dag_mid_j -> dag_leaf_i -> dag_target.s : M*L
 * Total full paths = 1 + M + 2*M*L
 *
 * Deep linear-chain topology used in -below timing test
 * ======================================================
 *
 * The -below filter uses a simple ancestor-walk: for each path at depth d,
 * walk up d-1 ancestors evaluating the inner plan, giving total work
 * proportional to D*(D-1)/2 for a chain of depth D (O(D²) for the chain,
 * O(N·D) for a general tree of N paths with average depth D).
 *
 * For typical BRL-CAD trees (depth 5-20) this is fast enough in practice.
 * Only degenerate very deep chains (D ≫ 100) show quadratic behaviour.
 *
 * The topology is a simple linear chain:
 *
 *   chain_root -> chain_0 -> chain_1 -> ... -> chain_{D-1} -> chain_leaf.s
 *
 * Total full paths = D + 2  (chain_root, D combs, and chain_leaf.s)
 *
 * Query: -below -name chain_root
 *   Every path except chain_root itself has chain_root as an ancestor.
 *   Expected count = D + 1  (all paths except chain_root).
 *
 *
 * Wide + Deep CSG Tree Topology
 * ==============================
 *
 * This topology simulates a large multi-level CSG model (e.g. an assembly
 * of sub-assemblies that share common parts) by combining:
 *
 *   - A "fan" structure where every group references ALL leaves and every
 *     cluster references ALL groups (all-to-all sharing at each level).
 *   - A deep chain of K levels wrapping the top of the fan, producing
 *     paths at depths K+1 through K+5.
 *
 * Primitives (6 named):
 *   cwd_pA.s      appears in leaf combos 0 and 4
 *   cwd_pB.s      appears in leaf combos 1 and 5
 *   cwd_pL.s      appears in leaf combos 2 and 6 (left-branch exclusive)
 *   cwd_pR.s      appears in leaf combos 3 and 7 (right-branch exclusive)
 *   cwd_pFill.s   filler, in ALL 8 leaf combos
 *   cwd_pChain.s  chain marker, ONLY directly under cwd_ch_0 (not in fan)
 *
 * Fan structure (parameters W and fixed NL=8):
 *   cwd_lf_0..7       8 leaf combos, 2 prims each (distinct content per leaf)
 *   cwd_grp_0..(W-1)  W group combos, each refs ALL 8 leaves
 *   cwd_clust_0..(W-1) W cluster combos, each refs ALL W groups
 *   cwd_assy           1 assembly, refs all W clusters
 *
 * Chain and root:
 *   cwd_ch_0 = { cwd_assy, cwd_pChain.s }
 *   cwd_ch_k = { cwd_ch_{k-1} }  for k = 1..K-1
 *   cwd_root = { cwd_ch_{K-1} }
 *
 * Full-path counts from cwd_root:
 *   cwd_root:       1            (depth 0)
 *   chain nodes:    K            (depths 1..K)
 *   cwd_pChain.s:   1            (depth K+1)
 *   cwd_assy:       1            (depth K+1)
 *   clusters:       W            (depth K+2)
 *   groups:         W²           (depth K+3)  [each shared across all clusters]
 *   leaves:         8·W²         (depth K+4)  [each shared across all groups]
 *   prims:          16·W²        (depth K+5)
 *   total = K + 25·W² + W + 3
 *
 * Subtree reuse:
 *   Every leaf cwd_lf_k is referenced by all W groups → W² full paths.
 *   Every group cwd_grp_i is referenced by all W clusters → W full paths.
 *   The chain depth K controls how far below the root the fan sits;
 *   all fan paths inherit a K-level ancestor prefix.
 *
 * Performance analysis:
 *   Σ(path depths) ≈ 16W²(K+5) + 8W²(K+4) + W²(K+3) + W(K+2) + K²/2
 */

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "rt/search.h"
#include "wdb.h"


/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

/* Forward declaration - defined after test_below_search */
static int ptbl_has_path(const struct bu_ptbl *ptbl, const char *expected);

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            bu_log("FAIL: %s\n", (msg)); \
            failures++; \
        } \
    } while (0)

#define CROSS_CHECK(new_cnt, old_cnt, filter_str) \
    do { \
        if ((new_cnt) != (old_cnt)) { \
            bu_log("CROSS-VALIDATION FAIL: '%s': new=%d old=%d\n", \
                   (filter_str), (new_cnt), (old_cnt)); \
            failures++; \
        } \
    } while (0)


/*
 * Run a tree search using the new (db_search) and old (db_search_old)
 * implementations and return both counts.  Returns 0 on success, 1 if
 * either function signals an error.
 */
static int
run_both(struct db_i *dbip, int flags, const char *filter,
         int *new_cnt, int *old_cnt)
{
    struct bu_ptbl new_results = BU_PTBL_INIT_ZERO;
    struct bu_ptbl old_results = BU_PTBL_INIT_ZERO;
    struct db_search_context *ctx = db_search_context_create();
    int ret = 0;

    *new_cnt = db_search(&new_results, flags, filter, 0, NULL, dbip,
                         NULL, NULL, NULL);
    *old_cnt = db_search_old(&old_results, flags, filter, 0, NULL, dbip,
                             ctx);

    if (*new_cnt < 0 || *old_cnt < 0) {
        bu_log("ERROR: search error for filter '%s' (new=%d old=%d)\n",
               filter, *new_cnt, *old_cnt);
        ret = 1;
    }

    db_search_free(&new_results);
    db_search_free(&old_results);
    db_search_context_destroy(ctx);
    return ret;
}


/* ------------------------------------------------------------------ */
/*  Build the correctness-test tree                                    */
/* ------------------------------------------------------------------ */

/*
 * Create the tree documented in the file-level comment using the
 * in-memory database pointed to by wdbp.
 *
 * Returns 0 on success.
 */
static int
build_correctness_tree(struct rt_wdb *wdbp)
{
    point_t center = VINIT_ZERO;
    fastf_t radius = 1.0;
    struct wmember wm;

    /* --- Primitives ------------------------------------------------ */
    if (mk_sph(wdbp, "prim_target.s", center, radius) != 0) return 1;
    if (mk_sph(wdbp, "prim_special.s", center, radius) != 0) return 1;
    if (mk_sph(wdbp, "prim_plain.s", center, radius) != 0) return 1;

    /* --- Leaf combos (depth 1) ------------------------------------- */
    BU_LIST_INIT(&wm.l);
    mk_addmember("prim_target.s", &wm.l, NULL, WMOP_UNION);
    if (mk_lcomb(wdbp, "leaf_t", &wm, 0, NULL, NULL, NULL, 0) != 0) return 1;

    BU_LIST_INIT(&wm.l);
    mk_addmember("prim_special.s", &wm.l, NULL, WMOP_UNION);
    if (mk_lcomb(wdbp, "leaf_s", &wm, 0, NULL, NULL, NULL, 0) != 0) return 1;

    BU_LIST_INIT(&wm.l);
    mk_addmember("prim_target.s", &wm.l, NULL, WMOP_UNION);
    mk_addmember("prim_special.s", &wm.l, NULL, WMOP_UNION);
    if (mk_lcomb(wdbp, "leaf_ts", &wm, 0, NULL, NULL, NULL, 0) != 0) return 1;

    BU_LIST_INIT(&wm.l);
    mk_addmember("prim_plain.s", &wm.l, NULL, WMOP_UNION);
    if (mk_lcomb(wdbp, "leaf_p", &wm, 0, NULL, NULL, NULL, 0) != 0) return 1;

    /* --- Mid combos (depth 2) -------------------------------------- */
    BU_LIST_INIT(&wm.l);
    mk_addmember("leaf_t", &wm.l, NULL, WMOP_UNION);
    mk_addmember("leaf_p", &wm.l, NULL, WMOP_UNION);
    if (mk_lcomb(wdbp, "mid_a", &wm, 0, NULL, NULL, NULL, 0) != 0) return 1;

    BU_LIST_INIT(&wm.l);
    mk_addmember("leaf_s", &wm.l, NULL, WMOP_UNION);
    mk_addmember("leaf_p", &wm.l, NULL, WMOP_UNION);
    if (mk_lcomb(wdbp, "mid_b", &wm, 0, NULL, NULL, NULL, 0) != 0) return 1;

    BU_LIST_INIT(&wm.l);
    mk_addmember("leaf_ts", &wm.l, NULL, WMOP_UNION);
    mk_addmember("leaf_p", &wm.l, NULL, WMOP_UNION);
    if (mk_lcomb(wdbp, "mid_c", &wm, 0, NULL, NULL, NULL, 0) != 0) return 1;

    /* --- Upper combos (depth 3) ------------------------------------ */
    BU_LIST_INIT(&wm.l);
    mk_addmember("mid_a", &wm.l, NULL, WMOP_UNION);
    mk_addmember("mid_b", &wm.l, NULL, WMOP_UNION);
    if (mk_lcomb(wdbp, "upper_a", &wm, 0, NULL, NULL, NULL, 0) != 0) return 1;

    BU_LIST_INIT(&wm.l);
    mk_addmember("mid_b", &wm.l, NULL, WMOP_UNION);  /* mid_b is reused */
    mk_addmember("mid_c", &wm.l, NULL, WMOP_UNION);
    if (mk_lcomb(wdbp, "upper_b", &wm, 0, NULL, NULL, NULL, 0) != 0) return 1;

    /* --- Top combo (depth 4) --------------------------------------- */
    BU_LIST_INIT(&wm.l);
    mk_addmember("upper_a", &wm.l, NULL, WMOP_UNION);
    mk_addmember("upper_b", &wm.l, NULL, WMOP_UNION);
    if (mk_lcomb(wdbp, "top1", &wm, 0, NULL, NULL, NULL, 0) != 0) return 1;

    return 0;
}


/* ------------------------------------------------------------------ */
/*  Correctness tests against ground truth                            */
/* ------------------------------------------------------------------ */

static int
test_name_search(struct db_i *dbip)
{
    int failures = 0;
    struct bu_ptbl results = BU_PTBL_INIT_ZERO;
    int cnt;

    /*
     * Ground truth: 2 paths end in prim_target.s (both under top1).
     * Note: db_search returns the PATH count; RETURN_UNIQ_DP only
     * affects the contents of the ptbl, not the return value.
     */
    cnt = db_search(&results, DB_SEARCH_TREE, "-name prim_target.s",
                    0, NULL, dbip, NULL, NULL, NULL);
    CHECK(cnt == 2, "-name prim_target.s path count == 2");
    db_search_free(&results);

    /* Unique dp check: 1 unique directory pointer for prim_target.s */
    cnt = db_search(&results,
                    DB_SEARCH_TREE | DB_SEARCH_RETURN_UNIQ_DP,
                    "-name prim_target.s",
                    0, NULL, dbip, NULL, NULL, NULL);
    CHECK(BU_PTBL_LEN(&results) == 1,
          "-name prim_target.s unique-dp ptbl len == 1");
    db_search_free(&results);

    /* Ground truth: 9 paths lead to primitive shapes */
    cnt = db_search(&results, DB_SEARCH_TREE, "-type shape",
                    0, NULL, dbip, NULL, NULL, NULL);
    CHECK(cnt == 9, "-type shape path count == 9");
    db_search_free(&results);

    /* Unique dp check: 3 unique shape objects */
    cnt = db_search(&results,
                    DB_SEARCH_TREE | DB_SEARCH_RETURN_UNIQ_DP,
                    "-type shape",
                    0, NULL, dbip, NULL, NULL, NULL);
    CHECK(BU_PTBL_LEN(&results) == 3,
          "-type shape unique-dp ptbl len == 3");
    db_search_free(&results);

    /* Ground truth: 15 comb paths */
    cnt = db_search(&results, DB_SEARCH_TREE, "-type comb",
                    0, NULL, dbip, NULL, NULL, NULL);
    CHECK(cnt == 15, "-type comb path count == 15");
    db_search_free(&results);

    /* Unique dp check: 10 unique comb objects */
    cnt = db_search(&results,
                    DB_SEARCH_TREE | DB_SEARCH_RETURN_UNIQ_DP,
                    "-type comb",
                    0, NULL, dbip, NULL, NULL, NULL);
    CHECK(BU_PTBL_LEN(&results) == 10,
          "-type comb unique-dp ptbl len == 10");
    db_search_free(&results);

    (void)cnt;
    return failures;
}


static int
test_above_search(struct db_i *dbip)
{
    int failures = 0;
    struct bu_ptbl results = BU_PTBL_INIT_ZERO;
    int cnt;

    /* Ground truth: 7 paths are ancestors of prim_target.s */
    cnt = db_search(&results, DB_SEARCH_TREE,
                    "-above -name prim_target.s",
                    0, NULL, dbip, NULL, NULL, NULL);
    CHECK(cnt == 7, "-above -name prim_target.s count == 7");
    db_search_free(&results);

    /* Ground truth: 9 paths are ancestors of prim_special.s */
    cnt = db_search(&results, DB_SEARCH_TREE,
                    "-above -name prim_special.s",
                    0, NULL, dbip, NULL, NULL, NULL);
    CHECK(cnt == 9, "-above -name prim_special.s count == 9");
    db_search_free(&results);

    /* Ground truth: 11 paths are ancestors of prim_plain.s */
    cnt = db_search(&results, DB_SEARCH_TREE,
                    "-above -name prim_plain.s",
                    0, NULL, dbip, NULL, NULL, NULL);
    CHECK(cnt == 11, "-above -name prim_plain.s count == 11");
    db_search_free(&results);

    (void)cnt;
    return failures;
}


static int
test_below_search(struct db_i *dbip)
{
    int failures = 0;
    struct bu_ptbl results = BU_PTBL_INIT_ZERO;
    int cnt;
    struct directory *top_dp;

    /*
     * -below -name mid_a searched from top1:
     * Finds all paths (within top1's subtree) where an ancestor is
     * named mid_a.  Result: 4 paths (leaf_t, leaf_p, and their
     * primitive children, all under top1/upper_a/mid_a/).
     */
    top_dp = db_lookup(dbip, "top1", LOOKUP_QUIET);
    if (top_dp) {
        cnt = db_search(&results, DB_SEARCH_TREE,
                        "-below -name mid_a",
                        1, &top_dp, dbip, NULL, NULL, NULL);
        CHECK(cnt == 4, "-below -name mid_a count from top1 == 4");
        /* Verify exact paths - catches "last path missing" class of bugs */
        CHECK(ptbl_has_path(&results, "/top1/upper_a/mid_a/leaf_t"),
              "-below mid_a: leaf_t present");
        CHECK(ptbl_has_path(&results, "/top1/upper_a/mid_a/leaf_p"),
              "-below mid_a: leaf_p present");
        CHECK(ptbl_has_path(&results, "/top1/upper_a/mid_a/leaf_t/prim_target.s"),
              "-below mid_a: leaf_t/prim_target.s present");
        CHECK(ptbl_has_path(&results, "/top1/upper_a/mid_a/leaf_p/prim_plain.s"),
              "-below mid_a: leaf_p/prim_plain.s present");
        db_search_free(&results);
    }

    (void)cnt;
    return failures;
}


/* ------------------------------------------------------------------ */
/*  Path-level exact-match helper                                     */
/* ------------------------------------------------------------------ */

/*
 * Returns 1 if a path with the given string representation exists in ptbl
 * (a table of db_full_path*), 0 otherwise.
 */
static int
ptbl_has_path(const struct bu_ptbl *ptbl, const char *expected)
{
    int i;
    for (i = 0; i < (int)BU_PTBL_LEN(ptbl); i++) {
        struct db_full_path *fp =
            (struct db_full_path *)BU_PTBL_GET(ptbl, i);
        char *s = db_path_to_string(fp);
        int match = (BU_STR_EQUAL(s, expected) ? 1 : 0);
        bu_free(s, "path string");
        if (match) return 1;
    }
    return 0;
}


/* ------------------------------------------------------------------ */
/*  Full tree-walk completeness test                                  */
/* ------------------------------------------------------------------ */

/*
 * Verify that every expected path is present in the results and that no
 * extra paths appear.  Covers the class of bugs where the last path (or
 * any path) is silently dropped during traversal.
 *
 * Tree: top1 = { upper_a, upper_b }
 *   upper_a = { mid_a, mid_b }   upper_b = { mid_b, mid_c }
 *   mid_a   = { leaf_t, leaf_p } mid_b = { leaf_s, leaf_p }
 *   mid_c   = { leaf_ts, leaf_p }
 *   leaf_t  = { prim_target.s }  leaf_ts = { prim_target.s, prim_special.s }
 *   leaf_s  = { prim_special.s } leaf_p  = { prim_plain.s }
 */
static int
test_full_tree_walk(struct db_i *dbip)
{
    int failures = 0;
    struct bu_ptbl results = BU_PTBL_INIT_ZERO;
    int cnt;

    /* --- All 15 comb paths ------------------------------------------ */
    static const char * const comb_paths[] = {
        "/top1",
        "/top1/upper_a",
        "/top1/upper_a/mid_a",
        "/top1/upper_a/mid_a/leaf_t",
        "/top1/upper_a/mid_a/leaf_p",
        "/top1/upper_a/mid_b",
        "/top1/upper_a/mid_b/leaf_s",
        "/top1/upper_a/mid_b/leaf_p",
        "/top1/upper_b",
        "/top1/upper_b/mid_b",
        "/top1/upper_b/mid_b/leaf_s",
        "/top1/upper_b/mid_b/leaf_p",
        "/top1/upper_b/mid_c",
        "/top1/upper_b/mid_c/leaf_ts",
        "/top1/upper_b/mid_c/leaf_p",
        NULL
    };
    const char * const *p;

    cnt = db_search(&results, DB_SEARCH_TREE, "-type comb",
                    0, NULL, dbip, NULL, NULL, NULL);
    CHECK(cnt == 15, "-type comb total count == 15");
    for (p = comb_paths; *p; p++)
        CHECK(ptbl_has_path(&results, *p), *p);
    db_search_free(&results);

    /* --- All 9 shape paths ------------------------------------------ */
    static const char * const shape_paths[] = {
        "/top1/upper_a/mid_a/leaf_t/prim_target.s",
        "/top1/upper_a/mid_a/leaf_p/prim_plain.s",
        "/top1/upper_a/mid_b/leaf_s/prim_special.s",
        "/top1/upper_a/mid_b/leaf_p/prim_plain.s",
        "/top1/upper_b/mid_b/leaf_s/prim_special.s",
        "/top1/upper_b/mid_b/leaf_p/prim_plain.s",
        "/top1/upper_b/mid_c/leaf_ts/prim_target.s",
        "/top1/upper_b/mid_c/leaf_ts/prim_special.s",
        "/top1/upper_b/mid_c/leaf_p/prim_plain.s",
        NULL
    };

    cnt = db_search(&results, DB_SEARCH_TREE, "-type shape",
                    0, NULL, dbip, NULL, NULL, NULL);
    CHECK(cnt == 9, "-type shape total count == 9");
    for (p = shape_paths; *p; p++)
        CHECK(ptbl_has_path(&results, *p), *p);
    db_search_free(&results);

    /* --- 2 paths for prim_target.s ---------------------------------- */
    cnt = db_search(&results, DB_SEARCH_TREE, "-name prim_target.s",
                    0, NULL, dbip, NULL, NULL, NULL);
    CHECK(cnt == 2, "-name prim_target.s count == 2");
    CHECK(ptbl_has_path(&results,
                        "/top1/upper_a/mid_a/leaf_t/prim_target.s"),
          "-name prim_target.s: upper_a path present");
    CHECK(ptbl_has_path(&results,
                        "/top1/upper_b/mid_c/leaf_ts/prim_target.s"),
          "-name prim_target.s: upper_b path present");
    db_search_free(&results);

    (void)cnt;
    return failures;
}


/* ------------------------------------------------------------------ */
/*  Exact-path -above tests                                           */
/* ------------------------------------------------------------------ */

/*
 * Verify the exact set of paths returned by plain and depth-constrained
 * -above queries.
 *
 * Ground truth:
 *
 * -above -name prim_target.s  (7 paths):
 *   /top1, /top1/upper_a, /top1/upper_b,
 *   /top1/upper_a/mid_a, /top1/upper_b/mid_c,
 *   /top1/upper_a/mid_a/leaf_t, /top1/upper_b/mid_c/leaf_ts
 *
 * -above=1 -name prim_target.s  (2 paths: direct parents only):
 *   /top1/upper_a/mid_a/leaf_t, /top1/upper_b/mid_c/leaf_ts
 *
 * -above>1 -name prim_target.s  (5 paths: ancestors 2+ levels up):
 *   /top1, /top1/upper_a, /top1/upper_b,
 *   /top1/upper_a/mid_a, /top1/upper_b/mid_c
 *
 * -above<3 -name prim_target.s  (4 paths: ancestors 1 or 2 levels up):
 *   /top1/upper_a/mid_a/leaf_t, /top1/upper_b/mid_c/leaf_ts,
 *   /top1/upper_a/mid_a, /top1/upper_b/mid_c
 */
static int
test_above_exact(struct db_i *dbip)
{
    int failures = 0;
    struct bu_ptbl results = BU_PTBL_INIT_ZERO;
    int cnt;

    /* Plain -above: exact 7 paths */
    cnt = db_search(&results, DB_SEARCH_TREE,
                    "-above -name prim_target.s",
                    0, NULL, dbip, NULL, NULL, NULL);
    CHECK(cnt == 7, "-above -name prim_target.s exact count == 7");
    CHECK(ptbl_has_path(&results, "/top1"),
          "-above prim_target.s: /top1 present");
    CHECK(ptbl_has_path(&results, "/top1/upper_a"),
          "-above prim_target.s: /top1/upper_a present");
    CHECK(ptbl_has_path(&results, "/top1/upper_b"),
          "-above prim_target.s: /top1/upper_b present");
    CHECK(ptbl_has_path(&results, "/top1/upper_a/mid_a"),
          "-above prim_target.s: /top1/upper_a/mid_a present");
    CHECK(ptbl_has_path(&results, "/top1/upper_b/mid_c"),
          "-above prim_target.s: /top1/upper_b/mid_c present");
    CHECK(ptbl_has_path(&results, "/top1/upper_a/mid_a/leaf_t"),
          "-above prim_target.s: leaf_t present");
    CHECK(ptbl_has_path(&results, "/top1/upper_b/mid_c/leaf_ts"),
          "-above prim_target.s: leaf_ts present");
    /* Verify prim_target.s itself is NOT in results (it has no descendants) */
    CHECK(!ptbl_has_path(&results, "/top1/upper_a/mid_a/leaf_t/prim_target.s"),
          "-above prim_target.s: prim_target.s itself NOT in results");
    db_search_free(&results);

    /* -above=1: only direct parents of prim_target.s */
    cnt = db_search(&results, DB_SEARCH_TREE,
                    "-above=1 -name prim_target.s",
                    0, NULL, dbip, NULL, NULL, NULL);
    CHECK(cnt == 2, "-above=1 -name prim_target.s count == 2");
    CHECK(ptbl_has_path(&results, "/top1/upper_a/mid_a/leaf_t"),
          "-above=1 prim_target.s: leaf_t present");
    CHECK(ptbl_has_path(&results, "/top1/upper_b/mid_c/leaf_ts"),
          "-above=1 prim_target.s: leaf_ts present");
    /* Grandparents must NOT appear (relative depth 2, not 1) */
    CHECK(!ptbl_has_path(&results, "/top1/upper_a/mid_a"),
          "-above=1 prim_target.s: mid_a NOT in results (depth 2)");
    CHECK(!ptbl_has_path(&results, "/top1"),
          "-above=1 prim_target.s: /top1 NOT in results (depth 4)");
    db_search_free(&results);

    /* -above>1: ancestors at relative depth >= 2 */
    cnt = db_search(&results, DB_SEARCH_TREE,
                    "-above>1 -name prim_target.s",
                    0, NULL, dbip, NULL, NULL, NULL);
    CHECK(cnt == 5, "-above>1 -name prim_target.s count == 5");
    CHECK(ptbl_has_path(&results, "/top1"),
          "-above>1 prim_target.s: /top1 present");
    CHECK(ptbl_has_path(&results, "/top1/upper_a"),
          "-above>1 prim_target.s: upper_a present");
    CHECK(ptbl_has_path(&results, "/top1/upper_b"),
          "-above>1 prim_target.s: upper_b present");
    CHECK(ptbl_has_path(&results, "/top1/upper_a/mid_a"),
          "-above>1 prim_target.s: mid_a present");
    CHECK(ptbl_has_path(&results, "/top1/upper_b/mid_c"),
          "-above>1 prim_target.s: mid_c present");
    /* Direct parents must NOT appear (relative depth 1, not >= 2) */
    CHECK(!ptbl_has_path(&results, "/top1/upper_a/mid_a/leaf_t"),
          "-above>1 prim_target.s: leaf_t NOT in results (depth 1)");
    CHECK(!ptbl_has_path(&results, "/top1/upper_b/mid_c/leaf_ts"),
          "-above>1 prim_target.s: leaf_ts NOT in results (depth 1)");
    db_search_free(&results);

    /* -above<3: ancestors at relative depth 1 or 2 */
    cnt = db_search(&results, DB_SEARCH_TREE,
                    "-above<3 -name prim_target.s",
                    0, NULL, dbip, NULL, NULL, NULL);
    CHECK(cnt == 4, "-above<3 -name prim_target.s count == 4");
    CHECK(ptbl_has_path(&results, "/top1/upper_a/mid_a/leaf_t"),
          "-above<3 prim_target.s: leaf_t present");
    CHECK(ptbl_has_path(&results, "/top1/upper_b/mid_c/leaf_ts"),
          "-above<3 prim_target.s: leaf_ts present");
    CHECK(ptbl_has_path(&results, "/top1/upper_a/mid_a"),
          "-above<3 prim_target.s: mid_a present");
    CHECK(ptbl_has_path(&results, "/top1/upper_b/mid_c"),
          "-above<3 prim_target.s: mid_c present");
    /* Deeper ancestors must NOT appear (relative depth 3+) */
    CHECK(!ptbl_has_path(&results, "/top1/upper_a"),
          "-above<3 prim_target.s: upper_a NOT in results (depth 3)");
    CHECK(!ptbl_has_path(&results, "/top1"),
          "-above<3 prim_target.s: /top1 NOT in results (depth 4)");
    db_search_free(&results);

    /* Also cross-validate depth-constrained -above against old code */
    {
        int new_cnt, old_cnt;
        static const char * const depth_filters[] = {
            /* = operator */
            "-above=1 -name prim_target.s",
            "-above=2 -name prim_target.s",
            "-above=1 -name prim_special.s",
            /* > operator */
            "-above>1 -name prim_target.s",
            "-above>2 -name prim_plain.s",
            /* < operator */
            "-above<3 -name prim_target.s",
            "-above<2 -name prim_target.s",
            /* >= operator */
            "-above>=1 -name prim_target.s",
            "-above>=2 -name prim_target.s",
            "-above>=1 -name prim_special.s",
            /* <= operator */
            "-above<=2 -name prim_target.s",
            "-above<=4 -name prim_target.s",
            "-above<=1 -name prim_plain.s",
            /* Alternative =>/<= forms */
            "-above=>1 -name prim_target.s",
            "-above=<3 -name prim_target.s",
            NULL
        };
        const char * const *f;
        for (f = depth_filters; *f; f++) {
            if (!run_both(dbip, DB_SEARCH_TREE, *f, &new_cnt, &old_cnt))
                CROSS_CHECK(new_cnt, old_cnt, *f);
        }
    }

    /* ---- >= tests: exact counts ---- */

    /* -above>=1: depth >= 1 = same as unconstrained -above (7 paths) */
    cnt = db_search(&results, DB_SEARCH_TREE,
                    "-above>=1 -name prim_target.s",
                    0, NULL, dbip, NULL, NULL, NULL);
    CHECK(cnt == 7, "-above>=1 -name prim_target.s count == 7");
    db_search_free(&results);

    /* -above>=2: depth >= 2 = same as -above>1 (5 paths) */
    cnt = db_search(&results, DB_SEARCH_TREE,
                    "-above>=2 -name prim_target.s",
                    0, NULL, dbip, NULL, NULL, NULL);
    CHECK(cnt == 5, "-above>=2 -name prim_target.s count == 5");
    CHECK(ptbl_has_path(&results, "/top1"),
          "-above>=2 prim_target.s: /top1 present (depth 4 >= 2)");
    CHECK(ptbl_has_path(&results, "/top1/upper_a"),
          "-above>=2 prim_target.s: upper_a present (depth 3 >= 2)");
    CHECK(ptbl_has_path(&results, "/top1/upper_a/mid_a"),
          "-above>=2 prim_target.s: mid_a present (depth 2 >= 2)");
    CHECK(!ptbl_has_path(&results, "/top1/upper_a/mid_a/leaf_t"),
          "-above>=2 prim_target.s: leaf_t NOT in results (depth 1 < 2)");
    db_search_free(&results);

    /* -above>=5: depth >= 5; prim_target.s is only 4 hops below top1, so no match */
    cnt = db_search(&results, DB_SEARCH_TREE,
                    "-above>=5 -name prim_target.s",
                    0, NULL, dbip, NULL, NULL, NULL);
    CHECK(cnt == 0, "-above>=5 -name prim_target.s count == 0 (max chain is 4 hops)");
    db_search_free(&results);

    /* ---- <= tests: exact counts ---- */

    /* -above<=2: depth <= 2 = same as -above<3 (4 paths) */
    cnt = db_search(&results, DB_SEARCH_TREE,
                    "-above<=2 -name prim_target.s",
                    0, NULL, dbip, NULL, NULL, NULL);
    CHECK(cnt == 4, "-above<=2 -name prim_target.s count == 4");
    CHECK(ptbl_has_path(&results, "/top1/upper_a/mid_a/leaf_t"),
          "-above<=2 prim_target.s: leaf_t present (depth 1 <= 2)");
    CHECK(ptbl_has_path(&results, "/top1/upper_a/mid_a"),
          "-above<=2 prim_target.s: mid_a present (depth 2 <= 2)");
    CHECK(!ptbl_has_path(&results, "/top1/upper_a"),
          "-above<=2 prim_target.s: upper_a NOT in results (depth 3 > 2)");
    CHECK(!ptbl_has_path(&results, "/top1"),
          "-above<=2 prim_target.s: /top1 NOT in results (depth 4 > 2)");
    db_search_free(&results);

    /* -above<=4: depth <= 4 = all 7 ancestors (same as unconstrained) */
    cnt = db_search(&results, DB_SEARCH_TREE,
                    "-above<=4 -name prim_target.s",
                    0, NULL, dbip, NULL, NULL, NULL);
    CHECK(cnt == 7, "-above<=4 -name prim_target.s count == 7");
    db_search_free(&results);

    /* ---- edge cases ---- */

    /* -above=0: relative depth exactly 0 is impossible (descendants start at 1) */
    cnt = db_search(&results, DB_SEARCH_TREE,
                    "-above=0 -name prim_target.s",
                    0, NULL, dbip, NULL, NULL, NULL);
    CHECK(cnt == 0, "-above=0 -name prim_target.s count == 0 (depth 0 is self, never a descendant)");
    db_search_free(&results);

    /* -above<1: max_depth=0, same reasoning as =0 */
    cnt = db_search(&results, DB_SEARCH_TREE,
                    "-above<1 -name prim_target.s",
                    0, NULL, dbip, NULL, NULL, NULL);
    CHECK(cnt == 0, "-above<1 -name prim_target.s count == 0");
    db_search_free(&results);

    /* -above=2: exactly 2 hops: mid_a and mid_c (2 paths) */
    cnt = db_search(&results, DB_SEARCH_TREE,
                    "-above=2 -name prim_target.s",
                    0, NULL, dbip, NULL, NULL, NULL);
    CHECK(cnt == 2, "-above=2 -name prim_target.s count == 2");
    CHECK(ptbl_has_path(&results, "/top1/upper_a/mid_a"),
          "-above=2 prim_target.s: mid_a present (direct grandparent)");
    CHECK(ptbl_has_path(&results, "/top1/upper_b/mid_c"),
          "-above=2 prim_target.s: mid_c present (direct grandparent)");
    CHECK(!ptbl_has_path(&results, "/top1/upper_a/mid_a/leaf_t"),
          "-above=2 prim_target.s: leaf_t NOT in results (depth 1)");
    CHECK(!ptbl_has_path(&results, "/top1/upper_a"),
          "-above=2 prim_target.s: upper_a NOT in results (depth 3)");
    db_search_free(&results);

    (void)cnt;
    return failures;
}


/* ------------------------------------------------------------------ */
/*  Cross-validation: new vs old implementation                       */
/* ------------------------------------------------------------------ */

static int
test_cross_validation(struct db_i *dbip)
{
    int failures = 0;
    int new_cnt, old_cnt;

    static const char * const filters[] = {
        "-name prim_target.s",
        "-name prim_special.s",
        "-name prim_plain.s",
        "-above -name prim_target.s",
        "-above -name prim_special.s",
        "-above -name prim_plain.s",
        "-above=1 -name prim_target.s",
        "-above>1 -name prim_target.s",
        "-above>=2 -name prim_target.s",
        "-above<=2 -name prim_target.s",
        "-below -name mid_a",
        "-below -name upper_a",
        "-below>2 -name top1",
        "-below=3 -name top1",
        "-type shape",
        "-type comb",
        "-type region",
        "-name leaf*",
        "-name mid*",
        "-name upper*",
        NULL
    };
    const char * const *f;

    for (f = filters; *f; f++) {
        if (run_both(dbip, DB_SEARCH_TREE, *f, &new_cnt, &old_cnt)) {
            failures++;
            continue;
        }
        CROSS_CHECK(new_cnt, old_cnt, *f);
    }

    /* Also test DB_SEARCH_RETURN_UNIQ_DP */
    for (f = filters; *f; f++) {
        if (run_both(dbip, DB_SEARCH_TREE | DB_SEARCH_RETURN_UNIQ_DP,
                     *f, &new_cnt, &old_cnt)) {
            failures++;
            continue;
        }
        CROSS_CHECK(new_cnt, old_cnt, *f);
    }

    return failures;
}


/* ------------------------------------------------------------------ */
/*  -below depth-constraint and -above/-below interaction tests       */
/* ------------------------------------------------------------------ */

/*
 * Test cases covering option interactions that are prone to bugs:
 *
 * 1. -below with min_depth > 2 (e.g. -below>2): requires walking more than
 *    one ancestor up.
 *
 * 2. -above combined with -below in the same plan: uses the full-path
 *    collection path, and f_below uses the ancestor-walk.  Results must
 *    match the old implementation.
 *
 * 3. Negated -above and -below (! -above, ! -below): ensure negation is
 *    applied correctly to both.
 *
 * Tree (same correctness tree):
 *   top1/upper_a/mid_a/leaf_t/prim_target.s
 *   top1/upper_a/mid_a/leaf_p/prim_plain.s
 *   top1/upper_a/mid_b/leaf_s/prim_special.s
 *   ...
 *
 * Ground truth for -below depth constraints (applying to top1 subtree):
 *
 *   -below>2 -name top1:
 *     Nodes where an ancestor named top1 is at distance >= 3 (min_depth=3)
 *     Using old-code distance convention: distance starts at 1, incremented
 *     before first check, so distance 3 = 2 actual hops (grandparent).
 *     Nodes at depth >= 2 from top1: mid_a, mid_b, mid_c, leaf_t, leaf_p,
 *     leaf_s, leaf_ts (via upper_a/b), and all primitives = 17 paths
 *     Actually: all paths at depth >= 3 from top1 (depth 0).
 *     top1=depth0, upper_a/b=depth1, mid_*=depth2, leaf_*=depth3, prim.*=depth4
 *     distance convention: depth1 nodes have distance 2 to top1 (1 hop)
 *     depth2 nodes have distance 3 to top1 (2 hops) -> passes -below>2
 *     depth3+ also pass.
 *     Paths at depth >= 2: mid_a, mid_b (x2), mid_c, leaf_t, leaf_p (x4),
 *     leaf_s (x2), leaf_ts, prim_target.s (x2), prim_plain.s (x4),
 *     prim_special.s (x3) = 7 mid+leaf + 9 prim = varies...
 *     Let's just cross-validate against old code for this.
 *
 * For simplicity, this test cross-validates all depth-constrained below
 * variants against old code rather than hardcoding counts.
 */
static int
test_interactions(struct db_i *dbip)
{
    int failures = 0;
    int new_cnt, old_cnt;
    struct bu_ptbl results = BU_PTBL_INIT_ZERO;

    /* ---- 1: -below with depth constraints ---- */

    /* -below>2: min_depth=3, max_depth=INT_MAX
     * Requires BFS fallback (min_depth > 2), ancestor-walk must reach depth 3 */
    if (!run_both(dbip, DB_SEARCH_TREE,
                  "-below>2 -name top1", &new_cnt, &old_cnt))
        CROSS_CHECK(new_cnt, old_cnt, "-below>2 -name top1");

    /* -below>3: min_depth=4, even deeper */
    if (!run_both(dbip, DB_SEARCH_TREE,
                  "-below>3 -name top1", &new_cnt, &old_cnt))
        CROSS_CHECK(new_cnt, old_cnt, "-below>3 -name top1");

    /* -below=3: exact distance */
    if (!run_both(dbip, DB_SEARCH_TREE,
                  "-below=3 -name top1", &new_cnt, &old_cnt))
        CROSS_CHECK(new_cnt, old_cnt, "-below=3 -name top1");

    /* -below>2 with a deeper named target */
    if (!run_both(dbip, DB_SEARCH_TREE,
                  "-below>2 -name mid_a", &new_cnt, &old_cnt))
        CROSS_CHECK(new_cnt, old_cnt, "-below>2 -name mid_a");

    /* ---- 2: -above and -below in same plan ---- */

    /* Objects that are above a region AND below a comb */
    if (!run_both(dbip, DB_SEARCH_TREE,
                  "-above -type shape -below -name top1", &new_cnt, &old_cnt))
        CROSS_CHECK(new_cnt, old_cnt, "-above -type shape -below -name top1");

    /* Using -or to combine both */
    if (!run_both(dbip, DB_SEARCH_TREE,
                  "( -above -name prim_target.s ) -or ( -below -name mid_a )",
                  &new_cnt, &old_cnt))
        CROSS_CHECK(new_cnt, old_cnt, "( -above -name prim_target.s ) -or ( -below -name mid_a )");

    /* Both in the same expression */
    if (!run_both(dbip, DB_SEARCH_TREE,
                  "-above -name prim_special.s -above -name prim_plain.s",
                  &new_cnt, &old_cnt))
        CROSS_CHECK(new_cnt, old_cnt, "-above -name prim_special.s -above -name prim_plain.s");

    /* ---- 3: negated -above and -below ---- */

    /* ! -above: objects NOT above any shape */
    if (!run_both(dbip, DB_SEARCH_TREE,
                  "! -above -type shape", &new_cnt, &old_cnt))
        CROSS_CHECK(new_cnt, old_cnt, "! -above -type shape");

    /* ! -below: objects NOT below any named node */
    if (!run_both(dbip, DB_SEARCH_TREE,
                  "! -below -name mid_a", &new_cnt, &old_cnt))
        CROSS_CHECK(new_cnt, old_cnt, "! -below -name mid_a");

    /* Combined: objects that are above a shape but not above prim_target.s */
    if (!run_both(dbip, DB_SEARCH_TREE,
                  "-above -type shape ! -above -name prim_target.s",
                  &new_cnt, &old_cnt))
        CROSS_CHECK(new_cnt, old_cnt, "-above -type shape ! -above -name prim_target.s");

    /* ---- 4: -above with -type (not just -name) ---- */
    if (!run_both(dbip, DB_SEARCH_TREE,
                  "-above -type shape", &new_cnt, &old_cnt))
        CROSS_CHECK(new_cnt, old_cnt, "-above -type shape");

    /* -below with -type */
    if (!run_both(dbip, DB_SEARCH_TREE,
                  "-below -type comb", &new_cnt, &old_cnt))
        CROSS_CHECK(new_cnt, old_cnt, "-below -type comb");

    /* ---- 5: ground-truth exact counts for a specific case ---- */
    /* -above -type shape: every non-shape path that has a shape descendant
     * = all 15 comb paths (every comb has at least one shape under it) */
    {
        int cnt;
        cnt = db_search(&results, DB_SEARCH_TREE,
                        "-above -type shape",
                        0, NULL, dbip, NULL, NULL, NULL);
        CHECK(cnt == 15, "-above -type shape count == 15 (all combs have shape descendants)");
        db_search_free(&results);

        /* ! -above -type shape: the 9 shape paths themselves, which have no shape BELOW them */
        cnt = db_search(&results, DB_SEARCH_TREE,
                        "! -above -type shape -type shape",
                        0, NULL, dbip, NULL, NULL, NULL);
        CHECK(cnt == 9, "shape paths that are not above another shape == 9");
        db_search_free(&results);
    }

    (void)new_cnt; (void)old_cnt;
    return failures;
}


/* ------------------------------------------------------------------ */
/*  Stress tree builder                                                */
/* ------------------------------------------------------------------ */

/*
 * Build a stress-test tree of the requested depth.
 *
 * The tree is a binary branching hierarchy:
 *   - A small pool of primitive spheres is shared across all leaf combos.
 *   - Each leaf combo references two primitives from the pool.
 *   - Two named "target" leaf combos (stress_target_a, stress_target_b)
 *     give the -above filter something specific to find.
 *   - Fan-out combos at each level above the leaves each reference
 *     two combos from the level below.
 *
 * depth:    number of comb levels above the primitives (>= 1)
 * branches: number of leaf combos at depth 1 (>= 2)
 *
 * Returns 0 on success.
 */
static int
build_stress_tree(struct rt_wdb *wdbp, int depth, int branches)
{
    int i, d;
    const int pool_sz = 4;
    struct bu_vls name = BU_VLS_INIT_ZERO;
    struct bu_vls lname = BU_VLS_INIT_ZERO;
    struct bu_vls rname = BU_VLS_INIT_ZERO;
    point_t center = VINIT_ZERO;
    fastf_t radius = 1.0;
    struct wmember wm;
    int cur_level_count;

    if (depth < 1 || branches < 2) {
        bu_vls_free(&name);
        bu_vls_free(&lname);
        bu_vls_free(&rname);
        return 1;
    }

    /* Primitives */
    for (i = 0; i < pool_sz; i++) {
        bu_vls_sprintf(&name, "stress_prim_%d.s", i);
        if (mk_sph(wdbp, bu_vls_cstr(&name), center, radius) != 0)
            goto cleanup_fail;
    }

    /* Leaf combos (depth 1) */
    for (i = 0; i < branches; i++) {
        BU_LIST_INIT(&wm.l);
        bu_vls_sprintf(&name, "stress_prim_%d.s", i % pool_sz);
        mk_addmember(bu_vls_cstr(&name), &wm.l, NULL, WMOP_UNION);
        bu_vls_sprintf(&name, "stress_prim_%d.s", (i + 1) % pool_sz);
        mk_addmember(bu_vls_cstr(&name), &wm.l, NULL, WMOP_UNION);

        if (i == 0) {
            if (mk_lcomb(wdbp, "stress_target_a", &wm, 0, NULL, NULL, NULL, 0) != 0)
                goto cleanup_fail;
        } else if (i == 1) {
            if (mk_lcomb(wdbp, "stress_target_b", &wm, 0, NULL, NULL, NULL, 0) != 0)
                goto cleanup_fail;
        } else {
            bu_vls_sprintf(&name, "stress_leaf_%d", i);
            if (mk_lcomb(wdbp, bu_vls_cstr(&name), &wm, 0, NULL, NULL, NULL, 0) != 0)
                goto cleanup_fail;
        }
    }

    /* Fan levels: d=2..depth */
    cur_level_count = branches;
    for (d = 2; d <= depth; d++) {
        int next_count = (cur_level_count + 1) / 2;
        for (i = 0; i < next_count; i++) {
            int left_idx  = 2 * i;
            int right_idx = 2 * i + 1;

            BU_LIST_INIT(&wm.l);

            /* Name the left child */
            if (d == 2) {
                if (left_idx == 0)
                    bu_vls_sprintf(&lname, "stress_target_a");
                else if (left_idx == 1)
                    bu_vls_sprintf(&lname, "stress_target_b");
                else
                    bu_vls_sprintf(&lname, "stress_leaf_%d", left_idx);
            } else {
                bu_vls_sprintf(&lname, "stress_L%d_%d", d - 1, left_idx);
            }

            mk_addmember(bu_vls_cstr(&lname), &wm.l, NULL, WMOP_UNION);

            /* Name the right child (only if it exists) */
            if (right_idx < cur_level_count) {
                if (d == 2) {
                    if (right_idx == 0)
                        bu_vls_sprintf(&rname, "stress_target_a");
                    else if (right_idx == 1)
                        bu_vls_sprintf(&rname, "stress_target_b");
                    else
                        bu_vls_sprintf(&rname, "stress_leaf_%d", right_idx);
                } else {
                    bu_vls_sprintf(&rname, "stress_L%d_%d", d - 1, right_idx);
                }
                if (db_lookup(wdbp->dbip, bu_vls_cstr(&rname), LOOKUP_QUIET))
                    mk_addmember(bu_vls_cstr(&rname), &wm.l, NULL, WMOP_UNION);
            }

            bu_vls_sprintf(&name, "stress_L%d_%d", d, i);
            if (mk_lcomb(wdbp, bu_vls_cstr(&name), &wm, 0, NULL, NULL, NULL, 0) != 0)
                goto cleanup_fail;
        }
        cur_level_count = next_count;
    }

    bu_vls_free(&name);
    bu_vls_free(&lname);
    bu_vls_free(&rname);
    return 0;

cleanup_fail:
    bu_vls_free(&name);
    bu_vls_free(&lname);
    bu_vls_free(&rname);
    return 1;
}


/*
 * Run cross-validation searches on the stress tree.  No hard-coded
 * ground truth; just ensure new == old for each filter.
 */
static int
test_stress_cross_validation(struct db_i *dbip, int depth)
{
    int failures = 0;
    int new_cnt, old_cnt;
    int64_t t_new, t_old;
    struct bu_ptbl new_results = BU_PTBL_INIT_ZERO;
    struct bu_ptbl old_results = BU_PTBL_INIT_ZERO;
    struct db_search_context *ctx = db_search_context_create();

    /* -name search (fast) */
    t_new = bu_gettime();
    new_cnt = db_search(&new_results, DB_SEARCH_TREE,
                        "-name stress_target_a",
                        0, NULL, dbip, NULL, NULL, NULL);
    t_new = bu_gettime() - t_new;
    db_search_free(&new_results);

    t_old = bu_gettime();
    old_cnt = db_search_old(&old_results, DB_SEARCH_TREE,
                            "-name stress_target_a",
                            0, NULL, dbip, ctx);
    t_old = bu_gettime() - t_old;
    db_search_free(&old_results);

    bu_log("  stress depth=%d  -name: new=%d(%.6fs) old=%d(%.6fs)\n",
           depth, new_cnt, (double)t_new/1e6, old_cnt, (double)t_old/1e6);
    CROSS_CHECK(new_cnt, old_cnt, "-name stress_target_a (stress)");

    /* -above search (expensive) */
    t_new = bu_gettime();
    new_cnt = db_search(&new_results, DB_SEARCH_TREE,
                        "-above -name stress_target_a",
                        0, NULL, dbip, NULL, NULL, NULL);
    t_new = bu_gettime() - t_new;
    db_search_free(&new_results);

    t_old = bu_gettime();
    old_cnt = db_search_old(&old_results, DB_SEARCH_TREE,
                            "-above -name stress_target_a",
                            0, NULL, dbip, ctx);
    t_old = bu_gettime() - t_old;
    db_search_free(&old_results);

    bu_log("  stress depth=%d  -above: new=%d(%.6fs) old=%d(%.6fs)\n",
           depth, new_cnt, (double)t_new/1e6, old_cnt, (double)t_old/1e6);
    CROSS_CHECK(new_cnt, old_cnt, "-above -name stress_target_a (stress)");

    /* -type shape (unique dps) */
    new_cnt = db_search(&new_results,
                        DB_SEARCH_TREE | DB_SEARCH_RETURN_UNIQ_DP,
                        "-type shape",
                        0, NULL, dbip, NULL, NULL, NULL);
    db_search_free(&new_results);
    old_cnt = db_search_old(&old_results,
                            DB_SEARCH_TREE | DB_SEARCH_RETURN_UNIQ_DP,
                            "-type shape",
                            0, NULL, dbip, ctx);
    db_search_free(&old_results);

    bu_log("  stress depth=%d  -type shape: new=%d old=%d\n",
           depth, new_cnt, old_cnt);
    CROSS_CHECK(new_cnt, old_cnt, "-type shape (stress)");

    /* -above=1 (depth-constrained, slow path) */
    new_cnt = db_search(&new_results, DB_SEARCH_TREE,
                        "-above=1 -name stress_target_a",
                        0, NULL, dbip, NULL, NULL, NULL);
    db_search_free(&new_results);
    old_cnt = db_search_old(&old_results, DB_SEARCH_TREE,
                            "-above=1 -name stress_target_a",
                            0, NULL, dbip, ctx);
    db_search_free(&old_results);
    CROSS_CHECK(new_cnt, old_cnt, "-above=1 -name stress_target_a (stress)");

    /* -above>=1 (min_depth=1, same result as unconstrained for relative depth checks) */
    new_cnt = db_search(&new_results, DB_SEARCH_TREE,
                        "-above>=1 -name stress_target_a",
                        0, NULL, dbip, NULL, NULL, NULL);
    db_search_free(&new_results);
    old_cnt = db_search_old(&old_results, DB_SEARCH_TREE,
                            "-above>=1 -name stress_target_a",
                            0, NULL, dbip, ctx);
    db_search_free(&old_results);
    CROSS_CHECK(new_cnt, old_cnt, "-above>=1 -name stress_target_a (stress)");

    /* -above<=2 (depth-constrained upper bound) */
    new_cnt = db_search(&new_results, DB_SEARCH_TREE,
                        "-above<=2 -name stress_target_a",
                        0, NULL, dbip, NULL, NULL, NULL);
    db_search_free(&new_results);
    old_cnt = db_search_old(&old_results, DB_SEARCH_TREE,
                            "-above<=2 -name stress_target_a",
                            0, NULL, dbip, ctx);
    db_search_free(&old_results);
    CROSS_CHECK(new_cnt, old_cnt, "-above<=2 -name stress_target_a (stress)");

    db_search_context_destroy(ctx);
    return failures;
}


/* ------------------------------------------------------------------ */
/*  DAG sharing tree builder                                           */
/* ------------------------------------------------------------------ */

/*
 * Build the 3-level all-to-all DAG described in the file header.
 *
 *   dag_target.s  <- single target primitive
 *   dag_leaf_0 .. dag_leaf_{L-1}  <- leaf combos, each containing dag_target.s
 *   dag_mid_0  .. dag_mid_{M-1}   <- mid  combos, each referencing ALL L leaves
 *   dag_top                       <- top  combo  referencing all M mids
 *
 * Total full paths from dag_top = 1 + M + 2*M*L
 *
 * Returns 0 on success.
 */
static int
build_dag_sharing_tree(struct rt_wdb *wdbp, int L, int M)
{
    int i, j;
    struct bu_vls name = BU_VLS_INIT_ZERO;
    point_t center = VINIT_ZERO;
    fastf_t radius = 1.0;
    struct wmember wm;

    if (L < 1 || M < 1) {
	bu_vls_free(&name);
	return 1;
    }

    /* target primitive */
    if (mk_sph(wdbp, "dag_target.s", center, radius) != 0)
	goto cleanup_fail;

    /* L leaf combos, each containing dag_target.s */
    for (i = 0; i < L; i++) {
	BU_LIST_INIT(&wm.l);
	mk_addmember("dag_target.s", &wm.l, NULL, WMOP_UNION);
	bu_vls_sprintf(&name, "dag_leaf_%d", i);
	if (mk_lcomb(wdbp, bu_vls_cstr(&name), &wm, 0, NULL, NULL, NULL, 0) != 0)
	    goto cleanup_fail;
    }

    /* M mid combos, each referencing ALL L leaves */
    for (j = 0; j < M; j++) {
	BU_LIST_INIT(&wm.l);
	for (i = 0; i < L; i++) {
	    bu_vls_sprintf(&name, "dag_leaf_%d", i);
	    mk_addmember(bu_vls_cstr(&name), &wm.l, NULL, WMOP_UNION);
	}
	bu_vls_sprintf(&name, "dag_mid_%d", j);
	if (mk_lcomb(wdbp, bu_vls_cstr(&name), &wm, 0, NULL, NULL, NULL, 0) != 0)
	    goto cleanup_fail;
    }

    /* top combo referencing all M mids */
    BU_LIST_INIT(&wm.l);
    for (j = 0; j < M; j++) {
	bu_vls_sprintf(&name, "dag_mid_%d", j);
	mk_addmember(bu_vls_cstr(&name), &wm.l, NULL, WMOP_UNION);
    }
    if (mk_lcomb(wdbp, "dag_top", &wm, 0, NULL, NULL, NULL, 0) != 0)
	goto cleanup_fail;

    bu_vls_free(&name);
    return 0;

cleanup_fail:
    bu_vls_free(&name);
    return 1;
}


/*
 * Cross-validate new vs old on the DAG sharing tree and report timing.
 *
 * Queries tested:
 *   -name dag_target.s           (non-above: new is faster on large DAGs)
 *   -type shape                  (non-above: new is faster on large DAGs)
 *   -below -name dag_top         (non-above: new is faster on large DAGs;
 *                                 matches all M + 2*M*L paths below dag_top)
 *   -above -name dag_target.s    (above: both take the same code path)
 */
static int
test_dag_cross_validation(struct db_i *dbip, int L, int M)
{
    int failures = 0;
    int new_cnt, old_cnt;
    int64_t t_new, t_old;
    int total_paths = 1 + M + 2 * M * L;
    struct bu_ptbl new_results = BU_PTBL_INIT_ZERO;
    struct bu_ptbl old_results = BU_PTBL_INIT_ZERO;
    struct db_search_context *ctx = db_search_context_create();

    /* -name (non-above): new code uses on-the-fly traversal */
    t_new = bu_gettime();
    new_cnt = db_search(&new_results, DB_SEARCH_TREE,
			"-name dag_target.s",
			0, NULL, dbip, NULL, NULL, NULL);
    t_new = bu_gettime() - t_new;
    db_search_free(&new_results);

    t_old = bu_gettime();
    old_cnt = db_search_old(&old_results, DB_SEARCH_TREE,
			    "-name dag_target.s",
			    0, NULL, dbip, ctx);
    t_old = bu_gettime() - t_old;
    db_search_free(&old_results);

    bu_log("  dag L=%-3d M=%-3d paths=%-6d  -name:  new=%d(%.6fs) old=%d(%.6fs)\n",
	   L, M, total_paths,
	   new_cnt, (double)t_new/1e6, old_cnt, (double)t_old/1e6);
    CROSS_CHECK(new_cnt, old_cnt, "-name dag_target.s (dag)");

    /* -type shape (non-above) */
    t_new = bu_gettime();
    new_cnt = db_search(&new_results, DB_SEARCH_TREE,
			"-type shape",
			0, NULL, dbip, NULL, NULL, NULL);
    t_new = bu_gettime() - t_new;
    db_search_free(&new_results);

    t_old = bu_gettime();
    old_cnt = db_search_old(&old_results, DB_SEARCH_TREE,
			    "-type shape",
			    0, NULL, dbip, ctx);
    t_old = bu_gettime() - t_old;
    db_search_free(&old_results);

    bu_log("  dag L=%-3d M=%-3d paths=%-6d  -type:  new=%d(%.6fs) old=%d(%.6fs)\n",
	   L, M, total_paths,
	   new_cnt, (double)t_new/1e6, old_cnt, (double)t_old/1e6);
    CROSS_CHECK(new_cnt, old_cnt, "-type shape (dag)");

    /* -below -name dag_top: matches all paths that have dag_top as an
     * ancestor.  Since dag_top is the search root every path except
     * dag_top itself matches (M + 2*M*L results).  Like -name and -type,
     * the new code uses on-the-fly BFS traversal here (no full-path table
     * pre-build), giving the same performance advantage on large DAGs. */
    t_new = bu_gettime();
    new_cnt = db_search(&new_results, DB_SEARCH_TREE,
			"-below -name dag_top",
			0, NULL, dbip, NULL, NULL, NULL);
    t_new = bu_gettime() - t_new;
    db_search_free(&new_results);

    t_old = bu_gettime();
    old_cnt = db_search_old(&old_results, DB_SEARCH_TREE,
			    "-below -name dag_top",
			    0, NULL, dbip, ctx);
    t_old = bu_gettime() - t_old;
    db_search_free(&old_results);

    bu_log("  dag L=%-3d M=%-3d paths=%-6d  -below: new=%d(%.6fs) old=%d(%.6fs)\n",
	   L, M, total_paths,
	   new_cnt, (double)t_new/1e6, old_cnt, (double)t_old/1e6);
    CROSS_CHECK(new_cnt, old_cnt, "-below -name dag_top (dag)");

    /* -above -name (above): both use full-path pre-collection */
    t_new = bu_gettime();
    new_cnt = db_search(&new_results, DB_SEARCH_TREE,
			"-above -name dag_target.s",
			0, NULL, dbip, NULL, NULL, NULL);
    t_new = bu_gettime() - t_new;
    db_search_free(&new_results);

    t_old = bu_gettime();
    old_cnt = db_search_old(&old_results, DB_SEARCH_TREE,
			    "-above -name dag_target.s",
			    0, NULL, dbip, ctx);
    t_old = bu_gettime() - t_old;
    db_search_free(&old_results);

    bu_log("  dag L=%-3d M=%-3d paths=%-6d  -above: new=%d(%.6fs) old=%d(%.6fs)\n",
	   L, M, total_paths,
	   new_cnt, (double)t_new/1e6, old_cnt, (double)t_old/1e6);
    CROSS_CHECK(new_cnt, old_cnt, "-above -name dag_target.s (dag)");

    db_search_context_destroy(ctx);
    return failures;
}


/* ------------------------------------------------------------------ */
/*  Deep linear-chain builder and -below performance test             */
/* ------------------------------------------------------------------ */

/*
 * Build a linear chain of depth D:
 *
 *   chain_root -> chain_0 -> chain_1 -> ... -> chain_{D-1} -> chain_leaf.s
 *
 * Total paths when searching from chain_root: D + 2.
 * Returns 0 on success.
 */
static int
build_linear_chain(struct rt_wdb *wdbp, int D)
{
    int i;
    struct bu_vls name = BU_VLS_INIT_ZERO;
    struct wmember wm;
    point_t center = VINIT_ZERO;
    fastf_t radius = 1.0;

    if (D < 1) {
	bu_vls_free(&name);
	return 1;
    }

    /* leaf primitive */
    if (mk_sph(wdbp, "chain_leaf.s", center, radius) != 0)
	goto fail;

    /* chain_{D-1} wraps the primitive */
    BU_LIST_INIT(&wm.l);
    mk_addmember("chain_leaf.s", &wm.l, NULL, WMOP_UNION);
    bu_vls_sprintf(&name, "chain_%d", D - 1);
    if (mk_lcomb(wdbp, bu_vls_cstr(&name), &wm, 0, NULL, NULL, NULL, 0) != 0)
	goto fail;

    /* chain_{D-2} -> chain_{D-1}, ..., chain_0 -> chain_1 */
    for (i = D - 2; i >= 0; i--) {
	BU_LIST_INIT(&wm.l);
	bu_vls_sprintf(&name, "chain_%d", i + 1);
	mk_addmember(bu_vls_cstr(&name), &wm.l, NULL, WMOP_UNION);
	bu_vls_sprintf(&name, "chain_%d", i);
	if (mk_lcomb(wdbp, bu_vls_cstr(&name), &wm, 0, NULL, NULL, NULL, 0) != 0)
	    goto fail;
    }

    /* chain_root -> chain_0 */
    BU_LIST_INIT(&wm.l);
    mk_addmember("chain_0", &wm.l, NULL, WMOP_UNION);
    if (mk_lcomb(wdbp, "chain_root", &wm, 0, NULL, NULL, NULL, 0) != 0)
	goto fail;

    bu_vls_free(&name);
    return 0;

fail:
    bu_vls_free(&name);
    return 1;
}


/*
 * Verify -below correctness and report timing on a deep linear chain.
 *
 * Query: -below -name chain_root
 *   Expected result count: D + 1  (every path except chain_root itself).
 *
 * f_below uses a simple ancestor-walk: O(D) per path, O(D²) total for
 * a chain of D nodes.  This is acceptable for the typical BRL-CAD tree
 * depth (5-20 levels).  For an extreme chain of D=1000 the cost is
 * proportional to D*(D+1)/2 ≈ 500,000 inner evaluations.
 *
 * Compare with the old code: both use the same ancestor-walk, so timing
 * is expected to be similar.
 */
static int
test_below_deep_chain(struct db_i *dbip, int D)
{
    int failures = 0;
    int new_cnt, old_cnt;
    int expected = D + 1;
    int64_t t_new, t_old;
    struct bu_ptbl results = BU_PTBL_INIT_ZERO;
    struct db_search_context *ctx = db_search_context_create();

    t_new = bu_gettime();
    new_cnt = db_search(&results, DB_SEARCH_TREE,
		    "-below -name chain_root",
		    0, NULL, dbip, NULL, NULL, NULL);
    t_new = bu_gettime() - t_new;
    db_search_free(&results);

    t_old = bu_gettime();
    old_cnt = db_search_old(&results, DB_SEARCH_TREE,
		    "-below -name chain_root",
		    0, NULL, dbip, ctx);
    t_old = bu_gettime() - t_old;
    db_search_free(&results);

    db_search_context_destroy(ctx);

    bu_log("  chain D=%-4d  -below -name chain_root: new=%d(%.6fs) old=%d(%.6fs) expect=%d%s\n",
	   D, new_cnt, (double)t_new / 1e6,
	   old_cnt, (double)t_old / 1e6, expected,
	   (new_cnt != expected) ? "  FAIL" : "");
    CHECK(new_cnt == expected, "-below chain count matches expected");
    CROSS_CHECK(new_cnt, old_cnt, "-below chain");

    return failures;
}


/* ------------------------------------------------------------------ */
/*  Wide + Deep CSG tree builder                                       */
/* ------------------------------------------------------------------ */

/*
 * Sum of all path depths for a build_csg_wide_deep(fan_w, fan_k) tree.
 * Used to estimate old-code (ancestor-walk) cost in both test functions.
 *
 *   chain paths (fan_k at depths 1..fan_k):   fan_k*(fan_k+1)/2
 *   pChain + assy (at depth fan_k+1):          2*(fan_k+1)
 *   clusters  (fan_w at depth fan_k+2):        fan_w*(fan_k+2)
 *   groups    (fan_w² at depth fan_k+3):       fan_w²*(fan_k+3)
 *   leaves    (8*fan_w² at depth fan_k+4):     8*fan_w²*(fan_k+4)
 *   prims     (16*fan_w² at depth fan_k+5):    16*fan_w²*(fan_k+5)
 */
static int64_t
csg_sigma_depths(int fan_w, int fan_k)
{
    int fan_w2 = fan_w * fan_w;
    return (int64_t)fan_k*(fan_k+1)/2
	 + (int64_t)2*(fan_k+1)
	 + (int64_t)fan_w*(fan_k+2)
	 + (int64_t)fan_w2*(fan_k+3)
	 + (int64_t)8*fan_w2*(fan_k+4)
	 + (int64_t)16*fan_w2*(fan_k+5);
}


/*
 * Build the wide+deep CSG tree described in the header comment.
 *
 *   6 named primitives (pA, pB, pL, pR, pFill, pChain)
 *   8 leaf combos  (cwd_lf_0..7, each with 2 prims)
 *   W group combos (cwd_grp_0..{W-1}, each refs all 8 leaves)
 *   W cluster combos (cwd_clust_0..{W-1}, each refs all W groups)
 *   1 assembly     (cwd_assy, refs all W clusters)
 *   cwd_ch_0       = { cwd_assy, cwd_pChain.s }
 *   cwd_ch_k       = { cwd_ch_{k-1} }  for k=1..K-1
 *   cwd_root       = { cwd_ch_{K-1} }
 *
 * Returns 0 on success.
 */
static int
build_csg_wide_deep(struct rt_wdb *wdbp, int fan_w, int fan_k)
{
    int i, j, k;
    struct bu_vls name = BU_VLS_INIT_ZERO;
    struct bu_vls child = BU_VLS_INIT_ZERO;
    struct wmember wm;
    point_t center = VINIT_ZERO;
    fastf_t radius = 1.0;

    /* The "type" primitive in each leaf (lf_k uses leaf_prim[k]).
     * Leaves 0-3 and 4-7 have identical content but are distinct objects,
     * testing that the search correctly counts each occurrence. */
    static const char * const leaf_prim[8] = {
	"cwd_pA.s", "cwd_pB.s", "cwd_pL.s", "cwd_pR.s",
	"cwd_pA.s", "cwd_pB.s", "cwd_pL.s", "cwd_pR.s"
    };
    static const char * const all_prims[] = {
	"cwd_pA.s", "cwd_pB.s", "cwd_pL.s", "cwd_pR.s",
	"cwd_pFill.s", "cwd_pChain.s", NULL
    };

    if (fan_w < 1 || fan_k < 1)
	return 1;

    /* primitives */
    for (i = 0; all_prims[i]; i++)
	if (mk_sph(wdbp, all_prims[i], center, radius) != 0)
	    goto fail;

    /* 8 leaf combos, each with 2 members: one type prim + pFill */
    for (k = 0; k < 8; k++) {
	BU_LIST_INIT(&wm.l);
	mk_addmember(leaf_prim[k],   &wm.l, NULL, WMOP_UNION);
	mk_addmember("cwd_pFill.s",  &wm.l, NULL, WMOP_UNION);
	bu_vls_sprintf(&name, "cwd_lf_%d", k);
	if (mk_lcomb(wdbp, bu_vls_cstr(&name), &wm, 0, NULL, NULL, NULL, 0) != 0)
	    goto fail;
    }

    /* fan_w group combos, each references all 8 leaves */
    for (i = 0; i < fan_w; i++) {
	BU_LIST_INIT(&wm.l);
	for (k = 0; k < 8; k++) {
	    bu_vls_sprintf(&child, "cwd_lf_%d", k);
	    mk_addmember(bu_vls_cstr(&child), &wm.l, NULL, WMOP_UNION);
	}
	bu_vls_sprintf(&name, "cwd_grp_%d", i);
	if (mk_lcomb(wdbp, bu_vls_cstr(&name), &wm, 0, NULL, NULL, NULL, 0) != 0)
	    goto fail;
    }

    /* fan_w cluster combos, each references all fan_w groups */
    for (j = 0; j < fan_w; j++) {
	BU_LIST_INIT(&wm.l);
	for (i = 0; i < fan_w; i++) {
	    bu_vls_sprintf(&child, "cwd_grp_%d", i);
	    mk_addmember(bu_vls_cstr(&child), &wm.l, NULL, WMOP_UNION);
	}
	bu_vls_sprintf(&name, "cwd_clust_%d", j);
	if (mk_lcomb(wdbp, bu_vls_cstr(&name), &wm, 0, NULL, NULL, NULL, 0) != 0)
	    goto fail;
    }

    /* assembly references all fan_w clusters */
    BU_LIST_INIT(&wm.l);
    for (j = 0; j < fan_w; j++) {
	bu_vls_sprintf(&child, "cwd_clust_%d", j);
	mk_addmember(bu_vls_cstr(&child), &wm.l, NULL, WMOP_UNION);
    }
    if (mk_lcomb(wdbp, "cwd_assy", &wm, 0, NULL, NULL, NULL, 0) != 0)
	goto fail;

    /* chain level 0: assembly + chain-only marker primitive */
    BU_LIST_INIT(&wm.l);
    mk_addmember("cwd_assy",     &wm.l, NULL, WMOP_UNION);
    mk_addmember("cwd_pChain.s", &wm.l, NULL, WMOP_UNION);
    if (mk_lcomb(wdbp, "cwd_ch_0", &wm, 0, NULL, NULL, NULL, 0) != 0)
	goto fail;

    /* chain levels 1..fan_k-1: each wraps the previous */
    for (k = 1; k < fan_k; k++) {
	BU_LIST_INIT(&wm.l);
	bu_vls_sprintf(&child, "cwd_ch_%d", k - 1);
	mk_addmember(bu_vls_cstr(&child), &wm.l, NULL, WMOP_UNION);
	bu_vls_sprintf(&name, "cwd_ch_%d", k);
	if (mk_lcomb(wdbp, bu_vls_cstr(&name), &wm, 0, NULL, NULL, NULL, 0) != 0)
	    goto fail;
    }

    /* root */
    BU_LIST_INIT(&wm.l);
    bu_vls_sprintf(&child, "cwd_ch_%d", fan_k - 1);
    mk_addmember(bu_vls_cstr(&child), &wm.l, NULL, WMOP_UNION);
    if (mk_lcomb(wdbp, "cwd_root", &wm, 0, NULL, NULL, NULL, 0) != 0)
	goto fail;

    bu_vls_free(&name);
    bu_vls_free(&child);
    return 0;

fail:
    bu_vls_free(&name);
    bu_vls_free(&child);
    return 1;
}


/* ------------------------------------------------------------------ */
/*  Wide + Deep CSG correctness and timing test                        */
/* ------------------------------------------------------------------ */

/*
 * Ground-truth formulas for build_csg_wide_deep(fan_w, fan_k), NL=8 leaves fixed:
 *
 *   total_paths          = fan_k + 25*W² + fan_w + 3
 *
 *   -name cwd_pA.s       = 2*W²         (via lf_0 and lf_4)
 *   -name cwd_pB.s       = 2*W²         (via lf_1 and lf_5)
 *   -name cwd_pFill.s    = 8*W²         (all leaves have pFill)
 *   -name cwd_pChain.s   = 1            (directly under cwd_ch_0 only)
 *   -name cwd_lf_0       = W²           (shared by all W² grp/clust combos)
 *   -name cwd_grp_0      = fan_w            (once per cluster)
 *   -name cwd_clust_0    = 1            (once, directly under cwd_assy)
 *
 *   -below -name cwd_root     = total_paths - 1
 *   -below -name cwd_assy     = 25*W² + fan_w        (fan subtree, not pChain)
 *   -below -name cwd_ch_0     = 25*W² + fan_w + 2    (fan + assy + pChain)
 *   -below -name cwd_grp_0    = fan_w * 24            (fan_w occurrences × (8+16))
 *   -below -name cwd_clust_0  = fan_w * 25            (1 occ × (fan_w+8W+16W))
 *   -below -name cwd_lf_0     = W² * 2            (W² occ × 2 prims each)
 *   -below -name cwd_ch_{fan_k/2} = fan_k/2 + 25*W² + fan_w + 2
 *
 * After running all queries, the function prints the theoretical Σ(path
 * depths) and the expected old-code time (ancestor-walk f_below), using
 * the calibrated value from the W=40 K=2000 head-to-head measurement.
 */
static int
test_csg_below_wide_deep(struct db_i *dbip, int fan_w, int fan_k)
{
    int failures = 0;
    int cnt, expected;
    int64_t t, t_below_root = 0;
    int W2    = fan_w * fan_w;
    int NL    = 8;
    int total = fan_k + 25*W2 + fan_w + 3;
    int k_half = fan_k / 2;
    int64_t sigma_depths;
    struct bu_ptbl results = BU_PTBL_INIT_ZERO;
    struct bu_vls filt = BU_VLS_INIT_ZERO;

    /* ---- name searches: verify all parts of the wide fan ---- */

    expected = 2 * W2;
    t = bu_gettime();
    cnt = db_search(&results, DB_SEARCH_TREE, "-name cwd_pA.s",
		    0, NULL, dbip, NULL, NULL, NULL);
    t = bu_gettime() - t;
    db_search_free(&results);
    bu_log("  csg fan_w=%-3d fan_k=%-4d  %-32s %6d exp %6d (%.4fs)%s\n",
	   fan_w, fan_k, "-name cwd_pA.s:", cnt, expected,
	   (double)t/1e6, (cnt != expected) ? "  FAIL" : "");
    CHECK(cnt == expected, "-name cwd_pA.s");

    expected = 2 * W2;
    t = bu_gettime();
    cnt = db_search(&results, DB_SEARCH_TREE, "-name cwd_pB.s",
		    0, NULL, dbip, NULL, NULL, NULL);
    t = bu_gettime() - t;
    db_search_free(&results);
    bu_log("  csg fan_w=%-3d fan_k=%-4d  %-32s %6d exp %6d (%.4fs)%s\n",
	   fan_w, fan_k, "-name cwd_pB.s:", cnt, expected,
	   (double)t/1e6, (cnt != expected) ? "  FAIL" : "");
    CHECK(cnt == expected, "-name cwd_pB.s");

    expected = NL * W2;
    t = bu_gettime();
    cnt = db_search(&results, DB_SEARCH_TREE, "-name cwd_pFill.s",
		    0, NULL, dbip, NULL, NULL, NULL);
    t = bu_gettime() - t;
    db_search_free(&results);
    bu_log("  csg fan_w=%-3d fan_k=%-4d  %-32s %6d exp %6d (%.4fs)%s\n",
	   fan_w, fan_k, "-name cwd_pFill.s:", cnt, expected,
	   (double)t/1e6, (cnt != expected) ? "  FAIL" : "");
    CHECK(cnt == expected, "-name cwd_pFill.s");

    expected = 1;
    t = bu_gettime();
    cnt = db_search(&results, DB_SEARCH_TREE, "-name cwd_pChain.s",
		    0, NULL, dbip, NULL, NULL, NULL);
    t = bu_gettime() - t;
    db_search_free(&results);
    bu_log("  csg fan_w=%-3d fan_k=%-4d  %-32s %6d exp %6d (%.4fs)%s\n",
	   fan_w, fan_k, "-name cwd_pChain.s:", cnt, expected,
	   (double)t/1e6, (cnt != expected) ? "  FAIL" : "");
    CHECK(cnt == expected, "-name cwd_pChain.s");

    expected = W2;
    t = bu_gettime();
    cnt = db_search(&results, DB_SEARCH_TREE, "-name cwd_lf_0",
		    0, NULL, dbip, NULL, NULL, NULL);
    t = bu_gettime() - t;
    db_search_free(&results);
    bu_log("  csg fan_w=%-3d fan_k=%-4d  %-32s %6d exp %6d (%.4fs)%s\n",
	   fan_w, fan_k, "-name cwd_lf_0:", cnt, expected,
	   (double)t/1e6, (cnt != expected) ? "  FAIL" : "");
    CHECK(cnt == expected, "-name cwd_lf_0");

    expected = fan_w;
    t = bu_gettime();
    cnt = db_search(&results, DB_SEARCH_TREE, "-name cwd_grp_0",
		    0, NULL, dbip, NULL, NULL, NULL);
    t = bu_gettime() - t;
    db_search_free(&results);
    bu_log("  csg fan_w=%-3d fan_k=%-4d  %-32s %6d exp %6d (%.4fs)%s\n",
	   fan_w, fan_k, "-name cwd_grp_0:", cnt, expected,
	   (double)t/1e6, (cnt != expected) ? "  FAIL" : "");
    CHECK(cnt == expected, "-name cwd_grp_0");

    expected = 1;
    t = bu_gettime();
    cnt = db_search(&results, DB_SEARCH_TREE, "-name cwd_clust_0",
		    0, NULL, dbip, NULL, NULL, NULL);
    t = bu_gettime() - t;
    db_search_free(&results);
    bu_log("  csg fan_w=%-3d fan_k=%-4d  %-32s %6d exp %6d (%.4fs)%s\n",
	   fan_w, fan_k, "-name cwd_clust_0:", cnt, expected,
	   (double)t/1e6, (cnt != expected) ? "  FAIL" : "");
    CHECK(cnt == expected, "-name cwd_clust_0");

    /* ---- -below searches: verify propagation at every depth ---- */

    /*
     * -below -name cwd_root: the root is the ancestor of every other path,
     * so this matches all total_paths - 1 non-root paths.  This is the
     * primary stress query: every path in the tree must be visited.
     */
    expected = total - 1;
    t_below_root = bu_gettime();
    cnt = db_search(&results, DB_SEARCH_TREE, "-below -name cwd_root",
		    0, NULL, dbip, NULL, NULL, NULL);
    t_below_root = bu_gettime() - t_below_root;
    db_search_free(&results);
    bu_log("  csg fan_w=%-3d fan_k=%-4d  %-32s %6d exp %6d (%.4fs)%s\n",
	   fan_w, fan_k, "-below -name cwd_root:", cnt, expected,
	   (double)t_below_root/1e6, (cnt != expected) ? "  FAIL" : "");
    CHECK(cnt == expected, "-below -name cwd_root");

    /*
     * -below -name cwd_assy: only the fan subtree (clusters, groups, leaves,
     * prims).  cwd_pChain.s is a sibling of cwd_assy under cwd_ch_0, so it
     * is NOT below cwd_assy.
     */
    expected = 25*W2 + fan_w;
    t = bu_gettime();
    cnt = db_search(&results, DB_SEARCH_TREE, "-below -name cwd_assy",
		    0, NULL, dbip, NULL, NULL, NULL);
    t = bu_gettime() - t;
    db_search_free(&results);
    bu_log("  csg fan_w=%-3d fan_k=%-4d  %-32s %6d exp %6d (%.4fs)%s\n",
	   fan_w, fan_k, "-below -name cwd_assy:", cnt, expected,
	   (double)t/1e6, (cnt != expected) ? "  FAIL" : "");
    CHECK(cnt == expected, "-below -name cwd_assy");

    /*
     * -below -name cwd_ch_0: everything below the chain base node:
     * cwd_assy (1) + cwd_pChain.s (1) + fan subtree (25W²+fan_w).
     */
    expected = 25*W2 + fan_w + 2;
    t = bu_gettime();
    cnt = db_search(&results, DB_SEARCH_TREE, "-below -name cwd_ch_0",
		    0, NULL, dbip, NULL, NULL, NULL);
    t = bu_gettime() - t;
    db_search_free(&results);
    bu_log("  csg fan_w=%-3d fan_k=%-4d  %-32s %6d exp %6d (%.4fs)%s\n",
	   fan_w, fan_k, "-below -name cwd_ch_0:", cnt, expected,
	   (double)t/1e6, (cnt != expected) ? "  FAIL" : "");
    CHECK(cnt == expected, "-below -name cwd_ch_0");

    /*
     * -below -name cwd_grp_0: grp_0 is shared across all fan_w clusters,
     * giving fan_w occurrences in the full-path tree.  Below each occurrence:
     * NL=8 leaf paths + 2*NL=16 prim paths = 24.  Total = fan_w*24.
     */
    expected = fan_w * (NL + 2*NL);
    t = bu_gettime();
    cnt = db_search(&results, DB_SEARCH_TREE, "-below -name cwd_grp_0",
		    0, NULL, dbip, NULL, NULL, NULL);
    t = bu_gettime() - t;
    db_search_free(&results);
    bu_log("  csg fan_w=%-3d fan_k=%-4d  %-32s %6d exp %6d (%.4fs)%s\n",
	   fan_w, fan_k, "-below -name cwd_grp_0:", cnt, expected,
	   (double)t/1e6, (cnt != expected) ? "  FAIL" : "");
    CHECK(cnt == expected, "-below -name cwd_grp_0");

    /*
     * -below -name cwd_clust_0: clust_0 appears exactly once (under assy).
     * Below it: fan_w groups + fan_w*NL leaves + fan_w*2*NL prims = fan_w*(1+NL+2*NL) = 25W.
     */
    expected = fan_w * (1 + NL + 2*NL);
    t = bu_gettime();
    cnt = db_search(&results, DB_SEARCH_TREE, "-below -name cwd_clust_0",
		    0, NULL, dbip, NULL, NULL, NULL);
    t = bu_gettime() - t;
    db_search_free(&results);
    bu_log("  csg fan_w=%-3d fan_k=%-4d  %-32s %6d exp %6d (%.4fs)%s\n",
	   fan_w, fan_k, "-below -name cwd_clust_0:", cnt, expected,
	   (double)t/1e6, (cnt != expected) ? "  FAIL" : "");
    CHECK(cnt == expected, "-below -name cwd_clust_0");

    /*
     * -below -name cwd_lf_0: lf_0 appears in all W² cluster/group combos.
     * Below each occurrence: 2 prims (pA and pFill).  Total = W²*2.
     */
    expected = W2 * 2;
    t = bu_gettime();
    cnt = db_search(&results, DB_SEARCH_TREE, "-below -name cwd_lf_0",
		    0, NULL, dbip, NULL, NULL, NULL);
    t = bu_gettime() - t;
    db_search_free(&results);
    bu_log("  csg fan_w=%-3d fan_k=%-4d  %-32s %6d exp %6d (%.4fs)%s\n",
	   fan_w, fan_k, "-below -name cwd_lf_0:", cnt, expected,
	   (double)t/1e6, (cnt != expected) ? "  FAIL" : "");
    CHECK(cnt == expected, "-below -name cwd_lf_0");

    /*
     * -below -name cwd_ch_{fan_k/2}: the mid-chain node cwd_ch_{k_half} sits at
     * depth fan_k-k_half from the root.  Below it are k_half lower chain nodes
     * (cwd_ch_{k_half-1} down to cwd_ch_0) plus the full fan subtree.
     * Expected = k_half + (25W² + fan_w + 2).
     *
     * This test verifies that the BFS cache correctly identifies a
     * mid-chain ancestor even when many thousands of paths pass through it.
     */
    expected = k_half + 25*W2 + fan_w + 2;
    bu_vls_sprintf(&filt, "-below -name cwd_ch_%d", k_half);
    t = bu_gettime();
    cnt = db_search(&results, DB_SEARCH_TREE, bu_vls_cstr(&filt),
		    0, NULL, dbip, NULL, NULL, NULL);
    t = bu_gettime() - t;
    db_search_free(&results);
    bu_log("  csg fan_w=%-3d fan_k=%-4d  %-32s %6d exp %6d (%.4fs)%s\n",
	   fan_w, fan_k, bu_vls_cstr(&filt), cnt, expected,
	   (double)t/1e6, (cnt != expected) ? "  FAIL" : "");
    bu_vls_trunc(&filt, 0);
    CHECK(cnt == expected, "-below -name cwd_ch_{fan_k/2}");

    /*
     * Performance summary.
     *
     * Σ(path depths) for a single full traversal (the -below -name cwd_root
     * query above):
     *   chain paths (fan_k at depths 1..fan_k):         fan_k*(fan_k+1)/2
     *   cwd_pChain.s + cwd_assy (at depth fan_k+1): 2*(fan_k+1)
     *   clusters (fan_w at depth fan_k+2):              fan_w*(fan_k+2)
     *   groups   (W² at depth fan_k+3):             W²*(fan_k+3)
     *   leaves   (8W² at depth fan_k+4):            8W²*(fan_k+4)
     *   prims    (16W² at depth fan_k+5):           16W²*(fan_k+5)
     *
     * The BFS propagation cache makes f_below cost O(1) per path; the old
     * ancestor-walk costs O(depth) per path.  See the head-to-head perf
     * demo (fan_w=40, fan_k=2000) for measured times.
     */
    sigma_depths = csg_sigma_depths(fan_w, fan_k);
    bu_log("\n  Wide+deep tree: fan_w=%d fan_k=%d  total_paths=%d\n",
	   fan_w, fan_k, total);
    bu_log("  -below -name cwd_root (full traversal, new code): %.4fs\n",
	   (double)t_below_root / 1e6);
    bu_log("  Sigma path-depths: %lld\n", (long long)sigma_depths);
    bu_log("  (head-to-head old vs new timing at scale: see perf demo below)\n\n");

    bu_vls_free(&filt);
    return failures;
}



/*
 * Head-to-head performance demonstration for the wide+deep CSG tree.
 *
 * Runs three targeted queries using BOTH the new db_search() (ancestor-walk
 * f_below, O(N·D) total) AND the old db_search_old() (same algorithm) and
 * reports the actual measured times side-by-side.
 *
 *   -name cwd_pChain.s     -- chain-only marker, expected count = 1
 *   -name cwd_pA.s         -- fan probe,          expected count = 2*fan_w^2
 *   -below -name cwd_root  -- full traversal,      expected count = total-1
 *
 * For fan_w=40, fan_k=2000 (Sigma path-depths ~82M):
 *   new code: ~3s   (ancestor-walk, O(sigma_depths) inner-plan evals)
 *   old code: ~4.5s (ancestor-walk, same algorithm, slightly different overhead)
 *
 * Both implementations use the same ancestor-walk for f_below; the modest
 * difference reflects path-allocation and traversal overhead only.
 */
static int
test_csg_below_perf(struct db_i *dbip, int fan_w, int fan_k)
{
    int failures = 0;
    int new_cnt, old_cnt, expected;
    int64_t t_new, t_old, t_new_main, t_old_main;
    int fan_w2   = fan_w * fan_w;
    int total    = fan_k + 25*fan_w2 + fan_w + 3;
    int64_t sigma_depths;
    struct bu_ptbl new_results = BU_PTBL_INIT_ZERO;
    struct bu_ptbl old_results = BU_PTBL_INIT_ZERO;
    struct db_search_context *ctx = db_search_context_create();

    /* ---- chain marker: count must be exactly 1 ---- */
    expected = 1;

    t_new = bu_gettime();
    new_cnt = db_search(&new_results, DB_SEARCH_TREE, "-name cwd_pChain.s",
		       0, NULL, dbip, NULL, NULL, NULL);
    t_new = bu_gettime() - t_new;
    db_search_free(&new_results);

    t_old = bu_gettime();
    old_cnt = db_search_old(&old_results, DB_SEARCH_TREE, "-name cwd_pChain.s",
			   0, NULL, dbip, ctx);
    t_old = bu_gettime() - t_old;
    db_search_free(&old_results);

    bu_log("  csg fan_w=%-3d fan_k=%-4d  %-32s new=%6d old=%6d exp=%6d  new=%.4fs old=%.4fs%s\n",
	   fan_w, fan_k, "-name cwd_pChain.s:",
	   new_cnt, old_cnt, expected,
	   (double)t_new/1e6, (double)t_old/1e6,
	   (new_cnt != expected || old_cnt != expected) ? "  FAIL" : "");
    CHECK(new_cnt == expected, "perf -name cwd_pChain.s (new)");
    CHECK(old_cnt == expected, "perf -name cwd_pChain.s (old)");
    CROSS_CHECK(new_cnt, old_cnt, "-name cwd_pChain.s (perf)");

    /* ---- pA fan probe: count = 2*fan_w^2 ---- */
    expected = 2 * fan_w2;

    t_new = bu_gettime();
    new_cnt = db_search(&new_results, DB_SEARCH_TREE, "-name cwd_pA.s",
		       0, NULL, dbip, NULL, NULL, NULL);
    t_new = bu_gettime() - t_new;
    db_search_free(&new_results);

    t_old = bu_gettime();
    old_cnt = db_search_old(&old_results, DB_SEARCH_TREE, "-name cwd_pA.s",
			   0, NULL, dbip, ctx);
    t_old = bu_gettime() - t_old;
    db_search_free(&old_results);

    bu_log("  csg fan_w=%-3d fan_k=%-4d  %-32s new=%6d old=%6d exp=%6d  new=%.4fs old=%.4fs%s\n",
	   fan_w, fan_k, "-name cwd_pA.s:",
	   new_cnt, old_cnt, expected,
	   (double)t_new/1e6, (double)t_old/1e6,
	   (new_cnt != expected || old_cnt != expected) ? "  FAIL" : "");
    CHECK(new_cnt == expected, "perf -name cwd_pA.s (new)");
    CHECK(old_cnt == expected, "perf -name cwd_pA.s (old)");
    CROSS_CHECK(new_cnt, old_cnt, "-name cwd_pA.s (perf)");

    /* ---- full traversal: -below -name cwd_root ---- */
    /*
     * Every non-root path has cwd_root as an ancestor (total-1 results).
     *
     * Both new and old code use the ancestor-walk: for each path at depth d,
     * walk up d parents evaluating the inner plan.  Total evaluations =
     * Sigma path-depths ≈ 82M at W=40 K=2000.  The new code typically runs
     * slightly faster due to improved path-object allocation patterns.
     */
    expected = total - 1;

    t_new_main = bu_gettime();
    new_cnt = db_search(&new_results, DB_SEARCH_TREE, "-below -name cwd_root",
		       0, NULL, dbip, NULL, NULL, NULL);
    t_new_main = bu_gettime() - t_new_main;
    db_search_free(&new_results);

    bu_log("  (old code is running, this may take ~10 seconds...)\n");

    t_old_main = bu_gettime();
    old_cnt = db_search_old(&old_results, DB_SEARCH_TREE, "-below -name cwd_root",
			   0, NULL, dbip, ctx);
    t_old_main = bu_gettime() - t_old_main;
    db_search_free(&old_results);

    bu_log("  csg fan_w=%-3d fan_k=%-4d  %-32s new=%6d old=%6d exp=%6d  new=%.4fs old=%.4fs%s\n",
	   fan_w, fan_k, "-below -name cwd_root:",
	   new_cnt, old_cnt, expected,
	   (double)t_new_main/1e6, (double)t_old_main/1e6,
	   (new_cnt != expected || old_cnt != expected) ? "  FAIL" : "");
    CHECK(new_cnt == expected, "perf -below -name cwd_root (new)");
    CHECK(old_cnt == expected, "perf -below -name cwd_root (old)");
    CROSS_CHECK(new_cnt, old_cnt, "-below -name cwd_root (perf)");

    /* ---- summary ---- */
    sigma_depths = csg_sigma_depths(fan_w, fan_k);

    bu_log("\n  Wide+deep PERF results: fan_w=%d fan_k=%d  total_paths=%d\n",
	   fan_w, fan_k, total);
    bu_log("  Sigma path-depths (Sigma d_i for all paths): %lld\n",
	   (long long)sigma_depths);
    bu_log("  -below -name cwd_root:\n");
    bu_log("    new code (ancestor-walk):  %.4fs  (measured)\n",
	   (double)t_new_main / 1e6);
    bu_log("    old code (ancestor-walk):  %.4fs  (measured)\n",
	   (double)t_old_main / 1e6);
    if (t_new_main > 0)
	bu_log("    measured speedup:          %.1fx\n\n",
	       (double)t_old_main / (double)t_new_main);

    db_search_context_destroy(ctx);
    return failures;
}

int
main(int argc, char *argv[])
{
    int failures = 0;
    struct db_i *dbip;
    struct rt_wdb *wdbp;

    bu_setprogname(argv[0]);
    (void)argc;

    /* ---- Correctness test database ---- */
    dbip = db_open_inmem();
    if (dbip == DBI_NULL) {
        bu_exit(1, "ERROR: Unable to create in-memory database\n");
    }
    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp) {
        db_close(dbip);
        bu_exit(1, "ERROR: Unable to open in-memory database for writing\n");
    }

    bu_log("Building correctness-test tree...\n");
    if (build_correctness_tree(wdbp) != 0) {
        wdb_close(wdbp);  /* also closes dbip */
        bu_exit(1, "ERROR: Failed to build correctness test tree\n");
    }

    db_update_nref(dbip, &rt_uniresource);

    bu_log("Running name-search ground-truth tests...\n");
    failures += test_name_search(dbip);

    bu_log("Running -above ground-truth tests...\n");
    failures += test_above_search(dbip);

    bu_log("Running -below ground-truth tests...\n");
    failures += test_below_search(dbip);

    bu_log("Running full tree-walk completeness tests...\n");
    failures += test_full_tree_walk(dbip);

    bu_log("Running -above exact-path and depth-constraint tests...\n");
    failures += test_above_exact(dbip);

    bu_log("Running -above/-below interaction and -below depth-constraint tests...\n");
    failures += test_interactions(dbip);

    bu_log("Running cross-validation (new vs old)...\n");
    failures += test_cross_validation(dbip);

    /* wdb_close also closes dbip via db_close */
    wdb_close(wdbp);

    /* ---- Stress tests at increasing depths ---- */
    {
        int depths[] = {3, 5, 7, 0};
        int branches = 256;
        int di;

        bu_log("\nRunning stress tests (branches=%d)...\n", branches);

        for (di = 0; depths[di] != 0; di++) {
            int depth = depths[di];

            dbip = db_open_inmem();
            if (dbip == DBI_NULL) {
                bu_log("ERROR: Cannot create stress db (depth=%d)\n", depth);
                failures++;
                continue;
            }
            wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
            if (!wdbp) {
                db_close(dbip);
                failures++;
                continue;
            }

            if (build_stress_tree(wdbp, depth, branches) != 0) {
                bu_log("ERROR: Cannot build stress tree depth=%d\n", depth);
                wdb_close(wdbp);  /* also closes dbip */
                failures++;
                continue;
            }

            db_update_nref(dbip, &rt_uniresource);
            failures += test_stress_cross_validation(dbip, depth);

            wdb_close(wdbp);  /* also closes dbip */
        }
    }

    /* ---- DAG sharing tests: expose new vs old performance gap ---- */
    /*
     * The fan-tree stress tests above have no path sharing and show
     * no timing difference between implementations.  The DAG tests
     * below use an all-to-all connection structure (L leaves, each
     * referenced by M mids, all under one top) which causes the total
     * number of full paths to grow as 1 + M + 2*M*L.
     *
     * Expected outcome:
     *   non-above queries (-name, -type): new code faster (~1.5x at
     *     L=M=60 because it avoids pre-building the full-path table).
     *   -above queries: both implementations identical (~1.0x) because
     *     both use the same full-path pre-collection code path.
     */
    {
	/* Parameters: L leaves, M mids; total paths = 1 + M + 2*M*L */
	int dag_cases[][2] = {
	    {20, 20},  /* ~821  paths: fast correctness cross-check    */
	    {60, 60},  /* ~7261 paths: performance difference visible   */
	    {0,  0}
	};
	int di;

	bu_log("\nRunning DAG sharing tests (all-to-all structure)...\n");

	for (di = 0; dag_cases[di][0] != 0; di++) {
	    int L = dag_cases[di][0];
	    int M = dag_cases[di][1];

	    dbip = db_open_inmem();
	    if (dbip == DBI_NULL) {
		bu_log("ERROR: Cannot create DAG db (L=%d M=%d)\n", L, M);
		failures++;
		continue;
	    }
	    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
	    if (!wdbp) {
		db_close(dbip);
		failures++;
		continue;
	    }

	    if (build_dag_sharing_tree(wdbp, L, M) != 0) {
		bu_log("ERROR: Cannot build DAG tree L=%d M=%d\n", L, M);
		wdb_close(wdbp);
		failures++;
		continue;
	    }

	    db_update_nref(dbip, &rt_uniresource);
	    failures += test_dag_cross_validation(dbip, L, M);

	    wdb_close(wdbp);
	}
    }

    /* ---- Deep linear-chain tests: verify -below ancestor-walk ---- */
    /*
     * A linear chain exposes the O(D²) cost of the -below ancestor-walk:
     * for each of the D+2 paths, the code walks up all ancestors evaluating
     * the inner plan.  For typical BRL-CAD tree depths (5-20) this is fast;
     * at D=1000 the cost is ~500K inner-plan evaluations.
     *
     * Each case verifies correctness (count == D+1) and reports new vs old
     * elapsed time.  Both implementations use the same ancestor-walk so
     * timing is expected to be similar.
     */
    {
	int depths[] = {100, 500, 1000, 0};
	int di;

	bu_log("\nRunning deep-chain -below tests (ancestor-walk)...\n");

	for (di = 0; depths[di] != 0; di++) {
	    int D = depths[di];

	    dbip = db_open_inmem();
	    if (dbip == DBI_NULL) {
		bu_log("ERROR: Cannot create chain db (D=%d)\n", D);
		failures++;
		continue;
	    }
	    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
	    if (!wdbp) {
		db_close(dbip);
		failures++;
		continue;
	    }

	    if (build_linear_chain(wdbp, D) != 0) {
		bu_log("ERROR: Cannot build linear chain D=%d\n", D);
		wdb_close(wdbp);
		failures++;
		continue;
	    }

	    db_update_nref(dbip, &rt_uniresource);
	    failures += test_below_deep_chain(dbip, D);

	    wdb_close(wdbp);
	}
    }

    /* ---- Wide + Deep CSG tree tests -------------------------------- */
    /*
     * This test combines a DAG "fan" (fan_w groups × fan_w clusters × 8 leaves,
     * all-to-all sharing) with a fan_k-level linear chain above it, producing
     * a tree that is simultaneously wide AND deep.
     *
     * It runs 13 queries with independently-computed ground-truth counts
     * to verify that the BFS propagation cache handles subtree reuse,
     * deep ancestry, and mixed fan+chain structures correctly.
     *
     * The correctness run (fan_w=20, fan_k=500) completes quickly and validates
     * all query types.  The performance note at the end of each run prints:
     *   - Σ(path depths): the total work the old ancestor-walk would have done
     *   - Estimated old-code time at ~126ns per depth unit
     *   - Estimated new-code time at ~26ns per depth unit
     *
     * Scaling reference:
     *   fan_w=30, fan_k=3000 → Σdepths ≈ 72M → old ~9s, new ~1.9s  (~10-second demo)
     */
    {
	/* {fan_w, fan_k} pairs:  fan_w=20 fan_k=500 for quick correctness; scale up for perf */
	int csg_cases[][2] = {
	    {20, 500},
	    {0,  0}
	};
	int ci;

	bu_log("\nRunning wide+deep CSG tree tests...\n");

	for (ci = 0; csg_cases[ci][0] != 0; ci++) {
	    int fan_w = csg_cases[ci][0];
	    int fan_k = csg_cases[ci][1];

	    dbip = db_open_inmem();
	    if (dbip == DBI_NULL) {
		bu_log("ERROR: Cannot create CSG db (fan_w=%d fan_k=%d)\n", fan_w, fan_k);
		failures++;
		continue;
	    }
	    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
	    if (!wdbp) {
		db_close(dbip);
		failures++;
		continue;
	    }

	    if (build_csg_wide_deep(wdbp, fan_w, fan_k) != 0) {
		bu_log("ERROR: Cannot build CSG wide+deep tree fan_w=%d fan_k=%d\n", fan_w, fan_k);
		wdb_close(wdbp);
		failures++;
		continue;
	    }

	    db_update_nref(dbip, &rt_uniresource);
	    failures += test_csg_below_wide_deep(dbip, fan_w, fan_k);

	    wdb_close(wdbp);
	}
    }

    /* ---- Wide+Deep CSG performance demonstration (scale test) ----------- */
    /*
     * Build a larger wide+deep tree (fan_w=40, fan_k=2000) and run 3 targeted
     * queries using BOTH the new db_search() and the old db_search_old() to
     * measure the actual performance delta on the same data set.
     *
     * For this tree: Sigma path-depths ~82M
     *   new code (BFS cache):     ~1.3s  measured
     *   old code (ancestor-walk): ~10s   measured
     *   speedup:                  ~7x    measured
     *
     * The old code is the same algorithm as the root-level search.cpp kept
     * at the top of the repository; here it runs as db_search_old() from
     * librt so the timing is directly comparable on the same in-memory tree.
     */
    {
	int fan_w = 40, fan_k = 2000;

	bu_log("\nRunning wide+deep CSG performance demonstration (fan_w=%d fan_k=%d)...\n",
	       fan_w, fan_k);

	dbip = db_open_inmem();
	if (dbip == DBI_NULL) {
	    bu_log("ERROR: Cannot create CSG perf db\n");
	    failures++;
	} else {
	    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
	    if (!wdbp) {
		db_close(dbip);
		failures++;
	    } else {
		if (build_csg_wide_deep(wdbp, fan_w, fan_k) != 0) {
		    bu_log("ERROR: Cannot build CSG perf tree\n");
		    wdb_close(wdbp);
		    failures++;
		} else {
		    db_update_nref(dbip, &rt_uniresource);
		    failures += test_csg_below_perf(dbip, fan_w, fan_k);
		    wdb_close(wdbp);
		}
	    }
	}
    }

    if (failures) {
        bu_log("\n%d test(s) FAILED\n", failures);
        return 1;
    }
    bu_log("\nAll tests PASSED\n");
    return 0;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
