/*                         D R A W . C P P
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
/** @file draw.cpp
 *
 * Drawing routines that generate scene objects.
 *
 */

#include "common.h"

#include <set>
#include <unordered_map>

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include "bsocket.h"

#include "bu/cmd.h"
#include "bu/hash.h"
#include "bu/opt.h"
#include "bu/sort.h"
#include "bu/str.h"
#include "bsg/geometry.h"
#include "bsg/appearance.h"
#include "bsg/draw_intent.h"
#include "bsg/lod.h"
#include "bsg/view_state.h"
#include "bg/sat.h"
#include "nmg.h"
#include "rt/view.h"

#include "ged/view.h"
#include "../librt/librt_private.h"
#include "./ged_private.h"

static int
prim_tess(bsg_scene_ref ref, struct rt_db_internal *ip)
{
    struct ged_draw_source_state *d = ged_draw_scene_ref_source_data(ref);
    struct directory *dp = ged_draw_scene_ref_leaf_dp(ref);
    const struct bn_tol *tol = d->tol;
    const struct bg_tess_tol *ttol = d->ttol;
    RT_CK_DB_INTERNAL(ip);
    RT_CK_DIR(dp);
    BN_CK_TOL(tol);
    BG_CK_TESS_TOL(ttol);
    if (!ip->idb_meth || !ip->idb_meth->ft_tessellate) {
	bu_log("ERROR(%s): tessellation support not available\n", dp->d_namep);
	return -1;
    }

    struct model *m = nmg_mm();
    struct nmgregion *r = (struct nmgregion *)NULL;
    if (ip->idb_meth->ft_tessellate(&r, m, ip, ttol, tol) < 0) {
	bu_log("ERROR(%s): tessellation failure\n", dp->d_namep);
	return -1;
    }

    NMG_CK_REGION(r);
    if (!ged_draw_scene_ref_geometry_publish_nmg_region(ref, r, GED_DRAW_NMG_STYLE_POLYGON)) {
	nmg_km(m);
	return -1;
    }
    nmg_km(m);

    ged_draw_scene_ref_realization_set_current(ref, 1);

    return 0;
}

static struct bsg_mesh_lod *
draw_mesh_lod(bsg_scene_ref ref)
{
    struct ged_draw_source_state *d = ged_draw_scene_ref_source_data(ref);
    return d ? d->mesh_lod : NULL;
}

static void
draw_set_transformed_bounds(bsg_scene_ref ref, const point_t src_bmin, const point_t src_bmax)
{
    if (bsg_scene_ref_is_null(ref))
	return;

    mat_t sm;
    if (!ged_draw_scene_ref_draw_mat(ref, sm))
	return;

    point_t corners[8] = {
	{src_bmin[X], src_bmin[Y], src_bmin[Z]},
	{src_bmin[X], src_bmin[Y], src_bmax[Z]},
	{src_bmin[X], src_bmax[Y], src_bmin[Z]},
	{src_bmin[X], src_bmax[Y], src_bmax[Z]},
	{src_bmax[X], src_bmin[Y], src_bmin[Z]},
	{src_bmax[X], src_bmin[Y], src_bmax[Z]},
	{src_bmax[X], src_bmax[Y], src_bmin[Z]},
	{src_bmax[X], src_bmax[Y], src_bmax[Z]}
    };

    point_t bmin, bmax, tp;
    MAT4X3PNT(tp, sm, corners[0]);
    VMOVE(bmin, tp);
    VMOVE(bmax, tp);
    for (int i = 1; i < 8; i++) {
	MAT4X3PNT(tp, sm, corners[i]);
	VMIN(bmin, tp);
	VMAX(bmax, tp);
    }

    bsg_scene_set_bounds(ref, bmin, bmax, 1);
}

static int
draw_mesh_lod_bounds_from_points(point_t bmin, point_t bmax, const point_t *points, int point_count)
{
    if (!points || point_count <= 0)
	return 0;

    VMOVE(bmin, points[0]);
    VMOVE(bmax, points[0]);
    for (int i = 1; i < point_count; i++) {
	VMIN(bmin, points[i]);
	VMAX(bmax, points[i]);
    }

    return 1;
}

static void
draw_mesh_lod_bounds_restore(bsg_scene_ref ref, struct ged_draw_source_state *d)
{
    if (bsg_scene_ref_is_null(ref) || !d || !d->mesh_lod_bounds_valid)
	return;

    draw_set_transformed_bounds(ref, d->mesh_lod_bmin, d->mesh_lod_bmax);
}

static int
draw_brep_lod_bounds_prepare(struct ged_draw_source_state *d, struct directory *dp)
{
    if (!d || !dp || d->mesh_lod_bounds_valid)
	return d && d->mesh_lod_bounds_valid;

    struct rt_db_internal dbintern;
    RT_DB_INTERNAL_INIT(&dbintern);
    if (rt_db_get_internal(&dbintern, dp, d->dbip, NULL) < 0)
	return 0;
    if (dbintern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	rt_db_free_internal(&dbintern);
	return 0;
    }

    struct rt_brep_internal *bi = (struct rt_brep_internal *)dbintern.idb_ptr;
    RT_BREP_CK_MAGIC(bi);

    int *faces = NULL;
    int face_cnt = 0;
    vect_t *normals = NULL;
    point_t *points = NULL;
    int point_count = 0;
    int ret = brep_cdt_fast(&faces, &face_cnt, &normals, &points, &point_count,
	    bi->brep, -1, d->ttol, d->tol);
    if (ret == BRLCAD_OK &&
	    draw_mesh_lod_bounds_from_points(d->mesh_lod_bmin, d->mesh_lod_bmax,
		(const point_t *)points, point_count))
	d->mesh_lod_bounds_valid = 1;

    bu_free(faces, "faces");
    bu_free(normals, "normals");
    bu_free(points, "pnts");
    rt_db_free_internal(&dbintern);

    return d->mesh_lod_bounds_valid;
}

