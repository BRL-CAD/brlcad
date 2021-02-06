/*                    B V I E W _ U T I L . C
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
/** @file bview_util.c
 *
 * Utility functions for operating on BRL-CAD views
 *
 */

#include "common.h"
#include "vmath.h"
#include "bu/log.h"
#include "bn/mat.h"
#include "dm/defines.h"
#include "dm/bview_util.h"

void
bview_update(struct bview *gvp)
{
    vect_t work, work1;
    vect_t temp, temp1;

    if (!gvp)
        return;

    bn_mat_mul(gvp->gv_model2view,
               gvp->gv_rotation,
               gvp->gv_center);
    gvp->gv_model2view[15] = gvp->gv_scale;
    bn_mat_inv(gvp->gv_view2model, gvp->gv_model2view);

    /* Find current azimuth, elevation, and twist angles */
    VSET(work, 0.0, 0.0, 1.0);       /* view z-direction */
    MAT4X3VEC(temp, gvp->gv_view2model, work);
    VSET(work1, 1.0, 0.0, 0.0);      /* view x-direction */
    MAT4X3VEC(temp1, gvp->gv_view2model, work1);

    /* calculate angles using accuracy of 0.005, since display
     * shows 2 digits right of decimal point */
    bn_aet_vec(&gvp->gv_aet[0],
               &gvp->gv_aet[1],
               &gvp->gv_aet[2],
               temp, temp1, (fastf_t)0.005);

    /* Force azimuth range to be [0, 360] */
    if ((NEAR_EQUAL(gvp->gv_aet[1], 90.0, (fastf_t)0.005) ||
         NEAR_EQUAL(gvp->gv_aet[1], -90.0, (fastf_t)0.005)) &&
        gvp->gv_aet[0] < 0 &&
        !NEAR_ZERO(gvp->gv_aet[0], (fastf_t)0.005))
        gvp->gv_aet[0] += 360.0;
    else if (NEAR_ZERO(gvp->gv_aet[0], (fastf_t)0.005))
        gvp->gv_aet[0] = 0.0;

    /* apply the perspective angle to model2view */
    bn_mat_mul(gvp->gv_pmodel2view, gvp->gv_pmat, gvp->gv_model2view);

    if (gvp->gv_callback) {

	if (gvp->callbacks) {
	    if (bu_ptbl_locate(gvp->callbacks, (long *)(long)gvp->gv_callback) != -1) {
		bu_log("Recursive callback (bview_update and gvp->gv_callback)");
	    }
	    bu_ptbl_ins_unique(gvp->callbacks, (long *)(long)gvp->gv_callback);
	}

	(*gvp->gv_callback)(gvp, gvp->gv_clientData);

	if (gvp->callbacks) {
	    bu_ptbl_rm(gvp->callbacks, (long *)(long)gvp->gv_callback);
	}

    }
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
