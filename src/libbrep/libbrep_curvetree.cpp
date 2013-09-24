/*          L I B B R E P _ C U R V E T R E E . C P P
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
/** @file libbrep_curvetree.cpp
 *
 * Brief description
 *
 */

#include "libbrep_curvetree.h"

#include "vmath.h"
#include <iostream>

#include <vector>
#include <map>
#include <set>
#include <queue>
#include <limits>

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

int ON_Curve_HV_Split(const ON_Curve *curve, double min, double max, double *mid) {
    // Don't split if min ~= max - that's infinite loop territory
    if (NEAR_ZERO(max - min, ON_ZERO_TOLERANCE)) return 0;
    // Go HV tangent hunting
    int tan_result = ON_Curve_Has_Tangent(curve, min, max);
    if (tan_result == 3) return 1;
    if (tan_result == 0) return 0;
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
	    // find the tangent point using binary search
	    while (fabs(hv_min - hv_max) > TOL2) {
		hv_mid = (hv_max + hv_min)/2.0;
		hv_tangent = curve->TangentAt(hv_mid);
		if ((tan_result == 1 && NEAR_ZERO(hv_tangent[X], TOL2))
			||(tan_result == 2 && NEAR_ZERO(hv_tangent[Y], TOL2))) {
		    (*mid) = hv_mid;
		    return 1;
		}
		if ((tan_result == 1 && ((hv_tangent[X] < 0.0) == tanmin)) ||
			(tan_result == 2 && ((hv_tangent[Y] < 0.0) == tanmin) )) {
		    hv_min = hv_mid;
		} else {
		    hv_max = hv_mid;
		}
	    }
	    return 0;
	} else {
          // The tangent was at the min and/or max, we don't need to split on account
          // of this case
          return 0;
        }
    }
    // Shouldn't get here - something went drastically wrong if we did
    return -1;
}

double ON_Curve_Solve_For_V(const ON_Curve *curve, double u, double min, double max, double tol) {

    ON_3dVector Tan_start, Tan_end;
    ON_3dPoint p;
    double guess, dT;
    double Ta = min;
    double Tb = max;
    ON_3dPoint A = curve->PointAt(Ta);
    ON_3dPoint B = curve->PointAt(Tb);
    double dU = fabs(A.x - B.x);
    if (dU <= tol) {
       if (A.y <= B.y) {return A.y;} else {return B.y;};
    }
    Tan_start = curve->TangentAt(Ta);
    Tan_end = curve->TangentAt(Tb);
    dT = Tb - Ta;

    /* Use quick binary subdivision until derivatives at end points in 'u' are within 5 percent */
    while (!(fabs(dU) <= tol) && !(fabs(dT) <= tol)) {
        guess = Ta + dT/2;
        p = curve->PointAt(guess);

	/* if we hit 'u' exactly, done deal */
	if (fabs(p.x-u) <= SMALL_FASTF) return p.y;

        if (p.x > u) {
            /* v is behind us, back up the end */
            Tb = guess;
            B = p;
            Tan_end = curve->TangentAt(Tb);
        } else {
            /* v is in front, move start forward */
            Ta = guess;
            A = p;
            Tan_start = curve->TangentAt(Ta);
        }
        dT = Tb - Ta;
        dU = B.x - A.x;
    }

    dU = fabs(B.x - A.x);
    if (dU <= tol) {  //vertical
       if (A.y <= B.y) {return A.y;} else {return B.y;};
    }

    guess = Ta + (u - A.x) * dT/dU;
    p = curve->PointAt(guess);

    int cnt=0;
    while ((cnt < 1000) && (!(fabs(p.x-u) <= tol))) {
        if (p.x < u) {
            Ta = guess;
            A = p;
        } else {
            Tb = guess;
            B = p;
        }
        dU = fabs(B.x - A.x);
        if (dU <= tol) {  //vertical
	    if (A.y <= B.y) {return A.y;} else {return B.y;};
	}

        dT = Tb - Ta;
        guess =Ta + (u - A[X]) * dT/dU;
        p = curve->PointAt(guess);
        cnt++;
    }
    if (cnt > 999) {
        std::cout << "ON_Curve_Solve_For_V(): estimate of 'v' given a trim curve and 'u' did not converge within iteration bound(" << cnt << ")\n";
    }
    return p.y;
}

