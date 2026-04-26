/*                  R A Y T R A C E . C P P
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
/** @file libged/lint/raytrace.cpp
 *
 * Raytrace validation pass for the lint command (lint --raytrace).
 *
 * Strategy
 * --------
 *  1. Collect the universe of objects from user-specified args or the
 *     database tops list.
 *  2. Build a dependency graph (comb -> direct children) and record, for
 *     each object, whether its subtree contains a halfspace or other
 *     unbounded primitive that makes Crofton sampling unreliable.
 *  3. Process objects in leaf-to-root (topological) order:
 *
 *       Leaf primitives
 *         Compare the Cauchy-Crofton SA/volume estimate (fired against
 *         the CSG raytracer) with the object's analytic formula values
 *         (ft_surf_area / ft_volume from the primitive function table).
 *         If no analytic values are available the Crofton result is
 *         recorded informatively without a pass/fail verdict.
 *
 *       Combs (all direct children validated and passing)
 *         Compare the Cauchy-Crofton SA/volume estimate on the CSG tree
 *         with the SA/volume obtained by facetizing the comb to a solid
 *         BoT and measuring the mesh directly.  A discrepancy indicates
 *         a problem with boolean evaluation or raytrace behavior for
 *         that combination.
 *
 *  4. If a child fails validation all ancestors are marked as skipped
 *     (blocked by child failure).  Failures are always attributed at the
 *     lowest level in the tree so they are never masked by aggregate
 *     results from complex assemblies.
 */

#include "common.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "../libbu/json.hpp"

extern "C" {
#include "rt/defines.h"
#include "rt/db_instance.h"
#include "rt/search.h"
#include "rt/func.h"
#include "raytrace.h"
#include "bg/trimesh.h"
#include "wdb.h"
#include "ged/commands.h"
}

#include "./ged_lint.h"

/* ---------------------------------------------------------------------- */
/* Per-object state used during the analysis pass                          */
/* ---------------------------------------------------------------------- */

enum rt_result_t {
    RT_RESULT_UNKNOWN = 0,
    RT_RESULT_OK,
    RT_RESULT_MISMATCH,
    RT_RESULT_FACETIZE_FAILED,
    RT_RESULT_SKIP_HALFSPACE,
    RT_RESULT_SKIP_CHILD_FAIL,
    RT_RESULT_SKIP_PREP_FAIL
};

struct obj_rt_info {
    bool is_comb = false;
    std::set<std::string> children; /* unique direct comb members */
    bool has_halfspace = false;     /* subtree contains unbounded primitive */
    rt_result_t result = RT_RESULT_UNKNOWN;
    /* Metric values: -1.0 means not available */
    double analytic_sa  = -1.0;
    double analytic_vol = -1.0;
    double crofton_sa   = -1.0;
    double crofton_vol  = -1.0;
    double ref_sa       = -1.0;  /* reference: analytic (prims) or BoT (combs) */
    double ref_vol      = -1.0;
    double sa_err_pct   = -1.0;
    double vol_err_pct  = -1.0;
};

static const double EMPTY_METRIC_TOL = 1.0e-9;

/* ---------------------------------------------------------------------- */
/* Tree-walk helpers                                                       */
/* ---------------------------------------------------------------------- */

/* Collect the names of all direct leaf references in a union tree.
 * The same name may appear more than once if it is referenced multiple
 * times; callers are responsible for deduplication where needed.         */
static void
collect_tree_leaves(union tree *tp, std::vector<std::string> &leaves)
{
    if (!tp) return;
    switch (tp->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    collect_tree_leaves(tp->tr_b.tb_right, leaves);
	    /* fall through */
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    collect_tree_leaves(tp->tr_b.tb_left, leaves);
	    break;
	case OP_DB_LEAF:
	    leaves.push_back(std::string(tp->tr_l.tl_name));
	    break;
	default:
	    break;
    }
}


/* Return true if dp's subtree contains a halfspace or other unbounded
 * primitive that would make Crofton sampling give wrong results.
 * visited guards against infinite recursion in shared/cyclic structures. */