static int
draw_mesh_lod_publish(bsg_scene_ref ref, struct bsg_mesh_lod *lod)
{
    if (bsg_scene_ref_is_null(ref) || !lod || lod->fcnt <= 0 || lod->pcnt <= 0 || !lod->faces || !lod->points)
	return 0;

    size_t face_count = (size_t)lod->fcnt;
    size_t point_count = (size_t)lod->pcnt;
    size_t valid_faces = 0;
    for (size_t i = 0; i < face_count; i++) {
	int valid = 1;
	for (int j = 0; j < 3; j++) {
	    int idx = lod->faces[3*i + j];
	    if (idx < 0 || (size_t)idx >= point_count) {
		valid = 0;
		break;
	    }
	}
	if (valid)
	    valid_faces++;
    }
    if (!valid_faces)
	return 0;

    size_t index_count = valid_faces * 4;
    size_t normal_count = valid_faces * 3;
    int *indices = (int *)bu_calloc(index_count, sizeof(int), "mesh lod indexed-face indices");
    vect_t *normals = (vect_t *)bu_calloc(normal_count, sizeof(vect_t), "mesh lod indexed-face normals");
    const point_t *normal_points = lod->points_orig ? lod->points_orig : lod->points;
    size_t out = 0;
    size_t normal_out = 0;

    for (size_t i = 0; i < face_count; i++) {
	int bad_face = 0;
	for (int j = 0; j < 3; j++) {
	    int idx = lod->faces[3*i + j];
	    if (idx < 0 || (size_t)idx >= point_count) {
		bad_face = 1;
		break;
	    }
	}
	if (bad_face)
	    continue;

	vect_t face_normal, ab, ac;
	VSUB2(ab, normal_points[lod->faces[3*i + 0]], normal_points[lod->faces[3*i + 1]]);
	VSUB2(ac, normal_points[lod->faces[3*i + 0]], normal_points[lod->faces[3*i + 2]]);
	VCROSS(face_normal, ab, ac);
	VUNITIZE(face_normal);

	for (int j = 0; j < 3; j++) {
	    indices[out] = lod->faces[3*i + j];
	    if (lod->normals) {
		VMOVE(normals[normal_out], lod->normals[3*i + j]);
		if (ZERO(MAGNITUDE(normals[normal_out])))
		    VMOVE(normals[normal_out], face_normal);
	    } else {
		VMOVE(normals[normal_out], face_normal);
	    }
	    normal_out++;
	    out++;
	}
	indices[out++] = -1;
    }

    int ret = bsg_geometry_ref_update_indexed_face_set(ged_draw_scene_ref_geometry_ref(ref),
	    lod->points, point_count, normals, normal_count, indices, index_count);
    bu_free(normals, "mesh lod indexed-face normals");
    bu_free(indices, "mesh lod indexed-face indices");
    return ret;
}

static struct bsg_lod_source_policy_settings
draw_lod_policy(const struct bsg_view *v)
{
    struct bsg_lod_source_policy_settings policy = {};
    policy.policy = BSG_LOD_AUTO;
    policy.scale = 1.0;
    policy.curve_scale = 1.0;
    policy.point_scale = 1.0;
    if (v)
	(void)bsg_view_lod_source_policy_get(v, &policy);
    return policy;
}

static int
csg_wireframe_update(bsg_scene_ref ref, struct bsg_view *v, int flag)
{
    /* Validate */
    if (bsg_scene_ref_is_null(ref) || !v)
	return 0;

    struct bsg_lod_source_policy_settings policy = draw_lod_policy(v);
    if (!policy.csg_enabled)
	return 0;

    bsg_log(1, "csg_wireframe_update %s[%s]", ged_draw_scene_ref_name(ref), bu_vls_cstr(&v->gv_name));

    ged_draw_scene_ref_realization_set_roles(ref, 1, 0);

    // If the object is not visible in the scene, don't change the data.  This
    // check is useful in orthographic camera mode, where we zoom in on a
    // narrow subset of the model and far away objects are still rendered in
    // full detail.  If we have a perspective matrix active don't make this
    // check, since far away objects outside the view obb will be visible.
    //bu_log("min: %f %f %f max: %f %f %f\n", V3ARGS(vo->bmin), V3ARGS(vo->bmax));
    point_t bmin = VINIT_ZERO;
    point_t bmax = VINIT_ZERO;
    bsg_scene_bounds(ref, bmin, bmax);
    if (!(v->gv_perspective > SMALL_FASTF) && !bg_sat_aabb_obb(bmin, bmax, v->obb_center, v->obb_extent1, v->obb_extent2, v->obb_extent3))
	return 0;

    bool rework = (flag) ? true : false;

    // Check point scale
    if (!rework && !NEAR_EQUAL(ged_draw_scene_ref_realization_curve_scale(ref), policy.curve_scale, SMALL_FASTF))
	rework = true;
    // Check point scale
    if (!rework && !NEAR_EQUAL(ged_draw_scene_ref_realization_point_scale(ref), policy.point_scale, SMALL_FASTF))
	rework = true;
    if (!rework) {
	// Check view scale
	fastf_t view_scale = ged_draw_scene_ref_realization_view_scale(ref);
	fastf_t delta = view_scale * 0.1/view_scale;
	if (!NEAR_EQUAL(view_scale, v->gv_scale, delta))
	    rework = true;
    }
    if (!rework)
	return 0;

    // We're going to redraw - sync with view
    struct ged_draw_source_state *d = ged_draw_scene_ref_source_data(ref);
    ged_draw_scene_ref_mark_view_inputs_changed(ref);
    ged_draw_scene_ref_realization_set_view_policy(ref,
	    policy.csg_enabled,
	    v->gv_scale,
	    policy.bot_threshold,
	    policy.curve_scale,
	    policy.point_scale);

    struct directory *dp = ged_draw_scene_ref_leaf_dp(ref);
    if (!d)
	return 0;
    struct db_i *dbip = d->dbip;
    struct rt_db_internal dbintern;
    RT_DB_INTERNAL_INIT(&dbintern);
    struct rt_db_internal *ip = &dbintern;
    mat_t draw_mat;
    if (!ged_draw_scene_ref_draw_mat(ref, draw_mat))
	return 0;
    int ret = rt_db_get_internal(ip, dp, dbip, draw_mat);
    if (ret < 0)
	return 0;

    if (ged_draw_scene_ref_publish_primitive_wireframe(ref, ip, NULL, d->tol,
	    v, 1) >= 0) {
	bsg_scene_update_bounds(ref, v);
	ged_draw_scene_ref_mark_realized_current(ref);
	ged_draw_scene_ref_invalidate(ref);
    }

    rt_db_free_internal(ip);

    return 1;
}

