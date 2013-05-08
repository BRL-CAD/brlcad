/*            L I B B R E P _ B R E P _ T O O L S . H
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
/** @addtogroup libbrep */
/** @{ */
/** @file libbrep_brep_tools.h
 *
 *  Utility routines for working with geometry.
 */

#ifndef __LIBBREP_BREP_TOOLS
#define __LIBBREP_BREP_TOOLS

#include <vector>

#include "opennurbs.h"

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
NURBS_EXPORT 
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

#endif /* __LIBBREP_BREP_TOOLS */
/** @} */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
