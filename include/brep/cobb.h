/*                         C O B B . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file brep/cobb.h
 *
 * Reusable Cobb rational Bezier sphere fixtures.
 */
#ifndef BREP_COBB_H
#define BREP_COBB_H

#include "common.h"

#include "brep/defines.h"

#ifdef __cplusplus
extern "C++" {

class ON_3dPoint;
class ON_BezierSurface;
class ON_Brep;

/** Return one unit Cobb sphere patch after rotations in degrees. */
BREP_EXPORT extern ON_BezierSurface *
ON_Brep_CobbSphereFace(double rotation_x, double rotation_z);

/**
 * Return the legacy six-patch representation with independent face topology.
 * This is an open plate-mode fixture, not a solid.
 */
BREP_EXPORT extern ON_Brep *
ON_Brep_CobbSphereUnsewn(double radius, const ON_3dPoint &origin);

/**
 * Return the same six support surfaces with eight shared vertices and twelve
 * shared edges.  The result is a closed, oriented solid.
 */
BREP_EXPORT extern ON_Brep *
ON_Brep_CobbSphereSewn(double radius, const ON_3dPoint &origin);

/** Measure maximum radial error on a uniform per-surface sample grid. */
BREP_EXPORT extern double
ON_Brep_CobbSphereMaxRadialError(const ON_Brep *brep, double radius,
	const ON_3dPoint &origin, int samples_per_direction = 33);

}
#endif

#endif  /* BREP_COBB_H */

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
