/*                      B R N O D E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2026 United States Government as represented by
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

#include "common.h"

#include <algorithm>
#include <cfloat>

#include "bu/log.h"
#include "brep/brnode.h"


namespace brlcad {


BRNode::BRNode(
    const ON_Curve *curve,
    int trim_index,
    int adj_face_index,
    const ON_BoundingBox &node,
    const ON_Interval &t,
    bool innerTrim) :
    m_adj_face_index(adj_face_index),
    m_XIncreasing(false),
    m_Horizontal(false),
    m_Vertical(false),
    m_innerTrim(innerTrim),
    m_trim(curve),
    m_trim_index(trim_index),
    m_t(t),
    m_bbox_min{node.m_min.x, node.m_min.y},
    m_bbox_max{node.m_max.x, node.m_max.y},
    m_start{0.0, 0.0},
    m_end{0.0, 0.0}
{
    const ON_3dPoint start = curve->PointAt(m_t[0]);
    const ON_3dPoint end = curve->PointAt(m_t[1]);
    m_start[0] = start.x;
    m_start[1] = start.y;
    m_end[0] = end.x;
    m_end[1] = end.y;
    /* check for vertical segments they can be removed from trims
     * above (can't tell direction and don't need
     */
    m_Horizontal = false;
    m_Vertical = false;

    /*
     * should be okay since we split on Horz/Vert tangents
     */
    if (NEAR_EQUAL(m_end[X], m_start[X], BREP_UV_DIST_FUZZ)) {
	m_Vertical = true;
	if (m_innerTrim) {
	    m_XIncreasing = false;
	} else {
	    m_XIncreasing = true;
	}
    } else if (NEAR_EQUAL(m_end[Y], m_start[Y], BREP_UV_DIST_FUZZ)) {
	m_Horizontal = true;
	if ((m_end[X] - m_start[X]) > 0.0) {
	    m_XIncreasing = true;
	} else {
	    m_XIncreasing = false;
	}
    } else {
	if ((m_end[X] - m_start[X]) > 0.0) {
	    m_XIncreasing = true;
	} else {
	    m_XIncreasing = false;
	}
    }
}


BRNode::BRNode(Deserializer &deserializer, const ON_Brep &brep) :
    m_adj_face_index(-1),
    m_XIncreasing(false),
    m_Horizontal(false),
    m_Vertical(false),
    m_innerTrim(false),
    m_trim(NULL),
    m_trim_index(-1),
    m_t(),
    m_bbox_min{0.0, 0.0},
    m_bbox_max{0.0, 0.0},
    m_start{0.0, 0.0},
    m_end{0.0, 0.0}
{
    deserializer.read(m_t);
    for (int direction = 0; direction < 2; ++direction) {
	m_bbox_min[direction] = deserializer.read_double();
	m_bbox_max[direction] = deserializer.read_double();
	m_start[direction] = deserializer.read_double();
	m_end[direction] = deserializer.read_double();
    }

    const uint8_t bool_flags = deserializer.read_uint8();
    m_trim_index = deserializer.read_int32();
    m_adj_face_index = deserializer.read_int32();

    m_XIncreasing = bool_flags & (1 << 0);
    m_Horizontal = bool_flags & (1 << 1);
    m_Vertical = bool_flags & (1 << 2);
    m_innerTrim = bool_flags & (1 << 3);

    if (m_trim_index != -1) {
	if (const ON_BrepTrim * const trim = brep.m_T.At(m_trim_index))
	    m_trim = trim->TrimCurveOf();
	else
	    bu_bomb("invalid trim index");
    }
}


void
BRNode::serialize(Serializer &serializer) const
{
    const uint8_t bool_flags = (m_XIncreasing << 0) |
	(m_Horizontal << 1) | (m_Vertical << 2) |
	(m_innerTrim << 3);

    serializer.write(m_t);
    for (int direction = 0; direction < 2; ++direction) {
	serializer.write_double(m_bbox_min[direction]);
	serializer.write_double(m_bbox_max[direction]);
	serializer.write_double(m_start[direction]);
	serializer.write_double(m_end[direction]);
    }

    serializer.write_uint8(bool_flags);
    serializer.write_int32(m_trim_index);
    serializer.write_int32(m_adj_face_index);
}


