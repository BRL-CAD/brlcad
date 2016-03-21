/*                     S U R F _ A R E A . C
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
/** @file surf_area.c
 *
 * Libanalyze utility to calculate the surface area using generic
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

/* this table keeps track of the "current" or "user selected units and
 * the associated conversion values
 */
#define LINE 0
#define VOL 1
#define WGT 2
static const struct cvt_tab units[3][3] = {
    {{1.0, "mm"}},	/* linear */
    {{1.0, "cu mm"}},	/* volume */
    {{1.0, "grams"}}	/* weight */
};

fastf_t
analyze_surf_area(struct raytracing_context *context, const char *name)
{
    fastf_t area;
    int i, view, obj = 0;
    double avg_area;

    for (i = 0; i < context->num_objects; i++) {
	if(!(bu_strcmp(context->objs[i].o_name, name))) {
	    obj = i;
	    break;
	}
    }

    avg_area = 0.0;
    for (view = 0; view < context->num_views; view++)
	avg_area += context->objs[obj].o_surf_area[view];

    avg_area /= context->num_views;
    area = avg_area / units[VOL]->val;
    return area;
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
