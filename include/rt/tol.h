/*                          T O L . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2015 United States Government as represented by
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
/** @file tol.h
 *
 * librt specific tolerance information.
 */

#ifndef RT_TOL_H
#define RT_TOL_H

#include "common.h"
#include "vmath.h"

__BEGIN_DECLS

/*
 * Unfortunately, to prevent divide-by-zero, some tolerancing needs to
 * be introduced.
 *
 *
 * RT_LEN_TOL is the shortest length, in mm, that can be stood as the
 * dimensions of a primitive.  Can probably become at least
 * SQRT_SMALL_FASTF.
 *
 * Dot products smaller than RT_DOT_TOL are considered to have a dot
 * product of zero, i.e., the angle is effectively zero.  This is used
 * to check vectors that should be perpendicular.
 *
 * asin(0.1) = 5.73917 degrees
 * asin(0.01) = 0.572967 degrees
 * asin(0.001) = 0.0572958 degrees
 * asin(0.0001) = 0.00572958 degrees
 *
 * sin(0.01 degrees) = sin(0.000174 radians) = 0.000174533
 *
 * Many TGCs at least, in existing databases, will fail the
 * perpendicularity test if DOT_TOL is much smaller than 0.001, which
 * establishes a 1/20th degree tolerance.  The intent is to eliminate
 * grossly bad primitives, not pick nits.
 *
 * RT_PCOEF_TOL is a tolerance on polynomial coefficients to prevent
 * the root finder from having heartburn.
 */
#define RT_LEN_TOL      (1.0e-8)
#define RT_DOT_TOL      (0.001)
#define RT_PCOEF_TOL    (1.0e-10)


/**
 * Tessellation (geometric) tolerances, different beasts than the
 * calculation tolerance in bn_tol.
 */
struct rt_tess_tol {
    uint32_t magic;
    double              abs;                    /**< @brief absolute dist tol */
    double              rel;                    /**< @brief rel dist tol */
    double              norm;                   /**< @brief normal tol */
};
#define RT_CK_TESS_TOL(_p) BU_CKMAG(_p, RT_TESS_TOL_MAGIC, "rt_tess_tol")
#define RT_TESS_TOL_INIT_ZERO {RT_TESS_TOL_MAGIC, 0.0, 0.0, 0.0}


__END_DECLS

#endif /* RT_TOL_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