struct ged_full_detail_clbk_data {
    struct db_i *dbip;
    struct directory *dp;
    struct rt_db_internal *intern;
    const struct bg_tess_tol *ttol;
    const struct bn_tol *tol;
    int *faces;
    int face_cnt;
    vect_t *normals;
    point_t *pnts;
    int pnt_cnt;
};

/* Set up the data for drawing */
static int
bot_mesh_info_clbk(struct bsg_mesh_lod *lod, void *cb_data)
{
    if (!lod || !cb_data)
	return -1;

    struct ged_full_detail_clbk_data *cd = (struct ged_full_detail_clbk_data *)cb_data;
    struct db_i *dbip = cd->dbip;
    struct directory *dp = cd->dp;

    BU_GET(cd->intern, struct rt_db_internal);
    RT_DB_INTERNAL_INIT(cd->intern);
    struct rt_db_internal *ip = cd->intern;
    int ret = rt_db_get_internal(ip, dp, dbip, NULL);
    if (ret < 0) {
	BU_PUT(cd->intern, struct rt_db_internal);
	return -1;
    }
    struct rt_bot_internal *bot = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(bot);

    lod->faces = bot->faces;
    lod->fcnt = bot->num_faces;
    lod->pcnt = bot->num_vertices;
    lod->points = (const point_t *)bot->vertices;
    lod->points_orig = (const point_t *)bot->vertices;

    return 0;
}

/* Free up the drawing data, but not (yet) done with ged_full_detail_clbk_data */
static int
bot_mesh_info_clear_clbk(struct bsg_mesh_lod *lod, void *cb_data)
{
    struct ged_full_detail_clbk_data *cd = (struct ged_full_detail_clbk_data *)cb_data;
    if (cd->intern) {
	rt_db_free_internal(cd->intern);
	BU_PUT(cd->intern, struct rt_db_internal);
    }
    cd->intern = NULL;

    lod->faces = NULL;
    lod->fcnt = 0;
    lod->pcnt = 0;
    lod->points = NULL;
    lod->points_orig = NULL;

    return 0;
}

/* Done - free up everything */
static int
bot_mesh_info_free_clbk(struct bsg_mesh_lod *lod, void *cb_data)
{
    bot_mesh_info_clear_clbk(lod, cb_data);
    struct ged_full_detail_clbk_data *cd = (struct ged_full_detail_clbk_data *)cb_data;
    BU_PUT(cd, struct ged_full_detail_clbk_data);
    return 0;
}

static int
brep_mesh_info_clbk(struct bsg_mesh_lod *lod, void *cb_data)
{
    if (!lod || !cb_data)
	return -1;

    struct ged_full_detail_clbk_data *cd = (struct ged_full_detail_clbk_data *)cb_data;
    struct db_i *dbip = cd->dbip;
    struct directory *dp = cd->dp;

    BU_GET(cd->intern, struct rt_db_internal);
    RT_DB_INTERNAL_INIT(cd->intern);
    struct rt_db_internal *ip = cd->intern;
    int ret = rt_db_get_internal(ip, dp, dbip, NULL);
    if (ret < 0) {
	BU_PUT(cd->intern, struct rt_db_internal);
	cd->intern = NULL;
	return -1;
    }

    struct rt_brep_internal *bi = (struct rt_brep_internal *)ip->idb_ptr;
    RT_BREP_CK_MAGIC(bi);

    ret = brep_cdt_fast(&cd->faces, &cd->face_cnt, &cd->normals,
	    &cd->pnts, &cd->pnt_cnt, bi->brep, -1, cd->ttol, cd->tol);
    rt_db_free_internal(cd->intern);
    BU_PUT(cd->intern, struct rt_db_internal);
    cd->intern = NULL;
    if (ret != BRLCAD_OK) {
	bu_free(cd->faces, "faces");
	bu_free(cd->normals, "normals");
	bu_free(cd->pnts, "pnts");
	cd->faces = NULL;
	cd->face_cnt = 0;
	cd->normals = NULL;
	cd->pnts = NULL;
	cd->pnt_cnt = 0;
	return -1;
    }

    lod->faces = cd->faces;
    lod->fcnt = cd->face_cnt;
    lod->pcnt = cd->pnt_cnt;
    lod->points = (const point_t *)cd->pnts;
    lod->porig_cnt = cd->pnt_cnt;
    lod->points_orig = (const point_t *)cd->pnts;
    lod->normals = cd->normals;

    return 0;
}

static int
brep_mesh_info_clear_clbk(struct bsg_mesh_lod *lod, void *cb_data)
{
    struct ged_full_detail_clbk_data *cd = (struct ged_full_detail_clbk_data *)cb_data;
    if (cd->intern) {
	rt_db_free_internal(cd->intern);
	BU_PUT(cd->intern, struct rt_db_internal);
	cd->intern = NULL;
    }
    bu_free(cd->faces, "faces");
    bu_free(cd->normals, "normals");
    bu_free(cd->pnts, "pnts");
    cd->faces = NULL;
    cd->face_cnt = 0;
    cd->normals = NULL;
    cd->pnts = NULL;
    cd->pnt_cnt = 0;

    lod->faces = NULL;
    lod->fcnt = 0;
    lod->pcnt = 0;
    lod->points = NULL;
    lod->porig_cnt = 0;
    lod->points_orig = NULL;
    lod->normals = NULL;

    return 0;
}

static int
brep_mesh_info_free_clbk(struct bsg_mesh_lod *lod, void *cb_data)
{
    brep_mesh_info_clear_clbk(lod, cb_data);
    struct ged_full_detail_clbk_data *cd = (struct ged_full_detail_clbk_data *)cb_data;
    BU_PUT(cd, struct ged_full_detail_clbk_data);
    return 0;
}

