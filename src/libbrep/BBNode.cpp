/*                      B B N O D E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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
#include "bu.h"
#include "brep.h"

namespace brlcad {
BBNode::~BBNode()
{
    /* delete the children */
    for (size_t i = 0; i < m_children.size(); i++) {
	delete m_children[i];
    }
}

bool
BBNode::intersectsHierarchy(ON_Ray &ray, std::list<BBNode *> &results_opt)
{
    double tnear, tfar;
    bool intersects = intersectedBy(ray, &tnear, &tfar);
    if (intersects && isLeaf()) {
	results_opt.push_back(this);
    } else if (intersects) {
	for (size_t i = 0; i < m_children.size(); i++) {
	    m_children[i]->intersectsHierarchy(ray, results_opt);
	}
    }
    return intersects;
}

bool
BBNode::containsUV(const ON_2dPoint &uv)
{
    if ((uv[0] > m_u[0]) && (uv[0] < m_u[1]) && (uv[1] > m_v[0]) && (uv[1] < m_v[1])) {
	return true;
    } else {
	return false;
    }
}

int
BBNode::depth()
{
    int d = 0;
    for (size_t i = 0; i < m_children.size(); i++) {
	d = 1 + std::max(d, m_children[i]->depth());
    }
    return d;
}

void
BBNode::getLeaves(std::list<BBNode *> &out_leaves)
{
    if (m_children.size() > 0) {
	for (size_t i = 0; i < m_children.size(); i++) {
	    m_children[i]->getLeaves(out_leaves);
	}
    } else {
	out_leaves.push_back(this);
    }
}

BBNode *
BBNode::closer(const ON_3dPoint &pt, BBNode *left, BBNode *right)
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

ON_2dPoint
BBNode::getClosestPointEstimate(const ON_3dPoint &pt)
{
    ON_Interval u, v;
    return getClosestPointEstimate(pt, u, v);
}

ON_2dPoint
BBNode::getClosestPointEstimate(const ON_3dPoint &pt, ON_Interval &u, ON_Interval &v)
{
    if (isLeaf()) {
	double uvs[5][2] = {{m_u.Min(), m_v.Min()}, {m_u.Max(), m_v.Min()},
	    {m_u.Max(), m_v.Max()}, {m_u.Min(), m_v.Max()},
	    {m_u.Mid(), m_v.Mid()}
	}; /* include the estimate */
	ON_3dPoint corners[5];
	const ON_Surface *surf = m_face->SurfaceOf();

	u = m_u;
	v = m_v;

	/* ??? pass these in from SurfaceTree::surfaceBBox() to avoid
	 * this recalculation?
	 */
	if (!surf->EvPoint(uvs[0][0], uvs[0][1], corners[0]) ||
	    !surf->EvPoint(uvs[1][0], uvs[1][1], corners[1]) ||
	    !surf->EvPoint(uvs[2][0], uvs[2][1], corners[2]) ||
	    !surf->EvPoint(uvs[3][0], uvs[3][1], corners[3]))
	{
	    throw new std::exception(); /* FIXME */
	}
	corners[4] = BBNode::m_estimate;

	/* find the point on the surface closest to pt */
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
	if (m_children.size() > 0) {
	    BBNode *closestNode = m_children[0];
	    for (size_t i = 1; i < m_children.size(); i++) {
		closestNode = closer(pt, closestNode, m_children[i]);
		TRACE("\t" << PT(closestNode->m_estimate));
	    }
	    return closestNode->getClosestPointEstimate(pt, u, v);
	}
	throw new std::exception();
    }
}

int
BBNode::getLeavesBoundingPoint(const ON_3dPoint &pt, std::list<BBNode *> &out)
{
    if (isLeaf()) {
	double min[3], max[3];
	GetBBox(min, max);
	if ((pt.x >= (min[0])) && (pt.x <= (max[0])) &&
	    (pt.y >= (min[1])) && (pt.y <= (max[1])) &&
	    (pt.z >= (min[2])) && (pt.z <= (max[2])))
	{
	    /* falls within BBox so put in list */
	    out.push_back(this);
	    return 1;
	}
	return 0;
    } else {
	int sum = 0;
	for (size_t i = 0; i < m_children.size(); i++) {
	    sum += m_children[i]->getLeavesBoundingPoint(pt, out);
	}
	return sum;
    }
}

