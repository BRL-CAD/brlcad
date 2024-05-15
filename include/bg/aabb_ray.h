/*                      A A B B _ R A Y . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2024 United States Government as represented by
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
 * Intersection between a ray and an axis-aligned box.
 *
 */

#ifndef BG_AABB_RAY_H
#define BG_AABB_RAY_H

#include "common.h"
#include "vmath.h"
#include "bg/defines.h"

__BEGIN_DECLS

/* Compute the inverse of the direction cosines */
BG_EXPORT extern void
bg_ray_invdir(vect_t *invdir, vect_t dir);

BG_EXPORT extern int
bg_isect_aabb_ray(
	fastf_t *r_min, // first hit point
	fastf_t *r_max, // second hit point
	point_t opt,    // ray origin
	const fastf_t *invdir, // inverses of dir[]
	const fastf_t *aabb_min, // AABB first point
	const fastf_t *aabb_max  // AABB second point
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
