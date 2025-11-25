/*                           S A T . H
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
/*----------------------------------------------------------------------*/
/* @file sat.h */
/** @addtogroup bg_sat */
/** @{ */

/**
 * @brief
 *
 * Implementation of Separating Axis Theorem intersection tests
 *
 */

#ifndef BG_SAT_H
#define BG_SAT_H

#include "common.h"
#include "vmath.h"
#include "bg/defines.h"

__BEGIN_DECLS

/**
 * Test for an intersection between a line and an Axis-Aligned Bounding Box (AABB).
 *
 * Returns 1 if they intersect, 0 otherwise.
 */
BG_EXPORT extern int
bg_sat_line_aabb(
	point_t origin, vect_t ldir,
	point_t aabb_center, vect_t aabb_extent
	);

/**
 * Test for an intersection between a line and an Oriented Bounding Box (OBB).
 *
 * Returns 1 if they intersect, 0 otherwise.
 */
BG_EXPORT extern int
bg_sat_line_obb(
	point_t origin, vect_t ldir,
	point_t obb_center, vect_t obb_extent1, vect_t obb_extent2, vect_t obb_extent3
	);

/**
 * Test for an intersection between a triangle and an Axis-Aligned Bounding Box (AABB).
 *
 * Returns 1 if they intersect, 0 otherwise.
 */
BG_EXPORT extern int
bg_sat_tri_aabb(
	point_t v1, point_t v2, point_t v3,
	point_t aabb_center, vect_t aabb_extent
	);

/**
 * Test for an intersection between a triangle and an Oriented Bounding Box (OBB).
 *
 * Returns 1 if they intersect, 0 otherwise.
 */
BG_EXPORT extern int
bg_sat_tri_obb(
	point_t v1, point_t v2, point_t v3,
	point_t obb_center, vect_t obb_extent1, vect_t obb_extent2, vect_t obb_extent3
	);

/**
 * Test for an intersection between an Axis-Aligned Bounding Box (AABB) and an
 * Oriented Bounding Box (OBB). The latter is defined by a center point and
 * three perpendicular vectors from the center to the centers of the various
 * faces.
 *
 * Returns 1 if they intersect, 0 otherwise.
 */
BG_EXPORT extern int
bg_sat_aabb_obb(
	point_t aabb_min, point_t aabb_max,
	point_t obb_center, vect_t obb_extent1, vect_t obb_extent2, vect_t obb_extent3
	);

/**
 * Test for an intersection between two Oriented Bounding Boxes (OBBs). The
 * boxes are defined by a center point and three perpendicular vectors from the
 * center to the centers of the various faces.
 *
 * Returns 1 if they intersect, 0 otherwise.
 */
BG_EXPORT extern int
bg_sat_obb_obb(
	point_t obb1_center, vect_t obb1_extent1, vect_t obb1_extent2, vect_t obb1_extent3,
	point_t obb2_center, vect_t obb2_extent1, vect_t obb2_extent2, vect_t obb2_extent3
	);


__END_DECLS

#endif  /* BG_SAT_H */
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
