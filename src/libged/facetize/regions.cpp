/*                     R E G I O N S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file libged/facetize.cpp
 *
 * Logic implementing the per-region facetize mode.
 *
 * Note:  we deliberately manage this somewhat differently from the "convert
 * everything to one BoT" case to minimize the number of subprocesses we have
 * to launch.  For very large models, if we just treat each region like its a
 * complete conversion, we may end up launching too many subprocesses and run
 * into operating system limitations.
 */

#include "common.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <vector>

#include <string.h>

#include "bu/app.h"
#include "bu/path.h"
#include "bu/env.h"
#include "bg/trimesh.h"
#include "rt/db_io.h"
#include "rt/search.h"
#include "raytrace.h"
#include "wdb.h"
#include "../ged_private.h"
#include "./ged_facetize.h"

static const double FACETIZE_RT_EMPTY_TOL = 1.0e-9;

/* Minimum Crofton crossing count for a statistically meaningful SA
 * comparison.  Below this threshold (~1/sqrt(N) noise > 14 %) the
 * estimate is too noisy to distinguish a real mismatch from sampling
 * variance; such results are accepted with a note rather than triggering
 * a perturb retry that would face the same sampling limitation.         */
static const long CROFTON_FEW_HIT_THRESHOLD = 50;

static void
_collect_tree_leaves(union tree *tp, std::set<std::string> &leaves)
{
    if (!tp) return;
    switch (tp->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    _collect_tree_leaves(tp->tr_b.tb_right, leaves);
	    /* fall through */
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    _collect_tree_leaves(tp->tr_b.tb_left, leaves);
	    break;
	case OP_DB_LEAF:
	    leaves.insert(std::string(tp->tr_l.tl_name));
	    break;
	default:
	    break;
    }
}

static bool
_has_perturbable_leaf(struct db_i *dbip, struct directory *dp, std::set<std::string> &visited)
{
    if (!dbip || !dp) return false;
    if (visited.find(dp->d_namep) != visited.end()) return false;
    visited.insert(std::string(dp->d_namep));

    if (!(dp->d_flags & RT_DIR_COMB))
	return (OBJ[dp->d_minor_type].ft_perturb != NULL);

    struct rt_db_internal in;
    RT_DB_INTERNAL_INIT(&in);
    if (rt_db_get_internal(&in, dp, dbip, NULL, &rt_uniresource) < 0)
	return false;

    struct rt_comb_internal *comb = (struct rt_comb_internal *)in.idb_ptr;
    std::set<std::string> leaves;
    if (comb->tree)
	_collect_tree_leaves(comb->tree, leaves);
    rt_db_free_internal(&in);

    for (const auto &lname : leaves) {
	struct directory *ldp = db_lookup(dbip, lname.c_str(), LOOKUP_QUIET);
	if (_has_perturbable_leaf(dbip, ldp, visited))
	    return true;
    }

    return false;
}

static long
_crofton_on_obj(struct db_i *dbip, const char *obj_name, double &out_sa, double &out_vol)
{
    out_sa = out_vol = -1.0;

    struct rt_i *rtip = rt_new_rti(dbip);
    if (!rtip) return -1L;

    if (rt_gettree(rtip, obj_name) != 0) {
	rt_free_rti(rtip);
	return -1L;
    }
    rt_prep_parallel(rtip, 1);

    struct rt_crofton_params crp;
    crp.n_rays = 0;
    crp.stability_mm = 0.05;
    crp.time_ms = 2000.0;

    int cr = rt_crofton_shoot(rtip, &crp, &out_sa, &out_vol);
    rt_free_rti(rtip);
    return (cr >= 0) ? (long)cr : -1L;
}

static int
_bot_metrics(struct db_i *dbip, const char *bot_name, double &out_sa, double &out_vol)
{
    out_sa = out_vol = -1.0;
    struct directory *dp = db_lookup(dbip, bot_name, LOOKUP_QUIET);
    if (!dp || (dp->d_flags & RT_DIR_COMB))
	return -1;

    struct rt_db_internal in;
    RT_DB_INTERNAL_INIT(&in);
    if (rt_db_get_internal(&in, dp, dbip, NULL, &rt_uniresource) < 0)
	return -1;

    int ret = -1;
    if (in.idb_minor_type == DB5_MINORTYPE_BRLCAD_BOT) {
	struct rt_bot_internal *bot = (struct rt_bot_internal *)in.idb_ptr;
	if (bot->mode == RT_BOT_SOLID) {
	    if (bot->num_faces > 0 && bot->num_vertices > 0) {
		out_sa = bg_trimesh_area(bot->faces, bot->num_faces,
			(const point_t *)bot->vertices, bot->num_vertices);
		out_vol = bg_trimesh_volume(bot->faces, bot->num_faces,
			(const point_t *)bot->vertices, bot->num_vertices);
	    } else {
		out_sa = 0.0;
		out_vol = 0.0;
	    }
	    ret = 0;
	}
    }
    rt_db_free_internal(&in);
    return ret;
}

/**
 * Delete any existing object named @p bot_name from @p dbip and write a
 * new zero-face BoT in its place.  Used when Crofton detects that the
 * Boolean evaluation of a region is almost certainly empty (zero ray
 * intersections) so the facetize result should be empty too.
 */
static void
_write_empty_bot(struct db_i *dbip, const char *bot_name, int verbosity)
{
    struct directory *od = db_lookup(dbip, bot_name, LOOKUP_QUIET);
    if (od != RT_DIR_NULL) {
	db_delete(dbip, od);
	db_dirdelete(dbip, od);
    }
    struct rt_bot_internal *ebot;
    BU_GET(ebot, struct rt_bot_internal);
    ebot->magic        = RT_BOT_INTERNAL_MAGIC;
    ebot->mode         = RT_BOT_SOLID;
    ebot->orientation  = RT_BOT_CCW;
    ebot->thickness    = NULL;
    ebot->face_mode    = (struct bu_bitv *)NULL;
    ebot->bot_flags    = 0;
    ebot->num_vertices = 0;
    ebot->num_faces    = 0;
    ebot->vertices     = NULL;
    ebot->faces        = NULL;
    (void)_ged_facetize_write_bot(dbip, ebot, bot_name, verbosity);
}

/* returns 1 on pass, 0 on mismatch, -1 on unavailable/skip */
static int
_validate_csg_vs_bot(struct db_i *csg_dbip, const char *obj_name, struct db_i *bot_dbip, const char *bot_name, double sa_tol_pct, double vol_tol_pct, double *sa_err_pct, double *vol_err_pct);

/* -----------------------------------------------------------------------
 * Perturbed-CSG in-memory db helpers for Pass 2 Crofton validation.
 *
 * Build a fresh in-memory database containing:
 *   - every CSG solid leaf from the original s->dbip region, with variant
 *     leaves replaced by ft_perturb-generated parametric copies (reusing
 *     the exact same perturbation factor recorded in vplan->variant_recs),
 *   - a region comb whose tree mirrors the original but with variant leaf
 *     names substituted.
 *
 * Crofton then raytraces against true parametric CSG geometry for the Pass 2
 * reference, rather than against BoTs or the unperturbed original.
 * ----------------------------------------------------------------------- */

