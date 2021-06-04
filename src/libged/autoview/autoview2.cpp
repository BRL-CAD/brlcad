/*                       A U T O V I E W . C P P
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
/** @file libged/autoview.cpp
 *
 * The autoview command.
 *
 */

#include "common.h"
#include "bu/opt.h"
#include "dm.h"
#include "../ged_private.h"

/*
 * Auto-adjust the view so that all displayed geometry is in view
 *
 * Usage:
 * autoview
 *
 * TODO - like draw2, this needs to be aware of whether we're using local or shared
 * grp sets - if we're adaptive plotting, for example.
 *
 */
extern "C" int
ged_autoview2_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "[options] [scale]";
    struct bu_vls cvls = BU_VLS_INIT_ZERO;
    int is_empty = 1;
    vect_t min, max;
    vect_t center = VINIT_ZERO;
    vect_t radial;
    vect_t sqrt_small;

    /* less than or near zero uses default, 0.5 model scale == 2.0 view factor */
    fastf_t factor = -1.0;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);


    int all_view_objs = 0;
    int print_help = 0;
    struct bview *v = gedp->ged_gvp;

    struct bu_opt_desc d[4];
    BU_OPT(d[0], "h", "help",      "",        NULL,     &print_help, "Print help and exit");
    BU_OPT(d[1], "",   "all-objs", "",        NULL,  &all_view_objs, "Bound all non-faceplate view objects");
    BU_OPT(d[2], "V", "view",  "name", &bu_opt_vls,           &cvls, "Specify view to adjust");
    BU_OPT_NULL(d[3]);

    argc-=(argc>0); argv+=(argc>0); /* skip command name argv[0] */

    int opt_ret = bu_opt_parse(NULL, argc, argv, d);

    if (print_help) {
	_ged_cmd_help(gedp, usage, d);
	return GED_OK;
    }

    argc = opt_ret;

    if (argc && argc != 1) {
	_ged_cmd_help(gedp, usage, d);
	return GED_ERROR;
    }

    if (bu_vls_strlen(&cvls)) {
	int found_match = 0;
	for (size_t i = 0; i < BU_PTBL_LEN(&gedp->ged_views); i++) {
	    struct bview *tv = (struct bview *)BU_PTBL_GET(&gedp->ged_views, i);
	    if (BU_STR_EQUAL(bu_vls_cstr(&tv->gv_name), bu_vls_cstr(&cvls))) {
		v = tv;
		found_match = 1;
		break;
	    }
	}
	if (!found_match) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s not found\n", bu_vls_cstr(&cvls));
	    bu_vls_free(&cvls);
	    return GED_ERROR;
	}
    }
    bu_vls_free(&cvls);


    /* parse the optional scale argument */
    if (argc) {
	double scale = 0.0;
	int ret = sscanf(argv[0], "%lf", &scale);
	if (ret != 1) {
	    bu_vls_printf(gedp->ged_result_str, "ERROR: Expecting floating point scale value\n");
	    return GED_ERROR;
	}
	if (scale > 0.0) {
	    factor = 1.0 / scale;
	}
    }

    /* set the default if unset or insane */
    if (factor < SQRT_SMALL_FASTF) {
	factor = 2.0; /* 2 is half the view */
    }

    VSETALL(sqrt_small, SQRT_SMALL_FASTF);

    /* calculate the bounding for all solids and polygons being displayed */
    VSETALL(min,  INFINITY);
    VSETALL(max, -INFINITY);

    struct bu_ptbl *so;
    if (v->independent) {
	so = v->gv_view_grps;
    } else {
	so = v->gv_db_grps;
    }
    vect_t minus, plus;
    int have_geom_objs = 0;
    for (size_t i = 0; i < BU_PTBL_LEN(so); i++) {
	struct bv_scene_group *g = (struct bv_scene_group *)BU_PTBL_GET(so, i);
	if (bu_list_len(&g->g->s_vlist)) {
	    bv_scene_obj_bound(g->g);
	    is_empty = 0;
	    have_geom_objs = 1;
	    minus[X] = g->g->s_center[X] - g->g->s_size;
	    minus[Y] = g->g->s_center[Y] - g->g->s_size;
	    minus[Z] = g->g->s_center[Z] - g->g->s_size;
	    VMIN(min, minus);
	    plus[X] = g->g->s_center[X] + g->g->s_size;
	    plus[Y] = g->g->s_center[Y] + g->g->s_size;
	    plus[Z] = g->g->s_center[Z] + g->g->s_size;
	    VMAX(max, plus);
	}
	for (size_t j = 0; j < BU_PTBL_LEN(&g->g->children); j++) {
	    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(&g->g->children, j);
	    bv_scene_obj_bound(s);
	    is_empty = 0;
	    minus[X] = s->s_center[X] - s->s_size;
	    minus[Y] = s->s_center[Y] - s->s_size;
	    minus[Z] = s->s_center[Z] - s->s_size;
	    VMIN(min, minus);
	    plus[X] = s->s_center[X] + s->s_size;
	    plus[Y] = s->s_center[Y] + s->s_size;
	    plus[Z] = s->s_center[Z] + s->s_size;
	    VMAX(max, plus);

	    for (size_t k = 0; k < BU_PTBL_LEN(&s->children); k++) {
		struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(&s->children, k);
		struct bv_vlist *tvp;
		for (BU_LIST_FOR(tvp, bv_vlist, &((struct bv_vlist *)(&s_c->s_vlist))->l)) {
		    int nused = tvp->nused;
		    int *cmd = tvp->cmd;
		    point_t *pt = tvp->pt;
		    for (int l = 0; l < nused; l++, cmd++, pt++) {
			VMINMAX(min, max, *pt);
		    }
		}
	    }

	}
    }

    // When it comes to view-only objects, normally we will only include those
    // that are db object based, polygons or labels, unless the flag to
    // consider all objects is set.   However, there is an exception - if there
    // are NO such objects in the scene (have_geom_objs == 0) and we do have
    // view objs (for example, when overlaying a plot file on an empty view)
    // then basing autoview on the view-only objs is more intuitive than just
    // using the default view settings.
    if (v->independent) {
	so = v->gv_view_objs;
    } else {
	so = v->gv_view_shared_objs;
    }
    for (size_t i = 0; i < BU_PTBL_LEN(so); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(so, i);
	if ((s->s_type_flags & BV_DBOBJ_BASED) ||
		(s->s_type_flags & BV_POLYGONS) ||
		(s->s_type_flags & BV_LABELS))
	    have_geom_objs = 1;
    }
    for (size_t i = 0; i < BU_PTBL_LEN(so); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(so, i);
	if (have_geom_objs && !all_view_objs) {
	    if (!(s->s_type_flags & BV_DBOBJ_BASED) &&
		    !(s->s_type_flags & BV_POLYGONS) &&
		    !(s->s_type_flags & BV_LABELS))
		continue;
	}
	bv_scene_obj_bound(s);
	is_empty = 0;
	minus[X] = s->s_center[X] - s->s_size;
	minus[Y] = s->s_center[Y] - s->s_size;
	minus[Z] = s->s_center[Z] - s->s_size;
	VMIN(min, minus);
	plus[X] = s->s_center[X] + s->s_size;
	plus[Y] = s->s_center[Y] + s->s_size;
	plus[Z] = s->s_center[Z] + s->s_size;
	VMAX(max, plus);

	for (size_t j = 0; j < BU_PTBL_LEN(&s->children); j++) {
	    struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(&s->children, j);
	    struct bv_vlist *tvp;
	    for (BU_LIST_FOR(tvp, bv_vlist, &((struct bv_vlist *)(&s_c->s_vlist))->l)) {
		int k;
		int nused = tvp->nused;
		int *cmd = tvp->cmd;
		point_t *pt = tvp->pt;
		for (k = 0; k < nused; k++, cmd++, pt++) {
		    VMINMAX(min, max, *pt);
		}
	    }
	}
    }

    if (is_empty) {
	/* Nothing is in view */
	VSETALL(radial, 1000.0);
    } else {
	VADD2SCALE(center, max, min, 0.5);
	VSUB2(radial, max, center);
    }

    /* make sure it's not inverted */
    VMAX(radial, sqrt_small);

    /* make sure it's not too small */
    if (VNEAR_ZERO(radial, SQRT_SMALL_FASTF))
	VSETALL(radial, 1.0);

    MAT_IDN(v->gv_center);
    MAT_DELTAS_VEC_NEG(v->gv_center, center);
    v->gv_scale = radial[X];
    V_MAX(v->gv_scale, radial[Y]);
    V_MAX(v->gv_scale, radial[Z]);

    v->gv_size = factor * v->gv_scale;
    v->gv_isize = 1.0 / v->gv_size;
    bv_update(v);

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
