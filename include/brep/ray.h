/*                          R A Y . H
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

/** @addtogroup on_ray 
 *
 * @brief
 * Implement the concept of a geometry ray in terms of OpenNURBS
 * data types.
 *
 */
#ifndef BREP_RAY_H
#define BREP_RAY_H

#include "common.h"
#include "brep/defines.h"

#ifdef __cplusplus

#include "bn/dvec.h"

extern "C++" {

/** @{ */
/** @file brep/ray.h */

__BEGIN_DECLS

class plane_ray {
public:
    vect_t n1;
    fastf_t d1;

    vect_t n2;
    fastf_t d2;
};

/**
 * These definitions were added to opennurbs_curve.h - they are
 * extensions of openNURBS, so add them to libbrep instead.
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
     *      * Intersect two 2d Rays
     *           * @param v [in] other ray to intersect with
     *                * @param isect [out] point of intersection
     *                     * @return true if single point of intersection is found
     *                          */
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

BREP_EXPORT int brep_get_plane_ray(const ON_Ray &r, plane_ray &pr);
BREP_EXPORT void brep_r(const ON_Surface* surf, const plane_ray& pr, pt2d_t uv, ON_3dPoint& pt, ON_3dVector& su, ON_3dVector& sv, pt2d_t R);
BREP_EXPORT void brep_newton_iterate(const plane_ray& pr, pt2d_t R, const ON_3dVector& su, const ON_3dVector& sv, pt2d_t uv, pt2d_t out_uv);
BREP_EXPORT void utah_ray_planes(const ON_Ray &r, ON_3dVector &p1, double &p1d, ON_3dVector &p2, double &p2d);

__END_DECLS

} /* extern C++ */
#endif

/** @} */

#endif  /* BREP_RAY_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