static void
bot_lod_mesh_realize(bsg_scene_ref ref, struct bsg_view *v)
{
    if (bsg_scene_ref_is_null(ref) || !v)
	return;

    ged_draw_scene_ref_realization_set_roles(ref, 0, 1);

    bsg_log(1, "bot_lod_mesh_realize %s[%s]", ged_draw_scene_ref_name(ref), (v) ? bu_vls_cstr(&v->gv_name) : "NULL");

    struct ged_draw_source_state *d = ged_draw_scene_ref_source_data(ref);
    if (!d || !d->mesh_c)
	return;

    if (!draw_mesh_lod(ref)) {

	ged_draw_scene_ref_realization_set_roles(ref, 0, 1);

	struct db_i *dbip = d->dbip;
	struct directory *dp = ged_draw_scene_ref_leaf_dp(ref);

	if (!dp)
	    return;

	// We need the key to look up the LoD data from the cache, and if we don't
	// already have cache data for this bot we need to generate it.
	unsigned long long key = bsg_mesh_lod_key_get(d->mesh_c, dp->d_namep);
	if (!key) {
	    // We don't have a key associated with the name.  Get and check the BoT
	    // data itself, creating the LoD data if we don't already have it
	    struct rt_db_internal dbintern;
	    RT_DB_INTERNAL_INIT(&dbintern);
	    struct rt_db_internal *ip = &dbintern;
	    int ret = rt_db_get_internal(ip, dp, dbip, NULL);
	    if (ret < 0)
		return;
	    struct rt_bot_internal *bot = (struct rt_bot_internal *)ip->idb_ptr;
	    RT_BOT_CK_MAGIC(bot);
	    key = bsg_mesh_lod_cache(d->mesh_c, (const point_t *)bot->vertices, bot->num_vertices, NULL, bot->faces, bot->num_faces, 0, 0.66);
	    bsg_mesh_lod_key_put(d->mesh_c, dp->d_namep, key);
	    rt_db_free_internal(&dbintern);
	}
	if (!key)
	    return;

	// Once we have a valid key, proceed to create the necessary
	// data structures and objects.
	struct bsg_mesh_lod *lod = bsg_mesh_lod_create(d->mesh_c, key);
	if (!lod) {
	    // Stale key?  Clear it and try a regeneration
	    unsigned long long old_key = key;
	    bsg_mesh_lod_clear_cache(d->mesh_c, key);

	    // Load mesh and process
	    struct rt_db_internal dbintern;
	    RT_DB_INTERNAL_INIT(&dbintern);
	    struct rt_db_internal *ip = &dbintern;
	    int ret = rt_db_get_internal(ip, dp, dbip, NULL);
	    if (ret < 0)
		return;
	    struct rt_bot_internal *bot = (struct rt_bot_internal *)ip->idb_ptr;
	    RT_BOT_CK_MAGIC(bot);
	    key = bsg_mesh_lod_cache(d->mesh_c, (const point_t *)bot->vertices, bot->num_vertices, NULL, bot->faces, bot->num_faces, 0, 0.66);
	    bsg_mesh_lod_key_put(d->mesh_c, dp->d_namep, key);
	    rt_db_free_internal(&dbintern);

	    // Sanity
	    if (old_key == key) {
		bu_log("%s: LoD lookup by key failed, but regeneration generated the same key (?)\n", dp->d_namep);
		return;
	    }
	    unsigned long long new_key = bsg_mesh_lod_key_get(d->mesh_c, dp->d_namep);
	    if (new_key == old_key) {
		bu_log("%s: LoD regenerated with new key, but key lookup still returns old key (?)\n", dp->d_namep);
		return;
	    }

	    // If after all that we STILL don't get an LoD struct, give up
	    lod = bsg_mesh_lod_create(d->mesh_c, key);
	    if (!lod)
		return;
	}

	d->mesh_lod = lod;
	VMOVE(d->mesh_lod_bmin, lod->bmin);
	VMOVE(d->mesh_lod_bmax, lod->bmax);
	d->mesh_lod_bounds_valid = 1;

	// The object bounds are based on the LoD's calculations.  Because the LoD
	// cache stores only one cached data set per object, but full path
	// instances in the scene can be placed with matrices, we must apply the
	// s_mat transformation to the "baseline" LoD bbox info to get the correct
	// box for the instance.
	draw_mesh_lod_bounds_restore(ref, d);

	// Record the necessary information for full detail information recovery.  We
	// don't duplicate the full mesh detail in the on-disk LoD storage, since we
	// already have that info in the .g itself, but we need to know how to get at
	// it when needed.  The free callback will clean up, but we need to initialize
	// the callback data here.
	struct ged_full_detail_clbk_data *cbd;
	BU_GET(cbd, ged_full_detail_clbk_data);
	memset(cbd, 0, sizeof(*cbd));
	cbd->dbip = dbip;
	cbd->dp = dp;
	bsg_mesh_lod_detail_setup_clbk(lod, &bot_mesh_info_clbk, (void *)cbd);
	bsg_mesh_lod_detail_clear_clbk(lod, &bot_mesh_info_clear_clbk);
	bsg_mesh_lod_detail_free_clbk(lod, &bot_mesh_info_free_clbk);

	// Initialize the LoD data to the current view
	int level = bsg_mesh_lod_load_view_scene_ref(lod, ref, v, 0);
	if (level < 0) {
	    bu_log("Error loading info for initial LoD view\n");
	}
    }

    bsg_mesh_lod_load_view_scene_ref(draw_mesh_lod(ref), ref, v, 0);
    draw_mesh_lod_publish(ref, draw_mesh_lod(ref));
    draw_mesh_lod_bounds_restore(ref, d);
    ged_draw_scene_ref_invalidate(ref);

    return;
}