struct PerturCsgCtx {
    struct db_i               *src_dbip;     /* original .g — read-only source */
    struct rt_wdb             *inmem_wdbp;   /* destination in-memory db */
    const FacetizeVariantPlan *vplan;
    std::vector<std::string>   path_stack;
    std::set<std::string>      written;      /* names already written to inmem */
};

/* Forward declarations (mutually recursive). */
static union tree  *_pcsg_copy_tree(PerturCsgCtx &ctx, union tree *tp, bool in_sub);
static bool         _pcsg_make_comb(PerturCsgCtx &ctx, const char *comb_name, bool in_sub);

static union tree *
_pcsg_copy_tree(PerturCsgCtx &ctx, union tree *tp, bool in_sub)
{
    if (!tp) return NULL;

    union tree *nt;
    BU_ALLOC(nt, union tree);
    RT_TREE_INIT(nt);
    nt->tr_op = tp->tr_op;

    switch (tp->tr_op) {
    case OP_DB_LEAF: {
	const char *leaf = tp->tr_l.tl_name;
	struct directory *ldp = db_lookup(ctx.src_dbip, leaf, LOOKUP_QUIET);
	std::string use_name = leaf;

	if (ldp && (ldp->d_flags & RT_DIR_COMB)) {
	    /* Intermediate comb: recurse, writing it into the inmem db. */
	    _pcsg_make_comb(ctx, leaf, in_sub);
	    /* Use the same name in the inmem db. */
	} else {
	    /* Solid leaf: check for a variant. */
	    std::string path_key;
	    for (const auto &seg : ctx.path_stack)
		path_key += "/" + seg;
	    path_key += "/" + std::string(leaf);
	    std::string role_key = path_key + (in_sub ? "#sub" : "#base");
	    auto it = ctx.vplan->inst_to_variant.find(role_key);

	    if (it != ctx.vplan->inst_to_variant.end()) {
		/* Variant exists: recreate the perturbed CSG from src_dbip. */
		const std::string &vname = it->second;
		use_name = vname;
		if (ctx.written.find(vname) == ctx.written.end()) {
		    auto rec_it = ctx.vplan->variant_recs.find(vname);
		    if (rec_it != ctx.vplan->variant_recs.end() && ldp) {
			struct rt_db_internal src_intern;
			RT_DB_INTERNAL_INIT(&src_intern);
			if (rt_db_get_internal(&src_intern, ldp, ctx.src_dbip,
					       NULL, &rt_uniresource) >= 0) {
			    int ptype = src_intern.idb_type;
			    struct rt_db_internal *var_intern = NULL;
			    bool ok = false;
			    if (OBJ[ptype].ft_perturb &&
				OBJ[ptype].ft_perturb(&var_intern, &src_intern, 0,
						      rec_it->second.factor) == BRLCAD_OK &&
				var_intern) {
				if (wdb_put_internal(ctx.inmem_wdbp, vname.c_str(),
						     var_intern, 1.0) >= 0)
				    ok = true;
				/* wdb_put_internal frees var_intern's idb_ptr;
				 * we still need to free the struct itself. */
				BU_PUT(var_intern, struct rt_db_internal);
			    }
			    if (!ok) {
				/* Fallback: write original CSG under variant name. */
				if (wdb_put_internal(ctx.inmem_wdbp, vname.c_str(),
						     &src_intern, 1.0) >= 0)
				    ok = true;
				/* src_intern freed by wdb_put_internal */
			    } else {
				rt_db_free_internal(&src_intern);
			    }
			    if (ok) ctx.written.insert(vname);
			}
		    } else {
			/* No variant record — fall back to original name. */
			use_name = leaf;
		    }
		}
	    }

	    /* Ensure the (possibly original) leaf exists in the inmem db. */
	    if (ctx.written.find(use_name) == ctx.written.end()) {
		struct directory *udp =
		    db_lookup(ctx.src_dbip, use_name.c_str(), LOOKUP_QUIET);
		if (udp) {
		    struct rt_db_internal leaf_intern;
		    RT_DB_INTERNAL_INIT(&leaf_intern);
		    if (rt_db_get_internal(&leaf_intern, udp, ctx.src_dbip,
					   NULL, &rt_uniresource) >= 0) {
			if (wdb_put_internal(ctx.inmem_wdbp, use_name.c_str(),
					     &leaf_intern, 1.0) >= 0)
			    ctx.written.insert(use_name);
			/* leaf_intern freed by wdb_put_internal */
		    }
		}
	    }
	}

	nt->tr_l.tl_name = bu_strdup(use_name.c_str());
	if (tp->tr_l.tl_mat) {
	    nt->tr_l.tl_mat = (matp_t)bu_malloc(sizeof(mat_t), "tl_mat cp");
	    MAT_COPY(nt->tr_l.tl_mat, tp->tr_l.tl_mat);
	}
	break;
    }
    case OP_UNION:
    case OP_INTERSECT:
	nt->tr_b.tb_left  = _pcsg_copy_tree(ctx, tp->tr_b.tb_left,  in_sub);
	nt->tr_b.tb_right = _pcsg_copy_tree(ctx, tp->tr_b.tb_right, in_sub);
	break;
    case OP_SUBTRACT:
	/* Right child of subtraction is always subtractive, regardless of
	 * the outer context, so pass in_sub=true for the right branch. */
	nt->tr_b.tb_left  = _pcsg_copy_tree(ctx, tp->tr_b.tb_left,  in_sub);
	nt->tr_b.tb_right = _pcsg_copy_tree(ctx, tp->tr_b.tb_right, true);
	break;
    case OP_NOT:
    case OP_GUARD:
    case OP_XNOP:
	nt->tr_b.tb_left = _pcsg_copy_tree(ctx, tp->tr_b.tb_left, in_sub);
	break;
    default:
	break;
    }
    return nt;
}

static bool
_pcsg_make_comb(PerturCsgCtx &ctx, const char *comb_name, bool in_sub)
{
    if (ctx.written.find(std::string(comb_name)) != ctx.written.end())
	return true;  /* already written */

    struct directory *dp = db_lookup(ctx.src_dbip, comb_name, LOOKUP_QUIET);
    if (!dp) return false;

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, ctx.src_dbip, NULL, &rt_uniresource) < 0)
	return false;

    struct rt_comb_internal *orig = (struct rt_comb_internal *)intern.idb_ptr;

    ctx.path_stack.push_back(std::string(comb_name));
    union tree *new_tree = _pcsg_copy_tree(ctx, orig->tree, in_sub);
    ctx.path_stack.pop_back();

    rt_db_free_internal(&intern);

    struct rt_comb_internal *new_comb;
    BU_ALLOC(new_comb, struct rt_comb_internal);
    RT_COMB_INTERNAL_INIT(new_comb);
    new_comb->tree = new_tree;

    struct rt_db_internal new_intern;
    RT_DB_INTERNAL_INIT(&new_intern);
    new_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    new_intern.idb_type       = ID_COMBINATION;
    new_intern.idb_ptr        = (void *)new_comb;
    new_intern.idb_meth       = &OBJ[ID_COMBINATION];

    bool ok = (wdb_put_internal(ctx.inmem_wdbp, comb_name,
				&new_intern, 1.0) >= 0);
    /* wdb_put_internal frees new_intern (including new_comb + new_tree). */
    if (ok) ctx.written.insert(std::string(comb_name));
    return ok;
}

