/*                        D E F I N E S . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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

/*----------------------------------------------------------------------*/
/** @addtogroup bg_defines
 * @brief
 * Common definitions for the headers used in bg.h (i.e. the headers found in include/bg)
 */
/** @{ */
/** @file bg/defines.h */

#ifndef BG_DEFINES_H
#define BG_DEFINES_H

#include "common.h"

#ifndef BG_EXPORT
#  if defined(BG_DLL_EXPORTS) && defined(BG_DLL_IMPORTS)
#    error "Only BG_DLL_EXPORTS or BG_DLL_IMPORTS can be defined, not both."
#  elif defined(BG_DLL_EXPORTS)
#    define BG_EXPORT COMPILER_DLLEXPORT
#  elif defined(BG_DLL_IMPORTS)
#    define BG_EXPORT COMPILER_DLLIMPORT
#  else
#    define BG_EXPORT
#  endif
#endif

/* Definitions for clockwise and counter-clockwise loop directions */
#define BG_CW 1
#define BG_CCW -1

/**
 * Tessellation (geometric) tolerances, different beasts than the
 * calculation tolerance in bn_tol.

 * norm - angle between adjacent sampling normals may be no greater than norm degrees
 * absmax - triangle edge must be no larger than length abs
 * absmin - triangle edge may be no smaller than length absmin
 * relmax - triangle edge must be no larger than rel % of object bbox diagonal len
 * relmin - triangle edge may be no smaller than relmin % of object bbox diagonal len.
 * relfacemax - triangle edge must be no larger than rel % of the smaller of
 *   the two bbox diagonal lengths of the faces associated with the edge (edge
 *   curves) or the bbox diagonal length of the current face (surfaces)
 * relfacemin - triangle edge may be no smaller than rel % of the smaller of
 *   the two bbox diagonal lengths of the faces associated with the edge (edge
 *   curves) or the bbox diagonal length of the current face (surfaces)
 *
 * For the min parameters, they will be specified with the caveot that any surface
 * will have at least one midpt breakdown (more for closed surfaces), even if that
 * produces smaller triangles than mins specifies.  I.e. the min lengths are a
 * goal, but not an absolute guarantee.  Triangle aspect ratio constraints will
 * also override these parameters.
 *
 * With the above caveot, the first limit hit of any set limits will hault refinement.
 * Need to track go/no-go refinement decisions so we can report on what the important
 * parameters are for a given operation - this will give users some idea of how to
 * modify settings to change behavior.
 *
 * For the max parameters, we may need to warn on objects where they would produce
 * a huge number of triangles if tiny maxes are specified - this is generally not
 * user intent and will not work in extreme cases due to resource exhaustion.
 *
 * All triangles must satisfy an aspect ratio parameter (smallest height may be no less than
 * .1x the longest edge length) to prevent wildly distorted triangles.)  For triangles near
 * edges, surface points may be removed if necessary to satisfy this criteria.
 *
 * All operations must satisfy above criteria first.  Additional constraints are
 * defined below.  These are intended, among other things, to prevent aspect ratio
 * problems when extremely long edge segments and fine surface meshes combine to
 * produce extremely distorted triangles near edges):
 *
 * For non-linear edges, max edge seg len may be no greater than 5x the smallest
 * avg line segment length of any non-linear edge breakdowns in either loop
 * associated with that edge.  This will require a two-pass operation - an initial
 * breakdown of all non-linear edge curves, and a refinement pass to break down
 * those segments which don't meet the above criteria.
 *
 * For linear edges, max edge seg len may be no greater than 10x the smallest
 * avg line segment length of any non-linear edge breakdowns in either
 * loop associated with that edge.  This will require a third edge pass, once
 * the above curved edge breakdown is complete.
 *
 * For any surface, max edge length may be no greater than .1 times the shorter
 * of the surface's length/width (we shrink surfaces so this number has
 * some relation to the trimmed face size).
 *
 * For a non-planar surface, max edge length may be no greater than 10 times the
 * shortest average line segment length in the face's loops' edges (curved or
 * linear). (e.g.  triangles in curved surfaces may not be enormously large
 * compared to the edges' fidelity.)  We don't impose this restriction on
 * planar surfaces on the theory that any valid CDT will accurate represent the
 * surface, so we don't need to enforce any particular edge length on interior
 * triangles.  However...
 *
 */
struct bg_tess_tol {
    uint32_t magic;
    double              abs;                    /**< @brief absolute dist tol */
    double              rel;                    /**< @brief rel dist tol */
    double              norm;                   /**< @brief normal tol */

    /* Parameters specialized for breps */
    double		absmax;
    double		absmin;
    double		relmax;
    double		relmin;
    double		rel_lmax; /* local relative maximum */
    double		rel_lmin; /* local relative minimum */
};
#define BG_CK_TESS_TOL(_p) BU_CKMAG(_p, BG_TESS_TOL_MAGIC, "bg_tess_tol")
#define BG_TESS_TOL_INIT_ZERO {BG_TESS_TOL_MAGIC, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}

/* A default tolerance to use when user supplied tolerances aren't available */
#define BG_TOL_INIT {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1.0e-6, 1.0 - 1.0e-6 }

#endif  /* BG_DEFINES_H */
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
