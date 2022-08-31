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

extern "C" {
#define XXH_STATIC_LINKING_ONLY
#include "xxhash.h"
}

#include "ged/view/state.h"
#define ALPHANUM_IMPL
#include "../alphanum.h"
#include "../ged_private.h"

#if 0
static int
alphanum_cmp(const void *a, const void *b, void *UNUSED(data)) {
    struct bv_scene_group *ga = *(struct bv_scene_group **)a;
    struct bv_scene_group *gb = *(struct bv_scene_group **)b;
    return alphanum_impl(bu_vls_cstr(&ga->s_name), bu_vls_cstr(&gb->s_name), NULL);
}
#endif

// This version of the cyclic check assumes the path entries other than the
// last one are OK, and checks only against that last entry.
static bool
path_addition_cyclic(std::vector<unsigned long long> &path)
{
    if (path.size() == 1)
	return false;
    int new_entry = path.size() - 1;
    int i = new_entry - 1;
    while (i >= 0) {
	if (path[new_entry] == path[i])
	    return true;
	i--;
    }
    return false;
}

struct dd_t {
    struct ged *gedp;
    struct bview *v;
    struct bv_obj_settings *vs;
    matp_t m;
    int op;
    int subtract_skip;
    bool transparency_set;
    std::vector<unsigned long long> path_hashes;
};

static struct bv_scene_obj *
init_scene_obj(struct dd_t *dd)
{
    // Solid - scene object time
    unsigned long long phash = dd->gedp->dbi_state->path_hash(dd->path_hashes, 0);
    struct bv_scene_obj *sp = bv_obj_get(dd->v, BV_DB_OBJS);

    std::unordered_map<unsigned long long, struct directory *>::iterator d_it;
    d_it = dd->gedp->dbi_state->d_map.find(dd->path_hashes[dd->path_hashes.size()-1]);
    if (d_it == dd->gedp->dbi_state->d_map.end()) {
	std::unordered_map<unsigned long long, unsigned long long>::iterator m_it;
	m_it = dd->gedp->dbi_state->i_map.find(dd->path_hashes[dd->path_hashes.size()-1]);
	if (m_it != dd->gedp->dbi_state->i_map.end())
	    d_it = dd->gedp->dbi_state->d_map.find(m_it->second);
    }

    struct draw_update_data_t *ud;
    BU_GET(ud, struct draw_update_data_t);
    ud->dbip = dd->gedp->dbip;
    ud->tol = &dd->gedp->ged_wdbp->wdb_tol;
    ud->ttol = &dd->gedp->ged_wdbp->wdb_ttol;
    ud->res = &rt_uniresource; // TODO - at some point this may be from the app or view... local_res is temporary, don't use it here
    ud->mesh_c = dd->gedp->ged_lod;
    sp->dp = d_it->second;
    sp->s_i_data = (void *)ud;

    // Get color from path, unless we're overridden
    struct bu_color c;
    dd->gedp->dbi_state->path_color(&c, dd->path_hashes);
    bu_color_to_rgb_chars(&c, sp->s_color);
    if (dd->vs->color_override) {
	sp->s_color[0] = dd->vs->color[0];
	sp->s_color[1] = dd->vs->color[1];
	sp->s_color[2] = dd->vs->color[2];
    }

    // Tell scene object what the current matrix is
    MAT_COPY(sp->s_mat, dd->m);

    // Assign the bounding box
    dd->gedp->dbi_state->get_path_bbox(&sp->bmin, &sp->bmax, dd->path_hashes);

    // If we're drawing a subtraction and we're not overridden, set the
    // appropriate flag for dashed line drawing
    if (!dd->vs->draw_solid_lines_only)
	sp->s_soldash = (dd->op == OP_SUBTRACT) ? 1 : 0;

    // Set line width, if the user specified a non-default value
    if (dd->vs->s_line_width)
	sp->s_os->s_line_width = dd->vs->s_line_width;

    // Set transparency, if the user set a non-default value
    if (dd->transparency_set)
	sp->s_os->transparency = dd->vs->transparency;

    dd->gedp->dbi_state->print_path(&sp->s_name, dd->path_hashes);
    BViewState *bvs = dd->gedp->dbi_state->get_view_state(dd->v);
    bu_log("make solid %s\n", bu_vls_cstr(&sp->s_name));
    bvs->s_map[phash] = sp;
    bvs->s_keys[phash] = dd->path_hashes;

    return sp;
}

