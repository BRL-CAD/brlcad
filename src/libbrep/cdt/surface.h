/*                      S U R F A C E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by the
 * U.S. Army Research Laboratory.
 *
 * Distributed under the terms of the GNU Lesser General Public License
 * (LGPL), version 2.1.
 */

#ifndef LIBBREP_CDT_SURFACE_H
#define LIBBREP_CDT_SURFACE_H

#include "common.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "brep.h"

/* Preserve exact seam sides and use bounded arithmetic even for distant
 * copies.  Allow roundoff from trim interpolation near a domain endpoint. */
static inline double
cdt_surface_parameter(double parameter, const ON_Interval &domain)
{
    const double period = domain.Length();
    if (!(period > 0.0) || !std::isfinite(period) ||
	!std::isfinite(parameter) || !std::isfinite(domain.Min()) ||
	!std::isfinite(domain.Max()))
	return ON_UNSET_VALUE;
    const double rounding_units = 256.0;
    const double magnitude = std::max(std::fabs(domain.Min()),
	std::fabs(domain.Max()));
    const double tolerance = rounding_units *
	std::numeric_limits<double>::epsilon() * std::max(magnitude, period);
    if (parameter >= domain.Min() - tolerance &&
	parameter <= domain.Max() + tolerance)
	return std::max(domain.Min(), std::min(domain.Max(), parameter));
    const double offset = parameter - domain.Min();
    /* Separate remainders avoid overflow for distant but finite copies. */
    double remainder = std::isfinite(offset) ? std::fmod(offset, period) :
	std::fmod(parameter, period) - std::fmod(domain.Min(), period);
    if (remainder < 0.0)
	remainder += period;
    if (remainder >= period)
	remainder -= period;
    return std::fabs(remainder) <= tolerance && parameter > domain.Min() ?
	domain.Max() : domain.Min() + remainder;
}

/* Trim charts may use other copies of a closed surface's domain.  Normalize
 * only the evaluation coordinates: changing stored UVs loses seam winding
 * and topology, while evaluating them directly can extrapolate a NURBS. */
static inline ON_2dPoint
cdt_surface_uv(const ON_Surface *surface, const ON_2dPoint &uv)
{
    if (!surface || !uv.IsValid())
	return ON_2dPoint::UnsetPoint;
    ON_2dPoint native = uv;
    for (int direction = 0; direction < 2; ++direction) {
	if (surface->IsClosed(direction))
	    native[direction] = cdt_surface_parameter(native[direction],
		surface->Domain(direction));
    }
    return native;
}

static inline ON_3dPoint
cdt_surface_point(const ON_Surface *surface, const ON_2dPoint &uv)
{
    const ON_2dPoint native = cdt_surface_uv(surface, uv);
    return native.IsValid() ? surface->PointAt(native.x, native.y) :
	ON_3dPoint::UnsetPoint;
}

static inline bool
cdt_surface_normal(const ON_Surface *surface, const ON_2dPoint &uv,
	ON_3dPoint &point, ON_3dVector &normal)
{
    point = ON_3dPoint::UnsetPoint;
    normal = ON_3dVector::UnsetVector;
    const ON_2dPoint native = cdt_surface_uv(surface, uv);

    if (native.IsValid() &&
	surface_EvNormal(surface, native.x, native.y, point, normal) &&
	point.IsValid() && normal.IsValid())
	return true;
    point = ON_3dPoint::UnsetPoint;
    normal = ON_3dVector::UnsetVector;
    return false;
}

#endif /* LIBBREP_CDT_SURFACE_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
