/*                      B B N O D E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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

#include "bu/log.h"
#include "brep/bbnode.h"


namespace brlcad {


BBNode::BBNode(const ON_BoundingBox &node, const CurveTree *ct) :
    m_node(node),
    m_u(),
    m_v(),
    m_checkTrim(true),
    m_trimmed(false),
    m_estimate(),
    m_normal(),
    m_ctree(ct),
    m_stl(new Stl)
{
    for (int i = 0; i < 3; i++) {
	double d = m_node.m_max[i] - m_node.m_min[i];
	if (ON_NearZero(d, ON_ZERO_TOLERANCE)) {
	    m_node.m_min[i] -= 0.001;
	    m_node.m_max[i] += 0.001;
	}
    }
}


BBNode::BBNode(
    const CurveTree *ct,
    const ON_BoundingBox &node,
    const ON_Interval &u,
    const ON_Interval &v,
    bool checkTrim,
    bool trimmed):
    m_node(node),
    m_u(u),
    m_v(v),
    m_checkTrim(checkTrim),
    m_trimmed(trimmed),
    m_estimate(),
    m_normal(),
    m_ctree(ct),
    m_stl(new Stl)
{
    for (int i = 0; i < 3; i++) {
	double d = m_node.m_max[i] - m_node.m_min[i];
	if (ON_NearZero(d, ON_ZERO_TOLERANCE)) {
	    m_node.m_min[i] -= 0.001;
	    m_node.m_max[i] += 0.001;
	}
    }
}


BBNode::~BBNode()
{
    /* delete the children */
    for (size_t i = 0; i < m_stl->m_children.size(); i++) {
	delete m_stl->m_children[i];
    }

    delete m_stl;
}


BBNode::BBNode(Deserializer &deserializer, const CurveTree &ctree) :
    m_node(),
    m_u(),
    m_v(),
    m_checkTrim(false),
    m_trimmed(false),
    m_estimate(),
    m_normal(),
    m_ctree(&ctree),
    m_stl(new Stl)
{
    deserializer.read(m_node);
    deserializer.read(m_u);
    deserializer.read(m_v);
    deserializer.read(m_estimate);
    deserializer.read(m_normal);

    const uint8_t bool_flags = deserializer.read_uint8();
    const std::size_t num_leaves_keys = deserializer.read_uint32();
    const std::size_t num_children = deserializer.read_uint32();

    m_checkTrim = bool_flags & (1 << 0);
    m_trimmed = bool_flags & (1 << 1);

    m_stl->m_children.resize(num_children);
    for (std::vector<BBNode *>::iterator it = m_stl->m_children.begin(); it != m_stl->m_children.end(); ++it)
	*it = new BBNode(deserializer, ctree);

    std::size_t buffer[8];
    std::size_t *leaves_keys = buffer;
    if (num_leaves_keys > sizeof(buffer) / sizeof(buffer[0]))
	leaves_keys = new std::size_t[num_leaves_keys];

    for (std::size_t i = 0; i < num_leaves_keys; ++i)
	leaves_keys[i] = deserializer.read_uint32();

    m_stl->m_trims_above = m_ctree->serialize_get_leaves(leaves_keys, num_leaves_keys);

    if (leaves_keys != buffer)
	delete[] leaves_keys;
}


void
BBNode::serialize(Serializer &serializer) const
{
    const std::vector<std::size_t> leaves_keys = m_ctree->serialize_get_leaves_keys(m_stl->m_trims_above);
    const uint8_t bool_flags = (m_checkTrim << 0) | (m_trimmed << 1);

    serializer.write(m_node);
    serializer.write(m_u);
    serializer.write(m_v);
    serializer.write(m_estimate);
    serializer.write(m_normal);

    serializer.write_uint8(bool_flags);
    serializer.write_uint32(leaves_keys.size());
    serializer.write_uint32(m_stl->m_children.size());

    for (std::vector<std::size_t>::const_iterator it = leaves_keys.begin(); it != leaves_keys.end(); ++it)
	serializer.write_uint32(*it);

    for (std::vector<BBNode *>::const_iterator it = m_stl->m_children.begin(); it != m_stl->m_children.end(); ++it)
	(*it)->serialize(serializer);
}


