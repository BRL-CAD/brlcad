/*                        C H U L L . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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

/*----------------------------------------------------------------------*/
/* @file chull.h */
/** @addtogroup chull */
/** @{ */

/**
 *  @brief Routines for the computation of convex hulls in 2D and 3D
 */

#ifndef BN_CHULL_H
#define BN_CHULL_H

#include "common.h"
#include "vmath.h"
#include "bn/defines.h"

__BEGIN_DECLS

/**
 * @brief
 * Melkman's 2D simple polyline O(n) convex hull algorithm
 *
 * On-line construction of the convex hull of a simple polyline
 * Melkman, Avraham A. Information Processing Letters 25.1 (1987): 11-12.
 *
 * See also <a href="http://geomalgorithms.com/a12-_hull-3.html">http://geomalgorithms.com/a12-_hull-3.html</a>
 *
 * @param[out]	hull convex hull array vertices in ccw orientation (max is n)
 * @param	polyline The points defining the input polyline, stored with ccw orientation
 * @param	n the number of points in polyline
 * @return the number of points in the output hull array
 */
BN_EXPORT int bn_polyline_2d_chull(point2d_t** hull, const point2d_t* polyline, int n);

/**
 * @brief
 * Find 2D convex hull for unordered co-planar point sets
 *
 * The monotone chain algorithm's sorting approach is used to do
 * the initial ordering of the points:
 *
 * Another efficient algorithm for convex hulls in two dimensions.
 * Andrew, A. M. Information Processing Letters 9.5 (1979): 216-219.
 *
 * See also <a href="http://geomalgorithms.com/a10-_hull-1.html">http://geomalgorithms.com/a10-_hull-1.html</a>
 *
 * From there, instead of using the monotonic chain hull assembly
 * step, recognize that the points thus ordered can be viewed as
 * defining a simple polyline and use Melkman's algorithm for the
 * hull building.
 *
 * The input point array currently uses type point_t, but all Z
 * values should be zero.
 *
 * @param[out]	hull 2D convex hull array vertices in ccw orientation (max is n)
 * @param	points_2d The input 2d points for which a convex hull will be built
 * @param	n the number of points in the input set
 * @return the number of points in the output hull array or zero if error.
 */
BN_EXPORT int bn_2d_chull(point2d_t** hull, const point2d_t* points_2d, int n);

/**
 * @brief
 * Find 3D coplanar point convex hull for unordered co-planar point sets
 *
 * This function assumes an input an array of 3D points which are coplanar
 * in some arbitrary plane.  This function:
 *
 * 1. Finds the plane that fits the points and picks an origin, x-axis and y-axis
 *    which allow 2D coordinates for all points to be calculated.
 * 2. Calls 2D routines on the array found by step 1 to get a 2D convex hull
 * 3. Translates the resultant 2D hull points back into 3D points so the hull array
 *    contains the bounding hull expressed in the 3D coordinate space of the
 *    original points.
 *
 * @param[out]	hull_3d convex hull array vertices using 3-space coordinates in ccw orientation (max is n)
 * @param	points_3d The input points for which a convex hull will be built
 * @param	n the number of points in the input set
 * @return the number of points in the output hull array
 */
BN_EXPORT int bn_3d_coplanar_chull(point_t** hull, const point_t* points_3d, int n);

__END_DECLS

#endif  /* BN_CHULL_H */
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
