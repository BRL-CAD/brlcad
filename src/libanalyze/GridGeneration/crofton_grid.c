/*                C R O F T O N _ G R I D . C
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/**
 * @file crofton_grid.c
 *
 * Isotropic Cauchy-Crofton ray generator for libanalyze.
 *
 * Generates uniformly random isotropic lines through the bounding sphere of
 * the model.  These lines are used to estimate surface area and volume via the
 * Cauchy-Crofton formulae:
 *
 *   Surface area:  SA ≈ (4 * π * R²) * total_crossings / (2 * N)
 *
 *   Volume:        V  ≈ π * R² * mean_chord_length
 *
 * where R is the bounding sphere radius and N is the number of rays.
 *
 * Unlike the axis-aligned rectangular grid approach, isotropic sampling has
 * no directional bias and converges uniformly for any model orientation.
 *
 * The method implemented here follows:
 *
 *   Li, Xueqing et al. (2003). "Using low-discrepancy sequences and the
 *   Crofton formula to compute surface areas of geometric models."
 *   Computer-Aided Design 35, 771-782.
 *
 *   Liu, Yu-Shen et al. (2010). "Surface area estimation of digitized 3D
 *   objects using quasi-Monte Carlo methods." Pattern Recognition 43, 3900-3909.
 *
 * Ray generation:
 *   1. Pick a uniformly random direction ω on the unit sphere.
 *   2. Pick a uniformly random point p on the disk of radius R perpendicular
 *      to ω, centred at the model centre.
 *   3. Pull p back by R along ω so the ray origin is outside the sphere.
 *   4. The resulting ray (origin = p − R*ω, direction = ω) is an isotropic
 *      random line in 3-space; sampling this way gives the correct kinematic
 *      measure for the Crofton formula.
 */

#include "common.h"

#include <math.h>
#include <stdlib.h>
#include <time.h>

#include "vmath.h"
#include "bn/mat.h"
#include "raytrace.h"

#include "analyze.h"


void
crofton_grid_setup(struct crofton_grid *g,
		   const point_t mdl_min, const point_t mdl_max,
		   size_t n_rays, unsigned int seed)
{
    if (!g || !mdl_min || !mdl_max)
	return;

    /* Bounding sphere: centre at bbox centre, radius = half-diagonal */
    point_t center;
    VADD2SCALE(center, mdl_min, mdl_max, 0.5);
    VMOVE(g->center, center);

    vect_t diag;
    VSUB2(diag, mdl_max, mdl_min);
    g->radius = MAGNITUDE(diag) * 0.5;

    /* Pad radius slightly so rays never clip the bbox corner exactly */
    g->radius *= 1.01;

    g->n_rays  = n_rays;
    g->current = 0;
    g->seed    = seed ? seed : (unsigned int)time(NULL);
}


int
crofton_grid_generator(struct xray *ray, void *context)
{
    struct crofton_grid *g = (struct crofton_grid *)context;

    if (!g || !ray)
	return 1;

    if (g->current >= g->n_rays)
	return 1;

    /* --- Generate a uniformly random direction on the unit sphere ---
     *
     * Use the standard spherical-coordinates rejection method: pick
     * (cos θ, φ) uniformly.  Using rand_r for thread-safety.
     */
    {
	int rv0 = rand_r(&g->seed);
	int rv1 = rand_r(&g->seed);
	double cos_theta = 2.0 * ((double)rv0 / ((double)RAND_MAX + 1.0)) - 1.0;
	double sin_theta = sqrt(1.0 - cos_theta * cos_theta);
	double phi = 2.0 * M_PI * ((double)rv1 / ((double)RAND_MAX + 1.0));

	vect_t omega;
	omega[0] = sin_theta * cos(phi);
	omega[1] = sin_theta * sin(phi);
	omega[2] = cos_theta;
	/* omega is already a unit vector */

	/* --- Build two orthogonal vectors spanning the perpendicular disk --- */
	vect_t u_perp, v_perp;
	bn_vec_perp(u_perp, omega);
	VUNITIZE(u_perp);
	VCROSS(v_perp, omega, u_perp);
	VUNITIZE(v_perp);

	/* --- Pick a uniformly random point on the disk of radius R ---
	 *
	 * To sample uniformly on a disk, the radius must be sqrt-distributed:
	 *   r = R * sqrt(U)  where U ~ Uniform(0,1)
	 */
	{
	    int rv2 = rand_r(&g->seed);
	    int rv3 = rand_r(&g->seed);
	    double r_sq   = (double)rv2 / ((double)RAND_MAX + 1.0);
	    double disk_r   = g->radius * sqrt(r_sq);
	    double disk_phi = 2.0 * M_PI * ((double)rv3 / ((double)RAND_MAX + 1.0));

	    double pu = disk_r * cos(disk_phi);
	    double pv = disk_r * sin(disk_phi);

	    /* Point on the disk in 3-space (disk is at model centre) */
	    point_t disk_pt;
	    VJOIN2(disk_pt, g->center, pu, u_perp, pv, v_perp);

	    /* Pull back along -omega so the ray origin is outside the sphere */
	    VJOIN1(ray->r_pt, disk_pt, -g->radius, omega);
	    VMOVE(ray->r_dir, omega);
	}
    }

    g->current++;
    return 0;
}


double
crofton_surface_area(size_t crossings, size_t n_rays, double radius)
{
    if (n_rays == 0 || radius <= 0.0)
	return 0.0;

    /* Cauchy-Crofton formula:
     *
     *   SA = (4 * π * R²) * crossings / (2 * N)
     *
     * 'crossings' counts individual surface crossings (each entry or exit is
     * one crossing, so a single solid partition contributes 2).  Dividing by 2
     * turns this into "number of hit segments", and the overall factor
     * (4*π*R²) / N gives the mean projected area hit per ray, which equals
     * SA / (4*π*R²) for the sphere, scaling back to SA.
     */
    double sphere_area = 4.0 * M_PI * radius * radius;
    return sphere_area * (double)crossings / ((double)n_rays * 2.0);
}


double
crofton_volume(double chord_sum, size_t n_rays, double radius)
{
    if (n_rays == 0 || radius <= 0.0)
	return 0.0;

    /* Cauchy-Crofton formula for volume:
     *
     * For isotropic random lines through a bounding sphere of radius R,
     * the expected total chord length through the unit sphere per ray is 2R
     * (mean chord of a sphere = 4R/3 ... wait, actually: a uniformly random
     * chord through a sphere of radius R using the "random endpoint" method
     * has expected length 4R/3).  However here we sample uniformly over the
     * *disk* (the kinematically correct measure), so the expected chord
     * through the bounding sphere is π*R/2.
     *
     * For the *object* inside the sphere, by linearity of expectation:
     *
     *   E[chord_through_object] = V / (π * R²)
     *
     * This follows from the Cauchy-Crofton integral: the measure of lines
     * hitting a convex body with volume V and surface SA is π*V/2 (in the
     * kinematic density on lines in R³), and the total measure of lines
     * through the disk of radius R is π²*R²/2, so:
     *
     *   mean_chord = π*V / (π²*R²/2 * … )
     *
     * The simplest derivation: for our specific sampling scheme (uniform on
     * disk of radius R with uniform direction), the sample density is
     * 1/(π*R²) per unit area, so:
     *
     *   V ≈ π * R² * (chord_sum / n_rays)
     */
    double disk_area = M_PI * radius * radius;
    return disk_area * chord_sum / (double)n_rays;
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