/**
 * Build a fresh in-memory database containing a perturbed-CSG copy of
 * @a region_name from @a src_dbip.  Solid leaves that have a variant record
 * in @a vplan are replaced by ft_perturb-generated parametric copies using
 * the exact factor stored in vplan->variant_recs; all other leaves are
 * copied verbatim from @a src_dbip.
 *
 * The region comb is written under its original name, so callers pass that
 * name directly to _validate_csg_vs_bot().
 *
 * Returns an allocated struct db_i * on success (caller must db_close() it),
 * or NULL on failure.
 */
static struct db_i *
_create_perturbed_csg_db(struct db_i *src_dbip, const char *region_name,
			 const FacetizeVariantPlan *vplan)
{
    if (!src_dbip || !region_name || !vplan) return NULL;

    struct db_i *inmem_dbip = db_create_inmem();
    if (!inmem_dbip) return NULL;

    struct rt_wdb *inmem_wdbp = wdb_dbopen(inmem_dbip, RT_WDB_TYPE_DB_INMEM);
    if (!inmem_wdbp) { db_close(inmem_dbip); return NULL; }

    PerturCsgCtx ctx;
    ctx.src_dbip    = src_dbip;
    ctx.inmem_wdbp  = inmem_wdbp;
    ctx.vplan       = vplan;

    if (!_pcsg_make_comb(ctx, region_name, false)) {
	db_close(inmem_dbip);
	return NULL;
    }
    return inmem_dbip;
}

static int
_validate_csg_vs_bot(struct db_i *csg_dbip, const char *obj_name, struct db_i *bot_dbip, const char *bot_name, double sa_tol_pct, double vol_tol_pct, double *sa_err_pct, double *vol_err_pct)
{
    /* Return codes:
     *   1  PASS:      SA and volume within tolerance.
     *   0  MISMATCH:  outside tolerance — trigger perturb retry.
     *  -1  ERROR:     Crofton prep or BoT metric failure.
     *   2  FEW_HIT:   sampler found 1..CROFTON_FEW_HIT_THRESHOLD-1 crossings —
     *                 too few for a meaningful comparison; accept with note.
     *   3  ZERO_HIT:  sampler found zero crossings for a non-empty BoT —
     *                 the BoT output is suspect; warn user to inspect.       */
    double csa = -1.0, cvol = -1.0, bsa = -1.0, bvol = -1.0;
    long csg_crossings = _crofton_on_obj(csg_dbip, obj_name, csa, cvol);
    if (csg_crossings < 0)
	return -1;
    if (_bot_metrics(bot_dbip, bot_name, bsa, bvol) != 0)
	return -1;

    /* --- Empty-BoT case: pass iff the CSG also read as empty ---------- */
    if (std::fabs(bsa) <= FACETIZE_RT_EMPTY_TOL && std::fabs(bvol) <= FACETIZE_RT_EMPTY_TOL) {
	bool csg_empty = (csg_crossings == 0);
	*sa_err_pct  = csg_empty ? 0.0 : 100.0;
	*vol_err_pct = csg_empty ? 0.0 : 100.0;
	return csg_empty ? 1 : 0;
    }

    /* --- Non-empty BoT: examine Crofton hit count --------------------- */

    /* Zero crossings: the sampler fired rays but hit nothing.  The BoT is
     * non-empty, which is suspicious — either the CSG has genuinely been
     * reduced to nothing (e.g. a subtractor fully engulfs the base) and
     * the BoT contains leftover noise triangles, or the geometry is so
     * extreme that not a single stochastic chord hit it.  Either outcome
     * warrants user inspection rather than silent acceptance.            */
    if (csg_crossings == 0) {
	*sa_err_pct  = 100.0;
	*vol_err_pct = 100.0;
	return 3;
    }

    /* Few crossings: the sampler did find geometry but the count is below
     * the threshold at which the SA estimate is statistically reliable
     * (~1/sqrt(N) noise > 14 %).  A perturb retry would face the same
     * sampling limitation, so we accept the BoT with a note instead.    */
    if (csg_crossings < CROFTON_FEW_HIT_THRESHOLD) {
	*sa_err_pct  = 100.0;
	*vol_err_pct = 100.0;
	return 2;
    }

    /* Normal case: enough crossings for a reliable SA/volume estimate.   */
    double sa_err  = (bsa > 0.0) ? std::fabs(csa - bsa) / bsa : 1.0;
    double vol_err = (bvol > 0.0) ? std::fabs(cvol - bvol) / bvol : 1.0;
    *sa_err_pct  = sa_err  * 100.0;
    *vol_err_pct = vol_err * 100.0;
    return (sa_err > sa_tol_pct || vol_err > vol_tol_pct) ? 0 : 1;
}

