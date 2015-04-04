/*                      T R I _ T R I . H
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
/* @file tri_ray.h */
/** @addtogroup tri_ray */
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

#ifndef BN_TRI_TRI_H
#define BN_TRI_TRI_H

#include "common.h"
#include "vmath.h"
#include "bn/defines.h"

__BEGIN_DECLS

BN_EXPORT extern int bn_isect_tri_ray(const point_t orig,
				      const point_t dir,
				      const point_t vert0,
				      const point_t vert1,
				      const point_t vert2,
				      point_t *isect);


__END_DECLS

#endif  /* BN_RAY_TRI_H */
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
