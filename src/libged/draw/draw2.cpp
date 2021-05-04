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

#include "ged/view/state.h"
#include "../alphanum.h"
#include "../ged_private.h"

#define GET_BVIEW_SCENE_OBJ(p, fp) { \
        if (BU_LIST_IS_EMPTY(fp)) { \
            BU_ALLOC((p), struct bview_scene_obj); \
        } else { \
            p = BU_LIST_NEXT(bview_scene_obj, fp); \
            BU_LIST_DEQUEUE(&((p)->l)); \
        } \
        BU_LIST_INIT( &((p)->s_vlist) ); }

#define FREE_BVIEW_SCENE_OBJ(p, fp) { \
        BU_LIST_APPEND(fp, &((p)->l)); }

// This is the generic "just create/update the view geometry" logic - for
// editing operations, which will have custom visuals and updating behavior,
// we'll need primitive specific callbacks (which really will almost certainly
// belong (like the labels logic) in the rt functab...)
//
// Note that this function shouldn't be called when doing tree walks, since
// it does extra work to determine the current matrix and the tree walk will
// already be tracking that.
static int
_ged_update_db_path(struct bview_scene_obj *s)
{
    /* Validate */
    if (!s)
	return 0;

    /* Set up drawing info */
    struct draw_update_data_t *d = (struct draw_update_data_t *)s->s_i_data;
    struct db_i *dbip = d->dbip;
    struct db_full_path *fp = &d->fp;
    const struct bn_tol *tol = d->tol;
    const struct bg_tess_tol *ttol = d->ttol;

    // Clear out existing vlist, if there is one...
    BN_FREE_VLIST(&s->s_v->gv_vlfree, &s->s_vlist);
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
        struct bview_scene_obj *s_c = (struct bview_scene_obj *)BU_PTBL_GET(&s->children, i);
        BN_FREE_VLIST(&s->s_v->gv_vlfree, &s_c->s_vlist);
        // TODO - free child bview_scene_obj itself (ptbls, etc.)
    }

    // Get the matrix
    mat_t mat;
    MAT_IDN(mat);
    if (!db_path_to_mat(dbip, fp, mat, fp->fp_len-1, d->res)) {
	return 0;
    }

    // Make sure we can get the internal form
    struct rt_db_internal dbintern;
    struct rt_db_internal *ip = &dbintern;
    int ret = rt_db_get_internal(ip, DB_FULL_PATH_CUR_DIR(fp), dbip, mat, d->res);
    if (ret < 0 || !ip->idb_meth->ft_plot)
	return 0;

    // TODO - call correct vlist method based on mode
    // Get wireframe
    struct rt_view_info info;
    info.bot_threshold = s->s_v->gvs.bot_threshold;
    ret = ip->idb_meth->ft_plot(&s->s_vlist, ip, ttol, tol, &info);

    // TODO - confirm color

    // Update s_size and s_center
    bview_scene_obj_bound(s);

#if 0
    // Draw label
    if (ip->idb_meth->ft_labels)
	ip->idb_meth->ft_labels(&s->children, ip, s->s_v);
#endif

    return 1;
}

static void
_ged_free_draw_data(struct bview_scene_obj *s)
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
    struct bview_scene_group *g;
    struct bview *v;
    struct bview_settings *vs;
    const struct bn_tol *tol;
    const struct bg_tess_tol *ttol;
    struct bview_scene_obj *free_scene_obj;
    struct bu_color c;
    int color_inherit;
};

