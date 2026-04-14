/*                        C L O N E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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
/** @file libged/clone.cpp
 *
 * Deep-copy geometry objects with optional pattern replication.
 *
 * C++17 rewrite of clone.c.  Improvements over the original:
 *
 *  - Per-call name table (no module-level global) for safe re-entrancy.
 *  - bu_opt argument parsing with full long-option support.  Multi-value
 *    options (-t, -a, -b, -m, --center-pat, etc.) use custom
 *    bu_opt_arg_process_t callbacks instead of a hand-rolled pre-scan.
 *  - bu_vls_incr-based unique name generation; handles embedded numbers
 *    at any position, preserves zero-padding, honours -c (second number).
 *  - V4 database support removed (dbupgrade first).
 *  - Three pattern modes in addition to the classic linear copy:
 *      --rect  rectangular grid    (replaces pattern_rect Tcl proc)
 *      --sph   spherical pattern   (replaces pattern_sph  Tcl proc)
 *      --cyl   cylindrical pattern (replaces pattern_cyl  Tcl proc)
 *  - Three depth modes (--depth top|regions|primitives):
 *      top        create wrapper comb {obj [matrix]}  (default for patterns)
 *      regions    copy combs down to regions; share primitives; apply matrix
 *                 at region leaf arcs (equiv. Tcl copy_obj + apply_mat -regions)
 *      primitives full deep copy, matrix baked into each solid (linear default)
 *  - --xpush flag: flatten instance matrices before cloning, replacing the
 *    xclone.tcl workaround.
 *  - -g / --group: collect all top-level clones into a named combination.
 *  - Multiple source objects accepted in pattern modes.
 *  - Double-free bug in v5 combination copy fixed.
 *  - Combination-name suffix preserved when number at start of name.
 *
 * NOTE: PatternMode::SPHERICAL is used instead of SPH to avoid the
 * SPH macro defined in rt/db4.h.
 */

#include "common.h"

#include <cmath>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "bu/opt.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "bn/mat.h"
#include "vmath.h"
#include "rt/db4.h"
#include "raytrace.h"

#include "../ged_private.h"


/* -----------------------------------------------------------------------
 * Core types
 * ----------------------------------------------------------------------- */

/** Maps each source name to its per-copy destination name list. */
typedef std::map<std::string, std::vector<std::string>> NameMap;

/* NOTE: avoid using SPH — it is #defined as 11 in rt/db4.h */
enum class PatternMode { LINEAR, RECT, SPHERICAL, CYL };
enum class DepthMode   {
    TOP,        /**< lightweight wrapper comb {obj [mat]} — geometry shared  */
    REGIONS,    /**< copy combs down to regions; share primitives; mat at    */
                /**< region leaf arcs (equiv. to Tcl copy_obj + apply_mat)  */
    PRIMITIVES  /**< full deep copy; mat baked into each primitive           */
};


/** All state for one clone invocation. */
struct CloneState {
    struct ged *gedp = nullptr;

    /* Source objects */
    std::vector<struct directory *> srcs;

    /* ---- Linear-mode transform ---- */
    int     incr     = 100;
    size_t  n_copies = 1;
    hvect_t trans    = HINIT_ZERO;
    hvect_t rot      = HINIT_ZERO;
    hvect_t rpnt     = HINIT_ZERO;
    int     miraxis  = W;
    fastf_t mirpos   = 0.0;
    long    updpos   = 0;           /**< incremented by -c */
    bool    do_xpush = false;

    /**
     * When non-NULL, copy_v5_solid uses this 4x4 matrix directly instead
     * of building one from trans/rot/rpnt.  Used by pattern primitives-depth
     * mode to avoid lossy Euler-angle decomposition.
     */
    const fastf_t *override_matrix = nullptr;

    /* ---- Pattern mode ---- */
    PatternMode pattern = PatternMode::LINEAR;
    DepthMode   depth   = DepthMode::PRIMITIVES;
    std::string group_name;

    /* Rectangular grid */
    int     nx = 1, ny = 1, nz = 1;
    fastf_t dx = 0, dy = 0, dz = 0;
    std::vector<fastf_t> lx, ly, lz;
    vect_t xdir = {1, 0, 0};
    vect_t ydir = {0, 1, 0};
    vect_t zdir = {0, 0, 1};

    /* Spherical  (az/el/r also reused by cylindrical) */
    int     naz = 0, nel = 0, nr = 0;
    fastf_t daz = 0, del_v = 0, dr = 0;   /* del_v avoids clash with ::del */
    std::vector<fastf_t> laz, lel, lr;
    fastf_t start_az = 0;
    fastf_t start_el = -M_PI_2;            /* default -90 deg */
    fastf_t start_r  = 0;
    vect_t  center_pat = {0, 0, 0};
    vect_t  center_obj = {0, 0, 0};
    bool    rotaz = false;
    bool    rotel = false;

    /* Cylindrical */
    int     nh = 0;
    fastf_t dh = 0, start_h = 0;
    std::vector<fastf_t> lh;
    vect_t  center_base  = {0, 0, 0};
    vect_t  height_dir   = {0, 0, 1};
    vect_t  start_az_dir = {1, 0, 0};
    bool    do_rot = false;

    /* Optional name string substitution: replace the first occurrence of
     * subs_src in each generated name with subs_dst (applied before the
     * number-increment step). */
    std::string subs_src;
    std::string subs_dst;

    /* BU_CLBK_DURING progress callback: called once per top-level clone
     * position created (pattern modes) or per copy (linear mode).
     * argv passed to the callback:
     *   {"step", new_obj_name, current_str, total_str}
     * where current_str is the 1-based index of this clone and total_str
     * is the total number of clones expected for the whole operation.
     * Populated from ged_clbk_get at the start of ged_clone_core. */
    bu_clbk_t   clbk    = nullptr;
    void       *clbk_u1 = nullptr;
    void       *clbk_u2 = nullptr;
    size_t      clones_done  = 0;  /**< running count, incremented per fire */
    size_t      clones_total = 0;  /**< total expected for this operation   */

    /* Output */
    struct bu_vls olist = BU_VLS_INIT_ZERO;
    NameMap        names;
};


/* -----------------------------------------------------------------------
 * Unique-name generation
 * ----------------------------------------------------------------------- */

static int
_clone_uniq(struct bu_vls *n, void *data)
{
    return (db_lookup((struct db_i *)data, bu_vls_cstr(n), LOOKUP_QUIET)
	    == RT_DIR_NULL) ? 1 : 0;
}


/**
 * Return the next available unique clone name derived from @a proto.
 *
 * The @a updpos-th (0-indexed) run of digits in @a proto is incremented
 * by @a step.  Zero-padding width is preserved.  When no digit run exists
 * bu_vls_incr appends a new number.
 */
static std::string
clone_next_name(struct db_i *dbip, const std::string& proto,
		int step, int updpos,
		const std::string& subs_src = std::string(),
		const std::string& subs_dst = std::string())
{
    /* Apply optional name substitution before number-increment logic.
     * Replace only the first occurrence so that e.g. "-s tank tread" on
     * "tank.r01" yields "tread.r01" without touching the "r" or "01". */
    std::string work = proto;
    if (!subs_src.empty()) {
	size_t p = work.find(subs_src);
	if (p != std::string::npos)
	    work.replace(p, subs_src.size(), subs_dst);
    }

    struct Run { size_t start, end; };
    std::vector<Run> runs;
    for (size_t i = 0; i < work.size(); ) {
	if (isdigit((unsigned char)work[i])) {
	    size_t j = i;
	    while (j < work.size() && isdigit((unsigned char)work[j])) ++j;
	    runs.push_back({i, j});
	    i = j;
	} else {
	    ++i;
	}
    }

    if (runs.empty()) {
	struct bu_vls vn = BU_VLS_INIT_ZERO;
	struct bu_vls sp = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&vn, "%s", work.c_str());
	bu_vls_sprintf(&sp, "0:0:0:%d", step);
	bu_vls_incr(&vn, nullptr, bu_vls_cstr(&sp), &_clone_uniq, dbip);
	std::string r(bu_vls_cstr(&vn));
	bu_vls_free(&vn);
	bu_vls_free(&sp);
	return r;
    }

    int ridx = (updpos < (int)runs.size()) ? updpos : (int)runs.size() - 1;
    const Run& run = runs[(size_t)ridx];
    std::string prefix = work.substr(0, run.start);
    int cur = std::stoi(work.substr(run.start, run.end - run.start));
    std::string suffix = work.substr(run.end);
    size_t width = run.end - run.start;
    bool zero_padded = (width > 1 && work[run.start] == '0');

    for (int i = 1; ; ++i) {
	int val = cur + i * step;
	char buf[64];
	if (zero_padded)
	    snprintf(buf, sizeof(buf), "%0*d", (int)width, val);
	else
	    snprintf(buf, sizeof(buf), "%d", val);
	std::string cand = prefix + buf + suffix;
	if (db_lookup(dbip, cand.c_str(), LOOKUP_QUIET) == RT_DIR_NULL)
	    return cand;
    }
}


/* -----------------------------------------------------------------------
 * V5 combination tree: update member names
 * ----------------------------------------------------------------------- */

