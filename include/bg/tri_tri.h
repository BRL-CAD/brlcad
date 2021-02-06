/*                      T R I _ T R I . H
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
/* @file tri_tri.h */
/** @addtogroup bg_tri_tri */
/** @{ */

/**
 * @brief
 * Tomas Möller's triangle/triangle intersection routines from the article
 *
 * "A Fast Triangle-Triangle Intersection Test",
 * Journal of Graphics Tools, 2(2), 1997
 */

#ifndef BG_TRI_TRI_H
#define BG_TRI_TRI_H

#include "common.h"
#include "vmath.h"
#include "bg/defines.h"

__BEGIN_DECLS

BG_EXPORT extern int bg_tri_tri_isect_coplanar(point_t V0,
					       point_t V1,
					       point_t V2,
					       point_t U0,
					       point_t U1,
					       point_t U2,
					       int area_flag);

/* Experimental */
BG_EXPORT extern int bg_tri_tri_isect_coplanar2(point_t V0,
					       point_t V1,
					       point_t V2,
					       point_t U0,
					       point_t U1,
					       point_t U2,
					       int area_flag);


/* Return 1 if the triangles intersect, else 0 */
BG_EXPORT extern int bg_tri_tri_isect(point_t V0,
				      point_t V1,
				      point_t V2,
				      point_t U0,
				      point_t U1,
				      point_t U2);

/* Return 1 if the triangles intersect, else 0.  coplanar flag
 * is set if the triangles are coplanar, and isectpts are set
 * to the start and end points of the line segment describing
 * the triangle intersections.  If the intersection is a point,
 * isectpt2 will be the same point as isectpt1. */
BG_EXPORT extern int bg_tri_tri_isect_with_line(point_t V0,
						point_t V1,
						point_t V2,
						point_t U0,
						point_t U1,
						point_t U2,
						int *coplanar,
						point_t *isectpt1,
						point_t *isectpt2);

__END_DECLS

#endif  /* BG_TRI_TRI_H */
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
