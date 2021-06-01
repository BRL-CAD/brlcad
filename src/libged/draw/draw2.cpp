/*                         D R A W 2 . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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
#include "nmg.h"
#include "rt/view.h"

#include "ged/view/state.h"
#include "../alphanum.h"
#include "../ged_private.h"

#define GET_BV_SCENE_OBJ(p, fp) { \
        if (BU_LIST_IS_EMPTY(fp)) { \
            BU_ALLOC((p), struct bv_scene_obj); \
        } else { \
            p = BU_LIST_NEXT(bv_scene_obj, fp); \
            BU_LIST_DEQUEUE(&((p)->l)); \
        } \
        BU_LIST_INIT( &((p)->s_vlist) ); }

static int
_prim_tess(struct bv_scene_obj *s, struct rt_db_internal *ip)
{
    struct draw_update_data_t *d = (struct draw_update_data_t *)s->s_i_data;
    struct db_full_path *fp = &d->fp;
    const struct bn_tol *tol = d->tol;
    const struct bg_tess_tol *ttol = d->ttol;
    struct directory *dp = DB_FULL_PATH_CUR_DIR(fp);
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
    nmg_r_to_vlist(&s->s_vlist, r, NMG_VLIST_STYLE_POLYGON, &s->s_v->gv_vlfree);
    nmg_km(m);
    return 0;
}

/* Wrapper to handle adaptive vs non-adaptive wireframes */
static void
_wireframe_plot(struct bv_scene_obj *s, struct rt_db_internal *ip)
{
    struct draw_update_data_t *d = (struct draw_update_data_t *)s->s_i_data;
    const struct bn_tol *tol = d->tol;
    const struct bg_tess_tol *ttol = d->ttol;

    if (s->s_v->gv_s->adaptive_plot && ip->idb_meth->ft_adaptive_plot) {
	ip->idb_meth->ft_adaptive_plot(&s->s_vlist, ip, d->tol, s->s_v, s->s_size);
    } else if (ip->idb_meth->ft_plot) {
	ip->idb_meth->ft_plot(&s->s_vlist, ip, ttol, tol, s->s_v);
    }
}


extern "C" int draw_m3(struct bv_scene_obj *s);

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
	    if (bv_vlist_bbox(&vhead, bmin, bmax, NULL)) {
		BV_FREE_VLIST(&v->gv_vlfree, &vhead);
		rt_db_free_internal(&dbintern);
		return -1;
	    }
	    BV_FREE_VLIST(&v->gv_vlfree, &vhead);
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

