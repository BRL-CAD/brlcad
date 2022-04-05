/*                         Z A P . C P P
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
ged_clear_view(struct ged *gedp, struct bview *v, int clear_solid_objs, int clear_view_objs)
{
    struct bu_ptbl *sv;

    if (clear_solid_objs) {
	// Always make sure the view's specified local containers are cleared.  We
	// may be skipping the shared objects depending on the settings, but a zap
	// on a view always clears the local versions.
	struct bu_ptbl *sg = v->gv_objs.view_grps;
	if (sg) {
	    for (size_t i = 0; i < BU_PTBL_LEN(sg); i++) {
		struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(sg, i);
		bv_scene_obj_free(cg, gedp->ged_views.free_scene_obj);
	    }
	    bu_ptbl_reset(sg);
	}

	// If set, clear the shared objects as well (this has implications
	// beyond just this view, so it is only done when the caller
	// specifically requests it.)
	if (!v->independent) {
	    sg = v->gv_objs.db_grps;
	    if (sg) {
		for (size_t i = 0; i < BU_PTBL_LEN(sg); i++) {
		    struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(sg, i);
		    bv_scene_obj_free(cg, gedp->ged_views.free_scene_obj);
		}
		bu_ptbl_reset(sg);
	    }
	}
    }

    // If we're not processing view objects, we're done
    if (!clear_view_objs)
	return BRLCAD_OK;

    // If the options indicate it, clear view objects as well
    sv = v->gv_objs.view_objs;
    if (sv) {
	for (long i = (long)BU_PTBL_LEN(sv) - 1; i >= 0; i--) {
	    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(sv, i);
	    bu_ptbl_rm(sv, (long *)s);
	    bv_scene_obj_free(s, gedp->ged_views.free_scene_obj);
	}
    }
    if (!v->independent) {
	sv = v->gv_objs.view_shared_objs;
	if (sv) {
	    for (long i = (long)BU_PTBL_LEN(sv) - 1; i >= 0; i--) {
		struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(sv, i);
		bu_ptbl_rm(sv, (long *)s);
		bv_scene_obj_free(s, gedp->ged_views.free_scene_obj);
	    }
	}
    }

    /* The application may need to adjust itself when the view is fully
     * cleared.  Since the blast command may immediately re-populate the
     * display list, we set a flag in the view to inform the app a zap
     * operation has taken place. */
    if (!BU_PTBL_LEN(v->gv_objs.view_objs) && !BU_PTBL_LEN(v->gv_objs.view_grps)) {
	if (v->independent || (!BU_PTBL_LEN(v->gv_objs.view_shared_objs) && !BU_PTBL_LEN(v->gv_objs.db_grps))) {
	    v->gv_s->gv_cleared = 1;
	}
    }

    return BRLCAD_OK;
}

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
    int print_help = 0;
    struct bu_vls cvls = BU_VLS_INIT_ZERO;
    int clear_view_objs = 0;
    int clear_solid_objs = 0;
    int clear_all_views = 0;
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);
    const char *usage = "zap [options]\n";
    struct bview *v = gedp->ged_gvp;

    argc-=(argc>0); argv+=(argc>0); /* done with command name argv[0] */

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bu_opt_desc d[7];
    BU_OPT(d[0],  "h", "help",          "",        NULL,       &print_help, "Print help and exit");
    BU_OPT(d[1],  "?", "",              "",        NULL,       &print_help, "");
    BU_OPT(d[2],  "V", "view",     "name", &bu_opt_vls,             &cvls, "specify view to draw on");
    BU_OPT(d[3],  "v", "view-objs",    "",        NULL,  &clear_view_objs, "clear non-solid based view objects");
    BU_OPT(d[4],  "g", "solid-objs",   "",        NULL, &clear_solid_objs, "clear solid based view objects");
    BU_OPT(d[5],  "",  "all",          "",        NULL, &clear_all_views, "clear shared and independent views");
    BU_OPT_NULL(d[6]);

    int opt_ret = bu_opt_parse(NULL, argc, argv, d);
    argc = opt_ret;

    if (print_help) {
	_ged_cmd_help(gedp, usage, d);
	return BRLCAD_HELP;
    }

    if (argc) {
	_ged_cmd_help(gedp, usage, d);
	return BRLCAD_ERROR;
    }

    if (!clear_all_views && bu_vls_strlen(&cvls)) {
	int found_match = 0;
	for (size_t i = 0; i < BU_PTBL_LEN(&gedp->ged_views.views); i++) {
	    struct bview *tv = (struct bview *)BU_PTBL_GET(&gedp->ged_views.views, i);
	    if (BU_STR_EQUAL(bu_vls_cstr(&tv->gv_name), bu_vls_cstr(&cvls))) {
		v = tv;
		found_match = 1;
		break;
	    }
	}
	if (!found_match) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s not found\n", bu_vls_cstr(&cvls));
	    bu_vls_free(&cvls);
	    return BRLCAD_ERROR;
	}

	if (!v->independent) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s is not an independent view, and as such does not support clearing db objects in only this view.  To change the view's status, the command 'view independent %s 1' may be applied.\n", bu_vls_cstr(&cvls), bu_vls_cstr(&cvls));
	    bu_vls_free(&cvls);
	    return BRLCAD_ERROR;
	}
    }
    bu_vls_free(&cvls);

    if (!clear_solid_objs && !clear_view_objs) {
	clear_solid_objs = 1;
	clear_view_objs = 1;
    }

    // If we're clearing just one view, handle it
    if (v && v->independent && !clear_all_views) {
	return ged_clear_view(gedp, v, clear_solid_objs, clear_view_objs);
    }

    // Clear everything
    int ret = BRLCAD_OK;
    for (size_t i = 0; i < BU_PTBL_LEN(&gedp->ged_views.views); i++) {
	v = (struct bview *)BU_PTBL_GET(&gedp->ged_views.views, i);
	if (v->independent && !clear_all_views)
	    continue;
	int nret = ged_clear_view(gedp, v, clear_solid_objs, clear_view_objs);
	if (nret & BRLCAD_ERROR)
	    ret = nret;
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
