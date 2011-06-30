/*                         Y P R . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file libged/ypr.c
 *
 * The ypr command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"
#include "./ged_private.h"


int
ged_ypr(struct ged *gedp, int argc, const char *argv[])
{
    vect_t ypr;
    mat_t mat;
    static const char *usage = "yaw pitch roll";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* return Viewrot as yaw, pitch and roll */
    if (argc == 1) {
	point_t pt;

	bn_mat_trn(mat, gedp->ged_gvp->gv_rotation);
	anim_v_unpermute(mat);

	if (anim_mat2ypr(pt, mat) == 2) {
	    bu_vls_printf(gedp->ged_result_str, "view %s - matrix is not a rotation matrix", argv[0]);
	    return GED_ERROR;
	}

	VSCALE(pt, pt, bn_radtodeg);
	bu_vls_printf(gedp->ged_result_str, "%.12g %.12g %.12g", V3ARGS(pt));

	return GED_OK;
    }

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: view %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* attempt to set Viewrot given yaw, pitch and roll */
    if (sscanf(argv[1], "%lf", ypr) != 1
	|| sscanf(argv[2], "%lf", ypr+1) != 1
	|| sscanf(argv[3], "%lf", ypr+2) != 1)
    {

	bu_vls_printf(gedp->ged_result_str, "view %s: bad value detected - %s %s %s",
		      argv[0], argv[1], argv[2], argv[3]);
	return GED_ERROR;
    }

    anim_dy_p_r2mat(mat, V3ARGS(ypr));
    anim_v_permute(mat);
    bn_mat_trn(gedp->ged_gvp->gv_rotation, mat);
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