int
BBNode::isTrimmed(const ON_2dPoint &uv, BRNode **closest, double &closesttrim) const
{
    BRNode *br;
    std::list<BRNode *> trims;

    closesttrim = -1.0;
    if (m_checkTrim) {
	getTrimsAbove(uv, trims);

	if (trims.empty()) {
	    return 1;
	} else { /* find closest BB */
	    std::list<BRNode *>::iterator i;
	    BRNode *vclosest = NULL;
	    BRNode *uclosest = NULL;
	    fastf_t currHeight = (fastf_t)0.0;
	    bool currTrimStatus = false;
	    bool verticalTrim = false;
	    bool underTrim = false;
	    double vdist = 0.0;
	    double udist = 0.0;

	    for (i = trims.begin(); i != trims.end(); i++) {
		br = dynamic_cast<BRNode *>(*i);

		/* skip if trim below */
		if (br->m_node.m_max[1] + BREP_EDGE_MISS_TOLERANCE < uv[Y]) {
		    continue;
		}

		if (br->m_Vertical) {
		    if ((br->m_v[0] <= uv[Y]) && (br->m_v[1] >= uv[Y])) {
			double dist = fabs(uv[X] - br->m_v[0]);
			if (!verticalTrim) { /* haven't seen vertical trim yet */
			    verticalTrim = true;
			    vdist = dist;
			    vclosest = br;
			} else {
			    if (dist < vdist) {
				vdist = dist;
				vclosest = br;
			    }
			}

		    }
		    continue;
		}
		double v;
		int trimstatus = br->isTrimmed(uv, v);
		if (v >= 0.0) {
		    if (closest && *closest == NULL) {
			currHeight = v;
			currTrimStatus = trimstatus;
			if (closest) {
			    *closest = br;
			}
		    } else if (v < currHeight) {
			currHeight = v;
			currTrimStatus = trimstatus;
			if (closest) {
			    *closest = br;
			}
		    }
		} else {
		    double dist = fabs(v);
		    if (!underTrim) {
			underTrim = true;
			udist = dist;
			uclosest = br;
		    } else {
			if (dist < udist) {
			    udist = dist;
			}
			uclosest = br;
		    }
		}
	    }
	    if (closest && *closest == NULL) {
		if (verticalTrim) {
		    closesttrim = vdist;
		    if (closest) {
			*closest = vclosest;
		    }
		}
		if ((underTrim) && (!verticalTrim || (udist < closesttrim))) {
		    closesttrim = udist;
		    if (closest) {
			*closest = uclosest;
		    }
		}
		return 1;
	    } else {
		closesttrim = currHeight;
		if ((verticalTrim) && (vdist < closesttrim)) {
		    closesttrim = vdist;
		    if (closest) {
			*closest = vclosest;
		    }
		}
		if ((underTrim) && (udist < closesttrim)) {
		    closesttrim = udist;
		    if (closest) {
			*closest = uclosest;
		    }
		}
		return currTrimStatus;
	    }
	}
    } else {
	if (m_trimmed) {
	    return 1;
	} else {
	    return 0;
	}
    }
}

void
BBNode::getTrimsAbove(const ON_2dPoint &uv, std::list<BRNode *> &out_leaves) const
{
    point_t bmin, bmax;
    double dist;
    for (std::list<BRNode *>::const_iterator i = m_trims_above.begin(); i != m_trims_above.end(); i++) {
	BRNode *br = dynamic_cast<BRNode *>(*i);
	br->GetBBox(bmin, bmax);
	dist = 0.000001; /* 0.03*DIST_PT_PT(bmin, bmax); */
	if ((uv[X] > bmin[X] - dist) && (uv[X] < bmax[X] + dist)) {
	    out_leaves.push_back(br);
	}
    }
}

