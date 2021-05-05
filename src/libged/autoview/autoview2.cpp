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
 */
extern "C" int
ged_autoview2_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "[options] [scale]";
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
    int use_vlists = 0;
    int print_help = 0;

    struct bu_opt_desc d[3];
    BU_OPT(d[0], "h", "help",      "",  NULL,  &print_help,    "Print help and exit");
    BU_OPT(d[1], "",  "all",       "",  NULL,  &all_view_objs, "Bound all non-faceplate view objects");
    BU_OPT_NULL(d[2]);

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
	factor = 1.0; /* 2 is half the view */
    }

    VSETALL(sqrt_small, SQRT_SMALL_FASTF);

    /* calculate the bounding for all solids and polygons being displayed */
    VSETALL(min,  INFINITY);
    VSETALL(max, -INFINITY);

    struct bu_ptbl *so = gedp->ged_gvp->gv_db_grps;
    if (!use_vlists) {
	vect_t minus, plus;
	for (size_t i = 0; i < BU_PTBL_LEN(so); i++) {
	    struct bview_scene_group *g = (struct bview_scene_group *)BU_PTBL_GET(so, i);
	    if (bu_list_len(&g->g->s_vlist)) {
		bview_scene_obj_bound(g->g);
		is_empty = 0;
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
		struct bview_scene_obj *s = (struct bview_scene_obj *)BU_PTBL_GET(&g->g->children, j);
		bview_scene_obj_bound(s);
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
		    struct bview_scene_obj *s_c = (struct bview_scene_obj *)BU_PTBL_GET(&s->children, k);
		    struct bn_vlist *tvp;
		    for (BU_LIST_FOR(tvp, bn_vlist, &((struct bn_vlist *)(&s_c->s_vlist))->l)) {
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
    }

    // When it comes to view-only objects, only include those that are db object based, polygons
    // or labels, unless the flag to consider all objects is set.
    so = gedp->ged_gvp->gv_view_objs;
    for (size_t i = 0; i < BU_PTBL_LEN(so); i++) {
	struct bview_scene_obj *s = (struct bview_scene_obj *)BU_PTBL_GET(so, i);
	if (!all_view_objs) {
	    if (!(s->s_type_flags & BVIEW_DBOBJ_BASED) &&
		    !(s->s_type_flags & BVIEW_POLYGONS) &&
		    !(s->s_type_flags & BVIEW_LABELS))
		continue;
	}
	bview_scene_obj_bound(s);
	is_empty = 0;
	vect_t minus, plus;
	minus[X] = s->s_center[X] - s->s_size;
	minus[Y] = s->s_center[Y] - s->s_size;
	minus[Z] = s->s_center[Z] - s->s_size;
	VMIN(min, minus);
	plus[X] = s->s_center[X] + s->s_size;
	plus[Y] = s->s_center[Y] + s->s_size;
	plus[Z] = s->s_center[Z] + s->s_size;
	VMAX(max, plus);

	for (size_t j = 0; j < BU_PTBL_LEN(&s->children); j++) {
	    struct bview_scene_obj *s_c = (struct bview_scene_obj *)BU_PTBL_GET(&s->children, j);
	    struct bn_vlist *tvp;
	    for (BU_LIST_FOR(tvp, bn_vlist, &((struct bn_vlist *)(&s_c->s_vlist))->l)) {
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

    MAT_IDN(gedp->ged_gvp->gv_center);
    MAT_DELTAS_VEC_NEG(gedp->ged_gvp->gv_center, center);
    gedp->ged_gvp->gv_scale = radial[X];
    V_MAX(gedp->ged_gvp->gv_scale, radial[Y]);
    V_MAX(gedp->ged_gvp->gv_scale, radial[Z]);

    gedp->ged_gvp->gv_size = factor * gedp->ged_gvp->gv_scale;
    gedp->ged_gvp->gv_isize = 1.0 / gedp->ged_gvp->gv_size;
    bview_update(gedp->ged_gvp);

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
