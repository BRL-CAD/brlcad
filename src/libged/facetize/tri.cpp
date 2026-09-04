/*                         T R I . C P P
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
/** @file libged/facetize/tri_booleval.cpp
 *
 * Triangle centric boolean evaluation logic using Manifold library.
 */

#include "common.h"

#include <map>
#include <set>
#include <vector>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iostream>
#include <fstream>
#include <queue>

#include <string.h>

#include "manifold/manifold.h"

#include "bg/trimesh.h"
#include "bu/app.h"
#include "bu/env.h"
#include "bu/parallel.h"
#include "bu/path.h"
#include "bu/process.h"
#include "bu/snooze.h"
#include "bu/datetime.h"
#include "../ged_private.h"
#include "./ged_facetize.h"
#include "./process.h"
#include "./tess_opts.h"
#include "./worker.h"

static const size_t FACETIZE_EMPTY_CHECK_CROFTON_RAYS = 800u;
static const double FACETIZE_EMPTY_CHECK_REL_VOL_TOL = 1.0e-9;
static const double FACETIZE_EMPTY_CHECK_ABS_VOL_TOL = 1.0e-12;
static const int FACETIZE_METHOD_COMMAND_INDEX = 5;
static const double FACETIZE_USEC_TO_SEC_DIVISOR = 1.0e6;
static const double FACETIZE_IO_PROBE_DURATION_SEC = 1.0;
static const size_t FACETIZE_MIB_BYTES = 1024u * 1024u;
static const size_t FACETIZE_IO_PROBE_CHUNK_BYTES = FACETIZE_MIB_BYTES;
static const size_t FACETIZE_IO_PROBE_MAX_BYTES = 128u * FACETIZE_MIB_BYTES;
static const size_t FACETIZE_IO_BUFFER_SIZE = 4096u;
static const int FACETIZE_POLL_INTERVAL_USEC = 1000;
static const int FACETIZE_PROGRESS_BATCH_MIN = 100;
static const int FACETIZE_PROGRESS_INTERVAL_SEC = 5;
static const int FACETIZE_WRITER_RESTART_LIMIT = 1;
static const char FACETIZE_WORKER_FILE_SUFFIX[] = ".facetize_worker_";

enum tess_work_type {
    TESS_WORK_ORIGINAL_PRIMITIVE,
    TESS_WORK_PERTURBATION_VARIANT,
    TESS_WORK_PLATE_MODE_PRIMITIVE
};

static const char *
tess_work_description(enum tess_work_type work_type, int count)
{
    switch (work_type) {
	case TESS_WORK_ORIGINAL_PRIMITIVE:
	    return (count == 1) ? "original primitive" : "original primitives";
	case TESS_WORK_PERTURBATION_VARIANT:
	    return (count == 1) ? "perturbation variant" : "perturbation variants";
	case TESS_WORK_PLATE_MODE_PRIMITIVE:
	    return (count == 1) ? "plate-mode primitive" : "plate-mode primitives";
    }

    return (count == 1) ? "input" : "inputs";
}

static const char *
bool_op_name(int op)
{
    switch (op) {
	case OP_UNION:
	    return "UNION";
	case OP_INTERSECT:
	    return "INTERSECT";
	case OP_SUBTRACT:
	    return "SUBTRACT";
	default:
	    return "UNKNOWN";
    }
}

static void
facetize_log_current_failure(struct _ged_facetize_state *s, const char *fallback)
{
    const char *msg = fallback ? fallback : "unknown failure";
    if (s && s->failure_msg && bu_vls_strlen(s->failure_msg))
	msg = bu_vls_cstr(s->failure_msg);

    facetize_log(s, 0, " failed: %s\n", msg);
}

static int
bot_to_manifold(struct _ged_facetize_state *s, void **out, struct db_tree_state *tsp, struct rt_db_internal *ip, int flip, const char *leaf_name)
{
    if (!out || !tsp || !ip) {
	facetize_failure(s, "internal error preparing Manifold leaf '%s': missing conversion input", leaf_name ? leaf_name : "(unknown)");
	return BRLCAD_ERROR;
    }

    // By this point all leaves should be bots
    if (ip->idb_minor_type != ID_BOT) {
	facetize_failure(s, "leaf '%s' was not converted to a BoT before boolean evaluation (minor type %d)", leaf_name ? leaf_name : "(unknown)", ip->idb_minor_type);
	return BRLCAD_ERROR;
    }

    struct rt_bot_internal *nbot = (struct rt_bot_internal *)ip->idb_ptr;
    if (!nbot) {
	facetize_failure(s, "leaf '%s' has no BoT data after tessellation", leaf_name ? leaf_name : "(unknown)");
	return BRLCAD_ERROR;
    }

    if (!nbot->num_vertices) {
	// Trivial case
        (*out) = new manifold::Manifold();
	return 0;
    }

    if (flip) {
	switch (nbot->orientation) {
	    case RT_BOT_CCW:
		nbot->orientation = RT_BOT_CW;
		break;
	    default:
		nbot->orientation = RT_BOT_CCW;
	}
    }

    if (nbot->num_vertices < 3) {
	facetize_failure(s, "BoT leaf '%s' has only %zu vertices; at least 3 are needed for a manifold mesh", leaf_name ? leaf_name : "(unknown)", nbot->num_vertices);
	return BRLCAD_ERROR;
    }

    if (!nbot->num_faces) {
	facetize_failure(s, "BoT leaf '%s' has %zu vertices but no faces", leaf_name ? leaf_name : "(unknown)", nbot->num_vertices);
	return BRLCAD_ERROR;
    }

    if (!nbot->vertices || !nbot->faces) {
	facetize_failure(s, "BoT leaf '%s' is missing %s array data", leaf_name ? leaf_name : "(unknown)", !nbot->vertices ? "vertex" : "face");
	return BRLCAD_ERROR;
    }

    // NOTE -  if long-thin-dense triangle fans end up causing super-long
    // evaluation times here the same way we did in plate mode extrusion, we
    // could try the preliminary decimation criteria we use there on volumetric
    // inputs as well.  Waiting on that until we see a real-world need to
    // justify it, since we would have to support the parameters bot extrude
    // needs here as well.
    manifold::MeshGL64 bot_mesh;
    for (size_t j = 0; j < nbot->num_vertices*3 ; j++) {
	if (!std::isfinite(nbot->vertices[j])) {
	    facetize_failure(s, "BoT leaf '%s' has a non-finite vertex coordinate at vertex %zu", leaf_name ? leaf_name : "(unknown)", j / 3);
	    return BRLCAD_ERROR;
	}
	bot_mesh.vertProperties.insert(bot_mesh.vertProperties.end(), nbot->vertices[j]);
    }
    if (nbot->orientation == RT_BOT_CW) {
	for (size_t j = 0; j < nbot->num_faces; j++) {
	    for (int k = 0; k < 3; k++) {
		if (nbot->faces[3*j+k] < 0 || (size_t)nbot->faces[3*j+k] >= nbot->num_vertices) {
		    facetize_failure(s, "BoT leaf '%s' face %zu references invalid vertex index %d (valid range 0..%zu)", leaf_name ? leaf_name : "(unknown)", j, nbot->faces[3*j+k], nbot->num_vertices - 1);
		    return BRLCAD_ERROR;
		}
	    }
	    bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), nbot->faces[3*j+0]);
	    bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), nbot->faces[3*j+2]);
	    bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), nbot->faces[3*j+1]);
	}
    } else {
	for (size_t j = 0; j < nbot->num_faces; j++) {
	    for (int k = 0; k < 3; k++) {
		if (nbot->faces[3*j+k] < 0 || (size_t)nbot->faces[3*j+k] >= nbot->num_vertices) {
		    facetize_failure(s, "BoT leaf '%s' face %zu references invalid vertex index %d (valid range 0..%zu)", leaf_name ? leaf_name : "(unknown)", j, nbot->faces[3*j+k], nbot->num_vertices - 1);
		    return BRLCAD_ERROR;
		}
	    }
	    bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), nbot->faces[3*j+0]);
	    bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), nbot->faces[3*j+1]);
	    bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), nbot->faces[3*j+2]);
	}
    }

    manifold::Manifold bot_manifold = manifold::Manifold(bot_mesh);
    if (bot_manifold.Status() != manifold::Manifold::Error::NoError) {
	// Urk - we got a mesh, but it's no good for a Manifold(??)
	facetize_failure(s, "Manifold rejected BoT leaf '%s': %s (vertices=%zu faces=%zu). Check the primitive with 'bot check' or 'lint'.",
		leaf_name ? leaf_name : "(unknown)",
		manifold::ToString(bot_manifold.Status()).c_str(),
		nbot->num_vertices, nbot->num_faces);
	return BRLCAD_ERROR;
    }

    // Passed - return the manifold
    (*out) = new manifold::Manifold(bot_manifold);
    return 0;
}

// We need to see if the matrix is turning the BoT inside out.  Make a
// test face, with a setup that will report non-flipping with an IDN
// matrix, and see what the currently active matrix does to it.
static int bot_flipped(mat_t *m)
{
    point_t oorigin = {-0.4, 0.5, 0.4};
    point_t othit = {-0.301, 0.581, 0.28};
    point_t ov[3] = {{0, 1, 1}, {-1, 1, 0}, {0, 0, 0}};

    point_t origin, thit;
    point_t v[3];

    for (int i = 0; i < 3; i++)
	MAT4X3PNT(v[i], *m, ov[i]);
    MAT4X3PNT(origin, *m, oorigin);
    MAT4X3PNT(thit, *m, othit);

    vect_t raydir;
    VSUB2(raydir, thit, origin);

    vect_t edges[2];
    VSUB2(edges[0], v[1], v[0]);
    VSUB2(edges[1], v[2], v[1]);

    vect_t ecross;
    VCROSS(ecross, edges[0], edges[1]);

    fastf_t vedot = VDOT(ecross, raydir);
    if (vedot > 0)
	return 1;

    return 0;
}

static double
bot_bbox_volume(const struct rt_bot_internal *bot)
{
    if (!bot || !bot->vertices || bot->num_vertices < 1)
	return 0.0;

    point_t bmin, bmax;
    VSETALL(bmin, INFINITY);
    VSETALL(bmax, -INFINITY);
    for (size_t i = 0; i < bot->num_vertices; i++) {
	const double *v = &bot->vertices[3*i];
	if (v[0] < bmin[0]) bmin[0] = v[0];
	if (v[1] < bmin[1]) bmin[1] = v[1];
	if (v[2] < bmin[2]) bmin[2] = v[2];
	if (v[0] > bmax[0]) bmax[0] = v[0];
	if (v[1] > bmax[1]) bmax[1] = v[1];
	if (v[2] > bmax[2]) bmax[2] = v[2];
    }

    vect_t d;
    VSUB2(d, bmax, bmin);
    if (d[0] <= 0.0 || d[1] <= 0.0 || d[2] <= 0.0)
	return 0.0;
    return d[0] * d[1] * d[2];
}

