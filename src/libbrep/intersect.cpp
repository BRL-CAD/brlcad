/*                  I N T E R S E C T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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
/** @file intersect.cpp
 *
 * Implementation of intersection routines openNURBS left out.
 *
 */

#include "common.h"

#include "bio.h"
#include <assert.h>
#include <vector>
#include <algorithm>

#include "vmath.h"
#include "bu.h"

#include "brep.h"

#if !defined(OPENNURBS_PLUS_INC_)

/**
 * Surface-surface intersections (SSI)
 *
 * approach:
 *
 * - Generate the bounding box of the two surfaces.
 *
 * - If their bounding boxes intersect:
 *
 * -- Split the two surfaces, both into four parts, and calculate the
 *    sub-surfaces' bounding boxes
 *
 * -- Calculate the intersection of sub-surfaces' bboxes, if they do
 *    intersect, go deeper with splitting surfaces and smaller bboxes,
 *    otherwise trace back.
 *
 * - After getting the intersecting bboxes, approximate the surface
 *   inside the bbox as two triangles, and calculate the intersection
 *   points of the triangles (both in 3d space and two surfaces' UV
 *   space)
 *
 * - Fit the intersection points into polyline curves, and then to NURBS
 *   curves. Points with distance less than max_dis are considered in
 *   one curve.
 *
 * See: Adarsh Krishnamurthy, Rahul Khardekar, Sara McMains, Kirk Haller,
 * and Gershon Elber. 2008.
 * Performing efficient NURBS modeling operations on the GPU.
 * In Proceedings of the 2008 ACM symposium on Solid and physical modeling
 * (SPM '08). ACM, New York, NY, USA, 257-268. DOI=10.1145/1364901.1364937
 * http://doi.acm.org/10.1145/1364901.1364937
 */

struct Triangle {
    ON_3dPoint A, B, C;
    inline void CreateFromPoints(ON_3dPoint &p_A, ON_3dPoint &p_B, ON_3dPoint &p_C) {
	A = p_A;
	B = p_B;
	C = p_C;
    }
    ON_3dPoint BarycentricCoordinate(ON_3dPoint &pt)
    {
	ON_Matrix M(3, 3);
	M[0][0] = A[0], M[0][1] = B[0], M[0][2] = C[0];
	M[1][0] = A[1], M[1][1] = B[1], M[1][2] = C[1];
	M[2][0] = A[2], M[2][1] = B[2], M[2][2] = C[2];
	M.Invert(0.0);
	ON_Matrix MX(3, 1);
	MX[0][0] = pt[0], MX[1][0] = pt[1], MX[2][0] = pt[2];
	M.Multiply(M, MX);
	return ON_3dPoint(M[0][0], M[1][0], M[2][0]);
    }
};