double
BRNode::curveBBoxDistance(const ON_2dPoint &uv) const
{
    double du = 0.0;
    double dv = 0.0;
    if (uv.x < m_bbox_min[X])
	du = m_bbox_min[X] - uv.x;
    else if (uv.x > m_bbox_max[X])
	du = uv.x - m_bbox_max[X];
    if (uv.y < m_bbox_min[Y])
	dv = m_bbox_min[Y] - uv.y;
    else if (uv.y > m_bbox_max[Y])
	dv = uv.y - m_bbox_max[Y];
    return hypot(du, dv);
}


double
BRNode::curveDistance(const ON_2dPoint &uv) const
{
    if (!m_trim)
	return INFINITY;
    const size_t sample_count = 8;
    size_t best_sample = 0;
    double best_squared = INFINITY;
    for (size_t sample = 0; sample <= sample_count; ++sample) {
	const double fraction = (double)sample / (double)sample_count;
	const ON_3dPoint point = m_trim->PointAt(m_t.ParameterAt(fraction));
	const double dx = point.x - uv.x;
	const double dy = point.y - uv.y;
	const double squared = dx * dx + dy * dy;
	if (squared < best_squared) {
	    best_squared = squared;
	    best_sample = sample;
	}
    }

    double low = best_sample ?
	(double)(best_sample - 1) / (double)sample_count : 0.0;
    double high = best_sample < sample_count ?
	(double)(best_sample + 1) / (double)sample_count : 1.0;
    const double golden = 0.6180339887498948482;
    double first = high - golden * (high - low);
    double second = low + golden * (high - low);
    auto squared_distance = [this, &uv](double fraction) {
	const ON_3dPoint point = m_trim->PointAt(m_t.ParameterAt(fraction));
	const double dx = point.x - uv.x;
	const double dy = point.y - uv.y;
	return dx * dx + dy * dy;
    };
    double first_squared = squared_distance(first);
    double second_squared = squared_distance(second);
    for (int iteration = 0; iteration < 24; ++iteration) {
	best_squared = std::min(best_squared,
	    std::min(first_squared, second_squared));
	if (first_squared < second_squared) {
	    high = second;
	    second = first;
	    second_squared = first_squared;
	    first = high - golden * (high - low);
	    first_squared = squared_distance(first);
	} else {
	    low = first;
	    first = second;
	    first_squared = second_squared;
	    second = low + golden * (high - low);
	    second_squared = squared_distance(second);
	}
    }
    best_squared = std::min(best_squared,
	std::min(first_squared, second_squared));
    return sqrt(std::max(0.0, best_squared));
}


bool
BRNode::crossesAbove(const ON_2dPoint &uv, double &distance,
	double endpoint_tolerance) const
{
    distance = -1.0;
    if (!m_trim || m_Vertical)
	return false;
    const double first_u = m_start[X];
    const double second_u = m_end[X];
    const bool exact =
	((first_u <= uv.x && uv.x < second_u) ||
	 (second_u <= uv.x && uv.x < first_u));
    double query_u = uv.x;
    if (!exact) {
	if (!(endpoint_tolerance > 0.0))
	    return false;
	/* A closed trim owns both coincident curve-domain endpoints.  Its exact
	 * half-open test already gives that vertex to one incident branch; the
	 * imported-loop gap allowance must not admit the other branch too. */
	if (m_trim->IsClosed())
	    return false;
	const double minimum_u = std::min(first_u, second_u);
	const double minimum_t = first_u < second_u ? m_t[0] : m_t[1];
	const ON_Interval curve_domain = m_trim->Domain();
	const double parameter_scale = std::max(1.0,
	    std::max(fabs(curve_domain.Min()), fabs(curve_domain.Max())));
	const double parameter_fuzz = 64.0 * DBL_EPSILON * parameter_scale;
	const bool minimum_is_trim_endpoint =
	    fabs(minimum_t - curve_domain.Min()) <= parameter_fuzz ||
	    fabs(minimum_t - curve_domain.Max()) <= parameter_fuzz;
	const double endpoint_fuzz = std::max(BREP_UV_DIST_FUZZ,
	    std::max(0.0, endpoint_tolerance));
	if (uv.x < minimum_u &&
		minimum_u - uv.x <= endpoint_fuzz &&
		minimum_is_trim_endpoint)
	    query_u = minimum_u;
	else
	    return false;
    }
    const double curve_v = getCurveEstimateOfV(query_u, 1.0e-7);
    distance = curve_v - uv.y;
    return distance >= 0.0;
}


