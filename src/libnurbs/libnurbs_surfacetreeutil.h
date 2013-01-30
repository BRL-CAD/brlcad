/*      L I B N U R B S _ S U R F A C E T R E E U T I L . H
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
/** @file libnurbs_surfacetreeutil.h
 *
 * Brief description
 *
 */

#ifndef __LIBNURBS_SURFTREEUTILS
#define __LIBNURBS_SURFTREEUTILS

#include <vector>

#include "opennurbs.h"
#include "libnurbs_curvetree.h"

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

// Determines the trimming status of a given patch
extern NURBS_EXPORT int ON_Surface_Patch_Trimmed(
	const ON_Surface *srf, 
	ON_CurveTree *c_tree, 
	ON_Interval *u_val, 
	ON_Interval *v_val); 

// Test a frame set from a surface patch for flatness and straightness
extern NURBS_EXPORT int ON_Surface_Flat_Straight(
	ON_SimpleArray<ON_Plane> *frames, 
	std::vector<int> *frame_index, 
	double flatness_threshold, 
	double straightness_threshold);
// Directional flatness functions
extern NURBS_EXPORT int ON_Surface_Flat_U(
	ON_SimpleArray<ON_Plane> *frames, 
	std::vector<int> *frame_index, 
	double flatness_threshold);
extern NURBS_EXPORT int ON_Surface_Flat_V(
	ON_SimpleArray<ON_Plane> *frames, 
	std::vector<int> *frame_index, 
	double flatness_threshold);

// Check flattened width/height ratio
extern NURBS_EXPORT int ON_Surface_Width_vs_Height(const ON_Surface *srf, double ratio, int *u_only, int *v_only);

// Check an individual domain and knot array for a split knot
extern NURBS_EXPORT int ON_Surface_Find_Split_Knot(ON_Interval *val, int kcnt, double *knots, double *mid); 


// Figure out whether knot splitting is needed, and if so prepare values
extern NURBS_EXPORT int ON_Surface_Knots_Split(
	ON_Interval *u_val, 
	ON_Interval *v_val, 
	int u_kcnt, 
	int v_kcnt, 
	double *u_knots, 
	double *v_knots, 
	double *u_mid, 
	double *v_mid);


// Prepare frames according to the standard node layout
extern NURBS_EXPORT int ON_Populate_Frame_Array(
	const ON_Surface *srf, 
	ON_Interval *u_domain, 
	ON_Interval *v_domain, 
	double umid, 
	double vmid, 
	ON_SimpleArray<ON_Plane> *frames, 
	std::vector<int> *frame_index);

// Extend frames according to the standard node layout for splitting
extern NURBS_EXPORT int ON_Extend_Frame_Array(
	const ON_Surface *srf, 
	ON_Interval *u_domain, 
	ON_Interval *v_domain, 
	double umid, 
	double vmid, 
	ON_SimpleArray<ON_Plane> *frames, 
	std::vector<int> *frame_index);


// For any pre-existing surface passed as one of the t* args, this is a no-op
extern NURBS_EXPORT void ON_Surface_Create_Scratch_Surfaces(
	ON_Surface **t1,
	ON_Surface **t2,
	ON_Surface **t3,
	ON_Surface **t4);


// Given a surface and UV intervals, return a NURBS surface corresponding to
// that subset of the patch.  If t1-t3 and result are supplied externally,
// they are re-used - if not, local versions will be created and destroyed.
// The latter usage will have a malloc overhead penalty.  It is always the
// responsibility of the caller to delete the result surface.  If temporary
// surfaces are passed in, deleting those is also the responsibility of the
// caller.
extern NURBS_EXPORT bool ON_Surface_SubSurface(
	const ON_Surface *srf, 
	ON_Surface **result,
	ON_Interval *u_val,
	ON_Interval *v_val,
	ON_Surface **t1,
	ON_Surface **t2,
	ON_Surface **t3,
	ON_Surface **t4);


#endif

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
