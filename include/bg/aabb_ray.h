/*                      A A B B _ R A Y . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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
/* @file aabb_ray.h */
/** @addtogroup bg_aabb_ray */
/** @{ */

/**
 * @brief
 *
 * Intersection between an infinite line and an axis-aligned box.
 *
 */

#ifndef BG_AABB_RAY_H
#define BG_AABB_RAY_H

#include "common.h"
#include "vmath.h"
#include "bg/defines.h"

__BEGIN_DECLS

/**
 * Compute the inverse direction components for use with
 * bg_isect_aabb_ray().  Components within SQRT_SMALL_FASTF of zero are
 * represented by INFINITY.  The input direction is not modified.
 */
BG_EXPORT extern void
bg_ray_invdir(vect_t *invdir, const vect_t dir);

/**
 * Test the infinite line defined by opt and the reciprocal direction vector
 * invdir against an axis-aligned bounding box.  Despite its historical name,
 * this function does not reject intersections behind opt: a successful result
 * can have both output parameters negative.  Callers that require a forward
 * ray must additionally require *r_max >= 0.0.
 *
 * On success, r_min and r_max are the entry and exit line parameters.  They
 * are valid only when the function returns 1.
 */
BG_EXPORT extern int
bg_isect_aabb_ray(
	fastf_t *r_min,
	fastf_t *r_max,
	const point_t opt,
	const fastf_t *invdir,
	const fastf_t *aabb_min,
	const fastf_t *aabb_max
	);

__END_DECLS

#endif  /* BG_AABB_RAY_H */
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
