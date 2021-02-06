/*                       I N T E R S E C T . H
 * BRL-CAD
 *
 * Copyright (c) 2007-2021 United States Government as represented by
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
/** @addtogroup brep_intersect
 * @brief
 * Intersection routines for Non-Uniform Rational
 * B-Spline (NURBS) curves and surfaces.
 */
#ifndef BREP_INTERSECT_H
#define BREP_INTERSECT_H

#include "common.h"
#include "brep/defines.h"

/** @{ */
/** @file brep/intersect.h */

#ifdef __cplusplus

extern "C++" {

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


} /* extern C++ */
#endif

#endif  /* BREP_INTERSECT_H */
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
