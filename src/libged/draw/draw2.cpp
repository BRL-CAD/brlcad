/*                         D R A W 2 . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
/** @file libged/draw2.c
 *
 * Testing command for experimenting with drawing routines.
 *
 */

#include "common.h"

#include <set>

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include "bsocket.h"

#include "bu/cmd.h"
#include "bu/opt.h"
#include "bu/sort.h"
#include "bg/lod.h"
#include "nmg.h"
#include "rt/view.h"

#include "ged/view/state.h"
#define ALPHANUM_IMPL
#include "../alphanum.h"
#include "../ged_private.h"


static int
_fp_bbox(fastf_t *s_size, point_t *bmin, point_t *bmax,
	 struct db_full_path *fp,
	 struct db_i *dbip,
	 const struct bg_tess_tol *ttol,
	 const struct bn_tol *tol,
	 mat_t *s_mat,
	 struct resource *res,
	 struct bview *v
	)
{
    VSET(*bmin, INFINITY, INFINITY, INFINITY);
    VSET(*bmax, -INFINITY, -INFINITY, -INFINITY);
    *s_size = 1;

    struct rt_db_internal dbintern;
    RT_DB_INTERNAL_INIT(&dbintern);
    struct rt_db_internal *ip = &dbintern;
    int ret = rt_db_get_internal(ip, DB_FULL_PATH_CUR_DIR(fp), dbip, *s_mat, res);
    if (ret < 0)
	return -1;

    int bbret = -1;
    if (ip->idb_meth->ft_bbox) {
	bbret = ip->idb_meth->ft_bbox(ip, bmin, bmax, tol);
    }
    if (bbret < 0 && ip->idb_meth->ft_plot) {
	/* As a fallback for primitives that don't have a bbox function,
	 * (there are still some as of 2021) use the old bounding method of
	 * calculating the default (non-adaptive) plot for the primitive
	 * and using the extent of the plotted segments as the bounds.
	 */
	struct bu_list vhead;
	BU_LIST_INIT(&(vhead));
	if (ip->idb_meth->ft_plot(&vhead, ip, ttol, tol, v) >= 0) {
	    if (bv_vlist_bbox(&vhead, bmin, bmax, NULL, NULL)) {
		BV_FREE_VLIST(&v->gv_objs.gv_vlfree, &vhead);
		rt_db_free_internal(&dbintern);
		return -1;
	    }
	    BV_FREE_VLIST(&v->gv_objs.gv_vlfree, &vhead);
	    bbret = 0;
	}
    }
    if (bbret >= 0) {
	// Got bounding box, use it to update sizing
	*s_size = (*bmax)[X] - (*bmin)[X];
	V_MAX(*s_size, (*bmax)[Y] - (*bmin)[Y]);
	V_MAX(*s_size, (*bmax)[Z] - (*bmin)[Z]);
    }

    rt_db_free_internal(&dbintern);
    return bbret;
}

/**
 * This walker builds up scene size and bounding information.
 */
static void
_bound_fp(struct db_full_path *path, mat_t *curr_mat, void *client_data)
{
    struct directory *dp;
    struct draw_data_t *dd = (struct draw_data_t *)client_data;
    RT_CK_FULL_PATH(path);
    RT_CK_DBI(dd->dbip);

    dp = DB_FULL_PATH_CUR_DIR(path);
    if (!dp)
	return;
    if (dp->d_flags & RT_DIR_COMB) {
	// Have a comb - keep going
	struct rt_db_internal in;
	struct rt_comb_internal *comb;
	if (rt_db_get_internal(&in, dp, dd->dbip, NULL, dd->res) < 0)
	    return;
	comb = (struct rt_comb_internal *)in.idb_ptr;
	draw_walk_tree(path, comb->tree, curr_mat, _bound_fp, client_data, NULL);
	rt_db_free_internal(&in);
	// Use bbox to update sizing
	if (dd->have_bbox) {
	    dd->g->s_size = dd->g->bmax[X] - dd->g->bmin[X];
	    V_MAX(dd->g->s_size, dd->g->bmax[Y] - dd->g->bmin[Y]);
	    V_MAX(dd->g->s_size, dd->g->bmax[Z] - dd->g->bmin[Z]);
	}
    } else {
	// If we're skipping subtractions there's no
	// point in going further.
	if (dd->skip_subtractions && dd->bool_op == 4) {
	    return;
	}

	// On initialization we don't have any wireframes to establish sizes, and if
	// we're adaptive we need some idea of object size to get a reasonable result.
	// Try for a bounding box, if the method is available.  Otherwise try the
	// bounding the default plot.
	point_t bmin, bmax;
	fastf_t s_size;
	int bbret = _fp_bbox(&s_size, &bmin, &bmax, path, dd->dbip, dd->ttol, dd->tol, curr_mat, dd->res, dd->v);
	if (bbret >= 0) {
	    // Got bounding box, use it to update sizing
	    (*dd->s_size)[dp] = s_size;
	    VMINMAX(dd->min, dd->max, bmin);
	    VMINMAX(dd->min, dd->max, bmax);
	    VMINMAX(dd->g->bmin, dd->g->bmax, bmin);
	    VMINMAX(dd->g->bmin, dd->g->bmax, bmax);
	    dd->g->s_size = s_size;
	    dd->have_bbox = 1;
	}
    }
}