static void
_scene_obj_geom(struct bv_scene_obj *s)
{
    struct draw_update_data_t *d = (struct draw_update_data_t *)s->s_i_data;
    struct db_i *dbip = d->dbip;
    struct db_full_path *fp = &d->fp;
    const struct bn_tol *tol = d->tol;
    const struct bg_tess_tol *ttol = d->ttol;

    /* Mode 3 is unique - it evaluates an object rather than visualizing
     * its solids */
    if (s->s_os.s_dmode == 3) {
	draw_m3(s);
	bv_scene_obj_bound(s);
	return;
    }

    struct rt_db_internal dbintern;
    RT_DB_INTERNAL_INIT(&dbintern);
    struct rt_db_internal *ip = &dbintern;
    int ret = rt_db_get_internal(ip, DB_FULL_PATH_CUR_DIR(fp), dbip, s->s_mat, d->res);
    if (ret < 0)
	return;

    // If we don't have a BRL-CAD type, see if we've got a plot routine
    if (ip->idb_major_type != DB5_MAJORTYPE_BRLCAD) {
	_wireframe_plot(s, ip);
	goto geom_done;
    }

    // At least for the moment, we don't try anything fancy with pipes
    if (ip->idb_minor_type == DB5_MINORTYPE_BRLCAD_PIPE) {
	_wireframe_plot(s, ip);
	goto geom_done;
    }

    // For anything other than mode 0, we call specific routines for
    // some of the primitives.
    if (s->s_os.s_dmode > 0) {
	switch (ip->idb_minor_type) {
	    case DB5_MINORTYPE_BRLCAD_BOT:
		(void)rt_bot_plot_poly(&s->s_vlist, ip, ttol, tol);
		goto geom_done;
		break;
	    case DB5_MINORTYPE_BRLCAD_POLY:
		(void)rt_pg_plot_poly(&s->s_vlist, ip, ttol, tol);
		goto geom_done;
		break;
	    case DB5_MINORTYPE_BRLCAD_BREP:
		(void)rt_brep_plot_poly(&s->s_vlist, fp, ip, ttol, tol, NULL);
		goto geom_done;
		break;
	    default:
		break;
	}
    }

    // Now the more general cases
    switch (s->s_os.s_dmode) {
	case 0:
	case 1:
	    // Get wireframe (for mode 1, all the non-wireframes are handled
	    // by the above BOT/POLY/BREP cases
	    _wireframe_plot(s, ip);
	    s->s_os.s_dmode = 0;
	    break;
	case 2:
	    // Shade everything except pipe, don't evaluate, fall
	    // back to wireframe in case of failure
	    if (_prim_tess(s, ip) < 0) {
		_wireframe_plot(s, ip);
		s->s_os.s_dmode = 0;
	    }
	    break;
	case 3:
	    // Shaded and evaluated
	    bu_log("Error - got too deep into _scene_obj_draw routine with drawing mode 3\n");
	    return;
	    break;
	case 4:
	    // Hidden line - generate polygonal forms, fall back to
	    // un-hidden wireframe in case of failure
	    if (_prim_tess(s, ip) < 0) {
		_wireframe_plot(s, ip);
		s->s_os.s_dmode = 0;
	    }
	    break;
	default:
	    // Default to wireframe
	    _wireframe_plot(s, ip);
	    break;
    }

geom_done:

    // Update s_size and s_center
    bv_scene_obj_bound(s);

    // Store current view info, in case of adaptive plotting
    s->adaptive_wireframe = s->s_v->gv_s->adaptive_plot;
    s->view_scale = s->s_v->gv_scale;
    s->bot_threshold= s->s_v->gv_s->bot_threshold;
    s->curve_scale = s->s_v->gv_s->curve_scale;
    s->point_scale = s->s_v->gv_s->point_scale;

    rt_db_free_internal(&dbintern);
}


// This is the generic "just create/update the view geometry" logic - for
// editing operations, which will have custom visuals and updating behavior,
// we'll need primitive specific callbacks (which really will almost certainly
// belong (like the labels logic) in the rt functab...)
//
// Note that this function shouldn't be called when doing tree walks.  It
// operates locally on the solid wireframe using s_mat, and won't take into
// account higher level hierarchy level changes.  If such higher level changes
// are made, the subtrees should be redrawn to properly repopulate the scene
// objects.
static int
_ged_update_db_path(struct bv_scene_obj *s)
{
    /* Validate */
    if (!s)
	return 0;

    // Normally this is a no-op, but there is one case where
    // we do potentially change.  If the view indicates adaptive
    // plotting, the current view scale does not match the
    // scale documented in s for its vlist generation, we need
    // a new vlist for the current conditions.

    bool rework = false;
    if (s->s_v->gv_s->adaptive_plot != s->adaptive_wireframe) {
	rework = true;
    }
    if (!rework && s->s_v->gv_s->adaptive_plot) {
	// Check bot threshold
	if (s->bot_threshold != s->s_v->gv_s->bot_threshold) {
	    rework = true;
	}
	// Check point scale
	if (!rework && !NEAR_EQUAL(s->curve_scale, s->s_v->gv_s->curve_scale, SMALL_FASTF)) {
	    rework = true;
	}
	// Check point scale
	if (!rework && !NEAR_EQUAL(s->point_scale, s->s_v->gv_s->point_scale, SMALL_FASTF)) {
	    rework = true;
	}
    }

    // If redraw-on-zoom is set, check the scale
    if (!rework && s->s_v->gv_s->adaptive_plot && s->s_v->gv_s->redraw_on_zoom) {
	// Check view scale
	fastf_t delta = s->view_scale * 0.1/s->view_scale;
	if (!rework && !NEAR_EQUAL(s->view_scale, s->s_v->gv_scale, delta)) {
	    rework = true;
	} 
    }

    if (!rework)
	return 0;

    // Process children - right now we have no view dependent child
    // drawing, but in principle we could...
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
        struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(&s->children, i);
	if (s_c->s_update_callback)
	    (*s_c->s_update_callback)(s_c);
    }

    // Clear out existing vlist, if any...
    BV_FREE_VLIST(&s->s_v->gv_vlfree, &s->s_vlist);

    // Get the new geometry
    _scene_obj_geom(s);