static bool
subtree_has_halfspace(struct db_i *dbip, struct directory *dp,
		      std::set<std::string> &visited)
{
    if (!dp) return false;
    if (visited.count(dp->d_namep)) return false; /* cycle guard */
    visited.insert(dp->d_namep);

    if (!(dp->d_flags & RT_DIR_COMB)) {
	/* Primitive: unbounded if it is a halfspace */
	return (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_HALF);
    }

    /* Comb: inspect children */
    struct rt_db_internal in;
    RT_DB_INTERNAL_INIT(&in);
    if (rt_db_get_internal(&in, dp, dbip, NULL) < 0)
	return false;

    struct rt_comb_internal *comb = (struct rt_comb_internal *)in.idb_ptr;
    bool found = false;
    if (comb->tree) {
	std::vector<std::string> children;
	collect_tree_leaves(comb->tree, children);
	for (const auto &cname : children) {
	    struct directory *cdp = db_lookup(dbip, cname.c_str(), LOOKUP_QUIET);
	    if (cdp && subtree_has_halfspace(dbip, cdp, visited)) {
		found = true;
		break;
	    }
	}
    }
    rt_db_free_internal(&in);
    return found;
}


/* Populate universe[name] and recurse into comb children.
 * Already-visited objects are skipped so each object is recorded once.   */
static void
collect_subtree(struct db_i *dbip, struct directory *dp,
		std::map<std::string, obj_rt_info> &universe)
{
    if (!dp) return;
    std::string name(dp->d_namep);
    if (universe.count(name)) return; /* already visited */

    obj_rt_info info;
    info.is_comb = ((dp->d_flags & RT_DIR_COMB) != 0);

    if (info.is_comb) {
	struct rt_db_internal in;
	RT_DB_INTERNAL_INIT(&in);
	int gret = rt_db_get_internal(&in, dp, dbip, NULL);
	if (gret >= 0) {
	    struct rt_comb_internal *comb = (struct rt_comb_internal *)in.idb_ptr;
	    if (comb->tree) {
		std::vector<std::string> children;
		collect_tree_leaves(comb->tree, children);
		for (const auto &c : children)
		    info.children.insert(c); /* deduplicate via set */
	    }
	    rt_db_free_internal(&in);
	}
    }

    /* Check whether the subtree rooted at dp contains a halfspace */
    {
	std::set<std::string> hs_visited;
	hs_visited.insert(name);
	if (!info.is_comb) {
	    info.has_halfspace = (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_HALF);
	} else {
	    for (const auto &cname : info.children) {
		struct directory *cdp = db_lookup(dbip, cname.c_str(), LOOKUP_QUIET);
		if (cdp && subtree_has_halfspace(dbip, cdp, hs_visited)) {
		    info.has_halfspace = true;
		    break;
		}
	    }
	}
    }

    universe[name] = info;

    /* Recurse into comb children so the entire reachable subgraph is in
     * the universe map.                                                   */
    for (const auto &cname : info.children) {
	struct directory *cdp = db_lookup(dbip, cname.c_str(), LOOKUP_QUIET);
	if (cdp) collect_subtree(dbip, cdp, universe);
    }
}


/* DFS-based topological sort: children appear before their parents.
 * Appends names to sorted in post-order.                                  */
static void
topo_dfs(const std::string &name,
	 const std::map<std::string, obj_rt_info> &universe,
	 std::set<std::string> &visited,
	 std::vector<std::string> &sorted)
{
    if (visited.count(name)) return;
    visited.insert(name);

    if (universe.count(name)) {
	const obj_rt_info &info = universe.at(name);
	for (const auto &child : info.children)
	    topo_dfs(child, universe, visited, sorted);
    }

    sorted.push_back(name);
}


/* ---------------------------------------------------------------------- */
/* Metric computation helpers                                              */
/* ---------------------------------------------------------------------- */

/* Fire Crofton rays against the named CSG object.
 * Returns 0 on success, -1 on failure.                                   */
