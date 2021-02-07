/*                        L S E G . H
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
/* @file lseg.h */
/** @addtogroup bg_lseg */
/** @{ */

/**
 *  @brief Functions for working with line segments
 */

#ifndef BG_LSEG_H
#define BG_LSEG_H

#include "common.h"
#include "vmath.h"
#include "bg/defines.h"

__BEGIN_DECLS

/* Compute the closest point on the line segment P0->P1 to point Q.  Returns
 * the distance squared from Q to the closest point and the closest point in
 * question if c is non-NULL.
 */
BG_EXPORT double
bg_distsq_lseg3_pt(point_t *c, const point_t P0, const point_t P1, const point_t Q);

/* Compute the closest points on the line segments P0->P1 and Q0->Q1.  Returns
 * the distance squared between the closest points and (optionally) the closest
 * points in question (c1 is the point on P0->P1, c2 is the point on Q0->Q1).
 */
BG_EXPORT double
bg_distsq_lseg3_lseg3(point_t *c1, point_t *c2,
	const point_t P0, const point_t P1, const point_t Q0, const point_t Q1);

__END_DECLS

#endif  /* BG_LSEG_H */
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
