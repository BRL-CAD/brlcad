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
    const ON_BrepFace *face,
    const ON_Interval &t,
    bool innerTrim,
    bool checkTrim,
    bool trimmed) :
    m_node(node),
    m_v(),
    m_adj_face_index(adj_face_index),
    m_XIncreasing(false),
    m_Horizontal(false),
    m_Vertical(false),
    m_innerTrim(innerTrim),
    m_stl(new Stl),
    m_face(face),
    m_u(),
    m_trim(curve),
    m_trim_index(trim_index),
    m_t(t),
    m_checkTrim(checkTrim),
    m_trimmed(trimmed),
    m_estimate(),
    m_slope(0.0),
    m_bb_diag(0.0),
    m_start(curve->PointAt(m_t[0])),
    m_end(curve->PointAt(m_t[1]))
{
    /* check for vertical segments they can be removed from trims
     * above (can't tell direction and don't need
     */
    m_Horizontal = false;
    m_Vertical = false;

    /*
     * should be okay since we split on Horz/Vert tangents
     */
    if (m_end[X] < m_start[X]) {
	m_u[0] = m_end[X];
	m_u[1] = m_start[X];
    } else {
	m_u[0] = m_start[X];
	m_u[1] = m_end[X];
    }
    if (m_end[Y] < m_start[Y]) {
	m_v[0] = m_end[Y];
	m_v[1] = m_start[Y];
    } else {
	m_v[0] = m_start[Y];
	m_v[1] = m_end[Y];
    }

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
	m_slope = 0.0;
    } else {
	if ((m_end[X] - m_start[X]) > 0.0) {
	    m_XIncreasing = true;
	} else {
	    m_XIncreasing = false;
	}
	m_slope = (m_end[Y] - m_start[Y]) / (m_end[X] - m_start[X]);
    }
    m_bb_diag = DIST_PNT_PNT(m_start, m_end);
}


BRNode::BRNode(const ON_BoundingBox &node) :
    m_node(node),
    m_v(),
    m_adj_face_index(-1),
    m_XIncreasing(false),
    m_Horizontal(false),
    m_Vertical(false),
    m_innerTrim(false),
    m_stl(new Stl),
    m_face(NULL),
    m_u(),
    m_trim(NULL),
    m_trim_index(-1),
    m_t(),
    m_checkTrim(true),
    m_trimmed(false),
    m_estimate(),
    m_slope(0.0),
    m_bb_diag(0.0),
    m_start(ON_3dPoint::UnsetPoint),
    m_end(ON_3dPoint::UnsetPoint)
{
    for (int i = 0; i < 3; i++) {
	double d = m_node.m_max[i] - m_node.m_min[i];
	if (NEAR_ZERO(d, ON_ZERO_TOLERANCE)) {
	    m_node.m_min[i] -= 0.001;
	    m_node.m_max[i] += 0.001;
	}
    }
    m_start = m_node.m_min;
    m_end = m_node.m_max;
}


BRNode::~BRNode()
{
    /* delete the children */
    for (size_t i = 0; i < m_stl->m_children.size(); i++) {
	delete m_stl->m_children[i];
    }

    delete m_stl;
}