static int
crofton_on_obj(struct db_i *dbip, const char *obj_name, size_t n_rays,
	       double &out_sa, double &out_vol)
{
    out_sa = out_vol = -1.0;

    struct rt_i *rtip = rt_new_rti(dbip);
    if (!rtip) return -1;

    if (rt_gettree(rtip, obj_name) != 0) {
	rt_free_rti(rtip);
	return -1;
    }
    rt_prep_parallel(rtip, 1);

    /* Use convergence-based sampling when n_rays == 0 (the recommended
     * default).  A fixed ray count can be requested for reproducibility
     * or timing studies, but for reliable SA/volume estimates the
     * convergence path is strongly preferred.                           */
    struct rt_crofton_params crp;
    if (n_rays > 0) {
	crp.n_rays       = n_rays;
	crp.stability_mm = 0.0;
	crp.time_ms      = 0.0;
    } else {
	crp.n_rays       = 0;
	crp.stability_mm = 0.05;  /* equivalent-radius stability target (mm) */
	crp.time_ms      = 2000.0; /* 2 s per-object wall-clock safety budget */
    }

    double sa = 0.0, vol = 0.0;
    int cr = rt_crofton_shoot(rtip, &crp, &sa, &vol);
    rt_free_rti(rtip);

    if (cr < 0) return -1;
    out_sa  = sa;
    out_vol = vol;
    return 0;
}


/* Return a temporary name that does not already exist in dbip.           */
static std::string
make_tmp_name(struct db_i *dbip)
{
    static int counter = 0;
    for (;;) {
	std::string name = std::string("_lint_rt_tmp_") + std::to_string(++counter);
	if (db_lookup(dbip, name.c_str(), LOOKUP_QUIET) == RT_DIR_NULL)
	    return name;
    }
}


/* Facetize obj_name to a temporary solid BoT, measure its SA and volume
 * via bg_trimesh_*, then delete the temporary object.
 * When use_perturb is true, the default facetize perturbation pass is used
 * (no --no-perturb flag), allowing comparison against the realistic output.
 * Returns 0 on success, -1 on failure.                                   */
static int
facetize_and_measure(struct ged *gedp, const char *obj_name,
		     double &out_sa, double &out_vol, bool use_perturb)
{
    out_sa = out_vol = -1.0;

    std::string tmp_name = make_tmp_name(gedp->dbip);

    /* Save the current result string so that facetize output does not
     * clobber whatever lint has already accumulated.                     */
    struct bu_vls saved_result = BU_VLS_INIT_ZERO;
    bu_vls_vlscat(&saved_result, gedp->ged_result_str);
    bu_vls_trunc(gedp->ged_result_str, 0);

    int fret;
    if (use_perturb) {
	/* Default facetize behavior: perturbation pass enabled */
	const char *av[3];
	av[0] = "facetize";
	av[1] = obj_name;
	av[2] = tmp_name.c_str();
	fret = ged_exec(gedp, 3, av);
    } else {
	/* Suppress perturbation so the BoT reference is geometrically
	 * consistent with the Crofton CSG measurement.                  */
	const char *av[4];
	av[0] = "facetize";
	av[1] = "--no-perturb";
	av[2] = obj_name;
	av[3] = tmp_name.c_str();
	fret = ged_exec(gedp, 4, av);
    }

    /* Restore lint result string */
    bu_vls_trunc(gedp->ged_result_str, 0);
    bu_vls_vlscat(gedp->ged_result_str, &saved_result);
    bu_vls_free(&saved_result);

    if (fret != BRLCAD_OK) {
	/* Clean up any partial output the facetize may have written */
	struct directory *tdp = db_lookup(gedp->dbip, tmp_name.c_str(), LOOKUP_QUIET);
	if (tdp) {
	    db_delete(gedp->dbip, tdp);
	    db_dirdelete(gedp->dbip, tdp);
	}
	return -1;
    }

    struct directory *tdp = db_lookup(gedp->dbip, tmp_name.c_str(), LOOKUP_QUIET);
    if (!tdp) return -1;

    /* Facetize of a complex comb may produce a wrapper comb rather than
     * a bare BoT.  Walk into the result to find a solid BoT.             */
    struct directory *bot_dp = tdp;
    if (tdp->d_flags & RT_DIR_COMB) {
	/* Try to find a BoT one level down */
	struct rt_db_internal win;
	RT_DB_INTERNAL_INIT(&win);
	if (rt_db_get_internal(&win, tdp, gedp->dbip, NULL) >= 0) {
	    struct rt_comb_internal *wcomb = (struct rt_comb_internal *)win.idb_ptr;
	    if (wcomb->tree && wcomb->tree->tr_op == OP_DB_LEAF) {
		const char *child_name = wcomb->tree->tr_l.tl_name;
		struct directory *cdp = db_lookup(gedp->dbip, child_name, LOOKUP_QUIET);
		if (cdp && !(cdp->d_flags & RT_DIR_COMB))
		    bot_dp = cdp;
	    }
	    rt_db_free_internal(&win);
	}
    }

    int got_metrics = 0;
    if (bot_dp && !(bot_dp->d_flags & RT_DIR_COMB)) {
	struct rt_db_internal in;
	RT_DB_INTERNAL_INIT(&in);
	if (rt_db_get_internal(&in, bot_dp, gedp->dbip, NULL) >= 0) {
	    if (in.idb_minor_type == DB5_MINORTYPE_BRLCAD_BOT) {
		struct rt_bot_internal *bot = (struct rt_bot_internal *)in.idb_ptr;
		RT_BOT_CK_MAGIC(bot);
		if (bot->mode == RT_BOT_SOLID) {
		    if (bot->num_faces > 0 && bot->num_vertices > 0) {
			out_sa  = bg_trimesh_area(bot->faces, bot->num_faces,
						  (const point_t *)bot->vertices,
						  bot->num_vertices);
			out_vol = bg_trimesh_volume(bot->faces, bot->num_faces,
						   (const point_t *)bot->vertices,
						   bot->num_vertices);
		    } else {
			/* Empty solid BoT is a valid result for empty CSG. */
			out_sa = 0.0;
			out_vol = 0.0;
		    }
		    got_metrics = 1;
		}
	    }
	    rt_db_free_internal(&in);
	}
    }

    /* Delete the temporary object (and any wrapper comb) */
    if (bot_dp && bot_dp != tdp) {
	db_delete(gedp->dbip, bot_dp);
	db_dirdelete(gedp->dbip, bot_dp);
    }
    db_delete(gedp->dbip, tdp);
    db_dirdelete(gedp->dbip, tdp);

    return got_metrics ? 0 : -1;
}