bool
BRNode::crossesRight(const ON_2dPoint &uv, double &distance,
	double endpoint_tolerance) const
{
    distance = -1.0;
    if (!m_trim || m_Horizontal)
	return false;
    const double first_v = m_start[Y];
    const double second_v = m_end[Y];
    const bool exact =
	((first_v <= uv.y && uv.y < second_v) ||
	 (second_v <= uv.y && uv.y < first_v));
    double query_v = uv.y;
    if (!exact) {
	if (!(endpoint_tolerance > 0.0))
	    return false;
	if (m_trim->IsClosed())
	    return false;
	const double minimum_v = std::min(first_v, second_v);
	const double minimum_t = first_v < second_v ? m_t[0] : m_t[1];
	const ON_Interval curve_domain = m_trim->Domain();
	const double parameter_scale = std::max(1.0,
	    std::max(fabs(curve_domain.Min()), fabs(curve_domain.Max())));
	const double parameter_fuzz = 64.0 * DBL_EPSILON * parameter_scale;
	const bool minimum_is_trim_endpoint =
	    fabs(minimum_t - curve_domain.Min()) <= parameter_fuzz ||
	    fabs(minimum_t - curve_domain.Max()) <= parameter_fuzz;
	const double endpoint_fuzz = std::max(BREP_UV_DIST_FUZZ,
	    std::max(0.0, endpoint_tolerance));
	if (uv.y < minimum_v &&
		minimum_v - uv.y <= endpoint_fuzz &&
		minimum_is_trim_endpoint)
	    query_v = minimum_v;
	else
	    return false;
    }
    const double curve_u = getCurveEstimateOfU(query_v, 1.0e-7);
    distance = curve_u - uv.x;
    return distance >= 0.0;
}


fastf_t
BRNode::getCurveEstimateOfV(fastf_t u, fastf_t tol) const
{
    point_t A, B;
    double Ta, Tb;

    if (m_start[X] < m_end[X]) {
	VSET(A, m_start[X], m_start[Y], 0.0);
	VSET(B, m_end[X], m_end[Y], 0.0);
	Ta = m_t[0];
	Tb = m_t[1];
    } else {
	VSET(A, m_end[X], m_end[Y], 0.0);
	VSET(B, m_start[X], m_start[Y], 0.0);
	Ta = m_t[1];
	Tb = m_t[0];
    }

    fastf_t dU = B[X] - A[X];
    if (NEAR_ZERO(dU, tol))    /* vertical */
    {
	return A[Y];
    }

    ON_3dVector Tan_start = m_trim->TangentAt(Ta);
    ON_3dVector Tan_end = m_trim->TangentAt(Tb);

    fastf_t dT = Tb - Ta;
    fastf_t guess;
    ON_3dPoint p;

    /* Use quick binary subdivision until derivatives at end points in 'u' are within 5 percent */
    while (!NEAR_ZERO(dU, tol) && !NEAR_ZERO(dT, tol)) {
	guess = Ta + dT / 2;
	p = m_trim->PointAt(guess);

	if (UNLIKELY(NEAR_EQUAL(p[X], u, SMALL_FASTF))) {
	    /* hit 'u' exactly, done deal */
	    return p[Y];
	}

	if (p[X] > u) {
	    /* v is behind us, back up the end */
	    Tb = guess;
	    VMOVE(B, p);
	    Tan_end = m_trim->TangentAt(Tb);
	} else {
	    /* v is in front, move start forward */
	    Ta = guess;
	    VMOVE(A, p);
	    Tan_start = m_trim->TangentAt(Ta);
	}
	dT = Tb - Ta;
	dU = B[X] - A[X];
    }

    dU = B[X] - A[X];
    if (NEAR_ZERO(dU, tol))    /* vertical */
    {
	return A[Y];
    }

    guess = Ta + (u - A[X]) * dT / dU;
    p = m_trim->PointAt(guess);

    int cnt = 0;
    while ((cnt < 1000) && (!NEAR_EQUAL(p[X], u, tol))) {
	if (p[X] < u) {
	    Ta = guess;
	    VMOVE(A, p);
	} else {
	    Tb = guess;
	    VMOVE(B, p);
	}
	dU = B[X] - A[X];
	if (NEAR_ZERO(dU, tol)) {  /* vertical */
	    return A[Y];
	}

	dT = Tb - Ta;
	guess = Ta + (u - A[X]) * dT / dU;
	p = m_trim->PointAt(guess);
	cnt++;
    }
    if (cnt > 999) {
	bu_log("getCurveEstimateOfV(): estimate of 'v' given a trim curve and "
	       "'u' did not converge within iteration bound(%d).\n", cnt);
    }
    return p[Y];
}


