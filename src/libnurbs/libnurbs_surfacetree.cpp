/*        L I B N U R B S _ S U R F A C E T R E E . C P P
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
/** @file libnurbs_surfacetree.cpp
 *
 * Brief description
 *
 */

#include <vector>
#include <map>
#include <set>
#include <queue>
#include <iostream>

#include "libnurbs_surfacetree.h"

long int ON_SurfaceTree::New_Node(int parent, int umin, int umax, int vmin, int vmax) {
    // Initialize the tree with the root node.
    nodes.push_back(parent);
    // UV information
    nodes.push_back(umin);
    nodes.push_back(umax);
    nodes.push_back(vmin);
    nodes.push_back(vmax);
    // As yet, the node has no children.
    for (int i = 0; i < 4; ++i) nodes.push_back(0);
    ON_BoundingBox surf_bbox;
    bboxes.Append(surf_bbox);
    nodes.push_back(bboxes.Count() - 1);
    nodes.push_back(0); // We haven't tested for trimming curves yet.
    nodes.push_back(0); // No temporary nodes as yet.
    return nodes.size() - NODE_STEP;
}

// Split a node to produce new nodes in both U and V directions
void ON_SurfaceTree::Node_Split_UV(double u_mid, double v_mid, std::pair<int, std::vector<int> *> *node, std::queue<std::vector<int> *> *frame_arrays, std::queue<std::pair<int, std::vector<int> *> > *node_queue) {
    std::vector<int> *parent_frame_index = node->second;
 
   // Four new frame index arrays will be needed
    std::vector<int> *q1f = NULL;    
    std::vector<int> *q2f = NULL;    
    std::vector<int> *q3f = NULL;    
    std::vector<int> *q4f = NULL;

    // Add the midpoint U and V coordiates, which will be the
    // only UV values not already present in the double array
    uv_coords.push_back(u_mid);
    int umid_id = uv_coords.size() - 1;
    uv_coords.push_back(v_mid);
    int vmid_id = uv_coords.size() - 1;

    // Create child nodes.  Reuse frame arrays if any are available to reuse
    
    // Create the SW node
    long int q1 = this->New_Node(node->first, nodes[node->first+1], umid_id, nodes[node->first+3], vmid_id);
    nodes[node->first+5] = q1;
    if (!frame_arrays->empty()) {q1f = frame_arrays->front(); frame_arrays->pop();} else {q1f = new std::vector<int>;}
    q1f->assign(13, -1);
    q1f->at(0) = parent_frame_index->at(0); q1f->at(1) = parent_frame_index->at(9); q1f->at(2) = parent_frame_index->at(4); 
    q1f->at(3) = parent_frame_index->at(10); q1f->at(4) = parent_frame_index->at(5);
    node_queue->push(std::make_pair(q1,q1f)); 

    // Create the SE node
    long int q2 = this->New_Node(node->first, umid_id, nodes[node->first+2], nodes[node->first+3], vmid_id);
    nodes[node->first+6] = q2;
    if (!frame_arrays->empty()) {q2f = frame_arrays->front(); frame_arrays->pop();} else {q2f = new std::vector<int>;}
    q2f->assign(13, -1);
    q2f->at(0) = parent_frame_index->at(9); q2f->at(1) = parent_frame_index->at(1); q2f->at(2) = parent_frame_index->at(12); 
    q2f->at(3) = parent_frame_index->at(4); q1f->at(4) = parent_frame_index->at(7);
    node_queue->push(std::make_pair(q2,q2f)); 

    // Create the NE node
    long int q3 = this->New_Node(node->first, umid_id, nodes[node->first+2], vmid_id, nodes[node->first+4]);
    nodes[node->first+7] = q3;
    if (!frame_arrays->empty()) {q3f = frame_arrays->front(); frame_arrays->pop();} else {q3f = new std::vector<int>;}
    q3f->assign(13, -1);
    q3f->at(0) = parent_frame_index->at(4); q3f->at(1) = parent_frame_index->at(12); q3f->at(2) = parent_frame_index->at(2); 
    q3f->at(3) = parent_frame_index->at(11); q3f->at(4) = parent_frame_index->at(8);
    node_queue->push(std::make_pair(q3,q3f)); 

    // Create the NW node
    long int q4 = this->New_Node(node->first, nodes[node->first+1], umid_id, vmid_id, nodes[node->first+4]);
    nodes[node->first+8] = q4;
    if (!frame_arrays->empty()) {q4f = frame_arrays->front(); frame_arrays->pop();} else {q4f = new std::vector<int>;}
    q4f->assign(13, -1);
    q4f->at(0) = parent_frame_index->at(10); q4f->at(1) = parent_frame_index->at(4); q4f->at(2) = parent_frame_index->at(11); 
    q4f->at(3) = parent_frame_index->at(3); q1f->at(4) = parent_frame_index->at(6);
    node_queue->push(std::make_pair(q4,q4f)); 
}