int
_ged_facetize_regions(struct _ged_facetize_state *s, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    struct db_i *dbip = s->dbip;
    struct bu_list *vlfree = &rt_vlfree;

    /* Convert percentage thresholds (0–100) to fractions (0–1) once. */
    const double perturb_sa_frac  = s->perturb_sa_tol  / 100.0;
    const double perturb_vol_frac = s->perturb_vol_tol / 100.0;

    /* Validation outcome counters — accumulated across all regions. */
    int vcnt_skip         = 0; /* skipped: no perturbable leaf */
    int vcnt_total        = 0; /* entered validation path */
    int vcnt_p1_pass      = 0; /* P1 MATCH (within threshold) */
    int vcnt_few_hit      = 0; /* P1: few Crofton hits — accepted with note (sub-mm geometry found) */
    int vcnt_zero_hit     = 0; /* P1: zero Crofton hits — non-empty BoT is suspicious */
    int vcnt_p1_trigger   = 0; /* P1 MISMATCH (triggered perturb retry) */
    int vcnt_p2_pass      = 0; /* P2 MATCH after perturb */
    int vcnt_p2_topoflip  = 0; /* P2: Crofton-zero/few after perturb (perturb shifted topology) */
    int vcnt_p2_warn      = 0; /* P2 persistent validation mismatch */
    int vcnt_unavail      = 0; /* validation unavailable (metric/prep failure) */

    /* Used the libged tolerances */
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DEFAULT);
    s->tol = &(wdbp->wdb_ttol);

    if (!argc) return BRLCAD_ERROR;

    struct directory **dpa = (struct directory **)bu_calloc(argc, sizeof(struct directory *), "dp array");
    int newobjcnt = _ged_sort_existing_objs(dbip, argc, argv, dpa);
    if (newobjcnt != 1) {
	if (!newobjcnt)
	    bu_vls_printf(s->gedp->ged_result_str, "Need non-existent output comb name.");
	if (newobjcnt)
	    bu_vls_printf(s->gedp->ged_result_str, "More than one non-existent object specified in region processing mode, aborting.");
	return BRLCAD_ERROR;
    }

    const char *oname = argv[argc-1];
    argc--;

    // Before we go any further, see if we actually have regions in the
    // specified input(s).
    const char *active_regions = "( -type r ! -below -type r )";
    struct bu_ptbl *ar = NULL;
    BU_ALLOC(ar, struct bu_ptbl);
    if (db_search(ar, DB_SEARCH_RETURN_UNIQ_DP, active_regions, argc, dpa, dbip, NULL, NULL, NULL) < 0) {
	if (s->verbosity >= 0) {
	    bu_log("regions.cpp:%d Problem searching for active regions - aborting.\n", __LINE__);
	}
	bu_ptbl_free(ar);
	bu_free(ar, "ar table");
	bu_free(dpa, "free dpa");
	return BRLCAD_OK;
    }

    // If we have none, just treat this as a normal facetize operation.
    if (!BU_PTBL_LEN(ar)) {
	bu_ptbl_free(ar);
	bu_free(ar, "ar table");

	/* If we're doing an NMG output, use the old-school libnmg booleval */
	if (s->make_nmg) {
	    if (!s->in_place) {
		ret = _ged_facetize_nmgeval(s, argc, argv, oname);
		bu_free(dpa, "dpa");
		return ret;
	    } else {
		for (int i = 0; i < argc; i++) {
		    const char *av[2];
		    av[0] = argv[i];
		    av[1] = NULL;
		    ret = _ged_facetize_nmgeval(s, 1, av, argv[i]);
		    if (ret == BRLCAD_ERROR) {
			bu_free(dpa, "dpa");
			return ret;
		    }
		}
		bu_free(dpa, "dpa");
		return ret;
	    }
	}

	// If we're not doing NMG, use the Manifold booleval
	if (!s->in_place) {
	    ret = _ged_facetize_booleval(s, argc, dpa, oname, false, false);
	} else {
	    for (int i = 0; i < argc; i++) {
		struct directory *idpa[2];
		idpa[0] = dpa[i];
		idpa[1] = NULL;
		ret = _ged_facetize_booleval(s, 1, (struct directory **)idpa, argv[i], false, false);
		if (ret == BRLCAD_ERROR) {
		    bu_free(dpa, "dpa");
		    return ret;
		}
	    }
	}

	// Report on the primitive processing
	facetize_primitives_summary(s);

	// After collecting info for summary, we can now clean up working files
	bu_dirclear(s->wdir);

	// Cleanup
	bu_free(dpa, "dpa");
	return ret;
    }

    // We've got something warranting region processiong. For the working file
    // setup, we need to check the solids to see if any supporting files need
    // to be copied
    const char *active_solids = "! -type comb";
    struct bu_ptbl *as = NULL;
    BU_ALLOC(as, struct bu_ptbl);
    if (db_search(as, DB_SEARCH_RETURN_UNIQ_DP, active_solids, argc, dpa, dbip, NULL, NULL, NULL) < 0) {
	if (s->verbosity >= 0) {
	    bu_log("regions.cpp:%d Problem searching for active solids - aborting.\n", __LINE__);
	}
	bu_ptbl_free(as);
	bu_free(as, "as table");
	bu_ptbl_free(ar);
	bu_free(ar, "ar table");
	bu_free(dpa, "free dpa");
	return BRLCAD_OK;
    }
    if (!BU_PTBL_LEN(as)) {
	/* No active solids (unlikely but technically possible), nothing to do */
	bu_ptbl_free(as);
	bu_free(as, "as table");
	bu_ptbl_free(ar);
	bu_free(ar, "ar table");
	bu_free(dpa, "free dpa");
	return BRLCAD_OK;
    }

    char kfname[MAXPATHLEN];
    char tmpwfile[MAXPATHLEN];

    /* Figure out the working .g filename */
    struct bu_vls wfilename = BU_VLS_INIT_ZERO;
    // Hash the path string and construct
    unsigned long long hash_num = bu_data_hash((void *)bu_vls_cstr(s->bname), bu_vls_strlen(s->bname));
    bu_vls_sprintf(&wfilename, "facetize_regions_%s_%llu", bu_vls_cstr(s->bname), hash_num);

    // Have filename, get a location in the cache directory
    bu_dir(tmpwfile, MAXPATHLEN, s->wdir, bu_vls_cstr(&wfilename), NULL);
    bu_vls_sprintf(s->wfile, "%s", tmpwfile);
    bu_vls_printf(&wfilename, "_keep");
    bu_dir(kfname, MAXPATHLEN, s->wdir, bu_vls_cstr(&wfilename), NULL);
    bu_vls_free(&wfilename);

    // Set up working file.  We will reuse this for each region->bot conversion.
    // We pass in the list of all active solids so any necessary supporting data
    // files also get copied over.
    if (_ged_facetize_working_file_setup(s, as) != BRLCAD_OK) {
	if (s->verbosity >= 0) {
	    bu_log("regions.cpp:%d Failed to set up working file - aborting.\n", __LINE__);
	}
	bu_ptbl_free(as);
	bu_free(as, "as table");
	bu_ptbl_free(ar);
	bu_free(ar, "ar table");
	bu_free(dpa, "free dpa");
	return BRLCAD_ERROR;
    }

    // We need all the solids converted
    if (!s->make_nmg && !s->nmg_booleval) {
	/* Instance-aware adjust planning pass: walk each *region* root to
	 * find every leaf that appears in both union and subtract roles, create
	 * perturbed CSG copies in the working .g *before* tessellation so that
	 * the adjusted variants are triangulated from their original CSG
	 * parameter definitions.  The plan is stored on the state so that
	 * _booltree_leaf_tess can substitute the correct variant BoT during
	 * each per-region booleval.
	 *
	 * We deliberately walk from the region roots (ar) rather than from the
	 * top-level input objects (dpa).  The per-region booleval also starts
	 * each db_walk_tree from the region root, so the path strings that
	 * db_path_to_string() produces during booleval (e.g.
	 * "/r.wind9/s.wind9.i") match the keys recorded here.  Walking from
	 * dpa would produce longer keys (e.g.
	 * "/havoc/havoc_front/.../r.wind9/s.wind9.i") that never match.
	 *
	 * Skip when --no-perturb is set. */
	if (!s->no_perturb) {
	    size_t nregions = BU_PTBL_LEN(ar);
	    struct directory **rdpa = (struct directory **)bu_calloc(
		nregions + 1, sizeof(struct directory *), "rdpa");
	    for (size_t ri = 0; ri < nregions; ri++)
		rdpa[ri] = (struct directory *)BU_PTBL_GET(ar, ri);
	    FacetizeVariantPlan *vplan =
		_ged_facetize_build_variant_plan(s, (int)nregions, rdpa);
	    bu_free(rdpa, "rdpa");
	    s->variant_plan = (void *)vplan;
	}

	if (_ged_facetize_leaves_tri(s, dbip, as)) {
	    if (s->verbosity >= 0) {
		bu_log("regions.cpp:%d Failed to tessellate all solids - aborting.\n", __LINE__);
	    }
	    if (s->variant_plan) {
		delete (FacetizeVariantPlan *)s->variant_plan;
		s->variant_plan = NULL;
	    }
	    bu_ptbl_free(as);
	    bu_free(as, "as table");
	    bu_ptbl_free(ar);
	    bu_free(ar, "ar table");
	    bu_free(dpa, "free dpa");
	    return BRLCAD_ERROR;
	}
    }

    // Done with solids table
    bu_ptbl_free(as);
    bu_free(as, "as table");

    // If we're going to be doing NMG outputs or NMG booleans,
    // we'll need to have a facetize_state container that has
    // info for the working .g, rather than the parent.  Set
    // up accordingly.
    struct _ged_facetize_state nmg_wstate;
    nmg_wstate.verbosity = s->verbosity;
    nmg_wstate.no_empty = s->no_empty;
    nmg_wstate.make_nmg = s->make_nmg;
    nmg_wstate.nonovlp_brep = s->nonovlp_brep;
    nmg_wstate.no_fixup= s->no_fixup;
    nmg_wstate.no_perturb = s->no_perturb;
    nmg_wstate.use_variant_plan = s->use_variant_plan;
    nmg_wstate.wdir = s->wdir;
    nmg_wstate.wfile = s->wfile;
    nmg_wstate.bname = s->bname;
    nmg_wstate.log_file = s->log_file;
    nmg_wstate.lfile = s->lfile;
    nmg_wstate.regions = s->regions;
    nmg_wstate.resume = s->resume;
    nmg_wstate.in_place = s->in_place;
    nmg_wstate.nmg_booleval = s->nmg_booleval;
    nmg_wstate.max_time = s->max_time;
    nmg_wstate.max_pnts = s->max_pnts;
    nmg_wstate.prefix = s->prefix;
    nmg_wstate.suffix = s->suffix;
    nmg_wstate.tol = s->tol;
    nmg_wstate.nonovlp_threshold = s->nonovlp_threshold;
    nmg_wstate.solid_suffix = s->solid_suffix;
    nmg_wstate.dbip = NULL;

    // If we have any solids in the hierarchies with only combs above them,
    // they are "implicit" regions and must be facetized individually.
    const char *implicit_regions = "( ! -below -type r ! -type comb )";
    struct bu_ptbl *ir = NULL;
    BU_ALLOC(ir, struct bu_ptbl);
    if (db_search(ir, DB_SEARCH_RETURN_UNIQ_DP, implicit_regions, argc, dpa, dbip, NULL, NULL, NULL) < 0) {
	if (s->verbosity >= 0) {
	    bu_log("Problem searching for implicit regions - aborting.\n");
	}
	bu_ptbl_free(ar);
	bu_free(ar, "ar table");
	bu_free(dpa, "free dpa");
	if (nmg_wstate.dbip)
	    db_close(nmg_wstate.dbip);
	return BRLCAD_OK;
    }
    if (BU_PTBL_LEN(ir)) {
	if (s->make_nmg || s->nmg_booleval) {
	    for (size_t i = 0; i < BU_PTBL_LEN(ir); i++) {
		struct directory *idp = (struct directory *)BU_PTBL_GET(ir, i);
		char *obj_name = bu_strdup(idp->d_namep);
		struct db_i *wdbip = db_open(bu_vls_cstr(s->wfile), DB_OPEN_READWRITE);
		db_dirbuild(wdbip);
		db_update_nref(wdbip, &rt_uniresource);
		nmg_wstate.dbip = wdbip;
		int nret = _ged_facetize_nmgeval(s, 1, (const char **)&obj_name, obj_name);
		if (nret != BRLCAD_OK) {
		    if (s->verbosity >= 0)
			bu_log("regions.cpp:%d Failed to process %s.\n", __LINE__, obj_name);
		    bu_ptbl_free(ir);
		    bu_free(ir, "ir table");
		    bu_ptbl_free(ar);
		    bu_free(ar, "ar table");
		    bu_free(obj_name, "obj_name");
		    bu_free(dpa, "free dpa");
		    return BRLCAD_ERROR;
		}
		bu_free(obj_name, "obj_name");
		db_close(wdbip);
	    }
	}
    }

    /* Build the list of roots to evaluate to BoTs:
     *  - active regions
     *  - implicit non-region solids when not in NMG mode */
    struct bu_ptbl eval_roots = BU_PTBL_INIT_ZERO;
    std::set<std::string> eval_names;
    for (size_t i = 0; i < BU_PTBL_LEN(ar); i++) {
	struct directory *dp = (struct directory *)BU_PTBL_GET(ar, i);
	eval_names.insert(std::string(dp->d_namep));
	bu_ptbl_ins(&eval_roots, (long *)dp);
    }
    if (!s->make_nmg && !s->nmg_booleval) {
	for (size_t i = 0; i < BU_PTBL_LEN(ir); i++) {
	    struct directory *dp = (struct directory *)BU_PTBL_GET(ir, i);
	    if (eval_names.find(std::string(dp->d_namep)) == eval_names.end()) {
		eval_names.insert(std::string(dp->d_namep));
		bu_ptbl_ins(&eval_roots, (long *)dp);
	    }
	}
    }

    FacetizeVariantPlan *vplan = (FacetizeVariantPlan *)s->variant_plan;
    bool variant_meshes_ready = false;
    /* Region mode starts with the baseline BoT path and only enables/tessellates
     * variants if Pass 1 validation says a perturb retry is needed. */
    if (!s->make_nmg && !s->nmg_booleval && !s->no_perturb)
	s->use_variant_plan = 0;

    // For evaluated roots, reduce each CSG tree to a single BoT/NMG result and
    // place that result back into the working hierarchy.
    struct bu_vls bname = BU_VLS_INIT_ZERO;
    for (size_t i = 0; i < BU_PTBL_LEN(&eval_roots); i++) {
	struct directory *dpw[2] = {NULL};
	dpw[0] = (struct directory *)BU_PTBL_GET(&eval_roots, i);

	facetize_log(s, 0, "Processing %s\n", dpw[0]->d_namep);

	// Get a name for the region's output BoT
	if (s->make_nmg) {
	    bu_vls_sprintf(&bname, "%s.nmg", dpw[0]->d_namep);
	} else {
	    bu_vls_sprintf(&bname, "%s.bot", dpw[0]->d_namep);
	}

	struct db_i *wdbip = db_open(bu_vls_cstr(s->wfile), DB_OPEN_READWRITE);
	wdbip->dbi_read_only = 1;
	db_dirbuild(wdbip);
	db_update_nref(wdbip, &rt_uniresource);
	struct directory *dcheck = db_lookup(wdbip, bu_vls_cstr(&bname), LOOKUP_QUIET);
	if (dcheck != RT_DIR_NULL)
	    bu_vls_incr(&bname, NULL, NULL, &_db_uniq_test, (void *)wdbip);
	wdbip->dbi_read_only = 0;
	nmg_wstate.dbip = wdbip;

	int bret = BRLCAD_OK;
	if (s->make_nmg || s->nmg_booleval) {
	    char *obj_name = bu_strdup(dpw[0]->d_namep);
	    bret = _ged_facetize_nmgeval(&nmg_wstate, 1, (const char **)&obj_name, bu_vls_cstr(&bname));
	    bu_free(obj_name, "obj_name");
	    db_close(wdbip);
	} else {
	    // Need wdbp in the next two stages for tolerances
	    struct rt_wdb *wwdbp = wdb_dbopen(wdbip, RT_WDB_TYPE_DB_DEFAULT);
	    char *obj_name = bu_strdup(dpw[0]->d_namep);
	    bret = _ged_facetize_booleval_tri(s, wdbip, wwdbp, 1, (const char **)&obj_name, bu_vls_cstr(&bname), vlfree, 1);
	    bu_free(obj_name, "obj_name");

	    bool can_validate = false;
	    if (!s->no_perturb) {
		std::set<std::string> visited;
		can_validate = _has_perturbable_leaf(s->dbip, dpw[0], visited);
	    }
	    if (!s->no_perturb && !can_validate) {
		vcnt_skip++;
		if (s->verbosity > 0)
		    bu_log("FACETIZE: %s has no ft_perturb-capable leaves; skipping raytrace validation\n", dpw[0]->d_namep);
	    }

	    if (bret == BRLCAD_OK && !s->no_perturb && can_validate) {
		vcnt_total++;
		double sa_err_pct = -1.0, vol_err_pct = -1.0;
		int vret = _validate_csg_vs_bot(s->dbip, dpw[0]->d_namep, wdbip, bu_vls_cstr(&bname),
			perturb_sa_frac, perturb_vol_frac,
			&sa_err_pct, &vol_err_pct);
		if (vret == 1) {
		    vcnt_p1_pass++;
		    bu_log("FACETIZE: %s CSG vs BoT MATCH (SA_err=%.2f%% VOL_err=%.2f%%) - skipping perturb\n",
			    dpw[0]->d_namep, sa_err_pct, vol_err_pct);
		}
		/* Return code 2: Crofton found a few crossings (1 to
		 * CROFTON_FEW_HIT_THRESHOLD-1) but not enough for a
		 * statistically reliable SA estimate.  The geometry is real
		 * but very small.  Accept the BoT and skip perturb retry.    */
		if (vret == 2) {
		    vcnt_few_hit++;
		    bu_log("FACETIZE NOTE: %s Crofton found very few ray intersections with CSG geometry (sub-mm or near-degenerate); BoT accepted - verify modeling intent\n",
			    dpw[0]->d_namep);
		}
		/* Return code 3: Crofton found zero crossings for a non-empty
		 * BoT.  With zero ray intersections the CSG Boolean evaluation
		 * most likely produces nothing (e.g. a subtractor fully engulfs
		 * the base).  Replace the suspect non-empty BoT with an empty
		 * one to match the expected raytrace behavior.                   */
		if (vret == 3) {
		    vcnt_zero_hit++;
		    bu_log("FACETIZE: %s Crofton found zero ray intersections with CSG geometry; Boolean eval likely empty - replacing BoT with empty\n",
			    dpw[0]->d_namep);
		    if (!s->no_empty)
			_write_empty_bot(wdbip, bu_vls_cstr(&bname), s->verbosity);
		}
		if (vret == 0) {
		    vcnt_p1_trigger++;
		    bu_log("FACETIZE: %s CSG vs BoT MISMATCH (SA_err=%.2f%% VOL_err=%.2f%%) - triggering perturb\n",
			    dpw[0]->d_namep, sa_err_pct, vol_err_pct);
		    bool reopened_wdb = false;
		    if (vplan && !variant_meshes_ready) {
			if (!vplan->variant_names.empty())
			    _ged_facetize_tessellate_variant_names(s, vplan);
			variant_meshes_ready = true;
			reopened_wdb = true;
		    }
		    if (reopened_wdb) {
			db_close(wdbip);
			wdbip = db_open(bu_vls_cstr(s->wfile), DB_OPEN_READWRITE);
			if (!wdbip) {
			    bret = BRLCAD_ERROR;
			    break;
			}
			db_dirbuild(wdbip);
			db_update_nref(wdbip, &rt_uniresource);
			wwdbp = wdb_dbopen(wdbip, RT_WDB_TYPE_DB_DEFAULT);
		    }

		    s->use_variant_plan = 1;
		    struct directory *od = db_lookup(wdbip, bu_vls_cstr(&bname), LOOKUP_QUIET);
		    if (od != RT_DIR_NULL) {
			db_delete(wdbip, od);
			db_dirdelete(wdbip, od);
		    }
		    char *obj_name_retry = bu_strdup(dpw[0]->d_namep);
		    bret = _ged_facetize_booleval_tri(s, wdbip, wwdbp, 1, (const char **)&obj_name_retry, bu_vls_cstr(&bname), vlfree, 1);
		    bu_free(obj_name_retry, "obj_name_retry");
		    s->use_variant_plan = 0;

		    if (bret == BRLCAD_OK) {
			double sa_err2 = -1.0, vol_err2 = -1.0;
			/* Compare the perturbed BoT (exact bg_trimesh metrics)
			 * against a Crofton raytrace of the perturbed CSG: build
			 * a fresh in-memory db whose leaves are ft_perturb-
			 * generated parametric CSG copies from s->dbip (the
			 * exact same factor stored in vplan->variant_recs).
			 * Fall back to the original-CSG reference if the db
			 * cannot be built (e.g. no vplan or all primitives lack
			 * ft_perturb support). */
			struct db_i *perturb_dbip = NULL;
			if (vplan)
			    perturb_dbip = _create_perturbed_csg_db(
				s->dbip, dpw[0]->d_namep, vplan);
			struct db_i *csg_ref_dbip =
			    perturb_dbip ? perturb_dbip : s->dbip;
			int vret2 = _validate_csg_vs_bot(
			    csg_ref_dbip, dpw[0]->d_namep,
			    wdbip, bu_vls_cstr(&bname),
			    perturb_sa_frac, perturb_vol_frac,
			    &sa_err2, &vol_err2);
			if (perturb_dbip) db_close(perturb_dbip);
			if (vret2 == 1) {
			    vcnt_p2_pass++;
			    bu_log("FACETIZE: %s perturbed CSG vs BoT MATCH (SA_err=%.2f%% VOL_err=%.2f%%) - perturb successful\n",
				    dpw[0]->d_namep, sa_err2, vol_err2);
			}
			/* Return code 2 at P2: few Crofton crossings for
			 * perturbed CSG — too noisy to compare, but the sampler
			 * found geometry.  Accept the perturbed BoT with a note
			 * (topology may have shifted slightly).                 */
			if (vret2 == 2) {
			    vcnt_p2_topoflip++;
			    bu_log("FACETIZE NOTE: %s Crofton found very few crossings for perturbed CSG; perturb may have shifted geometry - check output\n",
				    dpw[0]->d_namep);
			}
			/* Return code 3 at P2: zero Crofton crossings for
			 * perturbed CSG.  The perturb shifted the geometry so
			 * the parametric CSG appears empty (subtractor likely
			 * now fully engulfs the base).  Replace the perturbed
			 * BoT with an empty one to match raytrace behaviour.   */
			if (vret2 == 3) {
			    vcnt_zero_hit++;
			    bu_log("FACETIZE: %s Crofton found zero crossings for perturbed CSG; Boolean eval likely empty after perturb - replacing BoT with empty\n",
				    dpw[0]->d_namep);
			    if (!s->no_empty)
				_write_empty_bot(wdbip, bu_vls_cstr(&bname), s->verbosity);
			}
			if (vret2 == 0) {
			    vcnt_p2_warn++;
			    bu_log("FACETIZE WARNING: %s persistent validation mismatch after perturb retry (SA_err=%.2f%% VOL_err=%.2f%%) - check output geometry with 'lint'\n",
				    dpw[0]->d_namep, sa_err2, vol_err2);
			}
			if (vret2 < 0) {
			    vcnt_unavail++;
			    if (s->verbosity > 0)
				bu_log("FACETIZE: validation unavailable after perturb retry for %s\n", dpw[0]->d_namep);
			}
		    }
		}
		if (vret < 0) {
		    vcnt_unavail++;
		    if (s->verbosity > 0)
			bu_log("FACETIZE: validation unavailable for %s (crofton/metric prep failure)\n", dpw[0]->d_namep);
		}
	    }
	    db_close(wdbip);
	}

	if (bret != BRLCAD_OK) {
	    if (s->verbosity >= 0)
		bu_log("regions.cpp:%d Failed to generate %s.\n", __LINE__, bu_vls_cstr(&bname));
	    if (s->variant_plan) {
		delete (FacetizeVariantPlan *)s->variant_plan;
		s->variant_plan = NULL;
	    }
	    bu_ptbl_free(&eval_roots);
	    bu_ptbl_free(ir);
	    bu_free(ir, "ir table");
	    bu_ptbl_free(ar);
	    bu_free(ar, "ar table");
	    bu_vls_free(&bname);
	    bu_free(dpa, "free dpa");
	    return BRLCAD_ERROR;
	}

	// Replace comb roots with their evaluated BoT/NMG or swap primitive roots.
	wdbip = db_open(bu_vls_cstr(s->wfile), DB_OPEN_READWRITE);
	db_dirbuild(wdbip);
	db_update_nref(wdbip, &rt_uniresource);
	struct directory *wdp = db_lookup(wdbip, dpw[0]->d_namep, LOOKUP_QUIET);
	if (wdp && (wdp->d_flags & RT_DIR_COMB)) {
	    struct rt_db_internal intern;
	    struct rt_comb_internal *comb;
	    rt_db_get_internal(&intern, wdp, wdbip, NULL, &rt_uniresource);
	    comb = (struct rt_comb_internal *)(&intern)->idb_ptr;
	    RT_CK_COMB(comb);
	    db_free_tree(comb->tree, &rt_uniresource);
	    union tree *tp;
	    struct rt_tree_array *tree_list;
	    BU_GET(tree_list, struct rt_tree_array);
	    tree_list[0].tl_op = OP_UNION;
	    BU_GET(tp, union tree);
	    RT_TREE_INIT(tp);
	    tree_list[0].tl_tree = tp;
	    tp->tr_l.tl_op = OP_DB_LEAF;
	    tp->tr_l.tl_name = bu_strdup(bu_vls_cstr(&bname));
	    tp->tr_l.tl_mat = NULL;
	    comb->tree = (union tree *)db_mkgift_tree(tree_list, 1, &rt_uniresource);
	    struct rt_wdb *wwdbp = wdb_dbopen(wdbip, RT_WDB_TYPE_DB_DEFAULT);
	    wdb_put_internal(wwdbp, wdp->d_namep, &intern, 1.0);
	} else {
	    struct directory *bot_dp = db_lookup(wdbip, bu_vls_cstr(&bname), LOOKUP_QUIET);
	    if (!bot_dp || db_delete(wdbip, wdp) != 0 || db_dirdelete(wdbip, wdp) != 0 ||
		db_rename(wdbip, bot_dp, dpw[0]->d_namep) < 0) {
		if (s->verbosity >= 0)
		    bu_log("regions.cpp:%d Failed to replace implicit root %s.\n", __LINE__, dpw[0]->d_namep);
		db_close(wdbip);
		if (s->variant_plan) {
		    delete (FacetizeVariantPlan *)s->variant_plan;
		    s->variant_plan = NULL;
		}
		bu_ptbl_free(&eval_roots);
		bu_ptbl_free(ir);
		bu_free(ir, "ir table");
		bu_ptbl_free(ar);
		bu_free(ar, "ar table");
		bu_vls_free(&bname);
		bu_free(dpa, "free dpa");
		return BRLCAD_ERROR;
	    }
	}
	db_update_nref(wdbip, &rt_uniresource);
	db_close(wdbip);
    }
    bu_vls_free(&bname);
    bu_ptbl_free(&eval_roots);
    bu_ptbl_free(ir);
    bu_free(ir, "ir table");
    s->use_variant_plan = 1;

    /* Print a validation summary when any regions went through the check. */
    if ((vcnt_total > 0 || vcnt_skip > 0) && !s->make_nmg && !s->nmg_booleval) {
	bu_log("\nFACETIZE conversion validation summary:\n");
	if (vcnt_skip > 0)
	    bu_log("  %d region%s skipped validation (no perturbable leaf primitives)\n",
		    vcnt_skip, vcnt_skip == 1 ? "" : "s");
	if (vcnt_p1_pass > 0)
	    bu_log("  %d region%s passed P1 check (CSG-BoT within tolerance, no perturb needed)\n",
		    vcnt_p1_pass, vcnt_p1_pass == 1 ? "" : "s");
	if (vcnt_few_hit > 0)
	    bu_log("  %d region%s accepted with note: Crofton found very few ray intersections (sub-mm or near-degenerate geometry; verify modeling intent)\n",
		    vcnt_few_hit, vcnt_few_hit == 1 ? "" : "s");
	if (vcnt_zero_hit > 0)
	    bu_log("  %d region%s replaced with empty BoT: zero Crofton ray intersections (Boolean eval likely empty; use 'lint' to verify if unexpected)\n",
		    vcnt_zero_hit, vcnt_zero_hit == 1 ? "" : "s");
	if (vcnt_p1_trigger > 0) {
	    bu_log("  %d region%s triggered perturb retry:\n",
		    vcnt_p1_trigger, vcnt_p1_trigger == 1 ? "" : "s");
	    if (vcnt_p2_pass > 0)
		bu_log("    %d passed P2 check after perturb\n", vcnt_p2_pass);
	    if (vcnt_p2_topoflip > 0)
		bu_log("    %d accepted with note: Crofton-zero/few after perturb (perturb may have shifted topology; check output geometry)\n",
			vcnt_p2_topoflip);
	    if (vcnt_p2_warn > 0)
		bu_log("    %d persistent validation mismatch after perturb (check output geometry with 'lint')\n",
			vcnt_p2_warn);
	}
	if (vcnt_unavail > 0)
	    bu_log("  %d region%s had validation unavailable (metric/prep failure)\n",
		    vcnt_unavail, vcnt_unavail == 1 ? "" : "s");
    }

    // Report on the primitive processing
    if (!s->make_nmg && !s->nmg_booleval)
	facetize_primitives_summary(s);

    // keep active regions into .g copy
    struct ged *wgedp = ged_open("db", bu_vls_cstr(s->wfile), 1);
    if (!wgedp) {
	if (s->verbosity >= 0) {
	    bu_log("regions.cpp:%d unable to retrieve working data - FAIL\n", __LINE__);
	}
	if (s->variant_plan) {
	    delete (FacetizeVariantPlan *)s->variant_plan;
	    s->variant_plan = NULL;
	}
	bu_ptbl_free(&eval_roots);
	bu_ptbl_free(ir);
	bu_free(ir, "ir table");
    	bu_ptbl_free(ar);
	bu_free(ar, "ar table");
	bu_free(dpa, "free dpa");
	return BRLCAD_ERROR;
    }
    const char **av = (const char **)bu_calloc(argc+10, sizeof(const char *), "av");
    av[0] = "keep";
    av[1] = kfname;
    for (int i = 0; i < argc; i++) {
	av[i+2] = argv[i];
    }
    av[argc+2] = NULL;
    ged_exec_keep(wgedp, argc+2, av);
    ged_close(wgedp);

    /* Capture the current tops list.  If we're not doing an in-place overwrite, we
     * need to know what the new top level objects are for the assembly of the
     * final comb. */
    struct directory **tlist = NULL;
    size_t tcnt = db_ls(dbip, DB_LS_TOPS, NULL, &tlist);
    std::set<std::string> otops;
    for (size_t i = 0; i < tcnt; i++) {
	otops.insert(std::string(tlist[i]->d_namep));
    }
    bu_free(tlist, "tlist");
    tlist = NULL;

    /* The user may have specified a naming preference - if so, honor it */
    struct bu_vls prefix_str = BU_VLS_INIT_ZERO;
    struct bu_vls suffix_str = BU_VLS_INIT_ZERO;
    const char *affix = NULL;
    int use_prefix = 1;
    if (bu_vls_strlen(s->prefix)) {
	bu_vls_sprintf(&prefix_str, "%s", bu_vls_cstr(s->prefix));
    } else {
	bu_vls_sprintf(&prefix_str, "facetize_");
    }

    if (bu_vls_strlen(s->suffix)) {
	bu_vls_sprintf(&suffix_str, "%s", bu_vls_cstr(s->suffix));
	use_prefix = 0;
    }
    affix = (use_prefix) ? bu_vls_cstr(&prefix_str) : bu_vls_cstr(&suffix_str);

    // dbconcat output .g into original .g - either using -O to overwrite
    // or allowing dbconcat to suffix the names depending on whether we're
    // in-place or not.
    av[0] = "dbconcat";
    av[1] = (s->in_place) ? "-O" : "-L";
    av[2] = (use_prefix) ? "-p" : "-s";
    av[3] = kfname;
    av[4] = affix;
    av[5] = NULL;
    ged_exec_dbconcat(s->gedp, 5, av);
    bu_free(av, "av");

    bu_vls_free(&prefix_str);
    bu_vls_free(&suffix_str);

    /* Done importing stuff - update nref. */
    db_update_nref(dbip, &rt_uniresource);


    /* Capture the new tops list. */
    tcnt = db_ls(dbip, DB_LS_TOPS, NULL, &tlist);
    std::set<std::string> ntops;
    for (size_t i = 0; i < tcnt; i++) {
	ntops.insert(std::string(tlist[i]->d_namep));
    }
    bu_free(tlist, "tlist");


    /* Find the new top level objects from dbconcat */
    std::set<std::string> new_tobjs;
    std::set_difference(ntops.begin(), ntops.end(), otops.begin(), otops.end(), std::inserter(new_tobjs, new_tobjs.begin()));

    /* Check to see if oname ended up being created in the
     * dbconcat.  If it was, rename it. */
    struct directory *cdp  = db_lookup(dbip, oname, LOOKUP_QUIET);
    if (cdp != RT_DIR_NULL) {
	// Find a new name
	struct bu_vls nname = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&nname, "%s_0", oname);
	cdp  = db_lookup(dbip, bu_vls_cstr(&nname), LOOKUP_QUIET);
	if (cdp != RT_DIR_NULL) {
	    if (bu_vls_incr(&nname, NULL, NULL, &_db_uniq_test, (void *)dbip) < 0) {
		if (s->verbosity >= 0) {
		    bu_log("regions.cpp:%d unable to generate name - FAIL\n", __LINE__);
		}
		bu_vls_free(&nname);
		return BRLCAD_ERROR;
	    }
	}
	const char *mav[4];
	mav[0] = "mvall";
	mav[1] = oname;
	mav[2] = bu_vls_cstr(&nname);
	mav[3] = NULL;
	ged_exec_mvall(s->gedp, 3, mav);
	new_tobjs.erase(std::string(oname));
	new_tobjs.insert(std::string(bu_vls_cstr(&nname)));
	bu_vls_free(&nname);
    }

    /* Make a new comb to hold the output */
    struct wmember wcomb;
    BU_LIST_INIT(&wcomb.l);
    struct rt_wdb *cwdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DEFAULT);
    std::set<std::string>::iterator s_it;
    for (s_it = new_tobjs.begin(); s_it != new_tobjs.end(); ++s_it) {
	(void)mk_addmember(s_it->c_str(), &(wcomb.l), NULL, DB_OP_UNION);
    }
    mk_lcomb(cwdbp, oname, &wcomb, 0, NULL, NULL, NULL, 0);

    /* Done importing stuff - update nref. */
    db_update_nref(dbip, &rt_uniresource);

    /* Print variant-plan summary and clean up (Manifold path only). */
    if (s->variant_plan) {
	FacetizeVariantPlan *vp = (FacetizeVariantPlan *)s->variant_plan;
	if (vp->n_adjusted_instances > 0) {
	    bu_log("FACETIZE: variant summary: %d adjusted instance(s) "
		   "(%d subtractive), %d fallback(s), %d tess failure(s)\n",
		   vp->n_adjusted_instances,
		   vp->n_sub_variants,
		   vp->n_perturb_fallbacks,
		   vp->n_variant_tess_failures);
	}
	delete vp;
	s->variant_plan = NULL;
    }

    bu_ptbl_free(ar);
    bu_free(ar, "ar table");
    bu_free(dpa, "free dpa");
    bu_dirclear(s->wdir);
    return ret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