static int
csg_crofton_volume(struct db_i *dbip, const char *obj_name, double *out_vol)
{
    if (!dbip || !obj_name || !out_vol)
	return BRLCAD_ERROR;

    *out_vol = -1.0;
    point_t focus_min, focus_max;
    int have_focus = (_ged_facetize_csg_bbox(dbip, obj_name, focus_min, focus_max) == BRLCAD_OK);

    struct rt_i *rtip = rt_i_create(dbip);
    if (!rtip)
	return BRLCAD_ERROR;
    if (rt_gettree(rtip, obj_name) != 0) {
	rt_i_destroy(rtip);
	return BRLCAD_ERROR;
    }
    rt_prep_parallel(rtip, 1);

    double sa = 0.0, vol = 0.0;
    struct rt_crofton_params crp = {};
    crp.n_rays = FACETIZE_EMPTY_CHECK_CROFTON_RAYS;
    int rc = rt_crofton_shoot(&sa, &vol, NULL, NULL, NULL, NULL, NULL,
	    rtip, &crp,
	    have_focus ? focus_min : NULL,
	    have_focus ? focus_max : NULL);
    rt_i_destroy(rtip);
    if (rc < 0)
	return BRLCAD_ERROR;
    *out_vol = vol;
    return BRLCAD_OK;
}

// Customized version of rt_booltree_leaf_tess for Manifold processing
static union tree *
_booltree_leaf_tess(struct db_tree_state *tsp, const struct db_full_path *pathp, struct rt_db_internal *ip, void *data)
{
    int ts_status = 0;
    union tree *curtree;
    struct directory *dp;

    if (!tsp || !pathp || !ip)
	return TREE_NULL;

    RT_CK_DB_INTERNAL(ip);
    RT_CK_FULL_PATH(pathp);
    dp = DB_FULL_PATH_CUR_DIR(pathp);
    RT_CK_DIR(dp);

    if (tsp->ts_m)
	NMG_CK_MODEL(*tsp->ts_m);
    BN_CK_TOL(tsp->ts_tol);
    BG_CK_TESS_TOL(tsp->ts_ttol);

    BU_GET(curtree, union tree);
    RT_TREE_INIT(curtree);
    curtree->tr_op = OP_TESS;
    curtree->tr_d.td_name = bu_strdup(dp->d_namep);
    curtree->tr_d.td_r = NULL;
    curtree->tr_d.td_d = NULL;
    curtree->tr_d.td_i = NULL;

    // Infinite half spaces get special handling in the boolean evaluation
    if (ip->idb_minor_type == ID_HALF) {
	struct rt_db_internal *hintern;
	BU_GET(hintern, struct rt_db_internal);
	RT_DB_INTERNAL_INIT(hintern);
	hintern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	hintern->idb_type = ID_HALF;
	hintern->idb_meth = &OBJ[ID_HALF];
	struct rt_half_internal *hf_cp;
	BU_GET(hf_cp, struct rt_half_internal);
	hintern->idb_ptr = (void *)hf_cp;

	struct rt_half_internal *hf_ip= (struct rt_half_internal *)ip->idb_ptr;
	hf_cp->magic = hf_ip->magic;
	HMOVE(hf_cp->eqn, hf_ip->eqn);
	curtree->tr_d.td_i = hintern;
	return curtree;
    }

    // Anything else that's not a BoT is a no-op for booleans
    if (ip->idb_minor_type != ID_BOT)
	return curtree;

    // Observed in Goliath example model with SKTRACKdrivewheel2.c comb - due
    // to the values in ts_mat, the BoT ends up inside-out when read in.
    int flip = bot_flipped(&tsp->ts_mat);

    // Phase C: variant BoT override.
    // If a perturbed variant was pre-tessellated for this leaf instance, use
    // it instead of the original BoT to avoid coplanar face issues.
    struct rt_db_internal var_intern;
    RT_DB_INTERNAL_INIT(&var_intern);
    bool var_loaded = false;
    struct rt_db_internal *effective_ip = ip;
    struct _ged_facetize_state *s = (struct _ged_facetize_state *)data;
    if (s && s->use_variant_plan && s->variant_plan) {
	FacetizeVariantPlan *vplan = (FacetizeVariantPlan *)s->variant_plan;
	char *path_str = db_path_to_string(pathp);
	/* Reconstruct the same role-keyed key used in plan.cpp Phase C:
	 * TS_SOFAR_MINUS is set when the leaf is on the subtractive side of
	 * any boolean node encountered above it in the current walk. */
	bool is_sub_ctx = (tsp->ts_sofar & TS_SOFAR_MINUS) != 0;
	std::string role_key = std::string(path_str) +
	    (is_sub_ctx ? "#sub" : "#base");
	bu_free(path_str, "path_str");
	auto it = vplan->inst_to_variant.find(role_key);
	if (it != vplan->inst_to_variant.end()) {
	    struct directory *vdp =
		db_lookup(tsp->ts_dbip, it->second.c_str(), LOOKUP_QUIET);
	    if (vdp && vdp->d_minor_type == ID_BOT) {
		if (rt_db_get_internal(&var_intern, vdp, tsp->ts_dbip, NULL) >= 0) {
		    effective_ip = &var_intern;
		    var_loaded = true;
		}
	    }
	    /* If variant lookup failed (no BoT yet), fall through to original */
	}
    }

    void *odata = NULL;
    ts_status = bot_to_manifold(s, &odata, tsp, effective_ip, flip, dp->d_namep);

    if (var_loaded)
	rt_db_free_internal(&var_intern);
    if (ts_status < 0) {
	if (s && s->tolerate_failures) {
	    facetize_tolerated_failure(s, "leaf '%s' omitted during boolean preparation: %s",
		    dp->d_namep,
		    (s->failure_msg && bu_vls_strlen(s->failure_msg)) ? bu_vls_cstr(s->failure_msg) : "unable to convert BoT to Manifold");
	    facetize_failure_clear(s);
	    return curtree;
	}

	if (s)
	    s->error_flag = 1;
	return TREE_NULL;
    }

    /* Diagnostic: log leaf name, role, and mesh SA */
    {
	bool is_sub_ctx = (tsp->ts_sofar & TS_SOFAR_MINUS) != 0;
	double leaf_sa = 0.0;
	if (odata) {
	    manifold::Manifold *lm = (manifold::Manifold *)odata;
	    leaf_sa = lm->SurfaceArea();
	}
	if (s && s->verbosity > 1) {
	    bu_log("[LEAF_TESS] name=%-30s  role=%s  mesh_SA=%.6f mm^2\n",
		   dp->d_namep,
		   is_sub_ctx ? "SUB " : "BASE",
		   leaf_sa);
	}
    }

    BU_GET(curtree, union tree);
    RT_TREE_INIT(curtree);
    curtree->tr_op = OP_TESS;
    curtree->tr_d.td_name = bu_strdup(dp->d_namep);
    curtree->tr_d.td_r = NULL;
    curtree->tr_d.td_d = odata;
    curtree->tr_d.td_i = NULL;

    bool should_log_treewalk = (s && s->verbosity > 1 && (RT_G_DEBUG & RT_DEBUG_TREEWALK));
    if (should_log_treewalk)
	bu_log("_booltree_leaf_tess(%s) OK\n", dp->d_namep);

    return curtree;
}


static union tree *
facetize_region_end(struct db_tree_state *tsp,
	const struct db_full_path *pathp,
	union tree *curtree,
	void *client_data)
{
    union tree **facetize_tree;

    if (tsp) RT_CK_DBTS(tsp);
    if (pathp) RT_CK_FULL_PATH(pathp);

    struct _ged_facetize_state *s = (struct _ged_facetize_state *)client_data;
    facetize_tree = &s->facetize_tree;

    if (curtree->tr_op == OP_NOP) return curtree;

    if (*facetize_tree) {
	union tree *tr;
	BU_ALLOC(tr, union tree);
	RT_TREE_INIT(tr);
	tr->tr_op = OP_UNION;
	tr->tr_b.tb_regionp = REGION_NULL;
	tr->tr_b.tb_left = *facetize_tree;
	tr->tr_b.tb_right = curtree;
	*facetize_tree = tr;
    } else {
	*facetize_tree = curtree;
    }

    /* Tree has been saved, and will be freed later */
    return TREE_NULL;
}

