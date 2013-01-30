/*          L I B N U R B S _ S U R F A C E T R E E . H
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
/** @file libnurbs_surfacetree.h
 *
 * Brief description
 *
 */

#if !defined(LIBNURBS_SURFACETREE_)
#define LIBNURBS_SURFACETREE_

#include <vector>
#include <map>
#include <set>
#include <queue>

#include "libnurbs_surfacetreeutil.h"
#include "libnurbs_curvetree.h"

#define NODE_STEP 12

/* Maximum per-surface BVH depth */
#define BREP_MAX_FT_DEPTH 8
#define BREP_MAX_LN_DEPTH 20
#define SIGN(x) ((x) >= 0 ? 1 : -1)
/* Surface flatness parameter, Abert says between 0.8-0.9 */
#define BREP_SURFACE_FLATNESS 0.85
#define BREP_SURFACE_STRAIGHTNESS 0.75
/* Max newton iterations when finding closest point */
#define BREP_MAX_FCP_ITERATIONS 50
/* Root finding epsilon */
#define BREP_FCP_ROOT_EPSILON 1e-5

/* subdivision size factors */
#define BREP_SURF_SUB_FACTOR 1

/**
 * The EDGE_MISS_TOLERANCE setting is critical in a couple of ways -
 * too small and the allowed uncertainty region near edges will be
 * smaller than the actual uncertainty needed for accurate solid
 * raytracing, too large and trimming will not be adequate.  May need
 * to adapt this to the scale of the model, perhaps using bounding box
 * size to key off of.
 */
/* #define BREP_EDGE_MISS_TOLERANCE 5e-2 */
#define BREP_EDGE_MISS_TOLERANCE 5e-3
#define BREP_SAME_POINT_TOLERANCE 1e-6


/// arbitrary calculation tolerance (need to try VDIVIDE_TOL or VUNITIZE_TOL to tighten the bounds)
#ifndef TOL
#  define TOL 0.000001
#endif
//
///// another arbitrary calculation tolerance (need to try VDIVIDE_TOL or VUNITIZE_TOL to tighten the bounds)
#ifndef TOL2
#  define TOL2 0.00001
#endif

//////////////////////////////////////////////////

//         Surface Tree Functionality

//////////////////////////////////////////////////
/*
struct ON_SplitTest_Container {
    int split = 0;
    int u_split = 0;
    int v_split = 0;
    int trimmed = 0;
    std::vector<int> *frame_indices;
    ON_Interval u_local;
    ON_Interval v_local;
    ON_Surface *t1 = NULL;
    ON_Surface *t2 = NULL;
    ON_Surface *t3 = NULL;
    ON_Surface *sub_surface = NULL;
    ON_SimpleArray<ON_Plane> frames;
}
*/

////////////////////////////////////////////////////////////////
//
//   ON_SurfaceTree 
//
//  A "node" is a series of long integers stored in a vector,
//  which point to either the index location of other nodes
//  in the vector or the index into an asset array.
//
//  The first element (index 0) in the vector identifies the
//  tree type.  The categories are as follows
//
//  0 - standard quad tree
//  1 - temporary quad tree built using leaf nodes of a primary tree
//
//  The purpose of this mechanism is to allow the primary tree to
//  contain references into a temporary tree, while still allowing the
//  tree walking and other mechanisms to access the correct data. All
//  functions doing array lookups should be supplied with an array
//  type as a function parameter and use it to work with either nodes
//  or temp_nodes to retrieve data.
//
//  The position of each vector element in a "node" is as follows:
//
//  index + 0  :  holds the index of the parent node of this node.
//                The root node (1) is its own parent and is set to
//                1.  The tree's root node (1) is never a valid child node.
//  index + 1  :  Index into the vector of doubles identifying umin for this node.
//  index + 2  :  Index into the vector of doubles identifying umax for this node.
//  index + 3  :  Index into the vector of doubles identifying vmin for this node.
//  index + 4  :  Index into the vector of doubles identifying vmax for this node.
//                Specific values are retrieved with the offsets:
//  index + 5  :  holds the index of the southwest child node of
//                this node.  A value of zero indicates this node
//                does not have this child node.
//  index + 6  :  holds the index of the southeast child node of
//                this node.  A value of zero indicates this node
//                does not have this child node.
//  index + 7  :  holds the index of the northeast child node of
//                this node.  A value of zero indicates this node
//                does not have this child node.
//  index + 8  :  holds the index of the northwest child node of
//                this node.  A value of zero indicates this node
//                does not have this child node.
//  index + 9  :  Index of the node's BoundingBox.
//  index + 10 :  Trimming status of this node: 0 means untested,
//                1 means fully trimmed away, 2 means no trimming curves 
//                intersect this node, and 3 means a test must
//                be made to determine if a particular point within
//                this patch is trimmed or untrimmed.  Matters only
//                for leaf nodes - those with childeren will ask them.
//  index + 11 :  The index of a temporary child node created
//                using this node as a starting point.  The default
//                value is zero, indicating no such node exists.
//                The purpose of this mechanism is to allow a temporary
//                deepening of a surface tree locally, and then allow
//                the temporary resources to be cleared while leaving
//                the primary tree intact.  If the index is non-zero
//                it is a reference into the temp_nodes vector, rather
//                than the primary vector.
               
               
               
class ON_CLASS  ON_SurfaceTree
{
    public:

	ON_SurfaceTree(const ON_BrepFace*);
	~ON_SurfaceTree();


	//void LeafNodes(std::set<int_pair_t> *);
    private:

        long int New_Node(int, int, int, int, int);
	void Node_Split_UV(double, double, std::pair<int, std::vector<int> *> *, std::queue<std::vector<int> *> *, std::queue<std::pair<int, std::vector<int> *> > *);
	void Node_Split_U(double, std::pair<int, std::vector<int> *> *, std::queue<std::vector<int> *> *, std::queue<std::pair<int, std::vector<int> *> > *);
	void Node_Split_V(double, std::pair<int, std::vector<int> *> *, std::queue<std::vector<int> *> *, std::queue<std::pair<int, std::vector<int> *> > *);

	std::vector<long int> nodes;
	std::vector<double> uv_coords;
	ON_SimpleArray<ON_BoundingBox> bboxes;
	// Knot information
	int u_knotcnt;
	int v_knotcnt;
	double *u_knots;
	double *v_knots;
	// Contains for temporary data - these are intended
	// to be cleared after completion of a task requiring
	// local tree refinement
	std::vector<long int> temp_nodes;
	ON_SimpleArray<ON_BoundingBox> temp_bboxes;
	std::vector<double> temp_uv_coords;
	ON_CurveTree *m_ctree;
};

#endif

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
