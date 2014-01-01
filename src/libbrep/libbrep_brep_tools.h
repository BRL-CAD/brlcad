/*            L I B B R E P _ B R E P _ T O O L S . H
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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
/** @addtogroup libbrep */
/** @{ */
/** @file libbrep_brep_tools.h
 *
 *  Utility routines for working with geometry.
 */

#ifndef LIBBREP_LIBBREP_BREP_TOOLS_H
#define LIBBREP_LIBBREP_BREP_TOOLS_H

#include "common.h"

#include <vector>

#include "opennurbs.h"
#include "brep.h"


/* Directions */
#ifndef NE
#  define NE 1
#endif
#ifndef NW
#  define NW 2
#endif
#ifndef SW
#  define SW 3
#endif
#ifndef SE
#  define SE 4
#endif


#ifndef BREP_EXPORT
#  if defined(BREP_DLL_EXPORTS) && defined(BREP_DLL_IMPORTS)
#    error "Only BREP_DLL_EXPORTS or BREP_DLL_IMPORTS can be defined, not both."
#  elif defined(BREP_DLL_EXPORTS)
#    define BREP_EXPORT __declspec(dllexport)
#  elif defined(BREP_DLL_IMPORTS)
#    define BREP_EXPORT __declspec(dllimport)
#  else
#    define BREP_EXPORT
#  endif
#endif

/**
  \brief Return truthfully whether a value is within a specified epsilon distance from zero.

  @param val value to be tested
  @param epsilon distance from zero defining the interval to be treated as "near" zero for the test

  @return @c true if the value is within the near-zero interval specified by epsilon, @c false otherwise.
*/
BREP_EXPORT
bool ON_NearZero(double val, double epsilon);

/**
  \brief Search for a horizontal tangent on the curve between two curve parameters.

  @param curve curve to be tested
  @param min minimum curve parameter value
  @param max maximum curve parameter value
  @param zero_tol tolerance to use when testing for near-zero values

  @return t parameter corresponding to the point on the curve with the horizontal tangent.
*/
BREP_EXPORT
double ON_Curve_Get_Horizontal_Tangent(const ON_Curve* curve, double min, double max, double zero_tol);

/**
  \brief Search for a vertical tangent on the curve between two curve parameters.

  @param curve curve to be tested
  @param min minimum curve parameter value
  @param max maximum curve parameter value
  @param zero_tol tolerance to use when testing for near-zero values

  @return t parameter corresponding to the point on the curve with the vertical tangent.
*/
BREP_EXPORT
double ON_Curve_Get_Vertical_Tangent(const ON_Curve* curve, double min, double max, double zero_tol);


/**
  \brief Test whether a curve interval contains one or more horizontal or vertical tangents

  @param curve ON_Curve to be tested
  @param ct_min minimum t parameter value of the curve interval to be tested
  @param ct_max maximum t parameter value of the curve interval to be tested
  @param t_tol tolerance used to decide when a curve is a line tangent to X or Y axis

  @return @c 0 if there are no tangent points in the interval, @c 1 if there is a single vertical tangent,
  @c 2 if there is a single horizontal tangent, and @c 3 if multiple tangents are present.
*/
BREP_EXPORT
int ON_Curve_Has_Tangent(const ON_Curve* curve, double ct_min, double ct_max, double t_tol);


/**
 * \verbatim
 *   3-------------------2
 *   |                   |
 *   |    6         8    |
 *   |                   |
 *  V|         4         |
 *   |                   |
 *   |    5         7    |
 *   |                   |
 *   0-------------------1
 *      U
 * \endverbatim
 */


/**
  \brief Perform flatness test of surface

  Determine whether a given surface is flat enough, i.e. it falls
  beneath our simple flatness constraints. The flatness constraint in
  this case is a sampling of normals across the surface such that the
  product of their combined dot products is close to 1.

  @f[ \prod_{i=1}^{7} n_i \dot n_{i+1} = 1 @f]

  This code is using a slightly different placement of the interior normal
  tests as compared to <a href="http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.90.7500&rep=rep1&type=pdf">Abert's</a> approach:

  \verbatim
     +-------------------+
     |                   |
     |    +         +    |
     |                   |
   V |         +         |
     |                   |
     |    +         +    |
     |                   |
     +-------------------+
	       U
  \endverbatim


  The "+" indicates the normal sample.

  The frenet frames are stored in the frames arrays according
  to the following index values:

  \verbatim
     3-------------------2
     |                   |
     |    6         8    |
     |                   |
   V |         4         |
     |                   |
     |    5         7    |
     |                   |
     0-------------------1
	       U
  \endverbatim

  @param frames Array of 9 frenet frames
  @param f_tol Flatness tolerance - 0 always evaluates to flat, 1 would be a perfectly flat surface. Generally something in the range 0.8-0.9 should suffice in raytracing subdivision (per <a href="http://www.uni-koblenz.de/~cg/Diplomarbeiten/DA_Oliver_Abert.pdf">Abert, 2005</a>)
*/
BREP_EXPORT
bool ON_Surface_IsFlat(ON_Plane *frames, double f_tol);

