/*                        T O L . H
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
/** @addtogroup bn_tol
 *
 * @brief Support for uniform tolerances
 *
 * A handy way of passing around the tolerance information needed to
 * perform approximate floating-point calculations on geometry.
 *
 * dist & dist_sq establish the distance tolerance.
 *
 * If two points are closer together than dist, then they are to be
 * considered the same point.
 *
 * For example:
 @code
 point_t a, b;
 vect_t diff;
 VSUB2(diff, a, b);
 if (MAGNITUDE(diff) < tol->dist)	a & b are the same.
 or, more efficiently:
 if (MAQSQ(diff) < tol->dist_sq)
 @endcode
 * perp & para establish the angular tolerance.
 *
 * If two rays emanate from the same point, and their dot product is
 * nearly one, then the two rays are the same, while if their dot
 * product is nearly zero, then they are perpendicular.
 *
 * For example:
 @code
 vect_t a, b;
 if (fabs(VDOT(a, b)) >= tol->para)	a & b are parallel
 if (fabs(VDOT(a, b)) <= tol->perp)	a & b are perpendicular
 @endcode
 *
 *@note
 * tol->dist_sq = tol->dist * tol->dist;
 *@n tol->para = 1 - tol->perp;
 */
/** @{ */
/** @file bn/tol.h */

#ifndef BN_TOL_H
#define BN_TOL_H

#include "common.h"
#include "bn/defines.h"
#include "bu/magic.h"

__BEGIN_DECLS

struct bn_tol {
    uint32_t magic;
    double dist;		/**< @brief >= 0 */
    double dist_sq;		/**< @brief dist * dist */
    double perp;		/**< @brief nearly 0 */
    double para;		/**< @brief nearly 1 */
};

/**
 * asserts the validity of a bn_tol struct.
 */
#define BN_CK_TOL(_p)	BU_CKMAG(_p, BN_TOL_MAGIC, "bn_tol")

/**
 * initializes a bn_tol struct to zero without allocating any memory.
 */
#define BN_TOL_INIT(_p) { \
	(_p)->magic = BN_TOL_MAGIC; \
	(_p)->dist = 0.0; \
	(_p)->dist_sq = 0.0; \
	(_p)->perp = 0.0; \
	(_p)->para = 1.0; \
    }

/**
 * macro suitable for declaration statement zero-initialization of a
 * bn_tol struct.
 */
#define BN_TOL_INIT_ZERO { BN_TOL_MAGIC, 0.0, 0.0, 0.0, 1.0 }

/**
 * returns truthfully whether a bn_tol struct has been initialized.
 */
#define BN_TOL_IS_INITIALIZED(_p) (((struct bn_tol *)(_p) != (struct bn_tol *)0) && LIKELY((_p)->magic == BN_TOL_MAGIC))

/**
 * replaces the hard coded tolerance value
 */
#define BN_TOL_DIST 0.0005

/**
 * returns truthfully whether a given dot-product of two unspecified
 * vectors are within a specified parallel tolerance.
 */
#define BN_VECT_ARE_PARALLEL(_dot, _tol)				\
    (((_dot) <= -SMALL_FASTF) ? (NEAR_EQUAL((_dot), -1.0, (_tol)->perp)) : (NEAR_EQUAL((_dot), 1.0, (_tol)->perp)))

/**
 * returns truthfully whether a given dot-product of two unspecified
 * vectors are within a specified perpendicularity tolerance.
 */
#define BN_VECT_ARE_PERP(_dot, _tol)					\
    (((_dot) < 0) ? ((-(_dot))<=(_tol)->perp) : ((_dot) <= (_tol)->perp))

__END_DECLS

#endif  /* BN_TOL_H */
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