bool
BBNode::intersectedBy(const ON_Ray &ray, double *tnear_opt /* = NULL */, double *tfar_opt /* = NULL */) const
{
    double tnear = -DBL_MAX;
    double tfar = DBL_MAX;
    bool untrimmedresult = true;
    for (int i = 0; i < 3; i++) {
	if (UNLIKELY(ON_NearZero(ray.m_dir[i]))) {
	    if (ray.m_origin[i] < m_node.m_min[i] || ray.m_origin[i] > m_node.m_max[i]) {
		untrimmedresult = false;
	    }
	} else {
	    double t1 = (m_node.m_min[i] - ray.m_origin[i]) / ray.m_dir[i];
	    double t2 = (m_node.m_max[i] - ray.m_origin[i]) / ray.m_dir[i];
	    if (t1 > t2) {
		double tmp = t1;    /* swap */
		t1 = t2;
		t2 = tmp;
	    }

	    V_MAX(tnear, t1);
	    V_MIN(tfar, t2);

	    if (tnear > tfar) { /* box is missed */
		untrimmedresult = false;
	    }
	    /* go ahead and solve hits behind us
	       if (tfar < 0) untrimmedresult = false;
	    */
	}
    }
    if (LIKELY(tnear_opt != NULL && tfar_opt != NULL)) {
	*tnear_opt = tnear;
	*tfar_opt = tfar;
    }
    if (isLeaf()) {
	return !m_trimmed && untrimmedresult;
    } else {
	return untrimmedresult;
    }
}


bool
BBNode::intersectsHierarchy(const ON_Ray &ray, std::list<const BBNode *> &results_opt) const
{
    double tnear, tfar;
    bool intersects = intersectedBy(ray, &tnear, &tfar);
    if (intersects && isLeaf()) {
	results_opt.push_back(this);
    } else if (intersects) {
	for (size_t i = 0; i < m_stl->m_children.size(); i++) {
	    m_stl->m_children[i]->intersectsHierarchy(ray, results_opt);
	}
    }
    return intersects;
}


bool
BBNode::containsUV(const ON_2dPoint &uv) const
{
    if ((uv[0] > m_u[0]) && (uv[0] < m_u[1]) && (uv[1] > m_v[0]) && (uv[1] < m_v[1])) {
	return true;
    } else {
	return false;
    }
}


int
BBNode::depth() const
{
    int d = 0;
    for (size_t i = 0; i < m_stl->m_children.size(); i++) {
	d = 1 + std::max(d, m_stl->m_children[i]->depth());
    }
    return d;
}


void
BBNode::getLeaves(std::list<const BBNode *> &out_leaves) const
{
    if (!m_stl->m_children.empty()) {
	for (size_t i = 0; i < m_stl->m_children.size(); i++) {
	    m_stl->m_children[i]->getLeaves(out_leaves);
	}
    } else {
	out_leaves.push_back(this);
    }
}


const BBNode *
BBNode::closer(const ON_3dPoint &pt, const BBNode *left, const BBNode *right) const
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
BBNode::getClosestPointEstimate(const ON_3dPoint &pt) const
{
    ON_Interval u, v;
    return getClosestPointEstimate(pt, u, v);
}


ON_2dPoint
BBNode::getClosestPointEstimate(const ON_3dPoint &pt, ON_Interval &u, ON_Interval &v) const
{
    if (isLeaf()) {
	double uvs[5][2] = {{m_u.Min(), m_v.Min()}, {m_u.Max(), m_v.Min()},
			    {m_u.Max(), m_v.Max()}, {m_u.Min(), m_v.Max()},
			    {m_u.Mid(), m_v.Mid()}
	}; /* include the estimate */
	ON_3dPoint corners[5];
	const ON_Surface *surf = get_face().SurfaceOf();

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
	    throw std::exception(); /* FIXME */
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
	if (!m_stl->m_children.empty()) {
	    const BBNode *closestNode = m_stl->m_children[0];
	    for (size_t i = 1; i < m_stl->m_children.size(); i++) {
		closestNode = closer(pt, closestNode, m_stl->m_children[i]);
		TRACE("\t" << PT(closestNode->m_estimate));
	    }
	    return closestNode->getClosestPointEstimate(pt, u, v);
	}
	throw std::exception();
    }
}


int
BBNode::getLeavesBoundingPoint(const ON_3dPoint &pt, std::list<const BBNode *> &out) const
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
	for (size_t i = 0; i < m_stl->m_children.size(); i++) {
	    sum += m_stl->m_children[i]->getLeavesBoundingPoint(pt, out);
	}
	return sum;
    }
}


