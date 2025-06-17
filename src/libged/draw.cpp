/*                         D R A W . C P P
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
#include "bv/defines.h"
#include "bg/sat.h"
#include "bv/lod.h"
#include "nmg.h"
#include "rt/view.h"

#include "ged/view.h"
#include "./alphanum.h"
#include "./ged_private.h"

#if 0
static int
prim_tess(struct bv_scene_obj *s, struct rt_db_internal *ip)
{
    struct draw_update_data_t *d = (struct draw_update_data_t *)s->s_i_data;
    struct db_full_path *fp = (struct db_full_path *)s->s_path;
    struct directory *dp = (fp) ? DB_FULL_PATH_CUR_DIR(fp) : (struct directory *)s->dp;
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
    nmg_r_to_vlist(&s->s_vlist, r, NMG_VLIST_STYLE_POLYGON, s->vlfree);
    nmg_km(m);

    s->current = 1;

    return 0;
}
#endif

#if 0
static int
csg_wireframe_update(struct bv_scene_obj *vo, struct bview *v, int flag)
{
    /* Validate */
    if (!vo || !v)
	return 0;

    if (!v->gv_s->adaptive_plot_csg)
	return 0;

    bv_log(1, "csg_wireframe_update %s[%s]", bu_vls_cstr(&vo->s_name), bu_vls_cstr(&v->gv_name));

    vo->csg_obj = 1;

    // If the object is not visible in the scene, don't change the data.  This
    // check is useful in orthographic camera mode, where we zoom in on a
    // narrow subset of the model and far away objects are still rendered in
    // full detail.  If we have a perspective matrix active don't make this
    // check, since far away objects outside the view obb will be visible.
    //bu_log("min: %f %f %f max: %f %f %f\n", V3ARGS(vo->bmin), V3ARGS(vo->bmax));
    if (!(v->gv_perspective > SMALL_FASTF) && !bg_sat_aabb_obb(vo->bmin, vo->bmax, v->obb_center, v->obb_extent1, v->obb_extent2, v->obb_extent3))
	return 0;

    bool rework = (flag) ? true : false;

    // Check point scale
    if (!rework && !NEAR_EQUAL(vo->curve_scale, vo->s_v->gv_s->curve_scale, SMALL_FASTF))
	rework = true;
    // Check point scale
    if (!rework && !NEAR_EQUAL(vo->point_scale, vo->s_v->gv_s->point_scale, SMALL_FASTF))
	rework = true;
    if (!rework) {
	// Check view scale
	fastf_t delta = vo->view_scale * 0.1/vo->view_scale;
	if (!NEAR_EQUAL(vo->view_scale, v->gv_scale, delta))
	    rework = true;
    }
    if (!rework)
	return 0;

    // We're going to redraw - sync with view
    vo->curve_scale = v->gv_s->curve_scale;
    vo->point_scale = v->gv_s->point_scale;
    vo->view_scale = v->gv_scale;

    // Clear out existing vlists
    struct bu_list *p;
    while (BU_LIST_WHILE(p, bu_list, &vo->s_vlist)) {
	BU_LIST_DEQUEUE(p);
	struct bv_vlist *pv = (struct bv_vlist *)p;
	BU_FREE(pv, struct bv_vlist);
    }

    struct draw_update_data_t *d = (struct draw_update_data_t *)vo->s_i_data;
    struct db_full_path *fp = (struct db_full_path *)vo->s_path;
    struct directory *dp = (fp) ? DB_FULL_PATH_CUR_DIR(fp) : (struct directory *)vo->dp;
    struct db_i *dbip = d->dbip;
    struct rt_db_internal dbintern;
    RT_DB_INTERNAL_INIT(&dbintern);
    struct rt_db_internal *ip = &dbintern;
    int ret = rt_db_get_internal(ip, dp, dbip, NULL, d->res);
    if (ret < 0)
	return 0;

    if (ip->idb_meth->ft_adaptive_plot) {
	ip->idb_meth->ft_adaptive_plot(&vo->s_vlist, ip, d->tol, v, vo->s_size);
	vo->s_type_flags |= BV_CSG_LOD;
	bv_obj_stale(vo);
    }

    return 1;
}
#endif