int ON_Curve_Relative_Size(const ON_Curve *curve, const ON_Surface *srf, double tmin, double tmax, double factor) {
    int retval = 0;
    ON_Interval u = srf->Domain(0);
    ON_Interval v = srf->Domain(1);
    ON_3dPoint a(u[0], v[0], 0.0);
    ON_3dPoint b(u[1], v[1], 0.0);
    double ab = a.DistanceTo(b);
    double cd = curve->PointAt(tmin).DistanceTo(curve->PointAt(tmax));
    if (cd > factor*ab) retval = 1;
    return retval;
}

long int ON_CurveTree::New_Trim_Node(int parent, int trim_index, int tmin, int tmax) {
    // Parent node of new node
    nodes.push_back(parent);
    // Curve parameter information
    nodes.push_back(tmin);
    nodes.push_back(tmax);
    // As yet, the node has no children
    nodes.push_back(0);
    nodes.push_back(0);
    // knots/HV tangents info node
    nodes.push_back(0);
    // Populate uv_bbox_values
    nodes.push_back(uv_bbox_values.size());
    for (int i = 0; i < 2; ++i) uv_bbox_values.push_back(std::numeric_limits<double>::infinity());
    for (int i = 2; i < 4; ++i) uv_bbox_values.push_back(-1 * std::numeric_limits<double>::infinity());
    // Trim index in brep
    nodes.push_back(trim_index);

    return nodes.size() - TRIM_NODE_STEP;
}

unsigned int ON_CurveTree::Depth(long int node_id) {
    if (node_id < first_trim_node) return 0;
    int depth = 1;
    int parent = nodes[node_id];
    while (parent != 0 && parent >= first_trim_node) {
        depth++;
        parent = nodes[parent];
    }
    return depth;
}

int ON_Curve_Flat(const ON_Curve *curve, double min, double max, double tol){
    int retval = 1;
    // If min ~= max, call it flat
    if (NEAR_ZERO(max - min, ON_ZERO_TOLERANCE)) return retval;
    ON_3dVector tangent_start = curve->TangentAt(min);
    ON_3dVector tangent_end = curve->TangentAt(max);
    if (fabs(tangent_start * tangent_end) < tol) retval = 0;
    return retval;
}

