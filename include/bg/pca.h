/*                          P C A . H
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
/** @addtogroup bg_pca
 *
 * Principle Component Analysis
 *
 * Calculates an XYZ coordinate system such that it aligns with the largest
 * variations in the supplied data.  Intuitively, it "aligns" with the
 * shape of the points.
 *
 * To apply the coordinate system to the input points to relocate them to
 * the aligned position in model space, you can construct matrices and
 * apply them:
 *
 * @code
 * point_t center;
 * vect_t xaxis, yaxis, zaxis;
 * bg_pca(&center, &xaxis, &yaxis, &zaxis, pntcnt, pnts);
 * mat_t R, T, RT;
 * // Rotation
 * MAT_IDN(R);
 * VMOVE(&R[0], xaxis);
 * VMOVE(&R[4], yaxis);
 * VMOVE(&R[8], zaxis);
 * // Translation
 * MAT_IDN(T);
 * MAT_DELTAS_VEC_NEG(T, center);
 * // Combine
 * bn_mat_mul(RT, R, T);
 * // Apply
 * point_t p;
 * for (size_t i = 0; i < pntcnt; i++) {
 *     MAT4X3PNT(p, RT, pnts[i]);
 *     VMOVE(pnts[i], v);
 * }
 * @endcode
 */
/** @{ */
/* @file pca.h */

#ifndef BG_PCA_H
#define BG_PCA_H

#include "common.h"
#include "vmath.h"
#include "bg/defines.h"

__BEGIN_DECLS


/**
 * @brief
 * Perform a Principle Component Analysis on a set of points.
 *
 * Outputs are a center point and XYZ vectors for the coordinate system.
 *
 * @param[out]	c	Origin of aligned coordinate system
 * @param[out]	xaxis	Vector of X axis of aligned coordinate system (unit length)
 * @param[out]	yaxis	Vector of Y axis of aligned coordinate system (unit length)
 * @param[out]	zaxis	Vector of Z axis of aligned coordinate system (unit length)
 * @param[in]	npnts	Number of points in input pnts array
 * @param[in]	pnts	Array of points to analyze
 *
 * @return BRLCAD_OK success
 * @return BRLCAD_ERROR if inputs are invalid or calculation fails
 *
 */
BG_EXPORT extern int bg_pca(point_t *c, vect_t *xaxis, vect_t *yaxis, vect_t *zaxis, size_t npnts, point_t *pnts);

__END_DECLS

#endif  /* BG_PLANE_H */
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