#if 0
static void
brep_adaptive_plot(struct bv_scene_obj *s, struct bview *v)
{
    if (!s || !v)
	return;
    struct draw_update_data_t *d = (struct draw_update_data_t *)s->s_i_data;
    if (!d || !d->mesh_c)
	return;
    bv_log(1, "brep_adaptive_plot %s[%s]", bu_vls_cstr(&s->s_name), (v) ? bu_vls_cstr(&v->gv_name) : "NULL");

    s->csg_obj = 0;
    s->mesh_obj = 1;

    struct bv_scene_obj *vo = bv_obj_for_view(s, v);

    if (!vo) {

	vo = bv_obj_get_vo(s, v);

	vo->csg_obj = 0;
	vo->mesh_obj = 1;

	struct db_i *dbip = d->dbip;
	struct db_full_path *fp = (struct db_full_path *)s->s_path;
	struct directory *dp = (fp) ? DB_FULL_PATH_CUR_DIR(fp) : (struct directory *)s->dp;

	if (!dp)
	    return;

	const struct bn_tol *tol = d->tol;
	const struct bg_tess_tol *ttol = d->ttol;
	struct bv_mesh_lod *lod = NULL;

	// We need the key to look up the LoD data from the cache, and if we don't
	// already have cache data for this brep we need to generate it.
	unsigned long long key = bv_mesh_lod_key_get(d->mesh_c, dp->d_namep);
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
	    lod = bv_mesh_lod_create(d->mesh_c, key);
	    if (!lod) {
		// Just in case we have a stale key...
		bv_mesh_lod_clear_cache(d->mesh_c, key);

		struct rt_db_internal dbintern;
		RT_DB_INTERNAL_INIT(&dbintern);
		struct rt_db_internal *ip = &dbintern;
		int ret = rt_db_get_internal(ip, dp, dbip, NULL, d->res);
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
		key = bv_mesh_lod_cache(d->mesh_c, (const point_t *)pnts, pnt_cnt, normals, faces, face_cnt, key, 1);

		if (key)
		    bv_mesh_lod_key_put(d->mesh_c, dp->d_namep, key);

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
	lod = bv_mesh_lod_create(d->mesh_c, key);
	if (!lod)
	    return;

	// Assign the LoD information to the object's draw_data, and let
	// the LoD know which object it is associated with.
	vo->draw_data = (void *)lod;
	lod->s = vo;

	// The object bounds are based on the LoD's calculations.  Because the LoD
	// cache stores only one cached data set per object, but full path
	// instances in the scene can be placed with matrices, we must apply the
	// s_mat transformation to the "baseline" LoD bbox info to get the correct
	// box for the instance.
	MAT4X3PNT(vo->bmin, s->s_mat, lod->bmin);
	MAT4X3PNT(vo->bmax, s->s_mat, lod->bmax);
	VMOVE(s->bmin, vo->bmin);
	VMOVE(s->bmax, vo->bmax);

	// Record the necessary information for full detail information recovery.  We
	// don't duplicate the full mesh detail in the on-disk LoD storage, since we
	// already have that info in the .g itself, but we need to know how to get at
	// it when needed.  The free callback will clean up, but we need to initialize
	// the callback data here.
	struct ged_full_detail_clbk_data *cbd;
	BU_GET(cbd, ged_full_detail_clbk_data);
	cbd->dbip = dbip;
	cbd->dp = dp;
	cbd->res = &rt_uniresource;
	cbd->intern = NULL;
	bv_mesh_lod_detail_setup_clbk(lod, &bot_mesh_info_clbk, (void *)cbd);
	bv_mesh_lod_detail_clear_clbk(lod, &bot_mesh_info_clear_clbk);
	bv_mesh_lod_detail_free_clbk(lod, &bot_mesh_info_free_clbk);

	// LoD will need to re-check its level settings whenever the view changes
	vo->s_update_callback = &bv_mesh_lod_view;
	vo->s_free_callback = &bv_mesh_lod_free;

	// Initialize the LoD data to the current view
	int level = bv_mesh_lod_view(vo, vo->s_v, 0);
	if (level < 0) {
	    bu_log("Error loading info for initial LoD view\n");
	}

	// Mark the object as a Mesh LoD so the drawing routine knows to handle it differently
	vo->s_type_flags |= BV_MESH_LOD;
    }

    bv_mesh_lod_view(vo, vo->s_v, 0);
    bv_obj_stale(vo);
    return;
}
#endif

#if 0
static void
tree_color(struct directory *dp, struct draw_data_t *dd)
{
    struct bu_attribute_value_set c_avs = BU_AVS_INIT_ZERO;

    // Easy answer - if we're overridden, dd color is already set.
    if (dd->g->s_os->color_override)
	return;

    // Not overridden by settings.  Next question - are we under an inherit?
    // If so, dd color is already set.
    if (dd->color_inherit)
	return;

    // Need attributes for the rest of this
    db5_get_attributes(dd->dbip, &c_avs, dp);

    // No inherit.  Do we have a region material table?
    if (rt_material_head() != MATER_NULL) {
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
	    for (mp = rt_material_head(); mp != MATER_NULL; mp = mp->mt_forw) {
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
#endif

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

