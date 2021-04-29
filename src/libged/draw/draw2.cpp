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

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include "bsocket.h"

#include "bu/cmd.h"
#include "bu/getopt.h"

#include "../ged_private.h"

struct draw_data_t {
    struct db_i *dbip;
    struct bview *v;
    const struct bn_tol *tol;
    const struct bg_tess_tol *ttol;
};

// If possible don't use gedp for the update callback, since
// we most likely eventually want the update functions to
// live at lower levels...
struct draw_update_data_t {
    struct db_i *dbip;
    struct db_full_path fp;
    const struct bn_tol *tol;
    const struct bg_tess_tol *ttol;
};



/**
 * A generic traversal function maintaining awareness of the full path
 * to a given object.
 *
 * TODO - we need more than search does - we also need the matrix to be
 * maintained and passed down (see npush).
 */
HIDDEN void
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

		/* Done with branch - restore path, put back the old matrix state */
		DB_FULL_PATH_POP(path);
		MAT_COPY(*curr_mat, om);

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
HIDDEN void
db_fullpath_draw(struct db_full_path *path, mat_t *curr_mat, void *client_data)
{
    struct directory *dp;
    struct draw_data_t *dd= (struct draw_data_t *)client_data;
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
	BU_GET(s, struct bview_scene_obj);
	BU_LIST_INIT(&(s->s_vlist));
	BU_PTBL_INIT(&s->children);
	BU_VLS_INIT(&s->s_name);
	db_path_to_vls(&s->s_name, path);
	BU_VLS_INIT(&s->s_uuid);
	db_path_to_vls(&s->s_uuid, path);
	// TODO - append hash of matrix and op to uuid to make it properly unique...
	s->s_v = dd->v;
	s->s_type_flags |= BVIEW_DBOBJ_BASED;
	VSET(s->s_color, 255, 0, 0);

	// Stash the information needed for a draw update callback
	struct draw_update_data_t *ud;
	BU_GET(ud, struct draw_update_data_t);
	db_full_path_init(&ud->fp);
	db_dup_full_path(&ud->fp, path);
	ud->dbip = dd->dbip;
	ud->tol = dd->tol;
	ud->ttol = dd->ttol;
	s->s_i_data = (void *)ud;

	// TODO - set update callback function


	// Increment changed flag.  Note in particular that we do not actually do
	// the drawing operations here - that is the job of the update callback,
	// invoked by the application when it initiates a drawing operation.  This
	// flag is incremented to ensure the application knows this object needs
	// to be processed.
	s->s_changed++;

	// Add object to scene
	bu_ptbl_ins(dd->v->gv_scene_objs, (long *)s);

	// (TODO - the following belongs in the update callback...)

	// Make sure we can get the internal form
	struct rt_db_internal dbintern;
	struct rt_db_internal *ip = &dbintern;
	int ret = rt_db_get_internal(ip, DB_FULL_PATH_CUR_DIR(path), dd->dbip, *curr_mat, &rt_uniresource);
	if (ret < 0 || !ip->idb_meth->ft_plot)
	    return;

	// Get wireframe
	struct rt_view_info info;
	info.bot_threshold = dd->v->gvs.bot_threshold;
	ret = ip->idb_meth->ft_plot(&s->s_vlist, ip, dd->ttol, dd->tol, &info);

	// Draw label
	if (ip->idb_meth->ft_labels)
	    ip->idb_meth->ft_labels(&s->children, ip, s->s_v);

    }
}


extern "C" int
ged_draw2_core(struct ged *gedp, int argc, const char *argv[])
{
    struct db_i *dbip = gedp->ged_wdbp->dbip;
    static const char *usage = "path";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc < 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* Look up object.  NOTE: for testing simplicity this is just an object lookup
     * right now, but it needs to walk the tree and do the full-path instance creation.
     * Note also that view objects aren't what we want to report with a "who" return,
     * so we'll need to track the list of drawn paths independently. */
    struct db_full_path fp;
    db_full_path_init(&fp);
    int ret = db_string_to_path(&fp, dbip, argv[1]);
    if (ret < 0) {
	db_free_full_path(&fp);
	return GED_ERROR;
    }

    // Get the "seed" matrix from the path - everything we draw at or below the path
    // will be positioned using it
    mat_t mat;
    MAT_IDN(mat);
    if (!db_path_to_mat(dbip, &fp, mat, fp.fp_len-1, &rt_uniresource)) {
	db_free_full_path(&fp);
	return GED_ERROR;
    }

    // Prepare tree walking data container
    struct draw_data_t dd;
    dd.dbip = gedp->ged_wdbp->dbip;
    dd.v = gedp->ged_gvp;
    dd.tol = &gedp->ged_wdbp->wdb_tol;
    dd.ttol = &gedp->ged_wdbp->wdb_ttol;

    // Walk the tree to build up the set of scene objects
    db_fullpath_draw(&fp, &mat, (void *)&dd);

    // Done with path
    db_free_full_path(&fp);

    // Scene objects are created and stored in gv_scene_objs. The application
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
