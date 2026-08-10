/*                      B B N O D E . C P P
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
    m_children(NULL),
    m_child_count(0),
    m_child_capacity(0)
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
    m_children(NULL),
    m_child_count(0),
    m_child_capacity(0)
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
    for (std::size_t i = 0; i < m_child_count; ++i)
	delete m_children[i];
    if (m_children)
	bu_free(m_children, "BBNode child pointers");
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
    m_children(NULL),
    m_child_count(0),
    m_child_capacity(0)
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

    for (std::size_t i = 0; i < num_children; ++i)
	addChild(new BBNode(deserializer, ctree));

    /* Older cache records stored per-leaf trim indices here.  Consume them
     * for backward compatibility; current classification uses the owning
     * CurveTree's immutable per-loop index. */
    for (std::size_t i = 0; i < num_leaves_keys; ++i)
	(void)deserializer.read_uint32();
}


void
BBNode::serialize(Serializer &serializer) const
{
    const uint8_t bool_flags = (m_checkTrim << 0) | (m_trimmed << 1);

    serializer.write(m_node);
    serializer.write(m_u);
    serializer.write(m_v);
    serializer.write(m_estimate);
    serializer.write(m_normal);

    serializer.write_uint8(bool_flags);
    serializer.write_uint32(0);
    serializer.write_uint32(m_child_count);

    for (std::size_t i = 0; i < m_child_count; ++i)
	m_children[i]->serialize(serializer);
}


void
BBNode::addChild(BBNode *node)
{
    if (!node)
	return;
    if (m_child_count == UINT32_MAX)
	bu_bomb("BBNode child count exceeds compact storage");
    if (m_child_count == m_child_capacity) {
	uint32_t new_capacity = m_child_capacity ? m_child_capacity * 2 : 1;
	if (new_capacity < m_child_capacity || new_capacity < m_child_count + 1)
	    new_capacity = m_child_count + 1;
	m_children = static_cast<BBNode **>(bu_realloc(m_children,
	    (std::size_t)new_capacity * sizeof(BBNode *),
	    "BBNode child pointers"));
	m_child_capacity = new_capacity;
    }
    m_children[m_child_count++] = node;
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
	for (std::size_t i = 0; i < m_child_count; ++i)
	    m_children[i]->intersectsHierarchy(ray, results_opt);
    }
    return intersects;
}