static int
draw_opt_color(struct bu_vls *msg, size_t argc, const char **argv, void *data)
{
    struct bv_obj_settings *vs = (struct bv_obj_settings *)data;
    struct bu_color c;
    int ret = bu_opt_color(msg, argc, argv, (void *)&c);
    if (ret == 1 || ret == 3) {
	vs->color_override = 1;
	bu_color_to_rgb_chars(&c, vs->color);
    }
    return ret;
}

static int
alphanum_cmp(const void *a, const void *b, void *UNUSED(data)) {
    struct bv_scene_group *ga = *(struct bv_scene_group **)a;
    struct bv_scene_group *gb = *(struct bv_scene_group **)b;
    return alphanum_impl(bu_vls_cstr(&ga->s_name), bu_vls_cstr(&gb->s_name), NULL);
}

/* This function digests the paths into scene object sets.  It does NOT trigger
 * the routines that will actually produce the scene geometry and add it to the
 * scene objects - it only prepares the inputs to be used for that process. */
static int
ged_update_objs(struct ged *gedp, struct bview *v, struct bv_obj_settings *vs, int refresh, int argc, const char *argv[])
{
    struct db_i *dbip = gedp->dbip;
    struct bu_ptbl *sg = bv_view_objs(v, BV_DB_OBJS);
    struct resource *local_res;
    BU_GET(local_res, struct resource);
    rt_init_resource(local_res, 0, NULL);

    // If we have no active groups and no view objects, we are drawing into a
    // blank canvas - unless options specifically disable it, if we are doing
    // adaptive plotting we need to do a preliminary view characterization.
    struct draw_data_t bounds_data;
    std::map<struct directory *, fastf_t> s_size;
    bounds_data.bound_only = 1;
    bounds_data.res = local_res;
    bounds_data.have_bbox = 0;
    bounds_data.dbip = dbip;
    bounds_data.skip_subtractions = 1;
    bounds_data.bool_op = 2;
    bounds_data.v = v;
    VSET(bounds_data.min, INFINITY, INFINITY, INFINITY);
    VSET(bounds_data.max, -INFINITY, -INFINITY, -INFINITY);
    bounds_data.s_size = &s_size;

    /* Validate that the supplied args are current, valid paths in the
     * database.  If one or more are not, bail */
    std::set<struct db_full_path *> fps;
    std::set<struct db_full_path *>::iterator f_it;
    for (size_t i = 0; i < (size_t)argc; ++i) {
	struct db_full_path *fp;
	BU_GET(fp, struct db_full_path);
	db_full_path_init(fp);
	int ret = db_string_to_path(fp, dbip, argv[i]);
	if (ret < 0) {
	    // If that didn't work, there's one other thing we have to check
	    // for - a really strange path with the "/" character in it.
	    struct directory *fdp = db_lookup(dbip, argv[i], LOOKUP_QUIET);
	    if (fdp == RT_DIR_NULL) {
		// Invalid path
		db_free_full_path(fp);
		BU_PUT(fp, struct db_full_path);
		bu_vls_printf(gedp->ged_result_str, "Invalid path: %s\n", argv[i]);
		continue;
	    } else {
		// Object name contained forward slash (urk!)
		db_free_full_path(fp);
		db_full_path_init(fp);
		db_add_node_to_full_path(fp, fdp);
	    }
	}
	fps.insert(fp);
    }
    if (fps.size() != (size_t)argc) {
	for (f_it = fps.begin(); f_it != fps.end(); f_it++) {
	    struct db_full_path *fp = *f_it;
	    db_free_full_path(fp);
	    BU_PUT(fp, struct db_full_path);
	}
	return BRLCAD_ERROR;
    }
    // If we had no args, we're refreshing all drawn objects
    if (!argc) {
	for (size_t i = 0; i < BU_PTBL_LEN(sg); i++) {
	    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(sg, i);
	    struct db_full_path *fp;
	    BU_GET(fp, struct db_full_path);
	    db_full_path_init(fp);
	    int ret = db_string_to_path(fp, dbip, bu_vls_cstr(&s->s_name));
	    if (ret < 0) {
		// If that didn't work, there's one other thing we have to check
		// for - a really strange path with the "/" character in it.
		db_free_full_path(fp);
		struct directory *fdp = db_lookup(dbip, bu_vls_cstr(&s->s_name), LOOKUP_QUIET);
		if (fdp == RT_DIR_NULL) {
		    // Invalid path
		    db_free_full_path(fp);
		    BU_PUT(fp, struct db_full_path);
		    continue;
		} else {
		    // Object name contained forward slash (urk!)
		    db_free_full_path(fp);
		    db_full_path_init(fp);
		    db_add_node_to_full_path(fp, fdp);
		}
	    }
	    fps.insert(fp);
	}
    }

    // As a preliminary step, check the already drawn paths to see if the
    // proposed new path impacts them.  If we need to set up for adaptive
    // plotting, do initial bounds calculations to pave the way for an
    // initial view setup.
    std::map<struct db_full_path *, struct bv_scene_group *> fp_g;

    for (f_it = fps.begin(); f_it != fps.end(); f_it++) {
	struct bv_scene_group *g = NULL;
	struct db_full_path *fp = *f_it;

	// Get the "seed" matrix from the path - everything we draw at or below
	// the path will be positioned using it.  We don't need the matrix
	// itself in this stage, but if we can't get it the path isn't going to
	// be workable anyway so we may as well check now and yank it if there
	// is a problem.
	mat_t mat;
	MAT_IDN(mat);
	if (db_path_to_mat(dbip, fp, mat, 0, local_res)) {
	    db_free_full_path(fp);
	    BU_PUT(fp, struct db_full_path);
	    continue;
	}

	// Check all the current groups against the candidate.
	std::set<struct bv_scene_group *> clear;
	std::set<struct bv_scene_group *>::iterator g_it;
	struct bv_obj_settings fpvs;
	bv_obj_settings_sync(&fpvs, vs);
	bool clear_invalid_only = false;
	for (size_t i = 0; i < BU_PTBL_LEN(sg); i++) {
	    struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(sg, i);
	    // If we already know we're clearing this one, don't check
	    // again
	    if (clear.find(cg) != clear.end()) {
		continue;
	    }

	    // Not already clearing, need to check
	    struct db_full_path gfp;
	    db_full_path_init(&gfp);
	    int ret = db_string_to_path(&gfp, dbip, bu_vls_cstr(&cg->s_name));
	    if (ret < 0) {
		// If we can't get a db_fullpath, it's invalid
		clear.insert(cg);
		db_free_full_path(&gfp);
		continue;
	    }

	    // If we found an encompassing path, we don't need to do any more work
	    if (clear_invalid_only) {
		db_free_full_path(&gfp);
		continue;
	    }

	    // Two conditions to check for here:
	    // 1.  proposed draw path is a top match for existing path
	    if (db_full_path_match_top(fp, &gfp)) {
		// New path will replace the currently drawn path - clear it
		clear.insert(cg);
		db_free_full_path(&gfp);
		if (refresh)
		    bv_obj_settings_sync(&fpvs, &cg->s_os);
		continue;
	    }
	    // 2.  existing path is a top match encompassing the proposed path
	    if (db_full_path_match_top(&gfp, fp)) {
		// Already drawn - replace just the children of g that match this
		// path to update their contents
		g = cg;
		db_free_full_path(&gfp);
		if (refresh)
		    bv_obj_settings_sync(&fpvs, &cg->s_os);
		// We continue to weed out any other invalid paths in sg, even
		// though cg should be the only path in sg that matches in this
		// condition.  However, we no longer need to do the top matches,
		// so let the loop know
		clear_invalid_only = true;
		continue;
	    }

	    db_free_full_path(&gfp);
	}

	// IFF we are just redrawing part or all of an already drawn path, we don't
	// need to create a new scene object.  Otherwise, we do.
	if (g) {
	    // Remove children that match fp - we will be adding new versions
	    // to g to update them.  If g has no children, it was probably an
	    // evaluated shape - in that case, replace it with a fresh instance.
	    if (!BU_PTBL_LEN(&g->children)) {
		bv_obj_put(g);
		g = bv_obj_get(v, BV_DB_OBJS);
		db_path_to_vls(&g->s_name, fp);
		bv_obj_settings_sync(&g->s_os, &fpvs);
	    } else {
		std::set<struct bv_scene_obj *> sclear;
		std::set<struct bv_scene_obj *>::iterator s_it;
		for (size_t i = 0; i < BU_PTBL_LEN(&g->children); i++) {
		    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(&g->children, i);
		    struct db_full_path gfp;
		    db_full_path_init(&gfp);
		    db_string_to_path(&gfp, dbip, bu_vls_cstr(&s->s_name));
		    if (db_full_path_match_top(&gfp, fp)) {
			sclear.insert(s);
		    }
		}
		for (s_it = sclear.begin(); s_it != sclear.end(); s_it++) {
		    struct bv_scene_obj *s = *s_it;
		    bv_obj_put(s);
		}
	    }
	} else {
	    // Create new scene object.  Typically this will be a "parent"
	    // object and the actual per-solid wireframes or triangles will
	    // live in child objects below this object.  However, in
	    // "evaluated" drawing modes the visualization in the scene is
	    // unique to this object and in those cases drawing information
	    // will be stored in this object directly.
	    g = bv_obj_get(v, BV_DB_OBJS);
	    db_path_to_vls(&g->s_name, fp);
	    bv_obj_settings_sync(&g->s_os, &fpvs);
	}

	// If we're a blank slate, we're adaptive, and autoview isn't off
	// we need to be building up some sense of the view and object
	// sizes as we go, ahead of trying to draw anything.  That means
	// for each group we're going to be using mat and walking the tree
	// of fp (or getting its box directly if fp is a solid with no
	// parents) to build up bounding info.
	mat_t cmat;
	MAT_COPY(cmat, mat);
	VSETALL(g->bmin, INFINITY);
	VSETALL(g->bmax, -INFINITY);
	bounds_data.g = g;
	_bound_fp(fp, &cmat, (void *)&bounds_data);

	fp_g[fp] = g;

	if (clear.size()) {
	    // Clear anything superseded by the new path
	    for (g_it = clear.begin(); g_it != clear.end(); g_it++) {
		struct bv_scene_group *cg = *g_it;
		bv_obj_put(cg);
	    }
	}
    }

    // Initial setup is done, we now have the set of paths to walk to create
    // the children objects.
    std::map<struct db_full_path *, struct bv_scene_group *>::iterator fpg_it;
    for (fpg_it = fp_g.begin(); fpg_it != fp_g.end(); fpg_it++) {
	struct db_full_path *fp = fpg_it->first;
	struct bv_scene_group *g = fpg_it->second;

	// Seed initial matrix from the path
	mat_t mat;
	MAT_IDN(mat);
	if (db_path_to_mat(dbip, fp, mat, 0, local_res)) {
	    db_free_full_path(fp);
	    BU_PUT(fp, struct db_full_path);
	    continue;
	}

	// Get an initial color from the path, if we're not overridden
	struct bu_color c;
	if (!vs->color_override) {
	    unsigned char dc[3];
	    dc[0] = 255;
	    dc[1] = 0;
	    dc[2] = 0;
	    bu_color_from_rgb_chars(&c, dc);
	    db_full_path_color(&c, fp, dbip, local_res);
	}

	// Prepare tree walking data container
	struct draw_data_t dd;
	dd.dbip = gedp->dbip;
	dd.v = v;
	dd.tol = &gedp->ged_wdbp->wdb_tol;
	dd.ttol = &gedp->ged_wdbp->wdb_ttol;
	dd.mesh_c = gedp->ged_lod;
	dd.color_inherit = 0;
	dd.bound_only = 0;
	dd.res = local_res;
	dd.bool_op = 2; // Default to union
	if (vs->color_override) {
	    bu_color_from_rgb_chars(&dd.c, vs->color);
	} else {
	    HSET(dd.c.buc_rgb, c.buc_rgb[0], c.buc_rgb[1], c.buc_rgb[2], c.buc_rgb[3]);
	}
	dd.s_size = &s_size;
	dd.vs = vs;
	dd.g = g;

	// In drawing modes 3 (bigE) and 5 (points) we are producing an
	// evaluated shape, rather than iterating to get the solids
	if (vs->s_dmode == 3 || vs->s_dmode == 5) {
	    if (vs->color_override) {
		VMOVE(g->s_color, vs->color);
	    } else {
		bu_color_to_rgb_chars(&c, g->s_color);
	    }
	    // TODO - check object against default GED selection set
	    struct draw_update_data_t *ud;
	    BU_GET(ud, struct draw_update_data_t);
	    db_full_path_init(&ud->fp);
	    db_dup_full_path(&ud->fp, fp);
	    ud->dbip = dd.dbip;
	    ud->tol = dd.tol;
	    ud->ttol = dd.ttol;
	    ud->res = &rt_uniresource; // TODO - at some point this may be from the app or view... local_res is temporary, don't use it here
	    ud->mesh_c = dd.mesh_c;
	    g->s_i_data = (void *)ud;
	    g->s_v = dd.v;
	    // Done with path
	    db_free_full_path(fp);
	    BU_PUT(fp, struct db_full_path);
	    continue;
	}

	// Walk the tree to build up the set of scene objects that will hold
	// each instance's wireframe.  These scene objects will be stored as
	// children of g (which is the "who" level drawn object).
	draw_gather_paths(fp, &mat, (void *)&dd);

	// Done with path
	db_free_full_path(fp);
	BU_PUT(fp, struct db_full_path);
    }
    rt_clean_resource_basic(NULL, local_res);
    BU_PUT(local_res, struct resource);

    // Sort
    bu_sort(BU_PTBL_BASEADDR(sg), BU_PTBL_LEN(sg), sizeof(struct bv_scene_group *), alphanum_cmp, NULL);

    // Scene objects are created and stored. The next step is to generate
    // wireframes, triangles, etc. for each object based on current settings.
    // It is then the job of the dm to display the scene objects supplied by
    // the view.
    return BRLCAD_OK;
}