static void
brep_lod_mesh_realize(bsg_scene_ref ref, struct bsg_view *v)
{
    if (bsg_scene_ref_is_null(ref) || !v)
	return;
    struct ged_draw_source_state *d = ged_draw_scene_ref_source_data(ref);
    if (!d || !d->mesh_c)
	return;
    bsg_log(1, "brep_lod_mesh_realize %s[%s]", ged_draw_scene_ref_name(ref), (v) ? bu_vls_cstr(&v->gv_name) : "NULL");

    ged_draw_scene_ref_realization_set_roles(ref, 0, 1);

    if (!draw_mesh_lod(ref)) {

	struct db_i *dbip = d->dbip;
	struct directory *dp = ged_draw_scene_ref_leaf_dp(ref);

	if (!dp)
	    return;

	const struct bn_tol *tol = d->tol;
	const struct bg_tess_tol *ttol = d->ttol;
	struct bsg_mesh_lod *lod = NULL;

	// We need the key to look up the LoD data from the cache, and if we don't
	// already have cache data for this brep we need to generate it.
	unsigned long long key = bsg_mesh_lod_key_get(d->mesh_c, dp->d_namep);
	if (!key) {
	    // We don't have a key associated with the name.  Get and check the
	    // Brep data itself, creating the mesh data and the corresponding LoD
	    // data if we don't already have it
	    struct bu_external ext = BU_EXTERNAL_INIT_ZERO;
	    if (db_get_external(&ext, dp, dbip))
		return;
	    key = bu_data_hash((void *)ext.ext_buf,  ext.ext_nbytes);
	    bu_free_external(&ext);
	    if (!key)
		return;
	    lod = bsg_mesh_lod_create(d->mesh_c, key);
	    if (!lod) {
		// Just in case we have a stale key...
		bsg_mesh_lod_clear_cache(d->mesh_c, key);

		struct rt_db_internal dbintern;
		RT_DB_INTERNAL_INIT(&dbintern);
		struct rt_db_internal *ip = &dbintern;
		int ret = rt_db_get_internal(ip, dp, dbip, NULL);
		if (ret < 0)
		    return;
		struct rt_brep_internal *bi = (struct rt_brep_internal *)ip->idb_ptr;
		RT_BREP_CK_MAGIC(bi);

		// Unlike a BoT, which has the mesh data already, we need to generate the
		// mesh from the brep
		int *faces = NULL;
		int face_cnt = 0;
		vect_t *normals = NULL;
		point_t *pnts = NULL;
		int pnt_cnt = 0;

		ret = brep_cdt_fast(&faces, &face_cnt, &normals, &pnts, &pnt_cnt, bi->brep, -1, ttol, tol);
		if (ret != BRLCAD_OK) {
		    bu_free(faces, "faces");
		    bu_free(normals, "normals");
		    bu_free(pnts, "pnts");
		    return;
		}

		// Because we won't have the internal data to use for a full detail scenario, we set the ratio
		// to 1 rather than .66 for breps...
		key = bsg_mesh_lod_cache(d->mesh_c, (const point_t *)pnts, pnt_cnt, normals, faces, face_cnt, key, 1);

		if (key)
		    bsg_mesh_lod_key_put(d->mesh_c, dp->d_namep, key);

		rt_db_free_internal(&dbintern);

		bu_free(faces, "faces");
		bu_free(normals, "normals");
		bu_free(pnts, "pnts");
	    }
	}
	if (!key)
	    return;

	// Once we have a valid key, proceed to create the necessary
	// data structures and objects.  If the above didn't get us
	// a valid mesh, no point in trying further
	lod = bsg_mesh_lod_create(d->mesh_c, key);
	if (!lod)
	    return;

	d->mesh_lod = lod;
	if (!draw_brep_lod_bounds_prepare(d, dp)) {
	    VMOVE(d->mesh_lod_bmin, lod->bmin);
	    VMOVE(d->mesh_lod_bmax, lod->bmax);
	    d->mesh_lod_bounds_valid = 1;
	}

	// The object bounds are based on the LoD's calculations.  Because the LoD
	// cache stores only one cached data set per object, but full path
	// instances in the scene can be placed with matrices, we must apply the
	// s_mat transformation to the "baseline" LoD bbox info to get the correct
	// box for the instance.
	draw_mesh_lod_bounds_restore(ref, d);

	// Record the necessary information for full detail information recovery.  We
	// don't duplicate the full mesh detail in the on-disk LoD storage, since we
	// already have that info in the .g itself, but we need to know how to get at
	// it when needed.  The free callback will clean up, but we need to initialize
	// the callback data here.
	struct ged_full_detail_clbk_data *cbd;
	BU_GET(cbd, ged_full_detail_clbk_data);
	memset(cbd, 0, sizeof(*cbd));
	cbd->dbip = dbip;
	cbd->dp = dp;
	cbd->ttol = ttol;
	cbd->tol = tol;
	bsg_mesh_lod_detail_setup_clbk(lod, &brep_mesh_info_clbk, (void *)cbd);
	bsg_mesh_lod_detail_clear_clbk(lod, &brep_mesh_info_clear_clbk);
	bsg_mesh_lod_detail_free_clbk(lod, &brep_mesh_info_free_clbk);

	// Initialize the LoD data to the current view
	int level = bsg_mesh_lod_load_view_scene_ref(lod, ref, v, 0);
	if (level < 0) {
	    bu_log("Error loading info for initial LoD view\n");
	}
    }

    bsg_mesh_lod_load_view_scene_ref(draw_mesh_lod(ref), ref, v, 0);
    draw_mesh_lod_publish(ref, draw_mesh_lod(ref));
    draw_mesh_lod_bounds_restore(ref, d);
    ged_draw_scene_ref_invalidate(ref);

    return;
}

/* Wrapper to handle adaptive vs non-adaptive wireframes */
static void
wireframe_plot(bsg_scene_ref ref, struct bsg_view *v, struct rt_db_internal *ip)
{
    bsg_log(1, "wireframe_plot %s[%s]", ged_draw_scene_ref_name(ref), (v) ? bu_vls_cstr(&v->gv_name) : "NULL");
    struct ged_draw_source_state *d = ged_draw_scene_ref_source_data(ref);
    const struct bn_tol *tol = d->tol;
    const struct bg_tess_tol *ttol = d->ttol;
    ged_draw_scene_ref_realization_set_roles(ref, 1, 0);

    struct bsg_lod_source_policy_settings policy = draw_lod_policy(v);

    // Standard (view independent) wireframe
    if (!v || !policy.csg_enabled) {
	ged_draw_scene_ref_geometry_clear(ref);
	if (ged_draw_scene_ref_publish_primitive_wireframe(ref, ip, ttol, tol,
		bsg_scene_view(ref), 0) >= 0) {
	    // Because this data is view independent, it only needs to be
	    // generated once rather than per-view.
	    ged_draw_scene_ref_realization_set_current(ref, 1);
	}
	return;
    }

    // If we're adaptive, call the primitive's adaptive plotting, if any.
    if (ged_draw_source_primitive_has_lod_realize(ip)) {
	csg_wireframe_update(ref, v, 1);
	return;
    }

    // If we've got this far, we have no adaptive plotting capability for this
    // object.  Do the normal plot rather than show nothing.
    ged_draw_scene_ref_geometry_clear(ref);
    if (ged_draw_scene_ref_publish_primitive_wireframe(ref, ip, ttol, tol,
	    bsg_scene_view(ref), 0) >= 0) {
	// Because this data is view independent, it only needs to be
	// generated once rather than per-view.
	ged_draw_scene_ref_realization_set_current(ref, 1);
    }
}