/* ---------------------------------------------------------------------- */
/* Per-object analysis                                                     */
/* ---------------------------------------------------------------------- */

/* Validate a leaf primitive: analytic formula vs Crofton estimate.       */
static void
check_primitive(lint_data *ldata, struct directory *dp,
		obj_rt_info &info, double tol_pct)
{
    struct ged *gedp = ldata->gedp;
    const char *name = dp->d_namep;

    /* Query the primitive's analytic SA and volume when available */
    double analytic_sa = -1.0, analytic_vol = -1.0;
    {
	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	if (rt_db_get_internal(&intern, dp, gedp->dbip, NULL) >= 0) {
	    if (intern.idb_minor_type >= 0 && intern.idb_minor_type < ID_MAXIMUM) {
		const struct rt_functab *ft = &OBJ[intern.idb_minor_type];

		if (!BU_SETJUMP) {
		    if (ft->ft_surf_area) ft->ft_surf_area(&analytic_sa, &intern);
		    if (ft->ft_volume)    ft->ft_volume(&analytic_vol, &intern);
		} else {
		    BU_UNSETJUMP;
		    analytic_sa = analytic_vol = -1.0;
		}
		BU_UNSETJUMP;
	    }
	    rt_db_free_internal(&intern);
	}
    }

    /* Run Crofton on the CSG primitive */
    double csa = -1.0, cvol = -1.0;
    int cret = crofton_on_obj(gedp->dbip, name, ldata->rt_crofton_rays, csa, cvol);

    info.analytic_sa  = analytic_sa;
    info.analytic_vol = analytic_vol;
    info.crofton_sa   = csa;
    info.crofton_vol  = cvol;

    nlohmann::json jentry;
    jentry["object_name"] = name;
    jentry["object_type"] = "primitive";

    if (cret != 0) {
	info.result = RT_RESULT_SKIP_PREP_FAIL;
	jentry["problem_type"] = "raytrace_skip";
	jentry["reason"]       = "crofton_prep_failed";
	ldata->j.push_back(jentry);
	return;
    }

    jentry["crofton_sa"]  = csa;
    jentry["crofton_vol"] = cvol;

    bool has_analytic = (analytic_sa > 0.0 && analytic_vol > 0.0);
    if (has_analytic) {
	double sa_err  = std::fabs(csa  - analytic_sa)  / analytic_sa;
	double vol_err = std::fabs(cvol - analytic_vol) / analytic_vol;

	info.ref_sa      = analytic_sa;
	info.ref_vol     = analytic_vol;
	info.sa_err_pct  = sa_err  * 100.0;
	info.vol_err_pct = vol_err * 100.0;

	jentry["analytic_sa"]  = analytic_sa;
	jentry["analytic_vol"] = analytic_vol;
	jentry["sa_err_pct"]   = info.sa_err_pct;
	jentry["vol_err_pct"]  = info.vol_err_pct;

	bool fail_sa  = (sa_err  > tol_pct);
	bool fail_vol = (vol_err > tol_pct);

	if (fail_sa || fail_vol) {
	    info.result         = RT_RESULT_MISMATCH;
	    jentry["problem_type"] = "raytrace_mismatch";
	    if (fail_sa && fail_vol)
		jentry["reason"] = "Crofton SA and volume disagree with analytic formula";
	    else if (fail_sa)
		jentry["reason"] = "Crofton SA disagrees with analytic formula";
	    else
		jentry["reason"] = "Crofton volume disagrees with analytic formula";
	} else {
	    info.result         = RT_RESULT_OK;
	    jentry["problem_type"] = "raytrace_ok";
	}
    } else {
	/* No analytic reference available: record informatively */
	info.result         = RT_RESULT_OK;
	jentry["problem_type"] = "raytrace_ok";
	jentry["reason"]       = "no_analytic_reference";
    }

    ldata->j.push_back(jentry);
}