static int
manifold_do_bool(
	union tree *tp, union tree *tl, union tree *tr,
	int op, struct bu_list *UNUSED(vlfree), const struct bn_tol *UNUSED(tol), void *data)
{
    struct _ged_facetize_state *s = (struct _ged_facetize_state *)data;
    if (!s)
	return -1;

    // Translate op for MANIFOLD
    manifold::OpType manifold_op = manifold::OpType::Add;
    switch (op) {
	case OP_UNION:
	    manifold_op = manifold::OpType::Add;
	    break;
	case OP_INTERSECT:
	    manifold_op = manifold::OpType::Intersect;
	    break;
	case OP_SUBTRACT:
	    manifold_op = manifold::OpType::Subtract;
	    break;
	default:
	    manifold_op = manifold::OpType::Add;
    };

    // If we have a left half space, bail - that's not well defined for producing
    // a Manifold closed volume
    if (tl->tr_d.td_i) {
	facetize_failure(s, "unsupported boolean tree: left input '%s' to %s is a halfspace. Halfspaces must be used as right-side subtract/intersect operands for facetize Manifold evaluation.",
		tl->tr_d.td_name ? tl->tr_d.td_name : "(unknown)", bool_op_name(op));
	if (!s->tolerate_failures)
	    s->error_flag = 1;
	else {
	    facetize_tolerated_failure(s, "boolean subtree omitted: %s", bu_vls_cstr(s->failure_msg));
	    facetize_failure_clear(s);
	}
	return -1;
    }

    // By this point we should have prepared our Manifold inputs - now
    // it's a question of doing the evaluation.
    manifold::Manifold *lm = (manifold::Manifold *)tl->tr_d.td_d;
    manifold::Manifold *rm = (manifold::Manifold *)tr->tr_d.td_d;
    manifold::Manifold *result = NULL;
    int failed = 0;
    bool delete_left = false;
    // On the right we can either have a Manifold, or a half space.  If it's
    // the latter, we need special handling.
    bool delete_right = false;
    if (tr->tr_d.td_i) {
	if (tr->tr_d.td_i->idb_minor_type != ID_HALF) {
	    facetize_failure(s, "unsupported boolean tree: right input '%s' to %s has internal type %d, expected halfspace",
		    tr->tr_d.td_name ? tr->tr_d.td_name : "(unknown)", bool_op_name(op), tr->tr_d.td_i->idb_minor_type);
	    if (!s->tolerate_failures)
		s->error_flag = 1;
	    else {
		facetize_tolerated_failure(s, "boolean subtree omitted: %s", bu_vls_cstr(s->failure_msg));
		facetize_failure_clear(s);
	    }
	    return -1;
	}
	if (!lm) {
	    lm = new manifold::Manifold();
	    delete_left = true;
	}
	struct rt_half_internal *hf_ip= (struct rt_half_internal *)tr->tr_d.td_i->idb_ptr;
	if (manifold_op != manifold::OpType::Add) {

	    // Intersections and Subtractions with half spaces are handled
	    // by Manifold routines
	    vect_t pn;
	    pn[0] = hf_ip->eqn[0];
	    pn[1] = hf_ip->eqn[1];
	    pn[2] = hf_ip->eqn[2];
	    if (op == OP_INTERSECT)
		VSCALE(pn, pn, -1);
	    manifold::Manifold trimmed = lm->TrimByPlane(linalg::vec<double, 3>(pn[0], pn[1], pn[2]), hf_ip->eqn[3]);
	    result = new manifold::Manifold(trimmed);
	}

	BU_PUT(hf_ip, struct rt_half_internal);
	BU_PUT(tr->tr_d.td_i, struct rt_db_internal);
	tr->tr_d.td_i = NULL;
    }

    // Anything not already set up or handled is a no-op
    if (!lm) {
	lm = new manifold::Manifold();
	delete_left = true;
    }
    if (!rm) {
	rm = new manifold::Manifold();
	delete_right = true;
    }

    if (!result) {

	// We should have valid inputs - proceed
	facetize_log(s, 1, "Trying boolean op:  %s, %s\n", tl->tr_d.td_name, tr->tr_d.td_name);

	static const char *op_names[] = {"ADD","INTERSECT","SUBTRACT","ADD"};
	int opidx = (op == OP_INTERSECT) ? 1 : (op == OP_SUBTRACT) ? 2 : 0;
	if (s->verbosity > 1) {
	    bu_log("[BOOL_OP] %-8s L=%-30s SA=%.4f  R=%-30s SA=%.4f\n",
		   op_names[opidx],
		   tl->tr_d.td_name, lm->SurfaceArea(),
		   tr->tr_d.td_name, rm->SurfaceArea());
	}

	manifold::Manifold bool_out;
	try {
	    bool_out = lm->Boolean(*rm, manifold_op);
	} catch (...) {
	    facetize_failure(s, "Manifold boolean %s threw an exception for left '%s' and right '%s'",
		    bool_op_name(op),
		    tl->tr_d.td_name ? tl->tr_d.td_name : "(unknown)",
		    tr->tr_d.td_name ? tr->tr_d.td_name : "(unknown)");
	    // write out the failing inputs to files to aid in debugging
	    const char *evar = getenv("GED_MANIFOLD_DEBUG");
	    if (evar && strlen(evar)) {
		std::cerr << "Manifold op: " << (int)manifold_op << "\n";
		std::ofstream lofile, rofile;
		lofile.open(std::string(tl->tr_d.td_name)+std::string(".obj"));
		rofile.open(std::string(tr->tr_d.td_name)+std::string(".obj"));
		lm->WriteOBJ(lofile); rm->WriteOBJ(rofile);
		lofile.close(); rofile.close();
		bu_exit(EXIT_FAILURE, "Exiting to avoid overwriting debug outputs from Manifold boolean failure.");
	    }
	    failed = 1;
	}

	if (!failed) {
	    if (bool_out.Status() != manifold::Manifold::Error::NoError) {
		facetize_failure(s, "Manifold boolean %s failed for left '%s' and right '%s': %s",
			bool_op_name(op),
			tl->tr_d.td_name ? tl->tr_d.td_name : "(unknown)",
			tr->tr_d.td_name ? tr->tr_d.td_name : "(unknown)",
			manifold::ToString(bool_out.Status()).c_str());
		failed = 1;
	    }
	}

	if (!failed) {
	    if (s->verbosity > 1) {
		bu_log("[BOOL_OP] %-8s L=%-30s  R=%-30s  result_SA=%.4f\n",
		       op_names[opidx],
		       tl->tr_d.td_name, tr->tr_d.td_name,
		       bool_out.SurfaceArea());
	    }
	    result = new manifold::Manifold(bool_out);
	}

	// If we're debugging and need to capture OBJ meshes for "successful" cases can use GED_MANIFOLD_DEBUG env var.
	const char *evar = getenv("GED_MANIFOLD_DEBUG");
	if (evar && strlen(evar)) {
	    std::ofstream lofile, rofile, oofile;
	    lofile.open(std::string(tl->tr_d.td_name)+std::string(".obj"));
	    rofile.open(std::string(tr->tr_d.td_name)+std::string(".obj"));
	    oofile.open(std::string("out-") + std::string(tl->tr_d.td_name)+std::to_string(op)+std::string(tr->tr_d.td_name)+std::string(".obj"));
	    lm->WriteOBJ(lofile); rm->WriteOBJ(rofile); bool_out.WriteOBJ(oofile);
	    lofile.close(); rofile.close(); oofile.close();
	}
    }

    // Memory cleanup
    if (delete_left)
	delete lm;
    if (delete_right)
	delete rm;

    if (tl->tr_d.td_d) {
	manifold::Manifold *m = (manifold::Manifold *)tl->tr_d.td_d;
	delete m;
	tl->tr_d.td_d = NULL;
    }
    if (tr->tr_d.td_d) {
	manifold::Manifold *m = (manifold::Manifold *)tr->tr_d.td_d;
	delete m;
	tr->tr_d.td_d = NULL;
    }

    if (failed) {
	if (!s->tolerate_failures) {
	    s->error_flag = 1;
	} else {
	    facetize_tolerated_failure(s, "boolean %s subtree omitted: %s",
		    bool_op_name(op),
		    (s->failure_msg && bu_vls_strlen(s->failure_msg)) ? bu_vls_cstr(s->failure_msg) : "Manifold boolean evaluation failed");
	    facetize_failure_clear(s);
	}
	tp->tr_d.td_d = NULL;
	return -1;
    }

    tp->tr_op = OP_TESS;
    tp->tr_d.td_d = (void *)result;
    return 0;
}

std::vector<std::string>
tess_avail_methods()
{
    // Build up the path to the ged_exec executable
    char tess_exec[MAXPATHLEN];
    bu_dir(tess_exec, MAXPATHLEN, BU_DIR_BIN, "ged_exec", BU_DIR_EXT, NULL);

    const char *tess_cmd[4] = {NULL};
    tess_cmd[ 0] = tess_exec;
    tess_cmd[ 1] = "facetize_process";
    tess_cmd[ 2] = "--list-methods";
    tess_cmd[ 3] = NULL;

    struct bu_process *p = NULL;
    bu_process_create(&p, tess_cmd,
	    BU_PROCESS_HIDE_WINDOW | BU_PROCESS_OUT_EQ_ERR);
    if (!p) {
	bu_log("Unable to start %s %s\n", tess_cmd[0], tess_cmd[1]);
	return std::vector<std::string>();
    }

    char buffer[FACETIZE_IO_BUFFER_SIZE];
    std::string mstr;
    int read_res = 0;
    while ((read_res = bu_process_read_n(p, BU_PROCESS_STDOUT,
		    (int)sizeof(buffer), buffer)) > 0)
	mstr.append(buffer, (size_t)read_res);

    if (bu_process_wait_n(&p, 0) || mstr.empty()) {
	// wait error or read error
	bu_log("%s %s - wait or read error\n", tess_cmd[0], tess_cmd[1]);
	return std::vector<std::string>();
    }

    std::stringstream mstream(mstr);
    std::string m;
    std::vector<std::string> methods;
    while (std::getline(mstream, m, ' ')) {
	methods.push_back(m);
    }

    return methods;
}

static int
tess_write_probe(const char *work_file, double *written_bytes,
	double *write_usec)
{
    if (!work_file || !written_bytes || !write_usec)
	return BRLCAD_ERROR;

    *written_bytes = 0.0;
    *write_usec = 0.0;
    std::string probe_file = std::string(work_file) + ".io_probe";
    (void)bu_file_delete(probe_file.c_str());

    std::vector<char> probe_data(FACETIZE_IO_PROBE_CHUNK_BYTES, 0);
    std::ofstream output(probe_file.c_str(),
	    std::ios::binary | std::ios::trunc);
    if (!output.is_open())
	return BRLCAD_ERROR;

    size_t total_written = 0;
    int64_t start = bu_gettime();
    while (total_written < FACETIZE_IO_PROBE_MAX_BYTES) {
	size_t write_size = std::min(FACETIZE_IO_PROBE_CHUNK_BYTES,
		FACETIZE_IO_PROBE_MAX_BYTES - total_written);
	output.write(probe_data.data(), (std::streamsize)write_size);
	if (!output.good())
	    break;
	total_written += write_size;
	if (bu_gettime() - start >=
		BU_SEC2USEC(FACETIZE_IO_PROBE_DURATION_SEC))
	    break;
    }
    output.flush();
    output.close();
    bool write_ok = output.good();
    int64_t elapsed = bu_gettime() - start;
    bool deleted = bu_file_delete(probe_file.c_str());
    if (!write_ok || !deleted || !total_written || elapsed <= 0)
	return BRLCAD_ERROR;

    *written_bytes = (double)total_written;
    *write_usec = (double)elapsed;
    return BRLCAD_OK;
}

struct tess_worker_state {
    struct bu_process *process = NULL;
    FILE *input = NULL;
    FacetizeWorkerClient channel;
    FacetizeWorkerStatus status;
    int object_index = -1;
    int64_t request_start = 0;
    int64_t write_start = 0;
    int64_t write_deadline = 0;
    double write_timeout = 0.0;
    std::string result_file;
    int commit_retries = 0;
    bool enabled = true;
    bool write_permitted = false;
    bool awaiting_commit = false;
};

struct tess_writer_state {
    struct bu_process *process = NULL;
    FILE *input = NULL;
    FacetizeWorkerClient channel;
    FacetizeWorkerStatus status;
    size_t worker_index = SIZE_MAX;
    int64_t write_start = 0;
    int64_t write_deadline = 0;
    double write_timeout = 0.0;
    bool write_permitted = false;
};

static void
tess_worker_request_reset(struct tess_worker_state &worker)
{
    worker.status = FacetizeWorkerStatus();
    worker.object_index = -1;
    worker.request_start = 0;
    worker.write_start = 0;
    worker.write_deadline = 0;
    worker.write_timeout = 0.0;
    worker.commit_retries = 0;
    worker.write_permitted = false;
    worker.awaiting_commit = false;
}

static void
tess_worker_stop(struct _ged_facetize_state *s, struct tess_worker_state &worker,
	bool terminate)
{
    if (worker.process)
	(void)facetize_process_stop(s, &worker.process, worker.input,
		worker.channel, terminate);
    worker.input = NULL;
    worker.channel.reset(NULL);
    tess_worker_request_reset(worker);
}

static void
tess_writer_request_reset(struct tess_writer_state &writer)
{
    writer.status = FacetizeWorkerStatus();
    writer.worker_index = SIZE_MAX;
    writer.write_start = 0;
    writer.write_deadline = 0;
    writer.write_timeout = 0.0;
    writer.write_permitted = false;
}

static void
tess_writer_stop(struct _ged_facetize_state *s, struct tess_writer_state &writer,
	bool terminate)
{
    if (writer.process)
	(void)facetize_process_stop(s, &writer.process, writer.input,
		writer.channel, terminate);
    writer.input = NULL;
    writer.channel.reset(NULL);
    tess_writer_request_reset(writer);
}

