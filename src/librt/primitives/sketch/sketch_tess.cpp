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
#include "../../opennurbs_ext.h"

#define CROSSPRODUCT2D(u, v) (u).x * (v).y - (u).y * (v).x

/* intersect two 2d rays. return the point of intersection in isect.
 * -1: FAIL
 *  0: OKAY
 */
int
intersect_2dRay(const ON_Ray u, const ON_Ray v, ON_2dPoint& isect)
//intersect_2dRay(const ON_2dPoint p, const ON_2dPoint q, const ON_2dVector u, const ON_2dVector v, ON_2dPoint *isect)
{
    fastf_t uxv, q_pxv;
    /* check for parallel and collinear cases */
    if (ZERO((uxv = CROSSPRODUCT2D(u.m_dir, v.m_dir)))
        || (ZERO((q_pxv = CROSSPRODUCT2D(v.m_origin - u.m_origin, v.m_dir))))) {
        return -1;
    }

    isect = u.PointAt(q_pxv / uxv);
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
    ON_2dPoint isect, arc_pt;
    ON_2dPoint start(bezier.PointAt(0)), end(bezier.PointAt(1.0));
    ON_2dVector t_start(bezier.TangentAt(0)), t_end(bezier.TangentAt(1.0));
    ON_Ray r_start((ON_3dPoint&)start, (ON_3dVector&)t_start),
           r_end((ON_3dPoint&)end, (ON_3dVector&)t_end);

    intersect_2dRay(r_start, r_end, isect);
    arc_pt = incenter(start, end, isect);

    return ON_Arc(start, arc_pt, end);
}


/* computes the first point of inflection on a bezier if it exists by finding
 * where the magnitude of the curvature vector (2nd derivative) is at a maximum
 * -1: FAIL
 *  0: OKAY
 */
int
bezier_inflection(const ON_BezierCurve bezier, fastf_t& inflection)
{
    fastf_t t, step = 0.1;
    for (t = 0; t <= 1.0; t += step) {
        ON_2dVector crv_1, crv_2;
        crv_1 = bezier.CurvatureAt(t);
        crv_2 = bezier.CurvatureAt(t + (step * 0.5));
        // compare magnitude of curvature vectors
        if (crv_1.LengthSquared() > crv_2.LengthSquared()) {
            inflection = t;
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


/* approximates a bezier curve with a set of circular arcs.
 * returns approximation in carcs */
void
bezier_to_carcs(const ON_BezierCurve bezier, const struct bn_tol *tol, std::vector<ON_Arc>& carcs)
{
    bool curvature_changed = false;
    fastf_t inflection_pt, biarc_angle;
    ON_Arc biarc;
    ON_BezierCurve current, tmp;
    std::vector<ON_BezierCurve> rest;

    // get inflection point, if it exists
    if (bezier_inflection(bezier, inflection_pt)) {
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
        // approximate first bezier segment
        approx_bezier(current, biarc, tol, carcs);
        // get next bezier section and pop it
        current = rest.back();
        rest.pop_back();
        // approximate second bezier segment
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


extern "C" fastf_t
carc_area(const struct carc_seg *csg, const struct rt_sketch_internal *sk)
{
    fastf_t theta, side_ratio;
    side_ratio = DIST_PT_PT(sk->verts[csg->start], sk->verts[csg->end]) / (2.0 * csg->radius);
    theta = asin(side_ratio);
    return 0.5 * csg->radius * csg->radius * (theta - side_ratio);
}


void
rt_sketch_surf_area(fastf_t *area, const struct rt_sketch_internal *sketch_ip, const struct bn_tol *tol)
{
    size_t i;
    struct rt_curve crv = sketch_ip->curve;
    ON_3dVector sketch_normal, sum;
    std::vector<ON_2dPoint> verts;

    if (crv.count == 0) {
        return;
    }

    for (i = 0; i < crv.count; i++) {
        const struct line_seg *lsg;
        const struct carc_seg *csg;
        const struct bezier_seg *bsg;
        //const struct nurb_seg *nsg;
        const uint32_t *lng;

        lng = (uint32_t *)crv.segment[i];

        switch (*lng) {
        case CURVE_LSEG_MAGIC:
            lsg = (struct line_seg *)lng;
            verts.push_back(sketch_ip->verts[lsg->start]);
            verts.push_back(sketch_ip->verts[lsg->end]);
            break;
        case CURVE_CARC_MAGIC:
            csg = (struct carc_seg *)lng;
            verts.push_back(sketch_ip->verts[csg->start]);
            verts.push_back(sketch_ip->verts[csg->end]);
            *area += carc_area(csg, sketch_ip);
            break;
        case CURVE_BEZIER_MAGIC:
            bsg = (struct bezier_seg *)lng;
            ON_2dPointArray bez_pts(bsg->degree + 1);
            ON_BezierCurve bezier_curve;
            std::vector<ON_Arc> carcs;
            for (i = 0; (int)i < bsg->degree + 1; i++) {
                bez_pts[i] = sketch_ip->verts[bsg->ctl_points[i]];
            }
            bezier_curve = ON_BezierCurve(bez_pts);
            bezier_to_carcs(bezier_curve, tol, carcs);
            for (std::vector<ON_Arc>::iterator it = carcs.begin(); it != carcs.end(); ++it) {
                verts.push_back(it->StartPoint());
                verts.push_back(it->MidPoint());
                verts.push_back(it->EndPoint());
            }
            break;
        //case CURVE_NURB_MAGIC:
        //default:
            //break;
        }
    }

    for (std::vector<ON_2dPoint>::iterator it = verts.begin(); it != verts.end(); ++it) {
        sum += ON_CrossProduct((ON_2dVector&)*it, (ON_2dVector&)*(it + 1 == verts.end() ? verts.begin() : it + 1));
    }

    //for (i = 0; i < verts.size(); i++) {
        //sum += ON_CrossProduct((ON_2dVector&)verts[i], (ON_2dVector&)verts[i + 1 == verts.size() ? 0 : i + 1]);
    //}
    *area += fabs(ON_DotProduct(sketch_normal, sum)) / 2.0;
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
