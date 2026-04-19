/*                        R H C . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2026 United States Government as represented by
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
/** @addtogroup rt_rhc */
/** @{ */
/** @file rt/primitives/rhc.h */

#ifndef RT_PRIMITIVES_RHC_H
#define RT_PRIMITIVES_RHC_H

#include "common.h"
#include "vmath.h"
#include "rt/defines.h"

__BEGIN_DECLS

/**
 * Approximate a hyperbola with line segments.
 *
 * @param pts     Linked list of points; must have at least two nodes on entry.
 * @param r       Rectangular half-width of the hyperbola.
 * @param b       Breadth (half-height) of the hyperbola.
 * @param c       Distance from the apex to the asymptote origin.
 * @param dtol    Maximum allowable chord-to-curve distance (mm).
 * @param ntol    Maximum allowable normal-deviation angle (radians); pass
 *                M_PI to ignore normal tolerance.
 * @param min_abs Minimum absolute span (mm) below which subdivision stops,
 *                preventing runaway recursion from extremely tight tolerances.
 *
 *                Recommended min_abs values:
 *                - 0.05 is a conservative default suitable for most CAD
 *                  geometry.  A full circle at this dtol requires ~300
 *                  segments; finer values rarely produce visible improvement
 *                  on typical display hardware.
 *                - 0.005 produces noticeably smoother curves on very small
 *                  features but can multiply polygon counts by 10x or more.
 *                - Values below dtol have no additional effect until dtol
 *                  itself pushes subdivision to segments shorter than min_abs.
 *                - SMALL_FASTF (~1e-37) disables the floor entirely, matching
 *                  the deprecated rt_mk_hyperbola() function signature's
 *                  behavior; use only when the geometry is known to be free
 *                  of degenerate cases that could trigger unbounded recursion.
 *
 * @return Number of additional points inserted into @p pts.
 */
RT_EXPORT extern int rt_mk_hyperbola(struct rt_pnt_node *pts,
				     fastf_t r,
				     fastf_t b,
				     fastf_t c,
				     fastf_t dtol,
				     fastf_t ntol,
				     fastf_t min_abs);

/**
 * use rt_mk_hyperbola() with an explicit min_abs argument.
 * Preserves the original rt_mk_hyperbola behavior of trusting the caller's
 * tolerances unconditionally (equivalent to min_abs = SMALL_FASTF).
 */
//DEPRECATED RT_EXPORT extern int rt_mk_hyperbola_old(struct rt_pnt_node *pts,
RT_EXPORT extern int rt_mk_hyperbola_old(struct rt_pnt_node *pts,
						    fastf_t r,
						    fastf_t b,
						    fastf_t c,
						    fastf_t dtol,
						    fastf_t ntol);

/** @} */

__END_DECLS

#endif /* RT_PRIMITIVES_RHC_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
