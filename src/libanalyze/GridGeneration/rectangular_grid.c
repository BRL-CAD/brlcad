/*              R E C T A N G U L A R _ G R I D . C
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

#include "raytrace.h"
#include "vmath.h"

#include "analyze.h"

int rectangular_grid_generator(struct xray *ray, void *context)
{
    struct rectangular_grid *grid = (struct rectangular_grid*) context;
    int y_index, x_index;
    y_index = (int)(grid->current_point / (grid->x_points));
    x_index = (int)(grid->current_point - (y_index * (grid->x_points)));

    /* if refine flag is set then we skip for even values of
     * x_index and y_index in case of single grid
     */
    if (grid->refine_flag && grid->single_grid) {
	if (!(y_index&1) && !(x_index&1)) {
	    (grid->current_point++);
	    return(rectangular_grid_generator(ray, grid));
	}
    }

    /* if refine flag is set then we skip for odd values of
     * x_index and y_index in case of triple grid
     */
    if (grid->refine_flag && grid->single_grid == 0) {
	if (x_index&1 && y_index&1) {
	    grid->current_point++;
	    return (rectangular_grid_generator(ray, grid));
	}
    }
    /* reached end of grid generation */
    if (grid->current_point >= grid->total_points) {
	return 1;
    }

    /* set ray point */
    VJOIN2(ray->r_pt, grid->start_coord, x_index, grid->dx_grid, y_index, grid->dy_grid);

    /* set ray direction */
    VMOVE(ray->r_dir,grid->ray_direction);

    (grid->current_point)++;
    return 0;
}


double rectangular_grid_spacing(void *context)
{
    struct rectangular_grid *grid = (struct rectangular_grid *)context;
    return (grid->grid_spacing);
}


int rectangular_triple_grid_generator(struct xray *ray, void *context)
{
    struct rectangular_grid *grid = (struct rectangular_grid*) context;
    if (grid->current_point >= grid->total_points) {
	int i_axis, u_axis, v_axis, curr_view;
	vect_t u_dir, v_dir;
	if (grid->view == grid->max_views) return 1;
	curr_view = grid->view;
	i_axis = curr_view;
	u_axis = (curr_view+1) % 3;
	v_axis = (curr_view+2) % 3;

	u_dir[u_axis] = 1;
	u_dir[v_axis] = 0;
	u_dir[i_axis] = 0;

	v_dir[u_axis] = 0;
	v_dir[v_axis] = 1;
	v_dir[i_axis] = 0;

	grid->ray_direction[u_axis] = 0;
	grid->ray_direction[v_axis] = 0;
	grid->ray_direction[i_axis] = 1;

	VMOVE(grid->start_coord, grid->mdl_origin);
	grid->start_coord[u_axis] += grid->grid_spacing;
	grid->start_coord[v_axis] += grid->grid_spacing;

	VSCALE(grid->dx_grid, u_dir, grid->grid_spacing);
	VSCALE(grid->dy_grid, v_dir, grid->grid_spacing);

	grid->x_points = grid->steps[u_axis] - 1;
	grid->total_points = grid->x_points * (grid->steps[v_axis] - 1);
	(grid->view)++;
	grid->current_point = 0;
    }
    return (rectangular_grid_generator(ray, grid));
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
