/*                        P O L Y G O N . H
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
/* @file polygon.h */
/** @addtogroup polygon */
/** @{ */

/**
 *  @brief Functions for working with polygons
 */

#ifndef BN_POLYGON_H
#define BN_POLYGON_H

#include "common.h"
#include "bn/defines.h"

__BEGIN_DECLS

/**
 * @brief
 * Calculate the interior area of a polygon.
 *
 * If npts > 4, Greens Theorem is used. The polygon mustn't
 * be self-intersecting.
 *
 * @param[out] area The interior area of the polygon
 * @param[in] npts Number of point_ts, stored in pts
 * @param[in] pts All points of the polygon, sorted counter-clockwise.
 * The array mustn't contain duplicated points.
 *
 * @return 0 if calculation was successful
 * @return 1 if calculation failed, e.g. because one parameter is a NULL-pointer
 */
BN_EXPORT extern int bn_polygon_area(fastf_t *area, size_t npts, const point_t *pts);


/**
 * @brief
 * Calculate the centroid of a non self-intersecting polygon
 *
 * @param[out] cent The centroid of the polygon
 * @param[in] npts Number of point_ts, stored in pts
 * @param[in] pts all points of the polygon, sorted counter-clockwise.
 * The array mustn't contain duplicated points.
 *
 * @return 0 if calculation was successful
 * @return 1 if calculation failed, e.g. because one in-parameter is a NULL-pointer
 */
BN_EXPORT extern int bn_polygon_centroid(point_t *cent, size_t npts, const point_t *pts);


/**
 * @brief
 * Calculate for an array of plane_eqs, which build a polyhedron, the
 * point_t's for each face.
 *
 * @param[out] npts Array, which stores for every face the number of
 * point_ts, added to pts. Needs to be allocated with npts[neqs] already.
 * @param[out] pts 2D-array which stores the point_ts for every
 * face. The array needs to be allocated with pts[neqs][neqs-1] already.
 * @param[in] neqs Number of plane_ts, stored in eqs
 * @param[in] eqs Array, that contains the plane equations, which
 * build the polyhedron
 *
 * @return 0 if calculation was successful
 * @return 1 if calculation failed, e.g. because one parameter is a NULL-Pointer
 */
BN_EXPORT extern int bn_polygon_mk_pts_planes(size_t *npts, point_t **pts, size_t neqs, const plane_t *eqs);


/**
 * @brief
 * Sort an array of point_ts, building a convex polygon, counter-clockwise
 *
 *@param[in] npts Number of points, pts contains
 *@param pts Array of point_ts, building a convex polygon. Duplicated points
 *aren't allowed. The points in the array will be sorted counter-clockwise.
 *@param[in] cmp Plane equation of the polygon
 *
 *@return 0 if calculation was successful
 *@return 1 if calculation failed, e.g. because pts is a NULL-pointer
 */
BN_EXPORT extern int bn_polygon_sort_ccw(size_t npts, point_t *pts, plane_t cmp);

__END_DECLS

#endif  /* BN_POLYGON_H */
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