static int
update_comb_tree(CloneState *state, union tree *tree, size_t copy_idx)
{
    switch (tree->tr_op) {
	case OP_UNION: case OP_INTERSECT: case OP_SUBTRACT: case OP_XOR:
	    if (update_comb_tree(state, tree->tr_b.tb_right, copy_idx) < 0)
		return -1;
	    /* fall through */
	case OP_NOT: case OP_GUARD: case OP_XNOP:
	    return update_comb_tree(state, tree->tr_b.tb_left, copy_idx);

	case OP_DB_LEAF: {
	    auto it = state->names.find(tree->tr_l.tl_name);
	    if (it == state->names.end()) {
		bu_vls_printf(state->gedp->ged_result_str,
			     "clone: unknown member \"%s\"\n",
			     tree->tr_l.tl_name);
		return -1;
	    }
	    char *old = tree->tr_l.tl_name;
	    tree->tr_l.tl_name = bu_strdup(it->second[copy_idx].c_str());
	    bu_free(old, "comb tree leaf name");
	    return 0;
	}
	default:
	    bu_vls_printf(state->gedp->ged_result_str,
			 "clone: unsupported tree opcode %d\n", tree->tr_op);
	    return -1;
    }
}


/* -----------------------------------------------------------------------
 * V5 solid deep-copy (matrix baked in)
 * ----------------------------------------------------------------------- */

static void
copy_v5_solid(struct db_i *dbip, struct directory *proto, CloneState *state)
{
    /* Build the transform matrix for all copies */
    mat_t matrix;

    if (state->override_matrix) {
	/* Direct 4x4 matrix supplied (pattern primitives-depth mode) */
	MAT_COPY(matrix, state->override_matrix);
    } else {
	MAT_IDN(matrix);
	if (state->miraxis != W) {
	    matrix[state->miraxis * 5] = -1.0;
	    matrix[3 + state->miraxis * 4] = 2.0 * state->mirpos;
	}
	if (!ZERO(state->trans[W]))
	    MAT_DELTAS_ADD_VEC(matrix, state->trans);
	if (!ZERO(state->rot[W])) {
	    mat_t m2, t;
	    bn_mat_angles(m2, state->rot[X], state->rot[Y], state->rot[Z]);
	    if (!ZERO(state->rpnt[W])) {
		mat_t m3;
		bn_mat_xform_about_pnt(m3, m2, state->rpnt);
		bn_mat_mul(t, matrix, m3);
	    } else {
		bn_mat_mul(t, matrix, m2);
	    }
	    MAT_COPY(matrix, t);
	}
    }

    state->names[proto->d_namep].resize(state->n_copies);

    for (size_t i = 0; i < state->n_copies; i++) {
	const char *src_name = (i == 0) ? proto->d_namep
	    : state->names[proto->d_namep][i - 1].c_str();
	struct directory *src_dp = db_lookup(dbip, src_name, LOOKUP_QUIET);
	if (!src_dp) continue;

	std::string dest = clone_next_name(dbip, src_name, state->incr,
					   (int)state->updpos,
					   (i == 0) ? state->subs_src : std::string(),
					   (i == 0) ? state->subs_dst : std::string());
	state->names[proto->d_namep][i] = dest;

	const char *cp_av[4] = { "copy", proto->d_namep, dest.c_str(), nullptr };
	if (ged_exec_copy(state->gedp, 3, cp_av) != BRLCAD_OK) {
	    bu_vls_printf(state->gedp->ged_result_str,
			 "clone: failed to copy \"%s\" to \"%s\"\n",
			 proto->d_namep, dest.c_str());
	    continue;
	}

	struct rt_db_internal intern;
	if (rt_db_get_internal(&intern, src_dp, dbip, matrix,
			       &rt_uniresource) < 0) {
	    bu_vls_printf(state->gedp->ged_result_str,
			 "clone: rt_db_get_internal failed for \"%s\"\n",
			 proto->d_namep);
	    continue;
	}
	RT_CK_DB_INTERNAL(&intern);

	struct directory *new_dp = db_lookup(dbip, dest.c_str(), LOOKUP_QUIET);
	if (new_dp)
	    rt_db_put_internal(new_dp, dbip, &intern, &rt_uniresource);

	bu_vls_printf(&state->olist, "%s ", dest.c_str());
	rt_db_free_internal(&intern);
    }
}


/* -----------------------------------------------------------------------
 * V5 combination deep-copy
 * ----------------------------------------------------------------------- */

static struct directory *
copy_v5_comb(struct db_i *dbip, struct directory *proto, CloneState *state)
{
    struct directory *dp = nullptr;

    state->names[proto->d_namep].resize(state->n_copies);

    for (size_t i = 0; i < state->n_copies; i++) {
	const char *prev = (i == 0) ? proto->d_namep
	    : state->names[proto->d_namep][i - 1].c_str();
	if (i > 0 && !db_lookup(dbip, prev, LOOKUP_QUIET)) continue;

	std::string dest = clone_next_name(dbip, prev, 1, (int)state->updpos,
					   (i == 0) ? state->subs_src : std::string(),
					   (i == 0) ? state->subs_dst : std::string());
	state->names[proto->d_namep][i] = dest;

	struct directory *proto_dp = db_lookup(dbip, proto->d_namep,
					       LOOKUP_QUIET);
	if (!proto_dp) continue;
	struct rt_db_internal dbintern;
	if (rt_db_get_internal(&dbintern, proto_dp, dbip, bn_mat_identity,
			       &rt_uniresource) < 0) {
	    bu_vls_printf(state->gedp->ged_result_str,
			 "clone: cannot read \"%s\"\n", proto->d_namep);
	    return nullptr;
	}

	int minor = proto->d_minor_type;
	dp = db_diradd(dbip, dest.c_str(), RT_DIR_PHONY_ADDR, 0,
		       proto->d_flags, (void *)&minor);
	if (!dp) {
	    bu_vls_printf(state->gedp->ged_result_str,
			 "clone: cannot add \"%s\" to directory\n",
			 dest.c_str());
	    rt_db_free_internal(&dbintern);
	    return nullptr;
	}

	RT_CK_DB_INTERNAL(&dbintern);
	struct rt_comb_internal *comb =
	    (struct rt_comb_internal *)dbintern.idb_ptr;
	RT_CK_COMB(comb);
	if (comb->tree) {
	    RT_CK_TREE(comb->tree);
	    if (update_comb_tree(state, comb->tree, i) < 0) {
		rt_db_free_internal(&dbintern);
		return nullptr;
	    }
	}

	if (rt_db_put_internal(dp, dbip, &dbintern, &rt_uniresource) < 0) {
	    bu_vls_printf(state->gedp->ged_result_str,
			 "clone: write failed for \"%s\"\n", dest.c_str());
	    rt_db_free_internal(&dbintern);
	    return nullptr;
	}
	bu_vls_printf(&state->olist, "%s ", dest.c_str());
	rt_db_free_internal(&dbintern);
    }
    return dp;
}


/* -----------------------------------------------------------------------
 * db_functree callbacks and tree traversal
 * ----------------------------------------------------------------------- */

static void
copy_solid_cb(struct db_i *dbip, struct directory *dp, void *cd)
{
    CloneState *s = (CloneState *)cd;
    if (s->names.count(dp->d_namep)) return;
    s->names[dp->d_namep] = std::vector<std::string>(s->n_copies);
    copy_v5_solid(dbip, dp, s);
}

static void
copy_comb_cb(struct db_i *dbip, struct directory *dp, void *cd)
{
    CloneState *s = (CloneState *)cd;
    if (s->names.count(dp->d_namep)) return;
    s->names[dp->d_namep] = std::vector<std::string>(s->n_copies);
    copy_v5_comb(dbip, dp, s);
}

static struct directory *
copy_tree(struct directory *dp, struct resource *resp, CloneState *state)
{
    struct db_i *dbip = state->gedp->dbip;
    int step = (dp->d_flags & (RT_DIR_SOLID | RT_DIR_REGION))
	? state->incr : 1;
    std::string expected = clone_next_name(dbip, dp->d_namep, step,
					   (int)state->updpos,
					   state->subs_src, state->subs_dst);

    if (dp->d_flags & RT_DIR_COMB)
	db_functree(dbip, dp, copy_comb_cb, copy_solid_cb, resp, state);
    else if (dp->d_flags & RT_DIR_SOLID)
	copy_solid_cb(dbip, dp, state);
    else {
	bu_vls_printf(state->gedp->ged_result_str,
		     "clone: \"%s\" is neither comb nor solid\n",
		     dp->d_namep);
	return nullptr;
    }

    std::string now = clone_next_name(dbip, dp->d_namep, step,
				      (int)state->updpos,
				      state->subs_src, state->subs_dst);
    if (expected == now) {
	bu_vls_printf(state->gedp->ged_result_str,
		     "clone: failed to create \"%s\"\n", expected.c_str());
	return nullptr;
    }
    return db_lookup(dbip, expected.c_str(), LOOKUP_QUIET);
}

static struct directory *
deep_copy_object(struct resource *resp, CloneState *state)
{
    if (state->srcs.empty() || !state->n_copies) return nullptr;
    state->names.clear();
    return copy_tree(state->srcs[0], resp, state);
}


/* -----------------------------------------------------------------------
 * "Top" depth mode: wrapper comb referencing original with a matrix
 * ----------------------------------------------------------------------- */

