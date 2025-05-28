/*                       P C A . C P P
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
/** @addtogroup pca */
/** @{ */
/** @file libbg/pca.cpp
 *
 * @brief
 * Principle Component Analysis.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>

#include <Eigen/SVD>

#include "vmath.h"
#include "bn/mat.h"
#include "bg/pca.h"

// Use SVD algorithm from Soderkvist to fit a plane to vertex points
extern "C" int
bg_pca(point_t *c, vect_t *xa, vect_t *ya, vect_t *za, size_t npnts, point_t *pnts)
{
    if (!c || !xa || !ya || !za || npnts == 0 || !pnts)
	return BRLCAD_ERROR;

    // 1.  Find the center point
    point_t center = VINIT_ZERO;
    for (size_t i = 0; i < npnts; i++) {
	VADD2(center, pnts[i], center);
    }
    VSCALE(center, center, 1.0/(fastf_t)npnts);

    // 2.  Transfer the points into Eigen data types
    Eigen::MatrixXd A(3, npnts);
    for (size_t i = 0; i < npnts; i++) {
	A(0,i) = pnts[i][X] - center[X];
	A(1,i) = pnts[i][Y] - center[Y];
	A(2,i) = pnts[i][Z] - center[Z];
    }

    // 3.  Perform SVD
    Eigen::JacobiSVD<Eigen::MatrixXd> svd(A, Eigen::ComputeThinU);

    // 4.  Extract the vectors from the U matrix
    vect_t xaxis, yaxis, zaxis;
    xaxis[X] = svd.matrixU()(0,0);
    xaxis[Y] = svd.matrixU()(1,0);
    xaxis[Z] = svd.matrixU()(2,0);
    yaxis[X] = svd.matrixU()(0,1);
    yaxis[Y] = svd.matrixU()(1,1);
    yaxis[Z] = svd.matrixU()(2,1);
    zaxis[X] = svd.matrixU()(0,2);
    zaxis[Y] = svd.matrixU()(1,2);
    zaxis[Z] = svd.matrixU()(2,2);

    // 5.  Set the outputs
    VMOVE(*c, center);
    VMOVE(*xa, xaxis);
    VMOVE(*ya, yaxis);
    VMOVE(*za, zaxis);

    return BRLCAD_OK;
}

/** @} */
// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