bool
BBNode::intersectsHierarchy(const ON_Ray &ray, const BBNode **results,
	std::size_t capacity, std::size_t &count, bool &overflow) const
{
    double tnear, tfar;
    bool intersects = intersectedBy(ray, &tnear, &tfar);
    if (intersects && isLeaf()) {
	if (count < capacity && results) {
	    results[count] = this;
	} else {
	    overflow = true;
	}
	count++;
    } else if (intersects) {
	for (std::size_t i = 0; i < m_child_count; ++i)
	    m_children[i]->intersectsHierarchy(ray, results, capacity,
		count, overflow);
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
    for (std::size_t i = 0; i < m_child_count; ++i)
	d = 1 + std::max(d, m_children[i]->depth());
    return d;
}


void
BBNode::getLeaves(std::list<const BBNode *> &out_leaves) const
{
    if (m_child_count) {
	for (std::size_t i = 0; i < m_child_count; ++i)
	    m_children[i]->getLeaves(out_leaves);
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
	if (m_child_count) {
	    const BBNode *closestNode = m_children[0];
	    for (std::size_t i = 1; i < m_child_count; ++i) {
		closestNode = closer(pt, closestNode, m_children[i]);
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
	for (std::size_t i = 0; i < m_child_count; ++i)
	    sum += m_children[i]->getLeavesBoundingPoint(pt, out);
	return sum;
    }
}


namespace {

struct TrimClassificationState {
    explicit TrimClassificationState(const BRNode **closest_node) :
	closest(closest_node),
	crossing_closest(NULL),
	proximity_closest(NULL),
	crossings(0),
	crossing_distance(INFINITY),
	proximity_distance(INFINITY)
    {
    }

    const BRNode **closest;
    const BRNode *crossing_closest;
    const BRNode *proximity_closest;
    size_t crossings;
    double crossing_distance;
    double proximity_distance;
};


static bool
trim_candidate_on_coordinate(const BRNode *br, const ON_2dPoint &uv,
	int coordinate, double within_distance_tol)
{
    point_t bmin, bmax;
    br->GetBBox(bmin, bmax);
    const double envelope = std::max(BREP_UV_DIST_FUZZ,
	std::max(0.0, within_distance_tol));
    return uv[coordinate] >= bmin[coordinate] - envelope &&
	uv[coordinate] <= bmax[coordinate] + envelope;
}


static void
classify_trim_candidate(TrimClassificationState &state, const BRNode *br,
	const ON_2dPoint &uv, double within_distance_tol, bool horizontal_ray,
	bool extend_minimum)
{
    double crossing_distance = -1.0;
    const double endpoint_tolerance = extend_minimum ?
	within_distance_tol : 0.0;
    if ((horizontal_ray ? br->crossesRight(uv, crossing_distance,
		endpoint_tolerance) :
	    br->crossesAbove(uv, crossing_distance, endpoint_tolerance))) {
	state.crossings++;
	if (crossing_distance < state.crossing_distance) {
	    state.crossing_distance = crossing_distance;
	    state.crossing_closest = br;
	}
    }

    const double proximity_limit = std::max(0.0, within_distance_tol);
    if (br->curveBBoxDistance(uv) <= proximity_limit +
	    BREP_UV_DIST_FUZZ) {
	const double proximity_distance = br->curveDistance(uv);
	if (proximity_distance < state.proximity_distance) {
	    state.proximity_distance = proximity_distance;
	    state.proximity_closest = br;
	}
    }
}


static void
merge_trim_classification(TrimClassificationState &target,
	const TrimClassificationState &source)
{
    target.crossings += source.crossings;
    if (source.crossing_distance < target.crossing_distance) {
	target.crossing_distance = source.crossing_distance;
	target.crossing_closest = source.crossing_closest;
    }
    if (source.proximity_distance < target.proximity_distance) {
	target.proximity_distance = source.proximity_distance;
	target.proximity_closest = source.proximity_closest;
    }
}


static void
finish_trim_distance(TrimClassificationState &state,
	double &closesttrim)
{
    if (state.proximity_closest) {
	closesttrim = state.proximity_distance;
	if (state.closest)
	    *state.closest = state.proximity_closest;
    } else if (state.crossing_closest) {
	closesttrim = state.crossing_distance;
	if (state.closest)
	    *state.closest = state.crossing_closest;
    }
}

} /* anonymous namespace */


bool
CurveTree::isTrimmed(const ON_2dPoint &uv, const BRNode **closest,
	double &closesttrim, double within_distance_tol,
	std::size_t *candidate_count, bool heal_join_gaps) const
{
    closesttrim = -1.0;
    if (closest)
	*closest = NULL;
    if (candidate_count)
	*candidate_count = 0;

    const ON_Surface *surface = m_face ? m_face->SurfaceOf() : NULL;
    const double period[2] = {
	surface ? surface->Domain(0).Length() : 0.0,
	surface ? surface->Domain(1).Length() : 0.0
    };
    ON_2dPoint query(uv);
    if (surface) {
	for (int direction = 0; direction < 2; ++direction) {
	    if (!surface->IsClosed(direction))
		continue;
	    const ON_Interval domain = surface->Domain(direction);
	    const double direction_period = domain.Length();
	    if (!(direction_period > 0.0))
		continue;
	    query[direction] -= floor(
		(query[direction] - domain.Min()) / direction_period) *
		direction_period;
	    if (query[direction] >= domain.Max())
		query[direction] = domain.Min();
	}
    }
    TrimClassificationState state(closest);
    const bool horizontal_ray = surface && surface->IsClosed(1);
    const int ray_axis = horizontal_ray ? 0 : 1;
    const int fixed_coordinate = horizontal_ray ? 1 : 0;
    std::size_t candidates = 0;
    bool any_outer = false;
    bool inside_outer = false;
    bool inside_inner = false;
    bool parity = false;
    bool have_parity_loop = false;

    for (std::vector<Stl::Loop>::const_iterator loop =
	    m_stl->m_loops.begin(); loop != m_stl->m_loops.end(); ++loop) {
	if (!loop->have_bbox || loop->sorted.empty())
	    continue;
	ON_2dPoint loop_query(query);
	if (surface && surface->IsClosed(ray_axis)) {
	    const double ray_period = surface->Domain(ray_axis).Length();
	    if (ray_period > 0.0) {
		const double center = 0.5 *
		    (loop->minimum[ray_axis] + loop->maximum[ray_axis]);
		loop_query[ray_axis] += nearbyint(
		    (center - loop_query[ray_axis]) / ray_period) * ray_period;
	    }
	}
	TrimClassificationState loop_state(NULL);
	for (std::vector<Stl::Loop::Leaf>::const_iterator leaf =
		loop->sorted.begin(); leaf != loop->sorted.end(); ++leaf) {
	    const BRNode *br = &m_stl->m_leaves[leaf->node_index];
	    const ON_2dVector offset(
		(double)leaf->image[0] * period[0],
		(double)leaf->image[1] * period[1]);
	    const ON_2dPoint local_query(loop_query.x - offset.x,
		loop_query.y - offset.y);
	    point_t bmin, bmax;
	    br->GetBBox(bmin, bmax);
	    const double envelope = std::max(BREP_UV_DIST_FUZZ,
		std::max(0.0, within_distance_tol));
	    if (bmin[fixed_coordinate] + offset[fixed_coordinate] >
		    loop_query[fixed_coordinate] + envelope)
		break;
	    if (!trim_candidate_on_coordinate(br, local_query,
		    fixed_coordinate, within_distance_tol))
		continue;
	    ++candidates;
	    classify_trim_candidate(loop_state, br, local_query,
		within_distance_tol, horizontal_ray,
		heal_join_gaps && leaf->extend_minimum[fixed_coordinate]);
	}
	merge_trim_classification(state, loop_state);
	const bool loop_inside = (loop_state.crossings & 1U) != 0;
	switch (loop->type) {
	    case ON_BrepLoop::outer:
		any_outer = true;
		inside_outer = inside_outer || loop_inside;
		have_parity_loop = true;
		parity = parity != loop_inside;
		break;
	    case ON_BrepLoop::inner:
		inside_inner = inside_inner || loop_inside;
		have_parity_loop = true;
		parity = parity != loop_inside;
		break;
	    case ON_BrepLoop::unknown:
		have_parity_loop = true;
		parity = parity != loop_inside;
		break;
	    default:
		break;
	}
    }
    if (candidate_count)
	*candidate_count = candidates;
    finish_trim_distance(state, closesttrim);
    const bool inside = any_outer ? inside_outer && !inside_inner :
	have_parity_loop && parity;
    return !inside;
}


bool
CurveTree::classifyCell(const ON_Interval &u, const ON_Interval &v,
	double within_distance_tol, bool &trimmed) const
{
    const double envelope = std::max(BREP_UV_DIST_FUZZ,
	std::max(0.0, within_distance_tol));
    const ON_Surface *surface = m_face ? m_face->SurfaceOf() : NULL;
    const double period[2] = {
	surface ? surface->Domain(0).Length() : 0.0,
	surface ? surface->Domain(1).Length() : 0.0
    };
    const bool horizontal_ray = surface && surface->IsClosed(1);
    const int ray_axis = horizontal_ray ? 0 : 1;
    const int fixed_coordinate = horizontal_ray ? 1 : 0;
    const ON_Interval cell[2] = {u, v};
    for (std::vector<Stl::Loop>::const_iterator loop =
	    m_stl->m_loops.begin(); loop != m_stl->m_loops.end(); ++loop) {
	if (!loop->have_bbox)
	    continue;
	for (std::vector<Stl::Loop::Leaf>::const_iterator leaf =
		loop->sorted.begin(); leaf != loop->sorted.end(); ++leaf) {
	    const BRNode *br = &m_stl->m_leaves[leaf->node_index];
	    const ON_2dVector offset(
		(double)leaf->image[0] * period[0],
		(double)leaf->image[1] * period[1]);
	    point_t bmin, bmax;
	    br->GetBBox(bmin, bmax);
	    bmin[X] += offset.x;
	    bmax[X] += offset.x;
	    bmin[Y] += offset.y;
	    bmax[Y] += offset.y;
	    if (bmax[fixed_coordinate] + envelope <
		    cell[fixed_coordinate].Min())
		continue;
	    if (bmin[fixed_coordinate] - envelope >
		    cell[fixed_coordinate].Max())
		break;
	    if (bmax[ray_axis] + envelope < cell[ray_axis].Min())
		continue;

	    /* A displaced loop join can change the tolerance-extended parity
	     * even when its curve is far along the ray axis.  Exact joins do not:
	     * their two incident crossings retain parity. */
	    if (leaf->extend_minimum[fixed_coordinate]) {
		const ON_3dPoint start = br->startPoint();
		const ON_3dPoint end = br->endPoint();
		const double endpoint = std::min(start[fixed_coordinate],
		    end[fixed_coordinate]) + offset[fixed_coordinate];
		if (endpoint >= cell[fixed_coordinate].Min() - envelope &&
			endpoint <= cell[fixed_coordinate].Max() + envelope)
		    return false;
	    }

	    const ON_2dPoint bbox_min = br->bboxMinimum();
	    const ON_2dPoint bbox_max = br->bboxMaximum();
	    const double bbox_min_u = bbox_min.x + offset.x;
	    const double bbox_max_u = bbox_max.x + offset.x;
	    const double bbox_min_v = bbox_min.y + offset.y;
	    const double bbox_max_v = bbox_max.y + offset.y;
	    double du = 0.0;
	    double dv = 0.0;
	    if (bbox_max_u < u.Min())
		du = u.Min() - bbox_max_u;
	    else if (bbox_min_u > u.Max())
		du = bbox_min_u - u.Max();
	    if (bbox_max_v < v.Min())
		dv = v.Min() - bbox_max_v;
	    else if (bbox_min_v > v.Max())
		dv = bbox_min_v - v.Max();
	    if (hypot(du, dv) <= envelope)
		return false;
	}
    }

    const BRNode *closest = NULL;
    double distance = -1.0;
    trimmed = isTrimmed(ON_2dPoint(u.Mid(), v.Mid()), &closest,
	distance, envelope, NULL);
    return true;
}


bool
BBNode::isTrimmed(const ON_2dPoint &uv, const BRNode **closest,
	double &closesttrim, double within_distance_tol,
	std::size_t *candidate_count) const
{
    closesttrim = -1.0;
    if (candidate_count)
	*candidate_count = 0;

    if (!m_checkTrim)
	return m_trimmed;
    return m_ctree ? m_ctree->isTrimmed(uv, closest, closesttrim,
	within_distance_tol, candidate_count) : true;
}


bool
BBNode::isTrimmedAllocating(const ON_2dPoint &uv, const BRNode **closest,
	double &closesttrim, double within_distance_tol,
	std::size_t *candidate_count) const
{
    closesttrim = -1.0;
    if (candidate_count)
	*candidate_count = 0;

    if (!m_checkTrim)
	return m_trimmed;
    return m_ctree ? m_ctree->isTrimmed(uv, closest, closesttrim,
	within_distance_tol, candidate_count) : true;
}


void BBNode::BuildBBox()
{
    if (m_child_count) {
	for (std::size_t child_index = 0; child_index < m_child_count;
		++child_index) {
	    BBNode *child_node = m_children[child_index];
	    if (!child_node->isLeaf()) {
		child_node->BuildBBox();
	    }
	    if (child_index == 0) {
		m_node = ON_BoundingBox(child_node->m_node.m_min,
		    child_node->m_node.m_max);
	    } else {
		for (int j = 0; j < 3; j++) {
		    V_MIN(m_node.m_min[j], child_node->m_node.m_min[j]);
		    V_MAX(m_node.m_max[j], child_node->m_node.m_max[j]);
		}
	    }
	}
    }
}


bool
BBNode::prepTrims(double within_distance_tol)
{
    const CurveTree *ct = m_ctree;
    if (!ct) {
	m_checkTrim = false;
	m_trimmed = false;
	return true;
    }
    const double envelope = std::max(BREP_UV_DIST_FUZZ,
	std::max(0.0, within_distance_tol));
    bool classification = false;
    if (!ct->classifyCell(m_u, m_v, envelope, classification)) {
	m_checkTrim = true;
	m_trimmed = false;
	return true;
    }
    m_checkTrim = false;
    m_trimmed = classification;
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
