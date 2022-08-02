/*                        O B R . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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
/* @file obr.h */
/** @addtogroup bg_obr */
/** @{ */

/**
 *  @brief Routines for the computation of oriented bounding rectangles 2D and 3D
 */

#ifndef BG_OBR_H
#define BG_OBR_H

#include "common.h"
#include "vmath.h"
#include "bg/defines.h"

__BEGIN_DECLS

/**
 *@brief
 * Uses the Rotating Calipers algorithm to find the
 * minimum oriented bounding rectangle for a set of 2D
 * points.  Returns 0 on success.
 *
 * The box will be described by a center point and 2
 * vectors:
 *
 * \verbatim
 * ----------------------------
 * |            ^             |
 * |            |             |
 * |         v  |             |
 * |            |             |
 * |            *------------>|
 * |         center     u     |
 * |                          |
 * |                          |
 * ----------------------------
 * \endverbatim
 *
 * Note that the box is oriented, and thus not necessarily axis
 * aligned (u and v are perpendicular, but not necessarily parallel
 * with the coordinate space V=0 and U=0 axis vectors.)
 *
 * @param[out] center	center of oriented bounding rectangle
 * @param[out] u	vector in the direction of obr x with
 * 			vector length of 0.5 * obr length
 * @param[out] v	vector in the obr y direction with vector
 * 			length of 0.5 * obr width
 * @param points_2d	array of 2D points
 * @param pnt_cnt	number of points in pnts array
 */
BG_EXPORT extern int bg_2d_obr(point2d_t *center,
			       vect2d_t *u,
			       vect2d_t *v,
			       const point2d_t *points_2d,
			       int pnt_cnt);

/**
 *@brief
 * Uses the Rotating Calipers algorithm to find the
 * minimum oriented bounding rectangle for a set of coplanar 3D
 * points.  Returns 0 on success.
 *
 * @param[out] center	center of oriented bounding rectangle
 * @param[out] v1	vector in the direction of obr x with
 * 			vector length of 0.5 * obr length
 * @param[out] v2	vector in the obr y direction with vector
 * 			length of 0.5 * obr width
 * @param points_3d	array of coplanar 3D points
 * @param pnt_cnt	number of points in pnts array
 */
BG_EXPORT extern int bg_3d_coplanar_obr(point_t *center,
					vect_t *v1,
					vect_t *v2,
					const point_t *points_3d,
					int pnt_cnt);

/**
 *@brief
 * Find the minimum oriented bounding rectangular cuboid
 * for a set of 3D points.  Returns 0 on success.
 *
 * TODO - the GeometricTools engine has an implementation
 * of the stack needed to do this - want to look not only
 * at their MinimumVolumeBox3 implementation but the supporting
 * convex hull 3d routines to see if they're an improvement
 * on the Clarkson implementation.  Also want to check their
 * obb/obb intersection test (GteIntrOrientedBox3OrientedBox3.h)
 *
 * http://www.geometrictools.com/GTEngine/Include/GteMinimumVolumeBox3.h
 *
 * The points in the output array are arranged as seen
 * in the figure below, with the integer number at each
 * vertex position corresponding to the pnts array index
 * n-1 (for example, the first point, labeled 1 below,
 * would be pnts[0].)
 *
 * \verbatim
 *            8
 *         *  |   *
 *      *     |       *
 *  4         |           7
 *  |    *    |        *  |
 *  |         *     *     |
 *  |         |  3        |
 *  |         |  |        |
 *  |         5  |        |
 *  |       *    |*       |
 *  |   *        |    *   |
 *  1            |        6
 *      *        |     *
 *           *   |  *
 *               2
 * \endverbatim
 *
 *
 * @param[out] pnts     eight points of oriented bounding box
 * @param points_3d	array of coplanar 3D points
 * @param pnt_cnt	number of points in pnts array
 */
BG_EXPORT extern int bg_3d_obb(point_t **pnts,
			       const point_t *points_3d,
			       int pnt_cnt);


__END_DECLS

#endif  /* BG_OBR_H */
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
