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

#define GET_BVIEW_SCENE_OBJ(p, fp) { \
    if (BU_LIST_IS_EMPTY(fp)) { \
	BU_ALLOC((p), struct bview_scene_obj); \
    } else { \
	p = BU_LIST_NEXT(bview_scene_obj, fp); \
	BU_LIST_DEQUEUE(&((p)->l)); \
    } \
    BU_LIST_INIT( &((p)->s_vlist) ); }

/*
 * Erase all currently displayed geometry
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

    struct bu_ptbl *sg = gedp->ged_gvp->gv_db_grps;
    if (argc == 1 && sg) {
	for (size_t i = 0; i < BU_PTBL_LEN(sg); i++) {
	    struct bview_scene_group *cg = (struct bview_scene_group *)BU_PTBL_GET(sg, i);
	    bview_scene_obj_free(cg->g, gedp->free_scene_obj);
	    BU_PUT(cg, struct bview_scene_group);
	}
	bu_ptbl_reset(sg);
    }

    /* If -v specified, view objects are to be cleared */
    if (argc == 2 && BU_STR_EQUAL(argv[1], "-v")) {
	for (long i = (long)BU_PTBL_LEN(gedp->ged_gvp->gv_view_objs) - 1; i >= 0; i--) {
	    struct bview_scene_obj *s = (struct bview_scene_obj *)BU_PTBL_GET(gedp->ged_gvp->gv_view_objs, i);
	    bu_ptbl_rm(gedp->ged_gvp->gv_view_objs, (long *)s);
	    bview_scene_obj_free(s, gedp->free_scene_obj);
	}
    }

    /* The application may need to adjust itself when the view
     * is cleared.  Since the blast command may immediately
     * re-populate the display list, we set a flag in the view
     * to inform the app a zap operation has taken place. */
    if (gedp->ged_gvp && !BU_PTBL_LEN(gedp->ged_gvp->gv_view_objs)) {
	gedp->ged_gvp->gv_cleared = 1;
    }

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