/* Validate a comb: Crofton on CSG tree vs BoT metrics from facetize.    */
static void
check_comb(lint_data *ldata, struct directory *dp,
	   obj_rt_info &info, double tol_pct)
{
    struct ged *gedp = ldata->gedp;
    const char *name = dp->d_namep;

    nlohmann::json jentry;
    jentry["object_name"] = name;
    jentry["object_type"] = "comb";

    /* Crofton on the CSG tree */
    double csa = -1.0, cvol = -1.0;
    int cret = crofton_on_obj(gedp->dbip, name, ldata->rt_crofton_rays, csa, cvol);

    if (cret != 0) {
	info.result            = RT_RESULT_SKIP_PREP_FAIL;
	jentry["problem_type"] = "raytrace_skip";
	jentry["reason"]       = "crofton_prep_failed";
	ldata->j.push_back(jentry);
	return;
    }

    info.crofton_sa  = csa;
    info.crofton_vol = cvol;
    jentry["crofton_sa"]  = csa;
    jentry["crofton_vol"] = cvol;

    /* Facetize to get independent BoT SA/vol */
    double bsa = -1.0, bvol = -1.0;
    int fret = facetize_and_measure(gedp, name, bsa, bvol, ldata->rt_do_perturb);

    if (fret != 0 || bsa < 0.0 || bvol < 0.0) {
	info.result            = RT_RESULT_FACETIZE_FAILED;
	jentry["problem_type"] = "raytrace_facetize_failed";
	jentry["reason"]       = "facetize_failed_or_non_solid_result";
	ldata->j.push_back(jentry);
	return;
    }

    info.ref_sa  = bsa;
    info.ref_vol = bvol;
    jentry["bot_sa"]  = bsa;
    jentry["bot_vol"] = bvol;

    bu_log("[RAYTRACE_CHECK] obj=%-30s  crofton_SA=%.4f  bot_SA=%.4f  ratio=%.3f  crofton_vol=%.4f  bot_vol=%.4f\n",
	   name, csa, bsa, (bsa > 0.0) ? bsa/csa : 0.0, cvol, bvol);

    /* Empty reference BoT (empty CSG result) needs dedicated handling. */
    if (std::fabs(bsa) <= EMPTY_METRIC_TOL && std::fabs(bvol) <= EMPTY_METRIC_TOL) {
	bool csg_empty = (std::fabs(csa) <= EMPTY_METRIC_TOL &&
			  std::fabs(cvol) <= EMPTY_METRIC_TOL);
	info.sa_err_pct  = csg_empty ? 0.0 : 100.0;
	info.vol_err_pct = csg_empty ? 0.0 : 100.0;
	jentry["sa_err_pct"]  = info.sa_err_pct;
	jentry["vol_err_pct"] = info.vol_err_pct;
	if (csg_empty) {
	    info.result            = RT_RESULT_OK;
	    jentry["problem_type"] = "raytrace_ok";
	    jentry["reason"]       = "empty_csg_and_empty_bot";
	} else {
	    info.result            = RT_RESULT_MISMATCH;
	    jentry["problem_type"] = "raytrace_mismatch";
	    jentry["reason"]       = "Facetized BoT is empty but CSG Crofton is non-empty";
	}
	ldata->j.push_back(jentry);
	return;
    }

    /* Relative error uses the BoT (exact mesh) as the reference denominator,
     * consistent with how primitives use the analytic formula as denominator. */
    double sa_err  = (bsa  > 0.0) ? std::fabs(csa  - bsa)  / bsa  : 1.0;
    double vol_err = (bvol > 0.0) ? std::fabs(cvol - bvol) / bvol : 1.0;
    info.sa_err_pct  = sa_err  * 100.0;
    info.vol_err_pct = vol_err * 100.0;
    jentry["sa_err_pct"]  = info.sa_err_pct;
    jentry["vol_err_pct"] = info.vol_err_pct;

    bool fail_sa  = (sa_err  > tol_pct);
    bool fail_vol = (vol_err > tol_pct);

    if (fail_sa || fail_vol) {
	info.result            = RT_RESULT_MISMATCH;
	jentry["problem_type"] = "raytrace_mismatch";
	if (ldata->rt_do_perturb) {
	    /* Mismatch persists even when facetize uses its default
	     * perturbation pass.  This is a strong indicator of a
	     * modeling topology problem that perturbation cannot resolve.
	     * Two known patterns produce this signature:
	     *  - Coplanar sub-sub interface: two subtractive primitives
	     *    share an exact face plane end-to-end (e.g. TGC sections).
	     *  - Coplanar sub-base interface: a subtractive primitive has
	     *    one or more faces exactly flush with the base solid.
	     * See the lint man page RAYTRACE MODE section for ASCII diagrams
	     * and remediation guidance.                                     */
	    if (fail_sa && fail_vol)
		jentry["reason"] = "persistent SA and volume mismatch under perturbation; probable coplanar sub-sub or sub-base interface - geometry correction required (see lint man page)";
	    else if (fail_sa)
		jentry["reason"] = "persistent SA mismatch under perturbation; probable coplanar sub-sub or sub-base interface - geometry correction required (see lint man page)";
	    else
		jentry["reason"] = "persistent volume mismatch under perturbation; possible incorrect boolean evaluation result";
	} else {
	    /* Default (no-perturb) mode: a mismatch here may be a
	     * coplanar-face artifact that the perturbation pass would
	     * resolve, or a genuine modeling problem.  Re-run with
	     * --perturb to distinguish the two cases.                       */
	    if (fail_sa && fail_vol)
		jentry["reason"] = "CSG Crofton SA and volume disagree with BoT mesh; coplanar-face artifact possible - re-run with --perturb to distinguish artifact from modeling issue";
	    else if (fail_sa)
		jentry["reason"] = "CSG Crofton SA disagrees with BoT mesh; coplanar-face artifact possible - re-run with --perturb to distinguish artifact from modeling issue";
	    else
		jentry["reason"] = "CSG Crofton volume disagrees with BoT mesh";
	}
    } else {
	info.result            = RT_RESULT_OK;
	jentry["problem_type"] = "raytrace_ok";
    }

    ldata->j.push_back(jentry);
}