bool
triangle_intersection(const struct Triangle &TriA, const struct Triangle &TriB, ON_3dPoint &center)
{
    ON_Plane plane_a(TriA.A, TriA.B, TriA.C);
    ON_Plane plane_b(TriB.A, TriB.B, TriB.C);
    ON_Line intersect;
    if (!ON_Intersect(plane_a, plane_b, intersect))
	return false;
    ON_3dVector line_normal = ON_CrossProduct(plane_a.Normal(), intersect.Direction());

    // dpi - >0: one side of the line, <0: another side, ==0: on the line
    double dp1 = ON_DotProduct(TriA.A - intersect.from, line_normal);
    double dp2 = ON_DotProduct(TriA.B - intersect.from, line_normal);
    double dp3 = ON_DotProduct(TriA.C - intersect.from, line_normal);

    int points_on_one_side = (dp1 >= 0) + (dp2 >= 0) + (dp3 >= 0);
    if (points_on_one_side == 0 || points_on_one_side == 3)
	return false;

    double dp4 = ON_DotProduct(TriB.A - intersect.from, line_normal);
    double dp5 = ON_DotProduct(TriB.B - intersect.from, line_normal);
    double dp6 = ON_DotProduct(TriB.C - intersect.from, line_normal);
    points_on_one_side = (dp4 >= 0) + (dp5 >= 0) + (dp6 >= 0);
    if (points_on_one_side == 0 || points_on_one_side == 3)
	return false;

    double t[4];
    int count = 0;
    double t1, t2;

    // dpi*dpj < 0 - the corresponding points are on different sides
    // - the line segment between them are intersected by the plane-plane
    // intersection line
    if (dp1*dp2 < 0) {
	intersect.ClosestPointTo(TriA.A, &t1);
	intersect.ClosestPointTo(TriA.B, &t2);
	double d1 = TriA.A.DistanceTo(intersect.PointAt(t1));
	double d2 = TriA.B.DistanceTo(intersect.PointAt(t2));
	if (!ZERO(d1 + d2))
	    t[count++] = t1 + d1/(d1+d2)*(t2-t1);
    }
    if (dp1*dp3 < 0) {
	intersect.ClosestPointTo(TriA.A, &t1);
	intersect.ClosestPointTo(TriA.C, &t2);
	double d1 = TriA.A.DistanceTo(intersect.PointAt(t1));
	double d2 = TriA.C.DistanceTo(intersect.PointAt(t2));
	if (!ZERO(d1 + d2))
	    t[count++] = t1 + d1/(d1+d2)*(t2-t1);
    }
    if (dp2*dp3 < 0 && count != 2) {
	intersect.ClosestPointTo(TriA.B, &t1);
	intersect.ClosestPointTo(TriA.C, &t2);
	double d1 = TriA.B.DistanceTo(intersect.PointAt(t1));
	double d2 = TriA.C.DistanceTo(intersect.PointAt(t2));
	if (!ZERO(d1 + d2))
	    t[count++] = t1 + d1/(d1+d2)*(t2-t1);
    }
    if (count != 2)
	return false;
    if (dp4*dp5 < 0) {
	intersect.ClosestPointTo(TriB.A, &t1);
	intersect.ClosestPointTo(TriB.B, &t2);
	double d1 = TriB.A.DistanceTo(intersect.PointAt(t1));
	double d2 = TriB.B.DistanceTo(intersect.PointAt(t2));
	if (!ZERO(d1 + d2))
	    t[count++] = t1 + d1/(d1+d2)*(t2-t1);
    }
    if (dp4*dp6 < 0) {
	intersect.ClosestPointTo(TriB.A, &t1);
	intersect.ClosestPointTo(TriB.C, &t2);
	double d1 = TriB.A.DistanceTo(intersect.PointAt(t1));
	double d2 = TriB.C.DistanceTo(intersect.PointAt(t2));
	if (!ZERO(d1 + d2))
	    t[count++] = t1 + d1/(d1+d2)*(t2-t1);
    }
    if (dp5*dp6 < 0 && count != 4) {
	intersect.ClosestPointTo(TriB.B, &t1);
	intersect.ClosestPointTo(TriB.C, &t2);
	double d1 = TriB.B.DistanceTo(intersect.PointAt(t1));
	double d2 = TriB.C.DistanceTo(intersect.PointAt(t2));
	if (!ZERO(d1 + d2))
	    t[count++] = t1 + d1/(d1+d2)*(t2-t1);
    }
    if (count != 4)
	return false;
    if (t[0] > t[1])
	std::swap(t[1], t[0]);
    if (t[2] > t[3])
	std::swap(t[3], t[2]);
    double left = std::max(t[0], t[2]);
    double right = std::min(t[1], t[3]);
    if (left > right)
	return false;
    center = intersect.PointAt((left+right)/2);
    return true;
}


struct PointPair {
    int indexA, indexB;
    double distance;
    bool operator < (const PointPair &_pp) const {
	return distance < _pp.distance;
    }
};


struct Subsurface {
private:
    ON_BoundingBox m_node;
public:
    ON_Surface *m_surf;
    ON_Interval m_u, m_v;
    Subsurface *m_children[4];

