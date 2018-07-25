/*                 C H E C K _ O P T I O N S. C
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

#include "common.h"

#include "bu/malloc.h"

#include "analyze.h"
#include "./analyze_private.h"

struct current_state *
analyze_current_state_init()
{
    struct current_state *state;
    BU_ALLOC(state, struct current_state);

    state->num_objects = 0;
    state->num_views = 3;
    state->densityFileName = NULL;
    state->volume_tolerance = -1;
    state->mass_tolerance = -1;
    state->sa_tolerance = -1;
    state->azimuth_deg = 35.0;
    state->elevation_deg = 25.0;
    state->gridSpacing = 50.0;
    state->gridSpacingLimit = 0.5;
    state->use_air = 1;
    state->required_number_hits = 1;
    state->ncpu = (int) bu_avail_cpus();
    state->use_single_grid = 0;
    state->debug = 0;
    state->verbose = 0;
    state->plot_volume = NULL;

    state->exp_air_callback = NULL;
    state->exp_air_callback_data = NULL;

    state->overlaps_callback = NULL;
    state->overlaps_callback_data = NULL;

    state->gaps_callback = NULL;
    state->gaps_callback_data = NULL;

    state->adj_air_callback = NULL;
    state->adj_air_callback_data = NULL;

    state->log_str = bu_vls_vlsinit();

    return state;
}


void
analyze_free_current_state(struct current_state *state)
{
    int i;

    if (state == NULL)
	return;

    bu_vls_free(state->log_str);

    if (state->densities != NULL) {
	for (i = 0; i < state->num_densities; ++i) {
	    if (state->densities[i].name != 0)
		bu_free(state->densities[i].name, "density name");
	}

	bu_free(state->densities, "densities");
	state->densities = NULL;
	state->num_densities = 0;
    }

    if (state->shots != NULL) {
	bu_free(state->shots, "m_shots");
	state->shots = NULL;
    }

    if (state->objs != NULL) {
	for (i = 0; i < state->num_objects; i++) {
	    bu_free(state->objs[i].o_len, "o_len");
	    bu_free(state->objs[i].o_lenDensity, "o_lenDensity");
	    bu_free(state->objs[i].o_volume, "o_volume");
	    bu_free(state->objs[i].o_mass, "o_mass");
	    bu_free(state->objs[i].o_lenTorque, "o_lenTorque");
	    bu_free(state->objs[i].o_moi, "o_moi");
	    bu_free(state->objs[i].o_poi, "o_poi");
	}
	bu_free(state->objs, "object table");
	state->objs = NULL;
    }

    if (state->reg_tbl != NULL) {
	for (i = 0; i < state->num_regions; i++) {
	    bu_free(state->reg_tbl[i].r_name, "r_name");
	    bu_free(state->reg_tbl[i].r_lenDensity, "r_lenDensity");
	    bu_free(state->reg_tbl[i].r_len, "r_len");
	    bu_free(state->reg_tbl[i].r_volume, "r_volume");
	    bu_free(state->reg_tbl[i].r_mass, "r_mass");
	}
	bu_free(state->reg_tbl, "object table");
	state->reg_tbl = NULL;
    }

    bu_free((char *)state, "struct current_state");
}


void analyze_set_azimuth(struct current_state *state, fastf_t azimuth)
{
    state->azimuth_deg = azimuth;
    state->use_single_grid = 1;
}


void analyze_set_elevation(struct current_state *state, fastf_t elevation)
{
    state->elevation_deg = elevation;
    state->use_single_grid = 1;
}


void analyze_set_grid_spacing(struct current_state *state, fastf_t gridSpacing, fastf_t gridSpacingLimit)
{
    state->gridSpacing = gridSpacing;
    state->gridSpacingLimit = gridSpacingLimit;
}


void analyze_set_overlap_tolerance(struct current_state *state, fastf_t overlap_tolerance)
{
    state->overlap_tolerance = overlap_tolerance;
}


void analyze_set_volume_tolerance(struct current_state *state, fastf_t volume_tolerance)
{
    state->volume_tolerance = volume_tolerance;
}


void analyze_set_mass_tolerance(struct current_state *state, fastf_t mass_tolerance)
{
    state->mass_tolerance = mass_tolerance;
}


void analyze_set_surf_area_tolerance(struct current_state *state, fastf_t sa_tolerance)
{
    state->sa_tolerance = sa_tolerance;
}


void analyze_set_ncpu(struct current_state *state, int ncpu)
{
    state->ncpu = ncpu;
}


void analyze_set_required_number_hits(struct current_state *state, int required_number_hits)
{
    state->required_number_hits = required_number_hits;
}


void analyze_set_use_air(struct current_state *state, int use_air)
{
    state->use_air = use_air;
}


void analyze_set_num_views(struct current_state *state, int num_views)
{
    state->num_views = num_views;
}


void analyze_set_densityfile(struct current_state *state, char *densityFileName)
{
    state->densityFileName = densityFileName;
}


void analyze_register_overlaps_callback(struct current_state *state, overlap_callback_t callback_function, void* callback_data)
{
    state->overlaps_callback = callback_function;
    state->overlaps_callback_data = callback_data;
}


void analyze_register_exp_air_callback(struct current_state *state, exp_air_callback_t callback_function, void* callback_data)
{
    state->exp_air_callback = callback_function;
    state->exp_air_callback_data = callback_data;
}


void analyze_register_gaps_callback(struct current_state *state, gaps_callback_t callback_function, void* callback_data)
{
    state->gaps_callback = callback_function;
    state->gaps_callback_data = callback_data;
}


void analyze_register_adj_air_callback(struct current_state *state, adj_air_callback_t callback_function, void* callback_data)
{
    state->adj_air_callback = callback_function;
    state->adj_air_callback_data = callback_data;
}


void analyze_set_volume_plotfile(struct current_state *state, FILE* plot_volume)
{
    state->plot_volume = plot_volume;
}


void analyze_enable_debug(struct current_state *state, struct bu_vls *vls)
{
    state->debug = 1;
    state->debug_str = vls;
}


void analyze_enable_verbose(struct current_state *state, struct bu_vls *vls)
{
    state->verbose = 1;
    state->verbose_str = vls;
}


int analyze_get_num_regions(struct current_state *state)
{
    return (state->num_regions);
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