static int
ged_draw_view(struct bview *v, int bot_threshold, int no_autoview, int blank_slate)
{
    struct bu_ptbl *sg = bv_view_objs(v, BV_DB_OBJS);

    /* Bot threshold is managed as a per-view setting internally. */
    if (bot_threshold >= 0) {
	v->gv_s->bot_threshold = bot_threshold;
    }

    // Make sure the view knows how to update the obb
    v->gv_bounds_update = &bg_view_bounds;

    // Do an initial autoview so adaptive routines will have approximately
    // the right starting point
    if (blank_slate && !no_autoview) {
	point_t center, radial;
	point_t bmin, bmax;
	VSETALL(bmin, INFINITY);
	VSETALL(bmax, -INFINITY);
	if (BU_PTBL_LEN(sg)) {
	    for (size_t i = 0; i < BU_PTBL_LEN(sg); i++) {
		struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(sg, i);
		VMINMAX(bmin, bmax, s->bmin);
		VMINMAX(bmin, bmax, s->bmax);
	    }
	} else {
	    VSETALL(bmin, -1000);
	    VSETALL(bmax, 1000);
	}
	VADD2SCALE(center, bmax, bmin, 0.5);
	VSUB2(radial, bmax, center);
	vect_t sqrt_small;
	VSETALL(sqrt_small, SQRT_SMALL_FASTF);
	VMAX(radial, sqrt_small);
	if (VNEAR_ZERO(radial, SQRT_SMALL_FASTF))
	    VSETALL(radial, 1.0);
	MAT_IDN(v->gv_center);
	MAT_DELTAS_VEC_NEG(v->gv_center, center);
	v->gv_scale = radial[X];
	V_MAX(v->gv_scale, radial[Y]);
	V_MAX(v->gv_scale, radial[Z]);
	v->gv_isize = 1.0 / v->gv_size;
	bv_update(v);
    }


    // Do the actual drawing
    for (size_t i = 0; i < BU_PTBL_LEN(sg); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(sg, i);
	draw_scene(s, v);
    }

    // Make sure what we've drawn is visible, unless we've a reason not to.
    if (blank_slate && !no_autoview) {
	bv_autoview(v, BV_AUTOVIEW_SCALE_DEFAULT, 0);
    }

    // Scene objects are created and stored. The application may now call each
    // object's update callback to generate wireframes, triangles, etc. for
    // that object based on current settings.  It is then the job of the dm to
    // display the scene objects supplied by the view.
    return BRLCAD_OK;
}

