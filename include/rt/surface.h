/*                        S U R F A C E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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

/** @file rt/surface.h
 * Surface attributes and weighted sampling of prepared Boolean boundaries.
 */
#ifndef RT_SURFACE_H
#define RT_SURFACE_H

#include <stddef.h>

#include "vmath.h"
#include "rt/defines.h"
#include "rt/hit.h"

__BEGIN_DECLS

struct application;
struct region;
struct resource;
struct rt_i;
struct soltab;

/** A post-Boolean surface evaluation, in prepared geometry coordinates. */
struct rt_surface_attributes {
    point_t point;
    vect_t geometric_normal;
    vect_t shading_normal;
    fastf_t uv[2];
    struct rt_surface_derivatives derivatives;
    int uv_available;
    int derivatives_available;
};

/**
 * Evaluate a native primitive hit without modifying the hit or geometry.
 * Apply the supplied Boolean flip to normals and normal derivatives only.
 * Missing UV callbacks and singular or unavailable derivatives have availability zero
 * and zero values; failure to evaluate available data is an error.
 * All pointers are required.  Return 0 on success, -1 on failure, preserving
 * the output on failure.  Input and output storage must not overlap.
 */
RT_EXPORT extern int rt_obj_surface_attributes(struct rt_surface_attributes *out,
    struct application *ap, struct soltab *solid, const struct hit *hit, int flip);

/** Initialize using rt_surface_sampler_init(); do not modify fields. */
struct rt_surface_sampler {
    struct rt_i *rtip;
    point_t center;
    double radius;
    double area_scale; /**< 2*pi*radius^2, the inverse crossing intensity. */
};

struct rt_surface_sample {
    struct rt_surface_attributes surface;
    const struct soltab *solid;
    const struct region *region;
    int surface_number;
    int flipped;
    size_t crossing_count;
    double area_weight; /**< Square database units, not an inverse PDF. */
};

#define RT_SURFACE_SAMPLE_DIMENSIONS 5

enum rt_surface_sample_status {
    RT_SURFACE_SAMPLE_ERROR = -1,
    RT_SURFACE_SAMPLE_MISS = 0,
    RT_SURFACE_SAMPLE_HIT = 1
};

/**
 * Initialize an immutable sampler of one prepared region whose Boolean tree
 * has a finite conservative bound.  Geometry and rt_i must remain unchanged
 * and alive while sampling.  The same
 * sampler can be shared by threads with distinct registered resources.
 * Ray-dependent plate BOTs require conversion to explicit volumes first.
 * Return 0 on success, -1 on invalid, unprepared, unbounded, plate-mode or
 * multi-region input;
 * leave the output unchanged on failure.
 */
RT_EXPORT extern int rt_surface_sampler_init(struct rt_surface_sampler *out,
    struct rt_i *rtip);

/**
 * Draw an oriented line from two independent uniform bounding-sphere points,
 * then select one of its post-Boolean boundary crossings uniformly.  Supply
 * five independent variates in [0,1): four for the line, one for selection.
 * There is no retry on a missed or degenerate line.  An exactly coincident
 * entry/exit pair represents one zero-thickness surface crossing.
 *
 * This is a weighted surface estimator, not a uniform one-point PDF.  Let C
 * be crossing_count and K=area_scale=2*pi*radius^2.  The returned weight is
 * K*C.  For any integrable surface function f, averaging area_weight*f(point)
 * over ALL draws, counting misses as zero, estimates its surface integral.
 * Conditional on a line, C*f(selected) has expectation sum_i f(crossing_i);
 * the Cauchy-Crofton crossing intensity is 1/K per unit area.  Neither a
 * measured surface area nor a precomputed point pool enters this weight.
 * Do not normalize by successful draws, discard C, or treat 1/area_weight
 * as the marginal PDF for multiple importance sampling.
 *
 * Return HIT, MISS, or ERROR.  The output is changed only on HIT.  Missing
 * surface data is represented by availability flags; invalid evaluated data
 * is ERROR, not a rejected sample.  Borrowed identity pointers remain valid
 * while rt_i is alive.  Each concurrent call needs its own resource.
 *
 * Reference: Li et al., Computer-Aided Design 35 (2003), 771-782,
 * https://adrianbowyer.com/Publications/volume-crofton.pdf
 */
RT_EXPORT extern int rt_surface_sampler_sample(struct rt_surface_sample *out,
    const struct rt_surface_sampler *sampler, struct resource *resource,
    const double variates[RT_SURFACE_SAMPLE_DIMENSIONS]);

__END_DECLS

#endif /* RT_SURFACE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
