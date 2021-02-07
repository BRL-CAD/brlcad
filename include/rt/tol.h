/*                          T O L . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2021 United States Government as represented by
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

#ifndef RT_TOL_H
#define RT_TOL_H

#include "common.h"
#include "vmath.h"
#include "bn/tol.h"
#include "rt/defines.h"

__BEGIN_DECLS

/** @addtogroup rt_tol
 *
 * @brief
 * librt specific tolerance information.
 *
 * These routines provide access to the default tolerance values
 * available within LIBRT.  These routines assume working units of
 * 'mm' and are idealized to only attempt to account for
 * cross-platform hardware and floating point instability.  That is to
 * say that the default tolerance values are tight.

 */
/** @{ */
/** @file rt/tol.h */

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
 *
 * RT_ROOT_TOL is the tolerance on the imaginary component of complex
 * roots, for determining whether a root is sufficiently near zero.
 */
#define RT_LEN_TOL      (1.0e-8)
#define RT_DOT_TOL      (0.001)
#define RT_PCOEF_TOL    (1.0e-10)
#define RT_ROOT_TOL     (1.0e-5)

/**
 * Fills in the provided bn_tol structure with compile-time default
 * tolerance values.  These presently correspond to a distance
 * tolerance of 5e-5 (assuming default working units is 1/2000th a mm)
 * and a perpendicularity tolerance of 1e-6.  If tol is NULL, a
 * structure is allocated, initialized, and returned.
 */
RT_EXPORT extern struct bn_tol *rt_tol_default(struct bn_tol *tol);

/** @} */

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