/*
 *  Main drawing command control logic
 */
extern "C" int
ged_draw2_core(struct ged *gedp, int argc, const char *argv[])
{
    int print_help = 0;
    int bot_threshold = -1;
    int no_autoview = 0;
    static const char *usage = "[options] path1 [path2 ...]";
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);

    /* skip command name argv[0] */
    argc-=(argc>0); argv+=(argc>0);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Draw may operate on a specific user specified view.  If it does so,
     * we want the default settings to reflect those set in that particular
     * view.  In order to set up the correct default views, we need to know
     * if a specific view has in fact been specified.  We do a preliminary
     * option check to figure this out */
    struct bview *cv = gedp->ged_gvp;
    struct bu_vls cvls = BU_VLS_INIT_ZERO;
    struct bu_opt_desc vd[2];
    BU_OPT(vd[0],  "V", "view",    "name",      &bu_opt_vls, &cvls,   "specify view to draw on");
    BU_OPT_NULL(vd[1]);
    int opt_ret = bu_opt_parse(NULL, argc, argv, vd);
    argc = opt_ret;
    if (bu_vls_strlen(&cvls)) {
	cv = bv_set_find_view(&gedp->ged_views, bu_vls_cstr(&cvls));
	if (!cv) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s not found\n", bu_vls_cstr(&cvls));
	    bu_vls_free(&cvls);
	    return BRLCAD_ERROR;
	}

	if (!cv->independent) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s is not an independent view, and as such does not support specifying db objects for display in only this view.  To change the view's status, he command 'view independent %s 1' may be applied.\n", bu_vls_cstr(&cvls), bu_vls_cstr(&cvls));
	    bu_vls_free(&cvls);
	    return BRLCAD_ERROR;
	}
    }
    bu_vls_free(&cvls);

    // We need a current view, either from gedp or from the options
    if (!cv) {
	bu_vls_printf(gedp->ged_result_str, "No current GED view defined");
	return BRLCAD_ERROR;
    }

    /* User settings may override various options - set up to collect them.
     * Option defaults come from the current view, but may be overridden for
     * the purposes of the current draw command by command line options. */
    struct bv_obj_settings vs = BV_OBJ_SETTINGS_INIT;
    if (cv)
	bv_obj_settings_sync(&vs, &cv->gv_s->obj_s);


    int drawing_modes[6] = {-1, 0, 0, 0, 0, 0};
    int refresh = 0;
    struct bu_opt_desc d[18];
    BU_OPT(d[0],  "h", "help",          "",                 NULL, &print_help,         "Print help and exit");
    BU_OPT(d[1],  "?", "",              "",                 NULL, &print_help,         "");
    BU_OPT(d[2],  "m", "mode",         "#",          &bu_opt_int, &drawing_modes[0],  "0=wireframe;1=shaded bots;2=shaded;3=evaluated");
    BU_OPT(d[3],   "", "wireframe",     "",                 NULL, &drawing_modes[1],  "Draw using only wireframes (mode = 0)");
    BU_OPT(d[4],   "", "shaded",        "",                 NULL, &drawing_modes[2],  "Shade bots and polysolids (mode = 1)");
    BU_OPT(d[5],   "", "shaded-all",    "",                 NULL, &drawing_modes[3],  "Shade all solids, not evaluated (mode = 2)");
    BU_OPT(d[6],  "E", "evaluate",      "",                 NULL, &drawing_modes[4],  "Wireframe with evaluate booleans (mode = 3)");
    BU_OPT(d[7],   "", "hidden-line",   "",                 NULL, &drawing_modes[5],  "Hidden line wireframes");
    BU_OPT(d[8],  "t", "transparency", "#",      &bu_opt_fastf_t, &vs.transparency,   "Set transparency level in drawing: range 0 (clear) to 1 (opaque)");
    BU_OPT(d[9],  "x", "",             "#",      &bu_opt_fastf_t, &vs.transparency,   "");
    BU_OPT(d[10], "L", "",             "#",          &bu_opt_int, &bot_threshold,     "Set face count level for drawing bounding boxes instead of BoT triangles (NOTE: passing this updates the global view setting - bot_threshold is a view property).");
    BU_OPT(d[11], "S", "no-subtract",   "",                 NULL, &vs.draw_non_subtract_only,  "Do not draw subtraction solids");
    BU_OPT(d[12],  "", "no-dash",       "",                 NULL, &vs.draw_solid_lines_only,  "Use solid lines rather than dashed for subtraction solids");
    BU_OPT(d[13], "C", "color",         "r/g/b", &draw_opt_color, &vs,                "Override object colors");
    BU_OPT(d[14],  "", "line-width",   "#",          &bu_opt_int, &vs.s_line_width,   "Override default line width");
    BU_OPT(d[15], "R", "no-autoview",   "",                 NULL, &no_autoview,       "Do not calculate automatic view, even if initial scene is empty.");
    BU_OPT(d[16], "",  "refresh",       "",                 NULL, &refresh,       "Try to keep properties of existing drawn objects when updating.");
    BU_OPT_NULL(d[17]);

    /* If no args, must be wanting help */
    if (!argc) {
	_ged_cmd_help(gedp, usage, d);
	return BRLCAD_OK;
    }

    /* Process command line args into vs with bu_opt */
    struct bu_vls omsg = BU_VLS_INIT_ZERO;
    opt_ret = bu_opt_parse(&omsg, argc, argv, d);
    if (opt_ret < 0) {
	bu_vls_printf(gedp->ged_result_str, "option parsing error: %s\n", bu_vls_cstr(&omsg));
	bu_vls_free(&omsg);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&omsg);

    if (print_help) {
	_ged_cmd_help(gedp, usage, d);
	return BRLCAD_OK;
    }

    // Whatever is left after argument processing are the potential draw paths
    argc = opt_ret;

    // Drawing modes may be set either by -m or by the more verbose options,
    // with the latter taking precedence if both are set.
    int have_override = 0;
    for (int i = 1; i < 6; i++) {
	if (drawing_modes[i]) {
	    have_override++;
	}
    }
    if (have_override > 1 || (have_override &&  drawing_modes[0] > -1)) {
	bu_vls_printf(gedp->ged_result_str, "Multiple view modes specified\n");
	return BRLCAD_ERROR;
    }
    if (have_override) {
	for (int i = 1; i < 6; i++) {
	    if (drawing_modes[i]) {
		drawing_modes[0] = i - 1;
		break;
	    }
	}
    }
    if (drawing_modes[0] > -1) {
	vs.s_dmode = drawing_modes[0];
    }

    // Before we start doing anything with the object set, record if things are
    // starting out empty.
    int blank_slate = 0;
    struct bu_ptbl *vobjs = bv_view_objs(cv, BV_VIEW_OBJS);
    struct bu_ptbl *vlobjs = bv_view_objs(cv, BV_VIEW_OBJS | BV_LOCAL_OBJS);
    if (!BU_PTBL_LEN(bv_view_objs(cv, BV_DB_OBJS)) && !BU_PTBL_LEN(vobjs) && !BU_PTBL_LEN(vlobjs)) {
	blank_slate = 1;
    }

    // For the non-adaptive views, the object list is shared.  We process the
    // current list once to update the object set, but this step does not also
    // update the geometries of the objects.  Once we have the scene obj set,
    // we must process it on a per-view basis in case the objects have view
    // specific visualizations (such as in adaptive plotting.)
    ged_update_objs(gedp, cv, &vs, refresh, argc, argv);

    // Drawing can get complicated when we have multiple active views with
    // different settings. The simplest case is when the current or specified
    // view is an independent view - we just update it and return.
    if (cv->independent) {
	return ged_draw_view(cv, bot_threshold, no_autoview, blank_slate);
    }

    // If we have multiple views, we have to handle each view.  Most of the
    // time the work will be done in the first pass (when objects do not have
    // view specific geometry to generate) but this is not true when adaptive
    // plotting is enabled.
    struct bu_ptbl *views = bv_set_views(&gedp->ged_views);
    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	struct bview *v = (struct bview *)BU_PTBL_GET(views, i);
	if (v->independent) {
	    // Independent views are handled individually by the above case -
	    // this logic doesn't reference them.
	    continue;
	}
	ged_draw_view(v, bot_threshold, no_autoview, blank_slate);
    }

    return BRLCAD_OK;
}