void ON_CurveTree::Subdivide_Trim_Node(long int first_node_id, const ON_BrepTrim *trim) {

    // Initialize
    ON_3dPoint pmin, pmax;
    std::set<long int> leaf_nodes;
    const ON_Curve *curve = trim->TrimCurveOf();
    const ON_Surface *srf = trim->SurfaceOf();
    double c_min, c_max;
    int knotcnt = curve->SpanCount();
    double *knots = (double *)malloc(sizeof(double) * (knotcnt + 1));
    (void)curve->GetSpanVector(knots);
    (void)curve->GetDomain(&c_min, &c_max);

    // Populate the root node t_coords
    t_coords.push_back(c_min);
    t_coords.push_back(c_max);
    nodes.at(first_node_id + 1) = t_coords.size() - 2;
    nodes.at(first_node_id + 2) = t_coords.size() - 1;

    // Use a queue to process nodes
    std::queue<long int> trim_node_queue;

    // Prime the queue with the root node
    trim_node_queue.push(first_node_id);

    // Begin the core of the tree building process. Pop a node
    // off the queue, test it to see if it must be split further,
    // and proceed accordingly.
    while (!trim_node_queue.empty()) {
	int split = 0;
	long int node = trim_node_queue.front();
        trim_node_queue.pop();
        double node_tmin = t_coords.at(nodes.at(node+1));
        double node_tmax = t_coords.at(nodes.at(node+2));
        //std::cout << "tmin: " << node_tmin << ", tmax: " << node_tmax << "\n";
	ON_Interval node_ival(node_tmin, node_tmax);

	// Determine the depth of this node
	int node_depth = this->Depth(node);
	//std::cout << "Node depth: " << node_depth << "\n";

        // Use a variable for the midpoint, since it may be overridden
        // if we have knots or tangent points to split on.
	double t_mid = node_ival.Mid();

        // If we have knots in the interval, we need to split
	split += ON_Interval_Find_Split_Knot(&node_ival, knotcnt, knots, &t_mid);
        if (split) nodes.at(node+5) = 1;
	if (split) {
	    //std::cout << "Knot split:" << node << "," << t_mid << "\n";
	    //if(NEAR_ZERO(t_mid-node_tmin, TOL2) || NEAR_ZERO(t_mid-node_tmax, TOL2)) std::cout << "Arrgh! Knots\n";
	}
        // If we aren't already splitting, check for horizontal and vertical tangents
	if (!split) {
	    split += ON_Curve_HV_Split(curve, node_tmin, node_tmax, &t_mid);
	    if (split) nodes.at(node+5) = 1;
	    if (split) {
		//std::cout << "HV split:" << node << "," << t_mid << "\n";
		//if(NEAR_ZERO(t_mid-node_tmin, TOL2) || NEAR_ZERO(t_mid-node_tmax, TOL2)) std::cout << "Arrgh! HV\n";
	    }
	}

        // If we aren't already splitting, check flatness
        if (!split) {
	    split += !ON_Curve_Flat(curve, node_tmin, node_tmax, BREP_CURVE_FLATNESS);
	    if (split) {
		//std::cout << "Flatness:" << node << "\n";
		//if(NEAR_ZERO(t_mid-node_tmin, TOL2) || NEAR_ZERO(t_mid-node_tmax, TOL2)) std::cout << "Arrgh! Flat\n";
	    }
	}

	// We want subdivided curves to be small relative to their
	// parent surface.
	if (!split) {
	    split += ON_Curve_Relative_Size(curve, srf, node_tmin, node_tmax, BREP_TRIM_SUB_FACTOR);
	    if (split) {
		// std::cout << "Relative:" << node << "\n";
		//if(NEAR_ZERO(t_mid-node_tmin, TOL2) || NEAR_ZERO(t_mid-node_tmax, TOL2)) std::cout << "Arrgh! Relative\n";
	    }
	}

        // If we DO need to split, proceed
        if (split) {
	    long int left_child, right_child;
	    // Push the new midpoint onto tcoords
	    t_coords.push_back(t_mid);
	    left_child = this->New_Trim_Node(node, nodes.at(node+7), nodes.at(node+1), t_coords.size() - 1);
	    nodes.at(node+3) = left_child;
	    trim_node_queue.push(left_child);
	    right_child = this->New_Trim_Node(node, nodes.at(node+7), t_coords.size() - 1, nodes.at(node+2));
	    nodes.at(node+4) = right_child;
	    trim_node_queue.push(right_child);
        } else {
            double pmin_u, pmin_v, pmax_u, pmax_v;
            leaf_nodes.insert(node);
            pmin = curve->PointAt(node_tmin);
            pmax = curve->PointAt(node_tmax);
            (pmin.x > pmax.x) ? (pmin_u = pmax.x) : (pmin_u = pmin.x);
            (pmax.x > pmin.x) ? (pmax_u = pmax.x) : (pmax_u = pmin.x);
            (pmin.y > pmax.y) ? (pmin_v = pmax.y) : (pmin_v = pmin.y);
            (pmax.y > pmin.y) ? (pmax_v = pmax.y) : (pmax_v = pmin.y);
            uv_bbox_values.at(nodes.at(node+6)) = pmin_u;
            uv_bbox_values.at(nodes.at(node+6)+1) = pmin_v;
            uv_bbox_values.at(nodes.at(node+6)+2) = pmax_u;
            uv_bbox_values.at(nodes.at(node+6)+3) = pmax_v;
	    //std::cout << "node " << node << " bbox:" << pmin_u << "," << pmin_v << "," << pmax_u << "," << pmax_v << "\n";
        }

    }
    // Clean up
    free(knots);

    // Sync bboxes up to the root trim node
    std::set<long int> node_set;
    std::set<long int>::iterator p_it;
    std::set<long int> *parent_nodes, *current_nodes, *temp;
    double *p_umin, *p_vmin, *p_umax, *p_vmax;
    double *c_umin, *c_vmin, *c_umax, *c_vmax;
    current_nodes = &leaf_nodes;
    parent_nodes = &node_set;
    while (!current_nodes->empty()) {
	for (p_it = current_nodes->begin(); p_it != current_nodes->end(); ++p_it) {
	    if ((*p_it) > first_node_id) {
		long int parent_id = nodes.at((*p_it));
		p_umin = &(uv_bbox_values[nodes.at(parent_id+6)]);
		p_vmin = &(uv_bbox_values[nodes.at(parent_id+6)+1]);
		p_umax = &(uv_bbox_values[nodes.at(parent_id+6)+2]);
		p_vmax = &(uv_bbox_values[nodes.at(parent_id+6)+3]);
		//std::cout << "pnode " << parent_id << " bbox:" << *p_umin << "," << *p_vmin << "," << *p_umax << "," << *p_vmax << "\n";
		c_umin = &(uv_bbox_values[nodes.at((*p_it)+6)]);
		c_vmin = &(uv_bbox_values[nodes.at((*p_it)+6)+1]);
		c_umax = &(uv_bbox_values[nodes.at((*p_it)+6)+2]);
		c_vmax = &(uv_bbox_values[nodes.at((*p_it)+6)+3]);
		//std::cout << "cnode " << (*p_it) << " bbox:" << *c_umin << "," << *c_vmin << "," << *c_umax << "," << *c_vmax << "\n";
		if (*c_umin < *p_umin) (*p_umin) = (*c_umin);
		if (*c_vmin < *p_vmin) (*p_vmin) = (*c_vmin);
		if (*c_umax > *p_umax) (*p_umax) = (*c_umax);
		if (*c_vmax > *p_vmax) (*p_vmax) = (*c_vmax);
		if (parent_id > first_node_id) parent_nodes->insert(parent_id);
		//std::cout << "pnode u" << parent_id << " bbox:" << *p_umin << "," << *p_vmin << "," << *p_umax << "," << *p_vmax << "\n";
	    }
	}
        current_nodes->clear();
        temp = parent_nodes;
        parent_nodes = current_nodes;
        current_nodes = temp;
    }
}

