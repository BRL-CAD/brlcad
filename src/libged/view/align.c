/*                         A L I G N. C
 * BRL-CAD
 *
 * Copyright (c) 2008-2024 United States Government as represented by
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
/** @file libged/align.c
 *
 * The align command.
 * 'align' the eye point, view center, and a desired point
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../ged_private.h"


int
ged_align_core(struct ged *gedp, int argc, const char *argv[])
{
    point_t align;
    point_t eye;
    point_t center;
    point_t new_center;
    vect_t dir;
    fastf_t new_az, new_el;
    int inverse = 0;
    double scan[3];
    static const char *usage = "[-i] x y z";

    /* check state */
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* check usage */
    if (argc == 1) {
	// must be wanting help
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }
    if (!bu_strcmp(argv[1], "-i")) {
	// user requested inverse alignment
	inverse = 1;
	argv++;
	argc--;	// decrement to simplify filtering
    }
    if (argc != 2 && argc != 4) {
	// accepts grouping of point or explicit - ie "x y z" vs x y z
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* filter input */
    if (argc == 2) {
	if (bn_decode_vect(align, argv[1]) != 3) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return BRLCAD_ERROR;
	}
    } else {
	if (sscanf(argv[1], "%lf", &scan[X]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "ged_align_core: bad X value - %s\n", argv[1]);
	    return BRLCAD_ERROR;
	}

	if (sscanf(argv[2], "%lf", &scan[Y]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "ged_align_core: bad Y value - %s\n", argv[2]);
	    return BRLCAD_ERROR;
	}

	if (sscanf(argv[3], "%lf", &scan[Z]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "ged_align_core: bad Z value - %s\n", argv[3]);
	    return BRLCAD_ERROR;
	}

	// convert from double to fastf_t
	VMOVE(align, scan);
    }

    // scale align_pt to mm
    double scale = (gedp->dbip) ? gedp->dbip->dbi_local2base : 1.0;
    VSCALE(align, align, scale);

    // get center as point
    point_t tmp = {0.0, 0.0, 0.0};
    MAT4X3PNT(center, gedp->ged_gvp->gv_center, tmp);
    VSCALE(center, center, -1.0);

    // calculate eye / center / align_pt distances
    vect_t xlate = {0.0, 0.0, 1.0};
    MAT4X3PNT(eye, gedp->ged_gvp->gv_view2model, xlate);
    VSCALE(eye, eye, scale);
    double dist_eye_center = DIST_PNT_PNT(center, eye);
    double dist_alignpt_center = DIST_PNT_PNT(align, center);
    double dist = dist_eye_center - dist_alignpt_center;

    // find direction vector. either center->align_pt or align_pt->center
    if (inverse) {
	VSUB2(dir, center, align);
    } else {
	VSUB2(dir, align, center);
    }
    VUNITIZE(dir);
    bn_ae_vec(&new_az, &new_el, dir);

    // update view ae using direction
    VSET(gedp->ged_gvp->gv_aet, new_az, new_el, gedp->ged_gvp->gv_aet[Z]);
    bv_mat_aet(gedp->ged_gvp);

    // update eye
    point_t new_eye;
    VJOIN1(new_eye, align, -dist, dir);	// new_eye = align_pt - dist * dir
    MAT_DELTAS_VEC_NEG(gedp->ged_gvp->gv_view2model, new_eye);

    // done. update the view
    bv_update(gedp->ged_gvp);

    return BRLCAD_OK;
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
