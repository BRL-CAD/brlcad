/* BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1.
 */

#ifndef LIBBREP_AFFINE_SURFACE_H
#define LIBBREP_AFFINE_SURFACE_H

#include "common.h"

#include <cmath>

#include "brep.h"


struct AffinePlanarSurface {
    ON_Interval m_u_domain;
    ON_Interval m_v_domain;
    ON_3dPoint m_southwest;
    ON_3dVector m_u_direction;
    ON_3dVector m_v_direction;

    bool
    ParametersAt(const ON_3dPoint &point, ON_2dPoint &parameters) const
    {
	const double u_length = m_u_direction.Length();
	const double v_length = m_v_direction.Length();
	const ON_3dVector normal = ON_CrossProduct(m_u_direction,
	    m_v_direction);
	const double normal_length = normal.Length();
	if (!std::isfinite(u_length) || !std::isfinite(v_length) ||
	    !std::isfinite(normal_length) || u_length <= ON_ZERO_TOLERANCE ||
	    v_length <= ON_ZERO_TOLERANCE ||
	    normal_length <= ON_ZERO_TOLERANCE * u_length * v_length)
	{
	    return false;
	}

	const double u_dot_u = ON_DotProduct(m_u_direction, m_u_direction);
	const double u_dot_v = ON_DotProduct(m_u_direction, m_v_direction);
	const double v_dot_v = ON_DotProduct(m_v_direction, m_v_direction);
	const double determinant = u_dot_u * v_dot_v - u_dot_v * u_dot_v;
	const double determinant_scale = u_dot_u * v_dot_v;
	if (!std::isfinite(determinant) ||
	    !std::isfinite(determinant_scale) ||
	    determinant <= ON_EPSILON * determinant_scale)
	{
	    return false;
	}

	const ON_3dVector offset = point - m_southwest;
	const double u_fraction =
	    (ON_DotProduct(offset, m_u_direction) * v_dot_v -
	     ON_DotProduct(offset, m_v_direction) * u_dot_v) / determinant;
	const double v_fraction =
	    (ON_DotProduct(offset, m_v_direction) * u_dot_u -
	     ON_DotProduct(offset, m_u_direction) * u_dot_v) / determinant;
	if (!std::isfinite(u_fraction) || !std::isfinite(v_fraction)) {
	    return false;
	}

	parameters.x = m_u_domain.ParameterAt(u_fraction);
	parameters.y = m_v_domain.ParameterAt(v_fraction);
	return std::isfinite(parameters.x) && std::isfinite(parameters.y);
    }
};


static bool
affine_planar_surface_mapping(
    const ON_Surface *surface,
    AffinePlanarSurface &mapping,
    double closure_tolerance = ON_ZERO_TOLERANCE)
{
    if (surface == NULL || !std::isfinite(closure_tolerance) ||
	closure_tolerance < ON_ZERO_TOLERANCE)
    {
	return false;
    }

    ON_NurbsSurface surface_nurbs;
    if (surface->GetNurbForm(surface_nurbs) != 1 ||
	surface_nurbs.IsRational() || surface_nurbs.Degree(0) != 1 ||
	surface_nurbs.Degree(1) != 1 || surface_nurbs.CVCount(0) != 2 ||
	surface_nurbs.CVCount(1) != 2)
    {
	return false;
    }

    const ON_Interval u_domain = surface->Domain(0);
    const ON_Interval v_domain = surface->Domain(1);
    const ON_3dPoint southwest = surface->PointAt(u_domain.Min(),
	v_domain.Min());
    const ON_3dPoint southeast = surface->PointAt(u_domain.Max(),
	v_domain.Min());
    const ON_3dPoint northwest = surface->PointAt(u_domain.Min(),
	v_domain.Max());
    const ON_3dPoint northeast = surface->PointAt(u_domain.Max(),
	v_domain.Max());
    if (!southwest.IsValid() || !southeast.IsValid() ||
	!northwest.IsValid() || !northeast.IsValid() ||
	((northeast - southeast) - (northwest - southwest)).Length() >
	closure_tolerance)
    {
	return false;
    }

    mapping.m_u_domain = u_domain;
    mapping.m_v_domain = v_domain;
    mapping.m_southwest = southwest;
    mapping.m_u_direction = southeast - southwest;
    mapping.m_v_direction = northwest - southwest;
    return true;
}

#endif