static struct directory *
create_wrapper_comb(struct ged *gedp, const char *src_name,
		    const char *dest_name, const mat_t mat)
{
    struct db_i *dbip = gedp->dbip;

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type   = ID_COMBINATION;
    intern.idb_meth   = &OBJ[ID_COMBINATION];

    struct rt_comb_internal *comb;
    BU_ALLOC(comb, struct rt_comb_internal);
    RT_COMB_INTERNAL_INIT(comb);
    intern.idb_ptr = comb;

    union tree *leaf;
    BU_GET(leaf, union tree);
    RT_TREE_INIT(leaf);
    leaf->tr_l.tl_op   = OP_DB_LEAF;
    leaf->tr_l.tl_name = bu_strdup(src_name);
    if (!bn_mat_is_identity(mat)) {
	leaf->tr_l.tl_mat = (matp_t)bu_malloc(sizeof(mat_t), "leaf mat");
	MAT_COPY(leaf->tr_l.tl_mat, mat);
    } else {
	leaf->tr_l.tl_mat = nullptr;
    }
    comb->tree = leaf;

    int minor = ID_COMBINATION;
    struct directory *dp = db_diradd(dbip, dest_name, RT_DIR_PHONY_ADDR, 0,
				     RT_DIR_COMB, (void *)&minor);
    if (!dp) {
	bu_vls_printf(gedp->ged_result_str,
		     "clone: cannot add \"%s\" to directory\n", dest_name);
	rt_db_free_internal(&intern);
	return nullptr;
    }
    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str,
		     "clone: write failed for \"%s\"\n", dest_name);
	rt_db_free_internal(&intern);
	return nullptr;
    }
    rt_db_free_internal(&intern);
    return dp;
}


/* -----------------------------------------------------------------------
 * Group creation
 * ----------------------------------------------------------------------- */

static void
create_group(CloneState *state, const std::vector<std::string>& members)
{
    if (state->group_name.empty() || members.empty()) return;
    std::vector<const char *> av;
    av.push_back("g");
    av.push_back(state->group_name.c_str());
    for (auto& m : members) av.push_back(m.c_str());
    av.push_back(nullptr);
    if (ged_exec_g(state->gedp, (int)av.size() - 1, av.data()) != BRLCAD_OK)
	bu_vls_printf(state->gedp->ged_result_str,
		     "clone: failed to create group \"%s\"\n",
		     state->group_name.c_str());
}


/* -----------------------------------------------------------------------
 * "Regions" depth mode
 *
 * Equivalent to the Tcl copy_obj + apply_mat -regions workflow:
 *   1. Copy the combination hierarchy down to (and including) regions.
 *      Primitives are NOT copied — they are shared.
 *   2. Assign each new region a fresh ident from wdb_item_default.
 *   3. Apply the placement matrix at the leaf arcs INSIDE each region
 *      (matrix is propagated down through non-region combinations and
 *      accumulated before being written into region leaf arcs).
 * ----------------------------------------------------------------------- */

/** Forward declaration (mutual recursion). */
static void apply_mat_at_regions(struct db_i *dbip,
				  struct directory *dp, const mat_t mat);


/**
 * Apply @a mat to every leaf arc in @a tree.
 * Equivalent to Tcl's apply_mat_comb.
 */
static void
mat_into_comb_leaves(union tree *tree, const mat_t mat)
{
    if (!tree) return;
    switch (tree->tr_op) {
	case OP_DB_LEAF: {
	    mat_t new_mat;
	    if (tree->tr_l.tl_mat) {
		bn_mat_mul(new_mat, mat, tree->tr_l.tl_mat);
		MAT_COPY(tree->tr_l.tl_mat, new_mat);
	    } else if (!bn_mat_is_identity(mat)) {
		tree->tr_l.tl_mat = (matp_t)bu_malloc(sizeof(mat_t),
						       "region leaf mat");
		MAT_COPY(tree->tr_l.tl_mat, mat);
	    }
	    return;
	}
	case OP_UNION: case OP_INTERSECT: case OP_SUBTRACT: case OP_XOR:
	    mat_into_comb_leaves(tree->tr_b.tb_right, mat);
	    /* fall through */
	case OP_NOT: case OP_GUARD: case OP_XNOP:
	    mat_into_comb_leaves(tree->tr_b.tb_left, mat);
	    return;
	default: return;
    }
}


/**
 * Walk a Boolean tree and propagate @a mat down into each reachable region.
 * For each leaf arc that references a combination: accumulate the arc matrix
 * into @a mat, call apply_mat_at_regions recursively, then CLEAR the arc
 * matrix on this leaf (so the parent combination stores no matrix here).
 * Primitive leaves are silently ignored — they should not appear above regions.
 *
 * Equivalent to Tcl's apply_mat_to_regions.
 */
static void
apply_mat_to_regions_tree(struct db_i *dbip, union tree *tree, const mat_t mat)
{
    if (!tree) return;
    switch (tree->tr_op) {
	case OP_DB_LEAF: {
	    const char *name = tree->tr_l.tl_name;
	    struct directory *dp = db_lookup(dbip, name, LOOKUP_QUIET);
	    if (!dp || !(dp->d_flags & RT_DIR_COMB)) return; /* skip primitives */

	    /* Accumulate the arc matrix into the incoming mat */
	    mat_t accum;
	    if (tree->tr_l.tl_mat)
		bn_mat_mul(accum, mat, tree->tr_l.tl_mat);
	    else
		MAT_COPY(accum, mat);

	    /* Propagate down through the referenced combination */
	    apply_mat_at_regions(dbip, dp, accum);

	    /* Absorb the arc matrix — it has been pushed into lower levels */
	    if (tree->tr_l.tl_mat) {
		bu_free(tree->tr_l.tl_mat, "absorbed arc mat");
		tree->tr_l.tl_mat = nullptr;
	    }
	    return;
	}
	case OP_UNION: case OP_INTERSECT: case OP_SUBTRACT: case OP_XOR:
	    apply_mat_to_regions_tree(dbip, tree->tr_b.tb_right, mat);
	    /* fall through */
	case OP_NOT: case OP_GUARD: case OP_XNOP:
	    apply_mat_to_regions_tree(dbip, tree->tr_b.tb_left, mat);
	    return;
	default: return;
    }
}


/**
 * Entry point for matrix propagation.
 *
 * If @a dp is a region, apply @a mat directly to its leaf arcs.
 * If @a dp is a non-region combination, propagate through its sub-tree.
 * In both cases the modified combination is written back to the database.
 */
static void
apply_mat_at_regions(struct db_i *dbip, struct directory *dp, const mat_t mat)
{
    if (!dp || !(dp->d_flags & RT_DIR_COMB)) return;

    struct rt_db_internal intern;
    if (rt_db_get_internal(&intern, dp, dbip, bn_mat_identity,
			   &rt_uniresource) < 0) return;
    struct rt_comb_internal *comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    if (comb->region_flag) {
	/* Region: apply mat to all leaf arcs */
	mat_into_comb_leaves(comb->tree, mat);
    } else {
	/* Non-region combination: propagate through sub-tree */
	if (comb->tree)
	    apply_mat_to_regions_tree(dbip, comb->tree, mat);
    }

    rt_db_put_internal(dp, dbip, &intern, &rt_uniresource);
    rt_db_free_internal(&intern);
}


/* Forward declaration (mutual recursion in copy_regions_node). */
static std::string copy_regions_node(struct db_i *dbip,
				     const std::string &obj_name,
				     CloneState *state);


/**
 * Walk a Boolean tree and for each DB_LEAF that references a combination,
 * call copy_regions_node to recursively copy the sub-hierarchy, then update
 * the leaf name to the new (copied) name.  Primitive leaves are left
 * unchanged (they are shared).
 */
static void
copy_regions_and_update_tree(struct db_i *dbip, union tree *tree,
			     CloneState *state)
{
    if (!tree) return;
    switch (tree->tr_op) {
	case OP_DB_LEAF: {
	    std::string name(tree->tr_l.tl_name);
	    std::string new_name = copy_regions_node(dbip, name, state);
	    if (new_name != name) {
		char *old = tree->tr_l.tl_name;
		tree->tr_l.tl_name = bu_strdup(new_name.c_str());
		bu_free(old, "old regions leaf name");
	    }
	    return;
	}
	case OP_UNION: case OP_INTERSECT: case OP_SUBTRACT: case OP_XOR:
	    copy_regions_and_update_tree(dbip, tree->tr_b.tb_right, state);
	    /* fall through */
	case OP_NOT: case OP_GUARD: case OP_XNOP:
	    copy_regions_and_update_tree(dbip, tree->tr_b.tb_left, state);
	    return;
	default: return;
    }
}


/**
 * Recursively copy @a obj_name down to (and including) regions.
 * Primitives are not copied — an identity mapping is recorded so that
 * the parent combination tree continues to reference the original.
 *
 * Returns the new name on success, or the original name on failure.
 * Results are memoized in state->names to handle shared sub-trees and
 * to prevent infinite recursion on pathological (circular) inputs.
 */
