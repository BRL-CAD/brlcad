/*                        C H U L L . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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
/** @addtogroup bg_chull */
/** @{ */

/**
 *  @brief Routines for the computation of convex and concave hulls in 2D and 3D
 */

#ifndef BG_CHULL_H
#define BG_CHULL_H

#include "common.h"
#include "vmath.h"
#include "bg/defines.h"

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
BG_EXPORT int bg_polyline_2d_chull(point2d_t** hull, const point2d_t* polyline, int n);

BG_EXPORT int bg_polyline_2d_chull2(int** hull, const int *polyline, int n, const point2d_t* pnts);


/**
 * @brief
 * Return an array that contains just the set of 2D points active in the polyline.
 *
 * @param[out] opoly array containing just the active points in the polyline.
 * @param[in] n number of points in polyline
 * @param[in] polyline indices of polyline points in pnts array
 * @param[in] pnts array that holds the points defining the polyline
 *
 * The output array will store the points in polyline order, avoiding the need
 * for an explicit index array of point positions to define the polyline.
 *
 * @return number of points in opoly if calculation was successful
 * @return -1 if error
 */
BG_EXPORT extern int bg_2d_polyline_gc(point2d_t **opoly, int n, int *polyline, const point2d_t *pnts);


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
 * @param[out]	hull 2D convex hull array vertices in ccw orientation (max is n)
 * @param	points_2d The input 2d points for which a convex hull will be built
 * @param	n the number of points in the input set
 * @return the number of points in the output hull array or zero if error.
 */
BG_EXPORT int bg_2d_chull(point2d_t** hull, const point2d_t* points_2d, int n);


BG_EXPORT int bg_2d_chull2(int** hull, const point2d_t* points_2d, int n);

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
 * @param[out]	hull convex hull array vertices using 3-space coordinates in ccw orientation (max is n)
 * @param	points_3d The input points for which a convex hull will be built
 * @param	n the number of points in the input set
 * @return the number of points in the output hull array
 */
BG_EXPORT int bg_3d_coplanar_chull(point_t** hull, const point_t* points_3d, int n);


BG_EXPORT int bg_3d_coplanar_chull2(int** hull, const point_t* points_3d, int n);

/**
 * @brief
 * Find 3D point convex hull for unordered point sets
 *
 * This routine computes the convex hull of a three dimensional point set and
 * returns the set of vertices and triangular faces that define that hull, as
 * well as the numerical count of vertices and faces in the hull.
 *
 * @param[out]	faces set of faces in the convex hull, stored as integer indices to the vertices.  The first three indices are the vertices of the face, the second three define the second face, and so forth.
 * @param[out]  num_faces the number of faces in the faces array
 * @param[out]  vertices the set of vertices used by the convex hull.
 * @param[out]  num_vertices the number of vertices in the convex hull.
 * @param	input_points_3d The input points for which a convex hull will be built
 * @param	num_input_pnts the number of points in the input set
 * @return dimension of output (3 is three dimensional faces and hulls, 2 is a polygon hull in a plane, etc.)
 *
 * This routine is based off of Antti Kuukka's implementation of the QuickHull
 * algorithm: https://github.com/akuukka/quickhull implementation
 */
BG_EXPORT int bg_3d_chull(int **faces, int *num_faces, point_t **vertices, int *num_vertices,
			  const point_t *input_points_3d, int num_input_pnts);


__END_DECLS

#endif  /* BG_CHULL_H */
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