static int
tess_run(struct _ged_facetize_state *s, const char **tess_cmd,
	int tess_cmd_cnt, fastf_t max_time, int ocnt,
	std::vector<std::string> *failed_names, enum tess_work_type work_type,
	bool replay)
{
    if (!s || !tess_cmd || !tess_cmd[3] || ocnt <= 0 ||
	    ocnt > tess_cmd_cnt || !failed_names)
	return BRLCAD_ERROR;
    failed_names->clear();

    const int fixed_cnt = tess_cmd_cnt - ocnt;
    const char *work_file = tess_cmd[3];
    if (!s->write_profiled) {
	if (tess_write_probe(work_file, &s->write_profile_bytes,
		&s->write_profile_usec) != BRLCAD_OK) {
	    facetize_log(s, 0,
		    "FACETIZE: unable to profile database writes in the working cache\n");
	    return BRLCAD_ERROR;
	}
	s->write_profiled = 1;
	double bytes_per_second = s->write_profile_bytes *
	    FACETIZE_USEC_TO_SEC_DIVISOR / s->write_profile_usec;
	facetize_log(s, 1,
		"FACETIZE: measured working-cache write speed: %.1f MiB/s using %.1f MiB\n",
		bytes_per_second / FACETIZE_MIB_BYTES,
		s->write_profile_bytes / FACETIZE_MIB_BYTES);
    }

    std::string backup_file = std::string(work_file) + ".bak";
    if (facetize_file_copy(work_file, backup_file.c_str()) != BRLCAD_OK) {
	bu_file_delete(backup_file.c_str());
	facetize_log(s, 0, "FACETIZE: unable to back up working database %s\n",
		work_file);
	return BRLCAD_ERROR;
    }

    ssize_t total_memory_result = bu_mem(BU_MEM_ALL, NULL);
    ssize_t available_memory_result = bu_mem(BU_MEM_AVAIL, NULL);
    size_t total_memory = (total_memory_result > 0) ?
	(size_t)total_memory_result : 0;
    size_t available_memory = (available_memory_result > 0) ?
	(size_t)available_memory_result : 0;
    size_t requested_workers = replay ? 1 : (size_t)s->max_workers;
    FacetizeWorkerPolicy worker_policy(requested_workers, (size_t)ocnt,
	bu_avail_cpus(), total_memory, available_memory);
    bool staged_writes = worker_policy.worker_count() > 1;

    std::string worker_threads = std::to_string(
	    worker_policy.threads_per_worker());
    std::vector<std::string> worker_cmd;
    for (int i = 0; i < fixed_cnt; i++)
	worker_cmd.push_back(tess_cmd[i]);
    worker_cmd.push_back("--threads");
    worker_cmd.push_back(worker_threads);

    std::vector<std::string> writer_cmd;
    if (staged_writes) {
	writer_cmd.push_back(tess_cmd[0]);
	writer_cmd.push_back(tess_cmd[1]);
	writer_cmd.push_back(work_file);
    }

    const char *method = (tess_cmd_cnt > FACETIZE_METHOD_COMMAND_INDEX &&
	    tess_cmd[FACETIZE_METHOD_COMMAND_INDEX]) ?
	tess_cmd[FACETIZE_METHOD_COMMAND_INDEX] : "unknown";
    int64_t run_start = bu_gettime();
    int64_t next_progress = run_start +
	BU_SEC2USEC(FACETIZE_PROGRESS_INTERVAL_SEC);
    int start_msg_level = (ocnt >= FACETIZE_PROGRESS_BATCH_MIN) ? 0 : 1;
    const char *work_description = tess_work_description(work_type, ocnt);
    if (replay) {
	facetize_log(s, start_msg_level,
		"FACETIZE: retrying tessellation of %d %s after restoring the working database, using method %s...\n",
		ocnt, work_description, method);
    } else {
	facetize_log(s, start_msg_level,
		"FACETIZE: tessellating %d %s with method %s...\n",
		ocnt, work_description, method);
    }
    facetize_log(s, start_msg_level,
	    "FACETIZE: using %zu tessellation worker%s with up to %zu thread%s each\n",
	    worker_policy.worker_count(),
	    (worker_policy.worker_count() == 1) ? "" : "s",
	    worker_policy.threads_per_worker(),
	    (worker_policy.threads_per_worker() == 1) ? "" : "s");

    bool unsafe_failure = false;
    std::vector<tess_worker_state> workers(worker_policy.worker_count());
    tess_writer_state writer;
    std::string no_result_file;
    if (staged_writes) {
	if (facetize_process_start(&writer.process, &writer.input,
		writer.channel, writer_cmd, no_result_file,
		"--writer") != BRLCAD_OK) {
	    tess_writer_stop(s, writer, true);
	    bu_file_delete(backup_file.c_str());
	    facetize_log(s, 0,
		    "FACETIZE: unable to start the working database writer\n");
	    return BRLCAD_ERROR;
	}
	for (size_t i = 0; i < workers.size(); i++) {
	    workers[i].result_file = std::string(work_file) +
		FACETIZE_WORKER_FILE_SUFFIX + std::to_string(i) + ".g";
	    (void)bu_file_delete(workers[i].result_file.c_str());
	}
    }
    size_t active_workers = 0;
    size_t observed_worker_resident = 0;
    size_t write_owner = workers.size();
    int next_object = fixed_cnt;
    int completed_objects = 0;

    auto submit_staged_result = [&](size_t worker_index) {
	if (worker_index >= workers.size())
	    return false;
	tess_worker_state &worker = workers[worker_index];
	if (!writer.process && facetize_process_start(&writer.process,
		&writer.input, writer.channel, writer_cmd, no_result_file,
		"--writer") != BRLCAD_OK) {
	    tess_writer_stop(s, writer, true);
	    return false;
	}
	const char *object_name = tess_cmd[worker.object_index];
	if (!writer.channel.send_commit(worker.result_file.c_str(), object_name,
		worker.status.payload_size)) {
	    tess_writer_stop(s, writer, true);
	    return false;
	}
	tess_writer_request_reset(writer);
	writer.worker_index = worker_index;
	worker.awaiting_commit = true;
	return true;
    };

    while (completed_objects < ocnt && !unsafe_failure) {
	bool made_progress = false;
	if (staged_writes && writer.worker_index == SIZE_MAX &&
		writer.process && bu_process_poll(writer.process, NULL) != 0)
	    tess_writer_stop(s, writer, false);

	for (tess_worker_state &worker : workers) {
	    if (!worker.enabled || worker.object_index >= 0 ||
		    next_object >= tess_cmd_cnt)
		continue;

	    available_memory_result = bu_mem(BU_MEM_AVAIL, NULL);
	    available_memory = (available_memory_result > 0) ?
		(size_t)available_memory_result : 0;
	    if (!worker_policy.can_dispatch(active_workers, available_memory,
		    observed_worker_resident))
		break;

	    if (worker.process && bu_process_poll(worker.process, NULL) != 0)
		tess_worker_stop(s, worker, false);
	    if (!worker.process && facetize_process_start(&worker.process,
		    &worker.input, worker.channel, worker_cmd,
		    worker.result_file, "--server") != BRLCAD_OK) {
		tess_worker_stop(s, worker, true);
		worker.enabled = false;
		facetize_log(s, 0,
			"FACETIZE: unable to start a tessellation worker\n");
		continue;
	    }

	    const char *object_name = tess_cmd[next_object];
	    if (!worker.channel.send_request(object_name)) {
		facetize_log(s, 0,
			"FACETIZE: unable to submit %s to a tessellation worker\n",
			object_name);
		failed_names->push_back(object_name);
		tess_worker_stop(s, worker, true);
		next_object++;
		completed_objects++;
		made_progress = true;
		continue;
	    }

	    worker.status = FacetizeWorkerStatus();
	    worker.object_index = next_object++;
	    worker.request_start = bu_gettime();
	    active_workers++;
	    made_progress = true;
	}

	if (!active_workers && next_object < tess_cmd_cnt) {
	    bool usable_worker = false;
	    for (const tess_worker_state &worker : workers)
		usable_worker = usable_worker || worker.enabled;
	    if (!usable_worker) {
		for (; next_object < tess_cmd_cnt; next_object++) {
		    failed_names->push_back(tess_cmd[next_object]);
		    completed_objects++;
		}
		break;
	    }
	}

	for (size_t worker_index = 0; worker_index < workers.size();
		worker_index++) {
	    tess_worker_state &worker = workers[worker_index];
	    if (worker.object_index < 0)
		continue;
	    if (worker.awaiting_commit)
		continue;

	    const char *object_name = tess_cmd[worker.object_index];
	    facetize_process_drain_stdout(s, worker.process, worker.channel,
		    &worker.status);
	    facetize_process_drain_stderr(s, worker.process);
	    observed_worker_resident = std::max(observed_worker_resident,
		    worker.status.resident_size);

	    int64_t now = bu_gettime();
	    bool request_interrupted = false;
	    bool tessellation_timed_out = false;
	    bool write_timed_out = false;

	    if (worker.status.result_received) {
		if (worker.status.write_started && !worker.status.write_done) {
		    request_interrupted = true;
		} else {
		    bool commit_submission_failed = false;
		    if (worker.status.write_done &&
			    worker.status.result == BRLCAD_OK &&
			    now > worker.write_start &&
			    worker.status.payload_size >=
			    FACETIZE_WRITE_PROFILE_MIN_BYTES) {
			s->write_profile_bytes += worker.status.payload_size;
			s->write_profile_usec += now - worker.write_start;
		    }
		    if (worker.status.write_done &&
			    worker.status.result == BRLCAD_OK && staged_writes) {
			commit_submission_failed =
			    !submit_staged_result(worker_index);
			if (!commit_submission_failed) {
			    made_progress = true;
			    continue;
			}
			worker.status.result = BRLCAD_ERROR;
		    }

		    if (worker.status.result != BRLCAD_OK) {
			bool canonical_write_ambiguous =
			    worker.status.write_started && !staged_writes;
			if (!canonical_write_ambiguous)
			    failed_names->push_back(object_name);
			if (commit_submission_failed) {
			    facetize_log(s, 0,
				    "FACETIZE: unable to submit staged tessellation for %s to the database writer with method %s\n",
				    object_name, method);
			} else if (worker.status.write_started && !staged_writes) {
			    facetize_log(s, 0,
				    "FACETIZE: failed to write tessellation for %s with method %s\n",
				    object_name, method);
			    unsafe_failure = true;
			} else if (worker.status.write_started) {
			    facetize_log(s, 0,
				    "FACETIZE: failed to stage tessellation for %s with method %s\n",
				    object_name, method);
			} else {
			    facetize_log(s, 0,
				    "FACETIZE: tessellation failed for %s with method %s\n",
				    object_name, method);
			}
		    }

		    if (write_owner == worker_index)
			write_owner = workers.size();
		    bool restart_worker = staged_writes &&
			worker.status.write_started &&
			worker.status.result != BRLCAD_OK &&
			!commit_submission_failed;
		    if (restart_worker) {
			tess_worker_stop(s, worker, true);
		    } else {
			tess_worker_request_reset(worker);
		    }
		    active_workers--;
		    completed_objects++;
		    made_progress = true;
		    if (unsafe_failure)
			break;
		    continue;
		}
	    }

	    if (!request_interrupted && worker.status.write_ready &&
		    !worker.write_permitted && write_owner == workers.size()) {
		if (max_time > 0 && now - worker.request_start >=
			BU_SEC2USEC(max_time)) {
		    request_interrupted = true;
		    tessellation_timed_out = true;
		} else {
		    /* Reserve the write slot while the child acknowledges permission
		     * and performs the update. */
		    worker.write_permitted = true;
		    write_owner = worker_index;
	worker.write_timeout = facetize_write_timeout_seconds(
			    worker.status.payload_size, s->write_profile_bytes,
			    s->write_profile_usec);
		    facetize_log(s, 1,
			    "FACETIZE: worker ready to write %zu bytes for %s (%.1f second limit; resident %.1f MiB)\n",
			    worker.status.payload_size, object_name,
			    worker.write_timeout,
			    worker.status.resident_size /
			    (double)FACETIZE_MIB_BYTES);
		    worker.write_start = now;
		    worker.write_deadline = now +
			BU_SEC2USEC(worker.write_timeout);
		    if (!worker.channel.send_write_proceed())
			request_interrupted = true;
		    made_progress = true;
		}
	    }

	    if (!request_interrupted &&
		    bu_process_poll(worker.process, NULL) != 0)
		request_interrupted = true;
	    if (!request_interrupted && !worker.status.write_ready &&
		    max_time > 0 && now - worker.request_start >=
		    BU_SEC2USEC(max_time)) {
		request_interrupted = true;
		tessellation_timed_out = true;
	    }
	    if (!request_interrupted && worker.write_permitted &&
		    now >= worker.write_deadline) {
		request_interrupted = true;
		write_timed_out = true;
	    }

	    if (request_interrupted) {
		if (write_timed_out) {
		    facetize_log(s, 0,
			    "FACETIZE: %s write timed out after %.1f seconds for %s with method %s\n",
			    staged_writes ? "staging" : "database",
			    worker.write_timeout, object_name, method);
		} else if (tessellation_timed_out) {
		    facetize_log(s, 0,
			    "FACETIZE: tessellation timed out after %.1f seconds for %s with method %s\n",
			    max_time, object_name, method);
		} else if (worker.status.write_started) {
		    facetize_log(s, 0,
			    "FACETIZE: tessellation worker failed while writing %s%s with method %s\n",
			    object_name, staged_writes ? " to staging" : "",
			    method);
		} else if (worker.write_permitted) {
		    facetize_log(s, 0,
			    "FACETIZE: tessellation worker failed before acknowledging write start for %s with method %s\n",
			    object_name, method);
		} else {
		    facetize_log(s, 0,
			    "FACETIZE: tessellation worker failed while processing %s with method %s\n",
			    object_name, method);
		}
		bool write_was_started = worker.status.write_started;
		if (!write_was_started || staged_writes)
		    failed_names->push_back(object_name);
		if (write_owner == worker_index)
		    write_owner = workers.size();
		tess_worker_stop(s, worker, true);
		active_workers--;
		completed_objects++;
		unsafe_failure = write_was_started && !staged_writes;
		made_progress = true;
		if (unsafe_failure)
		    break;
	    }
	}

	if (!unsafe_failure && staged_writes &&
		writer.worker_index < workers.size()) {
	    tess_worker_state &committing_worker =
		workers[writer.worker_index];
	    const char *object_name =
		tess_cmd[committing_worker.object_index];
	    facetize_process_drain_stdout(s, writer.process, writer.channel,
		    &writer.status);
	    facetize_process_drain_stderr(s, writer.process);
	    observed_worker_resident = std::max(observed_worker_resident,
		    writer.status.resident_size);

	    int64_t now = bu_gettime();
	    bool writer_interrupted = false;
	    bool writer_timed_out = false;
	    if (writer.status.result_received) {
		if (writer.status.write_started && !writer.status.write_done) {
		    writer_interrupted = true;
		} else {
		    if (writer.status.write_done &&
			    writer.status.result == BRLCAD_OK &&
			    now > writer.write_start &&
			    committing_worker.status.payload_size >=
			    FACETIZE_WRITE_PROFILE_MIN_BYTES) {
			s->write_profile_bytes +=
			    committing_worker.status.payload_size;
			s->write_profile_usec += now - writer.write_start;
		    }
		    if (writer.status.result != BRLCAD_OK) {
			if (!writer.status.write_started)
			    failed_names->push_back(object_name);
			facetize_log(s, 0,
				"FACETIZE: failed to transfer tessellation for %s to the working database with method %s\n",
				object_name, method);
			unsafe_failure = writer.status.write_started;
		    }

		    write_owner = workers.size();
		    tess_worker_request_reset(committing_worker);
		    active_workers--;
		    completed_objects++;
		    tess_writer_request_reset(writer);
		    made_progress = true;
		}
	    }

	    if (!writer_interrupted && writer.worker_index < workers.size() &&
		    writer.status.write_ready && !writer.write_permitted) {
		writer.write_permitted = true;
		writer.write_timeout = facetize_write_timeout_seconds(
			committing_worker.status.payload_size,
			s->write_profile_bytes, s->write_profile_usec);
		writer.write_start = now;
		writer.write_deadline = now +
		    BU_SEC2USEC(writer.write_timeout);
		facetize_log(s, 1,
			"FACETIZE: database writer ready to commit %zu bytes for %s (%.1f second limit; resident %.1f MiB)\n",
			committing_worker.status.payload_size, object_name,
			writer.write_timeout,
			writer.status.resident_size /
			(double)FACETIZE_MIB_BYTES);
		if (!writer.channel.send_write_proceed())
		    writer_interrupted = true;
		made_progress = true;
	    }
	    if (!writer_interrupted && writer.worker_index < workers.size() &&
		    bu_process_poll(writer.process, NULL) != 0)
		writer_interrupted = true;
	    if (!writer_interrupted && writer.worker_index < workers.size() &&
		    writer.write_permitted && now >= writer.write_deadline) {
		writer_interrupted = true;
		writer_timed_out = true;
	    }

	    if (writer_interrupted) {
		if (writer_timed_out) {
		    facetize_log(s, 0,
			    "FACETIZE: working database write timed out after %.1f seconds for %s with method %s\n",
			    writer.write_timeout, object_name, method);
		} else if (writer.status.write_started) {
		    facetize_log(s, 0,
			    "FACETIZE: database writer failed while committing %s with method %s\n",
			    object_name, method);
		} else if (writer.write_permitted) {
		    facetize_log(s, 0,
			    "FACETIZE: database writer failed before acknowledging write start for %s with method %s\n",
			    object_name, method);
		} else {
		    facetize_log(s, 0,
			    "FACETIZE: database writer failed before committing %s with method %s\n",
			    object_name, method);
		}
		bool canonical_write_started = writer.status.write_started;
		size_t interrupted_worker_index = writer.worker_index;
		tess_writer_stop(s, writer, true);
		if (!canonical_write_started &&
			committing_worker.commit_retries <
			FACETIZE_WRITER_RESTART_LIMIT) {
		    committing_worker.commit_retries++;
		    if (submit_staged_result(interrupted_worker_index)) {
			facetize_log(s, 1,
				"FACETIZE: resubmitted staged tessellation for %s after the database writer exited before acknowledging write start\n",
				object_name);
			made_progress = true;
			continue;
		    }
		}
		if (!canonical_write_started)
		    failed_names->push_back(object_name);
		write_owner = workers.size();
		tess_worker_request_reset(committing_worker);
		active_workers--;
		completed_objects++;
		unsafe_failure = canonical_write_started;
		made_progress = true;
	    }
	}

	int64_t now = bu_gettime();
	if (!unsafe_failure && now >= next_progress) {
	    if (writer.worker_index < workers.size()) {
		const tess_worker_state &committing_worker =
		    workers[writer.worker_index];
		if (writer.status.write_started) {
		    facetize_log(s, 0,
			    "FACETIZE: committing tessellation for %s with method %s (%d of %d complete, %zu workers active, %.1f seconds elapsed; %.1f second write limit)\n",
			    tess_cmd[committing_worker.object_index], method,
			    completed_objects, ocnt, active_workers,
			    (now - run_start) / FACETIZE_USEC_TO_SEC_DIVISOR,
			    writer.write_timeout);
		} else {
		    facetize_log(s, 0,
			    "FACETIZE: waiting to commit tessellation for %s with method %s (%d of %d complete, %zu workers active, %.1f seconds elapsed)\n",
			    tess_cmd[committing_worker.object_index], method,
			    completed_objects, ocnt, active_workers,
			    (now - run_start) / FACETIZE_USEC_TO_SEC_DIVISOR);
		}
	    } else if (write_owner < workers.size()) {
		const tess_worker_state &writing_worker = workers[write_owner];
		facetize_log(s, 0,
			"FACETIZE: writing tessellation for %s with method %s (%d of %d complete, %zu workers active, %.1f seconds elapsed; %.1f second write limit)\n",
			tess_cmd[writing_worker.object_index], method, completed_objects,
			ocnt, active_workers,
			(now - run_start) / FACETIZE_USEC_TO_SEC_DIVISOR,
			writing_worker.write_timeout);
	    } else {
		facetize_log(s, 0,
			"FACETIZE: tessellating with method %s (%d of %d complete, %zu workers active, %.1f seconds elapsed)\n",
			method, completed_objects, ocnt, active_workers,
			(now - run_start) / FACETIZE_USEC_TO_SEC_DIVISOR);
	    }
	    next_progress = now + BU_SEC2USEC(FACETIZE_PROGRESS_INTERVAL_SEC);
	}

	if (!made_progress && !unsafe_failure)
	    (void)bu_snooze(FACETIZE_POLL_INTERVAL_USEC);
    }

    int abnormal_workers = 0;
    if (writer.process) {
	if (!unsafe_failure) {
	    bu_process_file_close(writer.process, BU_PROCESS_STDIN);
	    writer.input = NULL;
	}
	int writer_status = facetize_process_reap(s, &writer.process,
		writer.channel, unsafe_failure);
	writer.channel.reset(NULL);
	if (writer_status != BRLCAD_OK)
	    abnormal_workers++;
    }
    for (tess_worker_state &worker : workers) {
	if (worker.process) {
	    if (!unsafe_failure) {
		bu_process_file_close(worker.process, BU_PROCESS_STDIN);
		worker.input = NULL;
	    }
	    int process_status = facetize_process_reap(s, &worker.process,
		    worker.channel, unsafe_failure);
	    worker.channel.reset(NULL);
	    if (process_status != BRLCAD_OK)
		abnormal_workers++;
	}
	if (!worker.result_file.empty())
	    (void)bu_file_delete(worker.result_file.c_str());
    }
    if (!unsafe_failure) {
	if (abnormal_workers)
	    facetize_log(s, 0,
		    "FACETIZE: %d subprocess%s exited abnormally after reporting object results\n",
		    abnormal_workers, (abnormal_workers == 1) ? "" : "s");
	double elapsed_seconds = (bu_gettime() - run_start) /
	    FACETIZE_USEC_TO_SEC_DIVISOR;
	int completion_msg_level = (start_msg_level == 0 ||
		elapsed_seconds >= FACETIZE_PROGRESS_INTERVAL_SEC) ? 0 : 1;
	facetize_log(s, completion_msg_level,
		"FACETIZE: tessellation complete: %d of %d %s succeeded with method %s (%.1f seconds)\n",
		ocnt - (int)failed_names->size(), ocnt, work_description, method,
		elapsed_seconds);
	bu_file_delete(backup_file.c_str());
	return failed_names->empty() ? BRLCAD_OK : BRLCAD_ERROR;
    }

    if (facetize_file_copy(backup_file.c_str(), work_file) != BRLCAD_OK) {
	facetize_log(s, 0,
		"FACETIZE: unable to restore working database %s after tessellation failure\n",
		work_file);
	return BRLCAD_ERROR;
    }
    bu_file_delete(backup_file.c_str());

    /* A second ambiguous write failure means the serial recovery attempt did
     * not produce a trustworthy database.  Its checkpoint predates all work
     * in this batch, so every requested result is absent after restoration. */
    if (replay) {
	for (int obj_ind = fixed_cnt; obj_ind < tess_cmd_cnt; obj_ind++)
	    failed_names->push_back(tess_cmd[obj_ind]);
	std::sort(failed_names->begin(), failed_names->end());
	failed_names->erase(std::unique(failed_names->begin(),
	    failed_names->end()), failed_names->end());
	facetize_log(s, 0,
		"FACETIZE: serial recovery failed while updating the working database\n");
	return BRLCAD_ERROR;
    }

    // An ambiguous canonical database write may be partial.  Restore the
    // checkpoint, then replay only names not already tied to a known failure.
    std::set<std::string> failed_set(failed_names->begin(),
	    failed_names->end());
    const char *retry_cmd[MAXPATHLEN] = {NULL};
    for (int i = 0; i < fixed_cnt; i++)
	retry_cmd[i] = tess_cmd[i];
    int retry_cnt = fixed_cnt;
    for (int obj_ind = fixed_cnt; obj_ind < tess_cmd_cnt; obj_ind++) {
	if (failed_set.find(tess_cmd[obj_ind]) == failed_set.end())
	    retry_cmd[retry_cnt++] = tess_cmd[obj_ind];
    }

    if (retry_cnt > fixed_cnt) {
	std::vector<std::string> retry_failures;
	(void)tess_run(s, retry_cmd, retry_cnt, max_time,
		retry_cnt - fixed_cnt, &retry_failures, work_type, true);
	failed_names->insert(failed_names->end(), retry_failures.begin(),
		retry_failures.end());
    }

    std::sort(failed_names->begin(), failed_names->end());
    failed_names->erase(std::unique(failed_names->begin(),
	    failed_names->end()), failed_names->end());
    return failed_names->empty() ? BRLCAD_OK : BRLCAD_ERROR;
}