/* ---------------------------------------------------------------------- */
/* Main entry point                                                        */
/* ---------------------------------------------------------------------- */

int
_ged_raytrace_check(lint_data *ldata)
{
    if (!ldata) return BRLCAD_ERROR;

    struct ged    *gedp = ldata->gedp;
    struct db_i *dbip = gedp->dbip;

    /* 1. Determine root objects */
    std::vector<std::string> roots;
    if (ldata->argc && ldata->dpa) {
	for (int i = 0; i < ldata->argc; i++)
	    roots.push_back(std::string(ldata->dpa[i]->d_namep));
    } else {
	struct directory **tlist = NULL;
	size_t tcnt = db_ls(dbip, DB_LS_TOPS, NULL, &tlist);
	for (size_t i = 0; i < tcnt; i++)
	    roots.push_back(std::string(tlist[i]->d_namep));
	if (tlist) bu_free(tlist, "db_ls tops");
    }

    if (roots.empty()) {
	bu_log("lint --raytrace: no objects to check\n");
	return BRLCAD_OK;
    }

    /* 2. Build the object universe (all objects reachable from roots) */
    std::map<std::string, obj_rt_info> universe;
    for (const auto &rname : roots) {
	struct directory *dp = db_lookup(dbip, rname.c_str(), LOOKUP_QUIET);
	if (dp) collect_subtree(dbip, dp, universe);
    }

    /* 3. Topological sort: leaves first, combs last */
    std::set<std::string> visited;
    std::vector<std::string> order;
    for (const auto &rname : roots)
	topo_dfs(rname, universe, visited, order);
    /* Handle objects reachable via children that aren't among the explicit
     * roots (shouldn't happen with well-formed trees, but be safe).      */
    for (const auto &kv : universe)
	topo_dfs(kv.first, universe, visited, order);

    double tol_pct = ldata->rt_tol_pct;

    int n_pass = 0, n_fail = 0, n_skip = 0;

    if (ldata->verbosity > 0)
	bu_log("lint --raytrace: universe has %zu objects, order has %zu entries\n",
	       universe.size(), order.size());

    /* 4. Process in leaf-to-root order */
    for (const auto &name : order) {
	if (!universe.count(name)) {
	    if (ldata->verbosity > 0)
		bu_log("lint --raytrace: skipping %s (not in universe)\n", name.c_str());
	    continue;
	}
	obj_rt_info &info = universe[name];

	struct directory *dp = db_lookup(dbip, name.c_str(), LOOKUP_QUIET);
	if (!dp) {
	    if (ldata->verbosity > 0)
		bu_log("lint --raytrace: skipping %s (not found in db)\n", name.c_str());
	    continue;
	}

	if (ldata->verbosity > 0)
	    bu_log("lint --raytrace: processing %s (is_comb=%d has_halfspace=%d)\n",
		   name.c_str(), (int)info.is_comb, (int)info.has_halfspace);

	/* Skip objects with halfspace in their subtree */
	if (info.has_halfspace) {
	    info.result = RT_RESULT_SKIP_HALFSPACE;
	    nlohmann::json j;
	    j["problem_type"] = "raytrace_skip";
	    j["object_name"]  = name;
	    j["object_type"]  = info.is_comb ? "comb" : "primitive";
	    j["reason"]       = "contains_unbounded_primitive";
	    ldata->j.push_back(j);
	    n_skip++;
	    continue;
	}

	/* Skip if any direct child failed or was skipped in a way that makes
	 * the parent result untrustworthy.                                   */
	bool child_fail = false;
	for (const auto &cname : info.children) {
	    if (!universe.count(cname)) continue;
	    rt_result_t cr = universe[cname].result;
	    if (cr == RT_RESULT_MISMATCH ||
		cr == RT_RESULT_FACETIZE_FAILED ||
		cr == RT_RESULT_SKIP_PREP_FAIL ||
		cr == RT_RESULT_SKIP_CHILD_FAIL) {
		child_fail = true;
		break;
	    }
	}
	if (child_fail) {
	    info.result = RT_RESULT_SKIP_CHILD_FAIL;
	    nlohmann::json j;
	    j["problem_type"] = "raytrace_skip";
	    j["object_name"]  = name;
	    j["object_type"]  = info.is_comb ? "comb" : "primitive";
	    j["reason"]       = "child_validation_failed";
	    ldata->j.push_back(j);
	    n_skip++;
	    continue;
	}

	if (!info.is_comb)
	    check_primitive(ldata, dp, info, tol_pct);
	else
	    check_comb(ldata, dp, info, tol_pct);

	switch (info.result) {
	    case RT_RESULT_OK:              n_pass++; break;
	    case RT_RESULT_MISMATCH:        n_fail++; break;
	    default:                        n_skip++; break;
	}
    }

    if (ldata->verbosity >= 0)
	bu_log("Raytrace validation: %d passed, %d failed, %d skipped\n",
	       n_pass, n_fail, n_skip);

    return (n_fail > 0) ? BRLCAD_ERROR : BRLCAD_OK;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
