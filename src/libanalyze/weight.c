/*                        W E I G H T . C
 * BRL-CAD
 *
 * Copyright (c) 2018 United States Government as represented by
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
/** @file weight.c
 *
 * Libanalyze utility to calculate the weight using generic
 * method.
 *
 */

#include "common.h"

#include "analyze.h"
#include "./analyze_private.h"


fastf_t
analyze_weight(struct current_state *state, const char *name)
{
    fastf_t weight;
    int i, view, obj = 0;

    for (i = 0; i < state->num_objects; i++) {
	if(!(bu_strcmp(state->objs[i].o_name, name))) {
	    obj = i;
	    break;
	}
    }

    weight = 0.0;
    for (view = 0; view < state->num_views; view++)
	weight += state->objs[obj].o_weight[view];

    weight /= state->num_views;
    return weight;
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