/*
 * Tessellate variant primitives that were created by _ged_facetize_build_variant_plan().
 * Processes all names using the NMG method (same fixed command structure as
 * _ged_facetize_leaves_tri).  Tessellation failures are logged but do not
 * abort: the booleval will silently fall back to the original (non-variant)
 * mesh for any variant whose BoT is not available.
 */
int
_ged_facetize_tessellate_variant_names(struct _ged_facetize_state *s,
				       FacetizeVariantPlan *plan)
{
    if (!s || !plan || plan->variant_names.empty())
	return BRLCAD_OK;

    char tess_exec[MAXPATHLEN];
    bu_dir(tess_exec, MAXPATHLEN, BU_DIR_BIN, "ged_exec", BU_DIR_EXT, NULL);

    char lcache[MAXPATHLEN] = {0};
    bu_dir(lcache, MAXPATHLEN, BU_DIR_CACHE, NULL);

    method_options_t *mo = (method_options_t *)s->method_opts;
    std::string mstrpp("NMG");
    std::string nmg_opts;
    fastf_t l_max_time = 30;
    if (mo) {
	nmg_opts = mo->method_optstr(mstrpp, s->dbip);
	l_max_time = (fastf_t)mo->max_time[mstrpp];
    }

    const char *tess_cmd[MAXPATHLEN] = {NULL};
    tess_cmd[0] = tess_exec;
    tess_cmd[1] = "facetize_process";
    tess_cmd[2] = "-O";
    tess_cmd[3] = bu_vls_cstr(s->wfile);
    tess_cmd[4] = "--methods";
    tess_cmd[5] = "NMG";
    tess_cmd[6] = "--method-opts";

    struct bu_vls mopts_vls = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&mopts_vls, "%s", nmg_opts.c_str());
    tess_cmd[7] = bu_vls_cstr(&mopts_vls);
    tess_cmd[8] = "--cache-dir";
    tess_cmd[9] = lcache;
    int cmd_fixed_cnt = 10;

    /* Names travel over stdin, so only the local pointer array bounds a batch. */
    int fail_cnt = 0;
    size_t vi = 0;
    while (vi < plan->variant_names.size()) {
	std::vector<const char *> batch_names;
	while (vi < plan->variant_names.size() &&
	       cmd_fixed_cnt + (int)batch_names.size() < MAXPATHLEN) {
	    batch_names.push_back(plan->variant_names[vi].c_str());
	    vi++;
	}

	if (batch_names.empty())
	    break;

	for (size_t i = 0; i < batch_names.size(); i++)
	    tess_cmd[cmd_fixed_cnt + i] = batch_names[i];
	int total_cnt = cmd_fixed_cnt + (int)batch_names.size();

	std::vector<std::string> batch_failures;
	int ret = tess_run(s, tess_cmd, total_cnt, l_max_time,
		(int)batch_names.size(), &batch_failures,
		TESS_WORK_PERTURBATION_VARIANT, false);
	if (ret != BRLCAD_OK) {
	    facetize_log(s, 0,
			"FACETIZE: variant tessellation failed for %d object(s)\n",
			(int)batch_failures.size());
	    fail_cnt += (int)batch_failures.size();
	}

	/* Clear per-batch name slots */
	for (size_t i = 0; i < batch_names.size(); i++)
	    tess_cmd[cmd_fixed_cnt + i] = NULL;
    }

    bu_vls_free(&mopts_vls);
    plan->n_variant_tess_failures = fail_cnt;
    return (fail_cnt == 0) ? BRLCAD_OK : BRLCAD_ERROR;
}

