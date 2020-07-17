/*                        L I B F U N C S . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2020 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file libged/libfuncs.c
 *
 * BRL-CAD's libged wrappers for various libraries.
 *
 */

#include "common.h"

#include "ged.h"

int
ged_mat_ae(struct ged *gedp,
	   int argc,
	   const char *argv[])
{
    mat_t o;
    double az, el;
    static const char *usage = "azimuth elevation";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc < 3 ||
	bu_sscanf(argv[1], "%lf", &az) != 1 ||
	bu_sscanf(argv[2], "%lf", &el) != 1) {
	bu_vls_printf(gedp->ged_result_str, "usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    bn_mat_ae(o, (fastf_t)az, (fastf_t)el);
    bn_encode_mat(gedp->ged_result_str, o, 1);

    return GED_OK;
}


int
ged_mat_mul(struct ged *gedp,
	    int argc,
	    const char *argv[])
{
    mat_t o, a, b;
    static const char *usage = "matA matB";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc < 3 || bn_decode_mat(a, argv[1]) < 16 || bn_decode_mat(b, argv[2]) < 16) {
	bu_vls_printf(gedp->ged_result_str, "usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    bn_mat_mul(o, a, b);
    bn_encode_mat(gedp->ged_result_str, o, 1);

    return GED_OK;
}


int
ged_mat4x3pnt(struct ged *gedp,
	      int argc,
	      const char *argv[])
{
    mat_t m = MAT_INIT_ZERO;
    point_t i, o;
    static const char *usage = "matrix point";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc < 3 || bn_decode_mat(m, argv[1]) < 16 ||
	bn_decode_vect(i, argv[2]) < 3) {
	bu_vls_printf(gedp->ged_result_str, "usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    MAT4X3PNT(o, m, i);
    bn_encode_vect(gedp->ged_result_str, o, 1);

    return GED_OK;
}


int
ged_mat_scale_about_pnt(struct ged *gedp,
		       int argc,
		       const char *argv[])
{
    mat_t o;
    vect_t v;
    double scale;
    static const char *usage = "point scale";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc < 3 || bn_decode_vect(v, argv[1]) < 3 ||
	bu_sscanf(argv[2], "%lf", &scale) != 1) {
	bu_vls_printf(gedp->ged_result_str, "usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (bn_mat_scale_about_pnt(o, v, scale) != 0) {
	bu_vls_printf(gedp->ged_result_str, "error performing calculation");
	return GED_ERROR;
    }

    bn_encode_mat(gedp->ged_result_str, o, 1);

    return GED_OK;
}