static void
draw_walk_tree(unsigned long long chash, void *d, int subtract_skip,
	void (*leaf_func)(void *, unsigned long long, matp_t, int)
	)
{
    struct dd_t *dd = (struct dd_t *)d;

    size_t op = OP_UNION;
    std::unordered_map<unsigned long long, std::unordered_map<unsigned long long, size_t>>::iterator b_it;
    b_it = dd->gedp->dbi_state->i_bool.find(dd->path_hashes[dd->path_hashes.size() - 1]);
    if (b_it != dd->gedp->dbi_state->i_bool.end()) {
	std::unordered_map<unsigned long long, size_t>::iterator bb_it;
	bb_it = b_it->second.find(chash);
	if (bb_it != b_it->second.end()) {
	    op = bb_it->second;
	}
    }

    if (op == OP_SUBTRACT)
	dd->op = op;

    if (op == OP_SUBTRACT && subtract_skip)
	return;

    matp_t mp = NULL;
    mat_t m;
    unsigned long long phash = dd->path_hashes[dd->path_hashes.size() - 1];
    if (dd->gedp->dbi_state->get_matrix(m, phash, chash))
	mp = m;

    (*leaf_func)(d, chash, mp, op);
}

static void
draw_gather_paths(void *d, unsigned long long c_hash, matp_t m, int UNUSED(op))
{
    struct dd_t *dd = (struct dd_t *)d;

    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pc_it;
    pc_it = dd->gedp->dbi_state->p_c.find(c_hash);

    std::unordered_map<unsigned long long, struct directory *>::iterator d_it;
    d_it = dd->gedp->dbi_state->d_map.find(c_hash);
    if (d_it == dd->gedp->dbi_state->d_map.end()) {
	std::unordered_map<unsigned long long, unsigned long long>::iterator m_it;
	m_it = dd->gedp->dbi_state->i_map.find(c_hash);
	if (m_it != dd->gedp->dbi_state->i_map.end()) {
	    d_it = dd->gedp->dbi_state->d_map.find(m_it->second);
	} else {
	    bu_log("Could not find dp!\n");
	    return;
	}
    } 

    dd->path_hashes.push_back(c_hash);
    mat_t om, nm;
    /* Update current matrix state to reflect the new branch of
     * the tree. Either we have a local matrix, or we have an
     * implicit IDN matrix. */
    MAT_COPY(om, dd->m);
    if (m) {
	MAT_COPY(nm, m);
    } else {
	MAT_IDN(nm);
    }
    bn_mat_mul(dd->m, om, nm);

    if (pc_it != dd->gedp->dbi_state->p_c.end()) {
	// Two things may prevent further processing of a comb - a hidden dp, or
	// a cyclic path.
	struct directory *dp = d_it->second;
	if (!(dp->d_flags & RT_DIR_HIDDEN) && !path_addition_cyclic(dd->path_hashes)) {

	    /* Keep going */
	    std::unordered_set<unsigned long long>::iterator c_it;
	    for (c_it = pc_it->second.begin(); c_it != pc_it->second.end(); c_it++) {
		draw_walk_tree(*c_it, d, dd->subtract_skip, draw_gather_paths);
	    }
	}
    } else {
	// Solid - scene object time
	init_scene_obj(dd);
    }

    /* Done with branch - restore path, put back the old matrix state,
     * and restore previous color settings */
    dd->path_hashes.pop_back();
    MAT_COPY(dd->m, om);
}


/* This function digests the paths into scene object sets.  It does NOT trigger
 * the routines that will actually produce the scene geometry and add it to the
 * scene objects - it only prepares the inputs to be used for that process. */
