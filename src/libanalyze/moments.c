/*                      M O M E N T S . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2021 United States Government as represented by
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
/** @file moments.c
 *
 * Libanalyze utility to calculate the moments and products of inertia
 * using generic method.
 *
 */

#include "common.h"
#include "vmath.h"

#include "analyze.h"
#include "./analyze_private.h"

void
analyze_moments(struct current_state *state, const char *name, mat_t moments)
{
    int view = 0;
    int obj = 0;
    int i;
    fastf_t Dx_sq, Dy_sq, Dz_sq;
    double avg_mass = analyze_mass(state, name);
    point_t centroid;
    analyze_centroid(state, name, centroid);
    MAT_ZERO(moments);

    for (i = 0; i < state->num_objects; i++) {
	if(!(bu_strcmp(state->objs[i].o_name, name))) {
	    obj = i;
	    break;
	}
    }

    for (view=0; view < state->num_views; view++) {
	vectp_t moi = &state->objs[obj].o_moi[view*3];
	vectp_t poi = &state->objs[obj].o_poi[view*3];

	moments[MSX] += moi[X];
	moments[MSY] += moi[Y];
	moments[MSZ] += moi[Z];
	moments[1] += poi[X];
	moments[2] += poi[Y];
	moments[6] += poi[Z];
    }

    moments[MSX] /= (fastf_t)state->num_views;
    moments[MSY] /= (fastf_t)state->num_views;
    moments[MSZ] /= (fastf_t)state->num_views;
    moments[1] /= (fastf_t)state->num_views;
    moments[2] /= (fastf_t)state->num_views;
    moments[6] /= (fastf_t)state->num_views;

    /* Lastly, apply the parallel axis theorem */
    Dx_sq = centroid[X] * centroid[X];
    Dy_sq = centroid[Y] * centroid[Y];
    Dz_sq = centroid[Z] * centroid[Z];
    moments[MSX] -= avg_mass * (Dy_sq + Dz_sq);
    moments[MSY] -= avg_mass * (Dx_sq + Dz_sq);
    moments[MSZ] -= avg_mass * (Dx_sq + Dy_sq);
    moments[1] += avg_mass * centroid[X] * centroid[Y];
    moments[2] += avg_mass * centroid[X] * centroid[Z];
    moments[6] += avg_mass * centroid[Y] * centroid[Z];

    moments[4] = moments[1];
    moments[8] = moments[2];
    moments[9] = moments[6];
}


void
analyze_moments_total(struct current_state *state, mat_t moments)
{
    int view;
    fastf_t Dx_sq, Dy_sq, Dz_sq;
    double avg_mass = analyze_total_mass(state);
    point_t centroid;
    analyze_total_centroid(state, centroid);
    MAT_ZERO(moments);

    for (view=0; view < state->num_views; view++) {
	vectp_t moi = &state->m_moi[view*3];
	vectp_t poi = &state->m_poi[view*3];

	moments[MSX] += moi[X];
	moments[MSY] += moi[Y];
	moments[MSZ] += moi[Z];
	moments[1] += poi[X];
	moments[2] += poi[Y];
	moments[6] += poi[Z];
    }

    moments[MSX] /= (fastf_t)state->num_views;
    moments[MSY] /= (fastf_t)state->num_views;
    moments[MSZ] /= (fastf_t)state->num_views;
    moments[1] /= (fastf_t)state->num_views;
    moments[2] /= (fastf_t)state->num_views;
    moments[6] /= (fastf_t)state->num_views;

    /* Lastly, apply the parallel axis theorem */
    Dx_sq = centroid[X] * centroid[X];
    Dy_sq = centroid[Y] * centroid[Y];
    Dz_sq = centroid[Z] * centroid[Z];
    moments[MSX] -= avg_mass * (Dy_sq + Dz_sq);
    moments[MSY] -= avg_mass * (Dx_sq + Dz_sq);
    moments[MSZ] -= avg_mass * (Dx_sq + Dy_sq);
    moments[1] += avg_mass * centroid[X] * centroid[Y];
    moments[2] += avg_mass * centroid[X] * centroid[Z];
    moments[6] += avg_mass * centroid[Y] * centroid[Z];

    moments[4] = moments[1];
    moments[8] = moments[2];
    moments[9] = moments[6];
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