fastf_t
BRNode::getCurveEstimateOfU(fastf_t v, fastf_t tol) const
{
    point_t A, B;
    double Ta, Tb;

    if (m_start[Y] < m_end[Y]) {
	VSET(A, m_start[X], m_start[Y], 0.0);
	VSET(B, m_end[X], m_end[Y], 0.0);
	Ta = m_t[0];
	Tb = m_t[1];
    } else {
	VSET(A, m_end[X], m_end[Y], 0.0);
	VSET(B, m_start[X], m_start[Y], 0.0);
	Ta = m_t[1];
	Tb = m_t[0];
    }

    fastf_t dV = B[Y] - A[Y];
    if (NEAR_ZERO(dV, tol))    /* horizontal */
    {
	return A[X];
    }

    ON_3dVector Tan_start = m_trim->TangentAt(Ta);
    ON_3dVector Tan_end = m_trim->TangentAt(Tb);

    fastf_t dT = Tb - Ta;
    fastf_t guess;
    ON_3dPoint p;

    /* Use quick binary subdivision until derivatives at end points in 'u' are within 5 percent */
    while (!NEAR_ZERO(dV, tol) && !NEAR_ZERO(dT, tol)) {
	guess = Ta + dT / 2;
	p = m_trim->PointAt(guess);
	if (p[Y] < v) {
	    Ta = guess;
	    VMOVE(A, p);
	    Tan_start = m_trim->TangentAt(Ta);
	} else {
	    Tb = guess;
	    VMOVE(B, p);
	    Tan_end = m_trim->TangentAt(Tb);
	}
	dT = Tb - Ta;
	dV = B[Y] - A[Y];
    }

    dV = B[Y] - A[Y];
    if (NEAR_ZERO(dV, tol))    /* horizontal */
    {
	return A[X];
    }

    guess = Ta + (v - A[Y]) * dT / dV;
    p = m_trim->PointAt(guess);

    int cnt = 0;
    while ((cnt < 1000) && (!NEAR_EQUAL(p[Y], v, tol))) {
	if (p[Y] < v) {
	    Ta = guess;
	    VMOVE(A, p);
	} else {
	    Tb = guess;
	    VMOVE(B, p);
	}
	dV = B[Y] - A[Y];
	if (NEAR_ZERO(dV, tol)) {  /* horizontal */
	    return A[X];
	}
	dT = Tb - Ta;
	guess = Ta + (v - A[Y]) * dT / dV;
	p = m_trim->PointAt(guess);
	cnt++;
    }
    if (cnt > 999) {
	bu_log("getCurveEstimateOfV(): estimate of 'u' given a trim curve and "
	       "'v' did not converge within iteration bound(%d).\n", cnt);
    }
    return p[X];
}
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