// When testing a curve for trimming contributions, the first stage is to check
// the curve tree node to see whether it contains a segment from below the knot
// and HV tangent breakdowns of the curve.  If it does not, we need to walk down
// the tree until we have the set of nodes that represent corresponding sub-segments
// that are below the knot and HV tangent breakdowns.
//
// Once we have an appropriate segment set, do the pnpoly intersection checks.
// For anything where the point is not in the segment bounding box, the test is
// simple - if the point is within the bounding box of the node, walk down the
// tree until either a determination can be made or the point is found to be
// within a leaf node.
//
// If the point is within a leaf node, the more expensive search against the
// actual curve to determine 2D ray intersection status is necessary.
bool ON_CurveTree::CurveTrim(double u, double v, long int node) {
    bool c = false;
    std::set<long int> test_nodes;
    std::set<long int>::iterator tn_it;
    std::queue<long int> q1, q2;
    std::queue<long int> *current_queue, *next_queue, *tmp;
    // Get the node or nodes we need into test_nodes
    q1.push(node);
    current_queue = &q1;
    next_queue= &q2;
    while (!current_queue->empty()) {
	while (!current_queue->empty()) {
	    long int current_node = current_queue->front();
	    current_queue->pop();
	    if (nodes.at(current_node+5) != 1) {
		test_nodes.insert(current_node);
	    } else {
		next_queue->push(nodes.at(current_node+3));
		next_queue->push(nodes.at(current_node+4));
	    }
	}
	tmp = current_queue;
	current_queue = next_queue;
	next_queue = tmp;
    }
    // Check node bounding boxes vs uv point
    double *c_umin, *c_vmin, *c_umax, *c_vmax;
    for(tn_it = test_nodes.begin(); tn_it != test_nodes.end(); ++tn_it) {
	c_umin = &uv_bbox_values[nodes.at((*tn_it)+6)];
	c_vmin = &uv_bbox_values[nodes.at((*tn_it)+6)+1];
	c_umax = &uv_bbox_values[nodes.at((*tn_it)+6)+2];
	c_vmax = &uv_bbox_values[nodes.at((*tn_it)+6)+3];
        // Are we inside the u interval of the bbox?
        if ((u <= *c_umax) && (u > *c_umin)) {
           // Are we below the bbox?  if so, a v+ ray is guaranteed to intersect
           if (v <= *c_vmin) {
              c = !c;
           } else {
               // We're not below it - are we inside it?  If not, no intersection possible,
               // otherwise, need closer look
	       if (v < *c_vmax) {
                   // Decend the tree until either we're outside
                   // the bboxes and can make a determination, or
                   // we find the leaf node containing the uv point
                   // and do the full curve test.
		   current_queue->push((*tn_it));
		   while (!current_queue->empty()) {
		       while (!current_queue->empty()) {
			   long int current_node = current_queue->front();
			   current_queue->pop();
			   c_umin = &uv_bbox_values[nodes.at(current_node+6)];
			   c_vmin = &uv_bbox_values[nodes.at(current_node+6)+1];
			   c_umax = &uv_bbox_values[nodes.at(current_node+6)+2];
			   c_vmax = &uv_bbox_values[nodes.at(current_node+6)+3];
			   if ((u <= *c_umax) && (u >= *c_umin)) {
			       if (v <= *c_vmin) {
				   c = !c;
			       } else {
				   if (v < *c_vmax) {
                                       // If we are at a leaf, it's time to do a
                                       // close check.  Otherwise, queue the children
				       if (nodes.at(current_node + 3)) {
					   next_queue->push(nodes.at(current_node + 3));
					   next_queue->push(nodes.at(current_node + 4));
				       } else {
					   // We're close to a leaf
					   const ON_Curve *curve = this->ctree_face->Brep()->m_C2[(int)nodes.at(current_node + 7)];
					   double v_curve = ON_Curve_Solve_For_V(curve, u, nodes.at(current_node + 1), nodes.at(current_node + 2), TOL3);
					   if (v <= v_curve) c = !c;
				       }
				   }
			       }
			   }
		       }
		       tmp = current_queue;
		       current_queue = next_queue;
		       next_queue = tmp;
		   }
	       }
           }
        }
    }
    return c;
}