static int
tess_run_inputs(struct _ged_facetize_state *s,
	std::vector<struct directory *> &bad_dps,
	const std::vector<struct directory *> &inputs, const char **orig_cmd,
	int cmd_cnt, fastf_t max_time, enum tess_work_type work_type)
{
    bad_dps.clear();
    if (inputs.empty())
	return 0;

    const char *tess_cmd[MAXPATHLEN] = {NULL};
    for (int i = 0; i < cmd_cnt; i++)
	tess_cmd[i] = orig_cmd[i];
    for (size_t i = 0; i < inputs.size(); i++)
	tess_cmd[cmd_cnt+i] = inputs[i]->d_namep;

    std::vector<std::string> failed_names;
    (void)tess_run(s, tess_cmd, cmd_cnt+inputs.size(), max_time,
	    (int)inputs.size(), &failed_names, work_type, false);
    std::set<std::string> failed_set(failed_names.begin(),
	    failed_names.end());
    for (size_t i = 0; i < inputs.size(); i++) {
	if (failed_set.find(inputs[i]->d_namep) != failed_set.end())
	    bad_dps.push_back(inputs[i]);
    }
    return (int)bad_dps.size();
}


class DpCompare
{
    public:
	bool operator()(struct directory *dp1, struct directory *dp2) {
	    // C++ priority queues return the largest element, but
	    // we want to start with the smaller elements - so we
	    // invert the large/small reporting
	    return (dp1->d_len > dp2->d_len);
	}
};

static void
mark_failed_tessellations(struct _ged_facetize_state *s, const std::vector<std::string> &failed_dps)
{
    if (!s || failed_dps.empty())
	return;

    struct db_i *cdbip = db_open(bu_vls_cstr(s->wfile), DB_OPEN_READWRITE);
    if (cdbip) {
	db_dirbuild(cdbip);
	db_update_nref(cdbip);
	for (size_t i = 0; i < failed_dps.size(); i++) {
	    struct directory *dp = db_lookup(cdbip, failed_dps[i].c_str(), LOOKUP_QUIET);
	    if (!dp)
		continue;
	    struct bu_attribute_value_set avs = BU_AVS_INIT_ZERO;
	    db5_get_attributes(cdbip, &avs, dp);
	    (void)bu_avs_add(&avs, FACETIZE_METHOD_ATTR, "FAIL");
	    (void)db5_update_attributes(dp, &avs, cdbip);
	    bu_avs_free(&avs);
	}
	db_close(cdbip);
    }

    if (s->tolerate_failures) {
	for (size_t i = 0; i < failed_dps.size(); i++)
	    facetize_tolerated_failure(s, "primitive tessellation failed for '%s'; leaf will be omitted from boolean evaluation", failed_dps[i].c_str());
    }
}