    Subsurface() : m_surf(NULL)
    {
	m_children[0] = m_children[1] = m_children[2] = m_children[3] = NULL;
    }
    Subsurface(const Subsurface &_ssurf)
    {
	m_surf = _ssurf.m_surf->Duplicate();
	m_u = _ssurf.m_u;
	m_v = _ssurf.m_v;
	m_children[0] = m_children[1] = m_children[2] = m_children[3] = NULL;
	SetBBox(_ssurf.m_node);
    }
    ~Subsurface()
    {
	for (int i = 0; i < 4; i++) {
	    if (m_children[i])
		delete m_children[i];
	}
	delete m_surf;
    }
    int Split()
    {
	for (int i = 0; i < 4; i++)
	    m_children[i] = new Subsurface();
	ON_Surface *temp_surf1 = NULL, *temp_surf2 = NULL;
	ON_BOOL32 ret = true;
	ret = m_surf->Split(0, m_u.Mid(), temp_surf1, temp_surf2);
	if (!ret) {
	    delete temp_surf1;
	    delete temp_surf2;
	    return -1;
	}

	ret = temp_surf1->Split(1, m_v.Mid(), m_children[0]->m_surf, m_children[1]->m_surf);
	delete temp_surf1;
	if (!ret) {
	    delete temp_surf2;
	    return -1;
	}
	m_children[0]->m_u = ON_Interval(m_u.Min(), m_u.Mid());
	m_children[0]->m_v = ON_Interval(m_v.Min(), m_v.Mid());
	m_children[0]->SetBBox(m_children[0]->m_surf->BoundingBox());
	m_children[1]->m_u = ON_Interval(m_u.Min(), m_u.Mid());
	m_children[1]->m_v = ON_Interval(m_v.Mid(), m_v.Max());
	m_children[1]->SetBBox(m_children[1]->m_surf->BoundingBox());

	ret = temp_surf2->Split(1, m_v.Mid(), m_children[2]->m_surf, m_children[3]->m_surf);
	delete temp_surf2;
	if (!ret)
	    return -1;
	m_children[2]->m_u = ON_Interval(m_u.Mid(), m_u.Max());
	m_children[2]->m_v = ON_Interval(m_v.Min(), m_v.Mid());
	m_children[2]->SetBBox(m_children[2]->m_surf->BoundingBox());
	m_children[3]->m_u = ON_Interval(m_u.Mid(), m_u.Max());
	m_children[3]->m_v = ON_Interval(m_v.Mid(), m_v.Max());
	m_children[3]->SetBBox(m_children[3]->m_surf->BoundingBox());

	return 0;
    }
    void GetBBox(ON_3dPoint &min, ON_3dPoint &max)
    {
	min = m_node.m_min;
	max = m_node.m_max;
    }
    void SetBBox(const ON_BoundingBox &bbox)
    {
	m_node = bbox;
	/* Make sure that each dimension of the bounding box is greater than
	 * ON_ZERO_TOLERANCE.
	 * It does the same work as building the surface tree when ray tracing
	 */
	for (int i = 0; i < 3; i++) {
	    double d = m_node.m_max[i] - m_node.m_min[i];
	    if (ON_NearZero(d, ON_ZERO_TOLERANCE)) {
		m_node.m_min[i] -= 0.001;
		m_node.m_max[i] += 0.001;
	    }
	}
    }
};


