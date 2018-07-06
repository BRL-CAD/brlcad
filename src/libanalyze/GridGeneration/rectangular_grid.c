/*              R E C T A N G U L A R _ G R I D . C
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

#include "raytrace.h"
#include "vmath.h"

#include "analyze.h"


int rectangular_grid_generator(struct xray *ray, void *context)
{
    struct rectangular_grid *grid = (struct rectangular_grid*) context;
    int y_index,x_index;
    y_index = (int)(grid->current_point / (grid->y_points));
    x_index = (int)(grid->current_point - (y_index * (grid->y_points)));

    /* reached end of grid generation */
    if (grid->current_point == grid->total_points){
	return 1;
    }

    /* set ray point */
    VJOIN2(ray->r_pt, grid->start_coord, x_index, grid->dx_grid, y_index, grid->dy_grid);

    /* set ray direction */
    VMOVE(ray->r_dir,grid->ray_direction);

    (grid->current_point)++;
    return 0;
}

double rectangular_grid_cell_width(void *context)
{
    struct rectangular_grid *grid = (struct rectangular_grid *)context;
    return (grid->cell_width);
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