static int
_ged_redraw_view(struct ged *gedp, struct bview *v, int argc, const char *argv[])
{
    if (!gedp || !v)
	return BRLCAD_ERROR;

    int ac = (v->independent) ? 5 : 3;
    const char *av[7] = {NULL};
    av[0] = "draw";
    av[1] = "-R";
    av[2] = "--refresh";
    av[3] = (v->independent) ? "--view" : NULL;
    av[4] = (v->independent) ? bu_vls_cstr(&v->gv_name) : NULL;
    int oind = (v->independent) ? 5 : 3;
    if (!argc) {
	struct bu_ptbl *sg = bv_view_objs(v, BV_DB_OBJS);
	for (size_t i = 0; i < BU_PTBL_LEN(sg); i++) {
	    struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(sg, i);
	    av[oind] = bu_vls_cstr(&cg->s_name);
	    ged_exec(gedp, ac, (const char **)av);
	}
	return BRLCAD_OK;
    } else {
	for (int i = 0; i < argc; i++) {
	    av[oind] = argv[i];
	    ged_exec(gedp, ac, (const char **)av);
	}
	return BRLCAD_OK;
    }
}

extern "C" int
ged_redraw2_core(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);
    RT_CHECK_DBI(gedp->dbip);

    argc--;argv++;

    bu_vls_trunc(gedp->ged_result_str, 0);

    /* redraw may operate on a specific user specified view, or on
     * all views (default) */
    struct bview *cv = NULL;
    struct bu_vls cvls = BU_VLS_INIT_ZERO;
    struct bu_opt_desc vd[2];
    BU_OPT(vd[0],  "V", "view",    "name",      &bu_opt_vls, &cvls,   "specify view to draw on");
    BU_OPT_NULL(vd[1]);
    int opt_ret = bu_opt_parse(NULL, argc, argv, vd);
    argc = opt_ret;
    if (bu_vls_strlen(&cvls)) {
	cv = bv_set_find_view(&gedp->ged_views, bu_vls_cstr(&cvls));
	if (!cv) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s not found\n", bu_vls_cstr(&cvls));
	    bu_vls_free(&cvls);
	    return BRLCAD_ERROR;
	}
    }
    bu_vls_free(&cvls);

    int ret = BRLCAD_OK;
    if (cv) {
	return _ged_redraw_view(gedp, cv, argc, argv);
    } else {
	struct bu_ptbl *views = bv_set_views(&gedp->ged_views);
	if (!BU_PTBL_LEN(views)) {
	    bu_vls_printf(gedp->ged_result_str, "No views defined\n");
	    return BRLCAD_OK;
	}
	for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	    struct bview *v = (struct bview *)BU_PTBL_GET(views, i);
	    if (!v) {
		bu_log("WARNING, draw2.cpp:%d - null view stored in ged_views index %zu, skipping\n", __LINE__, i);
		continue;
	    }
	    int nret = _ged_redraw_view(gedp, v, argc, argv);
	    if (nret != BRLCAD_OK)
		ret = nret;
	}
    }
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