void BBNode::BuildBBox()
{
    if (m_children.size() > 0) {
	for (std::vector<BBNode *>::iterator childnode = m_children.begin(); childnode != m_children.end(); childnode++) {
	    if (!(*childnode)->isLeaf()) {
		(*childnode)->BuildBBox();
	    }
	    if (childnode == m_children.begin()) {
		m_node = ON_BoundingBox((*childnode)->m_node.m_min, (*childnode)->m_node.m_max);
	    } else {
		for (int j = 0; j < 3; j++) {
		    if (m_node.m_min[j] > (*childnode)->m_node.m_min[j]) {
			m_node.m_min[j] = (*childnode)->m_node.m_min[j];
		    }
		    if (m_node.m_max[j] < (*childnode)->m_node.m_max[j]) {
			m_node.m_max[j] = (*childnode)->m_node.m_max[j];
		    }
		}
	    }
	}
    }
}

bool
BBNode::prepTrims()
{
    CurveTree *ct = m_ctree;
    std::list<BRNode *>::iterator i;
    BRNode *br;
    point_t curvemin, curvemax;
    double dist = 0.000001;
    bool trim_already_assigned = false;

    m_trims_above.clear();

    if (LIKELY(ct != NULL)) {
	ct->getLeavesAbove(m_trims_above, m_u, m_v);
    }

    m_trims_above.sort(sortY);

    if (!m_trims_above.empty()) {
	i = m_trims_above.begin();
	while (i != m_trims_above.end()) {
	    br = dynamic_cast<BRNode *>(*i);
	    if (br->m_Vertical) { /* check V to see if trim possibly overlaps */
		br->GetBBox(curvemin, curvemax);
		if (curvemin[Y] - dist <= m_v[1]) {
		    /* possibly contains trim can't rule out check
		     * closer */
		    m_checkTrim = true;
		    trim_already_assigned = true;
		    i++;
		} else {
		    i = m_trims_above.erase(i);
		}
	    } else {
		i++;
	    }
	}
    }

    if (!trim_already_assigned) { /* already contains possible vertical trim */
	if (m_trims_above.empty() /*|| m_trims_right.empty()*/) {
	    m_trimmed = true;
	    m_checkTrim = false;
	} else if (!m_trims_above.empty()) { /*trimmed above check contains */
	    i = m_trims_above.begin();
	    br = dynamic_cast<BRNode *>(*i);
	    br->GetBBox(curvemin, curvemax);
	    dist = 0.000001; /* 0.03*DIST_PT_PT(curvemin, curvemax); */
	    if (curvemin[Y] - dist > m_v[1]) {
		i++;

		if (i == m_trims_above.end()) { /* easy only trim in above list */
		    if (br->m_XIncreasing) {
			m_trimmed = true;
			m_checkTrim = false;
		    } else {
			m_trimmed = false;
			m_checkTrim = false;
		    }
		} else {
		    /* check for trim bbox overlap TODO: look for
		     * multiple overlaps.
		     */
		    BRNode *bs;
		    bs = dynamic_cast<BRNode *>(*i);
		    point_t smin, smax;
		    bs->GetBBox(smin, smax);
		    if ((smin[Y] >= curvemax[Y]) || (smin[X] >= curvemax[X]) || (smax[X] <= curvemin[X])) { /* can determine inside/outside without closer inspection */
			if (br->m_XIncreasing) {
			    m_trimmed = true;
			    m_checkTrim = false;
			} else {
			    m_trimmed = false;
			    m_checkTrim = false;
			}
		    } else {
			m_checkTrim = true;
		    }
		}
	    } else {
		m_checkTrim = true;
	    }
	} else { /* something wrong here */
	    bu_log("Error prepping trims");
	    return false;
	}
    }
    return true;
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