static std::string
copy_regions_node(struct db_i *dbip, const std::string &obj_name,
		  CloneState *state)
{
    /* Already processed? */
    auto it = state->names.find(obj_name);
    if (it != state->names.end())
	return it->second.empty() ? obj_name : it->second[0];

    struct directory *dp = db_lookup(dbip, obj_name.c_str(), LOOKUP_QUIET);
    if (!dp) {
	/* Unknown object — keep as is */
	state->names[obj_name] = { obj_name };
	return obj_name;
    }

    /* Primitive: share the original, record identity mapping */
    if (!(dp->d_flags & RT_DIR_COMB)) {
	state->names[obj_name] = { obj_name };
	return obj_name;
    }

    /* Read the combination record */
    struct rt_db_internal intern;
    if (rt_db_get_internal(&intern, dp, dbip, bn_mat_identity,
			   &rt_uniresource) < 0) {
	state->names[obj_name] = { obj_name };
	return obj_name;
    }
    struct rt_comb_internal *comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    /* Reserve a placeholder to break potential circular references */
    state->names[obj_name] = {};

    bool is_region = (comb->region_flag != 0);

    if (!is_region) {
	/* Non-region combination: recurse into leaves, then create copy */
	if (comb->tree)
	    copy_regions_and_update_tree(dbip, comb->tree, state);

	std::string new_name = clone_next_name(dbip, obj_name, 1,
					       (int)state->updpos,
					       state->subs_src, state->subs_dst);
	state->names[obj_name] = { new_name };

	int minor = dp->d_minor_type;
	struct directory *new_dp = db_diradd(dbip, new_name.c_str(),
					     RT_DIR_PHONY_ADDR, 0,
					     dp->d_flags, (void *)&minor);
	if (new_dp && rt_db_put_internal(new_dp, dbip, &intern,
					 &rt_uniresource) == 0) {
	    bu_vls_printf(&state->olist, "%s ", new_name.c_str());
	    rt_db_free_internal(&intern);
	    return new_name;
	}
	rt_db_free_internal(&intern);
	state->names[obj_name] = { obj_name };
	return obj_name;
    }

    /* Region: copy record and assign a fresh ident */
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (wdbp)
	comb->region_id = wdbp->wdb_item_default++;

    std::string new_name = clone_next_name(dbip, obj_name, state->incr,
					   (int)state->updpos,
					   state->subs_src, state->subs_dst);
    state->names[obj_name] = { new_name };

    int minor = dp->d_minor_type;
    struct directory *new_dp = db_diradd(dbip, new_name.c_str(),
					 RT_DIR_PHONY_ADDR, 0,
					 dp->d_flags, (void *)&minor);
    if (new_dp && rt_db_put_internal(new_dp, dbip, &intern,
				     &rt_uniresource) == 0) {
	bu_vls_printf(&state->olist, "%s ", new_name.c_str());
	rt_db_free_internal(&intern);
	return new_name;
    }
    rt_db_free_internal(&intern);
    state->names[obj_name] = { obj_name };
    return obj_name;
}


/**
 * "Regions" depth mode: copy @a src down to regions (sharing primitives),
 * then apply @a mat at the region leaf arcs.
 *
 * Returns the new top-level name, or "" on failure.
 */
static std::string
apply_one_position_regions(CloneState *state, struct directory *src,
			   const mat_t mat)
{
    if (!(src->d_flags & RT_DIR_COMB)) {
	bu_vls_printf(state->gedp->ged_result_str,
		     "clone --depth regions: \"%s\" is a primitive; "
		     "--depth regions requires a combination as source\n",
		     src->d_namep);
	return {};
    }

    struct db_i *dbip = state->gedp->dbip;
    state->names.clear();

    std::string new_name = copy_regions_node(dbip, src->d_namep, state);
    if (new_name.empty() || new_name == src->d_namep)
	return {};

    struct directory *new_dp = db_lookup(dbip, new_name.c_str(), LOOKUP_QUIET);
    if (!new_dp) return {};

    /* Apply placement matrix at region leaf arcs */
    if (!bn_mat_is_identity(mat))
	apply_mat_at_regions(dbip, new_dp, mat);

    return new_name;
}

static std::string
apply_one_position(CloneState *state, struct directory *src,
		   const mat_t mat)
{
    struct db_i *dbip = state->gedp->dbip;

    if (state->depth == DepthMode::TOP) {
	int step = (src->d_flags & (RT_DIR_SOLID | RT_DIR_REGION))
	    ? state->incr : 1;
	std::string dest = clone_next_name(dbip, src->d_namep, step,
					   (int)state->updpos,
					   state->subs_src, state->subs_dst);
	struct directory *dp = create_wrapper_comb(state->gedp, src->d_namep,
						   dest.c_str(), mat);
	return dp ? dest : std::string();
    }

    if (state->depth == DepthMode::REGIONS)
	return apply_one_position_regions(state, src, mat);

    /* PRIMITIVES: deep copy with the raw 4x4 matrix baked into each solid */
    CloneState tmp;
    tmp.gedp            = state->gedp;
    tmp.srcs            = { src };
    tmp.incr            = state->incr;
    tmp.n_copies        = 1;
    tmp.updpos          = state->updpos;
    tmp.miraxis         = W;
    tmp.override_matrix = mat;
    tmp.subs_src        = state->subs_src;
    tmp.subs_dst        = state->subs_dst;
    bu_vls_init(&tmp.olist);

    state->names.clear();
    struct directory *copy = deep_copy_object(&rt_uniresource, &tmp);
    for (auto& kv : tmp.names)
	state->names[kv.first] = kv.second;
    bu_vls_printf(&state->olist, "%s", bu_vls_cstr(&tmp.olist));
    bu_vls_free(&tmp.olist);

    return copy ? std::string(copy->d_namep) : std::string();
}


/* -----------------------------------------------------------------------
 * Pattern placement matrix builders
 * ----------------------------------------------------------------------- */

static void mat_rect(mat_t m, const vect_t pos)
{
    MAT_IDN(m);
    MAT_DELTAS_VEC(m, pos);
}


/**
 * Spherical placement matrix — matches pattern_sph Tcl transform:
 *   mat = translate(r_vec + center_pat) * optRot(az, el) * translate(-center_obj)
 */
static void
mat_sph(mat_t m,
	fastf_t az, fastf_t el, fastf_t r,
	const vect_t center_pat, const vect_t center_obj,
	bool do_rotaz, bool do_rotel)
{
    vect_t neg_cobj;
    VREVERSE(neg_cobj, center_obj);
    mat_t m_neg_cobj;
    MAT_IDN(m_neg_cobj);
    MAT_DELTAS_VEC(m_neg_cobj, neg_cobj);

    mat_t m_rot;
    MAT_IDN(m_rot);
    if (do_rotaz && do_rotel)
	bn_mat_ae(m_rot, az * RAD2DEG, el * RAD2DEG);
    else if (do_rotaz)
	bn_mat_ae(m_rot, az * RAD2DEG, 0.0);
    else if (do_rotel)
	bn_mat_ae(m_rot, 0.0, el * RAD2DEG);

    mat_t m2;
    bn_mat_mul(m2, m_rot, m_neg_cobj);

    vect_t r_vec;
    r_vec[X] = r * cos(az) * cos(el);
    r_vec[Y] = r * sin(az) * cos(el);
    r_vec[Z] = r * sin(el);
    vect_t tv;
    VADD2(tv, r_vec, center_pat);
    mat_t m_trans;
    MAT_IDN(m_trans);
    MAT_DELTAS_VEC(m_trans, tv);

    bn_mat_mul(m, m_trans, m2);
}


/**
 * Cylindrical placement matrix — matches pattern_cyl Tcl transform:
 *   r_vec = r·cos(az)·start_az_dir + r·sin(az)·az_dir2 + h·height_dir
 *   mat   = translate(r_vec+center_base) * optArb(az) * translate(-center_obj)
 */
static void
mat_cyl(mat_t m,
	fastf_t az, fastf_t r, fastf_t h,
	const vect_t center_base, const vect_t center_obj,
	const vect_t height_dir, const vect_t start_az_dir,
	const vect_t az_dir2, bool do_rot)
{
    vect_t neg_cobj;
    VREVERSE(neg_cobj, center_obj);
    mat_t m_neg_cobj;
    MAT_IDN(m_neg_cobj);
    MAT_DELTAS_VEC(m_neg_cobj, neg_cobj);

    mat_t m_rot;
    MAT_IDN(m_rot);
    if (do_rot)
	bn_mat_arb_rot(m_rot, neg_cobj, height_dir, az);

    mat_t m2;
    bn_mat_mul(m2, m_rot, m_neg_cobj);

    vect_t r_vec;
    VSETALL(r_vec, 0);
    VJOIN2(r_vec, r_vec, r * cos(az), start_az_dir, r * sin(az), az_dir2);
    VJOIN1(r_vec, r_vec, h, height_dir);

    vect_t tv;
    VADD2(tv, r_vec, center_base);
    mat_t m_trans;
    MAT_IDN(m_trans);
    MAT_DELTAS_VEC(m_trans, tv);

    bn_mat_mul(m, m_trans, m2);
}


/* -----------------------------------------------------------------------
 * Position-list builder (count+delta or explicit list)
 * ----------------------------------------------------------------------- */

static std::vector<fastf_t>
make_positions(int n, fastf_t start, fastf_t delta,
	       const std::vector<fastf_t>& explicit_list,
	       fastf_t scale = 1.0)
{
    if (!explicit_list.empty()) {
	std::vector<fastf_t> v;
	v.reserve(explicit_list.size());
	for (fastf_t x : explicit_list) v.push_back(x * scale);
	return v;
    }
    if (n <= 0) return { start * scale };
    std::vector<fastf_t> v;
    v.reserve((size_t)n);
    for (int i = 0; i < n; i++)
	v.push_back((start + delta * i) * scale);
    return v;
}


/* -----------------------------------------------------------------------
 * Pattern runners
 * ----------------------------------------------------------------------- */

/** Elevation within this distance (radians) of ±π/2 is treated as a pole. */
static const double SPH_POLE_TOL = 0.001;

/** Buffer size for rendering a clone counter as a decimal string. */
static const int CLONE_COUNTER_STR_SIZE = 32;

