/*                         Z A P . C P P
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
/** @file libged/zap.cpp
 *
 * The zap command.
 *
 */

#include "common.h"

#include <stdlib.h>


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
ged_clear_view(struct ged *gedp, struct bview *v, int clear_shared, int clear_view_obj)
{
    struct bu_ptbl *sv;

    if (clear_view_obj != 2) {
	// Always make sure the view's specified local containers are cleared.  We
	// may be skipping the shared objects depending on the settings, but a zap
	// on a view alwayw clears the local versions.
	struct bu_ptbl *sg = v->gv_view_grps;
	if (sg) {
	    for (size_t i = 0; i < BU_PTBL_LEN(sg); i++) {
		struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(sg, i);
		bv_scene_obj_free(cg->g, gedp->free_scene_obj);
		BU_PUT(cg, struct bv_scene_group);
	    }
	    bu_ptbl_reset(sg);
	}

	// If set, clear the shared objects as well (this has implications
	// beyond just this view, so it is only done when the caller
	// specifically requests it.)
	if (clear_shared) {
	    sg = v->gv_db_grps;
	    if (sg) {
		for (size_t i = 0; i < BU_PTBL_LEN(sg); i++) {
		    struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(sg, i);
		    bv_scene_obj_free(cg->g, gedp->free_scene_obj);
		    BU_PUT(cg, struct bv_scene_group);
		}
		bu_ptbl_reset(sg);
	    }
	}
    }

    // If we're not processing view objects, we're done
    if (clear_view_obj)
	return GED_OK;

    // If the options indicate it, clear view objects as well
    sv = v->gv_view_objs;
    if (sv) {
	for (long i = (long)BU_PTBL_LEN(sv) - 1; i >= 0; i--) {
	    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(sv, i);
	    bu_ptbl_rm(sv, (long *)s);
	    bv_scene_obj_free(s, gedp->free_scene_obj);
	}
    }
    if (clear_shared) {
	sv = v->gv_view_shared_objs;
	if (sv) {
	    for (long i = (long)BU_PTBL_LEN(sv) - 1; i >= 0; i--) {
		struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(sv, i);
		bu_ptbl_rm(sv, (long *)s);
		bv_scene_obj_free(s, gedp->free_scene_obj);
	    }
	}
    }

    /* The application may need to adjust itself when the view is fully
     * cleared.  Since the blast command may immediately re-populate the
     * display list, we set a flag in the view to inform the app a zap
     * operation has taken place. */
    if (!BU_PTBL_LEN(v->gv_view_objs) && !BU_PTBL_LEN(v->gv_view_shared_objs) &&
	 !BU_PTBL_LEN(v->gv_view_grps) && !BU_PTBL_LEN(v->gv_db_grps)) {
	v->gv_s->gv_cleared = 1;
    }

    return GED_OK;
}

/*
 * Erase all currently displayed geometry
 *
 * TODO = needs an option to specify a single view to zap
 *
 * Usage:
 * zap
 *
 */
extern "C" int
ged_zap2_core(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc != 1 && argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", argv[0]);
	return GED_ERROR;
    }

    int clear_view_obj = 0;
    if (argc == 2 && BU_STR_EQUAL(argv[1], "-v")) {
	clear_view_obj = 1;
    }

    int ret = GED_OK;
    for (size_t i = 0; i < BU_PTBL_LEN(&gedp->ged_views); i++) {
	struct bview *v = (struct bview *)BU_PTBL_GET(&gedp->ged_views, i);
	int nret = ged_clear_view(gedp, v, 1, clear_view_obj);
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