bool
BBNode::isTrimmed(const ON_2dPoint &uv, const BRNode **closest, double &closesttrim, double within_distance_tol) const
{
    const BRNode *br;
    std::list<const BRNode *> trims;

    closesttrim = -1.0;
    if (m_checkTrim) {
	getTrimsAbove(uv, trims);

	if (trims.empty()) {
	    return true;
	} else { /* find closest BB */
	    std::list<const BRNode *>::const_iterator i;
	    const BRNode *vclosest = NULL;
	    const BRNode *uclosest = NULL;
	    fastf_t currHeight = (fastf_t)0.0;
	    bool currTrimStatus = false;
	    bool verticalTrim = false;
	    bool underTrim = false;
	    double vdist = 0.0;
	    double udist = 0.0;

	    for (i = trims.begin(); i != trims.end(); i++) {
		br = *i;

		/* skip if trim below */
		if (br->m_node.m_max[1] + within_distance_tol < uv[Y]) {
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
		bool trimstatus = br->isTrimmed(uv, v);
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
			V_MIN(udist, dist);
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
		return true;
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
	    return true;
	} else {
	    return false;
	}
    }
}


void
BBNode::getTrimsAbove(const ON_2dPoint &uv, std::list<const BRNode *> &out_leaves) const
{
    point_t bmin, bmax;
    double dist;
    for (std::list<const BRNode *>::const_iterator i = m_stl->m_trims_above.begin(); i != m_stl->m_trims_above.end(); i++) {
	const BRNode *br = *i;
	br->GetBBox(bmin, bmax);
	dist = BREP_UV_DIST_FUZZ; /* 0.03*DIST_PNT_PNT(bmin, bmax); */
	if ((uv[X] > bmin[X] - dist) && (uv[X] < bmax[X] + dist)) {
	    out_leaves.push_back(br);
	}
    }
}


void BBNode::BuildBBox()
{
    if (!m_stl->m_children.empty()) {
	for (std::vector<BBNode *>::const_iterator childnode = m_stl->m_children.begin(); childnode != m_stl->m_children.end(); childnode++) {
	    if (!(*childnode)->isLeaf()) {
		(*childnode)->BuildBBox();
	    }
	    if (childnode == m_stl->m_children.begin()) {
		m_node = ON_BoundingBox((*childnode)->m_node.m_min, (*childnode)->m_node.m_max);
	    } else {
		for (int j = 0; j < 3; j++) {
		    V_MIN(m_node.m_min[j], (*childnode)->m_node.m_min[j]);
		    V_MAX(m_node.m_max[j], (*childnode)->m_node.m_max[j]);
		}
	    }
	}
    }
}


bool
BBNode::prepTrims()
{
    const CurveTree *ct = m_ctree;
    std::list<const BRNode *>::iterator i;
    const BRNode *br;
    point_t curvemin, curvemax;
    double dist = BREP_UV_DIST_FUZZ;
    bool trim_already_assigned = false;

    m_stl->m_trims_above.clear();

    if (LIKELY(ct != NULL)) {
	ct->getLeavesAbove(m_stl->m_trims_above, m_u, m_v);
    }

    m_stl->m_trims_above.sort(sortY);

    if (!m_stl->m_trims_above.empty()) {
	i = m_stl->m_trims_above.begin();
	while (i != m_stl->m_trims_above.end()) {
	    br = *i;
	    if (br->m_Vertical) { /* check V to see if trim possibly overlaps */
		br->GetBBox(curvemin, curvemax);
		if (curvemin[Y] - dist <= m_v[1]) {
		    /* possibly contains trim can't rule out check
		     * closer */
		    m_checkTrim = true;
		    trim_already_assigned = true;
		    i++;
		} else {
		    i = m_stl->m_trims_above.erase(i);
		}
	    } else {
		i++;
	    }
	}
    }

    if (!trim_already_assigned) { /* already contains possible vertical trim */
	if (m_stl->m_trims_above.empty() /*|| m_trims_right.empty()*/) {
	    m_trimmed = true;
	    m_checkTrim = false;
	} else if (!m_stl->m_trims_above.empty()) { /*trimmed above check contains */
	    i = m_stl->m_trims_above.begin();
	    br = *i;
	    br->GetBBox(curvemin, curvemax);
	    dist = BREP_UV_DIST_FUZZ; /* 0.03*DIST_PNT_PNT(curvemin, curvemax); */
	    if (curvemin[Y] - dist > m_v[1]) {
		i++;

		if (i == m_stl->m_trims_above.end()) { /* easy only trim in above list */
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
		    const BRNode *bs;
		    bs = *i;
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