/**
  \brief Perform flatness test of surface in U only

  Array index conventions are the same as ::ON_Surface_IsFlat.

  @param frames Array of 9 frenet frames
  @param s_tol Straightness tolerance - 0 always evaluates to straight, 1 requires perfect straightness
*/
BREP_EXPORT
bool ON_Surface_IsFlat_U(ON_Plane *frames, double f_tol);


/**
  \brief Perform flatness test of surface in V only

  Array index conventions are the same as ::ON_Surface_IsFlat.

  @param frames Array of 9 frenet frames
  @param s_tol Straightness tolerance - 0 always evaluates to straight, 1 requires perfect straightness
*/
BREP_EXPORT
bool ON_Surface_IsFlat_V(ON_Plane *frames, double f_tol);


/**
  \brief Perform straightness test of surface

  The straightness test compares flatness criteria to running product of the tangent vector of
  the frenet frame projected onto each other tangent in the frame set.  Array index conventions
  are the same as ::ON_Surface_IsFlat.

  @param frames Array of 9 frenet frames
  @param s_tol Straightness tolerance - 0 always evaluates to straight, 1 requires perfect straightness
*/
BREP_EXPORT
bool ON_Surface_IsStraight(ON_Plane *frames, double s_tol);


/**
  \brief Create a surface based on a subset of a parent surface

  Create a NURBS surface that corresponds to a subset
  of an input surface, as defined by UV intervals. The
  t parameters may be NULL, in which case working surfaces
  will be created by the function.  If supplied, existing
  surfaces are reused to avoid extra malloc operations
  and memory usage associated with creating the working
  surfaces.

  @param srf parent ON_Surface
  @param u_val U interval of proposed subsurface
  @param v_val V interval of proposed subsurface
  @param t1 surface used during split algorithm
  @param t2 surface used during split algorithm
  @param t3 surface used during split algorithm
  @param t4 surface holding final result of split passes
  @param[out] result final subsurface - holds *t4 if it was non-NULL as an input, else holds a pointer to the new ON_Surface

  @return @c true if surface creation is successful or if the subsurface
  is the same as the parent surface, @c false if one or more split
  operations failed.
*/
BREP_EXPORT
bool ON_Surface_SubSurface(
	const ON_Surface *srf,
	ON_Interval *u_val,
	ON_Interval *v_val,
	ON_Surface **t1,
	ON_Surface **t2,
	ON_Surface **t3,
	ON_Surface **t4,
	ON_Surface **result
	);

/**
  \brief Create four sub-surfaces from a parent surface

  Create four NURBS surfaces that corresponds to subsets
  of an input surface, as defined by UV intervals and a
  point within the U and V intervals.

  \verbatim
     *---------------------*
     |          |          |
     |    q3    |    q2    |
     |          |          |
   V |----------+----------|
     |          |          |
     |    q0    |    q1    |
     |          |          |
     *---------------------*
	       U

  + is the point (upt, vpt) that defines the quads
  * points represent the mins and maxes of the U and V domains

  \endverbatim


  @param srf parent ON_Surface
  @param u U interval of parent surface
  @param v V interval of parent surface
  @param upt U interval point for quad definition
  @param upt U interval point for quad definition
  @param q0 surface calculated by split algorithm
  @param q1 surface calculated by split algorithm
  @param q2 surface calculated by split algorithm
  @param q3 surface calculated by split algorithm

  @return @c true if surfaces are successfully created, @c false if one or more split
  operations failed, the q* containers are not NULL, or the upt,vpt coordinates are
  not contained within the UV interval.
*/
BREP_EXPORT
bool ON_Surface_Quad_Split(
	const ON_Surface *srf,
	const ON_Interval& u,
	const ON_Interval& v,
	double upt,
	double vpt,
	ON_Surface **q0,
	ON_Surface **q1,
	ON_Surface **q2,
	ON_Surface **q3
	);


#endif /* LIBBREP_LIBBREP_BREP_TOOLS_H */
/** @} */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