BRNode::BRNode(Deserializer &deserializer, const ON_Brep &brep) :
    m_node(),
    m_v(),
    m_adj_face_index(-1),
    m_XIncreasing(false),
    m_Horizontal(false),
    m_Vertical(false),
    m_innerTrim(false),
    m_stl(new Stl),
    m_face(NULL),
    m_u(),
    m_trim(NULL),
    m_trim_index(-1),
    m_t(),
    m_checkTrim(false),
    m_trimmed(false),
    m_estimate(),
    m_slope(0.0),
    m_bb_diag(0.0),
    m_start(ON_3dPoint::UnsetPoint),
    m_end(ON_3dPoint::UnsetPoint)
{
    deserializer.read(m_node);
    deserializer.read(m_u);
    deserializer.read(m_v);
    deserializer.read(m_t);
    deserializer.read(m_start);
    deserializer.read(m_end);
    deserializer.read(m_estimate);

    m_slope = deserializer.read_double();
    m_bb_diag = deserializer.read_double();

    const uint8_t bool_flags = deserializer.read_uint8();
    const int face_index = deserializer.read_int32();
    m_trim_index = deserializer.read_int32();
    m_adj_face_index = deserializer.read_uint32();
    const std::size_t num_children = deserializer.read_uint32();

    m_XIncreasing = bool_flags & (1 << 0);
    m_Horizontal = bool_flags & (1 << 1);
    m_Vertical = bool_flags & (1 << 2);
    m_innerTrim = bool_flags & (1 << 3);
    m_checkTrim = bool_flags & (1 << 4);
    m_trimmed = bool_flags & (1 << 5);

    if (face_index != -1) {
	if (const ON_BrepFace * const face = brep.m_F.At(face_index))
	    m_face = face;
	else
	    bu_bomb("invalid face index");
    }

    if (m_trim_index != -1) {
	if (const ON_BrepTrim * const trim = brep.m_T.At(m_trim_index))
	    m_trim = trim->TrimCurveOf();
	else
	    bu_bomb("invalid face index");
    }

    m_stl->m_children.resize(num_children);
    for (std::vector<const BRNode *>::iterator it = m_stl->m_children.begin(); it != m_stl->m_children.end(); ++it)
	*it = new BRNode(deserializer, brep);
}


void
BRNode::serialize(Serializer &serializer) const
{
    const uint8_t bool_flags = (m_XIncreasing << 0) | (m_Horizontal << 1) | (m_Vertical << 2) | (m_innerTrim << 3) | (m_checkTrim << 4) | (m_trimmed << 5);

    serializer.write(m_node);
    serializer.write(m_u);
    serializer.write(m_v);
    serializer.write(m_t);
    serializer.write(m_start);
    serializer.write(m_end);
    serializer.write(m_estimate);

    serializer.write_double(m_slope);
    serializer.write_double(m_bb_diag);

    serializer.write_uint8(bool_flags);
    serializer.write_int32(m_face ? m_face->m_face_index : -1);
    serializer.write_int32(m_trim_index);
    serializer.write_uint32(m_adj_face_index);
    serializer.write_uint32(m_stl->m_children.size());

    for (std::vector<const BRNode *>::const_iterator it = m_stl->m_children.begin(); it != m_stl->m_children.end(); ++it)
	(*it)->serialize(serializer);
}


int
BRNode::depth() const
{
    int d = 0;
    for (size_t i = 0; i < m_stl->m_children.size(); i++) {
	d = 1 + std::max(d, m_stl->m_children[i]->depth());
    }
    return d;
}


void
BRNode::getLeaves(std::list<const BRNode *> &out_leaves) const
{
    if (!m_stl->m_children.empty()) {
	for (size_t i = 0; i < m_stl->m_children.size(); i++) {
	    m_stl->m_children[i]->getLeaves(out_leaves);
	}
    } else {
	out_leaves.push_back(this);
    }
}


const BRNode *
BRNode::closer(const ON_3dPoint &pt, const BRNode *left, const BRNode *right) const
{
    double ldist = pt.DistanceTo(left->m_estimate);
    double rdist = pt.DistanceTo(right->m_estimate);
    TRACE("\t" << ldist << " < " << rdist);
    if (ldist < rdist) {
	return left;
    } else {
	return right;
    }
}


