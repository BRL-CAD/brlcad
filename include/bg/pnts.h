/*                         P N T S . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2025 United States Government as represented by
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
/* @file pnts.h */
/** @addtogroup bg_pnts */
/** @{ */

/**
 *  @brief Routines for working with points.
 */

#ifndef BG_PNTS_H
#define BG_PNTS_H

#include "common.h"
#include "vmath.h"
#include "bn/numgen.h"
#include "bg/defines.h"

__BEGIN_DECLS

/**
 * @brief
 * Calculate the axis-aligned bounding box for a point set.
 *
 * Returns BRLCAD_OK if successful, else BRLCAD_ERROR.
 *
 * @param[out] min XYZ coordinate defining the minimum bbox point
 * @param[out] max XYZ coordinate defining the maximum bbox point
 * @param[in] p array that holds the points defining the trimesh
 * @param[in] num_pnts size of pnts array
 */
BG_EXPORT extern int
bg_pnts_aabb(point_t *min, point_t *max, const point_t *p, size_t num_pnts);

/**
 * @brief
 * Calculate an oriented bounding box for a point set.
 *
 * Returns BRLCAD_OK if successful, else BRLCAD_ERROR.
 *
 * @param[out] c  XYZ coordinate defining the bbox center
 * @param[out] v1 first extent vector of the bbox
 * @param[out] v2 second extent vector of the bbox
 * @param[out] v3 third extent vector of the bbox
 * @param[in] p array that holds the points defining the trimesh
 * @param[in] num_pnts size of pnts array
 */
BG_EXPORT extern int
bg_pnts_obb(point_t *c, vect_t *v1, vect_t *v2, vect_t *v3, const point_t *p, size_t num_pnts);

__END_DECLS

#endif  /* BG_PNTS_H */
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
