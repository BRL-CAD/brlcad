/*                        S K E T C H _ T E S S . C
 * BRL-CAD
 *
 * Copyright (c) 2012 United States Government as represented by
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
/** @addtogroup primitives */
/** @{ */
/** @file primitives/sketch/sketch_tess.c
 *
 * Provide support for tesselation of sketch primitive
 * including approximation of bezier/NURBS curves by
 * circular arcs
 *
 */
/** @} */

#include "common.h"

#include <vector>

#include "raytrace.h"
#include "rtgeom.h"
#include "brep.h"


#define CROSSPRODUCT2D(u, v) (u).x * (v).y - (u).y * (v).x

/* intersect two 2d rays, defined by a point of origin and a vector. return the
 * point of intersection in isect.
 * -1: FAIL
 *  0: OKAY
 */
int
intersect_2dRay(const ON_2dPoint p, const ON_2dPoint q, const ON_2dVector u, const ON_2dVector v, ON_2dPoint *isect)
{
    fastf_t uxv, q_pxv;
    /* check for parallel and collinear cases */
    if (ZERO((uxv = CROSSPRODUCT2D(u, v)))
        || (ZERO((q_pxv = CROSSPRODUCT2D(q - p, v))))) {
        return -1;
    }

    *isect = p + (u * q_pxv / uxv);
    return 0;
}


/* returns the incenter of the inscribed circle inside the triangle defined by
 * points a, b, c */
ON_2dPoint
incenter(const ON_2dPoint a, const ON_2dPoint b, const ON_2dPoint c)
{
    fastf_t a_b, a_c, b_c, sum;
    ON_2dPoint incenter;

    a_b = a.DistanceTo(b);
    a_c = a.DistanceTo(c);
    b_c = b.DistanceTo(c);
    sum = a_b + a_c + b_c;

    incenter.x = (b_c * a.x + a_c * b.x + a_b * c.x) / sum;
    incenter.y = (b_c * a.y + a_c * b.y + a_b * c.y) / sum;
    return incenter;
}

/* create a biarc for a bezier curve.
 *
 * extends the tangent lines to the bezier curve at its first and last control
 * points and finds the incenter of the circle defined by the first, last
 * control points and the intersection of the two tangents.
 * the biarc is defined by the first and last control points, and the incenter */
ON_Arc
make_biarc(const ON_BezierCurve bezier)
{
    ON_2dPoint start, end, arc_pt, isect;
    ON_2dVector t1, t2;

    start = bezier.PointAt(0.0);
    end = bezier.PointAt(1.0);

    t1 = bezier.TangentAt(0.0);
    t2 = bezier.TangentAt(1.0);

    intersect_2dRay(start, end, t1, t2, &isect);
    arc_pt = incenter(start, end, isect);

    return ON_Arc(start, arc_pt, end);
}


/* computes the first point of inflection on a bezier if it exists by finding
 * where the magnitude of the curvature vector (2nd derivative) is at a maximum
 * -1: FAIL
 *  0: OKAY
 */
int
bezier_inflection(const ON_BezierCurve bezier, fastf_t *inflection)
{
    fastf_t t, step = 0.1;
    for (t = 0; t <= 1.0; t += step) {
        ON_2dVector crv_1, crv_2;
        crv_1 = bezier.CurvatureAt(t);
        crv_2 = bezier.CurvatureAt(t + (step * 0.5));
        // compare magnitude of curvature vectors
        if (crv_1.LengthSquared() > crv_2.LengthSquared()) {
            *inflection = t;
            return 0;
        }
    }
    return -1;
}


/* approximates a bezier curve with a set of circular arcs by dividing where
 * the bezier deviates from the approximation biarc by more than a given
 * tolerance, and recursively calling on the sub-sections until it is
 * approximated to tolerance by the biarc */
void
approx_bezier(const ON_BezierCurve bezier, const ON_Arc biarc, const struct bn_tol *tol, std::vector<ON_Arc>& approx)
{
    fastf_t t, step = 0.1;
    ON_3dPoint test;
    ON_BezierCurve head, tail;
    // walk the bezier curve at interval given by step
    for (t = 0; t <= 1.0; t += step) {
        test = bezier.PointAt(t);
        // compare distance from arc to the bezier evaluated at 't' to the
        // given tolerance
        if ((test - biarc.ClosestPointTo(test)).LengthSquared() > tol->dist_sq) {
            bezier.Split(step, head, tail);
            approx_bezier(head, make_biarc(head), tol, approx);
            approx_bezier(tail, make_biarc(tail), tol, approx);
        }
    }
    // bezier doesn't deviate from biarc by the given tolerance, add the biarc
    // approximation
    approx.push_back(biarc);
}


/* approximates a bezier curve with a set of circular arcs */
void
bezier_to_carcs(const ON_BezierCurve bezier, const struct bn_tol *tol, std::vector<ON_Arc>& carcs)
{
    bool curvature_changed = false;
    fastf_t inflection_pt, biarc_angle;
    ON_Arc biarc;
    ON_BezierCurve current, tmp;
    std::vector<ON_BezierCurve> rest;

    // get inflection point, if it exists
    if (bezier_inflection(bezier, &inflection_pt)) {
        current = bezier;
    } else {
        curvature_changed = true;
        bezier.Split(inflection_pt, current, tmp);
        rest.push_back(tmp);
    }

    do {
    biarc = make_biarc(current);
    if ((biarc_angle = biarc.AngleRadians()) <= M_PI_2) {
        // approximate the current bezier segment and add its biarc
        // approximation to carcs
        approx_bezier(current, biarc, tol, carcs);
    } else if (biarc_angle <= M_PI) {
        // divide the current bezier segment in half
        current.Split(0.5, current, tmp);
        rest.push_back(tmp);

        approx_bezier(current, biarc, tol, carcs);
        // get next bezier section and pop it
        current = rest.back();
        rest.pop_back();

        approx_bezier(current, biarc, tol, carcs);
    } else {
        fastf_t t = 1.0;
        ON_Arc test_biarc;
        ON_BezierCurve test_bezier;
        // divide the current bezier such that the first curve segment would
        // have an approximating biarc segment <=90 degrees
        do {
            t *= 0.5;
            current.Split(t, test_bezier, tmp);
            test_biarc = make_biarc(test_bezier);
        } while(test_biarc.AngleRadians() > M_PI_2);

        rest.push_back(tmp);

        approx_bezier(test_bezier, test_biarc, tol, carcs);

        current = rest.back();
        rest.pop_back();
        continue;
    }

    if (curvature_changed) {
        curvature_changed = false;
        current = rest.back();
        rest.pop_back();
        // dont check while() condition, we want to continue even if we just
        // popped the last element
        continue;
    }
    } while (!rest.empty());
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
