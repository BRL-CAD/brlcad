/*                 C H E C K _ O P T I O N S. C
 * BRL-CAD
 *
 * Copyright (c) 2018-2021 United States Government as represented by
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

/* 
 * initializes the current state with meaningful defaults for raytracing.
 */
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
    state->gridRatio = 1;
    state->aspect = 1;
    state->use_air = 1;
    state->required_number_hits = 1;
    state->ncpu = (int) bu_avail_cpus();
    state->use_single_grid = 0;
    state->use_view_information = 0;
    state->debug = 0;
    state->verbose = 0;
    state->quiet_missed_report = 0;
    state->plot_volume = NULL;
    state->default_den = 0;
    state->analysis_flags = 0;
    state->samples_per_model_axis = 2.0;
    state->aborted = 0;
    state->grid_size_flag = 0;

    state->exp_air_callback = NULL;
    state->exp_air_callback_data = NULL;

    state->overlaps_callback = NULL;
    state->overlaps_callback_data = NULL;

    state->gaps_callback = NULL;
    state->gaps_callback_data = NULL;

    state->adj_air_callback = NULL;
    state->adj_air_callback_data = NULL;

    state->first_air_callback = NULL;
    state->first_air_callback_data = NULL;

    state->last_air_callback = NULL;
    state->last_air_callback_data = NULL;

    state->unconf_air_callback = NULL;
    state->unconf_air_callback_data = NULL;

    state->log_str = bu_vls_vlsinit();

    return state;
}

/* 
 * frees dynamically allocated memory for current state while raytracing.
 */
void
analyze_free_current_state(struct current_state *state)
{
    int i;

    if (state == NULL)
	return;

    bu_vls_free(state->log_str);

    if (state->densities != NULL) {
	analyze_densities_destroy(state->densities);	
	state->densities = NULL;
    }

    if (state->m_lenTorque != NULL)
	bu_free(state->m_lenTorque, "m_lenTorque");

    if (state->m_moi != NULL)
	bu_free(state->m_moi, "m_moi");

    if (state->m_poi != NULL)
	bu_free(state->m_poi, "m_poi");

    if (state->m_lenDensity != NULL)
	bu_free(state->m_lenDensity, "m_lenDensity");

    if (state->m_len != NULL)
	bu_free(state->m_len, "m_len");

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
	    bu_free(state->objs[i].o_area, "o_area");
	    bu_free(state->objs[i].o_surf_area, "o_surf_area");
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
	    bu_free(state->reg_tbl[i].r_area, "r_area");
	    bu_free(state->reg_tbl[i].r_surf_area, "r_surf_area");
	}
	bu_free(state->reg_tbl, "object table");
	state->reg_tbl = NULL;
    }

    bu_free((char *)state, "struct current_state");
}

/*
 * sets the azimuth angle, and forces single grid to shoot rays.
 */
void
analyze_set_azimuth(struct current_state *state, fastf_t azimuth)
{
    state->azimuth_deg = azimuth;
    state->use_single_grid = 1;
}

/*
 * sets the elevation angle, and forces single grid to shoot rays.
 */
void
analyze_set_elevation(struct current_state *state, fastf_t elevation)
{
    state->elevation_deg = elevation;
    state->use_single_grid = 1;
}

/*
 * sets the grid_spacing and a limit on refining the grid_spacing.
 */
void
analyze_set_grid_spacing(struct current_state *state, fastf_t gridSpacing, fastf_t gridSpacingLimit)
{
    state->gridSpacing = gridSpacing;
    state->gridSpacingLimit = gridSpacingLimit;
}

/*
 * returns the grid_spacing when the raytracing stopped -- used for printing summaries
 */
fastf_t
analyze_get_grid_spacing(struct current_state *state)
{
    return state->gridSpacing;
}

/*
 * sets the cell_width by cell_height ratio (default is 1).
 * used when we want rectangular grid cells.
 */
void
analyze_set_grid_ratio(struct current_state *state, fastf_t gridRatio)
{
    state->gridRatio = gridRatio;
}

/*
 * sets the grid width and grid height values. A flag is set which
 * calculates the grid_spacing for the new grid size from the viewsize value.
 */
void
analyze_set_grid_size(struct current_state *state, fastf_t width, fastf_t height)
{
    state->grid_size_flag = 1;
    state->grid_height = height;
    state->grid_width = width;
}

/*
 * sets the width by height ratio (default is 1).
 * used for certain applications when the grid should be formed rectangular
 */
void
analyze_set_aspect(struct current_state *state, fastf_t aspect)
{
    state->aspect = aspect;
}

/*
 * used for the specifying minimum samples per model axis.
 */
