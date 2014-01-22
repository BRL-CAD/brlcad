/*                       B R E P . H
 * BRL-CAD
 *
 * Copyright (c) 2007-2014 United States Government as represented by
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
/** @file brep.h
 *
 * Define surface and curve structures for Non-Uniform Rational
 * B-Spline (NURBS) curves and surfaces. Uses openNURBS library.
 *
 */
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

#include "dvec.h"
#include "opennurbs.h"
#include <iostream>
#include <fstream>

namespace brlcad {
template <class T> class BVNode;
typedef class BVNode<ON_BoundingBox> BBNode;
}
}
__BEGIN_DECLS
#endif

#include "vmath.h"
#include "bu.h"

#ifndef __cplusplus
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

#ifndef __cplusplus
typedef struct _bounding_volume_placeholder {
    int dummy;
} BrepBoundingVolume;
#else
/* namespace brlcad { */
/*     class BBNode; */
/* }; */
typedef brlcad::BBNode BrepBoundingVolume;
#endif

typedef struct _brep_cdbitem {
    int dummy; /* MS Visual C hack which can be removed if the struct contains something meaningful */
} brep_cdbitem;


#ifdef __cplusplus

__END_DECLS

extern "C++" {
class plane_ray {
public:
    vect_t n1;
    fastf_t d1;

    vect_t n2;
    fastf_t d2;
};

/**
 * These definitions were added to opennurbs_curve.h - they are
 * extensions of openNURBS, so add them here instead.  At some point a
 * more coherent structure should probably be set up for organization
 * of openNURBS extensions, since there may be more to come, but at
 * least try to keep as many as possible out of the external openNURBS
 * tree - simplifies syncing.
 */
class ON_Ray {
public:
    ON_3dPoint m_origin;
    ON_3dVector m_dir;

    ON_Ray(ON_3dPoint &origin, ON_3dVector &dir);
    ON_Ray(ON_2dPoint &origin, ON_2dVector &dir);
    ON_Ray(const ON_Ray &r);

    ON_Ray &operator=(const ON_Ray &r);
    ON_3dPoint PointAt(double t) const;
    double DistanceTo(const ON_3dPoint &pt, double *out_t = NULL) const;

    /**
     * Intersect two 2d Rays
     * @param v [in] other ray to intersect with
     * @param isect [out] point of intersection
     * @return true if single point of intersection is found
     */
    bool IntersectRay(const ON_Ray &v, ON_2dPoint &isect) const;
};

inline
ON_Ray::ON_Ray(ON_3dPoint &origin, ON_3dVector &dir)
    : m_origin(origin), m_dir(dir)
{
}

inline
ON_Ray::ON_Ray(ON_2dPoint &origin, ON_2dVector &dir)
    : m_origin(origin), m_dir(dir)
{
}

inline
ON_Ray::ON_Ray(const ON_Ray &r)
    : m_origin(r.m_origin), m_dir(r.m_dir)
{
}

inline
ON_Ray &
ON_Ray::operator=(const ON_Ray &r)
{
    m_origin = r.m_origin;
    m_dir = r.m_dir;
    return *this;
}

inline
ON_3dPoint
ON_Ray::PointAt(double t) const
{
    return m_origin + m_dir * t;
}

inline
double
ON_Ray::DistanceTo(const ON_3dPoint &pt, double *out_t /* = NULL */) const
{
    ON_3dVector w = pt - m_origin;
    double c1 = w * m_dir;
    if (c1 <= 0) {
	return pt.DistanceTo(m_origin);
    }
    double c2 = m_dir * m_dir;
    double b = c1 / c2;
    ON_3dPoint p = m_dir * b + m_origin;
    if (out_t != NULL) {
	*out_t = b;
    }
    return p.DistanceTo(pt);
}

inline
bool
ON_Ray::IntersectRay(const ON_Ray &v, ON_2dPoint &isect) const
{
    double uxv, q_pxv;
    /* consider parallel and collinear cases */
    if (ZERO(uxv = V2CROSS(m_dir, v.m_dir)) ||
	(ZERO(q_pxv = V2CROSS(v.m_origin - m_origin, v.m_dir))))
    {
	return false;
    }
    isect = PointAt(q_pxv / uxv);
    return true;
}

BREP_EXPORT void brep_get_plane_ray(ON_Ray &r, plane_ray &pr);
BREP_EXPORT void brep_r(const ON_Surface *surf, plane_ray &pr, pt2d_t uv, ON_3dPoint &pt, ON_3dVector &su, ON_3dVector &sv, pt2d_t R);
BREP_EXPORT void brep_newton_iterate(plane_ray &pr, pt2d_t R, ON_3dVector &su, ON_3dVector &sv, pt2d_t uv, pt2d_t out_uv);
BREP_EXPORT void utah_ray_planes(const ON_Ray &r, ON_3dVector &p1, double &p1d, ON_3dVector &p2, double &p2d);

BREP_EXPORT bool ON_NearZero(double x, double tolerance = ON_ZERO_TOLERANCE);

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
 * growing bounding box
 */
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

/* FIXME: debugging crapola (clean up later) */
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

namespace brlcad {
/**
 * Bounding area hierarchy classes
 */
template<class BA> class BANode;

template<class BA>
class BANode {
public:
    BANode();
    BANode(const BA &node);
    BANode(const ON_Curve *curve, int m_adj_face_index, const BA &node,
    const ON_BrepFace *face, const ON_Interval &t,
    bool innerTrim = false, bool checkTrim = true, bool trimmed = false);
    ~BANode();

    /** List of all children of a given node */
    typedef std::vector<BANode<BA>*> ChildList;
    ChildList m_children;

    /** Bounding Box */
    BA m_node;

    /** Node management functions */
    void addChild(const BA &child);
    void addChild(BANode<BA> *child);
    void removeChild(BANode<BA> *child);

    /** Test if this node is a leaf node (i.e. m_children is empty) */
    bool isLeaf();

    /** Return a list of all nodes below this node that are leaf nodes */
    void getLeaves(std::list<BANode<BA>*> &out_leaves);

    /** Report the depth of this node in the hierarchy */
    int depth();

    /**
     * Get 2 points defining bounding box:
     *
     * @verbatim
     *       *----------------max
     *       |                 |
     *  v    |                 |
     *       |                 |
     *      min----------------*
     *                 u
     * @endverbatim
     */
    void GetBBox(fastf_t *min, fastf_t *max) const;

    /** Surface Information */
    const ON_BrepFace *m_face;
    ON_Interval m_u;
    ON_Interval m_v;

    /** Trim Curve Information */
    const ON_Curve *m_trim;
    ON_Interval m_t;
    int m_adj_face_index;

    /** Trimming Flags */
    bool m_checkTrim;
    bool m_trimmed;
    bool m_XIncreasing;
    bool m_Horizontal;
    bool m_Vertical;
    bool m_innerTrim;

    ON_3dPoint m_estimate;
    ON_2dPoint getClosestPointEstimate(const ON_3dPoint &pt);
    ON_2dPoint getClosestPointEstimate(const ON_3dPoint &pt, ON_Interval &u, ON_Interval &v);
    fastf_t getLinearEstimateOfV(fastf_t u);
    fastf_t getCurveEstimateOfV(fastf_t u, fastf_t tol) const;
    fastf_t getCurveEstimateOfU(fastf_t v, fastf_t tol) const;

    int isTrimmed(const ON_2dPoint &uv, double &trimdist) const;
    bool doTrimming() const;

private:
    BANode<BA> *closer(const ON_3dPoint &pt, BANode<BA> *left, BANode<BA> *right);
    fastf_t m_slope;
    fastf_t m_vdot;
    fastf_t m_bb_diag;
    ON_3dPoint m_start;
    ON_3dPoint m_end;
};

typedef BANode<ON_BoundingBox> BRNode;

template<class BA>
inline
BANode<BA>::BANode() { }

template<class BA>
inline
_BU_ATTR_ALWAYS_INLINE
BANode<BA>::BANode(const ON_Curve *curve, int adj_face_index, const BA &node,
		   const ON_BrepFace *face, const ON_Interval &t,
		   bool innerTrim, bool checkTrim, bool trimmed)
    : m_node(node), m_face(face), m_trim(curve), m_t(t), m_adj_face_index(adj_face_index),
      m_checkTrim(checkTrim), m_trimmed(trimmed), m_innerTrim(innerTrim), m_slope(0.0), m_vdot(0.0)
{
    m_start = curve->PointAt(m_t[0]);
    m_end = curve->PointAt(m_t[1]);
    /* check for vertical segments they can be removed from trims
     * above (can't tell direction and don't need
     */
    m_Horizontal = false;
    m_Vertical = false;

    if (NEAR_EQUAL(m_end[X], m_start[X], 0.000001)) {
	m_Vertical = true;
	if (m_innerTrim) {
	    m_XIncreasing = false;
	} else {
	    m_XIncreasing = true;
	}
    } else if (NEAR_EQUAL(m_end[Y], m_start[Y], 0.000001)) {
	m_Horizontal = true;
	if ((m_end[X] - m_start[X]) > 0.0) {
	    m_XIncreasing = true;
	} else {
	    m_XIncreasing = false;
	}
	m_slope = 0.0;
    } else {
	if ((m_end[X] - m_start[X]) > 0.0) {
	    m_XIncreasing = true;
	} else {
	    m_XIncreasing = false;
	}
	m_slope = (m_end[Y] - m_start[Y]) / (m_end[X] - m_start[X]);
    }
    m_bb_diag = DIST_PT_PT(m_start, m_end);
}

template<class BA>
inline
_BU_ATTR_ALWAYS_INLINE
BANode<BA>::BANode(const BA &node) : m_node(node)
{
    m_adj_face_index = -99;
    m_checkTrim = true;
    m_trimmed = false;
    m_Horizontal = false;
    m_Vertical = false;
    m_XIncreasing = false;
    m_innerTrim = false;
    m_bb_diag = 0.0;
    m_slope = 0.0;
    m_vdot = 0.0;
    m_face = NULL;
    m_trim = NULL;
    for (int i = 0; i < 3; i++) {
	double d = m_node.m_max[i] - m_node.m_min[i];
	if (NEAR_ZERO(d, ON_ZERO_TOLERANCE)) {
	    m_node.m_min[i] -= 0.001;
	    m_node.m_max[i] += 0.001;
	}
    }
}

template<class BA>
BANode<BA>::~BANode()
{
    /* delete the children */
    for (size_t i = 0; i < m_children.size(); i++) {
	delete m_children[i];
    }
}

template<class BA>
inline void
BANode<BA>::addChild(const BA &child)
{
    m_children.push_back(new BANode<BA>(child));
}

template<class BA>
inline void
BANode<BA>::addChild(BANode<BA> *child)
{
    if (LIKELY(child != NULL)) {
	m_children.push_back(child);
    }
}

template<class BA>
inline void
BANode<BA>::removeChild(BANode<BA> *child)
{
    typename ChildList::iterator i;
    for (i = m_children.begin(); i < m_children.end(); ++i) {
	if (*i == child) {
	    delete *i;
	    m_children.erase(i);
	}
    }
}

template<class BA>
inline bool
BANode<BA>::isLeaf()
{
    if (m_children.size() == 0) {
	return true;
    }
    return false;
}

/*
  template<class BA>
  inline void
  BANode<BA>::GetBBox(double* min, double* max) const
  {
  min[0] = m_node.m_min[0];
  min[1] = m_node.m_min[1];
  min[2] = m_node.m_min[2];
  max[0] = m_node.m_max[0];
  max[1] = m_node.m_max[1];
  max[2] = m_node.m_max[2];
  }
*/

template<class BA>
inline void
_BU_ATTR_ALWAYS_INLINE
BANode<BA>::GetBBox(fastf_t *min, fastf_t *max) const
{
    VSETALL(min, INFINITY);
    VSETALL(max, -INFINITY);
    VMINMAX(min, max, m_start);
    VMINMAX(min, max, m_end);
}

template<class BA>
int
BANode<BA>::depth()
{
    int d = 0;
    for (size_t i = 0; i < m_children.size(); i++) {
	d = 1 + std::max(d, m_children[i]->depth());
    }
    return d;
}

template<class BA>
void
BANode<BA>::getLeaves(std::list<BANode<BA>*> &out_leaves)
{
    if (m_children.size() > 0) {
	for (size_t i = 0; i < m_children.size(); i++) {
	    m_children[i]->getLeaves(out_leaves);
	}
    } else {
	out_leaves.push_back(this);
    }
}

template<class BA>
BANode<BA> *
BANode<BA>::closer(const ON_3dPoint &pt, BANode<BA> *left, BANode<BA> *right)
{
    double ldist = pt.DistanceTo(left->m_estimate);
    double rdist = pt.DistanceTo(right->m_estimate);
    TRACE("\t" << ldist << " < " << rdist);
    if (ldist < rdist) {
	return left;
    } else {
	return right;
    }
}

template<class BA>
int
BANode<BA>::isTrimmed(const ON_2dPoint &uv, double &trimdist) const
{
    point_t bmin, bmax;
    BANode<BA>::GetBBox(bmin, bmax);
    if ((bmin[X] <= uv[X]) && (uv[X] <= bmax[X]))   /* if check trim and in BBox */
    {
	fastf_t v = getCurveEstimateOfV(uv[X], 0.0000001);
	trimdist = v - uv[Y];
	if (uv[Y] <= v) {
	    if (m_XIncreasing) {
		return 1;
	    } else {
		return 0;
	    }
	} else if (uv[Y] > v) {
	    if (!m_XIncreasing) {
		return 1;
	    } else {
		return 0;
	    }
	} else {
	    return 1;
	}
    } else {
	trimdist = -1.0;
	if (m_trimmed) {
	    return 1;
	} else {
	    return 0;
	}
    }
}

template<class BA>
inline bool
BANode<BA>::doTrimming() const
{
    return m_checkTrim;
}

template<class BA>
ON_2dPoint
BANode<BA>::getClosestPointEstimate(const ON_3dPoint &pt)
{
    ON_Interval u, v;
    return getClosestPointEstimate(pt, u, v);
}

template<class BA>
ON_2dPoint
BANode<BA>::getClosestPointEstimate(const ON_3dPoint &pt, ON_Interval &u, ON_Interval &v)
{
    if (isLeaf()) {
	double uvs[5][2] = {{m_u.Min(), m_v.Min()},  /* include the corners for an easy refinement */
	    {m_u.Max(), m_v.Min()},
	    {m_u.Max(), m_v.Max()},
	    {m_u.Min(), m_v.Max()},
	    {m_u.Mid(), m_v.Mid()}
	}; /* include the estimate */
	ON_3dPoint corners[5];
	const ON_Surface *surf = m_face->SurfaceOf();

	u = m_u;
	v = m_v;

	/* ??? should we pass these in from SurfaceTree::curveBBox()
	 * to avoid this recalculation?
	 */
	if (!surf->EvPoint(uvs[0][0], uvs[0][1], corners[0]) ||
	    !surf->EvPoint(uvs[1][0], uvs[1][1], corners[1]) ||
	    !surf->EvPoint(uvs[2][0], uvs[2][1], corners[2]) ||
	    !surf->EvPoint(uvs[3][0], uvs[3][1], corners[3]))
	{
	    throw new std::exception(); /* FIXME */
	}
	corners[4] = BANode<BA>::m_estimate;

	/* find the point on the curve closest to pt */
	size_t mini = 0;
	double mindist = pt.DistanceTo(corners[mini]);
	double tmpdist;
	for (size_t i = 1; i < 5; i++) {
	    tmpdist = pt.DistanceTo(corners[i]);
	    TRACE("\t" << mindist << " < " << tmpdist);
	    if (tmpdist < mindist) {
		mini = i;
		mindist = tmpdist;
	    }
	}
	TRACE("Closest: " << mindist << "; " << PT2(uvs[mini]));
	return ON_2dPoint(uvs[mini][0], uvs[mini][1]);
    } else {
	if (m_children.size() > 0) {
	    BRNode *closestNode = m_children[0];
	    for (size_t i = 1; i < m_children.size(); i++) {
		closestNode = closer(pt, closestNode, m_children[i]);
	    }
	    return closestNode->getClosestPointEstimate(pt, u, v);
	} else {
	    throw new std::exception();
	}
    }
}

template<class BA>
fastf_t
BANode<BA>::getLinearEstimateOfV(fastf_t u)
{
    fastf_t v = m_start[Y] + m_slope * (u - m_start[X]);
    return v;
}

template<class BA>
fastf_t
BANode<BA>::getCurveEstimateOfV(fastf_t u, fastf_t tol) const
{
    ON_3dVector tangent;
    point_t A, B;
    double Ta, Tb;

    if (m_start[X] < m_end[X]) {
	VMOVE(A, m_start);
	VMOVE(B, m_end);
	Ta = m_t[0];
	Tb = m_t[1];
    } else {
	VMOVE(A, m_end);
	VMOVE(B, m_start);
	Ta = m_t[1];
	Tb = m_t[0];
    }

    fastf_t dU = B[X] - A[X];
    if (NEAR_ZERO(dU, tol))    /* vertical */
    {
	return A[Y];
    }

    ON_3dVector Tan_start = m_trim->TangentAt(Ta);
    ON_3dVector Tan_end = m_trim->TangentAt(Tb);

    fastf_t dT = Tb - Ta;
    fastf_t guess;
    ON_3dPoint p;

    /* Use quick binary subdivision until derivatives at end points in 'u' are within 5 percent */
    while (!NEAR_ZERO(dU, tol) && !NEAR_ZERO(dT, tol)) {
	guess = Ta + dT / 2;
	p = m_trim->PointAt(guess);

	if (UNLIKELY(NEAR_EQUAL(p[X], u, SMALL_FASTF))) {
	    /* hit 'u' exactly, done deal */
	    return p[Y];
	}

	if (p[X] > u) {
	    /* v is behind us, back up the end */
	    Tb = guess;
	    VMOVE(B, p);
	    Tan_end = m_trim->TangentAt(Tb);
	} else {
	    /* v is in front, move start forward */
	    Ta = guess;
	    VMOVE(A, p);
	    Tan_start = m_trim->TangentAt(Ta);
	}
	dT = Tb - Ta;
	dU = B[X] - A[X];
    }

    dU = B[X] - A[X];
    if (NEAR_ZERO(dU, tol))    /* vertical */
    {
	return A[Y];
    }

    guess = Ta + (u - A[X]) * dT / dU;
    p = m_trim->PointAt(guess);

    int cnt = 0;
    while ((cnt < 1000) && (!NEAR_EQUAL(p[X], u, tol))) {
	if (p[X] < u) {
	    Ta = guess;
	    VMOVE(A, p);
	} else {
	    Tb = guess;
	    VMOVE(B, p);
	}
	dU = B[X] - A[X];
	if (NEAR_ZERO(dU, tol)) {  /* vertical */
	    return A[Y];
	}

	dT = Tb - Ta;
	guess = Ta + (u - A[X]) * dT / dU;
	p = m_trim->PointAt(guess);
	cnt++;
    }
    if (cnt > 999) {
	bu_log("getCurveEstimateOfV(): estimate of 'v' given a trim curve and "
		"'u' did not converge within iteration bound(%d).\n", cnt);
    }
    return p[Y];
}

template<class BA>
fastf_t
BANode<BA>::getCurveEstimateOfU(fastf_t v, fastf_t tol) const
{
    ON_3dVector tangent;
    point_t A, B;
    double Ta, Tb;

    if (m_start[Y] < m_end[Y]) {
	VMOVE(A, m_start);
	VMOVE(B, m_end);
	Ta = m_t[0];
	Tb = m_t[1];
    } else {
	VMOVE(A, m_end);
	VMOVE(B, m_start);
	Ta = m_t[1];
	Tb = m_t[0];
    }

    fastf_t dV = B[Y] - A[Y];
    if (NEAR_ZERO(dV, tol))    /* horizontal */
    {
	return A[X];
    }

    ON_3dVector Tan_start = m_trim->TangentAt(Ta);
    ON_3dVector Tan_end = m_trim->TangentAt(Tb);

    fastf_t dT = Tb - Ta;
    fastf_t guess;
    ON_3dPoint p;

    /* Use quick binary subdivision until derivatives at end points in 'u' are within 5 percent */
    while (!NEAR_ZERO(dV, tol) && !NEAR_ZERO(dT, tol)) {
	guess = Ta + dT / 2;
	p = m_trim->PointAt(guess);
	if (p[Y] < v) {
	    Ta = guess;
	    VMOVE(A, p);
	    Tan_start = m_trim->TangentAt(Ta);
	} else {
	    Tb = guess;
	    VMOVE(B, p);
	    Tan_end = m_trim->TangentAt(Tb);
	}
	dT = Tb - Ta;
	dV = B[Y] - A[Y];
    }

    dV = B[Y] - A[Y];
    if (NEAR_ZERO(dV, tol))    /* horizontal */
    {
	return A[X];
    }

    guess = Ta + (v - A[Y]) * dT / dV;
    p = m_trim->PointAt(guess);

    int cnt = 0;
    while ((cnt < 1000) && (!NEAR_EQUAL(p[Y], v, tol))) {
	if (p[Y] < v) {
	    Ta = guess;
	    VMOVE(A, p);
	} else {
	    Tb = guess;
	    VMOVE(B, p);
	}
	dV = B[Y] - A[Y];
	if (NEAR_ZERO(dV, tol)) {  /* horizontal */
	    return A[X];
	}
	dT = Tb - Ta;
	guess = Ta + (v - A[Y]) * dT / dV;
	p = m_trim->PointAt(guess);
	cnt++;
    }
    if (cnt > 999) {
	bu_log("getCurveEstimateOfV(): estimate of 'u' given a trim curve and "
		"'v' did not converge within iteration bound(%d).\n", cnt);
    }
    return p[X];
}

extern bool sortX(BRNode *first, BRNode *second);
extern bool sortY(BRNode *first, BRNode *second);

/*--------------------------------------------------------------------------------
 * CurveTree declaration
 */
class BREP_EXPORT CurveTree {
public:
    CurveTree(const ON_BrepFace *face);
    ~CurveTree();

    BRNode *getRootNode() const;

    /**
     * Calculate, using the surface bounding volume hierarchy, a uv
     * estimate for the closest point on the surface to the point in
     * 3-space.
     */
    ON_2dPoint getClosestPointEstimate(const ON_3dPoint &pt);
    ON_2dPoint getClosestPointEstimate(const ON_3dPoint &pt, ON_Interval &u, ON_Interval &v);

    /**
     * Return just the leaves of the surface tree
     */
    void getLeaves(std::list<BRNode *> &out_leaves);
    void getLeavesAbove(std::list<BRNode *> &out_leaves, const ON_Interval &u, const ON_Interval &v);
    void getLeavesAbove(std::list<BRNode *> &out_leaves, const ON_2dPoint &pt, fastf_t tol);
    void getLeavesRight(std::list<BRNode *> &out_leaves, const ON_Interval &u, const ON_Interval &v);
    void getLeavesRight(std::list<BRNode *> &out_leaves, const ON_2dPoint &pt, fastf_t tol);
    int depth();

private:
    bool getHVTangents(const ON_Curve *curve, ON_Interval &t, std::list<fastf_t> &list);
    bool isLinear(const ON_Curve *curve, double min, double max);
    BRNode *subdivideCurve(const ON_Curve *curve, int adj_face_index, double min, double max, bool innerTrim, int depth);
    BRNode *curveBBox(const ON_Curve *curve, int adj_face_index, ON_Interval &t, bool leaf, bool innerTrim, const ON_BoundingBox &bb);
    BRNode *initialLoopBBox();

    const ON_BrepFace *m_face;
    BRNode *m_root;
    std::list<BRNode *> m_sortedX;
    std::list<BRNode *> m_sortedY;
};


/*--------------------------------------------------------------------------------
 * Bounding volume hierarchy classes
 */
template<class BV>
class BVNode {
public:
    BVNode();
    BVNode(const BV &node);
    BVNode(CurveTree *ct);
    BVNode(CurveTree *ct, const BV &node);
    BVNode(CurveTree *ct, const BV &node, const ON_BrepFace *face, const ON_Interval &u,
    const ON_Interval &v, bool checkTrim = false, bool trimmed = false);

    ~BVNode();

    /** List of all children of a given node */
    typedef std::vector<BVNode<BV>*> ChildList;
    ChildList m_children;

    /** Curve Tree associated with the parent Surface Tree */
    CurveTree *m_ctree;

    /** Bounding Box */
    BV m_node;

    /** Test if this node is a leaf node in the hierarchy */
    bool isLeaf();

    /** Return all leaves below this node that are leaf nodes */
    void getLeaves(std::list<BVNode<BV>*> &out_leaves);

    /** Functions to add and remove child nodes from this node. */
    void addChild(const BV &child);
    void addChild(BVNode<BV> *child);
    void removeChild(BVNode<BV> *child);

    /** Report the depth of this node in the hierarchy */
    int depth();

    /** Get 2 points defining a bounding box
     *
     * @verbatim
     *                _  max  _
     *        _   -       +      -  _
     *     *  _           +         _  *
     *     |      -   _   + _   -      |
     *     |             *+            |
     *     |             |+            |
     *     |          _  |+   _        |
     *     |  _   -      |       -  _  |
     *     *  _          |          _  *
     *            -   _  |  _   -
     *                  min
     * @endverbatim
     */
    void GetBBox(float *min, float *max);
    void GetBBox(double *min, double *max);

    /** Surface Information */
    const ON_BrepFace *m_face;
    ON_Interval m_u;
    ON_Interval m_v;

    /** Trimming Flags */
    bool m_checkTrim;
    bool m_trimmed;

    /** Point used for closeness testing - usually based on evaluation
     * of the curve/surface at the center of the parametric domain
     */
    ON_3dPoint m_estimate;

    /* Normal at the m_estimate point */
    ON_3dVector m_normal;

    /** Test whether a ray intersects the 3D bounding volume of the
     * node - if so, and node is not a leaf node, query children.  If
     * leaf node, and intersects, add to list.
     */
    bool intersectedBy(ON_Ray &ray, double *tnear = 0, double *tfar = 0);
    bool intersectsHierarchy(ON_Ray &ray, std::list<BVNode<ON_BoundingBox>*> &results);

    /** Report if a given uv point is within the uv boundaries defined
     * by a node.
     */
    bool containsUV(const ON_2dPoint &uv);


    ON_2dPoint getClosestPointEstimate(const ON_3dPoint &pt);
    ON_2dPoint getClosestPointEstimate(const ON_3dPoint &pt, ON_Interval &u, ON_Interval &v);
    int getLeavesBoundingPoint(const ON_3dPoint &pt, std::list<BVNode<BV> *> &out);
    int isTrimmed(const ON_2dPoint &uv, BRNode **closest, double &closesttrim) const;
    bool doTrimming() const;

    void getTrimsAbove(const ON_2dPoint &uv, std::list<BRNode *> &out_leaves) const;
    void BuildBBox();
    bool prepTrims();

private:
    BVNode<BV> *closer(const ON_3dPoint &pt, BVNode<BV> *left, BVNode<BV> *right);
    std::list<BRNode *> m_trims_above;
    std::list<BRNode *> m_trims_vertical;
    /*		std::list<BRNode*> m_trims_right; */
};

typedef BVNode<ON_BoundingBox> BBNode;

/*--------------------------------------------------------------------------------
 * Template Implementation
 */
template<class BV>
inline
BVNode<BV>::BVNode()
{
    m_face = NULL;
    m_ctree = NULL;
    m_checkTrim = true;
    m_trimmed = false;

}

template<class BV>
inline
BVNode<BV>::BVNode(const BV &node) : m_node(node)
{
    m_face = NULL;
    m_ctree = NULL;
    m_checkTrim = true;
    m_trimmed = false;
    for (int i = 0; i < 3; i++) {
	double d = m_node.m_max[i] - m_node.m_min[i];
	if (ON_NearZero(d, ON_ZERO_TOLERANCE)) {
	    m_node.m_min[i] -= 0.001;
	    m_node.m_max[i] += 0.001;
	}
    }
}

template<class BV>
inline
BVNode<BV>::BVNode(CurveTree *ct): m_ctree(ct)
{
    m_face = NULL;
    m_checkTrim = true;
    m_trimmed = false;
}

template<class BV>
inline
BVNode<BV>::BVNode(CurveTree *ct, const BV &node) : m_ctree(ct), m_node(node)
{
    m_face = NULL;
    m_checkTrim = true;
    m_trimmed = false;
    for (int i = 0; i < 3; i++) {
	double d = m_node.m_max[i] - m_node.m_min[i];
	if (ON_NearZero(d, ON_ZERO_TOLERANCE)) {
	    m_node.m_min[i] -= 0.001;
	    m_node.m_max[i] += 0.001;
	}
    }
}

template<class BV>
inline
_BU_ATTR_ALWAYS_INLINE
BVNode<BV>::BVNode(CurveTree *ct, const BV &node, const ON_BrepFace *face,
		   const ON_Interval &u, const ON_Interval &v, bool checkTrim, bool trimmed)
    : m_ctree(ct), m_node(node), m_face(face), m_u(u), m_v(v), m_checkTrim(checkTrim),
      m_trimmed(trimmed)
{
    for (int i = 0; i < 3; i++) {
	double d = m_node.m_max[i] - m_node.m_min[i];
	if (ON_NearZero(d, ON_ZERO_TOLERANCE)) {
	    m_node.m_min[i] -= 0.001;
	    m_node.m_max[i] += 0.001;
	}
    }
}

template<class BV>
BVNode<BV>::~BVNode()
{
    /* delete the children */
    for (size_t i = 0; i < m_children.size(); i++) {
	delete m_children[i];
    }
}

template<class BV>
inline void
BVNode<BV>::addChild(const BV &child)
{
    m_children.push_back(new BVNode<BV>(child));
}

template<class BV>
inline void
BVNode<BV>::addChild(BVNode<BV> *child)
{
    if (LIKELY(child != NULL)) {
	m_children.push_back(child);
    }
}

template<class BV>
inline void
BVNode<BV>::removeChild(BVNode<BV> *child)
{
    typename ChildList::iterator i;
    for (i = m_children.begin(); i < m_children.end(); ++i) {
	if (*i == child) {
	    delete *i;
	    m_children.erase(i);
	}
    }
}

template<class BV>
inline bool
BVNode<BV>::isLeaf()
{
    if (m_children.size() == 0) {
	return true;
    }
    return false;
}

template<class BV>
inline void
BVNode<BV>::GetBBox(float *min, float *max)
{
    min[0] = m_node.m_min[0];
    min[1] = m_node.m_min[1];
    min[2] = m_node.m_min[2];
    max[0] = m_node.m_max[0];
    max[1] = m_node.m_max[1];
    max[2] = m_node.m_max[2];
}

template<class BV>
inline void
BVNode<BV>::GetBBox(double *min, double *max)
{
    min[0] = m_node.m_min[0];
    min[1] = m_node.m_min[1];
    min[2] = m_node.m_min[2];
    max[0] = m_node.m_max[0];
    max[1] = m_node.m_max[1];
    max[2] = m_node.m_max[2];
}

template<class BV>
inline bool
BVNode<BV>::intersectedBy(ON_Ray &ray, double *tnear_opt, double *tfar_opt)
{
    double tnear = -DBL_MAX;
    double tfar = DBL_MAX;
    bool untrimmedresult = true;
    for (int i = 0; i < 3; i++) {
	if (UNLIKELY(ON_NearZero(ray.m_dir[i]))) {
	    if (ray.m_origin[i] < m_node.m_min[i] || ray.m_origin[i] > m_node.m_max[i]) {
		untrimmedresult = false;
	    }
	} else {
	    double t1 = (m_node.m_min[i] - ray.m_origin[i]) / ray.m_dir[i];
	    double t2 = (m_node.m_max[i] - ray.m_origin[i]) / ray.m_dir[i];
	    if (t1 > t2) {
		double tmp = t1;    /* swap */
		t1 = t2;
		t2 = tmp;
	    }
	    if (t1 > tnear) {
		tnear = t1;
	    }
	    if (t2 < tfar) {
		tfar = t2;
	    }
	    if (tnear > tfar) { /* box is missed */
		untrimmedresult = false;
	    }
	    /* go ahead and solve hits behind us
	    if (tfar < 0) untrimmedresult = false;
		*/
	}
    }
    if (LIKELY(tnear_opt != NULL && tfar_opt != NULL)) {
	*tnear_opt = tnear;
	*tfar_opt = tfar;
    }
    if (isLeaf()) {
	return !m_trimmed && untrimmedresult;
    } else {
	return untrimmedresult;
    }
}

template<class BV>
bool
BVNode<BV>::intersectsHierarchy(ON_Ray &ray, std::list<BBNode *> &results_opt)
{
    double tnear, tfar;
    bool intersects = intersectedBy(ray, &tnear, &tfar);
    if (intersects && isLeaf()) {
	results_opt.push_back(this);
    } else if (intersects) {
	for (size_t i = 0; i < m_children.size(); i++) {
	    m_children[i]->intersectsHierarchy(ray, results_opt);
	}
    }
    return intersects;
}

template<class BV>
bool
BVNode<BV>::containsUV(const ON_2dPoint &uv)
{
    if ((uv[0] > m_u[0]) && (uv[0] < m_u[1]) && (uv[1] > m_v[0]) && (uv[1] < m_v[1])) {
	return true;
    } else {
	return false;
    }
}

template<class BV>
int
BVNode<BV>::depth()
{
    int d = 0;
    for (size_t i = 0; i < m_children.size(); i++) {
	d = 1 + std::max(d, m_children[i]->depth());
    }
    return d;
}

template<class BV>
void
BVNode<BV>::getLeaves(std::list<BVNode<BV>*> &out_leaves)
{
    if (m_children.size() > 0) {
	for (size_t i = 0; i < m_children.size(); i++) {
	    m_children[i]->getLeaves(out_leaves);
	}
    } else {
	out_leaves.push_back(this);
    }
}

template<class BV>
BVNode<BV> *
BVNode<BV>::closer(const ON_3dPoint &pt, BVNode<BV> *left, BVNode<BV> *right)
{
    double ldist = pt.DistanceTo(left->m_estimate);
    double rdist = pt.DistanceTo(right->m_estimate);
    TRACE("\t" << ldist << " < " << rdist);
    if (ldist < rdist) {
	return left;
    } else {
	return right;
    }
}

template<class BV>
ON_2dPoint
BVNode<BV>::getClosestPointEstimate(const ON_3dPoint &pt)
{
    ON_Interval u, v;
    return getClosestPointEstimate(pt, u, v);
}

template<class BV>
ON_2dPoint
BVNode<BV>::getClosestPointEstimate(const ON_3dPoint &pt, ON_Interval &u, ON_Interval &v)
{
    if (isLeaf()) {
	double uvs[5][2] = {{m_u.Min(), m_v.Min()}, {m_u.Max(), m_v.Min()},
	    {m_u.Max(), m_v.Max()}, {m_u.Min(), m_v.Max()},
	    {m_u.Mid(), m_v.Mid()}
	}; /* include the estimate */
	ON_3dPoint corners[5];
	const ON_Surface *surf = m_face->SurfaceOf();

	u = m_u;
	v = m_v;

	/* ??? pass these in from SurfaceTree::surfaceBBox() to avoid
	 * this recalculation?
	 */
	if (!surf->EvPoint(uvs[0][0], uvs[0][1], corners[0]) ||
	    !surf->EvPoint(uvs[1][0], uvs[1][1], corners[1]) ||
	    !surf->EvPoint(uvs[2][0], uvs[2][1], corners[2]) ||
	    !surf->EvPoint(uvs[3][0], uvs[3][1], corners[3]))
	{
	    throw new std::exception(); /* FIXME */
	}
	corners[4] = BVNode<BV>::m_estimate;

	/* find the point on the surface closest to pt */
	size_t mini = 0;
	double mindist = pt.DistanceTo(corners[mini]);
	double tmpdist;
	for (size_t i = 1; i < 5; i++) {
	    tmpdist = pt.DistanceTo(corners[i]);
	    TRACE("\t" << mindist << " < " << tmpdist);
	    if (tmpdist < mindist) {
		mini = i;
		mindist = tmpdist;
	    }
	}
	TRACE("Closest: " << mindist << "; " << PT2(uvs[mini]));
	return ON_2dPoint(uvs[mini][0], uvs[mini][1]);
    } else {
	if (m_children.size() > 0) {
	    BBNode *closestNode = m_children[0];
	    for (size_t i = 1; i < m_children.size(); i++) {
		closestNode = closer(pt, closestNode, m_children[i]);
		TRACE("\t" << PT(closestNode->m_estimate));
	    }
	    return closestNode->getClosestPointEstimate(pt, u, v);
	}
	throw new std::exception();
    }
}

template<class BV>
int
BVNode<BV>::getLeavesBoundingPoint(const ON_3dPoint &pt, std::list<BVNode<BV> *> &out)
{
    if (isLeaf()) {
	double min[3], max[3];
	GetBBox(min, max);
	if ((pt.x >= (min[0])) && (pt.x <= (max[0])) &&
	    (pt.y >= (min[1])) && (pt.y <= (max[1])) &&
	    (pt.z >= (min[2])) && (pt.z <= (max[2])))
	{
	    /* falls within BBox so put in list */
	    out.push_back(this);
	    return 1;
	}
	return 0;
    } else {
	int sum = 0;
	for (size_t i = 0; i < m_children.size(); i++) {
	    sum += m_children[i]->getLeavesBoundingPoint(pt, out);
	}
	return sum;
    }
}

template<class BV>
int
BVNode<BV>::isTrimmed(const ON_2dPoint &uv, BRNode **closest, double &closesttrim) const
{
    BRNode *br;
    std::list<BRNode *> trims;

    closesttrim = -1.0;
    if (m_checkTrim) {
	getTrimsAbove(uv, trims);

	if (trims.empty()) {
	    return 1;
	} else { /* find closest BB */
	    std::list<BRNode *>::iterator i;
	    BRNode *vclosest = NULL;
	    BRNode *uclosest = NULL;
	    fastf_t currHeight = (fastf_t)0.0;
	    bool currTrimStatus = false;
	    bool verticalTrim = false;
	    bool underTrim = false;
	    double vdist = 0.0;
	    double udist = 0.0;

	    for (i = trims.begin(); i != trims.end(); i++) {
		br = dynamic_cast<BRNode *>(*i);

		/* skip if trim below */
		if (br->m_node.m_max[1] + BREP_EDGE_MISS_TOLERANCE < uv[Y]) {
		    continue;
		}

		if (br->m_Vertical) {
		    if ((br->m_v[0] <= uv[Y]) && (br->m_v[1] >= uv[Y])) {
			double dist = fabs(uv[X] - br->m_v[0]);
			if (!verticalTrim) { /* haven't seen vertical trim yet */
			    verticalTrim = true;
			    vdist = dist;
			    vclosest = br;
			} else {
			    if (dist < vdist) {
				vdist = dist;
				vclosest = br;
			    }
			}

		    }
		    continue;
		}
		double v;
		int trimstatus = br->isTrimmed(uv, v);
		if (v >= 0.0) {
		    if (closest && *closest == NULL) {
			currHeight = v;
			currTrimStatus = trimstatus;
			if (closest) {
			    *closest = br;
			}
		    } else if (v < currHeight) {
			currHeight = v;
			currTrimStatus = trimstatus;
			if (closest) {
			    *closest = br;
			}
		    }
		} else {
		    double dist = fabs(v);
		    if (!underTrim) {
			underTrim = true;
			udist = dist;
			uclosest = br;
		    } else {
			if (dist < udist) {
			    udist = dist;
			}
			uclosest = br;
		    }
		}
	    }
	    if (closest && *closest == NULL) {
		if (verticalTrim) {
		    closesttrim = vdist;
		    if (closest) {
			*closest = vclosest;
		    }
		}
		if ((underTrim) && (!verticalTrim || (udist < closesttrim))) {
		    closesttrim = udist;
		    if (closest) {
			*closest = uclosest;
		    }
		}
		return 1;
	    } else {
		closesttrim = currHeight;
		if ((verticalTrim) && (vdist < closesttrim)) {
		    closesttrim = vdist;
		    if (closest) {
			*closest = vclosest;
		    }
		}
		if ((underTrim) && (udist < closesttrim)) {
		    closesttrim = udist;
		    if (closest) {
			*closest = uclosest;
		    }
		}
		return currTrimStatus;
	    }
	}
    } else {
	if (m_trimmed) {
	    return 1;
	} else {
	    return 0;
	}
    }
}

template<class BV>
inline bool
BVNode<BV>::doTrimming() const
{
    return m_checkTrim;
}

template<class BV>
void
BVNode<BV>::getTrimsAbove(const ON_2dPoint &uv, std::list<BRNode *> &out_leaves) const
{
    point_t bmin, bmax;
    double dist;
    for (std::list<BRNode *>::const_iterator i = m_trims_above.begin(); i != m_trims_above.end(); i++) {
	BRNode *br = dynamic_cast<BRNode *>(*i);
	br->GetBBox(bmin, bmax);
	dist = 0.000001; /* 0.03*DIST_PT_PT(bmin, bmax); */
	if ((uv[X] > bmin[X] - dist) && (uv[X] < bmax[X] + dist)) {
	    out_leaves.push_back(br);
	}
    }
}

template<class BV>
void BVNode<BV>::BuildBBox()
{
    if (m_children.size() > 0) {
	for (std::vector<BBNode *>::iterator childnode = m_children.begin(); childnode != m_children.end(); childnode++) {
	    if (!(*childnode)->isLeaf()) {
		(*childnode)->BuildBBox();
	    }
	    if (childnode == m_children.begin()) {
		m_node = ON_BoundingBox((*childnode)->m_node.m_min, (*childnode)->m_node.m_max);
	    } else {
		for (int j = 0; j < 3; j++) {
		    if (m_node.m_min[j] > (*childnode)->m_node.m_min[j]) {
			m_node.m_min[j] = (*childnode)->m_node.m_min[j];
		    }
		    if (m_node.m_max[j] < (*childnode)->m_node.m_max[j]) {
			m_node.m_max[j] = (*childnode)->m_node.m_max[j];
		    }
		}
	    }
	}
    }
}

template<class BV>
bool
BVNode<BV>::prepTrims()
{
    CurveTree *ct = m_ctree;
    std::list<BRNode *>::iterator i;
    BRNode *br;
    point_t curvemin, curvemax;
    double dist = 0.000001;
    bool trim_already_assigned = false;

    m_trims_above.clear();

    if (LIKELY(ct != NULL)) {
	ct->getLeavesAbove(m_trims_above, m_u, m_v);
    }

    m_trims_above.sort(sortY);

    if (!m_trims_above.empty()) {
	i = m_trims_above.begin();
	while (i != m_trims_above.end()) {
	    br = dynamic_cast<BRNode *>(*i);
	    if (br->m_Vertical) { /* check V to see if trim possibly overlaps */
		br->GetBBox(curvemin, curvemax);
		if (curvemin[Y] - dist <= m_v[1]) {
		    /* possibly contains trim can't rule out check
		     * closer */
		    m_checkTrim = true;
		    trim_already_assigned = true;
		    i++;
		} else {
		    i = m_trims_above.erase(i);
		}
	    } else {
		i++;
	    }
	}
    }

    if (!trim_already_assigned) { /* already contains possible vertical trim */
	if (m_trims_above.empty() /*|| m_trims_right.empty()*/) {
	    m_trimmed = true;
	    m_checkTrim = false;
	} else if (!m_trims_above.empty()) { /*trimmed above check contains */
	    i = m_trims_above.begin();
	    br = dynamic_cast<BRNode *>(*i);
	    br->GetBBox(curvemin, curvemax);
	    dist = 0.000001; /* 0.03*DIST_PT_PT(curvemin, curvemax); */
	    if (curvemin[Y] - dist > m_v[1]) {
		i++;

		if (i == m_trims_above.end()) { /* easy only trim in above list */
		    if (br->m_XIncreasing) {
			m_trimmed = true;
			m_checkTrim = false;
		    } else {
			m_trimmed = false;
			m_checkTrim = false;
		    }
		} else {
		    /* check for trim bbox overlap TODO: look for
		     * multiple overlaps.
		     */
		    BRNode *bs;
		    bs = dynamic_cast<BRNode *>(*i);
		    point_t smin, smax;
		    bs->GetBBox(smin, smax);
		    if ((smin[Y] >= curvemax[Y]) || (smin[X] >= curvemax[X]) || (smax[X] <= curvemin[X])) { /* can determine inside/outside without closer inspection */
			if (br->m_XIncreasing) {
			    m_trimmed = true;
			    m_checkTrim = false;
			} else {
			    m_trimmed = false;
			    m_checkTrim = false;
			}
		    } else {
			m_checkTrim = true;
		    }
		}
	    } else {
		m_checkTrim = true;
	    }
	} else { /* something wrong here */
	    bu_log("Error prepping trims");
	    return false;
	}
    }
    return true;
}

/*--------------------------------------------------------------------------------
 * SurfaceTree declaration
 */
class BREP_EXPORT SurfaceTree {
private:
    bool m_removeTrimmed;

public:
    SurfaceTree();
    SurfaceTree(const ON_BrepFace *face, bool removeTrimmed = true, int depthLimit = BREP_MAX_FT_DEPTH);
    ~SurfaceTree();

    CurveTree *ctree;

    BBNode *getRootNode() const;

    /**
     * Calculate, using the surface bounding volume hierarchy, a uv
     * estimate for the closest point on the surface to the point in
     * 3-space.
     */
    ON_2dPoint getClosestPointEstimate(const ON_3dPoint &pt);
    ON_2dPoint getClosestPointEstimate(const ON_3dPoint &pt, ON_Interval &u, ON_Interval &v);

    /**
     * Return surface
     */
    const ON_Surface *getSurface();
    int getSurfacePoint(const ON_3dPoint &pt, ON_2dPoint &uv, const ON_3dPoint &from, double tolerance = BREP_SAME_POINT_TOLERANCE) const;

    /**
     * Return just the leaves of the surface tree
     */
    void getLeaves(std::list<BBNode *> &out_leaves);
    int depth();

private:
    bool isFlat(ON_Plane frames[]);
    bool isStraight(ON_Plane frames[]);
    bool isFlatU(ON_Plane frames[]);
    bool isFlatV(ON_Plane frames[]);
    BBNode *subdivideSurface(const ON_Surface *localsurf, const ON_Interval &u, const ON_Interval &v, ON_Plane frames[], int depth, int depthLimit, int prev_knot);
    BBNode *surfaceBBox(const ON_Surface *localsurf, bool leaf, ON_Plane frames[], const ON_Interval &u, const ON_Interval &v);

    const ON_BrepFace *m_face;
    BBNode *m_root;
    std::queue<ON_Plane *> f_queue;
};

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
    brlcad::SurfaceTree *surftree;
    std::list<ON_2dPointArray *> segments;
    const ON_BrepEdge *edge;
    bool order_reversed;
} PBCData;

extern BREP_EXPORT PBCData *
pullback_samples(const brlcad::SurfaceTree *surfacetree,
		 const ON_Curve *curve,
		 double tolerance = 1.0e-6,
		 double flatness = 1.0e-3);

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
class ON_CLASS BREP_EXPORT ON_PX_EVENT
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
 * @param pointA [in]
 * @param pointB [in]
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
 * @param surfaceA [in]
 * @param surfaceB [in]
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
 * @param a, b [in] either of them can be the larger one
 *
 * @return the result curve segment. NULL for error.
 */
extern BREP_EXPORT ON_Curve *
sub_curve(const ON_Curve *in, double a, double b);

/**
 * Get the sub-surface whose u \in [a,b] or v \in [a, b]
 *
 * @param in [in] the surface to split
 * @param dir [in] 0: u-split, 1: v-split
 * @param a, b [in] either of them can be the larger one
 *
 * @return the result sub-surface. NULL for error.
 */
extern BREP_EXPORT ON_Surface *
sub_surface(const ON_Surface *in, int dir, double a, double b);
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