/** Fire the BU_CLBK_DURING progress callback for one clone event. */
static void
clone_fire_progress(CloneState *state, const std::string& new_name)
{
    if (!state->clbk) return;
    state->clones_done++;
    char done_str[CLONE_COUNTER_STR_SIZE], total_str[CLONE_COUNTER_STR_SIZE];
    snprintf(done_str,  CLONE_COUNTER_STR_SIZE, "%zu", state->clones_done);
    snprintf(total_str, CLONE_COUNTER_STR_SIZE, "%zu", state->clones_total);
    const char *av[4] = {"step", new_name.c_str(), done_str, total_str};
    (*state->clbk)(4, av, state->clbk_u1, state->clbk_u2);
}


static std::vector<std::string>
run_pattern_rect(CloneState *state)
{
    fastf_t l2b = state->gedp->dbip->dbi_local2base;

    std::vector<fastf_t> px = make_positions(state->nx, state->dx, state->dx,
					      state->lx, l2b);
    std::vector<fastf_t> py = make_positions(state->ny, state->dy, state->dy,
					      state->ly, l2b);
    std::vector<fastf_t> pz = make_positions(state->nz, state->dz, state->dz,
					      state->lz, l2b);

    vect_t xd, yd, zd;
    VMOVE(xd, state->xdir); VUNITIZE(xd);
    VMOVE(yd, state->ydir); VUNITIZE(yd);
    VMOVE(zd, state->zdir); VUNITIZE(zd);

    state->clones_total = px.size() * py.size() * pz.size() * state->srcs.size();
    state->clones_done  = 0;

    std::vector<std::string> names;
    for (fastf_t z : pz)
	for (fastf_t y : py)
	    for (fastf_t x : px) {
		vect_t pos; VSETALL(pos, 0);
		VJOIN1(pos, pos, x, xd);
		VJOIN1(pos, pos, y, yd);
		VJOIN1(pos, pos, z, zd);
		mat_t m; mat_rect(m, pos);
		for (auto *src : state->srcs) {
		    std::string n = apply_one_position(state, src, m);
		    if (!n.empty()) {
			names.push_back(n);
			clone_fire_progress(state, n);
		    }
		}
	    }
    return names;
}


static std::vector<std::string>
run_pattern_sph(CloneState *state)
{
    fastf_t l2b = state->gedp->dbip->dbi_local2base;

    std::vector<fastf_t> azl;
    if (!state->laz.empty())
	for (fastf_t v : state->laz) azl.push_back(v * DEG2RAD);
    else if (state->naz > 0)
	for (int i = 0; i < state->naz; i++)
	    azl.push_back(state->start_az + state->daz * i);
    else
	azl.push_back(state->start_az);

    std::vector<fastf_t> ell;
    if (!state->lel.empty())
	for (fastf_t v : state->lel) ell.push_back(v * DEG2RAD);
    else if (state->nel > 0)
	for (int i = 0; i < state->nel; i++)
	    ell.push_back(state->start_el + state->del_v * i);
    else
	ell.push_back(state->start_el);

    std::vector<fastf_t> rl = make_positions(state->nr, state->start_r,
					     state->dr, state->lr, l2b);

    vect_t cpat, cobj;
    VSCALE(cpat, state->center_pat, l2b);
    VSCALE(cobj, state->center_obj, l2b);

    /* Pre-compute total accounting for pole collapse (at_pole → only 1 az). */
    {
	size_t az_total = 0;
	for (fastf_t el : ell) {
	    bool at_pole = (fabs(M_PI_2 - fabs(el)) < SPH_POLE_TOL);
	    az_total += at_pole ? 1 : azl.size();
	}
	state->clones_total = az_total * rl.size() * state->srcs.size();
    }
    state->clones_done = 0;

    std::vector<std::string> names;
    for (fastf_t r : rl) {
	for (fastf_t el : ell) {
	    bool at_pole = (fabs(M_PI_2 - fabs(el)) < SPH_POLE_TOL);
	    for (fastf_t az : azl) {
		mat_t m;
		mat_sph(m, az, el, r, cpat, cobj, state->rotaz, state->rotel);
		for (auto *src : state->srcs) {
		    std::string n = apply_one_position(state, src, m);
		    if (!n.empty()) {
			names.push_back(n);
			clone_fire_progress(state, n);
		    }
		}
		if (at_pole) break;
	    }
	}
    }
    return names;
}


static std::vector<std::string>
run_pattern_cyl(CloneState *state)
{
    fastf_t l2b = state->gedp->dbip->dbi_local2base;

    vect_t hd, sad;
    VMOVE(hd, state->height_dir);
    VMOVE(sad, state->start_az_dir);
    fastf_t hd_mag  = MAGNITUDE(hd);
    fastf_t sad_mag = MAGNITUDE(sad);
    if (hd_mag < SMALL_FASTF || sad_mag < SMALL_FASTF) {
	bu_vls_printf(state->gedp->ged_result_str,
		     "clone --cyl: direction vector magnitude too small\n");
	return {};
    }
    VSCALE(hd,  hd,  1.0 / hd_mag);
    VSCALE(sad, sad, 1.0 / sad_mag);
    if (fabs(VDOT(hd, sad)) > 0.001) {
	bu_vls_printf(state->gedp->ged_result_str,
		     "clone --cyl: height-dir and start-az-dir must be "
		     "perpendicular\n");
	return {};
    }
    vect_t az_dir2;
    VCROSS(az_dir2, hd, sad);

    std::vector<fastf_t> azl;
    if (!state->laz.empty())
	for (fastf_t v : state->laz) azl.push_back(v * DEG2RAD);
    else if (state->naz > 0)
	for (int i = 0; i < state->naz; i++)
	    azl.push_back(state->start_az + state->daz * i);
    else
	azl.push_back(state->start_az);

    std::vector<fastf_t> rl = make_positions(state->nr, state->start_r,
					     state->dr, state->lr, l2b);
    std::vector<fastf_t> hl = make_positions(state->nh, state->start_h,
					     state->dh, state->lh, l2b);

    vect_t cbase, cobj;
    VSCALE(cbase, state->center_base, l2b);
    VSCALE(cobj,  state->center_obj,  l2b);

    state->clones_total = azl.size() * rl.size() * hl.size() * state->srcs.size();
    state->clones_done  = 0;

    std::vector<std::string> names;
    for (fastf_t r : rl)
	for (fastf_t h : hl)
	    for (fastf_t az : azl) {
		mat_t m;
		mat_cyl(m, az, r, h, cbase, cobj, hd, sad, az_dir2,
			state->do_rot);
		for (auto *src : state->srcs) {
		    std::string n = apply_one_position(state, src, m);
		    if (!n.empty()) {
			names.push_back(n);
			clone_fire_progress(state, n);
		    }
		}
	    }
    return names;
}


/* -----------------------------------------------------------------------
 * --xpush pre-processing
 * ----------------------------------------------------------------------- */

static struct directory *
make_xpushed_src(struct ged *gedp, struct directory *src)
{
    const char *clone_av[3] = { "clone", src->d_namep, nullptr };
    struct bu_vls saved = BU_VLS_INIT_ZERO;
    bu_vls_vlscat(&saved, gedp->ged_result_str);
    bu_vls_trunc(gedp->ged_result_str, 0);

    int ret = ged_exec_clone(gedp, 2, clone_av);
    std::string tmp_name;
    if (ret == BRLCAD_OK) {
	std::string res(bu_vls_cstr(gedp->ged_result_str));
	size_t sp = res.find_first_of(" {");
	tmp_name = (sp != std::string::npos) ? res.substr(0, sp) : res;
    }

    bu_vls_trunc(gedp->ged_result_str, 0);
    bu_vls_vlscat(gedp->ged_result_str, &saved);
    bu_vls_free(&saved);

    if (tmp_name.empty()) return nullptr;
    struct directory *tmp_dp = db_lookup(gedp->dbip, tmp_name.c_str(),
					 LOOKUP_QUIET);
    if (!tmp_dp) return nullptr;

    const char *xp_av[3] = { "xpush", tmp_dp->d_namep, nullptr };
    ged_exec_xpush(gedp, 2, xp_av);
    return tmp_dp;
}


/* -----------------------------------------------------------------------
 * Custom bu_opt_arg_process_t callbacks
 *
 * Pattern from libbu/opt.c and src/rtwizard/main.c:
 *   - use BU_OPT_CHECK_ARGV0 to guard against empty argv
 *   - return count of argv entries consumed, or -1 on error
 * ----------------------------------------------------------------------- */

/** Vec3Opt: flag + vect_t.  Used for -t / -r / -p. */
struct Vec3Opt {
    vect_t v   = {0, 0, 0};
    bool   set = false;
};

/**
 * Parse a 3-component vector from either 3 separate tokens ("x" "y" "z") or
 * a single space-separated string ("x y z") — the Tcl lappend form.
 * Returns argv entries consumed (3 or 1), or -1 on failure.
 */
static int
parse_vec3_argv(struct bu_vls *msg, size_t argc, const char **argv, vect_t v)
{
    /* Try 3-separate-token form first (standard CLI and explicit expand). */
    if (argc >= 3) {
	char *ep0 = NULL, *ep1 = NULL, *ep2 = NULL;
	fastf_t x = strtod(argv[0], &ep0);
	fastf_t y = strtod(argv[1], &ep1);
	fastf_t z = strtod(argv[2], &ep2);
	if (ep0 && !*ep0 && ep1 && !*ep1 && ep2 && !*ep2) {
	    VSET(v, x, y, z);
	    return 3;
	}
    }
    /* Fall back to a single "x y z" string (Tcl lappend form). */
    if (argc >= 1) {
	fastf_t x = 0, y = 0, z = 0;
	if (sscanf(argv[0], "%lf %lf %lf", &x, &y, &z) == 3) {
	    VSET(v, x, y, z);
	    return 1;
	}
    }
    if (msg)
	bu_vls_printf(msg, "parse_vec3_argv: expected 'x y z' (3 numbers)\n");
    return -1;
}

