/*                        M A S S . C
 * BRL-CAD
 *
 * Copyright (c) 2018-2022 United States Government as represented by
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
/** @file mass.c
 *
 * Libanalyze utility to calculate the mass using generic
 * method.
 *
 */

#include "common.h"

#include "analyze.h"
#include "./analyze_private.h"


fastf_t
analyze_mass(struct current_state *state, const char *name)
{
    fastf_t mass;
    int i, view, obj = 0;

    for (i = 0; i < state->num_objects; i++) {
	if(!(bu_strcmp(state->objs[i].o_name, name))) {
	    obj = i;
	    break;
	}
    }

    mass = 0.0;
    for (view = 0; view < state->num_views; view++)
	mass += state->objs[obj].o_mass[view];

    mass /= state->num_views;
    return mass;
}


void
analyze_mass_region(struct current_state *state, int i, char **name, double *mass, double *high, double *low)
{
    double *mm;
    double lo = INFINITY;
    double hi = -INFINITY;
    double avg_mass = 0.0;
    int view;

    for (view=0; view < state->num_views; view++) {
	mm = &state->reg_tbl[i].r_mass[view];

	/* convert view length to a mass */
	*mm = state->reg_tbl[i].r_lenDensity[view] * (state->area[view] / state->shots[view]);

	/* find limits of values */
	if (*mm < lo) lo = *mm;
	if (*mm > hi) hi = *mm;

	avg_mass += *mm;
    }

    avg_mass /= state->num_views;

    /* set values to the pointers */
    *mass = avg_mass;
    *name = state->reg_tbl[i].r_name;
    *high = hi - avg_mass;
    *low = avg_mass - lo;
}


fastf_t
analyze_total_mass(struct current_state *state)
{
    int view;
    double avg_mass = 0.0;
    for (view=0; view < state->num_views; view++) {
	avg_mass += state->m_lenDensity[view] * (state->area[view] / state->shots[view]);
    }
    avg_mass /= state->num_views;
    return avg_mass;
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