#define INTERSECT_MAX_DEPTH 8
int
ON_Surface::IntersectSurface(
        const ON_Surface* surfB,
        ON_ClassArray<ON_SSX_EVENT>& x,
        double,
        double,
        double,
        const ON_Interval*,
        const ON_Interval*,
        const ON_Interval*,
        const ON_Interval*
        ) const
{
    bu_log("ON_Surface::IntersectSurface() in libbrep is called.\n");
    const ON_Surface *surfA = this;
    if (surfA == NULL || surfB == NULL) {
	return -1;
    }

    /* First step: Initialize the first two Subsurface.
     * It's just like getting the root of the surface tree.
     */
    typedef std::vector<std::pair<Subsurface*, Subsurface*> > NodePairs;
    NodePairs nodepairs;
    Subsurface *rootA = new Subsurface(), *rootB = new Subsurface();
    rootA->SetBBox(surfA->BoundingBox());
    rootA->m_u = surfA->Domain(0);
    rootA->m_v = surfA->Domain(1);
    rootA->m_surf = surfA->Duplicate();
    rootB->SetBBox(surfB->BoundingBox());
    rootB->m_u = surfB->Domain(0);
    rootB->m_v = surfB->Domain(1);
    rootB->m_surf = surfB->Duplicate();
    nodepairs.push_back(std::make_pair(rootA, rootB));
    ON_3dPointArray curvept;
    ON_2dPointArray curveuv, curvest;
    int bbox_count = 0;

    /* Second step: calculate the intersection of the bounding boxes, using
     * trigonal approximation.
     * Only the children of intersecting b-box pairs need to be considered.
     * The children will be generated only when they are needed, using the
     * method of splitting a NURBS surface.
     * So finally only a small subset of the surface tree is created.
     */
    for (int h = 0; h <= INTERSECT_MAX_DEPTH; h++) {
	if (nodepairs.empty())
	    break;
	NodePairs tmp_pairs;
	if (h) {
	    for (NodePairs::iterator i = nodepairs.begin(); i != nodepairs.end(); i++) {
		int ret1 = 0;
		if ((*i).first->m_children[0] == NULL)
		    ret1 = (*i).first->Split();
		int ret2 = 0;
		if ((*i).second->m_children[0] == NULL)
		    ret2 = (*i).second->Split();
		if (ret1) {
		    if (ret2) { /* both splits failed */
			tmp_pairs.push_back(*i);
			h = INTERSECT_MAX_DEPTH;
		    } else { /* the first failed */
			for (int j = 0; j < 4; j++)
			    tmp_pairs.push_back(std::make_pair((*i).first, (*i).second->m_children[j]));
		    }
		} else {
		    if (ret2) { /* the second failed */
			for (int j = 0; j < 4; j++)
			    tmp_pairs.push_back(std::make_pair((*i).first->m_children[j], (*i).second));
		    } else { /* both success */
			for (int j = 0; j < 4; j++)
			    for (int k = 0; k < 4; k++)
				tmp_pairs.push_back(std::make_pair((*i).first->m_children[j], (*i).second->m_children[k]));
		    }
		}
	    }
	} else {
	    tmp_pairs = nodepairs;
	}
	nodepairs.clear();
	for (NodePairs::iterator i = tmp_pairs.begin(); i != tmp_pairs.end(); i++) {
	    ON_BoundingBox box_a, box_b, box_intersect;
	    (*i).first->GetBBox(box_a.m_min, box_a.m_max);
	    (*i).second->GetBBox(box_b.m_min, box_b.m_max);
	    if (box_intersect.Intersection(box_a, box_b)) {
		if (h == INTERSECT_MAX_DEPTH) {
		    // We have arrived at the bottom of the trees.
		    // Get an estimate of the intersection point lying on the intersection curve

		    // Get the corners of each surface sub-patch inside the bounding-box.
		    ON_3dPoint cornerA[4], cornerB[4];
		    double u_min = (*i).first->m_u.Min(), u_max = (*i).first->m_u.Max();
		    double v_min = (*i).first->m_v.Min(), v_max = (*i).first->m_v.Max();
		    double s_min = (*i).second->m_u.Min(), s_max = (*i).second->m_u.Max();
		    double t_min = (*i).second->m_v.Min(), t_max = (*i).second->m_v.Max();
		    cornerA[0] = surfA->PointAt(u_min, v_min);
		    cornerA[1] = surfA->PointAt(u_min, v_max);
		    cornerA[2] = surfA->PointAt(u_max, v_min);
		    cornerA[3] = surfA->PointAt(u_max, v_max);
		    cornerB[0] = surfB->PointAt(s_min, t_min);
		    cornerB[1] = surfB->PointAt(s_min, t_max);
		    cornerB[2] = surfB->PointAt(s_max, t_min);
		    cornerB[3] = surfB->PointAt(s_max, t_max);

		    /* We approximate each surface sub-patch inside the bounding-box with two
		     * triangles that share an edge.
		     * The intersection of the surface sub-patches is approximated as the
		     * intersection of triangles.
		     */
		    struct Triangle triangle[4];
		    triangle[0].CreateFromPoints(cornerA[0], cornerA[1], cornerA[2]);
		    triangle[1].CreateFromPoints(cornerA[1], cornerA[2], cornerA[3]);
		    triangle[2].CreateFromPoints(cornerB[0], cornerB[1], cornerB[2]);
		    triangle[3].CreateFromPoints(cornerB[1], cornerB[2], cornerB[3]);
		    bool is_intersect[4];
		    ON_3dPoint intersect_center[4];
		    is_intersect[0] = triangle_intersection(triangle[0], triangle[2], intersect_center[0]);
		    is_intersect[1] = triangle_intersection(triangle[0], triangle[3], intersect_center[1]);
		    is_intersect[2] = triangle_intersection(triangle[1], triangle[2], intersect_center[2]);
		    is_intersect[3] = triangle_intersection(triangle[1], triangle[3], intersect_center[3]);

		    // Calculate the intersection centers' uv (st) coordinates.
		    ON_3dPoint bcoor[8];
		    ON_2dPoint uv[4]/*surfA*/, st[4]/*surfB*/;
		    if (is_intersect[0]) {
			bcoor[0] = triangle[0].BarycentricCoordinate(intersect_center[0]);
			bcoor[1] = triangle[2].BarycentricCoordinate(intersect_center[0]);
			uv[0].x = bcoor[0].x*u_min + bcoor[0].y*u_min + bcoor[0].z*u_max;
			uv[0].y = bcoor[0].x*v_min + bcoor[0].y*v_max + bcoor[0].z*v_min;
			st[0].x = bcoor[1].x*s_min + bcoor[1].y*s_min + bcoor[1].z*s_max;
			st[0].y = bcoor[1].x*t_min + bcoor[1].y*t_max + bcoor[1].z*t_min;
		    }
		    if (is_intersect[1]) {
			bcoor[2] = triangle[0].BarycentricCoordinate(intersect_center[1]);
			bcoor[3] = triangle[3].BarycentricCoordinate(intersect_center[1]);
			uv[1].x = bcoor[2].x*u_min + bcoor[2].y*u_min + bcoor[2].z*u_max;
			uv[1].y = bcoor[2].x*v_min + bcoor[2].y*v_max + bcoor[2].z*v_min;
			st[1].x = bcoor[3].x*s_min + bcoor[3].y*s_max + bcoor[3].z*s_max;
			st[1].y = bcoor[3].x*t_max + bcoor[3].y*t_min + bcoor[3].z*t_max;
		    }
		    if (is_intersect[2]) {
			bcoor[4] = triangle[1].BarycentricCoordinate(intersect_center[2]);
			bcoor[5] = triangle[2].BarycentricCoordinate(intersect_center[2]);
			uv[2].x = bcoor[4].x*u_min + bcoor[4].y*u_max + bcoor[4].z*u_max;
			uv[2].y = bcoor[4].x*v_max + bcoor[4].y*v_min + bcoor[4].z*v_max;
			st[2].x = bcoor[5].x*s_min + bcoor[5].y*s_min + bcoor[5].z*s_max;
			st[2].y = bcoor[5].x*t_min + bcoor[5].y*t_max + bcoor[5].z*t_min;
		    }
		    if (is_intersect[3]) {
			bcoor[6] = triangle[1].BarycentricCoordinate(intersect_center[3]);
			bcoor[7] = triangle[3].BarycentricCoordinate(intersect_center[3]);
			uv[3].x = bcoor[6].x*u_min + bcoor[6].y*u_max + bcoor[6].z*u_max;
			uv[3].y = bcoor[6].x*v_max + bcoor[6].y*v_min + bcoor[6].z*v_max;
			st[3].x = bcoor[7].x*s_min + bcoor[7].y*s_max + bcoor[7].z*s_max;
			st[3].y = bcoor[7].x*t_max + bcoor[7].y*t_min + bcoor[7].z*t_max;
		    }

		    // The centroid of these intersection centers is the
		    // intersection point we want.
		    int num_intersects = 0;
		    ON_3dPoint average(0.0, 0.0, 0.0);
		    ON_2dPoint avguv(0.0, 0.0), avgst(0.0, 0.0);
		    for (int j = 0; j < 4; j++) {
			if (is_intersect[j]) {
			    average += intersect_center[j];
			    avguv += uv[j];
			    avgst += st[j];
			    num_intersects++;
			}
		    }
		    if (num_intersects) {
			average /= num_intersects;
			avguv /= num_intersects;
			avgst /= num_intersects;
			if (box_intersect.IsPointIn(average)) {
			    curvept.Append(average);
			    curveuv.Append(avguv);
			    curvest.Append(avgst);
			}
		    }
		    bbox_count++;
		} else {
		    nodepairs.push_back(*i);
		}
	    }
	}
	/* for (int i = 0; i < curvept.Count(); i++) {
	    bu_log("(%lf %lf %lf)\n", curvept[i].x, curvept[i].y, curvept[i].z);
	}
	bu_log("%d %d\n", h, tmp_pairs.size());*/
    }
    bu_log("We get %d intersection bounding boxes.\n", bbox_count);
    bu_log("%d points on the intersection curves.\n", curvept.Count());

    if (!curvept.Count()) {
	delete rootA;
	delete rootB;
	return 0;
    }

    // Third step: Fit the points in curvept into NURBS curves.
    // Here we use polyline approximation.
    // TODO: Find a better fitting algorithm unless this is good enough.

    // We need to automatically generate a threshold.
    double max_dis;
    if (ZERO(surfA->BoundingBox().Volume())) {
	max_dis = pow(surfB->BoundingBox().Volume(), 1.0/3.0) * 0.2;
    } else if (ZERO(surfB->BoundingBox().Volume())) {
	max_dis = pow(surfA->BoundingBox().Volume(), 1.0/3.0) * 0.2;
    } else {
	max_dis = pow(surfA->BoundingBox().Volume()*surfB->BoundingBox().Volume(), 1.0/6.0) * 0.2;
    }

    double max_dis_2dA = ON_2dVector(surfA->Domain(0).Length(), surfA->Domain(1).Length()).Length() * 0.01;
    double max_dis_2dB = ON_2dVector(surfB->Domain(0).Length(), surfB->Domain(1).Length()).Length() * 0.01;
    bu_log("max_dis: %lf\n", max_dis);
    bu_log("max_dis_2dA: %lf\n", max_dis_2dA);
    bu_log("max_dis_2dB: %lf\n", max_dis_2dB);
    // NOTE: More tests are needed to find a better threshold.

    std::vector<PointPair> ptpairs;
    for (int i = 0; i < curvept.Count(); i++) {
	for (int j = i + 1; j < curvept.Count(); j++) {
	    PointPair ppair;
	    ppair.distance = curvept[i].DistanceTo(curvept[j]);
	    if (ppair.distance < max_dis) {
		ppair.indexA = i;
		ppair.indexB = j;
		ptpairs.push_back(ppair);
	    }
	}
    }
    std::sort(ptpairs.begin(), ptpairs.end());

    std::vector<ON_SimpleArray<int>*> polylines(curvept.Count());
    int *index = (int*)bu_malloc(curvept.Count() * sizeof(int), "int");
    // index[i] = j means curvept[i] is a startpoint/endpoint of polylines[j]
    int *startpt = (int*)bu_malloc(curvept.Count() * sizeof(int), "int");
    int *endpt = (int*)bu_malloc(curvept.Count() * sizeof(int), "int");
    // index of startpoints and endpoints of polylines[i]

    // Initialize each polyline with only one point.
    for (int i = 0; i < curvept.Count(); i++) {
	ON_SimpleArray<int> *single = new ON_SimpleArray<int>();
	single->Append(i);
	polylines[i] = single;
	index[i] = i;
	startpt[i] = i;
	endpt[i] = i;
    }

    // Merge polylines with distance less than max_dis.
    for (unsigned int i = 0; i < ptpairs.size(); i++) {
	int index1 = index[ptpairs[i].indexA], index2 = index[ptpairs[i].indexB];
	if (index1 == -1 || index2 == -1)
	    continue;
	index[startpt[index1]] = index[endpt[index1]] = index1;
	index[startpt[index2]] = index[endpt[index2]] = index1;
	ON_SimpleArray<int> *line1 = polylines[index1];
	ON_SimpleArray<int> *line2 = polylines[index2];
	if (line1 != NULL && line2 != NULL && line1 != line2) {
	    ON_SimpleArray<int> *unionline = new ON_SimpleArray<int>();
	    if ((*line1)[0] == ptpairs[i].indexA) {
		if ((*line2)[0] == ptpairs[i].indexB) {
		    // Case 1: endA -- startA -- startB -- endB
		    line1->Reverse();
		    unionline->Append(line1->Count(), line1->Array());
		    unionline->Append(line2->Count(), line2->Array());
		    startpt[index1] = endpt[index1];
		    endpt[index1] = endpt[index2];
		} else {
		    // Case 2: startB -- endB -- startA -- endA
		    unionline->Append(line2->Count(), line2->Array());
		    unionline->Append(line1->Count(), line1->Array());
		    startpt[index1] = startpt[index2];
		    endpt[index1] = endpt[index1];
		}
	    } else {
		if ((*line2)[0] == ptpairs[i].indexB) {
		    // Case 3: startA -- endA -- startB -- endB
		    unionline->Append(line1->Count(), line1->Array());
		    unionline->Append(line2->Count(), line2->Array());
		    startpt[index1] = startpt[index1];
		    endpt[index1] = endpt[index2];
		} else {
		    // Case 4: start -- endA -- endB -- startB
		    unionline->Append(line1->Count(), line1->Array());
		    line2->Reverse();
		    unionline->Append(line2->Count(), line2->Array());
		    startpt[index1] = startpt[index1];
		    endpt[index1] = startpt[index2];
		}
	    }
	    polylines[index1] = unionline;
	    polylines[index2] = NULL;
	    if (line1->Count() >= 2) {
		index[ptpairs[i].indexA] = -1;
	    }
	    if (line2->Count() >= 2) {
		index[ptpairs[i].indexB] = -1;
	    }
	    delete line1;
	    delete line2;
	}
    }

    // Generate NURBS curves from the polylines.
    ON_SimpleArray<ON_NurbsCurve *> intersect3d, intersect_uv2d, intersect_st2d;
    for (unsigned int i = 0; i < polylines.size(); i++) {
	if (polylines[i] != NULL) {
	    int startpoint = (*polylines[i])[0];
	    int endpoint = (*polylines[i])[polylines[i]->Count() - 1];

	    // The intersection curves in the 3d space
	    ON_3dPointArray ptarray;
	    for (int j = 0; j < polylines[i]->Count(); j++)
		ptarray.Append(curvept[(*polylines[i])[j]]);
	    // If it form a loop in the 3D space
	    if (curvept[startpoint].DistanceTo(curvept[endpoint]) < max_dis) {
		ptarray.Append(curvept[startpoint]);
	    }
	    ON_PolylineCurve curve(ptarray);
	    ON_NurbsCurve *nurbscurve = ON_NurbsCurve::New();
	    if (curve.GetNurbForm(*nurbscurve)) {
		intersect3d.Append(nurbscurve);
	    }

	    // The intersection curves in the 2d UV parameter space (surfA)
	    ptarray.Empty();
	    for (int j = 0; j < polylines[i]->Count(); j++) {
		ON_2dPoint &pt2d = curveuv[(*polylines[i])[j]];
		ptarray.Append(ON_3dPoint(pt2d.x, pt2d.y, 0.0));
	    }
	    // If it form a loop in the 2D space (happens rarely compared to 3D)
	    if (curveuv[startpoint].DistanceTo(curveuv[endpoint]) < max_dis_2dA) {
		ON_2dPoint &pt2d = curveuv[startpoint];
		ptarray.Append(ON_3dPoint(pt2d.x, pt2d.y, 0.0));
	    }
	    curve = ON_PolylineCurve(ptarray);
	    curve.ChangeDimension(2);
	    nurbscurve = ON_NurbsCurve::New();
	    if (curve.GetNurbForm(*nurbscurve)) {
		intersect_uv2d.Append(nurbscurve);
	    }

	    // The intersection curves in the 2d UV parameter space (surfB)
	    ptarray.Empty();
	    for (int j = 0; j < polylines[i]->Count(); j++) {
		ON_2dPoint &pt2d = curvest[(*polylines[i])[j]];
		ptarray.Append(ON_3dPoint(pt2d.x, pt2d.y, 0.0));
	    }
	    // If it form a loop in the 2D space (happens rarely compared to 3D)
	    if (curvest[startpoint].DistanceTo(curvest[endpoint]) < max_dis_2dB) {
		ON_2dPoint &pt2d = curvest[startpoint];
		ptarray.Append(ON_3dPoint(pt2d.x, pt2d.y, 0.0));
	    }
	    curve = ON_PolylineCurve(ptarray);
	    curve.ChangeDimension(2);
	    nurbscurve = ON_NurbsCurve::New();
	    if (curve.GetNurbForm(*nurbscurve)) {
		intersect_st2d.Append(nurbscurve);
	    }

	    delete polylines[i];
	}
    }

    bu_log("Segments: %d\n", intersect3d.Count());
    bu_free(index, "int");
    bu_free(startpt, "int");
    bu_free(endpt, "int");
    delete rootA;
    delete rootB;

    // Generate ON_SSX_EVENTs
    if (intersect3d.Count()) {
	for (int i = 0; i < intersect3d.Count(); i++) {
	    ON_SSX_EVENT *ssx = new ON_SSX_EVENT;
	    ssx->m_curve3d = intersect3d[i];
	    ssx->m_curveA = intersect_uv2d[i];
	    ssx->m_curveB = intersect_st2d[i];
	    // Now we can only have ssx_transverse
	    ssx->m_type = ON_SSX_EVENT::ssx_transverse;
	    x.Append(*ssx);
	}
    }

    return 0;
}

#endif