static int
opt_vec3(struct bu_vls *msg, size_t argc, const char **argv, void *sv)
{
    Vec3Opt *opt = (Vec3Opt *)sv;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "vec3");
    int ret = parse_vec3_argv(msg, argc, argv, opt->v);
    if (ret > 0) opt->set = true;
    return ret;
}

/**
 * opt_vect_t: like bu_opt_vect_t but uses parse_vec3_argv, which handles
 * both "x y z" as 3 separate tokens AND as a single "x y z" string
 * (e.g. from Tcl lappend '{ 1 0 0 }' which produces a string with spaces).
 * bu_opt_vect_t fails when the last token has a trailing space because
 * bu_argv_from_string does not null-terminate the trailing space, causing
 * bu_opt_fastf_t to reject "0 " via the endptr check.
 */
static int
opt_vect_t(struct bu_vls *msg, size_t argc, const char **argv, void *sv)
{
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "vect_t");
    fastf_t *v = (fastf_t *)sv;   /* vect_t is fastf_t[3]; decays to fastf_t* */
    return parse_vec3_argv(msg, argc, argv, v);
}


/** NVec3Opt: int + vect_t.  Used for -a / -b ("n x y z"). */
struct NVec3Opt {
    int    n   = 0;
    vect_t v   = {0, 0, 0};
    bool   set = false;
};

/**
 * Read "n x y z" — n as a separate first arg, then x/y/z as either
 * 3 separate tokens or a single "x y z" string (Tcl lappend form).
 * Returns total argv entries consumed (4 or 2 respectively).
 */
static int
opt_n_vec3(struct bu_vls *msg, size_t argc, const char **argv, void *sv)
{
    NVec3Opt *opt = (NVec3Opt *)sv;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "n_vec3");
    if (argc < 2) {
	if (msg)
	    bu_vls_printf(msg, "opt_n_vec3: requires n and x y z\n");
	return -1;
    }
    if (bu_opt_int(msg, 1, argv, (void *)&opt->n) < 0) return -1;
    int vret = parse_vec3_argv(msg, argc - 1, argv + 1, opt->v);
    if (vret < 0) return -1;
    opt->set = true;
    return 1 + vret;
}


/** MirrorOpt: axis + distance.  Used for -m ("axis dist"). */
struct MirrorOpt {
    int     axis = W;
    fastf_t dist = 0.0;
};

/**
 * Read mirror axis (x|y|z) and plane distance.  Returns 2.
 */
static int
opt_mirror(struct bu_vls *msg, size_t argc, const char **argv, void *sv)
{
    MirrorOpt *opt = (MirrorOpt *)sv;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "mirror");
    if (argc < 2) {
	if (msg)
	    bu_vls_printf(msg, "opt_mirror: requires axis (x|y|z) and distance\n");
	return -1;
    }
    char ax = (char)tolower((unsigned char)argv[0][0]);
    if      (ax == 'x') opt->axis = X;
    else if (ax == 'y') opt->axis = Y;
    else if (ax == 'z') opt->axis = Z;
    else {
	if (msg)
	    bu_vls_printf(msg, "opt_mirror: axis must be x, y, or z\n");
	return -1;
    }
    if (bu_opt_fastf_t(msg, 1, argv + 1, (void *)&opt->dist) < 0) return -1;
    return 2;
}


/** SubsOpt: name string-substitution pair.  Used for -s ("sstr rstr"). */
struct SubsOpt {
    std::string src, dst;
    bool set = false;
};

/**
 * Read search-string and replacement-string for name substitution.
 * Consumes two argv tokens; returns 2.
 */
static int
opt_subs(struct bu_vls *msg, size_t argc, const char **argv, void *sv)
{
    SubsOpt *opt = (SubsOpt *)sv;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "subs");
    if (argc < 2) {
	if (msg)
	    bu_vls_printf(msg, "opt_subs: requires search string and replacement string\n");
	return -1;
    }
    opt->src = argv[0];
    if (opt->src.empty()) {
	if (msg)
	    bu_vls_printf(msg, "opt_subs: search string must not be empty\n");
	return -1;
    }
    opt->dst = argv[1];
    opt->set = true;
    return 2;
}


/**
 * Parse depth choice "top"|"regions"|"primitives" into int*:
 *   2 = top, 1 = regions, 0 = primitives.
 * Returns 1 on success.
 */
static int
opt_depth(struct bu_vls *msg, size_t argc, const char **argv, void *sv)
{
    int *d = (int *)sv;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "depth");
    if (BU_STR_EQUAL(argv[0], "top")) {
	*d = 2; return 1;
    }
    if (BU_STR_EQUAL(argv[0], "regions")) {
	*d = 1; return 1;
    }
    if (BU_STR_EQUAL(argv[0], "primitives") || BU_STR_EQUAL(argv[0], "prim")) {
	*d = 0; return 1;
    }
    if (msg)
	bu_vls_printf(msg, "opt_depth: must be 'top', 'regions', or 'primitives', "
		      "got '%s'\n", argv[0]);
    return -1;
}


/**
 * Parse a space- or comma-separated list of floats from one argv token
 * into a std::vector<fastf_t>*.  Returns 1 (always consumes one entry).
 *
 * Accepts: "0 30 60 90"  or  "0,30,60,90".
 */
static int
opt_float_list(struct bu_vls *msg, size_t argc, const char **argv, void *sv)
{
    std::vector<fastf_t> *lst = (std::vector<fastf_t> *)sv;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "float_list");

    std::string s(argv[0]);
    for (char& c : s) if (c == ',') c = ' ';

    std::istringstream iss(s);
    fastf_t x;
    while (iss >> x) lst->push_back(x);

    if (lst->empty()) {
	if (msg)
	    bu_vls_printf(msg, "opt_float_list: no valid numbers in '%s'\n",
			 argv[0]);
	return -1;
    }
    return 1;
}


/* -----------------------------------------------------------------------
 * Usage
 * ----------------------------------------------------------------------- */

static void
print_usage(struct bu_vls *str)
{
    bu_vls_printf(str,
"Usage:\n"
"  clone [options] <object>           linear deep-copy\n"
"  clone --rect [options] <object>... rectangular grid\n"
"  clone --sph  [options] <object>... spherical pattern\n"
"  clone --cyl  [options] <object>... cylindrical pattern\n"
"\n"
"Common:\n"
"  -h, --help\n"
"  -i, --increment <n>          name number increment (default 100)\n"
"  -c, --second-number          increment next embedded number (repeat for Nth)\n"
"  --depth top|regions|primitives  copy depth (default: top for patterns,\n"
"                                primitives for linear mode)\n"
"                                  top:        wrapper comb {obj [mat]},\n"
"                                              primitives shared\n"
"                                  regions:    copy combs/regions, share\n"
"                                              primitives, mat at leaf arcs\n"
"                                  primitives: full deep copy, mat baked in\n"
"  --xpush                      flatten instance matrices before cloning\n"
"  -g, --group <name>           collect all clones into <name>\n"
"  -s, --subs  <sstr> <rstr>   replace first occurrence of sstr with rstr\n"
"                               in every generated object name\n"
"\n"
"Linear copy:\n"
"  -n, --copies <n>             number of copies\n"
"  -t, --translate <x y z>     per-copy translation\n"
"  -r, --rotate    <x y z>     per-copy rotation (deg) about x,y,z\n"
"  -p, --rpoint    <x y z>     rotation centre\n"
"  -a, --atrans  <n x y z>     total translation split over n copies\n"
"  -b, --arot    <n x y z>     total rotation split over n copies\n"
"  -m, --mirror  <axis d>      mirror: axis=x|y|z, d=plane distance\n"
"\n"
"Rectangular grid (--rect):\n"
"  --nx <n>  --dx <d>  --lx \"v1 v2...\"   (x)\n"
"  --ny <n>  --dy <d>  --ly \"v1 v2...\"   (y)\n"
"  --nz <n>  --dz <d>  --lz \"v1 v2...\"   (z)\n"
"  --xdir <x y z>  --ydir <x y z>  --zdir <x y z>\n"
"\n"
"Spherical (--sph):\n"
"  --naz <n>  --daz <deg>  --laz \"v1...\"  --start-az <deg>\n"
"  --nel <n>  --del <deg>  --lel \"v1...\"  --start-el <deg>\n"
"  --nr  <n>  --dr  <d>    --lr  \"v1...\"  --start-r  <r>\n"
"  --center-pat <x y z>  --center-obj <x y z>\n"
"  --rotaz  --rotel\n"
"\n"
"Cylindrical (--cyl):\n"
"  --naz <n>  --daz <deg>  --laz \"v1...\"  --start-az <deg>\n"
"  --nr  <n>  --dr  <d>    --lr  \"v1...\"  --start-r  <r>\n"
"  --nh  <n>  --dh  <d>    --lh  \"v1...\"  --start-h  <h>\n"
"  --center-base <x y z>  --center-obj <x y z>\n"
"  --height-dir <x y z>   --start-az-dir <x y z>\n"
"  --rot\n");
}


/* -----------------------------------------------------------------------
 * Argument parsing
 *
 * BU_OPT is a do-while macro that ASSIGNS fields; it cannot appear inside
 * a compound initialiser.  We declare the array first and then fill it
 * with sequential BU_OPT() / BU_OPT_NULL() calls.
 * ----------------------------------------------------------------------- */

