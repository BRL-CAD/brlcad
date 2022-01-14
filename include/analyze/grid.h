/*                          G R I D . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
/** @addtogroup libanalyze
 *
 */
/** @{ */
/** @file analyze/grid.h */

#ifndef ANALYZE_GRID_H
#define ANALYZE_GRID_H

#include "common.h"
#include "vmath.h"
#include "bu/opt.h"
#include "bu/vls.h"
#include "raytrace.h"

#include "analyze/defines.h"

__BEGIN_DECLS

/*
 *     Grid specific structures
 */

/**
 * This structure acts a function pointer table for grid generating functions
 */

struct grid_generator_functions {
    int (*next_ray)(struct xray *rayp, void *grid_context);
    double (*grid_spacing)(void *grid_context);
};

/**
 * This structure is the context passed to the grid generating functions for
 * the rectangular grid type.
 */

struct rectangular_grid {
    int view;
    int max_views;
    int single_grid;
    point_t mdl_origin;
    long steps[3];
    int refine_flag;

    vect_t ray_direction;
    point_t start_coord;
    vect_t dx_grid;
    vect_t dy_grid;
    size_t x_points;
    fastf_t grid_spacing;
    size_t current_point;
    size_t total_points;
};

/**
 * grid generator for rectangular grid type
 */
ANALYZE_EXPORT extern int rectangular_grid_generator(struct xray *rayp, void *grid_context);

/**
 * grid generator for rectangular triple grid type
 */
ANALYZE_EXPORT extern int rectangular_triple_grid_generator(struct xray *rayp, void *grid_context);

/**
 * function to get the grid spacing of rectangular grid
 */
ANALYZE_EXPORT extern double rectangular_grid_spacing(void *grid_context);


__END_DECLS

#endif /* ANALYZE_GRID_H */

/** @} */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
