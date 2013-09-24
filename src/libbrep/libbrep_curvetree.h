/*            L I B B R E P _ C U R V E T R E E . H
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
/** @file libbrep_curvetree.h
 *
 * Brief description
 *
 */

#if !defined(__LIBBREP_CURVETREE)
#define __LIBBREP_CURVETREE

#include "libbrep_util.h"

#include <vector>
#include <map>
#include <set>
#include <queue>

#ifndef NURBS_EXPORT
#  if defined(NURBS_DLL_EXPORTS) && defined(NURBS_DLL_IMPORTS)
#    error "Only NURBS_DLL_EXPORTS or NURBS_DLL_IMPORTS can be defined, not both."
#  elif defined(NURBS_DLL_EXPORTS)
#    define NURBS_EXPORT __declspec(dllexport)
#  elif defined(NURBS_DLL_IMPORTS)
#    define NURBS_EXPORT __declspec(dllimport)
#  else
#    define NURBS_EXPORT
#  endif
#endif

/* trim curve point sampling count for isLinear() check and possibly growing bounding box*/
#define BREP_BB_CRV_PNT_CNT 10
#define BREP_CURVE_FLATNESS 0.95

/* subdivision size factors */
#define BREP_TRIM_SUB_FACTOR 1

/// arbitrary calculation tolerance (need to try VDIVIDE_TOL or VUNITIZE_TOL to tighten the bounds)
#ifndef TOL
#  define TOL 0.000001
#endif
//
///// another arbitrary calculation tolerance (need to try VDIVIDE_TOL or VUNITIZE_TOL to tighten the bounds)
#ifndef TOL2
#  define TOL2 0.00001
#endif

/// another arbitrary calculation tolerance (used in getting a value of v from a curve given a u value)
#ifndef TOL3
#  define TOL3 0.0000001
#endif

////////////////////////////////////////////////////////////////
//
//   ON_CurveTree
//
// An ON_BrepFace has 1 or more trimming loops, which
// are in turn made up of trimming curves.
//
// There are actually two "tiers" to a curve tree - the loop level,
// which manages which loops need to be addressed in particular
// queries, and the trim level which actually uses tree structures
// to break curves into sufficiently small sub-segments.
//
// The position of each vector element in a "loop node" is as follows:
// index + 0  : holds the index of the next loop node.  Unlike
//              trim nodes, loop node element counts aren't fixed. The
//              final loop node will have value of zero in this slot.
// index + 1  : number of trim curves in this loop
// index + 2  : index into the uv_bbox_values vector identifying the umin value associated
//              with this node. The next three values in the array after umin will be vmin, umax
//              and vmax.
// index + 3 + n  : index of the nth trim root node associated with this loop (counting from index 0)

#define NON_TRIM_LOOP_STEP 3


// The position of each vector element in a "trim node" is as follows:
//
// index + 0  : holds the index of the parent node of this node. If this is a trim node under
//              a loop node, this index identifies the index of the parent loop node.
// index + 1  : index into the t_coords vector of doubles identifying tmin for this node.
// index + 2  : index into the t_coords vector of doubles identifying tmax for this node.
// index + 3  : index of left child node of this node.
// index + 4  : index of right child node of this node.
// index + 5  : information on whether or not this node has problematic points such as
//              knots or HV tangents - 1 means problem points are present, 0 is clean,
//              2 means clean but curve segment is part of outer loop.
// index + 6  : index into the uv_bbox_values vector identifying the umin value associated
//              with this node. The next three values in the array after umin will be vmin, umax
//              and vmax.  Building these values up from the leaf nodes ensures proper bounding.
// index + 7  : index of associated curve in BRep's m_C2 array

#define TRIM_NODE_STEP 8

class ON_CLASS ON_CurveTree
{
    public:

	ON_CurveTree(const ON_BrepFace*);
	~ON_CurveTree();

	unsigned int Depth(long int);  // Depth of a given node in the tree.

        // Set a bounding box to the overall UV bounding box of the curve tree
        void BoundingBox(ON_BoundingBox *);

        bool CurveTrim(double u, double v, long int node);

	// One of the most important queries that can be made of a Curve tree is whether
	// a given point is trimmed or untrimmed by the loops of the tree.  This test
	// uses a modified PNPOLY test.  The two modifications are 1) a polygon is dynamically formed
	// for PNPOLY using the corsest polygon that can be constructed - once a point is
	// outside a bounding box for a curve segment, the linear approximation for that
	// curve segment is sufficient for the test - and 2), when a point is too close
	// to a curve segment to be sufficient for the ray intersection test, use a test
	// that actually queries the curve.
	bool IsTrimmed(
		double u,
		double v,
		std::vector<long int> *subset = NULL,
		bool pnpoly_state = false
		);

	// We also need to be able to tell whether a UV rectangular region is fully trimmed,
	// fully present, or partially trimmed.  The patch may want to pass in a subset of
	// the curves it is interested in, get back a subset of the curves that overlap its
	// domain, or both.
	int IsTrimmed(
		ON_Interval *u_dom,
		ON_Interval *v_dom,
		std::vector<long int> *included = NULL,
		std::vector<long int> *subset = NULL,
		bool pnpoly_state = false
		);

        // We store the face pointer so we have access to face info at
        // need - for example, the outer loop pointer is face->OuterLoop()
        // and the outer loop index is face->OuterLoop()->m_loop_index
        const ON_BrepFace *ctree_face;

    private:
        long int first_trim_node;
        long int New_Trim_Node(int parent, int curve_index, int tmin, int tmax);
        void Subdivide_Trim_Node(long int node_id, const ON_BrepTrim *);

        std::vector<long int> nodes;
        std::vector<double> t_coords;
        std::vector<double> uv_bbox_values;

        ON_BoundingBox ctree_bbox;
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