static int
clone_parse_args(struct ged *gedp, int argc, const char **argv,
		 CloneState *state)
{
    /* Skip argv[0] (command name) */
    argc--; argv++;

    /* Scalar option targets */
    int print_help    = 0;
    int mode_rect     = 0, mode_sph = 0, mode_cyl = 0;
    int depth_flag    = -1;   /* -1 = not set; 2=top; 1=regions; 0=primitives */
    int n_copies_opt  = 0;
    int incr_opt      = 0;
    int nx_opt=0, ny_opt=0, nz_opt=0;
    int naz_opt=0, nel_opt=0, nr_opt=0, nh_opt=0;
    fastf_t dx_opt=0, dy_opt=0, dz_opt=0;
    fastf_t daz_opt=0, del_opt=0, dr_opt=0, dh_opt=0;
    fastf_t start_az_opt=0, start_el_opt=-90.0;
    fastf_t start_r_opt=-1.0, start_h_opt=-1.0;  /* -1 = not explicitly set */
    int rotaz_flag=0, rotel_flag=0, rot_flag=0;
    int xpush_flag=0;
    const char *group_cstr = nullptr;

    /* Custom-callback option targets */
    Vec3Opt   trans_opt, rot_opt, rpnt_opt;
    NVec3Opt  atrans_opt, arot_opt;
    MirrorOpt mirror_opt;
    SubsOpt   subs_opt;

    /* Build the option table.  The array needs 54 slots (53 options + NULL). */
    struct bu_opt_desc d[54];

    /* Common */
    BU_OPT(d[0],  "h", "help",         "",         NULL,           &print_help,         "Print help");
    BU_OPT(d[1],  "i", "increment",    "#",        bu_opt_int,     &incr_opt,           "Name increment");
    BU_OPT(d[2],  "c", "second-number","",         bu_opt_incr_long,&state->updpos,     "Increment next number");
    BU_OPT(d[3],  "",  "depth",        "top|reg|prim", opt_depth,      &depth_flag,         "Copy depth");
    BU_OPT(d[4],  "",  "xpush",        "",         NULL,           &xpush_flag,         "Flatten matrices");
    BU_OPT(d[5],  "g", "group",        "name",     bu_opt_str,     &group_cstr,         "Group clones");
    BU_OPT(d[6],  "s", "subs",         "sstr rstr",opt_subs,       &subs_opt,           "Name substitution");

    /* Linear copy */
    BU_OPT(d[7],  "n", "copies",       "#",        bu_opt_int,     &n_copies_opt,       "Number of copies");
    BU_OPT(d[8],  "t", "translate",    "x y z",    opt_vec3,       &trans_opt,          "Per-copy translation");
    BU_OPT(d[9],  "r", "rotate",       "x y z",    opt_vec3,       &rot_opt,            "Per-copy rotation");
    BU_OPT(d[10], "p", "rpoint",       "x y z",    opt_vec3,       &rpnt_opt,           "Rotation centre");
    BU_OPT(d[11], "a", "atrans",       "n x y z",  opt_n_vec3,     &atrans_opt,         "Total translation");
    BU_OPT(d[12], "b", "arot",         "n x y z",  opt_n_vec3,     &arot_opt,           "Total rotation");
    BU_OPT(d[13], "m", "mirror",       "axis d",   opt_mirror,     &mirror_opt,         "Mirror copy");

    /* Pattern mode selectors */
    BU_OPT(d[14], "", "rect", "", NULL, &mode_rect, "Rectangular grid");
    BU_OPT(d[15], "", "sph",  "", NULL, &mode_sph,  "Spherical pattern");
    BU_OPT(d[16], "", "cyl",  "", NULL, &mode_cyl,  "Cylindrical pattern");

    /* Rectangular grid */
    BU_OPT(d[17], "", "nx", "#",        bu_opt_int,     &nx_opt,      "Grid nx");
    BU_OPT(d[18], "", "ny", "#",        bu_opt_int,     &ny_opt,      "Grid ny");
    BU_OPT(d[19], "", "nz", "#",        bu_opt_int,     &nz_opt,      "Grid nz");
    BU_OPT(d[20], "", "dx", "#",        bu_opt_fastf_t, &dx_opt,      "Grid dx");
    BU_OPT(d[21], "", "dy", "#",        bu_opt_fastf_t, &dy_opt,      "Grid dy");
    BU_OPT(d[22], "", "dz", "#",        bu_opt_fastf_t, &dz_opt,      "Grid dz");
    BU_OPT(d[23], "", "lx", "\"...\"",  opt_float_list, &state->lx,  "x list");
    BU_OPT(d[24], "", "ly", "\"...\"",  opt_float_list, &state->ly,  "y list");
    BU_OPT(d[25], "", "lz", "\"...\"",  opt_float_list, &state->lz,  "z list");
    BU_OPT(d[26], "", "xdir", "x y z",  opt_vect_t,  state->xdir, "x direction");
    BU_OPT(d[27], "", "ydir", "x y z",  opt_vect_t,  state->ydir, "y direction");
    BU_OPT(d[28], "", "zdir", "x y z",  opt_vect_t,  state->zdir, "z direction");

    /* Spherical / cylindrical shared */
    BU_OPT(d[29], "", "naz",       "#",       bu_opt_int,      &naz_opt,           "Az count");
    BU_OPT(d[30], "", "nel",       "#",       bu_opt_int,      &nel_opt,           "El count (sph)");
    BU_OPT(d[31], "", "nr",        "#",       bu_opt_int,      &nr_opt,            "Radius count");
    BU_OPT(d[32], "", "nh",        "#",       bu_opt_int,      &nh_opt,            "Height count (cyl)");
    BU_OPT(d[33], "", "daz",       "#",       bu_opt_fastf_t,  &daz_opt,           "Az delta (deg)");
    BU_OPT(d[34], "", "del",       "#",       bu_opt_fastf_t,  &del_opt,           "El delta (deg, sph)");
    BU_OPT(d[35], "", "dr",        "#",       bu_opt_fastf_t,  &dr_opt,            "Radius delta");
    BU_OPT(d[36], "", "dh",        "#",       bu_opt_fastf_t,  &dh_opt,            "Height delta (cyl)");
    BU_OPT(d[37], "", "laz",       "\"...\"", opt_float_list,  &state->laz,        "Azimuth list (deg)");
    BU_OPT(d[38], "", "lel",       "\"...\"", opt_float_list,  &state->lel,        "Elevation list (deg)");
    BU_OPT(d[39], "", "lr",        "\"...\"", opt_float_list,  &state->lr,         "Radius list");
    BU_OPT(d[40], "", "lh",        "\"...\"", opt_float_list,  &state->lh,         "Height list (cyl)");
    BU_OPT(d[41], "", "start-az",  "#",       bu_opt_fastf_t,  &start_az_opt,      "Start azimuth (deg)");
    BU_OPT(d[42], "", "start-el",  "#",       bu_opt_fastf_t,  &start_el_opt,      "Start elevation (deg)");
    BU_OPT(d[43], "", "start-r",   "#",       bu_opt_fastf_t,  &start_r_opt,       "Start radius");
    BU_OPT(d[44], "", "start-h",   "#",       bu_opt_fastf_t,  &start_h_opt,       "Start height (cyl)");
    BU_OPT(d[45], "", "rotaz",     "",        NULL,            &rotaz_flag,         "Rotate with az");
    BU_OPT(d[46], "", "rotel",     "",        NULL,            &rotel_flag,         "Rotate with el");
    BU_OPT(d[47], "", "rot",       "",        NULL,            &rot_flag,           "Rotate with az (cyl)");
    BU_OPT(d[48], "", "center-pat",    "x y z", opt_vect_t, state->center_pat,  "Pattern origin (sph)");
    BU_OPT(d[49], "", "center-obj",    "x y z", opt_vect_t, state->center_obj,  "Object local centre");
    BU_OPT(d[50], "", "center-base",   "x y z", opt_vect_t, state->center_base, "Cylinder base (cyl)");
    BU_OPT(d[51], "", "height-dir",    "x y z", opt_vect_t, state->height_dir,  "Cylinder axis (cyl)");
    BU_OPT(d[52], "", "start-az-dir",  "x y z", opt_vect_t, state->start_az_dir,"Radial start dir");
    BU_OPT_NULL(d[53]);

    /* bu_opt_parse rewrites argv[] in-place, which would corrupt the
     * caller's (cmd_ged_edit_wrapper's) argv — the wrapper uses
     * argv[argc-1] after we return to decide what to redraw.  Work on a
     * private copy so the original is left intact. */
    std::vector<const char *> argv_local(argv, argv + argc);
    const char **av = argv_local.data();

    int opt_ret = bu_opt_parse(gedp->ged_result_str, (size_t)argc, av, d);

    if (print_help) {
	print_usage(gedp->ged_result_str);
	return GED_HELP;
    }
    if (opt_ret < 0) {
	print_usage(gedp->ged_result_str);
	return BRLCAD_ERROR;
    }

    /* Positional arguments → source objects.
     * bu_opt_parse returns the count of unrecognised (positional) tokens
     * and places them at av[0..opt_ret-1]. */
    for (int j = 0; j < opt_ret; j++) {
	struct directory *dp;
	GED_DB_LOOKUP(gedp, dp, av[j], LOOKUP_NOISY, BRLCAD_ERROR);
	state->srcs.push_back(dp);
    }
    if (state->srcs.empty()) {
	bu_vls_printf(gedp->ged_result_str, "clone: no object specified\n");
	print_usage(gedp->ged_result_str);
	return BRLCAD_ERROR;
    }

    /* Transfer scalar options */
    if (n_copies_opt > 0) state->n_copies = (size_t)n_copies_opt;
    if (incr_opt > 0) state->incr = incr_opt;
    if (subs_opt.set) {
	state->subs_src = subs_opt.src;
	state->subs_dst = subs_opt.dst;
    }
    if (nx_opt > 0) state->nx = nx_opt;
    if (ny_opt > 0) state->ny = ny_opt;
    if (nz_opt > 0) state->nz = nz_opt;
    state->dx = dx_opt; state->dy = dy_opt; state->dz = dz_opt;
    if (naz_opt > 0) state->naz = naz_opt;
    if (nel_opt > 0) state->nel = nel_opt;
    if (nr_opt  > 0) state->nr  = nr_opt;
    if (nh_opt  > 0) state->nh  = nh_opt;
    state->daz    = daz_opt * DEG2RAD;
    state->del_v  = del_opt * DEG2RAD;
    state->dr     = dr_opt;
    state->dh     = dh_opt;
    state->start_az = start_az_opt * DEG2RAD;
    state->start_el = start_el_opt * DEG2RAD;
    /* For radii/heights: if not explicitly set, default to dr/dh so that
     * the first shell/ring is at dr (matching --rect which starts copies
     * at dx, not 0).  An explicit --start-r 0 or --start-h 0 still works. */
    state->start_r  = (start_r_opt >= 0.0) ? start_r_opt : dr_opt;
    state->start_h  = (start_h_opt >= 0.0) ? start_h_opt : dh_opt;
    state->rotaz    = (rotaz_flag != 0);
    state->rotel    = (rotel_flag != 0);
    state->do_rot   = (rot_flag   != 0);
    state->do_xpush = (xpush_flag != 0);
    if (group_cstr) state->group_name = group_cstr;

    /* Pattern mode */
    if      (mode_rect) state->pattern = PatternMode::RECT;
    else if (mode_sph)  state->pattern = PatternMode::SPHERICAL;
    else if (mode_cyl)  state->pattern = PatternMode::CYL;

    /* Depth mode */
    if (depth_flag >= 0) {
	if (depth_flag == 2)      state->depth = DepthMode::TOP;
	else if (depth_flag == 1) state->depth = DepthMode::REGIONS;
	else                      state->depth = DepthMode::PRIMITIVES;
    } else if (state->pattern != PatternMode::LINEAR) {
	state->depth = DepthMode::TOP;  /* default for all pattern modes */
    }

    /* Unit conversions and resolve -a / -b */
    fastf_t l2b = gedp->dbip->dbi_local2base;

    if (atrans_opt.set) {
	if (atrans_opt.n > 0) {
	    state->n_copies = (size_t)atrans_opt.n;
	    VSCALE(atrans_opt.v, atrans_opt.v, 1.0 / atrans_opt.n);
	}
	VMOVE(trans_opt.v, atrans_opt.v);
	trans_opt.set = true;
    }
    if (arot_opt.set) {
	if (arot_opt.n > 0) {
	    state->n_copies = (size_t)arot_opt.n;
	    VSCALE(arot_opt.v, arot_opt.v, 1.0 / arot_opt.n);
	}
	VMOVE(rot_opt.v, arot_opt.v);
	rot_opt.set = true;
    }

    if (trans_opt.set) {
	VSCALE(trans_opt.v, trans_opt.v, l2b);
	VMOVE(state->trans, trans_opt.v);
	state->trans[W] = 1;
    }
    if (rot_opt.set) {
	VMOVE(state->rot, rot_opt.v);
	state->rot[W] = 1;
    }
    if (rpnt_opt.set) {
	VSCALE(rpnt_opt.v, rpnt_opt.v, l2b);
	VMOVE(state->rpnt, rpnt_opt.v);
	state->rpnt[W] = 1;
    }

    state->miraxis = mirror_opt.axis;
    state->mirpos  = mirror_opt.dist * l2b;

    return BRLCAD_OK;
}


