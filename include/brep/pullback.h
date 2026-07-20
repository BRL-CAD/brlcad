/*                        P U L L B A C K . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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

#  include <memory>

__BEGIN_DECLS

extern "C++" {

namespace brlcad {
class PullbackContext;
}

enum class PullbackFailureReason {
    None,
    Cancelled,
    ProjectionFailed,
    SurfaceDistanceExceeded
};

typedef struct pbc_data {
    double tolerance;
    double flatness;
    const ON_Curve *curve;
    const ON_Surface *surf;
    brlcad::SurfaceTree *surftree;
    std::list<ON_2dPointArray *> *segments;
    const ON_BrepEdge *edge;
    bool order_reversed;
    /** Job-local closest-point cache.  It never owns STEP or geometry data. */
    std::shared_ptr<brlcad::PullbackContext> context;
    /** Typed result from the bounded point-projection stage. */
    PullbackFailureReason failure_reason = PullbackFailureReason::None;
    size_t projection_samples = 0;
    size_t rejected_projection_samples = 0;
    /** Rejected samples for which the closest-point solver found no finite
     * candidate.  These are solver failures, not evidence that the source
     * curve lies outside its declared surface tolerance. */
    size_t failed_projection_samples = 0;
    /** Closed-curve seam transitions for which the local root finder did not
     * locate an exact split.  Callers may defer these to loop-level seam
     * resolution, but must report the aggregate rather than printing inside
     * this low-level numerical routine. */
    size_t failed_seam_crossing_searches = 0;
    double maximum_projection_distance = 0.0;
    bool tolerance_adjusted = false;
    double declared_tolerance = 0.0;
    /** Loop-level proof selected this edge boundary as the unique periodic
     * cut to a surface pole.  Endpoint closure must preserve this pair until
     * the exact seam-edge and singular trims are emitted. */
    bool periodic_pole_cut_before = false;
    bool periodic_pole_cut_after = false;
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

    /** Install thread-local cancellation and elapsed-work limits for the
     * current geometry job.  A zero elapsed limit disables the deadline. */
    typedef bool (*PullbackCancellationCallback)(void *context);
    extern BREP_EXPORT void SetPullbackWorkLimit(
        PullbackCancellationCallback cancellation_callback,
        void *cancellation_context,
        uint64_t maximum_elapsed_milliseconds);
    extern BREP_EXPORT void ClearPullbackWorkLimit();
    extern BREP_EXPORT bool PullbackWorkCancelled();
    extern BREP_EXPORT bool PullbackWorkDeadlineExpired();
    /** Milliseconds remaining on the calling thread's elapsed-work budget.
     * UINT64_MAX means no deadline is installed.  This permits a bounded
     * parent conversion job to propagate its original deadline to helper
     * threads without restarting the per-item budget. */
    extern BREP_EXPORT uint64_t PullbackWorkRemainingMilliseconds();

    /**
     * Mutable state used by one pullback job.  Keeping span and bounding-box
     * caches here makes independent conversion jobs safe to run concurrently
     * and bounds the cache lifetime to the job that owns it.
     */
    class BREP_EXPORT PullbackContext {
    public:
        PullbackContext();
        ~PullbackContext();

        PullbackContext(const PullbackContext &) = delete;
        PullbackContext &operator=(const PullbackContext &) = delete;

        bool SurfaceClosestPoint(const ON_Surface *surf,
                                 const ON_3dPoint& point,
                                 ON_2dPoint& surface_point,
                                 ON_3dPoint& lifted_point,
                                 double& distance,
                                 int quadrant = 0,
                                 double same_point_tol = BREP_SAME_POINT_TOLERANCE,
                                 double within_distance_tol = BREP_EDGE_MISS_TOLERANCE);

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };

    extern BREP_EXPORT bool surface_GetClosestPoint3dFirstOrder(
        PullbackContext &context,
        const ON_Surface *surf,
        const ON_3dPoint& point,
        ON_2dPoint& surface_point,
        ON_3dPoint& lifted_point,
        double& distance,
        int quadrant = 0,
        double same_point_tol = BREP_SAME_POINT_TOLERANCE,
        double within_distance_tol = BREP_EDGE_MISS_TOLERANCE);

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


extern BREP_EXPORT bool
ON_NurbsCurve_GetClosestPoint(
	double *t,
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
