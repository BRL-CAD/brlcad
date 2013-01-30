/*          L I B N U R B S _ C U R V E T R E E . C P P
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
/** @file libnurbs_curvetree.cpp
 *
 * Brief description
 *
 */

#include "libnurbs_curvetree.h"

#include "vmath.h"
#include <iostream>

#include <vector>
#include <map>
#include <set>
#include <queue>

// Return 0 if there are no tangent points in the interval, 1
// if there is a horizontal tangent, two if there is a vertical
// tangent, 3 if a normal split is in order.
int ON_Curve_Has_Tangent(const ON_Curve* curve, double min, double max) {

    bool tanx1, tanx2, x_changed;
    bool tany1, tany2, y_changed;
    bool slopex, slopey; 
    double xdelta, ydelta; 
    ON_3dVector tangent1, tangent2;
    ON_3dPoint p1, p2;
    ON_Interval t(min, max);

    tangent1 = curve->TangentAt(t[0]);
    tangent2 = curve->TangentAt(t[1]);

    tanx1 = (tangent1[X] < 0.0);
    tanx2 = (tangent2[X] < 0.0);
    tany1 = (tangent1[Y] < 0.0);
    tany2 = (tangent2[Y] < 0.0);

    x_changed =(tanx1 != tanx2);
    y_changed =(tany1 != tany2);

    if (x_changed && y_changed) return 3; //horz & vert 
    if (x_changed) return 1;//need to get vertical tangent
    if (y_changed) return 2;//need to find horizontal tangent

    p1 = curve->PointAt(t[0]); 
    p2 = curve->PointAt(t[1]); 

    xdelta = (p2[X] - p1[X]); 
    slopex = (xdelta < 0.0); 
    ydelta = (p2[Y] - p1[Y]); 
    slopey = (ydelta < 0.0); 

    // If we have no slope change
    // in x or y, we have a tangent line
    if (NEAR_ZERO(xdelta, TOL) || NEAR_ZERO(ydelta, TOL)) return 0; 

    if ((slopex != tanx1) || (slopey != tany1)) return 3;

    return 0;
}