static int
ged_update_objs(struct ged *gedp, struct bview *v, struct bv_obj_settings *vs, int UNUSED(refresh), int argc, const char *argv[])
{
    //struct bu_ptbl *sg = bv_view_objs(v, BV_DB_OBJS);

    // It takes a bit of work to determine if transparency is set -
    // do it once
    bool transparency_set = false;
    if (!NEAR_ZERO(vs->transparency, SMALL_FASTF) && !NEAR_EQUAL(vs->transparency, 1, SMALL_FASTF)) {
	if (vs->transparency < 0) {
	    vs->transparency = 0;
	}
	if (vs->transparency > 1) {
	    vs->transparency = 1;
	}
	if (!NEAR_ZERO(vs->transparency, SMALL_FASTF) && !NEAR_EQUAL(vs->transparency, 1, SMALL_FASTF))
	    transparency_set = true;
    }

    BViewState *bvs = gedp->dbi_state->get_view_state(v);

    /* Validate that the supplied args are current, valid paths in the
     * database.  If so, walk to find the solids. */
    for (size_t i = 0; i < (size_t)argc; ++i) {
	mat_t fm;
	MAT_IDN(fm);
	struct dd_t d;
	d.v = v;
	d.vs = vs;
	d.m = fm;
	d.gedp = gedp;
	d.transparency_set = transparency_set;
	d.subtract_skip = vs->draw_non_subtract_only;
	d.path_hashes = gedp->dbi_state->digest_path(argv[i]);
	if (!d.path_hashes.size()) {
	    continue;
	}

	// Get initial matrix from path
	gedp->dbi_state->get_path_matrix(d.m, d.path_hashes);

	// In drawing modes 3 (bigE) and 5 (points) we are producing an
	// evaluated shape, rather than iterating to get the solids, so
	// we work directly with the supplied path
	if (vs->s_dmode == 3 || vs->s_dmode == 5) {
	    unsigned long long phash = gedp->dbi_state->path_hash(d.path_hashes, 0);
	    std::unordered_map<unsigned long long, struct bv_scene_obj *>::iterator m_it;
	    m_it = bvs->s_map.find(phash);
	    if (m_it != bvs->s_map.end()) {
		bv_obj_put(m_it->second);
		bvs->s_map.erase(phash);
	    }
	    bvs->s_keys.erase(phash);
	    init_scene_obj(&d);
	    continue;
	}

	// Clear any solids that are subpaths of the current path - they
	// will be recreated
	std::vector<unsigned long long> bad_paths;
	std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator k_it;
	for (k_it = bvs->s_keys.begin(); k_it != bvs->s_keys.end(); k_it++) {
	    if (k_it->second.size() < d.path_hashes.size())
		continue;
	    bool match = std::equal(d.path_hashes.begin(), d.path_hashes.end(), k_it->second.begin());
	    if (match)
		bad_paths.push_back(k_it->first);
	}
	for (size_t j = 0; j < bad_paths.size(); j++) {
	    std::unordered_map<unsigned long long, struct bv_scene_obj *>::iterator m_it;
	    m_it = bvs->s_map.find(bad_paths[j]);
	    if (m_it != bvs->s_map.end()) {
		bv_obj_put(m_it->second);
		bvs->s_map.erase(bad_paths[j]);
	    }
	    bvs->s_keys.erase(bad_paths[j]);
	}

	// Walk the tree (via the dbi_state so we don't have to hit disk
	// to unpack comb trees) to create the solids to be added to
	// represent this path.
	unsigned long long i_key = d.path_hashes[d.path_hashes.size() - 1];
	d.path_hashes.pop_back();
	draw_gather_paths(&d, i_key, NULL, OP_UNION);
    }

    // Update BViewState active_paths set with a collapse call, now that we
    // have accommodated all specified paths incorporated
    bvs->collapse(NULL);

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
	bv_autoview(v, BV_AUTOVIEW_SCALE_DEFAULT, 0);
    }

    // Do the actual drawing
    for (size_t i = 0; i < BU_PTBL_LEN(sg); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(sg, i);
	bu_log("draw %s\n", bu_vls_cstr(&s->s_name));
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
    if (argc < 0) {
	bu_vls_free(&cvls);
	return BRLCAD_ERROR;
    }

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
    // TODO - can we default to the shared view set, if none is
    // specified?
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

