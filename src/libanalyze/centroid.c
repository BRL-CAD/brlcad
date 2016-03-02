/*                      C E N T R O I D . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2016 United States Government as represented by
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
/** @file centroid.c
 *
 * Libanalyze utility to calculate the centroid using generic
 * method.
 *
 */

/* BRL-CAD includes */
#include "common.h"
#include "raytrace.h"			/* For interfacing to librt */
#include "vmath.h"			/* Vector Math macros */
#include "ged.h"
#include "rt/geom.h"
#include "bu/parallel.h"
#include "analyze.h"

/* System headers */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

struct per_obj_data {
    char *o_name;
    double *o_len;
    double *o_lenDensity;
    double *o_volume;
    double *o_weight;
    double *o_surf_area;
    fastf_t *o_lenTorque; /* torque vector for each view */
};

struct cvt_tab {
    double val;
    char name[32];
};


point_t *
analyze_centroid(struct raytracing_context *context, const char *name)
{
    point_t *cent;
    int i, view, obj = 0;
    double avg_mass;

    for (i = 0; i < context->num_objects; i++) {
	if(!(bu_strcmp(context->objs[i].o_name, name))) {
	    obj = i;
	    break;
	}
    }

    BU_GET(cent, point_t);
    avg_mass = 0.0;
    for (view = 0; view < context->num_views; view++)
	avg_mass += context->objs[obj].o_weight[view];

    avg_mass /= context->num_views;
    if (!ZERO(avg_mass)) {
	point_t centroid;
	fastf_t inv_total_mass = 1.0/avg_mass;

	VSETALL(centroid, 0.0);
	for (view = 0; view < context->num_views; view++) {
	    vect_t torque;
	    fastf_t cell_area = context->area[view] / context->shots[view];

	    VSCALE(torque, &context->objs[obj].o_lenTorque[view*3], cell_area);
	    VADD2(centroid, centroid, torque);
	}

	VSCALE(centroid, centroid, 1.0/(fastf_t)context->num_views);
	VSCALE(centroid, centroid, inv_total_mass);
	(*cent)[0] = centroid[X];
	(*cent)[1] = centroid[Y];
	(*cent)[2] = centroid[Z];
    }
    return cent;
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
