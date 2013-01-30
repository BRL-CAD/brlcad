/*            L I B N U R B S _ C U R V E T R E E . H
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
/** @file libnurbs_curvetree.h
 *
 * Brief description
 *
 */

#if !defined(__LIBNURBS_CURVETREE)
#define __LIBNURBS_CURVETREE

#include "opennurbs.h"

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

typedef std::pair<std::pair<int, int>, std::pair<double, double> > local_node_t;

// Return 0 if there are no tangent points in the interval, 1
// if there is a horizontal tangent, two if there is a vertical
// tangent, 3 if a normal split is in order.
extern NURBS_EXPORT int ON_Curve_Has_Tangent(const ON_Curve* curve, double min, double max);

// Once we reach the curve level, build a real tree - the basic approach is
// to seed a queue with the root node, and then subdivide based on appropriate
// test criteria until valid leaf nodes are obtained
extern NURBS_EXPORT void ON_Curve_KDTree(
	const ON_Curve* curve, 
	const ON_Surface *srf, 
	std::map<std::pair<int, int>, 
	std::pair<double, double> > *local_kdtree);

////////////////////////////////////////////////////////////////
//
//   ON_CurveTree
// 
// An ON_BrepFace is made up of 1 or more trimming loops, which
// are in turn made up of trimming curves.  The trimming curves
// in turn must be subdivided until three criteria are satisfied:
//
// 1.  All knots are division points
// 2.  All points where the curve tangent slop in UV is either zero
//     or infinity are division points
// 3.  Either the local curve segment is near-linear or the depth limit
//     has been reached

class ON_CLASS ON_CurveTree
{
    public:

	ON_CurveTree(const ON_BrepFace*);
	~ON_CurveTree();

    private:
	int outer_loop;
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
