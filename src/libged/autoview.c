/*                         A U T O V I E W . C
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
/** @file autoview.c
 *
 * The autoview command.
 *
 */

#include "ged_private.h"
#include "solid.h"

/*
 * Auto-adjust the view so that all displayed geometry is in view
 *
 * Usage:
 *        autoview
 *
 */
int
ged_autoview(struct ged *gedp, int argc, const char *argv[])
{
    register struct solid	*sp;
    vect_t		min, max;
    vect_t		minus, plus;
    vect_t		center;
    vect_t		radial;
    vect_t		small;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);

    if (argc != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s", argv[0]);
	return BRLCAD_ERROR;
    }

    VSETALL(min,  INFINITY);
    VSETALL(max, -INFINITY);
    VSETALL(small, SQRT_SMALL_FASTF);

    FOR_ALL_SOLIDS(sp, &gedp->ged_gdp->gd_headSolid) {
	minus[X] = sp->s_center[X] - sp->s_size;
	minus[Y] = sp->s_center[Y] - sp->s_size;
	minus[Z] = sp->s_center[Z] - sp->s_size;
	VMIN(min, minus);
	plus[X] = sp->s_center[X] + sp->s_size;
	plus[Y] = sp->s_center[Y] + sp->s_size;
	plus[Z] = sp->s_center[Z] + sp->s_size;
	VMAX(max, plus);
    }

    if (BU_LIST_IS_EMPTY(&gedp->ged_gdp->gd_headSolid)) {
	/* Nothing is in view */
	VSETALL(center, 0.0);
	VSETALL(radial, 1000.0);	/* 1 meter */
    } else {
	VADD2SCALE(center, max, min, 0.5);
	VSUB2(radial, max, center);
    }

    /* make sure it's not inverted */
    VMAX(radial, small);

    /* make sure it's not too small */
    if (VNEAR_ZERO(radial, SQRT_SMALL_FASTF))
	VSETALL(radial, 1.0);

    MAT_IDN(gedp->ged_gvp->gv_center);
    MAT_DELTAS_VEC_NEG(gedp->ged_gvp->gv_center, center);
    gedp->ged_gvp->gv_scale = radial[X];
    V_MAX(gedp->ged_gvp->gv_scale, radial[Y]);
    V_MAX(gedp->ged_gvp->gv_scale, radial[Z]);

    gedp->ged_gvp->gv_size = 2.0 * gedp->ged_gvp->gv_scale;
    gedp->ged_gvp->gv_invSize = 1.0 / gedp->ged_gvp->gv_size;
    ged_view_update(gedp->ged_gvp);

    return BRLCAD_OK;
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
