/*                         P C A . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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

#include "common.h"

#include <math.h>

#include "bu/app.h"
#include "bu/log.h"
#include "bg/pca.h"


#define PCA_TEST_EPSILON 1.0e-9


static int
near_value(fastf_t a, fastf_t b)
{
    fastf_t scale = fmax(1.0, fmax(fabs(a), fabs(b)));

    return fabs(a - b) <= PCA_TEST_EPSILON * scale;
}


static int
test_box(void)
{
    point_t points[8];
    point_t center;
    vect_t xaxis;
    vect_t yaxis;
    vect_t zaxis;
    vect_t singular_values;
    size_t index = 0;

    for (int xsign = -1; xsign <= 1; xsign += 2) {
	for (int ysign = -1; ysign <= 1; ysign += 2) {
	    for (int zsign = -1; zsign <= 1; zsign += 2) {
		VSET(points[index], 4.0 + 3.0 * xsign,
		     -2.0 + 2.0 * ysign, 7.0 + zsign);
		index++;
	    }
	}
    }

    if (bg_pca_svd(&center, &xaxis, &yaxis, &zaxis, &singular_values,
	8, (const point_t *)points) != BRLCAD_OK) {
	bu_log("box PCA failed\n");
	return 1;
    }

    if (!near_value(center[X], 4.0) || !near_value(center[Y], -2.0) ||
	!near_value(center[Z], 7.0) ||
	!near_value(singular_values[X], 3.0 * sqrt(8.0)) ||
	!near_value(singular_values[Y], 2.0 * sqrt(8.0)) ||
	!near_value(singular_values[Z], sqrt(8.0)) ||
	!near_value(MAGNITUDE(xaxis), 1.0) ||
	!near_value(MAGNITUDE(yaxis), 1.0) ||
	!near_value(MAGNITUDE(zaxis), 1.0) ||
	!near_value(VDOT(xaxis, yaxis), 0.0) ||
	!near_value(VDOT(xaxis, zaxis), 0.0) ||
	!near_value(VDOT(yaxis, zaxis), 0.0)) {
	bu_log("box PCA returned incorrect center, frame, or singular values\n");
	return 1;
    }

    return 0;
}


static int
test_single_point(void)
{
    const point_t point = {3.0, -5.0, 8.0};
    point_t center;
    vect_t xaxis;
    vect_t yaxis;
    vect_t zaxis;
    vect_t singular_values;

    if (bg_pca_svd(&center, &xaxis, &yaxis, &zaxis, &singular_values,
	1, (const point_t *)&point) != BRLCAD_OK) {
	bu_log("single-point PCA failed\n");
	return 1;
    }

    if (!near_value(center[X], point[X]) || !near_value(center[Y], point[Y]) ||
	!near_value(center[Z], point[Z]) || !VNEAR_ZERO(singular_values, PCA_TEST_EPSILON) ||
	!near_value(MAGNITUDE(xaxis), 1.0) ||
	!near_value(MAGNITUDE(yaxis), 1.0) ||
	!near_value(MAGNITUDE(zaxis), 1.0)) {
	bu_log("single-point PCA returned invalid output\n");
	return 1;
    }

    return 0;
}


int
main(int UNUSED(argc), const char *argv[])
{
    point_t center;
    vect_t xaxis;
    vect_t yaxis;
    vect_t zaxis;

    bu_setprogname(argv[0]);

    if (test_box() || test_single_point())
	return 1;
    if (bg_pca_svd(&center, &xaxis, &yaxis, &zaxis, NULL, 0, NULL) != BRLCAD_ERROR) {
	bu_log("invalid PCA input was not rejected\n");
	return 1;
    }

    return 0;
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