#if 0
    // Draw label
    if (ip->idb_meth->ft_labels)
	ip->idb_meth->ft_labels(&s->children, ip, s->s_v);
#endif

    return 1;
}

static void
_ged_free_draw_data(struct bv_scene_obj *s)
{
    /* Validate */
    if (!s)
	return;

    /* free drawing info */
    struct draw_update_data_t *d = (struct draw_update_data_t *)s->s_i_data;
    if (!d)
	return;
    db_full_path_init(&d->fp);
    BU_PUT(d, struct draw_update_data_t);
    s->s_i_data = NULL;
}


/* Data for tree walk */
struct draw_data_t {
    struct db_i *dbip;
    struct bv_scene_group *g;
    struct bview *v;
    struct bv_obj_settings *vs;
    const struct bn_tol *tol;
    const struct bg_tess_tol *ttol;
    struct bv_scene_obj *free_scene_obj;
    struct bu_color c;
    int color_inherit;
    int bool_op;
    struct resource *res;

    /* To avoid the need for multiple subtree walking
     * functions, we also set up to support a bounding
     * only walk. */
    int bound_only;
    int skip_subtractions;
    int have_bbox;
    point_t min;
    point_t max;
    std::map<struct directory *, fastf_t> *s_size;
};

static void
_tree_color(struct directory *dp, struct draw_data_t *dd)
{
    struct bu_attribute_value_set c_avs = BU_AVS_INIT_ZERO;

    // Easy answer - if we're overridden, dd color is already set.
    if (dd->g->g->s_os.color_override)
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
	bu_log("material head\n");
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

/*****************************************************************************

  The primary drawing subtree walk.

  To get an initial idea of scene size and center for adaptive plotting (i.e.
  before we have wireframes drawn) we also have a need to check ft_bbox ahead
  of the vlist generation.  It can be thought of as a "preliminary autoview"
  step.  That mode is also supported by this subtree walk.

******************************************************************************/

static void
db_fullpath_draw_subtree(struct db_full_path *path, union tree *tp, mat_t *curr_mat,
	void (*traverse_func) (struct db_full_path *path, mat_t *, void *),
	void *client_data)
{
    mat_t om, nm;
    struct directory *dp;
    struct draw_data_t *dd= (struct draw_data_t *)client_data;
    int prev_bool_op = dd->bool_op;

    if (!tp)
	return;

    RT_CK_FULL_PATH(path);
    RT_CHECK_DBI(dd->dbip);
    RT_CK_TREE(tp);

    switch (tp->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    if (tp->tr_op == OP_SUBTRACT)
		dd->bool_op = 4;
	    db_fullpath_draw_subtree(path, tp->tr_b.tb_right, curr_mat, traverse_func, client_data);
	    /* fall through */
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    dd->bool_op = prev_bool_op;
	    db_fullpath_draw_subtree(path, tp->tr_b.tb_left, curr_mat, traverse_func, client_data);
	    break;
	case OP_DB_LEAF:
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
		    _tree_color(dp, dd);
		}

		// Two things may prevent further processing - a hidden dp, or
		// a cyclic path.  Can check either here or in traverse_func -
		// just do it here since otherwise the logic would have to be
		// duplicated in all traverse functions.
		if (!(dp->d_flags & RT_DIR_HIDDEN)) {
		    db_add_node_to_full_path(path, dp);
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
static void
db_fullpath_draw(struct db_full_path *path, mat_t *curr_mat, void *client_data)
{
    struct directory *dp;
    struct draw_data_t *dd= (struct draw_data_t *)client_data;
    struct bv_scene_obj *free_scene_obj = dd->free_scene_obj;
    RT_CK_FULL_PATH(path);
    RT_CK_DBI(dd->dbip);

    dp = DB_FULL_PATH_CUR_DIR(path);
    if (!dp)
	return;
    if (dp->d_flags & RT_DIR_COMB) {

	struct rt_db_internal in;
	struct rt_comb_internal *comb;

	if (rt_db_get_internal(&in, dp, dd->dbip, NULL, &rt_uniresource) < 0)
	    return;

	comb = (struct rt_comb_internal *)in.idb_ptr;

	db_fullpath_draw_subtree(path, comb->tree, curr_mat, db_fullpath_draw, client_data);
	rt_db_free_internal(&in);

    } else {

	// If we're skipping subtractions there's no
	// point in going further.
	if (dd->g->g->s_os.draw_non_subtract_only && dd->bool_op == 4) {
	    return;
	}

	// If we've got a solid, things get interesting.  There are a lot of
	// potentially relevant options to sort through.  It may be that most
	// will end up getting handled by the object update callbacks, and the
	// job here will just be to set up the key data for later use...

	// There are two scenarios - one is we are creating a new scene object.
	// The other is we are applying new global view settings to existing
	// scene objects (i.e. re-drawing an object with shaded mode arguments
	// instead of wireframe.)  In the latter case we need to find the
	// existing view object and update it, in the former (where we can't
	// find it) we create it instead.

	// Have database object, make scene object
	struct bv_scene_obj *s;
	GET_BV_SCENE_OBJ(s, &free_scene_obj->l);
	bv_scene_obj_init(s, free_scene_obj);
	db_path_to_vls(&s->s_name, path);
	db_path_to_vls(&s->s_uuid, path);
	// TODO - append hash of matrix and op to uuid to make it properly unique...
	s->s_v = dd->v;
	MAT_COPY(s->s_mat, *curr_mat);
	bv_obj_settings_sync(&s->s_os, &dd->g->g->s_os);
	s->s_type_flags = BV_DBOBJ_BASED;
	s->s_changed++;
	if (!s->s_os.draw_solid_lines_only) {
	    s->s_soldash = (dd->bool_op == 4) ? 1 : 0;
	}
	bu_color_to_rgb_chars(&dd->c, s->s_color);

	// Stash the information needed for a draw update callback
	struct draw_update_data_t *ud;
	BU_GET(ud, struct draw_update_data_t);
	db_full_path_init(&ud->fp);
	db_dup_full_path(&ud->fp, path);
	ud->dbip = dd->dbip;
	ud->tol = dd->tol;
	ud->ttol = dd->ttol;
	ud->res = &rt_uniresource; // TODO - at some point this may be from the app or view...
	s->s_i_data = (void *)ud;

	// set up callback functions
	s->s_update_callback = &_ged_update_db_path;
	s->s_free_callback = &_ged_free_draw_data;

	if (dd->s_size && dd->s_size->find(DB_FULL_PATH_CUR_DIR(path)) != dd->s_size->end()) {
	    s->s_size = (*dd->s_size)[DB_FULL_PATH_CUR_DIR(path)];
	}
	// Call correct vlist method based on mode
	_scene_obj_geom(s);

	// Add object to scene group
	bu_ptbl_ins(&dd->g->g->children, (long *)s);
    }
}

/**
 * This walker builds up scene size and bounding information.
 */
static void
_bound_fp(struct db_full_path *path, mat_t *curr_mat, void *client_data)
{
    struct directory *dp;
    struct draw_data_t *dd= (struct draw_data_t *)client_data;
    RT_CK_FULL_PATH(path);
    RT_CK_DBI(dd->dbip);

    dp = DB_FULL_PATH_CUR_DIR(path);
    if (!dp)
	return;
    if (dp->d_flags & RT_DIR_COMB) {
	// Have a comb - keep going
	struct rt_db_internal in;
	struct rt_comb_internal *comb;
	if (rt_db_get_internal(&in, dp, dd->dbip, NULL, &rt_uniresource) < 0)
	    return;
	comb = (struct rt_comb_internal *)in.idb_ptr;
	db_fullpath_draw_subtree(path, comb->tree, curr_mat, _bound_fp, client_data);
	rt_db_free_internal(&in);
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
    return alphanum_impl(bu_vls_cstr(&ga->g->s_name), bu_vls_cstr(&gb->g->s_name), NULL);
}

// TODO - based on adaptive or non-adaptive plotting, we're either doing the general view objects or
// a view specific object.  Need to adjust the logic accordingly - right now it's assuming ged_gvp
// as the only view of interest.
static int
ged_draw_view(struct ged *gedp, struct bview *v, struct bv_obj_settings *vs, int argc, const char *argv[], int bot_threshold, int no_autoview)
{
    // Abbreviations for convenience
    struct db_i *dbip = gedp->ged_wdbp->dbip;
    struct bv_scene_obj *free_scene_obj = gedp->free_scene_obj;
    struct bu_ptbl *sg;

    // Where the groups get stored depends on our mode
    if (v->gv_s->adaptive_plot || v->independent) {
	sg = v->gv_view_grps;
    } else {
	sg = v->gv_db_grps;
    }

    // If we have no active groups and no view objects, we are drawing into a
    // blank canvas - unless options specifically disable it, if we are doing
    // adaptive plotting we need to do a preliminary view characterization.
    int blank_slate = 0;
    struct draw_data_t bounds_data;
    std::map<struct directory *, fastf_t> s_size;
    bounds_data.bound_only = 1;
    bounds_data.res = &rt_uniresource;
    bounds_data.have_bbox = 0;
    bounds_data.dbip = dbip;
    bounds_data.skip_subtractions = vs->draw_non_subtract_only;
    VSET(bounds_data.min, INFINITY, INFINITY, INFINITY);
    VSET(bounds_data.max, -INFINITY, -INFINITY, -INFINITY);
    bounds_data.s_size = &s_size;
    // Before we start doing anything with sg, record if things are starting
    // out empty.
    if (!BU_PTBL_LEN(sg) && !BU_PTBL_LEN(v->gv_view_objs)) {
	blank_slate = 1;
    }

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
	    // Invalid path
	    db_free_full_path(fp);
	    bu_vls_printf(gedp->ged_result_str, "Invalid path: %s\n", argv[i]);
	    continue;
	}
	fps.insert(fp);
    }
    if (fps.size() != (size_t)argc) {
	for (f_it = fps.begin(); f_it != fps.end(); f_it++) {
	    struct db_full_path *fp = *f_it;
	    db_free_full_path(fp);
	    BU_PUT(fp, struct db_full_path);
	}
	return GED_ERROR;
    }

    /* Bot threshold a per-view setting. */
    if (bot_threshold >= 0) {
	v->gv_s->bot_threshold = bot_threshold;
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
	// itself in this stage unless we're doing adaptive plotting, but if we
	// can't get it the path isn't going to be workable anyway so we may
	// as well check now and yank it if there is a problem.
	mat_t mat;
	MAT_IDN(mat);
	// TODO - get resource from somewhere other than rt_uniresource
	if (!db_path_to_mat(dbip, fp, mat, fp->fp_len-1, &rt_uniresource)) {
	    db_free_full_path(fp);
	    BU_PUT(fp, struct db_full_path);
	    continue;
	}

	// Check all the current groups against the candidate.
	std::set<struct bv_scene_group *> clear;
	std::set<struct bv_scene_group *>::iterator g_it;
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
	    int ret = db_string_to_path(&gfp, dbip, bu_vls_cstr(&cg->g->s_name));
	    if (ret < 0) {
		// If we can't get a db_fullpath, it's invalid
		clear.insert(cg);
		db_free_full_path(&gfp);
		continue;
	    }
	    // Two conditions to check for here:
	    // 1.  proposed draw path is a top match for existing path
	    if (db_full_path_match_top(fp, &gfp)) {
		// New path will replace this path - clear it
		clear.insert(cg);
		db_free_full_path(&gfp);
		continue;
	    }
	    // 2.  existing path is a top match encompassing the proposed path
	    if (db_full_path_match_top(&gfp, fp)) {
		// Already drawn - replace just the children of g that match this
		// path to update their contents
		g = cg;
		db_free_full_path(&gfp);
		// We continue to weed out any other invalid paths in sg, even
		// though cg should be the only path in sg that matches in this
		// condition...
		continue;
	    }
	}

	if (g) {
	    // remove children that match fp - we will be adding new versions to g to update it,
	    // rather than creating a new one.
	    std::set<struct bv_scene_obj *> sclear;
	    std::set<struct bv_scene_obj *>::iterator s_it;
	    for (size_t i = 0; i < BU_PTBL_LEN(&g->g->children); i++) {
		struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(&g->g->children, i);
		struct db_full_path gfp;
		db_full_path_init(&gfp);
		db_string_to_path(&gfp, dbip, bu_vls_cstr(&s->s_name));
		if (db_full_path_match_top(&gfp, fp)) {
		    sclear.insert(s);
		}
	    }
	    for (s_it = sclear.begin(); s_it != sclear.end(); s_it++) {
		struct bv_scene_obj *s = *s_it;
		bu_ptbl_rm(&g->g->children, (long *)s);
		bv_scene_obj_free(s, free_scene_obj);
	    }
	} else {
	    // Create new group
	    BU_GET(g, struct bv_scene_group);
	    GET_BV_SCENE_OBJ(g->g, &free_scene_obj->l);
	    bv_scene_obj_init(g->g, free_scene_obj);
	    db_path_to_vls(&g->g->s_name, fp);
	    db_path_to_vls(&g->g->s_uuid, fp);
	    bv_obj_settings_sync(&g->g->s_os, vs);
	    bu_ptbl_ins(sg, (long *)g);

	    // If we're a blank slate, we're adaptive, and autoview isn't off
	    // we need to be building up some sense of the view and object
	    // sizes as we go, ahead of trying to draw anything.  That means
	    // for each group we're going to be using mat and walking the tree
	    // of fp (or getting its box directly if fp is a solid with no
	    // parents) to build up bounding info.
	    if (blank_slate && v->gv_s->adaptive_plot && !no_autoview) {
		mat_t cmat;
		MAT_COPY(cmat, mat);
		_bound_fp(fp, &cmat, (void *)&bounds_data);
	    }
	}
	fp_g[fp] = g;

	if (clear.size()) {
	    // Clear anything superseded by the new path
	    for (g_it = clear.begin(); g_it != clear.end(); g_it++) {
		struct bv_scene_group *cg = *g_it;
		bu_ptbl_rm(sg, (long *)cg);
		bv_scene_obj_free(cg->g, free_scene_obj);
		BU_PUT(cg, struct bv_scene_group);
	    }
	}
    }

    if (blank_slate && v->gv_s->adaptive_plot && !no_autoview) {
	// We've checked the paths for bounding info - now set up the
	// view
	point_t center = VINIT_ZERO;
	point_t radial;
	if (bounds_data.have_bbox) {
	    VADD2SCALE(center, bounds_data.max, bounds_data.min, 0.5);
	    VSUB2(radial, bounds_data.max, center);
	} else {
	    // No bboxes at all??
	    VSETALL(radial, 1000.0);
	}
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

    // Initial setup is done, we now have the set of paths to walk to do the
    // actual scene population as well as (if necessary) our preliminary size
    // and autoview estimates.
    //
    // This stage is where the vector lists or triangles that constitute the view
    // information are actually created.
    std::map<struct db_full_path *, struct bv_scene_group *>::iterator fpg_it;
    for (fpg_it = fp_g.begin(); fpg_it != fp_g.end(); fpg_it++) {
	struct db_full_path *fp = fpg_it->first;
	struct bv_scene_group *g = fpg_it->second;

	// Seed initial matrix from the path
	mat_t mat;
	MAT_IDN(mat);
	// TODO - get resource from somewhere other than rt_uniresource
	if (!db_path_to_mat(dbip, fp, mat, fp->fp_len-1, &rt_uniresource)) {
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
	    db_full_path_color(&c, fp, dbip, &rt_uniresource);
	}

	// Prepare tree walking data container
	struct draw_data_t dd;
	dd.dbip = gedp->ged_wdbp->dbip;
	dd.v = v;
	dd.tol = &gedp->ged_wdbp->wdb_tol;
	dd.ttol = &gedp->ged_wdbp->wdb_ttol;
	dd.free_scene_obj = gedp->free_scene_obj;
	dd.color_inherit = 0;
	dd.bound_only = 0;
	dd.res = &rt_uniresource;
	dd.bool_op = 2; // Default to union
	if (vs->color_override) {
	    bu_color_from_rgb_chars(&dd.c, vs->color);
	} else {
	    HSET(dd.c.buc_rgb, c.buc_rgb[0], c.buc_rgb[1], c.buc_rgb[2], c.buc_rgb[3]);
	}
	dd.s_size = &s_size;
	dd.vs = vs;
	dd.g = g;

	// In drawing mode 3 (bigE) we are producing an evaluated shape,
	// rather than iterating to get the solids
	if (vs->s_dmode == 3) {
	    if (vs->color_override) {
		VMOVE(g->g->s_color, vs->color);
	    } else {
		bu_color_to_rgb_chars(&c, g->g->s_color);
	    }
	    struct draw_update_data_t *ud;
	    BU_GET(ud, struct draw_update_data_t);
	    db_full_path_init(&ud->fp);
	    db_dup_full_path(&ud->fp, fp);
	    ud->dbip = dd.dbip;
	    ud->tol = dd.tol;
	    ud->ttol = dd.ttol;
	    ud->res = dd.res;
	    g->g->s_i_data = (void *)ud;
	    g->g->s_update_callback = &_ged_update_db_path;
	    g->g->s_free_callback = &_ged_free_draw_data;
	    g->g->s_v = dd.v;
	    g->g->s_v->vlfree = &RTG.rtg_vlfree;

	    if (bounds_data.s_size && bounds_data.s_size->find(DB_FULL_PATH_CUR_DIR(fp)) != bounds_data.s_size->end()) {
		g->g->s_size = (*bounds_data.s_size)[DB_FULL_PATH_CUR_DIR(fp)];
	    }
	    _scene_obj_geom(g->g);

	    // Done with path
	    db_free_full_path(fp);
	    BU_PUT(fp, struct db_full_path);
	    continue;
	}
	// Walk the tree to build up the set of scene objects
	db_fullpath_draw(fp, &mat, (void *)&dd);

	// Done with path
	db_free_full_path(fp);
	BU_PUT(fp, struct db_full_path);
    }

    // Sort
    bu_sort(BU_PTBL_BASEADDR(sg), BU_PTBL_LEN(sg), sizeof(struct bv_scene_group *), alphanum_cmp, NULL);


    // If we're starting from scratch and we're not being told to leave the
    // view alone, make sure what we've drawn is visible.
    if (blank_slate && !no_autoview) {
	int ac = 1;
	const char *av[2];
	av[0] = "autoview";
	av[1] = NULL;
	return ged_exec(gedp, ac, (const char **)av);
    }

    // Scene objects are created and stored. The application may now call each
    // object's update callback to generate wireframes, triangles, etc. for
    // that object based on current settings.  It is then the job of the dm to
    // display the scene objects supplied by the view.
    return GED_OK;
}

/*
 * There are actually three scene object management scenarios we might want to
 * consider:
 *
 * 1.  All shared (default, most efficient from a vlist perspective, no
 * independence of view contents)
 *
 * 2.  Adaptive plot, synced group lists (different vlists for different views,
 * but drawn group lists should match.  From a user perspective, #1 with
 * adaptive drawing).  This is almost #3 with adaptive plot on, but with one key
 * difference - we don't want to have to specify the same objects for each view.
 * I.e. - we want the controls from #1.
 *
 * 3.  Independent view lists - each view has its own info, not synced with
 * other views.  Maximal flexibility, but also more memory usage than minimally
 * necessary if views do in fact have shared objects with geometrically
 * identical vlists.
 *
 * Theoretically we could also mix local and shared lists at the same time
 * (i.e. "overlay" local lists on top of a shared list.)  My inclination right
 * now is to not do that - the only gain would be to get some vlist storage
 * benefits from being able to share objects that would otherwise be duplicated
 * in multiple local lists.  That's a dubious use case in some ways, hard to track,
 * and the complexity/usability implications are significant.
 *
 */
extern "C" int
ged_draw2_core(struct ged *gedp, int argc, const char *argv[])
{
    int print_help = 0;
    int bot_threshold = -1;
    int no_autoview = 0;
    static const char *usage = "[options] path1 [path2 ...]";
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);

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
	int found_match = 0;
	for (size_t i = 0; i < BU_PTBL_LEN(&gedp->ged_views); i++) {
	    struct bview *tv = (struct bview *)BU_PTBL_GET(&gedp->ged_views, i);
	    if (BU_STR_EQUAL(bu_vls_cstr(&tv->gv_name), bu_vls_cstr(&cvls))) {
		cv = tv;
		found_match = 1;
		break;
	    }
	}
	if (!found_match) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s not found\n", bu_vls_cstr(&cvls));
	    bu_vls_free(&cvls);
	    return GED_ERROR;
	}

	if (!cv->independent) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s is not an independent view, and as such does not support specifying db objects for display in only this view.  To change the view's status, he command 'view independent %s 1' may be applied.\n", bu_vls_cstr(&cvls), bu_vls_cstr(&cvls));
	    bu_vls_free(&cvls);
	    return GED_ERROR;
	}
    }
    bu_vls_free(&cvls);

    /* User settings may override various options - set up to collect them.
     * Option defaults come from the current view, but may be overridden for
     * the purposes of the current draw command by command line options. */
    struct bv_obj_settings vs = BV_OBJ_SETTINGS_INIT;
    if (cv)
	bv_obj_settings_sync(&vs, &cv->gv_s->obj_s);


    int drawing_modes[6] = {-1, 0, 0, 0, 0, 0};
    struct bu_opt_desc d[16];
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
    BU_OPT(d[14], "R", "no-autoview",   "",                 NULL, &no_autoview,       "Do not calculate automatic view, even if initial scene is empty.");
    BU_OPT_NULL(d[15]);

    /* If no args, must be wanting help */
    if (!argc) {
	_ged_cmd_help(gedp, usage, d);
	return GED_OK;
    }

    /* Process command line args into vs with bu_opt */
    opt_ret = bu_opt_parse(NULL, argc, argv, d);

    if (print_help) {
	_ged_cmd_help(gedp, usage, d);
	return GED_OK;
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
	return GED_ERROR;
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

    // If we're independent, we only operate on one view
    if (cv && cv->independent) {
	return ged_draw_view(gedp, cv, &vs, argc, argv, bot_threshold, no_autoview);
    }

    /* If adaptive plotting is enabled, we need to generate wireframes
     * specific to each view */
    int have_non_adaptive = 0;
    int ret = GED_OK;
    for (size_t i = 0; i < BU_PTBL_LEN(&gedp->ged_views); i++) {
	struct bview *v = (struct bview *)BU_PTBL_GET(&gedp->ged_views, i);
	if (v->independent) {
	    // Independent views are handled by specifying the view - this
	    // logic doesn't change them.
	    continue;
	}
	if (v->gv_s->adaptive_plot) {
	    int aret = ged_draw_view(gedp, v, &vs, argc, argv, bot_threshold, no_autoview);
	    if (aret & GED_ERROR)
		ret = aret;
	} else {
	    have_non_adaptive = 1;
	    // If we are switching from local adaptive geometry to shared
	    // non-adaptive geometry and we have both a shared and a local
	    // ptbl, we need to clear any local adaptive plotting that may have
	    // been produced by previous draw calls.
	    struct bu_ptbl *sg = v->gv_view_grps;
	    if (sg && sg != v->gv_db_grps) {
		for (size_t j = 0; j < BU_PTBL_LEN(sg); j++) {
		    struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(sg, j);
		    bv_scene_obj_free(cg->g, gedp->free_scene_obj);
		    BU_PUT(cg, struct bv_scene_group);
		}
		bu_ptbl_reset(sg);
	    }
	}
    }

    if (have_non_adaptive) {
	if (!cv) {
	    bu_vls_printf(gedp->ged_result_str, "No current GED view defined");
	    return GED_ERROR;
	}
	int nret = ged_draw_view(gedp, cv, &vs, argc, argv, bot_threshold, no_autoview);
	if (nret & GED_ERROR)
	    ret = nret;
    }

    return ret;

}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