double
BRNode::curveBBoxDistance(const ON_2dPoint &uv) const
{
    double du = 0.0;
    double dv = 0.0;
    if (uv.x < m_node.m_min.x)
	du = m_node.m_min.x - uv.x;
    else if (uv.x > m_node.m_max.x)
	du = uv.x - m_node.m_max.x;
    if (uv.y < m_node.m_min.y)
	dv = m_node.m_min.y - uv.y;
    else if (uv.y > m_node.m_max.y)
	dv = uv.y - m_node.m_max.y;
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
    const double first_u = m_start.x;
    const double second_u = m_end.x;
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
    const double first_v = m_start.y;
    const double second_v = m_end.y;
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


bool
BRNode::isTrimmed(const ON_2dPoint &uv, double &trimdist) const
{
    point_t bmin, bmax;
    BRNode::GetBBox(bmin, bmax);
    /* CurveTree candidate selection admits this same UV fuzz.  Preserve it
     * here so a candidate just beyond a split or imported trim endpoint is
     * classified instead of being silently discarded by a stricter second
     * bounding-box test.
     */
    if ((bmin[X] - BREP_UV_DIST_FUZZ <= uv[X]) &&
	    (uv[X] <= bmax[X] + BREP_UV_DIST_FUZZ))
    {
	const fastf_t query_u = std::max(bmin[X],
	    std::min(uv[X], bmax[X]));
	fastf_t v = getCurveEstimateOfV(query_u, 0.0000001);
	trimdist = v - uv[Y];
	if (uv[Y] <= v) {
	    if (m_XIncreasing) {
		return true;
	    } else {
		return false;
	    }
	} else if (uv[Y] > v) {
	    if (!m_XIncreasing) {
		return true;
	    } else {
		return false;
	    }
	} else {
	    return true;
	}
    } else {
	trimdist = -1.0;
	if (m_trimmed) {
	    return true;
	} else {
	    return false;
	}
    }
}


ON_2dPoint
BRNode::getClosestPointEstimate(const ON_3dPoint &pt) const
{
    ON_Interval u, v;
    return getClosestPointEstimate(pt, u, v);
}


ON_2dPoint
BRNode::getClosestPointEstimate(const ON_3dPoint &pt, ON_Interval &u, ON_Interval &v) const
{
    if (isLeaf()) {
	double uvs[5][2] = {{m_u.Min(), m_v.Min()},  /* include the corners for an easy refinement */
			    {m_u.Max(), m_v.Min()},
			    {m_u.Max(), m_v.Max()},
			    {m_u.Min(), m_v.Max()},
			    {m_u.Mid(), m_v.Mid()}
	}; /* include the estimate */
	ON_3dPoint corners[5];
	const ON_Surface *surf = m_face->SurfaceOf();

	u = m_u;
	v = m_v;

	/* ??? should we pass these in from SurfaceTree::curveBBox()
	 * to avoid this recalculation?
	 */
	if (!surf->EvPoint(uvs[0][0], uvs[0][1], corners[0]) ||
	    !surf->EvPoint(uvs[1][0], uvs[1][1], corners[1]) ||
	    !surf->EvPoint(uvs[2][0], uvs[2][1], corners[2]) ||
	    !surf->EvPoint(uvs[3][0], uvs[3][1], corners[3]))
	{
	    throw std::exception(); /* FIXME */
	}
	corners[4] = BRNode::m_estimate;

	/* find the point on the curve closest to pt */
	size_t mini = 0;
	double mindist = pt.DistanceTo(corners[mini]);
	double tmpdist;
	for (size_t i = 1; i < 5; i++) {
	    tmpdist = pt.DistanceTo(corners[i]);
	    TRACE("\t" << mindist << " < " << tmpdist);
	    if (tmpdist < mindist) {
		mini = i;
		mindist = tmpdist;
	    }
	}
	TRACE("Closest: " << mindist << "; " << PT2(uvs[mini]));
	return ON_2dPoint(uvs[mini][0], uvs[mini][1]);
    } else {
	if (!m_stl->m_children.empty()) {
	    const BRNode *closestNode = m_stl->m_children[0];
	    for (size_t i = 1; i < m_stl->m_children.size(); i++) {
		closestNode = closer(pt, closestNode, m_stl->m_children[i]);
	    }
	    return closestNode->getClosestPointEstimate(pt, u, v);
	} else {
	    throw std::exception();
	}
    }
}


fastf_t
BRNode::getLinearEstimateOfV(fastf_t u) const
{
    fastf_t v = m_start[Y] + m_slope * (u - m_start[X]);
    return v;
}


fastf_t
BRNode::getCurveEstimateOfV(fastf_t u, fastf_t tol) const
{
    point_t A, B;
    double Ta, Tb;

    if (m_start[X] < m_end[X]) {
	VMOVE(A, m_start);
	VMOVE(B, m_end);
	Ta = m_t[0];
	Tb = m_t[1];
    } else {
	VMOVE(A, m_end);
	VMOVE(B, m_start);
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
	VMOVE(A, m_start);
	VMOVE(B, m_end);
	Ta = m_t[0];
	Tb = m_t[1];
    } else {
	VMOVE(A, m_end);
	VMOVE(B, m_start);
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