// Create two child nodes, splitting in the U direction
void ON_SurfaceTree::Node_Split_U(double u_mid, std::pair<int, std::vector<int> *> *node, std::queue<std::vector<int> *> *frame_arrays, std::queue<std::pair<int, std::vector<int> *> > *node_queue) {
}

// Create two child nodes, splitting in the V direction
void ON_SurfaceTree::Node_Split_V(double v_mid, std::pair<int, std::vector<int> *> *node, std::queue<std::vector<int> *> *frame_arrays, std::queue<std::pair<int, std::vector<int> *> > *node_queue) {
}


ON_SurfaceTree::ON_SurfaceTree(const ON_BrepFace* face) {
    int depth = 0;
    int uv = 0;
    int first_node_id = 0;
    ON_SimpleArray<ON_Plane> frames;

    // We need a curve tree
    m_ctree = new ON_CurveTree(face);

    // Initialize
    const ON_Surface *srf = face->SurfaceOf();
    u_knotcnt = srf->SpanCount(0);
    v_knotcnt = srf->SpanCount(1);
    u_knots = new double[u_knotcnt + 1];
    v_knots = new double[v_knotcnt + 1];
    (void)srf->GetSpanVector(0, u_knots);
    (void)srf->GetSpanVector(1, v_knots);
    ON_Interval u_domain = srf->Domain(0);
    ON_Interval v_domain = srf->Domain(1);
    nodes.push_back(0);
    temp_nodes.push_back(1);
    // When we start splitting surfaces later, we need
    // some scratch surfaces to avoid lots of malloc
    // activity.
    ON_Surface *t1 = NULL;
    ON_Surface *t2 = NULL;
    ON_Surface *t3 = NULL;
    ON_Surface *t4 = NULL;
    ON_Surface *sub_surface = NULL;
    ON_Surface_Create_Scratch_Surfaces(&t1, &t2, &t3, &t4);

    // Initialize the tree with the root node.
    uv_coords.push_back(u_domain.Min());
    uv_coords.push_back(u_domain.Max());
    uv_coords.push_back(v_domain.Min());
    uv_coords.push_back(v_domain.Max());
    uv = uv_coords.size() - 1;
    first_node_id = this->New_Node(1, uv - 3, uv - 2, uv - 1, uv);

    // Each node has a set of frames, but those frames are
    // often shared with parent and sibling nodes.  In order
    // to take at least some advantage of this, we store
    // all frames in an array and use vectors of indicies to
    // point to a specific frame.  When a vector is no longer
    // needed, we queue it up in an "available" queue for reuse.
    std::queue<std::vector<int> *> frame_arrays;

    // Initialize with the root frame vector
    std::vector<int> *root_frame_index = new std::vector<int>;
    root_frame_index->assign(13, -1);

    // Processing of nodes is handled with a queue - each node,
    // if it splits out into child nodes, queues up its child
    // nodes in the work queue for further processing.  The
    // queue holds only two pieces of information - a nodes position
    // in the vector, and the index of the frame vector associated
    // with this particular node.
    std::queue<std::pair<int, std::vector<int> *> > node_queue;

    // Prime the queue with the root node.
    node_queue.push(std::make_pair(first_node_id,root_frame_index)); 

    // Begine the core of the tree building process.  Pop a node
    // off the queue, test it to see if it must be split further,
    // and proceed accordingly.
    while (!node_queue.empty()) {
	int split = 0;
	int u_split = 0;
	int v_split = 0;
	int trimmed = 0;
	std::pair<int, std::vector<int> *> node = node_queue.front();
	node_queue.pop();
	int node_id = node.first;
	std::vector<int> *frame_index = node.second;
	ON_Interval u_local(uv_coords.at(nodes[node_id+1]), uv_coords.at(nodes[node_id+2]));
	ON_Interval v_local(uv_coords.at(nodes[node_id+3]), uv_coords.at(nodes[node_id+4]));

	// Before we do anything else, find out the trimming status of this node.  If it is
	// fully trimmed out of the tree, there is no need to process it any further.
	trimmed = ON_Surface_Patch_Trimmed(srf, m_ctree, &u_local, &v_local); 

	//if (!trimmed == 0) {
	if (trimmed == 0) {

	    // Because the knots may override using Mid(), we have local variables to hold the
	    // working values.
	    double u_mid = u_local.Mid();
	    double v_mid = v_local.Mid();

	    // Get the local sub-surface

	    bool split_success = ON_Surface_SubSurface(srf, &sub_surface, &u_local, &v_local, &t1, &t2, &t3, &t4);

	    // Test for knots - if we have knots, we must split regardless of flatness or dimensions.
	    split += ON_Surface_Knots_Split(&u_local, &v_local, u_knotcnt, v_knotcnt, u_knots, v_knots, &u_mid, &v_mid);
	    if (fabs(u_local.Mid() - u_mid) > ON_ZERO_TOLERANCE) u_split++;
	    if (fabs(v_local.Mid() - v_mid) > ON_ZERO_TOLERANCE) v_split++;

	    // If we're proceeding, go ahead and populate the frame
	    if (ON_Populate_Frame_Array(srf, &u_local, &v_local, u_mid, v_mid, &frames, frame_index)) 
		std::cout << "Warning - Initial frame array build failure!"; 


	    // Pull the NURBS surface for this patch and test the
	    // "unrolled" width and height - too distored, and we will need to split in just U
	    // or just V to try and keep reasonably close to "square" patches.
	    if (split_success)
		split += ON_Surface_Width_vs_Height(sub_surface, 5.0, &u_split, &v_split);

	    // Check the flatness and straightness of this particular patch
	    if (!split) {
		// test flat and straight
		split += ON_Surface_Flat_Straight(&frames, frame_index, BREP_SURFACE_FLATNESS, BREP_SURFACE_STRAIGHTNESS);
		if (split) {
		    u_split += ON_Surface_Flat_U(&frames, frame_index, BREP_SURFACE_FLATNESS);
		    v_split += ON_Surface_Flat_V(&frames, frame_index, BREP_SURFACE_FLATNESS);
		}
	    }

	    // If we DO need to split, proceed
	    if (split) {
		// For efficiency, we calculate four additional frame here that are shared by the children
		if (ON_Extend_Frame_Array(srf, &u_local, &v_local, u_mid, v_mid, &frames, frame_index)) 
		    std::cout << "Warning - Frame array extension failure!"; 

                // Generate the new nodes
		if ((u_split && v_split) || (!u_split && !v_split)) {
		    this->Node_Split_UV(u_mid, v_mid, &node, &frame_arrays, &node_queue);
		} else {
		    if (u_split && !v_split) {
			this->Node_Split_U(u_mid, &node, &frame_arrays, &node_queue);
		    }
		    if (!u_split && v_split) {
			this->Node_Split_V(v_mid, &node, &frame_arrays, &node_queue);
		    }
		}
	    }
	}
	// Whether this is a leaf or has produced children, we are done with its
	// frame vector.  Add it to the frame vector queue for re-use.
	node.second->clear();
	frame_arrays.push(node.second);
    }

    // Now that we're all done with sub-surfaces, delete our working surface memory
    delete t1;
    delete t2;
    delete t3;
    delete sub_surface;
    while (!frame_arrays.empty()) {
	delete frame_arrays.front();
	frame_arrays.pop();
    }
}


ON_SurfaceTree::~ON_SurfaceTree()
{
}

/*
TODO Add the fast bbox/ray intersection test from:
http://www.cg.cs.tu-bs.de/media/publications/fast-rayaxis-aligned-bounding-box-overlap-tests-using-ray-slopes.pdf
http://graphics.tu-bs.de/people/eisemann/code/RaySlopeIntersection.zip

Will probably have use for this too (inside-outside polygon test, does the 2d raycasting bit):
http://www.ecse.rpi.edu/Homepages/wrf/Research/Short_Notes/pnpoly.html
*/


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