// Remember - unlike other
// trimming loops, points inside the outer loop are untrimmed and
// points outside it ARE trimmed.  Flip test results accordingly.
bool ON_CurveTree::IsTrimmed(double u, double v, std::vector<long int> *subset, bool pnpoly_state) {
    //std::cout << "u :" << u << ", v :" << v << "\n";
    int last_loop = 0;
    std::set<long int> loops_to_check;
    long int node_pos = 0;
    // First stage - identify which trimming loop(s) we care about, based on loop UV bounding boxes
    while (last_loop != 1) {
	double *p_umin = &uv_bbox_values[nodes.at(node_pos+2)];
	double *p_vmin = &uv_bbox_values[nodes.at(node_pos+2)+1];
	double *p_umax = &uv_bbox_values[nodes.at(node_pos+2)+2];
	double *p_vmax = &uv_bbox_values[nodes.at(node_pos+2)+3];
        //std::cout << "loop " << node_pos << " bbox:" << (*p_umin) << "," << (*p_vmin) << "," << (*p_umax) << "," << (*p_vmax) << "\n";
        // Is uv point inside bbox?
        if (!( u < (*p_umin) || v < (*p_vmin) || u > (*p_umax) || v > (*p_vmax) )) loops_to_check.insert(node_pos);
	// Jump the starting position to the next loop.
        node_pos = nodes.at(node_pos);
	if (node_pos == 0) last_loop = 1;
    }
    std::set<long int>::iterator l_it;
    if (loops_to_check.size() > 1) {
	std::cout << "Loops possibly containing UV pt (" << u << "," << v << "):";
	for(l_it = loops_to_check.begin(); l_it != loops_to_check.end(); ++l_it) {
	    std::cout << " " << (*l_it) << " ";
	}
    std::cout << "\n";
    for(l_it = loops_to_check.begin(); l_it != loops_to_check.end(); ++l_it) {
       long int current_node_pos = (*l_it) + 3;
       long int final_node_pos = (*l_it) + nodes.at((*l_it)+1) + 3;
       while (current_node_pos <= final_node_pos) {
           //std::cout << "loop " << (*l_it) << ", trim " << nodes.at(current_node_pos) << "\n";
           current_node_pos++;
       }
    }
    std::cout << "\n";
}
}

