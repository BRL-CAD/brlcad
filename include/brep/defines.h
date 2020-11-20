/*                      D E F I N E S . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2020 United States Government as represented by
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

/** @addtogroup brep_defines
 *
 * @brief
 * These are definitions specific to libbrep, used throughout the library.
 *
 */
#ifndef BREP_DEFINES_H
#define BREP_DEFINES_H

#include "common.h"

/* We need a guarded windows.h inclusion, so use bio.h to get it before
 * opennurbs.h pulls it in */
#include "bio.h"

#ifdef __cplusplus

/* Note - We aren't (yet) including opennurbs in our Doxygen output. Until we
 * do, use cond to hide the opennurbs header from Doxygen. */
/* @cond */
extern "C++" {


#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#endif
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif

/* don't let opennurbs include windows.h */
#define ON_NO_WINDOWS 1

#include "opennurbs.h"

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

}
/* @endcond */

#endif

#include "vmath.h"

/** @{ */
/** @file brep/defines.h */

#ifndef BREP_EXPORT
#  if defined(BREP_DLL_EXPORTS) && defined(BREP_DLL_IMPORTS)
#    error "Only BREP_DLL_EXPORTS or BREP_DLL_IMPORTS can be defined, not both."
#  elif defined(BREP_DLL_EXPORTS)
#    define BREP_EXPORT COMPILER_DLLEXPORT
#  elif defined(BREP_DLL_IMPORTS)
#    define BREP_EXPORT COMPILER_DLLIMPORT
#  else
#    define BREP_EXPORT
#  endif
#endif

#ifndef __cplusplus
/**
 * @brief Placeholder for ON_Brep to allow brep.h to compile when we're
 * compiling with a C compiler
 */
typedef struct _on_brep_placeholder {
    int dummy; /* MS Visual C hack which can be removed if the struct contains something meaningful */
} ON_Brep;
#endif

/** Maximum number of newton iterations on root finding */
#define BREP_MAX_ITERATIONS 100

/** Root finding threshold */
#define BREP_INTERSECTION_ROOT_EPSILON 1e-6

/* if threshold not reached what will we settle for close enough */
#define BREP_INTERSECTION_ROOT_SETTLE 1e-2

/** Jungle Gym epsilon */

/** tighten BREP grazing tolerance to 0.000017453(0.001 degrees) was using RT_DOT_TOL at 0.001 (0.05 degrees) **/
#define BREP_GRAZING_DOT_TOL 0.000017453

/** Use vector operations? For debugging */
#define DO_VECTOR 1

/** Maximum per-surface BVH depth */
#define BREP_MAX_FT_DEPTH 8
#define BREP_MAX_LN_DEPTH 20

#define SIGN(x) ((x) >= 0 ? 1 : -1)

/** Surface flatness parameter, Abert says between 0.8-0.9 */
#define BREP_SURFACE_FLATNESS 0.85
#define BREP_SURFACE_STRAIGHTNESS 0.75

/** Max newton iterations when finding closest point */
#define BREP_MAX_FCP_ITERATIONS 50

/** Root finding epsilon */
#define BREP_FCP_ROOT_EPSILON 1e-5

/** trim curve point sampling count for isLinear() check and possibly
 *  * growing bounding box
 *   */
#define BREP_BB_CRV_PNT_CNT 10

#define BREP_CURVE_FLATNESS 0.95

/** subdivision size factors */
#define BREP_SURF_SUB_FACTOR 1
#define BREP_TRIM_SUB_FACTOR 1

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

/* arbitrary calculation tolerance */
#define BREP_UV_DIST_FUZZ 0.000001

/* @todo: debugging crapola (clean up later) */
#define ON_PRINT4(p) "[" << (p)[0] << ", " << (p)[1] << ", " << (p)[2] << ", " << (p)[3] << "]"
#define ON_PRINT3(p) "(" << (p)[0] << ", " << (p)[1] << ", " << (p)[2] << ")"
#define ON_PRINT2(p) "(" << (p)[0] << ", " << (p)[1] << ")"
#define PT(p) ON_PRINT3(p)
#define PT2(p) ON_PRINT2(p)
#define IVAL(_ival) "[" << (_ival).m_t[0] << ", " << (_ival).m_t[1] << "]"
#define TRACE(s)
#define TRACE1(s)
#define TRACE2(s)
/* #define TRACE(s) std::cerr << s << std::endl; */
/* #define TRACE1(s) std::cerr << s << std::endl; */
/* #define TRACE2(s) std::cerr << s << std::endl; */

#ifdef __cplusplus
extern "C++" {
struct BrepTrimPoint
{
    int edge_ind;
    double e;     /* corresponding edge curve parameter (ON_UNSET_VALUE if using trim not edge) */
    ON_3dPoint *p3d; /* 3d edge/trim point depending on whether we're using the 3d edge to generate points or the trims */
    ON_3dPoint *n3d; /* normal on edge, average of the normals from the two surfaces at this point, or of all surface points associated with a vertex if this is a vertex point. */
    ON_3dVector tangent; /* Tangent from the curve, or from the surfaces if the curve wasn't usable at this point. */

    int trim_ind;
    double t;     /* corresponding trim curve parameter (ON_UNSET_VALUE if unknown or not pulled back) */
    ON_2dPoint p2d; /* 2d surface parameter space point */
    ON_3dVector normal; /* normal as calculated by this trim */

    BrepTrimPoint *other_face_trim_pnt;
    int from_singular;
};}
#endif



/** @} */

#endif  /* BREP_DEFINES_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