void ON_Curve_KDTree(const ON_Curve* curve, const ON_Surface *srf, std::map<std::pair<int, int>, std::pair<double, double> > *local_kdtree) {
    int depth = 0;
    double min, max;
    int knotcnt = curve->SpanCount();
    double *knots = new double[knotcnt + 1];
    (void)curve->GetSpanVector(knots);
    (void)curve->GetDomain(&min, &max);
    std::queue<local_node_t> tree_nodes;
    tree_nodes.push(std::make_pair(std::make_pair(0,0), std::make_pair(min, max)));
    std::pair<int, int> key;
    std::pair<double, double> bounds;
    while (!tree_nodes.empty()) {
	int split= 0;
	local_node_t node = tree_nodes.front();
	tree_nodes.pop();
	// push node into map
	key = node.first;
	bounds = node.second;
	(*local_kdtree)[key] = bounds;
	//std::cout << "Node: (Depth,CellID): (" << (node.first).first << "," << (node.first).second << ") - (tmin,tmax): (" << (node.second).first << "," << (node.second).second << ")\n";
	depth = key.first + 1;
	// Process node to see if further subnodes are needed
	min = (node.second).first;
	max = (node.second).second;
	// standard midpoint, use for splitting unless knots or tangents override
	double mid = min + fabs(max - min)/2;
	double midpt_dist = fabs(max - min);
	ON_Interval ival(min, max);
	ON_3dPoint pmin = curve->PointAt(min);
	ON_3dPoint pmax = curve->PointAt(max);

	// If we have knots in the interval, we need to go deeper
	for (int knot_index = 1; knot_index <= knotcnt; knot_index++) {
	    if (ival.Includes(knots[knot_index], true)) {
		split++;
		double local_dist = fabs(fabs(max - knots[knot_index]) - fabs(min - knots[knot_index]));
		if (local_dist < midpt_dist) {
		    mid = knots[knot_index];
		    midpt_dist = local_dist;
		}
	    }
	}
	// Check for a difference between the normals at the
	// ends of the curve - if this is different for an interval
	// we need to split
	if (!split) {
	    ON_3dVector tangent_start = curve->TangentAt(min);
	    ON_3dVector tangent_end = curve->TangentAt(max);
	    if (tangent_start * tangent_end < BREP_CURVE_FLATNESS) split++;
	}

	// We want subdivided curves to be small relative to their
	// parent surface.
	if (!split && srf) {
	    ON_Interval u = srf->Domain(0);
	    ON_Interval v = srf->Domain(1);
	    ON_3dPoint a(u[0], v[0], 0.0);
	    ON_3dPoint b(u[1], v[1], 0.0);
	    double ab = a.DistanceTo(b);
	    double cd = curve->PointAt(min).DistanceTo(curve->PointAt(max));
	    if (cd > BREP_TRIM_SUB_FACTOR*ab) split++;
	    if (split) std::cout << "Split - size constraint\n";
	}

	// If we have a single horizontal or vertical tangent, we want to
	// split on that point.  If we have multiples, we need further
	// subdivision.
	if (!split) {
	    int tan_result = ON_Curve_Has_Tangent(curve, min, max);
	    if (tan_result == 1 || tan_result == 2) {
		// If we have a tangent point at the min or max, that's OK
		ON_3dVector hv_tangent = curve->TangentAt(max);
		if (NEAR_ZERO(hv_tangent.x, TOL2) || NEAR_ZERO(hv_tangent.y, TOL2)) tan_result = 0;
		hv_tangent = curve->TangentAt(min);
		if (NEAR_ZERO(hv_tangent.x, TOL2) || NEAR_ZERO(hv_tangent.y, TOL2)) tan_result = 0;
		// The tangent point is in a bad spot - find it and subdivide on it
		if(tan_result) {
		    bool tanmin;
		    (tan_result == 1) ? (tanmin = (hv_tangent[X] < 0.0)) : (tanmin = (hv_tangent[Y] < 0.0));
		    double hv_mid;
		    double hv_min = min;
		    double hv_max = max;
		    int found_pnt = 0;
		    // find the tangent point using binary search
		    while (fabs(hv_min - hv_max) > TOL2 && !found_pnt) {
			hv_mid = (hv_max + hv_min)/2.0;
			hv_tangent = curve->TangentAt(hv_mid);
			if ((tan_result == 1 && NEAR_ZERO(hv_tangent[X], TOL2)) 
				||(tan_result == 2 && NEAR_ZERO(hv_tangent[Y], TOL2))) {
			    mid = hv_mid;
			    found_pnt = 1;
			    split++;
			}
			if ((tan_result == 1 && ((hv_tangent[X] < 0.0) == tanmin)) ||
				(tan_result == 2 && ((hv_tangent[Y] < 0.0) == tanmin) )) {
			    hv_min = hv_mid;
			} else {
			    hv_max = hv_mid;
			}
		    }
		} 
	    }
	    // Not ready to identify a specific tangent - just do a generic split
	    if (tan_result == 3) split++;
	} 

	// Now we know if and where to split the curve - if new nodes are needed, add them.
	if(split > 0) {
	    tree_nodes.push(std::make_pair(std::make_pair(depth,(node.first).second), std::make_pair(min, mid)));
	    tree_nodes.push(std::make_pair(std::make_pair(depth,(node.first).second + (2^(depth-1))), std::make_pair(mid, max)));
	} 
	// For debugging at the moment, bail if we run into a case that's crazy deep
	if (depth > 100000) exit(EXIT_FAILURE);
    }

}

ON_CurveTree::ON_CurveTree(const ON_BrepFace* face) 
{
    // Walk over all trimming loops.
    for (int loop_index = 0; loop_index < face->LoopCount(); loop_index++) {
	ON_BoundingBox loop_bb;
	ON_BrepLoop* loop = face->Loop(loop_index);
	if (loop->m_type == ON_BrepLoop::outer)
	    outer_loop = loop_index;
	// Walk over all trims in loop.  In principle, the core curve kdtree
	// buiding step here can be done in parallel - wait until the need is clear
	// to actually do it
	for (int trim_index = 0; trim_index < loop->m_ti.Count(); trim_index++) {
	    ON_BoundingBox bb;
	    const ON_Curve* curve = face->Brep()->m_T[loop->m_ti[trim_index]].TrimCurveOf();
	    /*for (lt_it = local_kdtree->begin(); lt_it != local_kdtree->end(); ++lt_it) {
		// Get the UV points associated with each segment, and use them to make sure
		// the top level trim curve bbox is accurate.
		// TODO - check whether it is faster to cache the local bboxes or just find
		// the leaf nodes and build them on the fly.
		bb.Set(curve->PointAt(((*lt_it).second).first), true);
		bb.Set(curve->PointAt(((*lt_it).second).second), true);
	    }*/
	    curve->GetBBox(bb[0],bb[1]);
	    loop_bb.Set(bb[0], true);
	    loop_bb.Set(bb[1], true);
	}
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