int
_ged_facetize_leaves_tri(struct _ged_facetize_state *s, struct db_i *dbip, struct bu_ptbl *leaf_dps)
{
    // Sort dp objects by d_len using a priority queue
    std::priority_queue<struct directory *, std::vector<struct directory *>, DpCompare> pq;
    std::priority_queue<struct directory *, std::vector<struct directory *>, DpCompare> q_pbot;
    for (size_t i = 0; i < BU_PTBL_LEN(leaf_dps); i++) {
	struct directory *ldp = (struct directory *)BU_PTBL_GET(leaf_dps, i);

	// If this isn't a proper BRL-CAD object, tessellation is a no-op
	if (ldp->d_major_type != DB5_MAJORTYPE_BRLCAD)
	    continue;

	// Plate mode bots only have a realistic chance of being handled by
	// the plate to vol conversion method, but they can be quite slow
	// and will run into max-time limitations if they are large.  Separate
	// the large ones out - we will treat their handling like a fallback method and
	// be more tolerant of time
	if (ldp->d_minor_type == ID_BOT) {
	    struct rt_db_internal intern;
	    RT_DB_INTERNAL_INIT(&intern);
	    if (rt_db_get_internal(&intern, ldp, dbip, NULL) < 0) {
		pq.push(ldp);
		continue;
	    }
	    struct rt_bot_internal *bot = (struct rt_bot_internal *)(intern.idb_ptr);
	    int propVal = (int)rt_bot_propget(bot, "type");
	    bool is_plate = (propVal == RT_BOT_PLATE ||
		    propVal == RT_BOT_PLATE_NOCOS);
	    rt_db_free_internal(&intern);
	    // Plate mode BoTs need an explicit volume representation
	    if (is_plate) {
		q_pbot.push(ldp);
		continue;
	    }
	}

	// Standard case
	pq.push(ldp);
    }

    if (pq.empty() && q_pbot.empty()) {
	bu_log("Note: no viable objects for tessellation found.\n");
	return BRLCAD_OK;
    }

    // Build up the path to the ged_exec executable
    char tess_exec[MAXPATHLEN];
    bu_dir(tess_exec, MAXPATHLEN, BU_DIR_BIN, "ged_exec", BU_DIR_EXT, NULL);

    // Set up a priority order of methods to try when processing primitives.
    std::vector<std::string> avail_methods = tess_avail_methods();
    if (avail_methods.size() == 0) {
	bu_log("No methods for tessellation found.\n");
	bu_dirclear(s->wdir);
	return BRLCAD_ERROR;
    }

    method_options_t *mo = (method_options_t*)s->method_opts;
    std::queue<std::string> method_flags;
    std::queue<std::string> method_flags_bak;
    for (size_t i = 0; i < mo->methods.size(); i++) {
	std::string cmethod = mo->methods[i];
	if (std::find(avail_methods.begin(), avail_methods.end(), cmethod) != avail_methods.end()) {
	    method_flags.push(cmethod);
	} else {
	    bu_log("Warning: user requested %s tessellation method not found.\n", cmethod.c_str());
	}
    }

    if (mo->methods.size() && !method_flags.size()) {
	bu_log("Error: all user requested tessellation methods unsupported.\n");
	bu_dirclear(s->wdir);
	return BRLCAD_ERROR;
    }

    if (!method_flags.size() && avail_methods.size()) {
	for (size_t i = 0; i < avail_methods.size(); i++) {
	    method_flags.push(avail_methods[i]);
	}
    }

    method_flags_bak = method_flags;

    // We want the subprocess to be using the same cache directory
    // as the parent
    char lcache[MAXPATHLEN] = {0};
    bu_dir(lcache, MAXPATHLEN, BU_DIR_CACHE, NULL);


    // Call ged_exec to produce evaluated solids.
    // First step is to build up the command to run
    std::vector<std::string> failed_dps;
    std::string mstrpp;
    int l_max_time;
    struct bu_vls method_str = BU_VLS_INIT_ZERO;
    struct bu_vls method_opts_str = BU_VLS_INIT_ZERO;
    const char *tess_cmd[MAXPATHLEN] = {NULL};
    int method_ind = 5;
    int method_opt_ind = 7;
    tess_cmd[ 0] = tess_exec;
    tess_cmd[ 1] = "facetize_process";
    tess_cmd[ 2] = "-O";
    tess_cmd[ 3] = bu_vls_cstr(s->wfile);
    tess_cmd[ 4] = "--methods";
    tess_cmd[ 5] = NULL;
    tess_cmd[ 6] = "--method-opts";
    tess_cmd[ 7] = NULL;
    tess_cmd[ 8] = "--cache-dir";
    tess_cmd[ 9] = lcache;
    int cmd_fixed_cnt = 10;
    while (!pq.empty()) {
	// Starting a new round of object processing - reset method flags
	method_flags = method_flags_bak;

	// There are a number of methods that can be tried.  We try them in priority
	// order, timing out if one of them goes too long.
	mstrpp = method_flags.front();
	method_flags.pop();
	bu_vls_sprintf(&method_str, "%s", mstrpp.c_str());
	tess_cmd[method_ind] = bu_vls_cstr(&method_str);
	// Each method has its own default (or possibly user set) time limit
	l_max_time = mo->max_time[mstrpp];
	// Get defined options for this particular method
	bu_vls_sprintf(&method_opts_str, "%s", mo->method_optstr(mstrpp, dbip).c_str());
	tess_cmd[method_opt_ind] = bu_vls_cstr(&method_opts_str);

	std::vector<struct directory *> dps;
	std::vector<struct directory *> bad_dps;
	while (!pq.empty() && cmd_fixed_cnt + dps.size() < MAXPATHLEN) {
	    struct directory *ldp = pq.top();
	    pq.pop();
	    dps.push_back(ldp);
	}

	// We have the list of objects to feed the process - now, trigger
	// the runs with as many methods as it takes to facetize all the
	// primitives
	int err_cnt = 0;
	while (bu_vls_strlen(&method_str)) {
	    err_cnt = tess_run_inputs(s, bad_dps, dps, tess_cmd,
		    cmd_fixed_cnt, l_max_time, TESS_WORK_ORIGINAL_PRIMITIVE);

	    // If we dealt successfully with everything, we're done
	    if (!err_cnt)
		break;

	    if (method_flags.size()) {
		// If we still have available methods to try, go another round
		err_cnt = 0;
		mstrpp = method_flags.front();
		method_flags.pop();
		bu_vls_sprintf(&method_str, "%s", mstrpp.c_str());
		tess_cmd[method_ind] = bu_vls_cstr(&method_str);
		// Each method has its own default (or possibly user set) time limit
		l_max_time = mo->max_time[mstrpp];
		// Get defined options for this particular method
		bu_vls_sprintf(&method_opts_str, "%s", mo->method_optstr(mstrpp, dbip).c_str());
		tess_cmd[method_opt_ind] = bu_vls_cstr(&method_opts_str);
		dps = bad_dps;
		bad_dps.clear();
	    } else {
		// All done - nothing left to try
		bu_vls_trunc(&method_str, 0);
		tess_cmd[method_ind] = NULL;
	    }
	}

	if (err_cnt || bad_dps.size() > 0) {
	    // If we tried all the active methods and still had failures, we have an
	    // error.  We'll keep trying to process all the leaves, since we want to
	    // get a full picture of what the issues with the conversion are, but
	    // we need to record these as a full-on failure.
	    for (size_t i = 0; i < bad_dps.size(); i++)
		failed_dps.push_back(std::string(bad_dps[i]->d_namep));
	}
    }

    while (!q_pbot.empty()) {
	bu_vls_sprintf(&method_str, "NMG");
	tess_cmd[method_ind] = bu_vls_cstr(&method_str);
	mstrpp = std::string("NMG");
	l_max_time = mo->plate_max_time;
	bu_vls_sprintf(&method_opts_str, "%s", mo->method_optstr(mstrpp, dbip).c_str());
	tess_cmd[method_opt_ind] = bu_vls_cstr(&method_opts_str);


	std::vector<struct directory *> dps;
	std::vector<struct directory *> bad_dps;
	while (!q_pbot.empty() && cmd_fixed_cnt + dps.size() < MAXPATHLEN) {
	    struct directory *ldp = q_pbot.top();
	    q_pbot.pop();
	    dps.push_back(ldp);
	}


	int err_cnt = tess_run_inputs(s, bad_dps, dps, tess_cmd,
		cmd_fixed_cnt, l_max_time, TESS_WORK_PLATE_MODE_PRIMITIVE);
	if (err_cnt) {
	    for (size_t i = 0; i < bad_dps.size(); i++)
		failed_dps.push_back(std::string(bad_dps[i]->d_namep));
	    // If we couldn't handle the plate mode conversion, we can't do the
	    // boolean evaluation unless partial output was explicitly requested.
	    if (!s->tolerate_failures) {
		mark_failed_tessellations(s, failed_dps);
		facetize_log(s, 0, "Plate mode conversion wasn't able to complete\n");
		return BRLCAD_ERROR;
	    }
	}
    }

    if (failed_dps.size()) {
	// As the parent process, we can know when we've run out of options
       // to try.  If we get there, flag the solid in the working copy so
       // the summary knows to report it.
	mark_failed_tessellations(s, failed_dps);
	if (s->tolerate_failures)
	    return BRLCAD_OK;
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

int
_ged_facetize_booleval_tri_to_db(struct _ged_facetize_state *s, struct db_i *dbip, struct rt_wdb *wdbp, int argc, const char **argv, const char *oname, struct bu_list *vlfree, struct db_i *odbip, int curr_cnt, int total_cnt)
{
    union tree *ftree;
    if (!dbip || !wdbp || !argv || !oname || !odbip)
	return BRLCAD_ERROR;

    if (total_cnt < 0) {
	facetize_log(s, 0, "Processing %s [%d perturb]...", oname, curr_cnt);
    } else if (total_cnt == 0) {
	facetize_log(s, 0, "Processing %s...", oname);
    } else {
	facetize_log(s, 0, "Processing %s [%d of %d]...", oname, curr_cnt, total_cnt);
    }
    facetize_failure_clear(s);

    /* Per-object booleval status is shown only in verbose mode. */
    if (s->verbosity >= 1) {
	if (argc == 1) {
	    bu_log("%s: evaluating booleans...\n", argv[0]);
	} else {
	    bu_log("Evaluating booleans for the trees of %d input objects...\n", argc);
	}
    }

    // Unlike the -r flag processing regions, where each individual region
    // processed is semantically a single solid , there is no guarantee in
    // general that the output is representing a single, well behaved solid.
    // Consequently, thin volumes and close faces may be expected features and
    // it's more problematic to do the fixup check.  However, if we were given
    // a single primitive or region, those outputs should satisfy the fixup
    // criteria.
    bool do_fixup = false;
    if (argc == 1 && !s->no_fixup) {
	struct directory *dp = db_lookup(dbip, argv[0], LOOKUP_QUIET);
	if ((dp->d_flags & RT_DIR_REGION) || (dp->d_flags & RT_DIR_SOLID))
	    do_fixup = true;
    }

    // If we don't have inputs that can be fed to db_walk_tree it will produce
    // an error, which we don't want.  What we do want in such a case - where
    // there are NO valid walking candidates - is to indicate that there wasn't
    // a logic failure.  That means we need an empty bot to be generated - i.e.
    // we don't want to trigger the db_walk_tree error path.
    int ac = 0;
    const char **av = (const char **)bu_calloc(argc, sizeof(const char *), "av");
    for (int i = 0; i < argc; i++) {
	struct directory *dp = db_lookup(dbip, argv[i], LOOKUP_QUIET);
	if (dp->d_flags & RT_DIR_COMB || dp->d_flags & RT_DIR_SOLID) {
	    av[ac] = argv[i];
	    ac++;
	}
    }

    if (ac) {
	s->error_flag = 0;
	struct db_tree_state init_state;
	db_init_db_tree_state(&init_state, dbip);
	/* Establish tolerances */
	init_state.ts_ttol = &wdbp->wdb_ttol;
	init_state.ts_tol = &wdbp->wdb_tol;
	init_state.ts_m = NULL;
	s->facetize_tree = (union tree *)0;
	int i = 0;
	if (!BU_SETJUMP) {
	    /* try */
	    i = db_walk_tree(dbip, argc, argv,
		    1,
		    &init_state,
		    0,			/* take all regions */
		    facetize_region_end,
		    _booltree_leaf_tess,
		    (void *)s
		    );
	} else {
	    /* catch */
	    BU_UNSETJUMP;
	    i = -1;
	} BU_UNSETJUMP;

	// Something went wrong - not just empty geometry, but an actual error.
	// Do not generate a BoT, empty or otherwise.
	if (i < 0 || s->error_flag) {
	    bu_free(av, "av");
	    if (!s->failure_msg || !bu_vls_strlen(s->failure_msg))
		facetize_failure(s, "database tree walk failed while preparing BoT leaves for Manifold boolean evaluation");
	    facetize_log_current_failure(s, "database tree walk failed while preparing BoT leaves for Manifold boolean evaluation");
	    return BRLCAD_ERROR;
	}
    }
    bu_free(av, "av");

    // We don't have a tree - unless we've been told not to, prepare an empty BoT
    if (!s->facetize_tree && !s->no_empty) {
	struct rt_bot_internal *bot;
	BU_GET(bot, struct rt_bot_internal);
	bot->magic = RT_BOT_INTERNAL_MAGIC;
	bot->mode = RT_BOT_SOLID;
	bot->orientation = RT_BOT_CCW;
	bot->thickness = NULL;
	bot->face_mode = (struct bu_bitv *)NULL;
	bot->bot_flags = 0;
	bot->num_vertices = 0;
	bot->num_faces = 0;
	bot->vertices = NULL;
	bot->faces = NULL;
	if (_ged_facetize_write_bot(odbip, bot, oname, s->verbosity) != BRLCAD_OK) {
	    facetize_failure(s, "unable to write empty BoT '%s' to the database", oname);
	    facetize_log_current_failure(s, "unable to write empty BoT to the database");
	    return BRLCAD_ERROR;
	}
	facetize_log(s, 0, " Success.\n");
	return BRLCAD_OK;
    }

    // Third stage is to execute the boolean operations
    ftree = rt_booltree_eval(s->facetize_tree, vlfree, &wdbp->wdb_tol, &manifold_do_bool, 0, (void *)s);
    if (s->error_flag && !s->tolerate_failures) {
	facetize_log_current_failure(s, "Boolean tree evaluation failed");
	return BRLCAD_ERROR;
    }
    if (!ftree) {
	if (s->tolerate_failures && s->tolerated_failures > 0)
	    facetize_failure(s, "all evaluated components were omitted after tolerated failures; no partial result could be generated");
	facetize_log_current_failure(s, "Boolean tree evaluation did not produce a result");
	return BRLCAD_ERROR;
    }

    if (ftree->tr_d.td_d) {
	manifold::Manifold *om = (manifold::Manifold *)ftree->tr_d.td_d;
	if (om->Status() != manifold::Manifold::Error::NoError) {
	    // Urk - boolean failure of some sort!
	    facetize_failure(s, "final Manifold result for '%s' is invalid: %s", oname, manifold::ToString(om->Status()).c_str());
	    facetize_log_current_failure(s, "final Manifold result is invalid");
	    return BRLCAD_ERROR;
	}

	if (s->verbosity > 1) {
	    bu_log("[FINAL_BOOL] obj=%s  final_mesh_SA=%.6f mm^2  num_verts=%zu  num_faces=%zu\n",
		   (argc > 0 && argv && argv[0]) ? argv[0] : "?",
		   om->SurfaceArea(),
		   (size_t)om->GetMeshGL64().vertProperties.size() / 3,
		   (size_t)om->GetMeshGL64().triVerts.size() / 3);
	}

	manifold::MeshGL64 rmesh = om->GetMeshGL64();
	struct rt_bot_internal *bot;
	BU_GET(bot, struct rt_bot_internal);
	bot->magic = RT_BOT_INTERNAL_MAGIC;
	bot->mode = RT_BOT_SOLID;
	bot->orientation = RT_BOT_CCW;
	bot->thickness = NULL;
	bot->face_mode = (struct bu_bitv *)NULL;
	bot->bot_flags = 0;
	bot->num_vertices = (int)rmesh.vertProperties.size()/3;
	bot->num_faces = (int)rmesh.triVerts.size()/3;
	bot->vertices = (double *)calloc(rmesh.vertProperties.size(), sizeof(double));
	bot->faces = (int *)calloc(rmesh.triVerts.size(), sizeof(int));
	for (size_t j = 0; j < rmesh.vertProperties.size(); j++)
	    bot->vertices[j] = rmesh.vertProperties[j];
	for (size_t j = 0; j < rmesh.triVerts.size(); j++)
	    bot->faces[j] = rmesh.triVerts[j];

	/* Guard against near-zero perturb slivers: if the booleval mesh is tiny,
	 * quickly Crofton-check the original CSG.  If CSG is effectively empty,
	 * emit an empty BoT to match raytrace behavior. */
	double bot_vol = 0.0;
	if (bot->num_faces > 0 && bot->num_vertices > 0) {
	    bot_vol = std::fabs(bg_trimesh_volume(bot->faces, bot->num_faces,
						  (const point_t *)bot->vertices,
						  bot->num_vertices));
	}
	double bbox_vol = bot_bbox_volume(bot);
	bool tiny_bot = (bbox_vol > 0.0) ?
	    (bot_vol <= bbox_vol * FACETIZE_EMPTY_CHECK_REL_VOL_TOL) :
	    (bot_vol <= FACETIZE_EMPTY_CHECK_ABS_VOL_TOL);
	bool is_single_input = (argc == 1 && argv && argv[0]);
	bool has_csg_context = (s && s->dbip);
	if (tiny_bot && is_single_input && has_csg_context) {
	    double csg_vol = -1.0;
	    if (csg_crofton_volume(s->dbip, argv[0], &csg_vol) == BRLCAD_OK) {
		double csg_abs = std::fabs(csg_vol);
		double csg_vtol = (bbox_vol > 0.0) ?
		    (bbox_vol * FACETIZE_EMPTY_CHECK_REL_VOL_TOL) :
		    FACETIZE_EMPTY_CHECK_ABS_VOL_TOL;
		if (csg_abs <= csg_vtol) {
		    rt_bot_internal_free(bot);
		    bot->magic = RT_BOT_INTERNAL_MAGIC;
		    bot->mode = RT_BOT_SOLID;
		    bot->orientation = RT_BOT_CCW;
		    bot->thickness = NULL;
		    bot->face_mode = (struct bu_bitv *)NULL;
		    bot->bot_flags = 0;
		}
	    }
	}
	delete om;
	ftree->tr_d.td_d = NULL;

	// If we have a manifold_mesh, write it out as a bot
	if (_ged_facetize_write_bot(odbip, bot, oname, s->verbosity) != BRLCAD_OK) {
	    facetize_failure(s, "unable to write evaluated BoT '%s' to the database", oname);
	    facetize_log_current_failure(s, "unable to write evaluated BoT to the database");
	    return BRLCAD_ERROR;
	}
    } else {
	// Evaluation didn't produce a tree - unless we've been told not to,
	// prepare an empty BoT
	if (!s->no_empty) {
	    struct rt_bot_internal *bot;
	    BU_GET(bot, struct rt_bot_internal);
	    bot->magic = RT_BOT_INTERNAL_MAGIC;
	    bot->mode = RT_BOT_SOLID;
	    bot->orientation = RT_BOT_CCW;
	    bot->thickness = NULL;
	    bot->face_mode = (struct bu_bitv *)NULL;
	    bot->bot_flags = 0;
	    bot->num_vertices = 0;
	    bot->num_faces = 0;
	    bot->vertices = NULL;
	    bot->faces = NULL;
	    if (_ged_facetize_write_bot(odbip, bot, oname, s->verbosity) != BRLCAD_OK) {
		facetize_failure(s, "unable to write empty BoT '%s' to the database", oname);
		facetize_log_current_failure(s, "unable to write empty BoT to the database");
		return BRLCAD_ERROR;
	    }
	    facetize_log(s, 0, "Success.\n");
	    return BRLCAD_OK;
	}
    }

    // If we meet the conditions, apply the fixup logic
    if (do_fixup) {
	struct directory *dp = db_lookup(dbip, argv[0], LOOKUP_QUIET);
	if ((dp->d_flags & RT_DIR_REGION) || (!(dp->d_flags & RT_DIR_COMB))) {
	    struct directory *bot_dp = db_lookup(odbip, oname, LOOKUP_QUIET);
	    struct rt_bot_internal *nbot = bot_fixup(s, odbip, bot_dp, oname);
	    if (nbot) {
		// Write out new version of BoT
		db_delete(odbip, bot_dp);
		db_dirdelete(odbip, bot_dp);
		if (_ged_facetize_write_bot(odbip, nbot, oname, s->verbosity) != BRLCAD_OK) {
		    facetize_failure(s, "BoT fixup succeeded for '%s' but writing the repaired BoT failed", oname);
		    facetize_log_current_failure(s, "BoT fixup succeeded but writing the repaired BoT failed");
		    return BRLCAD_ERROR;
		}
	    }
	}
    }

    facetize_log(s, 0, " Success.\n");
    return BRLCAD_OK;
}

int
_ged_facetize_booleval_tri(struct _ged_facetize_state *s, struct db_i *dbip, struct rt_wdb *wdbp, int argc, const char **argv, const char *oname, struct bu_list *vlfree, bool output_to_working, int curr_cnt, int total_cnt)
{
    struct db_i *output_dbip = output_to_working ? dbip : (s ? s->dbip : NULL);
    return _ged_facetize_booleval_tri_to_db(s, dbip, wdbp, argc, argv,
	    oname, vlfree, output_dbip, curr_cnt, total_cnt);
}

int
_ged_facetize_booleval(struct _ged_facetize_state *s, int argc, struct directory **dpa, const char *oname, bool output_to_working, bool cleanup)
{
    int ret = BRLCAD_OK;
    struct bu_list *vlfree = &rt_vlfree;

    if (!s)
	return BRLCAD_ERROR;

    if (!argc || !dpa)
	return BRLCAD_ERROR;

    struct db_i *dbip = s->dbip;
    struct rt_wdb *wwdbp;

    /* First stage is to process the primitive instances.  We include points in
     * this even though they do not define a volume in order to allow for the
     * possibility of applying the alternative pnt based reconstruction methods
     * to their data. */
    const char *sfilter = "-type shape -or -type pnts";
    struct bu_ptbl leaf_dps = BU_PTBL_INIT_ZERO;
    if (db_search(&leaf_dps, DB_SEARCH_RETURN_UNIQ_DP, sfilter, argc, dpa, dbip, NULL, NULL, NULL) < 0) {
	// Empty input - nothing to facetize.
	return BRLCAD_OK;
    }

    /* OK, we have work to do. Set up a working copy of the .g file. */
    if (_ged_facetize_working_file_setup(s, &leaf_dps) != BRLCAD_OK) {
	facetize_log(s, 0, "FACETIZE: failed to set up working database copy %s\n", bu_vls_cstr(s->wfile));
	bu_ptbl_free(&leaf_dps);
	return BRLCAD_ERROR;
    }

    /* Direct Manifold booleval keeps the eager perturb path: when enabled,
     * build and tessellate coplanarity-avoidance variants up front.
     * Region mode overrides this by validating first and only retrying with
     * variants on demand. */
    if (s->variant_plan) {
	delete (FacetizeVariantPlan *)s->variant_plan;
	s->variant_plan = NULL;
    }
    if (!s->make_nmg && !s->nmg_booleval && !s->no_perturb) {
	FacetizeVariantPlan *vplan = _ged_facetize_build_variant_plan(s, argc, dpa, NULL);
	s->variant_plan = (void *)vplan;
    }

    if (_ged_facetize_leaves_tri(s, dbip, &leaf_dps)) {
	facetize_log(s, 0, "FACETIZE: primitive tessellation failed; BoT boolean evaluation cannot proceed. Check the Primitive tessellation section in the final FACETIZE summary.\n");
	facetize_collect_primitive_summary(s);
	bu_ptbl_free(&leaf_dps);
	return BRLCAD_ERROR;
    }

    if (s->variant_plan) {
	FacetizeVariantPlan *vplan = (FacetizeVariantPlan *)s->variant_plan;
	if (!vplan->variant_names.empty())
	    _ged_facetize_tessellate_variant_names(s, vplan);
    }

    // Re-open working .g copy after BoTs have replaced CSG solids and perform
    // the tree walk to set up Manifold data.
    struct db_i *wdbip = db_open(bu_vls_cstr(s->wfile), (output_to_working) ? DB_OPEN_READWRITE :  DB_OPEN_READONLY);
    if (!wdbip) {
	facetize_log(s, 0, "FACETIZE: unable to open working database %s for boolean evaluation\n", bu_vls_cstr(s->wfile));
	bu_dirclear(s->wdir);
	bu_ptbl_free(&leaf_dps);
	return BRLCAD_ERROR;
    }
    if (db_dirbuild(wdbip) < 0) {
	facetize_log(s, 0, "FACETIZE: unable to build directory for working database %s\n", bu_vls_cstr(s->wfile));
	db_close(wdbip);
	bu_ptbl_free(&leaf_dps);
	return BRLCAD_ERROR;
    }

    db_update_nref(wdbip);

    // Need wdbp in the next two stages for tolerances
    wwdbp = wdb_dbopen(wdbip, RT_WDB_TYPE_DB_DEFAULT);
    if (!wwdbp) {
	facetize_log(s, 0, "FACETIZE: unable to create writable database handle for %s\n", bu_vls_cstr(s->wfile));
	db_close(wdbip);
	bu_ptbl_free(&leaf_dps);
	return BRLCAD_ERROR;
    }

    /* Second stage is to prepare Manifold versions of the instances of the BoT
     * obj conversions generated by stage 1.  This is where matrix placement
     * is handled. */
    // Prepare argc/argv array for db_walk_tree
    const char **av = (const char **)bu_calloc(argc+1, sizeof(char *), "av");
    for (int i = 0; i < argc; i++) {
	av[i] = dpa[i]->d_namep;
    }

    if (_ged_facetize_booleval_tri(s, wdbip, wwdbp, argc, av, oname, vlfree, output_to_working, 1, 1) != BRLCAD_OK) {
	ret = BRLCAD_ERROR;
	if (s->verbosity >= 0) {
	    bu_log("FACETIZE: failed to generate %s; see %s for the full facetize log\n", oname, bu_vls_cstr(s->log_file));
	}
    }

    bu_free(av, "av");
    db_close(wdbip);

    if (cleanup)
	bu_dirclear(s->wdir);

    bu_ptbl_free(&leaf_dps);

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