ON_CurveTree::ON_CurveTree(const ON_BrepFace* face)
{
    // Save the face pointer associated with this tree, in case we
    // need to access data from the face
    ctree_face = face;

    // We can find out in advance how many node elements we will have due to
    // trimming loops - populate those elements up front.
    long int elements = 0;
    for (int loop_index = 0; loop_index < face->LoopCount(); loop_index++) {
	elements += face->Loop(loop_index)->TrimCount();
    }
    elements += (face->LoopCount()) * NON_TRIM_LOOP_STEP;
    // Import to initialize to zero - used later as stopping criteria
    nodes.assign(elements, 0);

    // Any node elements after those already in place will pertain to trim
    // nodes - to be able to easily distinguish when an index is referring
    // to a loop or a trim, record the index which marks the cross-over point
    // in the vector.  The test is then a simple < or >= check.
    first_trim_node = nodes.size();

    // Now that we have the setup, add actual information and handle the trims
    long int node_pos = 0;
    for (int loop_index = 0; loop_index < face->LoopCount(); loop_index++) {
	ON_BrepLoop *loop = face->Loop(loop_index);
	nodes.at(node_pos + 1) = loop->TrimCount();
	// Populate uv_bbox_values
	nodes.at(node_pos + 2) = uv_bbox_values.size();
	for (int i = 0; i < 2; ++i) uv_bbox_values.push_back(std::numeric_limits<double>::infinity());
	for (int i = 2; i < 4; ++i) uv_bbox_values.push_back(-1 * std::numeric_limits<double>::infinity());

	for (int trim_index = 0; trim_index < loop->TrimCount(); trim_index++) {
            int curve_index = loop->Trim(trim_index)->TrimCurveIndexOf();
            long int current_trim_node = node_pos+NON_TRIM_LOOP_STEP+trim_index;
            // Add "leaf" trim node from loop
            long int trim_root = this->New_Trim_Node(node_pos, curve_index, 0, 0);
	    nodes.at(current_trim_node) = trim_root;
            // Build KDtree below "leaf" trim
            this->Subdivide_Trim_Node(trim_root, face->Loop(loop_index)->Trim(trim_index));
	}
        // Now that we've handled the trims, build the loop bbox
	double *p_umin = &uv_bbox_values[nodes.at(node_pos+2)];
	double *p_vmin = &uv_bbox_values[nodes.at(node_pos+2)+1];
	double *p_umax = &uv_bbox_values[nodes.at(node_pos+2)+2];
	double *p_vmax = &uv_bbox_values[nodes.at(node_pos+2)+3];
	for (int trim_index = 0; trim_index < loop->TrimCount(); trim_index++) {
	    long int current_trim_node = nodes.at(node_pos+NON_TRIM_LOOP_STEP+trim_index);
	    double *c_umin = &uv_bbox_values[nodes.at(current_trim_node+6)];
	    double *c_vmin = &uv_bbox_values[nodes.at(current_trim_node+6)+1];
	    double *c_umax = &uv_bbox_values[nodes.at(current_trim_node+6)+2];
	    double *c_vmax = &uv_bbox_values[nodes.at(current_trim_node+6)+3];
	    if (*c_umin < *p_umin) (*p_umin) = (*c_umin);
            if (*c_vmin < *p_vmin) (*p_vmin) = (*c_vmin);
            if (*c_umax > *p_umax) (*p_umax) = (*c_umax);
            if (*c_vmax > *p_vmax) (*p_vmax) = (*c_vmax);
	}
        std::cout << "loop " << node_pos << " bbox:" << (*p_umin) << "," << (*p_vmin) << "," << (*p_umax) << "," << (*p_vmax) << "\n";

        // Identify the size of this loop node and store it in the first entry
        // to enable "walking" of the loops
        // Jump the starting position to the next loop.
        if (loop_index != face->LoopCount() - 1)
	    nodes.at(node_pos) = node_pos + loop->TrimCount() + NON_TRIM_LOOP_STEP + 1;
	node_pos = nodes.at(node_pos);
    }
}

ON_CurveTree::~ON_CurveTree() {
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