static void
draw_tessellation_failure_fallback(bsg_scene_ref ref,
				   struct bsg_view *v,
				   struct rt_db_internal *ip,
				   const char *name,
				   const char *mode_name)
{
    if (bsg_scene_strict_fallback(ref)) {
	bu_log("ERROR(%s): %s tessellation failed; geometry cleared\n",
		name ? name : "<unknown>", mode_name ? mode_name : "draw");
	ged_draw_scene_ref_geometry_clear(ref);
	ged_draw_scene_ref_realization_set_current(ref, 0);
	return;
    }

    bu_log("WARNING(%s): %s tessellation failed; falling back to wireframe\n",
	    name ? name : "<unknown>", mode_name ? mode_name : "draw");
    wireframe_plot(ref, v, ip);
}

/* This function is the master controller that decides, based on available settings
 * and data, which specific drawing routines need to be triggered. */
extern "C" void
ged_draw_scene_ref_realize(bsg_scene_ref ref, struct bsg_view *v)
{
    // If the scene object indicates we're good, don't repeat.
    if (ged_draw_scene_ref_realization_current(ref) && !v)
	return;

    bsg_log(1, "draw_scene %s[%s]", ged_draw_scene_ref_name(ref), (v) ? bu_vls_cstr(&v->gv_name) : "NULL");

    // If we're not adaptive, trigger the view insensitive drawing routines
    struct bsg_lod_source_policy_settings policy = draw_lod_policy(v);
    if (v && !policy.csg_enabled && !policy.mesh_enabled) {
	return ged_draw_scene_ref_realize(ref, NULL);
    }

    // If we have a scene object without drawing data, it is most likely
    // a container holding other objects we do need to draw.  Iterate over
    // any children and trigger their drawing operations.
    struct ged_draw_source_state *d = ged_draw_scene_ref_source_data(ref);
    if (!d) {
	for (size_t i = 0; i < bsg_scene_child_count(ref); i++) {
	    bsg_scene_ref child_ref = bsg_scene_child_at(ref, i);
	    ged_draw_scene_ref_realize(child_ref, v);
	}
	return;
    }

    /**************************************************************************
     * A couple of the drawing modes have their own completely independent and
     * substantial logic defined elsewhere.  Spot those cases first and steer
     * them where they need to go
     **************************************************************************/

    /* Evaluated wireframe draws boolean-evaluated edges rather than the
     * individual solid wireframes. */
    if (ged_draw_scene_ref_draw_mode(ref) == BSG_DRAW_MODE_EVAL_WIRE) {
	ged_draw_scene_ref_eval_wireframe(ref);
	bsg_scene_update_bounds(ref, v);
	ged_draw_scene_ref_realization_set_current(ref, 1);
	return;
    }

    /* Sampled evaluated points draw triangle samples in lieu of wireframes. */
    if (ged_draw_scene_ref_draw_mode(ref) == BSG_DRAW_MODE_EVAL_POINTS) {
	ged_draw_scene_ref_eval_points(ref);
	bsg_scene_update_bounds(ref, v);
	ged_draw_scene_ref_realization_set_current(ref, 1);
	return;
    }


    /**************************************************************************
     * A couple of the object types will also have unique logic, typically for
     * special handling of difficult drawing cases.  Look for those as well.
    **************************************************************************/
    struct db_i *dbip = d->dbip;
    const struct db_full_path *fp = ged_draw_scene_ref_fullpath(ref);
    if (fp && fp->fp_len <= 0)
	return;
    struct directory *dp = ged_draw_scene_ref_leaf_dp(ref);
    if (!dp)
	return;

    // Mesh LoD BoTs have specialized routines to help cope with very large
    // data sets, both for wireframe and shaded mode.
    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_BOT && v && policy.mesh_enabled &&
       (ged_draw_scene_ref_draw_mode(ref) == 0 || ged_draw_scene_ref_draw_mode(ref) == 1)) {
	bot_lod_mesh_realize(ref, v);
	return;
    }

    // Mesh LoD BReps have specialized routines to manage shaded displays, which
    // can involve slow and large mesh generations.  BRep wireframes are based on the
    // NURBS data, so this is used only for shaded mode
    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_BREP && v && policy.mesh_enabled && ged_draw_scene_ref_draw_mode(ref) == 1) {
	brep_lod_mesh_realize(ref, v);
	return;
    }

    /**************************************************************************
     * For the remainder of the options we're into more standard wireframe
     * callback modes - crack the internal and stage the tolerances
     **************************************************************************/
    const struct bn_tol *tol = d->tol;
    const struct bg_tess_tol *ttol = d->ttol;
    struct rt_db_internal dbintern;
    RT_DB_INTERNAL_INIT(&dbintern);
    struct rt_db_internal *ip = &dbintern;
    mat_t _smat;
    if (!ged_draw_scene_ref_draw_mat(ref, _smat))
	return;
    int ret = rt_db_get_internal(ip, dp, dbip, _smat);
    if (ret < 0)
	return;

    // If we don't have a BRL-CAD type, see if we've got a plot routine
    if (ip->idb_major_type != DB5_MAJORTYPE_BRLCAD) {
	wireframe_plot(ref, v, ip);
	goto geom_done;
    }

    // At least for the moment, we don't try anything fancy with pipes - they
    // get a wireframe, regardless of mode settings
    if (ip->idb_minor_type == DB5_MINORTYPE_BRLCAD_PIPE) {
	wireframe_plot(ref, v, ip);
	goto geom_done;
    }

    // For anything other than mode 0, let primitives with typed surface
    // realization callbacks publish those surfaces directly.
    if (ged_draw_scene_ref_draw_mode(ref) > 0) {
	ged_draw_scene_ref_realization_set_roles(ref, 0, 0);
	if (ip->idb_meth && ip->idb_meth->ft_indexed_face_set) {
	    if (!ged_draw_scene_ref_publish_primitive_face_set(ref, ip, ttol,
			tol, v)) {
		bu_log("ERROR(%s): %s shaded face-set publication failed\n",
			dp->d_namep, ip->idb_meth->ft_label);
	    }
	    goto geom_done;
	}
    }

    // Now the more general cases
    switch (ged_draw_scene_ref_draw_mode(ref)) {
	case 0:
	case 1:
	    // Get wireframe (for mode 1, all the non-wireframes are handled
	    // by the above BOT/POLY/BREP cases
	    wireframe_plot(ref, v, ip);
	    ged_draw_scene_ref_set_draw_mode(ref, 0);
	    break;
	case 2:
	    // Shade everything except pipe, don't evaluate.  Preserve legacy
	    // interactive fallback by default; --strict makes failures explicit.
	    if (prim_tess(ref, ip) < 0) {
		draw_tessellation_failure_fallback(ref, v, ip, dp->d_namep, "shaded");
	    } else {
		ged_draw_scene_ref_realization_set_current(ref, 1);
	    }
	    break;
	case BSG_DRAW_MODE_EVAL_WIRE:
	    // Evaluated wireframes
	    bu_log("Error - got too deep into _scene_obj_draw routine with evaluated wireframe mode\n");
	    return;
	    break;
	case 4:
	    // Hidden line - generate polygonal forms.  Preserve legacy fallback
	    // by default; --strict makes failures explicit.
	    if (prim_tess(ref, ip) < 0) {
		draw_tessellation_failure_fallback(ref, v, ip, dp->d_namep, "hidden-line");
	    } else {
		ged_draw_scene_ref_realization_set_current(ref, 1);
	    }
	    break;
	case BSG_DRAW_MODE_EVAL_POINTS:
	    // Triangles at sampled points
	    bu_log("Error - got too deep into _scene_obj_draw routine with evaluated sampled-points mode\n");
	    return;
	    break;
	default:
	    // Default to wireframe
	    wireframe_plot(ref, v, ip);
	    break;
    }