static void
_tree_color(struct directory *dp, struct draw_data_t *dd)
{
    struct bu_attribute_value_set c_avs;

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

static void
db_fullpath_draw_subtree(struct db_full_path *path, int curr_bool, union tree *tp, mat_t *curr_mat,
	void (*traverse_func) (struct db_full_path *path, mat_t *, void *),
	void *client_data)
{
    mat_t om, nm;
    struct directory *dp;
    struct draw_data_t *dd= (struct draw_data_t *)client_data;
    int bool_val = curr_bool;

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
	    if (tp->tr_op == OP_UNION)
		bool_val = 2;
	    if (tp->tr_op == OP_INTERSECT)
		bool_val = 3;
	    if (tp->tr_op == OP_SUBTRACT) {
		bool_val = 4;
		// TODO - check if we're skipping subtractions.  If we are,
		// there's no point in going further.
	    }
	    db_fullpath_draw_subtree(path, bool_val, tp->tr_b.tb_right, curr_mat, traverse_func, client_data);
	    /* fall through */
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    db_fullpath_draw_subtree(path, OP_UNION, tp->tr_b.tb_left, curr_mat, traverse_func, client_data);
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
		int inherit_old = dd->color_inherit;
		struct bu_color oc;
		HSET(oc.buc_rgb, dd->c.buc_rgb[0], dd->c.buc_rgb[1], dd->c.buc_rgb[2], dd->c.buc_rgb[3]);
		_tree_color(dp, dd);

		// Two things may prevent further processing - a hidden dp, or
		// a cyclic path.  Can check either here or in traverse_func -
		// just do it here since otherwise the logic would have to be
		// duplicated in all traverse functions.
		if (!(dp->d_flags & RT_DIR_HIDDEN)) {
		    db_add_node_to_full_path(path, dp);
		    DB_FULL_PATH_SET_CUR_BOOL(path, bool_val);
		    if (!db_full_path_cyclic(path, NULL, 0)) {
			/* Keep going */
			traverse_func(path, curr_mat, client_data);
		    }
		}

		/* Done with branch - restore path, put back the old matrix state,
		 * and restore previous color settings */
		DB_FULL_PATH_POP(path);
		MAT_COPY(*curr_mat, om);
		dd->color_inherit = inherit_old;
		HSET(dd->c.buc_rgb, oc.buc_rgb[0], oc.buc_rgb[1], oc.buc_rgb[2], oc.buc_rgb[3]);

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
    struct bview_scene_obj *free_scene_obj = dd->free_scene_obj;
    RT_CK_FULL_PATH(path);
    RT_CK_DBI(dd->dbip);

    dp = DB_FULL_PATH_CUR_DIR(path);
    if (!dp)
	return;
    if (dp->d_flags & RT_DIR_COMB) {

	// If we have a comb, there are a couple possible actions based
	// on draw settings.  If we're supposed to generate an evaluated
	// shape, we need to check for a region flag and if we have one
	// do the evaluation to make the scene object.  In most other
	// cases, continue down the tree looking for solids.

	struct rt_db_internal in;
	struct rt_comb_internal *comb;

	if (rt_db_get_internal(&in, dp, dd->dbip, NULL, &rt_uniresource) < 0)
	    return;

	comb = (struct rt_comb_internal *)in.idb_ptr;

	db_fullpath_draw_subtree(path, OP_UNION, comb->tree, curr_mat, db_fullpath_draw, client_data);
	rt_db_free_internal(&in);
    } else {
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
	struct bview_scene_obj *s;
	GET_BVIEW_SCENE_OBJ(s, &free_scene_obj->l);
	s->free_scene_obj = free_scene_obj;
	BU_LIST_INIT(&(s->s_vlist));
	BU_PTBL_INIT(&s->children);
	BU_VLS_INIT(&s->s_name);
	db_path_to_vls(&s->s_name, path);
	BU_VLS_INIT(&s->s_uuid);
	db_path_to_vls(&s->s_uuid, path);
	// TODO - append hash of matrix and op to uuid to make it properly unique...
	s->s_v = dd->v;
	bview_settings_sync(&s->s_os, &dd->g->g->s_os);
	s->s_type_flags |= BVIEW_DBOBJ_BASED;

	int rgb[3];
	bu_color_to_rgb_ints(&dd->c, &rgb[0], &rgb[1], &rgb[2]);
	VMOVE(s->s_color, rgb);

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

	// Create the initial data
	struct rt_db_internal dbintern;
	struct rt_db_internal *ip = &dbintern;
	int ret = rt_db_get_internal(ip, DB_FULL_PATH_CUR_DIR(&ud->fp), ud->dbip, *curr_mat, ud->res);
	if (ret < 0 || !ip->idb_meth->ft_plot)
	    return;

	// TODO - call correct vlist method based on mode
	// Get wireframe
	struct rt_view_info info;
	info.bot_threshold = s->s_v->gvs.bot_threshold;
	ip->idb_meth->ft_plot(&s->s_vlist, ip, ud->ttol, ud->tol, &info);

	// Update s_size and s_center
	bview_scene_obj_bound(s);

	// Add object to scene group
	bu_ptbl_ins(&dd->g->g->children, (long *)s);
    }
}

static int
draw_opt_color(struct bu_vls *msg, size_t argc, const char **argv, void *data)
{
    struct bview_settings *vs = (struct bview_settings *)data;
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
    struct bview_scene_group *ga = *(struct bview_scene_group **)a;
    struct bview_scene_group *gb = *(struct bview_scene_group **)b;
    return alphanum_impl(bu_vls_cstr(&ga->g->s_name), bu_vls_cstr(&gb->g->s_name), NULL);
}

extern "C" int
ged_draw2_core(struct ged *gedp, int argc, const char *argv[])
{
    int print_help = 0;
    static const char *usage = "[options] path1 [path2 ...]";
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);

    /* skip command name argv[0] */
    argc-=(argc>0); argv+=(argc>0);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* If we have no view, we have no way to draw */
    if (!gedp->ged_gvp) {
	bu_vls_printf(gedp->ged_result_str, "No current GED view defined");
	return GED_ERROR;
    }

    struct bview_settings vs;
    int drawing_modes[5] = {-1, 0, 0, 0, 0};
    struct bu_opt_desc d[15];
    BU_OPT(d[0],  "h", "help",          "",                 NULL,       &print_help,  "Print help and exit");
    BU_OPT(d[1],  "?", "",              "",                 NULL,       &print_help,  "");
    BU_OPT(d[2],  "m", "mode",         "#",          &bu_opt_int, &drawing_modes[0],  "0=wireframe;1=shaded bots;2=shaded;3=evaluated");
    BU_OPT(d[3],   "", "wireframe",     "",                 NULL, &drawing_modes[1],  "Draw using only wireframes (mode = 0)");
    BU_OPT(d[4],   "", "shaded",        "",                 NULL, &drawing_modes[2],  "Shade bots and polysolids (mode = 1)");
    BU_OPT(d[5],   "", "shaded-all",    "",                 NULL, &drawing_modes[3],  "Shade all solids, not evaluated (mode = 2)");
    BU_OPT(d[6],  "E", "evaluate",      "",                 NULL, &drawing_modes[4],  "Shade and evaluate booleans (mode = 3)");
    BU_OPT(d[7],  "t", "transparency", "#",      &bu_opt_fastf_t,  &vs.transparency,  "Set transparency level in drawing");
    BU_OPT(d[8],   "", "hidden-line",   "",                 NULL,  &vs.s_hiddenLine,  "Draw using hidden lines (TODO - is this a drawing mode?)");
    BU_OPT(d[9],  "L", "lod",           "",                 NULL, &vs.adaptive_plot,  "Enable view adaptive Level of Detail plotting");
    BU_OPT(d[10],  "", "adaptive",      "",                 NULL, &vs.adaptive_plot,  "");
    BU_OPT(d[11], "S", "no-subtract",   "",                 NULL, &vs.draw_non_subtract_only,  "Do not draw subtraction solids");
    BU_OPT(d[12], "C", "color",         "r/g/b", &draw_opt_color, &vs,                 "Override object colors");
    BU_OPT(d[13],  "", "line-width",   "#",          &bu_opt_int, &vs.s_line_width,   "Override default line width");
    BU_OPT_NULL(d[14]);

    /* If no args, must be wanting help */
    if (!argc) {
	_ged_cmd_help(gedp, usage, d);
	return GED_OK;
    }

    /* Option defaults come from the current view, but may be overridden for
     * the purposes of the current draw command by command line options. */
    bview_settings_sync(&vs, &gedp->ged_gvp->gvs);

    /* Process command line args into vs with bu_opt */
    int opt_ret = bu_opt_parse(NULL, argc, argv, d);

    if (print_help) {
	_ged_cmd_help(gedp, usage, d);
	return GED_OK;
    }

    // Drawing modes may be set either by -m or by the more verbose options,
    // with the latter taking precedence if both are set.
    int have_override = 0;
    for (int i = 1; i < 5; i++) {
	if (drawing_modes[i]) {
	    have_override++;
	}
    }
    if (have_override > 1 || (have_override &&  drawing_modes[0] > -1)) {
	bu_vls_printf(gedp->ged_result_str, "Multiple view modes specified\n");
	return GED_ERROR;
    }
    if (have_override) {
	for (int i = 1; i < 5; i++) {
	    if (drawing_modes[i]) {
		drawing_modes[0] = i - 1;
		break;
	    }
	}
    }
    if (drawing_modes[0] > -1) {
	vs.s_dmode = drawing_modes[0];
    }


    // Abbreviations for convenience
    struct db_i *dbip = gedp->ged_wdbp->dbip;
    struct bu_ptbl *sg = gedp->ged_gvp->gv_db_grps;
    struct bview_scene_obj *free_scene_obj = gedp->free_scene_obj;

    // Whatever is left after argument processing are the potential draw paths
    argc = opt_ret;

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

    // Check the already drawn paths to see if the proposed new path impacts them.
    for (f_it = fps.begin(); f_it != fps.end(); f_it++) {
	struct bview_scene_group *g = NULL;
	struct db_full_path *fp = *f_it;

	// Get the "seed" matrix from the path - everything we draw at or below the path
	// will be positioned using it
	mat_t mat;
	MAT_IDN(mat);
	// TODO - get resource from somewhere other than rt_uniresource
	if (!db_path_to_mat(dbip, fp, mat, fp->fp_len-1, &rt_uniresource)) {
	    db_free_full_path(fp);
	    BU_PUT(fp, struct db_full_path);
	    continue;
	}

	std::set<struct bview_scene_group *> clear;
	std::set<struct bview_scene_group *>::iterator g_it;
	for (size_t i = 0; i < BU_PTBL_LEN(sg); i++) {
	    struct bview_scene_group *cg = (struct bview_scene_group *)BU_PTBL_GET(sg, i);
	    struct db_full_path gfp;
	    db_full_path_init(&gfp);
	    int ret = db_string_to_path(&gfp, dbip, bu_vls_cstr(&cg->g->s_name));
	    if (ret < 0) {
		// If this fails eliminate the invalid path from sg...
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
		continue;
	    }
	}

	if (g) {
	    // remove children that match fp - we will be adding new versions to g to update it,
	    // rather than creating a new one.
	    std::set<struct bview_scene_obj *> sclear;
	    std::set<struct bview_scene_obj *>::iterator s_it;
	    for (size_t i = 0; i < BU_PTBL_LEN(&g->g->children); i++) {
		struct bview_scene_obj *s = (struct bview_scene_obj *)BU_PTBL_GET(&g->g->children, i);
		struct db_full_path gfp;
		db_full_path_init(&gfp);
		db_string_to_path(&gfp, dbip, bu_vls_cstr(&s->s_name));
		if (db_full_path_match_top(&gfp, fp)) {
		    sclear.insert(s);
		}
	    }
	    for (s_it = sclear.begin(); s_it != sclear.end(); s_it++) {
		struct bview_scene_obj *s = *s_it;
		bu_ptbl_rm(&g->g->children, (long *)s);
		bview_scene_obj_free(s, free_scene_obj);
		FREE_BVIEW_SCENE_OBJ(s, &free_scene_obj->l);
	    }
	} else {
	    // Create new group
	    BU_GET(g, struct bview_scene_group);
	    GET_BVIEW_SCENE_OBJ(g->g, &free_scene_obj->l);
	    g->g->free_scene_obj = free_scene_obj;
	    BU_LIST_INIT(&(g->g->s_vlist));
	    BU_PTBL_INIT(&g->g->children);
	    BU_VLS_INIT(&g->g->s_name);
	    db_path_to_vls(&g->g->s_name, fp);
	    BU_VLS_INIT(&g->g->s_uuid);
	    db_path_to_vls(&g->g->s_uuid, fp);
	    bview_settings_sync(&g->g->s_os, &vs);
	    bu_ptbl_ins(gedp->ged_gvp->gv_db_grps, (long *)g);
	}

	if (clear.size()) {
	    // Clear anything superseded by the new path
	    for (g_it = clear.begin(); g_it != clear.end(); g_it++) {
		struct bview_scene_group *cg = *g_it;
		bu_ptbl_rm(gedp->ged_gvp->gv_db_grps, (long *)cg);
		bview_scene_obj_free(cg->g, free_scene_obj);
		FREE_BVIEW_SCENE_OBJ(cg->g, &free_scene_obj->l);
		BU_PUT(cg, struct bview_scene_group);
	    }
	}


	// Prepare tree walking data container
	struct draw_data_t dd;
	dd.dbip = gedp->ged_wdbp->dbip;
	dd.v = gedp->ged_gvp;
	dd.tol = &gedp->ged_wdbp->wdb_tol;
	dd.ttol = &gedp->ged_wdbp->wdb_ttol;
	dd.free_scene_obj = gedp->free_scene_obj;
	dd.color_inherit = 0;
	if (vs.color_override) {
	    bu_color_from_rgb_chars(&dd.c, vs.color);
	} else {
	    unsigned char dc[3];
	    dc[0] = 255;
	    dc[1] = 0;
	    dc[2] = 0;
	    bu_color_from_rgb_chars(&dd.c, dc);
	}
	dd.vs = &vs;
	dd.g = g;

	// Walk the tree to build up the set of scene objects
	db_fullpath_draw(fp, &mat, (void *)&dd);

	// Done with path
	db_free_full_path(fp);
	BU_PUT(fp, struct db_full_path);
    }

    // Sort
    bu_sort(BU_PTBL_BASEADDR(gedp->ged_gvp->gv_db_grps), BU_PTBL_LEN(gedp->ged_gvp->gv_db_grps), sizeof(struct bview_scene_group *), alphanum_cmp, NULL);

    // Scene objects are created and stored in gv_db_grps. The application
    // may now call each object's update callback to generate wireframes,
    // triangles, etc. for that object based on current settings.  It is then
    // the job of the dm to display the scene objects supplied by the view.
    return GED_OK;
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
