/*                     B N _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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

/** @addtogroup libbn */
/** @{ */
/** @file bn_private.h
 *
 * Private header file for the BRL-CAD Numerical Computation Library, LIBBN.
 *
 */

/**
 * @brief
 * Find a 2D coordinate system for a set of co-planar 3D points
 *
 * Based on the planar normal and the vector from the center point to the
 * point furthest from that center, find vectors describing a 2D coordinate system.
 *
 * @param[out]  origin_pnt Origin of 2D coordinate system in 3 space
 * @param[out]  u_axis 3D vector describing the U axis of the 2D coordinate system in 3 space
 * @param[out]  v_axis 3D vector describing the V axis of the 2D coordinate system in 3 space
 * @param       points_3d Array of 3D points
 * @param       n the number of points in the input set
 * @return 0 if successful
 */
int
bn_coplanar_2d_coord_sys(point_t *origin_pnt, vect_t *u_axis, vect_t *v_axis, const point_t *points_3d, int n);

/**
 * @brief
 * Find 2D coordinates for a set of co-planar 3D points
 *
 * @param[out]  points_2d Array of parameterized 2D points
 * @param       origin_pnt Origin of 2D coordinate system in 3 space
 * @param       u_axis 3D vector describing the U axis of the 2D coordinate system in 3 space
 * @param       v_axis 3D vector describing the V axis of the 2D coordinate system in 3 space
 * @param       points_3d 3D input points
 * @param       n the number of points in the input set
 * @return 0 if successful
 */
int
bn_coplanar_3d_to_2d(point2d_t **points_2d, const point_t *origin_pnt,
                     const vect_t *u_axis, const vect_t *v_axis,
                     const point_t *points_3d, int n);

/**
 * @brief
 * Find 3D coordinates for a set of 2D points given a coordinate system
 *
 * @param[out]  points_3d Array of 3D points
 * @param       origin_pnt Origin of 2D coordinate system in 3 space
 * @param       u_axis 3D vector describing the U axis of the 2D coordinate system in 3 space
 * @param       v_axis 3D vector describing the V axis of the 2D coordinate system in 3 space
 * @param       points_2d 2D input points
 * @param       n the number of points in the input set
 * @return 0 if successful
 */
int
bn_coplanar_2d_to_3d(point_t **points_3d, const point_t *origin_pnt,
                     const vect_t *u_axis, const vect_t *v_axis,
                     const point2d_t *points_2d, int n);


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
