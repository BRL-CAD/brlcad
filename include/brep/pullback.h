/*                        P U L L B A C K . H
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

/** @addtogroup brep_pullback
 *
 * @brief
 * point pullback.
 *
 */

#ifndef BREP_PULLBACK_H
#define BREP_PULLBACK_H

#include "common.h"
#include "brep/defines.h"
#include "brep/surfacetree.h"

/** @{ */
/** @file brep/pullback.h */

#ifdef __cplusplus

__BEGIN_DECLS

extern "C++" {

typedef struct pbc_data {
    double tolerance;
    double flatness;
    const ON_Curve *curve;
    const ON_Surface *surf;
    brlcad::SurfaceTree *surftree;
    std::list<ON_2dPointArray *> *segments;
    const ON_BrepEdge *edge;
    bool order_reversed;
} PBCData;

extern BREP_EXPORT int IsAtSingularity(const ON_Surface *surf, double u, double v, double tol = 1e-6);
extern BREP_EXPORT int IsAtSingularity(const ON_Surface *surf, const ON_2dPoint &pt, double tol = 1e-6);
extern BREP_EXPORT int IsAtSeam(const ON_Surface *surf,int dir,double u, double v,double tol = 0.0);
extern BREP_EXPORT int IsAtSeam(const ON_Surface *surf,double u, double v,double tol = 0.0);
extern BREP_EXPORT int IsAtSeam(const ON_Surface *surf,int dir,const ON_2dPoint &pt,double tol = 0.0);
extern BREP_EXPORT int IsAtSeam(const ON_Surface *surf,const ON_2dPoint &pt,double tol = 0.0);
extern BREP_EXPORT ON_2dPoint UnwrapUVPoint(const ON_Surface *surf,const ON_2dPoint &pt,double tol = 0.0);
extern BREP_EXPORT double DistToNearestClosedSeam(const ON_Surface *surf,const ON_2dPoint &pt);
extern BREP_EXPORT void SwapUVSeamPoint(const ON_Surface *surf,ON_2dPoint &p, int hint = 3);
extern BREP_EXPORT void ForceToClosestSeam(const ON_Surface *surf,ON_2dPoint &pt,double tol= 0.0);
extern BREP_EXPORT bool Find3DCurveSeamCrossing(PBCData &data,double t0,double t1,double offset,double &seam_t,ON_2dPoint &from,ON_2dPoint &to,double tol = 0.0, double same_point_tol=BREP_SAME_POINT_TOLERANCE,double within_distance_tol = BREP_EDGE_MISS_TOLERANCE);
extern BREP_EXPORT bool FindTrimSeamCrossing(const ON_BrepTrim &trim,double t0,double t1,double &seam_t,ON_2dPoint &from,ON_2dPoint &to,double tol = 0.0);
extern BREP_EXPORT bool surface_GetClosestPoint3dFirstOrder(const ON_Surface *surf,const ON_3dPoint& p,ON_2dPoint& p2d,ON_3dPoint& p3d,double &current_distance,int quadrant = 0,double same_point_tol=BREP_SAME_POINT_TOLERANCE,double within_distance_tol=BREP_EDGE_MISS_TOLERANCE);
extern BREP_EXPORT bool trim_GetClosestPoint3dFirstOrder(const ON_BrepTrim& trim,const ON_3dPoint& p,ON_2dPoint& p2d,double& t,double& distance,const ON_Interval* interval,double same_point_tol=BREP_SAME_POINT_TOLERANCE,double within_distance_tol=BREP_EDGE_MISS_TOLERANCE);
extern BREP_EXPORT bool ConsecutivePointsCrossClosedSeam(const ON_Surface *surf,const ON_2dPoint &pt,const ON_2dPoint &prev_pt, int &udir, int &vdir,double tol = 1e-6);

extern BREP_EXPORT PBCData *pullback_samples(const ON_Surface *surf,const ON_Curve *curve,double tolerance = 1.0e-6,double flatness = 1.0e-3,double same_point_tol=BREP_SAME_POINT_TOLERANCE,double within_distance_tol=BREP_EDGE_MISS_TOLERANCE);

extern BREP_EXPORT bool check_pullback_data(std::list<PBCData *> &pbcs);

extern BREP_EXPORT int
check_pullback_singularity_bridge(const ON_Surface *surf, const ON_2dPoint &p1, const ON_2dPoint &p2);

namespace brlcad {

    /**
     * approach:
     *
     * - get an estimate using the surface tree (if non-null, create
     * one otherwise)
     *
     * - find a point (u, v) for which S(u, v) is closest to _point_
     *                                                     _      __
     *   -- minimize the distance function: D(u, v) = sqrt(|S(u, v)-pt|^2)
     *                                       _      __
     *   -- simplify by minimizing f(u, v) = |S(u, v)-pt|^2
     *
     *   -- minimum occurs when the gradient is zero, i.e.
     *     \f[ \nabla f(u, v) = |\vec{S}(u, v)-\vec{p}|^2 = 0 \f]
     */
    BREP_EXPORT bool get_closest_point(ON_2dPoint& outpt,
				       const ON_BrepFace& face,
				       const ON_3dPoint& point,
				       const SurfaceTree* tree = NULL,
				       double tolerance = BREP_FCP_ROOT_EPSILON);

    /**
     * Pull an arbitrary model-space *curve* onto the given *surface* as a
     * curve within the surface's domain when, for each point c = C(t) on
     * the curve and the closest point s = S(u, v) on the surface, we
     * have: distance(c, s) <= *tolerance*.
     *
     * The resulting 2-dimensional curve will be approximated using the
     * following process:
     *
     * 1. Adaptively sample the 3d curve in the domain of the surface
     *    (ensure tolerance constraint). Sampling terminates when the
     *    following flatness criterion is met:
     *
     * given two parameters on the curve t1 and t2 (which map to points p1
     * and p2 on the curve) let m be a parameter randomly chosen near the
     * middle of the interval [t1, t2] ____ then the curve between t1 and
     * t2 is flat if distance(C(m), p1p2) < flatness
     *
     * 2. Use the sampled points to perform a global interpolation using
     *    universal knot generation to build a B-Spline curve.
     *
     * 3. If the curve is a line or an arc (determined with openNURBS
     *    routines), return the appropriate ON_Curve subclass (otherwise,
     *    return an ON_NurbsCurve).
     */
    extern ON_Curve *pullback_curve(ON_BrepFace *face,
				    const ON_Curve *curve,
				    SurfaceTree *tree = NULL,
				    double tolerance = BREP_FCP_ROOT_EPSILON,
				    double flatness = 1.0e-3);
} /* end namespace brlcad */


bool
ON_NurbsCurve_GetClosestPoint(
	double *t,
	ON_3dPoint *cp,
	const ON_NurbsCurve *nc,
	const ON_3dPoint &p,
	double maximum_distance = 0.0,
	const ON_Interval *sub_domain = NULL
	);


bool
ON_TrimCurve_GetClosestPoint(
	double *t,
	const ON_BrepTrim *trim,
	const ON_3dPoint &p,
	double maximum_distance = 0.0,
	const ON_Interval *sub_domain = NULL
	);

extern BREP_EXPORT bool
ON_NurbsCurve_ClosestPointToLineSegment(
	double *dist,
	double *t,
	const ON_NurbsCurve *nc,
	const ON_Line &l,
	double maximum_distance = 0.0,
	const ON_Interval *subdomain = NULL
	);

} /* extern C++ */

__END_DECLS

#endif

/** @} */

#endif  /* BREP_PULLBACK_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
