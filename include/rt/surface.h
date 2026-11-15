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

#include "vmath.h"
#include "rt/defines.h"
#include "rt/hit.h"

__BEGIN_DECLS

struct application;
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