geom_done:

    // Update s_size and s_center
    bsg_scene_update_bounds(ref, v);

    // Store current view info, in case of adaptive plotting
    struct bsg_view *sv = bsg_scene_view(ref);
    if (sv) {
	struct bsg_lod_source_policy_settings sv_policy = draw_lod_policy(sv);
	ged_draw_scene_ref_realization_set_view_policy(ref,
		sv_policy.csg_enabled,
		sv->gv_scale,
		sv_policy.bot_threshold,
		sv_policy.curve_scale,
		sv_policy.point_scale);
    }

    rt_db_free_internal(&dbintern);
}

static void
tree_color(struct directory *dp, struct draw_data_t *dd)
{
    struct bu_attribute_value_set c_avs = BU_AVS_INIT_ZERO;

    // Easy answer - if we're overridden, dd color is already set.
    if (dd->vs && dd->vs->color_override)
	return;

    // Not overridden by settings.  Next question - are we under an inherit?
    // If so, dd color is already set.
    if (dd->color_inherit)
	return;

    // Need attributes for the rest of this
    db5_get_attributes(dd->dbip, &c_avs, dp);

    // No inherit.  Do we have a region material table?
    if (db_mater_head(dd->dbip) != MATER_NULL) {
	// If we do, do we have a region id?
	int region_id = -1;
	const char *region_id_val = bu_avs_get(&c_avs, "region_id");
	if (region_id_val) {
	    bu_opt_int(NULL, 1, &region_id_val, (void *)&region_id);
	} else if (dp->d_flags & RT_DIR_REGION) {
	    // If we have a region flag but no region_id, for color table
	    // purposes treat the region_id as 0
	    region_id = 0;
	}
	if (region_id >= 0) {
	    const struct mater *mp;
	    int material_color = 0;
	    for (mp = db_mater_head(dd->dbip); mp != MATER_NULL; mp = mp->mt_forw) {
		if (region_id > mp->mt_high || region_id < mp->mt_low) {
		    continue;
		}
		unsigned char mt[3];
		mt[0] = mp->mt_r;
		mt[1] = mp->mt_g;
		mt[2] = mp->mt_b;
		bu_color_from_rgb_chars(&dd->c, mt);
		material_color = 1;
	    }
	    if (material_color) {
		// Have answer from color table
		bu_avs_free(&c_avs);
		return;
	    }
	}
    }

    // Material table didn't give us the answer - do we have a color or
    // rgb attribute?
    const char *color_val = bu_avs_get(&c_avs, "color");
    if (!color_val) {
	color_val = bu_avs_get(&c_avs, "rgb");
    }
    if (color_val) {
	bu_opt_color(NULL, 1, &color_val, (void *)&dd->c);
    }

    // Check for an inherit flag
    dd->color_inherit = (BU_STR_EQUAL(bu_avs_get(&c_avs, "inherit"), "1")) ? 1 : 0;

    // Done with attributes
    bu_avs_free(&c_avs);
}

