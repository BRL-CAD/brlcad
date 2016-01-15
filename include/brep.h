/*                       B R E P . H
 * BRL-CAD
 *
 * Copyright (c) 2007-2016 United States Government as represented by
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
/** @addtogroup libbrep
 * @brief
 * Define surface and curve structures for Non-Uniform Rational
 * B-Spline (NURBS) curves and surfaces. Uses openNURBS library.
 */
/** @{ */
/** @file include/brep.h */
#ifndef BREP_H
#define BREP_H

#include "common.h"

#ifdef __cplusplus
extern "C++" {
#include <vector>
#include <list>
#include <iostream>
#include <queue>
#include <assert.h>

#include "bu/ptbl.h"
#include "bn/dvec.h"
#include <iostream>
#include <fstream>

}
__BEGIN_DECLS
#endif

#include "vmath.h"
#include "brep/defines.h"
#include "brep/util.h"
#include "brep/ray.h"
#include "brep/brnode.h"
#include "brep/curvetree.h"
#include "brep/bbnode.h"
#include "brep/surfacetree.h"

#ifdef __cplusplus

__END_DECLS

extern "C++" {

BREP_EXPORT void set_key(struct bu_vls *key, int k, int *karray);
BREP_EXPORT void brep_get_plane_ray(ON_Ray &r, plane_ray &pr);
BREP_EXPORT void brep_r(const ON_Surface *surf, plane_ray &pr, pt2d_t uv, ON_3dPoint &pt, ON_3dVector &su, ON_3dVector &sv, pt2d_t R);
BREP_EXPORT void brep_newton_iterate(plane_ray &pr, pt2d_t R, ON_3dVector &su, ON_3dVector &sv, pt2d_t uv, pt2d_t out_uv);
BREP_EXPORT void utah_ray_planes(const ON_Ray &r, ON_3dVector &p1, double &p1d, ON_3dVector &p2, double &p2d);

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
BREP_EXPORT bool get_closest_point(ON_2dPoint &outpt,
				   ON_BrepFace *face,
				   const ON_3dPoint &point,
				   SurfaceTree *tree = NULL,
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

struct BrepTrimPoint
{
    ON_2dPoint p2d; /* 2d surface parameter space point */
    ON_3dPoint *p3d; /* 3d edge/trim point depending on whether we're using the 3d edge to generate points or the trims */
    double t;     /* corresponding trim curve parameter (ON_UNSET_VALUE if unknown or not pulled back) */
    double e;     /* corresponding edge curve parameter (ON_UNSET_VALUE if using trim not edge) */
};

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
extern BREP_EXPORT ON_BOOL32 face_GetBoundingBox(const ON_BrepFace &face,ON_BoundingBox& bbox,ON_BOOL32 bGrowBox);
extern BREP_EXPORT ON_BOOL32 surface_GetBoundingBox(const ON_Surface *surf,const ON_Interval &u_interval,const ON_Interval &v_interval,ON_BoundingBox& bbox,ON_BOOL32 bGrowBox);
extern BREP_EXPORT ON_BOOL32 surface_EvNormal(const ON_Surface *surf,double s,double t,ON_3dPoint& point,ON_3dVector& normal,int side=0,int* hint=0);

extern BREP_EXPORT PBCData *pullback_samples(const ON_Surface *surf,const ON_Curve *curve,double tolerance = 1.0e-6,double flatness = 1.0e-3,double same_point_tol=BREP_SAME_POINT_TOLERANCE,double within_distance_tol=BREP_EDGE_MISS_TOLERANCE);

extern BREP_EXPORT bool check_pullback_data(std::list<PBCData *> &pbcs);

extern BREP_EXPORT ON_Curve *interpolateCurve(ON_2dPointArray &samples);

extern BREP_EXPORT int
check_pullback_singularity_bridge(const ON_Surface *surf, const ON_2dPoint &p1, const ON_2dPoint &p2);

extern BREP_EXPORT ON_NurbsCurve *
interpolateLocalCubicCurve(const ON_3dPointArray &Q);

/**
 * Dump the information of an ON_SSX_EVENT.
 */
extern BREP_EXPORT void
DumpSSXEvent(ON_SSX_EVENT &x, ON_TextLog &text_log);

/* Sub-division support for a curve.
 * It's similar to generating the bounding box tree, when the Split()
 * method is called, the curve is split into two parts, whose bounding
 * boxes become the children of the original curve's bbox.
 */
class Subcurve {
    friend class Subsurface;
private:
    ON_BoundingBox m_node;
public:
    ON_Curve *m_curve;
    ON_Interval m_t;
    Subcurve *m_children[2];
    ON_BOOL32 m_islinear;

    Subcurve();
    Subcurve(ON_Curve *curve);
    Subcurve(const Subcurve &_scurve);
    ~Subcurve();
    int Split();
    void GetBBox(ON_3dPoint &min, ON_3dPoint &max);
    void SetBBox(const ON_BoundingBox &bbox);
    bool IsPointIn(const ON_3dPoint &pt, double tolerance = 0.0);
    bool Intersect(const Subcurve &other, double tolerance = 0.0, ON_BoundingBox *intersection = NULL) const;
};

/* Sub-division support for a surface.
 * It's similar to generating the bounding box tree, when the Split()
 * method is called, the surface is split into two parts, whose bounding
 * boxes become the children of the original surface's bbox.
 */
class Subsurface {
private:
    ON_BoundingBox m_node;
public:
    ON_Surface *m_surf;
    ON_Interval m_u, m_v;
    Subsurface *m_children[4];
    ON_BOOL32 m_isplanar;

    Subsurface();
    Subsurface(ON_Surface *surf);
    Subsurface(const Subsurface &_ssurf);
    ~Subsurface();

    int Split();
    void GetBBox(ON_3dPoint &min, ON_3dPoint &max);
    void SetBBox(const ON_BoundingBox &bbox);
    bool IsPointIn(const ON_3dPoint &pt, double tolerance = 0.0);
    bool Intersect(const Subcurve &curve, double tolerance = 0.0, ON_BoundingBox *intersection = NULL) const;
    bool Intersect(const Subsurface &surf, double tolerance = 0.0, ON_BoundingBox *intersection = NULL) const;
};

/** The ON_PX_EVENT class is used to report point-point, point-curve
 * and point-surface intersection events.
 */
class BREP_EXPORT ON_PX_EVENT
{
public:
    /** Default construction sets everything to zero. */
    ON_PX_EVENT();

    /**
     * Compares point intersection events and sorts them in the
     * canonical order.
     *
     * @retval -1 this < other
     * @retval  0 this == other
     * @retval +1 this > other
     *
     * @remarks ON_PX_EVENT::Compare is used to sort intersection
     * events into canonical order.
     */
    static
    int Compare(const ON_PX_EVENT *a, const ON_PX_EVENT *b);

    /**
     * Check point intersection event values to make sure they are
     * valid.
     *
     * @param text_log [in] If not null and an error is found, then
     *     a description of the error is printed to text_log.
     * @param intersection_tolerance [in] 0.0 or value used in
     *     intersection calculation.
     * @param pointA [in] NULL or pointA passed to intersection
     *     calculation.
     * @param pointB [in] NULL or pointB passed to intersection
     *     calculation.
     * @param curveB [in] NULL or curveB passed to intersection
     *     calculation.
     * @param curveB_domain [in] NULL or curveB domain used in
     *     intersection calculation.
     * @param surfaceB [in] NULL or surfaceB passed to intersection
     *     calculation.
     * @param surfaceB_domain0 [in] NULL or surfaceB "u" domain used
     *     in intersection calculation.
     * @param surfaceB_domain1 [in] NULL or surfaceB "v" domain used
     *     in intersection calculation.
     *
     * @return True if event is valid.
     */
    bool IsValid(ON_TextLog *text_log,
    double intersection_tolerance,
    const class ON_3dPoint *pointA,
    const class ON_3dPoint *pointB,
    const class ON_Curve *curveB,
    const class ON_Interval *curveB_domain,
    const class ON_Surface *surfaceB,
    const class ON_Interval *surfaceB_domain0,
    const class ON_Interval *surfaceB_domain1) const;

    void Dump(ON_TextLog &text_log) const;

    enum TYPE {
	no_px_event =  0,
	ppx_point   =  1, /**< point-point intersection */
	pcx_point   =  2, /**< point-curve intersection */
	psx_point   =  3  /**< point-surface intersection */
    };

    TYPE m_type;

    ON_3dPoint m_A;	/**< Point A in 3D space */
    ON_3dPoint m_B;	/**< Point B in 3D space */

    ON_2dPoint m_b;	/**< Point B in 2D space for the curve/surface
		     * For a curve, m_b[1] == 0
		     * For a point, m_b[0] == m_b[1] == 0
		     */

    ON_3dPoint m_Mid;	/**< The mid-point of Point A and Point B */
    double m_radius;	/**< To trace the uncertainty area */
};

/**
 * An overload of ON_Intersect for point-point intersection.
 * Intersect pointA with pointB.
 *
 * @param pointA [in]
 * @param pointB [in]
 * @param x [out] Intersection events are appended to this array.
 * @param tolerance [in] If the input intersection_tolerance <= 0.0,
 *     then 0.001 is used.
 *
 * @return True for an intersection. False for no intersection.
 */
extern BREP_EXPORT bool
ON_Intersect(const ON_3dPoint &pointA,
	     const ON_3dPoint &pointB,
	     ON_ClassArray<ON_PX_EVENT> &x,
	     double tolerance = 0.0);

/**
 * An overload of ON_Intersect for point-curve intersection.
 * Intersect pointA with curveB.
 *
 * @param pointA [in] pointA
 * @param curveB [in] curveB
 * @param x [out] Intersection events are appended to this array.
 * @param tolerance [in] If the input intersection_tolerance <= 0.0,
 *     then 0.001 is used.
 * @param curveB_domain [in] optional restriction on curveB t domain
 * @param treeB [in] optional curve tree for curveB, to avoid re-computation
 *
 * @return True for an intersection. False for no intersection.
 */
extern BREP_EXPORT bool
ON_Intersect(const ON_3dPoint &pointA,
	     const ON_Curve &curveB,
	     ON_ClassArray<ON_PX_EVENT> &x,
	     double tolerance = 0.0,
	     const ON_Interval *curveB_domain = 0,
	     Subcurve *treeB = 0);

/**
 * An overload of ON_Intersect for point-surface intersection.
 * Intersect pointA with surfaceB.
 *
 * @param pointA [in]
 * @param surfaceB [in]
 * @param x [out] Intersection events are appended to this array.
 * @param tolerance [in] If the input intersection_tolerance <= 0.0,
 *     then 0.001 is used.
 * @param surfaceB_udomain [in] optional restriction on surfaceB u
 *     domain
 * @param surfaceB_vdomain [in] optional restriction on surfaceB v
 *     domain
 * @param treeB [in] optional surface tree for surfaceB, to avoid
 *     re-computation
 *
 * @return True for an intersection. False for no intersection.
 */
extern BREP_EXPORT bool
ON_Intersect(const ON_3dPoint &pointA,
	     const ON_Surface &surfaceB,
	     ON_ClassArray<ON_PX_EVENT> &x,
	     double tolerance = 0.0,
	     const ON_Interval *surfaceB_udomain = 0,
	     const ON_Interval *surfaceB_vdomain = 0,
	     Subsurface *treeB = 0);

/**
 * An overload of ON_Intersect for curve-curve intersection.
 * Intersect curveA with curveB.
 *
 * Parameters:
 * @param curveA [in]
 * @param curveB [in]
 * @param x [out] Intersection events are appended to this array.
 * @param intersection_tolerance [in]  If the distance from a point
 *     on curveA to curveB is <= intersection tolerance, then the
 *     point will be part of an intersection event. If the input
 *     intersection_tolerance <= 0.0, then 0.001 is used.
 * @param overlap_tolerance [in] If t1 and t2 are parameters of
 *     curveA's intersection events and the distance from curveA(t)
 *     to curveB is <= overlap_tolerance for every t1 <= t <= t2,
 *     then the event will be returned as an overlap event. If the
 *     input overlap_tolerance <= 0.0, then
 *     intersection_tolerance * 2.0 is used.
 * @param curveA_domain [in] optional restriction on curveA domain
 * @param curveB_domain [in] optional restriction on curveB domain
 * @param treeA [in] optional curve tree for curveA, to avoid re
 *     computation
 * @param treeB [in] optional curve tree for curveB, to avoid re
 *     computation
 *
 * @return Number of intersection events appended to x.
 */
extern BREP_EXPORT int
ON_Intersect(const ON_Curve *curveA,
	     const ON_Curve *curveB,
	     ON_SimpleArray<ON_X_EVENT> &x,
	     double intersection_tolerance = 0.0,
	     double overlap_tolerance = 0.0,
	     const ON_Interval *curveA_domain = 0,
	     const ON_Interval *curveB_domain = 0,
	     Subcurve *treeA = 0,
	     Subcurve *treeB = 0);

/**
 * An overload of ON_Intersect for curve-surface intersection.
 * Intersect curveA with surfaceB.
 *
 * @param curveA [in]
 * @param surfaceB [in]
 * @param x [out] Intersection events are appended to this array.
 * @param intersection_tolerance [in] If the distance from a
 *     point on curveA to the surface is <= intersection tolerance,
 *     then the point will be part of an intersection event, or
 *     there is an intersection event the point leads to. If the
 *     input intersection_tolerance <= 0.0, then 0.001 is used.
 * @param overlap_tolerance [in] If the input overlap_tolerance
 *     <= 0.0, then 2.0*intersection_tolerance is used.  Otherwise,
 *     overlap tolerance must be >= intersection_tolerance.  In all
 *     cases, the intersection calculation is performed with an
 *     overlap_tolerance that is >= intersection_tolerance.  If t1
 *     and t2 are curve parameters of intersection events and the
 *     distance from curve(t) to the surface is <=
 *     overlap_tolerance for every t1 <= t <= t2, then the event
 *     will be returned as an overlap event.
 * @param curveA_domain [in] optional restriction on curveA domain
 * @param surfaceB_udomain [in] optional restriction on surfaceB
 *     u domain
 * @param surfaceB_vdomain [in] optional restriction on surfaceB
 *     v domain
 * @param overlap2d [out] return the 2D overlap curves on surfaceB.
 *     overlap2d[i] is the curve for event x[i].
 * @param treeA [in] optional curve tree for curveA, to avoid
 *     re-computation
 * @param treeB [in] optional surface tree for surfaceB, to avoid
 *     re-computation
 *
 * @return Number of intersection events appended to x.
 */
extern BREP_EXPORT int
ON_Intersect(const ON_Curve *curveA,
	     const ON_Surface *surfaceB,
	     ON_SimpleArray<ON_X_EVENT> &x,
	     double intersection_tolerance = 0.0,
	     double overlap_tolerance = 0.0,
	     const ON_Interval *curveA_domain = 0,
	     const ON_Interval *surfaceB_udomain = 0,
	     const ON_Interval *surfaceB_vdomain = 0,
	     ON_CurveArray *overlap2d = 0,
	     Subcurve *treeA = 0,
	     Subsurface *treeB = 0);

/**
 * An overload of ON_Intersect for surface-surface intersection.
 * Intersect surfaceA with surfaceB.
 *
 * @param surfA [in]
 * @param surfB [in]
 * @param x [out] Intersection events are appended to this array.
 * @param intersection_tolerance [in] If the input
 *     intersection_tolerance <= 0.0, then 0.001 is used.
 * @param overlap_tolerance [in] If positive, then overlap_tolerance
 *     must be >= intersection_tolerance and is used to test for
 *     overlapping regions. If the input overlap_tolerance <= 0.0,
 *     then 2*intersection_tolerance is used.
 * @param fitting_tolerance [in] If fitting_tolerance is > 0 and
 *     >= intersection_tolerance, then the intersection curves are
 *     fit to this tolerance.  If input fitting_tolerance <= 0.0 or
 *     < intersection_tolerance, then intersection_tolerance is used.
 * @param surfaceA_udomain [in] optional restriction on surfaceA
 *     u domain
 * @param surfaceA_vdomain [in] optional restriction on surfaceA
 *     v domain
 * @param surfaceB_udomain [in] optional restriction on surfaceB
 *     u domain
 * @param surfaceB_vdomain [in] optional restriction on surfaceB
 *     v domain
 * @param treeA [in] optional surface tree for surfaceA, to avoid
 *     re-computation
 * @param treeB [in] optional surface tree for surfaceB, to avoid
 *     re-computation
 *
 * @return Number of intersection events appended to x.
 */
extern BREP_EXPORT int
ON_Intersect(const ON_Surface *surfA,
	     const ON_Surface *surfB,
	     ON_ClassArray<ON_SSX_EVENT> &x,
	     double intersection_tolerance = 0.0,
	     double overlap_tolerance = 0.0,
	     double fitting_tolerance = 0.0,
	     const ON_Interval *surfaceA_udomain = 0,
	     const ON_Interval *surfaceA_vdomain = 0,
	     const ON_Interval *surfaceB_udomain = 0,
	     const ON_Interval *surfaceB_vdomain = 0,
	     Subsurface *treeA = 0,
	     Subsurface *treeB = 0);

enum op_type {
    BOOLEAN_UNION = 0,
    BOOLEAN_INTERSECT = 1,
    BOOLEAN_DIFF = 2,
    BOOLEAN_XOR = 3
};

/**
 * Evaluate NURBS boolean operations.
 *
 * @param brepO [out]
 * @param brepA [in]
 * @param brepB [in]
 * @param operation [in]
 */
extern BREP_EXPORT int
ON_Boolean(ON_Brep *brepO, const ON_Brep *brepA, const ON_Brep *brepB, op_type operation);

/**
 * Get the curve segment between param a and param b
 *
 * @param in [in] the curve to split
 * @param a  [in] either a or b can be the larger one
 * @param b  [in] either a or b can be the larger one
 *
 * @return the result curve segment. NULL for error.
 */
extern BREP_EXPORT ON_Curve *
sub_curve(const ON_Curve *in, double a, double b);

/**
 * Get the sub-surface whose u in [a,b] or v in [a, b]
 *
 * @param in [in] the surface to split
 * @param dir [in] 0: u-split, 1: v-split
 * @param a [in] either a or b can be the larger one
 * @param b [in] either a or b can be the larger one
 *
 * @return the result sub-surface. NULL for error.
 */
extern BREP_EXPORT ON_Surface *
sub_surface(const ON_Surface *in, int dir, double a, double b);

extern BREP_EXPORT void
ON_MinMaxInit(ON_3dPoint *min, ON_3dPoint *max);

extern BREP_EXPORT int
ON_Curve_PolyLine_Approx(ON_Polyline *polyline, const ON_Curve *curve, double tol);


/* Experimental function to generate Tikz plotting information
 * from B-Rep objects.  This may or may not be something we
 * expose as a feature long term - probably should be a more
 * generic API that supports multiple formats... */
extern BREP_EXPORT int
ON_BrepTikz(ON_String &s, const ON_Brep *brep, const char *color, const char *prefix);

/* Shape recognition functions - HIGHLY EXPERIMENTAL,
 * DO NOT RELY ON - the odds are quite good that this whole
 * setup will be moving to libanalyze and it's public API
 * there will likely be quite different */

/* Structure for holding parameters corresponding
 * to a csg primitive.  Not all parameters will be
 * used for all primitives - the structure includes
 * enough data slots to describe any primitive that may
 * be matched by the shape recognition logic */
struct csg_object_params {
    struct subbrep_shoal_data *s;
    int csg_type;
    int negative;
    /* Unique id number */
    int csg_id;
    char bool_op; /* Boolean operator - u = union (default), - = subtraction, + = intersection */
    point_t origin;
    vect_t hv;
    fastf_t radius;
    fastf_t r2;
    fastf_t height;
    int arb_type;
    point_t p[8];
    size_t plane_cnt;
    plane_t *planes;
    /* An implicit plane, if present, may close a face on a parent solid */
    int have_implicit_plane;
    point_t implicit_plane_origin;
    vect_t implicit_plane_normal;
    /* bot */
    int csg_face_cnt;
    int csg_vert_cnt;
    int *csg_faces;
    point_t *csg_verts;
    /* information flags */
    int half_cyl;
};

/* Forward declarations */
struct subbrep_island_data;

/* Topological shoal */
struct subbrep_shoal_data {
    struct subbrep_island_data *i;
    int shoal_type;
    /* Unique id number */
    int shoal_id;
    struct csg_object_params *params;
    /* struct csg_obj_params */
    struct bu_ptbl *shoal_children;

    /* Working information */
    int *shoal_loops;
    int shoal_loops_cnt;
};

/* Topological island */
struct subbrep_island_data {

    /* Overall type of island - typically comb or brep, but may
     * be an actual type if the nucleus corresponds to a single
     * implicit primitive */
    int island_type;

    /* Unique id number */
    int island_id;

    /* Context information */
    const ON_Brep *brep;

    /* Shape representation data */
    ON_Brep *local_brep;
    char local_brep_bool_op; /* Boolean operator - u = union (default), - = subtraction, + = intersection */

    /* Nucleus */
    struct subbrep_shoal_data *nucleus;
    /* struct subbrep_shoal_data */
    struct bu_ptbl *island_children;

    /* For union objects, we list the subtractions it needs */
    struct bu_ptbl *subtractions;

    /* subbrep metadata */
    struct bu_vls *key;
    ON_BoundingBox *bbox;

    /* Working information - should probably be in private struct */
    void *face_surface_types;
    int *obj_cnt;
    int *island_faces;
    int *island_loops;
    int *fol; /* Faces with outer loops in object loop network */
    int *fil; /* Faces with only inner loops in object loop network */
    int island_faces_cnt;
    int island_loops_cnt;
    int fol_cnt;
    int fil_cnt;
    int null_vert_cnt;
    int *null_verts;
    int null_edge_cnt;
    int *null_edges;
};


extern BREP_EXPORT struct bu_ptbl *brep_to_csg(struct bu_vls *msgs, const ON_Brep *brep);

} /* extern C++ */
#endif

#endif  /* BREP_H */
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
