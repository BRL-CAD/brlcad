/*                      B R N O D E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2016 United States Government as represented by
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

/* library headers */
extern "C" {
#include "bu/log.h"
}
#include "brep.h"


namespace brlcad {
BRNode::~BRNode()
{
    /* delete the children */
    for (size_t i = 0; i < m_children->size(); i++) {
	delete (*m_children)[i];
    }

    delete m_children;
}


BRNode::BRNode(ON_BinaryArchive &archive, const ON_Brep &brep) :
    m_node(),
    m_v(),
    m_adj_face_index(-99),
    m_XIncreasing(false),
    m_Horizontal(false),
    m_Vertical(false),
    m_innerTrim(false),
    m_children(new std::vector<const BRNode *>),
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
    if (!archive.ReadBoundingBox(m_node) || !archive.ReadInterval(m_v)
	    || !archive.ReadInt(&m_adj_face_index) || !archive.ReadBool(&m_XIncreasing)
	    || !archive.ReadBool(&m_Horizontal) || !archive.ReadBool(&m_Vertical)
	    || !archive.ReadBool(&m_innerTrim))
	bu_bomb("ON_BinaryArchive read failed");

    std::size_t num_children;
    if (!archive.ReadBigSize(&num_children))
	bu_bomb("ON_BinaryArchive read failed");

    for (std::size_t i = 0; i < num_children; ++i)
	m_children->push_back(new BRNode(archive, brep));

    int face_index;
    if (!archive.ReadInt(&face_index))
	bu_bomb("ON_BinaryArchive read failed");

    if (face_index != -1 && !(m_face = brep.m_F.At(face_index)))
	bu_bomb("invalid face index");

    if (!archive.ReadInterval(m_u) || !archive.ReadInt(&m_trim_index))
	bu_bomb("ON_BinaryArchive read failed");

    if (m_trim_index != -1 && !brep.m_T.At(m_trim_index))
	bu_bomb("invalid trim index");

    if (m_trim_index != -1) m_trim = brep.m_T[m_trim_index].TrimCurveOf();

    double temp_slope, temp_bb_diag;

    if (!archive.ReadInterval(m_t) || !archive.ReadBool(&m_checkTrim)
	    || !archive.ReadBool(&m_trimmed) || !archive.ReadPoint(m_estimate)
	    || !archive.ReadDouble(&temp_slope) || !archive.ReadDouble(&temp_bb_diag)
	    || !archive.ReadPoint(m_start) || !archive.ReadPoint(m_end))
	bu_bomb("ON_BinaryArchive read failed");

    m_slope = temp_slope;
    m_bb_diag = temp_bb_diag;
}


void
BRNode::serialize(ON_BinaryArchive &archive) const
{
    if (!archive.WriteBoundingBox(m_node) || !archive.WriteInterval(m_v)
	    || !archive.WriteInt(m_adj_face_index) || !archive.WriteBool(m_XIncreasing)
	    || !archive.WriteBool(m_Horizontal) || !archive.WriteBool(m_Vertical)
	    || !archive.WriteBool(m_innerTrim))
	bu_bomb("ON_BinaryArchive write failed");

    if (!archive.WriteBigSize(m_children->size()))
	bu_bomb("ON_BinaryArchive write failed");

    for (std::vector<const BRNode *>::const_iterator it = m_children->begin(); it != m_children->end(); ++it)
	(*it)->serialize(archive);

    if (!archive.WriteInt(m_face ? m_face->m_face_index : -1))
	bu_bomb("ON_BinaryArchive write failed");

    if (!archive.WriteInterval(m_u) || !archive.WriteInt(m_trim_index)
	    || !archive.WriteInterval(m_t) || !archive.WriteBool(m_checkTrim)
	    || !archive.WriteBool(m_trimmed) || !archive.WritePoint(m_estimate)
	    || !archive.WriteDouble(m_slope) || !archive.WriteDouble(m_bb_diag)
	    || !archive.WritePoint(m_start) || !archive.WritePoint(m_end))
	bu_bomb("ON_BinaryArchive write failed");
}


int
BRNode::depth() const
{
    int d = 0;
    for (size_t i = 0; i < m_children->size(); i++) {
	d = 1 + std::max(d, (*m_children)[i]->depth());
    }
    return d;
}

void
BRNode::getLeaves(std::list<const BRNode *> &out_leaves) const
{
    if (!m_children->empty()) {
	for (size_t i = 0; i < m_children->size(); i++) {
	    (*m_children)[i]->getLeaves(out_leaves);
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

bool
BRNode::isTrimmed(const ON_2dPoint &uv, double &trimdist) const
{
    point_t bmin, bmax;
    BRNode::GetBBox(bmin, bmax);
    if ((bmin[X] <= uv[X]) && (uv[X] <= bmax[X]))   /* if check trim and in BBox */
    {
	fastf_t v = getCurveEstimateOfV(uv[X], 0.0000001);
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
	    throw new std::exception(); /* FIXME */
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
	if (!m_children->empty()) {
	    const BRNode *closestNode = (*m_children)[0];
	    for (size_t i = 1; i < m_children->size(); i++) {
		closestNode = closer(pt, closestNode, (*m_children)[i]);
	    }
	    return closestNode->getClosestPointEstimate(pt, u, v);
	} else {
	    throw new std::exception();
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
    ON_3dVector tangent;
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
    ON_3dVector tangent;
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