/*****************************************************************************

  The primary drawing subtree walk.

  To get an initial idea of scene size and center for adaptive plotting (i.e.
  before we have wireframes drawn) we also have a need to check ft_bbox ahead
  of the vlist generation.  It can be thought of as a "preliminary autoview"
  step.  That mode is also supported by this subtree walk.

******************************************************************************/
static void
draw_walk_tree(struct db_full_path *path, union tree *tp, mat_t *curr_mat,
			 void (*traverse_func) (struct db_full_path *path, mat_t *, void *),
			 void *client_data, void *comb_inst_map)
{
    mat_t om, nm;
    struct directory *dp;
    struct draw_data_t *dd= (struct draw_data_t *)client_data;
    std::unordered_map<std::string, int> *cinst_map = (std::unordered_map<std::string, int> *)comb_inst_map;

    if (!tp)
	return;

    RT_CK_FULL_PATH(path);
    RT_CHECK_DBI(dd->dbip);
    RT_CK_TREE(tp);

    switch (tp->tr_op) {
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    draw_walk_tree(path, tp->tr_b.tb_left, curr_mat, traverse_func, client_data, comb_inst_map);
	    break;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    draw_walk_tree(path, tp->tr_b.tb_left, curr_mat, traverse_func, client_data, comb_inst_map);
	    dd->bool_op = tp->tr_op;
	    draw_walk_tree(path, tp->tr_b.tb_right, curr_mat, traverse_func, client_data, comb_inst_map);
	    break;
	case OP_DB_LEAF:
	    if (UNLIKELY(dd->dbip->i->dbi_use_comb_instance_ids && cinst_map))
		(*cinst_map)[std::string(tp->tr_l.tl_name)]++;
	    if ((dp=db_lookup(dd->dbip, tp->tr_l.tl_name, LOOKUP_QUIET)) == RT_DIR_NULL) {
		return;
	    } else {

		/* Update current matrix state to reflect the new branch of
		 * the tree. Either we have a local matrix, or we have an
		 * implicit IDN matrix. */
		MAT_COPY(om, *curr_mat);
		if (tp->tr_l.tl_mat) {
		    MAT_COPY(nm, tp->tr_l.tl_mat);
		} else {
		    MAT_IDN(nm);
		}
		bn_mat_mul(*curr_mat, om, nm);

		// Stash current color settings and see if we're getting new ones
		struct bu_color oc;
		int inherit_old = dd->color_inherit;
		HSET(oc.buc_rgb, dd->c.buc_rgb[0], dd->c.buc_rgb[1], dd->c.buc_rgb[2], dd->c.buc_rgb[3]);
		if (!dd->bound_only) {
		    tree_color(dp, dd);
		}

		// Two things may prevent further processing - a hidden dp, or
		// a cyclic path.  Can check either here or in traverse_func -
		// just do it here since otherwise the logic would have to be
		// duplicated in all traverse functions.
		if (!(dp->d_flags & RT_DIR_HIDDEN)) {
		    db_add_node_to_full_path(path, dp);
		    DB_FULL_PATH_SET_CUR_BOOL(path, tp->tr_op);
		    if (UNLIKELY(dd->dbip->i->dbi_use_comb_instance_ids && cinst_map))
			DB_FULL_PATH_SET_CUR_COMB_INST(path, (*cinst_map)[std::string(tp->tr_l.tl_name)]-1);
		    if (!db_full_path_cyclic(path, NULL, 0)) {
			/* Keep going */
			traverse_func(path, curr_mat, client_data);
		    }
		}

		/* Done with branch - restore path, put back the old matrix state,
		 * and restore previous color settings */
		DB_FULL_PATH_POP(path);
		MAT_COPY(*curr_mat, om);
		if (!dd->bound_only) {
		    dd->color_inherit = inherit_old;
		    HSET(dd->c.buc_rgb, oc.buc_rgb[0], oc.buc_rgb[1], oc.buc_rgb[2], oc.buc_rgb[3]);
		}
		return;
	    }

	default:
	    bu_log("db_functree_subtree: unrecognized operator %d\n", tp->tr_op);
	    bu_bomb("db_functree_subtree: unrecognized operator\n");
    }
}

/**
 * This walker builds a list of db_full_path entries corresponding to
 * the contents of the tree under *path.  It does so while assigning
 * the boolean operation associated with each path entry to the
 * db_full_path structure.  This list is then used for further
 * processing and filtering by the search routines.
 */
extern "C" void
draw_gather_paths(struct db_full_path *path, mat_t *curr_mat, void *client_data)
{
    struct directory *dp;
    struct draw_data_t *dd= (struct draw_data_t *)client_data;
    RT_CK_FULL_PATH(path);
    RT_CK_DBI(dd->dbip);

    dp = DB_FULL_PATH_CUR_DIR(path);
    if (!dp)
	return;

    // If we're skipping subtractions and we have a subtraction op there's no
    // point in going further.
    if (dd->vs && dd->vs->draw_non_subtract_only && dd->bool_op == 4) {
	return;
    }


    if (dp->d_flags & RT_DIR_COMB) {

	struct rt_db_internal in;
	struct rt_comb_internal *comb;

	if (rt_db_get_internal(&in, dp, dd->dbip, NULL) < 0)
	    return;

	comb = (struct rt_comb_internal *)in.idb_ptr;
	if (UNLIKELY(dd->dbip->i->dbi_use_comb_instance_ids)) {
	    std::unordered_map<std::string, int> cinst_map;
	    draw_walk_tree(path, comb->tree, curr_mat, draw_gather_paths, client_data, (void *)&cinst_map);
	} else {
	    draw_walk_tree(path, comb->tree, curr_mat, draw_gather_paths, client_data, NULL);
	}
	rt_db_free_internal(&in);

    } else {

	// If we've got a solid, things get interesting.  There are a lot of
	// potentially relevant options to sort through.  It may be that most
	// will end up getting handled by the object update callbacks, and the
	// job here will just be to set up the key data for later use...

	ged_draw_shape_draft *draft = ged_draw_shape_draft_create(dd->gedp, dd->v, 0);
	if (!draft)
	    return;

	char *_nm = db_path_to_string(path);
	ged_draw_shape_draft_set_name(draft, _nm);
	bu_free(_nm, "path string");
	ged_draw_shape_draft_set_fullpath(draft, path);
	ged_draw_shape_draft_set_draw_mat(draft, *curr_mat);
	if (dd->vs)
	    ged_draw_shape_draft_apply_settings(draft, dd->vs);
	ged_draw_shape_draft_mark_db_object(draft);
	ged_draw_shape_draft_bump_appearance_revision(draft);
	if (!dd->vs || !dd->vs->draw_solid_lines_only)
	    ged_draw_shape_draft_set_line_style(draft, (dd->bool_op == 4) ? 1 : 0);
	{
	    unsigned char _rgb[3];
	    bu_color_to_rgb_chars(&dd->c, _rgb);
	    ged_draw_shape_draft_set_material_rgb(draft, _rgb[0], _rgb[1], _rgb[2]);
	}

	// Stash the information needed for a draw update callback.
	struct ged_draw_source_state *ud;
	BU_GET(ud, struct ged_draw_source_state);
	memset(ud, 0, sizeof(*ud));
	BU_GET(ud->fp, struct db_full_path);
	db_full_path_init(ud->fp);
	db_dup_full_path(ud->fp, path);
	ud->dbip = dd->dbip;
	ud->tol = dd->tol;
	ud->ttol = dd->ttol;
	ud->mesh_c = dd->mesh_c;
	ud->stale_reason = GED_DRAW_STALE_NONE;
	ged_draw_shape_draft_set_source_state(draft, ud);

	// Let the object know about its size
	if (dd->s_size && dd->s_size->find(DB_FULL_PATH_CUR_DIR(path)) != dd->s_size->end()) {
	    ged_draw_shape_draft_set_draw_size(draft, (*dd->s_size)[DB_FULL_PATH_CUR_DIR(path)]);
	}

	(void)ged_draw_shape_draft_commit_to_scene_ref(draft, dd->g_ref);

    }
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