void
analyze_set_samples_per_model_axis(struct current_state *state, fastf_t samples_per_model_axis)
{
    state->samples_per_model_axis = samples_per_model_axis;
}

/*
 * sets tolerance for different analysis options -- overlaps, volume, mass and
 * surface area
 */
void
analyze_set_overlap_tolerance(struct current_state *state, fastf_t overlap_tolerance)
{
    state->overlap_tolerance = overlap_tolerance;
}

void
analyze_set_volume_tolerance(struct current_state *state, fastf_t volume_tolerance)
{
    state->volume_tolerance = volume_tolerance;
}

void
analyze_set_mass_tolerance(struct current_state *state, fastf_t mass_tolerance)
{
    state->mass_tolerance = mass_tolerance;
}

void
analyze_set_surf_area_tolerance(struct current_state *state, fastf_t sa_tolerance)
{
    state->sa_tolerance = sa_tolerance;
}


/*
 * sets the number of CPUs to be used for raytracing.
 */
void
analyze_set_ncpu(struct current_state *state, int ncpu)
{
    state->ncpu = ncpu;
}

/*
 * sets the minimum number of required hits. Default is 1.
 */
void
analyze_set_required_number_hits(struct current_state *state, size_t required_number_hits)
{
    state->required_number_hits = required_number_hits;
}

/*
 * when set, reports when regions are missed while raytracing are not printed
 */
void
analyze_set_quiet_missed_report(struct current_state *state)
{
    state->quiet_missed_report = 1;
}

/*
 * Specifies the Boolean value (0 or 1) for use_air.
 * which indicates whether regions which are marked as "air" should be
 * retained and included in the raytrace.
 */
void
analyze_set_use_air(struct current_state *state, int use_air)
{
    state->use_air = use_air;
}

/*
 * sets the number of views for triple grid, a debugging option.
 */
void
analyze_set_num_views(struct current_state *state, int num_views)
{
    state->num_views = num_views;
}

/*
 * Specifies that density values should be taken from an external file
 * instead of from the _DENSITIES object in the database.
 */
void
analyze_set_densityfile(struct current_state *state, char *densityFileName)
{
    state->densityFileName = densityFileName;
}

/*
 * registers the callback functions defined by the user to be called when raytracing
 */
void
analyze_register_overlaps_callback(struct current_state *state, overlap_callback_t callback_function, void* callback_data)
{
    state->overlaps_callback = callback_function;
    state->overlaps_callback_data = callback_data;
}

void
analyze_register_exp_air_callback(struct current_state *state, exp_air_callback_t callback_function, void* callback_data)
{
    state->exp_air_callback = callback_function;
    state->exp_air_callback_data = callback_data;
}

void
analyze_register_gaps_callback(struct current_state *state, gaps_callback_t callback_function, void* callback_data)
{
    state->gaps_callback = callback_function;
    state->gaps_callback_data = callback_data;
}

void
analyze_register_adj_air_callback(struct current_state *state, adj_air_callback_t callback_function, void* callback_data)
{
    state->adj_air_callback = callback_function;
    state->adj_air_callback_data = callback_data;
}

void
analyze_register_first_air_callback(struct current_state *state, first_air_callback_t callback_function, void* callback_data)
{
    state->first_air_callback = callback_function;
    state->first_air_callback_data = callback_data;
}

void
analyze_register_last_air_callback(struct current_state *state, last_air_callback_t callback_function, void* callback_data)
{
    state->last_air_callback = callback_function;
    state->last_air_callback_data = callback_data;
}

void
analyze_register_unconf_air_callback(struct current_state *state, unconf_air_callback_t callback_function, void* callback_data)
{
    state->unconf_air_callback = callback_function;
    state->unconf_air_callback_data = callback_data;
}


void
analyze_set_volume_plotfile(struct current_state *state, FILE* plot_volume)
{
    state->plot_volume = plot_volume;
}


void
analyze_enable_debug(struct current_state *state, struct bu_vls *vls)
{
    state->debug = 1;
    state->debug_str = vls;
}


void
analyze_enable_verbose(struct current_state *state, struct bu_vls *vls)
{
    state->verbose = 1;
    state->verbose_str = vls;
}

/*
 * returns the number of regions for the objects that were
 * mentioned while raytracing.
 */
int
analyze_get_num_regions(struct current_state *state)
{
    return (state->num_regions);
}

/*
 * sets the view information by explicitly mentioning the viewsize, eyemodel and
 * orientation matrix.
 */
void
analyze_set_view_information(struct current_state *state, double viewsize, point_t *eye_model, quat_t *orientation)
{
    VMOVE(state->eye_model, *eye_model);
    state->viewsize = viewsize;
    quat_quat2mat(state->Viewrotscale, *orientation);
    state->use_view_information = 1;
    state->use_single_grid = 1;
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