/* -----------------------------------------------------------------------
 * ged_clone_core
 * ----------------------------------------------------------------------- */

int
ged_clone_core(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc == 1) {
	print_usage(gedp->ged_result_str);
	return GED_HELP;
    }

    CloneState state;
    state.gedp = gedp;
    bu_vls_init(&state.olist);

    int ret = clone_parse_args(gedp, argc, argv, &state);
    if (ret != BRLCAD_OK) {
	bu_vls_free(&state.olist);
	return ret;
    }

    /* Retrieve the BU_CLBK_DURING progress callback, if registered.
     * This fires once per top-level clone created so callers can drive
     * progress bars or other per-step feedback.
     * Skip if ged_skip_clbks is set (e.g. we are executing inside a
     * search -exec run) to avoid unsafe GUI side-effects on worker threads. */
    {
	bu_clbk_t clbk = nullptr;
	void *clbk_u2 = nullptr;
	if (!gedp->ged_skip_clbks &&
	    ged_clbk_get(&clbk, &clbk_u2, gedp, "clone", BU_CLBK_DURING)
	    == BRLCAD_OK) {
	    state.clbk    = clbk;
	    state.clbk_u2 = clbk_u2;
	}
	state.clbk_u1 = (void *)gedp;
    }

    if (db_version(gedp->dbip) < 5) {
	bu_vls_printf(gedp->ged_result_str,
		     "clone: v4 databases not supported; "
		     "use 'dbupgrade' to convert first\n");
	bu_vls_free(&state.olist);
	return BRLCAD_ERROR;
    }

    /* --xpush: replace each source with a xpushed temporary */
    std::vector<struct directory *> xpush_tmps;
    if (state.do_xpush) {
	std::vector<struct directory *> new_srcs;
	for (auto *src : state.srcs) {
	    struct directory *tmp = make_xpushed_src(gedp, src);
	    if (tmp) {
		xpush_tmps.push_back(tmp);
		new_srcs.push_back(tmp);
	    } else {
		bu_vls_printf(gedp->ged_result_str,
			     "clone: xpush failed for \"%s\", continuing "
			     "without it\n", src->d_namep);
		new_srcs.push_back(src);
	    }
	}
	state.srcs = new_srcs;
    }

    std::vector<std::string> top_names;

    switch (state.pattern) {
	case PatternMode::LINEAR: {
	    if (state.depth == DepthMode::REGIONS) {
		/* Regions depth in linear mode: copy_regions + apply mat */
		mat_t mat;
		MAT_IDN(mat);
		if (state.miraxis != W) {
		    mat[state.miraxis * 5] = -1.0;
		    mat[3 + state.miraxis * 4] = 2.0 * state.mirpos;
		}
		if (!ZERO(state.trans[W]))
		    MAT_DELTAS_ADD_VEC(mat, state.trans);
		if (!ZERO(state.rot[W])) {
		    mat_t m2, t;
		    bn_mat_angles(m2, state.rot[X], state.rot[Y], state.rot[Z]);
		    if (!ZERO(state.rpnt[W])) {
			mat_t m3;
			bn_mat_xform_about_pnt(m3, m2, state.rpnt);
			bn_mat_mul(t, mat, m3);
		    } else {
			bn_mat_mul(t, mat, m2);
		    }
		    MAT_COPY(mat, t);
		}
		state.clones_total = state.srcs.size();
		state.clones_done  = 0;
		for (auto *src : state.srcs) {
		    std::string n = apply_one_position_regions(&state, src, mat);
		    if (!n.empty()) {
			top_names.push_back(n);
			bu_vls_printf(gedp->ged_result_str, "%s", n.c_str());
			clone_fire_progress(&state, n);
		    }
		}
		bu_vls_printf(gedp->ged_result_str,
			     " {%s}", bu_vls_cstr(&state.olist));
	    } else {
		state.clones_total = 1;
		state.clones_done  = 0;
		struct directory *copy = deep_copy_object(&rt_uniresource, &state);
		if (copy) {
		    top_names.push_back(copy->d_namep);
		    bu_vls_printf(gedp->ged_result_str, "%s", copy->d_namep);
		    clone_fire_progress(&state, copy->d_namep);
		}
		bu_vls_printf(gedp->ged_result_str,
			     " {%s}", bu_vls_cstr(&state.olist));
	    }
	    break;
	}
	case PatternMode::RECT:
	    top_names = run_pattern_rect(&state);
	    for (auto& n : top_names)
		bu_vls_printf(gedp->ged_result_str, "%s ", n.c_str());
	    break;
	case PatternMode::SPHERICAL:
	    top_names = run_pattern_sph(&state);
	    for (auto& n : top_names)
		bu_vls_printf(gedp->ged_result_str, "%s ", n.c_str());
	    break;
	case PatternMode::CYL:
	    top_names = run_pattern_cyl(&state);
	    for (auto& n : top_names)
		bu_vls_printf(gedp->ged_result_str, "%s ", n.c_str());
	    break;
    }

    if (!state.group_name.empty())
	create_group(&state, top_names);

    for (auto *tmp : xpush_tmps) {
	const char *kill_av[3] = { "killtree", tmp->d_namep, nullptr };
	ged_exec_killtree(gedp, 2, kill_av);
    }

    bu_vls_free(&state.olist);
    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_CLONE_COMMANDS(X, XID) \
    X(clone, ged_clone_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_CLONE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_clone", 1, GED_CLONE_COMMANDS)

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
