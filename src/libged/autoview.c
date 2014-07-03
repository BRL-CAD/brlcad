/*                         A U T O V I E W . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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
/** @file libged/autoview.c
 *
 * The autoview command.
 *
 */

#include "common.h"

#include "bio.h"

#include "solid.h"

#include "./ged_private.h"

/*
 * Auto-adjust the view so that all displayed geometry is in view
 *
 * Usage:
 * autoview
 *
 */
int
ged_autoview(struct ged *gedp, int argc, const char *argv[])
{
    struct ged_display_list *gdlp;
    struct ged_display_list *next_gdlp;
    struct solid *sp;
    int is_empty = 1;
    vect_t min, max;
    vect_t minus, plus;
    vect_t center;
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

    if (argc > 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s [scale]", argv[0]);
	return GED_ERROR;
    }

    /* parse the optional scale argument */
    if (argc > 1) {
	double scale = 0.0;
	int ret = sscanf(argv[1], "%lf", &scale);
	if (ret != 1) {
	    bu_vls_printf(gedp->ged_result_str, "ERROR: Expecting floating point scale value after %s\n", argv[0]);
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

    VSETALL(min,  INFINITY);
    VSETALL(max, -INFINITY);
    VSETALL(sqrt_small, SQRT_SMALL_FASTF);

    /* calculate the bounding for of all solids being displayed */
    gdlp = BU_LIST_NEXT(ged_display_list, gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, gedp->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {
	    minus[X] = sp->s_center[X] - sp->s_size;
	    minus[Y] = sp->s_center[Y] - sp->s_size;
	    minus[Z] = sp->s_center[Z] - sp->s_size;
	    VMIN(min, minus);
	    plus[X] = sp->s_center[X] + sp->s_size;
	    plus[Y] = sp->s_center[Y] + sp->s_size;
	    plus[Z] = sp->s_center[Z] + sp->s_size;
	    VMAX(max, plus);

	    is_empty = 0;
	}

	gdlp = next_gdlp;
    }

    if (is_empty) {
	/* Nothing is in view */
	VSETALL(center, 0.0);
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
    ged_view_update(gedp->ged_gvp);

    return GED_OK;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
