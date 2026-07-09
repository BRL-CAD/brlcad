/*                          G R I D . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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


/**
 * Rotated grid sampler: a rectangular grid oriented along an arbitrary
 * ray direction (not necessarily aligned to X/Y/Z axes).  This eliminates
 * the directional bias that the standard triple-grid approach exhibits when
 * geometry happens to be axis-aligned.
 *
 * Call rotated_grid_setup() to initialise the structure before using the
 * generator function.
 */
struct rotated_grid {
    /* ray direction – unit vector, set by rotated_grid_setup() */
    vect_t ray_dir;

    /* grid plane basis vectors (unit, perpendicular to ray_dir) */
    vect_t u_dir;
    vect_t v_dir;

    /* grid origin and step vectors (scaled by grid_spacing) */
    point_t start;
    vect_t  u_step;
    vect_t  v_step;

    /* grid dimensions */
    size_t u_count;
    size_t v_count;
    size_t total;

    /* traversal state – must be reset to 0 between views */
    size_t current;

    fastf_t grid_spacing;

    /* when non-zero, skip points that were already shot in the previous
     * (coarser) grid pass – mirrors the refinement logic of rectangular_grid */
    int refine_flag;
};

/**
 * Initialise a rotated_grid from an explicit ray direction.
 *
 * @param g            grid structure to fill
 * @param mdl_min      model bounding-box minimum
 * @param mdl_max      model bounding-box maximum
 * @param ray_dir      unit ray direction (need not be normalised on input –
 *                     the function normalises it)
 * @param grid_spacing desired spacing between adjacent rays (mm)
 */
ANALYZE_EXPORT extern void
rotated_grid_setup(struct rotated_grid *g,
		   const point_t mdl_min, const point_t mdl_max,
		   const vect_t ray_dir, fastf_t grid_spacing);

/**
 * Initialise a rotated_grid from azimuth / elevation angles (degrees).
 * The ray fires in the direction given by az/el using the BRL-CAD convention
 * (same as bn_mat_angles).
 */
ANALYZE_EXPORT extern void
rotated_grid_setup_ae(struct rotated_grid *g,
		      const point_t mdl_min, const point_t mdl_max,
		      fastf_t azimuth_deg, fastf_t elevation_deg,
		      fastf_t grid_spacing);

/**
 * Ray generator for a rotated_grid.  Returns 0 while rays remain,
 * 1 when the grid is exhausted.
 */
ANALYZE_EXPORT extern int rotated_grid_generator(struct xray *rayp, void *grid_context);

/**
 * Return the grid spacing of a rotated_grid.
 */
ANALYZE_EXPORT extern double rotated_grid_spacing(void *grid_context);


/**
 * Crofton / Cauchy-Crofton isotropic sampler.
 *
 * Generates uniformly random isotropic lines through the bounding sphere of
 * the model.  Surface area is estimated via:
 *
 *     SA ≈ (4 * π * R²) * total_crossings / (2 * N)
 *
 * where R is the bounding-sphere radius, total_crossings is the total number
 * of surface entry/exit events across all N rays.
 *
 * Volume is estimated via:
 *
 *     V ≈ (4/3 * π * R³) * sum_of_chord_lengths / (2 * R * N)
 *       = (2 * π * R²) * sum_of_chord_lengths / N
 *
 * because a uniformly random chord through a sphere of radius R has expected
 * length R (Bertrand), so the correct estimator is:
 *
 *     V ≈ π * R² * mean_chord_length   (Cauchy-Crofton for volume)
 *
 * Call crofton_grid_setup() before using the generator.
 */
struct crofton_grid {
    point_t center;   /* bounding sphere centre */
    double  radius;   /* bounding sphere radius */
    size_t  n_rays;   /* total rays to generate */
    size_t  current;  /* rays generated so far */
    unsigned int seed; /* PRNG seed – set for reproducibility, 0 = time-based */
};

/**
 * Initialise a crofton_grid from the model bounding box.
 *
 * @param g        grid to initialise
 * @param mdl_min  model bounding-box minimum
 * @param mdl_max  model bounding-box maximum
 * @param n_rays   number of random rays to generate
 * @param seed     PRNG seed (0 = use current time)
 */
ANALYZE_EXPORT extern void
crofton_grid_setup(struct crofton_grid *g,
		   const point_t mdl_min, const point_t mdl_max,
		   size_t n_rays, unsigned int seed);

/**
 * Ray generator for a crofton_grid.  Returns 0 while rays remain,
 * 1 when all n_rays have been generated.
 */
ANALYZE_EXPORT extern int crofton_grid_generator(struct xray *rayp, void *grid_context);

/**
 * Apply the Cauchy-Crofton surface-area formula.
 *
 * @param crossings  total number of surface entry/exit events seen
 * @param n_rays     total number of rays fired
 * @param radius     bounding sphere radius used during sampling
 * @return estimated surface area in mm²
 */
ANALYZE_EXPORT extern double
crofton_surface_area(size_t crossings, size_t n_rays, double radius);

/**
 * Apply the Cauchy-Crofton volume formula.
 *
 * @param chord_sum  sum of all solid chord lengths (entry-to-exit distances)
 *                   across all partitions from all rays, in mm
 * @param n_rays     total number of rays fired
 * @param radius     bounding sphere radius used during sampling
 * @return estimated volume in mm³
 */
ANALYZE_EXPORT extern double
crofton_volume(double chord_sum, size_t n_rays, double radius);


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
