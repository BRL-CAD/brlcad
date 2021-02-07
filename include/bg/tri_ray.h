/*                      T R I _ R A Y . H
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
/* @file tri_ray.h */
/** @addtogroup bg_tri_ray */
/** @{ */

/**
 * @brief
 * Möller & Trumbore's triangle/ray intersection test
 *
 * From the article
 *
 * "Fast, Minimal Storage Ray-Triangle Intersection",
 * Journal of Graphics Tools, 2(1):21-28, 1997
 *
 */

#ifndef BG_TRI_RAY_H
#define BG_TRI_RAY_H

#include "common.h"
#include "vmath.h"
#include "bg/defines.h"

__BEGIN_DECLS

BG_EXPORT extern int bg_isect_tri_ray(const point_t orig,
				      const point_t dir,
				      const point_t vert0,
				      const point_t vert1,
				      const point_t vert2,
				      point_t *isect);


__END_DECLS

#endif  /* BG_TRI_RAY_H */
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
