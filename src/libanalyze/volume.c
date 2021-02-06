/*                        V O L U M E . C
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
/** @file volume.c
 *
 * Libanalyze utility to calculate the volume using generic
 * method.
 *
 */

#include "common.h"

#include "analyze.h"
#include "./analyze_private.h"


fastf_t
analyze_volume(struct current_state *state, const char *name)
{
    fastf_t volume;
    int i, view, obj = 0;

    for (i = 0; i < state->num_objects; i++) {
	if(!(bu_strcmp(state->objs[i].o_name, name))) {
	    obj = i;
	    break;
	}
    }

    volume = 0.0;
    for (view = 0; view < state->num_views; view++)
	volume += state->objs[obj].o_volume[view];

    volume /= state->num_views;
    return volume;
}


void
analyze_volume_region(struct current_state *state, int i, char **name, double *volume, double *high, double *low)
{
    double *vv;
    double lo = INFINITY;
    double hi = -INFINITY;
    double avg_vol = 0.0;
    int view;

    for (view=0; view < state->num_views; view++) {
	vv = &state->reg_tbl[i].r_volume[view];

	/* convert view length to a volume */
	*vv = state->reg_tbl[i].r_len[view] * (state->area[view] / state->shots[view]);

	/* find limits of values */
	if (*vv < lo) lo = *vv;
	if (*vv > hi) hi = *vv;

	avg_vol += *vv;
    }

    avg_vol /= state->num_views;

    /* set values to the pointers */
    *volume = avg_vol;
    *name = state->reg_tbl[i].r_name;
    *high = hi - avg_vol;
    *low = avg_vol - lo;
}


fastf_t
analyze_total_volume(struct current_state *state)
{
    int view;
    double avg_vol = 0.0;
    for (view=0; view < state->num_views; view++) {
	avg_vol += state->m_len[view] * (state->area[view] / state->shots[view]);
    }
    avg_vol /= state->num_views;
    return avg_vol;
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
