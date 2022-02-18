/*                         D R A W . C P P
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
/** @file draw.cpp
 *
 * Drawing routines that generate scene objects.
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
#include "./alphanum.h"
#include "./ged_private.h"

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

void
ged_scene_obj_geom(struct bv_scene_obj *s)
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
int
ged_update_db_path(struct bv_scene_obj *s, int UNUSED(flag))
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
	    (*s_c->s_update_callback)(s_c, 0);
    }

    // Clear out existing vlist, if any...
    BV_FREE_VLIST(&s->s_v->gv_vlfree, &s->s_vlist);

    // Get the new geometry
    ged_scene_obj_geom(s);

#if 0
    // Draw label
    if (ip->idb_meth->ft_labels)
	ip->idb_meth->ft_labels(&s->children, ip, s->s_v);
#endif

    return 1;
}

void
ged_free_draw_data(struct bv_scene_obj *s)
{
    /* Validate */
    if (!s)
	return;

    /* free drawing info */
    struct draw_update_data_t *d = (struct draw_update_data_t *)s->s_i_data;
    if (!d)
	return;
    db_free_full_path(&d->fp);
    BU_PUT(d, struct draw_update_data_t);
    s->s_i_data = NULL;
}

static void
_tree_color(struct directory *dp, struct draw_data_t *dd)
{
    struct bu_attribute_value_set c_avs = BU_AVS_INIT_ZERO;

    // Easy answer - if we're overridden, dd color is already set.
    if (dd->g->s_os.color_override)
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

void
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
void
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
	if (dd->g->s_os.draw_non_subtract_only && dd->bool_op == 4) {
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
	bv_obj_settings_sync(&s->s_os, &dd->g->s_os);
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
	s->s_update_callback = &ged_update_db_path;
	s->s_free_callback = &ged_free_draw_data;

	if (dd->s_size && dd->s_size->find(DB_FULL_PATH_CUR_DIR(path)) != dd->s_size->end()) {
	    s->s_size = (*dd->s_size)[DB_FULL_PATH_CUR_DIR(path)];
	}
	// Call correct vlist method based on mode
	ged_scene_obj_geom(s);

	// Add object to scene group
	bu_ptbl_ins(&dd->g->children, (long *)s);
    }
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
