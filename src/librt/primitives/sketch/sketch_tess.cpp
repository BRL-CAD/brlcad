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

#include <algorithm>

#include "raytrace.h"
#include "../../opennurbs_ext.h"


/* returns the incenter of the inscribed circle inside the triangle defined by
 * points a, b, c
 */
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
 * points, and intersects them to find a third point.
 * the biarc passes through the first and last control points, and the incenter
 * of the circle defined by the first, last and intersection points.
 */
ON_Arc
make_biarc(const ON_BezierCurve bezier)
{
    ON_2dPoint isect, arc_pt;
    ON_2dPoint p_start(bezier.PointAt(0)), p_end(bezier.PointAt(1.0));
    ON_2dVector t_start(bezier.TangentAt(0)), t_end(bezier.TangentAt(1.0));
    ON_Ray r_start(p_start, t_start), r_end(p_end, t_end);

    r_start.IntersectRay(r_end, isect);
    arc_pt = incenter(p_start, p_end, isect);

    return ON_Arc(p_start, arc_pt, p_end);
}


/* computes the first point of inflection on a bezier if it exists by finding
 * where the magnitude of the curvature vector (2nd derivative) is at a
 * maximum.
 * Returns true if an inflection point was found
 */
bool
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
            return true;
        }
    }
    return false;
}


/* approximates a bezier curve with a set of circular arcs by dividing where
 * the bezier deviates from the approximation biarc by more than a given
 * tolerance, and recursively calling on the sub-sections until it is
 * approximated to tolerance by the biarc
 */
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
 * returns approximation in carcs
 */
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
        curvature_changed = true;
        bezier.Split(inflection_pt, current, tmp);
        rest.push_back(tmp);
    } else {
        current = bezier;
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

#define SKETCHPT(idx) sketch_ip->verts[(idx)]

inline fastf_t
carc_area(const struct carc_seg *csg, const struct rt_sketch_internal *sketch_ip)
{
    fastf_t theta, side_ratio;
    side_ratio = DIST_PT_PT(SKETCHPT(csg->start), SKETCHPT(csg->end)) / (2.0 * csg->radius);
    theta = asin(side_ratio);
    return 0.5 * csg->radius * csg->radius * (theta - side_ratio);
}


/**
 * R T _ S K E T C H _ S U R F _ A R E A
 *
 * calculate approximate surface area for a sketch primitive by iterating through
 * each curve segment in the sketch, calculating the area of the polygon
 * created by the start and end points of each curve segment, as well as the
 * additional areas for circular segments.

 * line_seg: calculate the area for the polygon edge Start->End
 * carc_seg: if the segment is a full circle, calculate its area.
 *      else, calculate the area for the polygon edge Start->End, and the area
 *      of the circular segment
 * bezier_seg: approximate the bezier using the bezier_to_carcs() function. for
 *      each carc_seg, calculate the area for the polygon edge Start->End, and
 *      the area of the circular segment
 */
extern "C" void
rt_sketch_surf_area(fastf_t *area, const struct rt_db_internal *ip) //, const struct bn_tol *tol)
{
    size_t i;
    fastf_t full_circle_area = 0.0;

    struct rt_sketch_internal *sketch_ip = (struct rt_sketch_internal *)ip->idb_ptr;
    RT_SKETCH_CK_MAGIC(sketch_ip);
    struct rt_curve crv = sketch_ip->curve;

    struct bn_tol tol;
    tol.magic = BN_TOL_MAGIC;
    tol.dist = RT_DOT_TOL;
    tol.dist_sq = RT_DOT_TOL * RT_DOT_TOL;

    // a sketch with no curves has no area
    if (crv.count == 0) {
        return;
    }

    // iterate through each curve segment in the sketch
    for (i = 0; i < crv.count; i++) {
        const struct line_seg *lsg;
        const struct carc_seg *csg;
        const struct bezier_seg *bsg;
        const uint32_t *lng;

        lng = (uint32_t *)crv.segment[i];

        switch (*lng) {
        case CURVE_LSEG_MAGIC:
            lsg = (struct line_seg *)lng;
            // calculate area for polygon edge
            *area += V2CROSS(SKETCHPT(lsg->start), SKETCHPT(lsg->end));
            break;
        case CURVE_CARC_MAGIC:
            csg = (struct carc_seg *)lng;
            if (csg->radius < 0) {
                // calculate full circle area
                full_circle_area += M_PI * DIST_PT_PT_SQ(SKETCHPT(csg->start), SKETCHPT(csg->end));
            } else {
                // calculate area for polygon edge
                *area += V2CROSS(SKETCHPT(csg->start), SKETCHPT(csg->end));
                // calculate area for circular segment
                *area += carc_area(csg, sketch_ip);
            }
            break;
        case CURVE_BEZIER_MAGIC: {
            bsg = (struct bezier_seg *)lng;
            ON_2dPointArray bez_pts(bsg->degree + 1);
            std::vector<ON_Arc> carcs;
            // convert struct bezier_seg into ON_BezierCurve
            for (i = 0; (int)i < bsg->degree + 1; i++) {
                bez_pts[i] = SKETCHPT(bsg->ctl_points[i]);
            }
            // approximate bezier curve by a set of circular arcs
            bezier_to_carcs(ON_BezierCurve(bez_pts), &tol, carcs);
            for (std::vector<ON_Arc>::iterator it = carcs.begin(); it != carcs.end(); ++it) {
                // calculate area for polygon edges Start->Mid, Mid->End
                *area += V2CROSS(it->StartPoint(), it->MidPoint());
                *area += V2CROSS(it->MidPoint(), it->EndPoint());
                // calculate area for circular segment
                *area += 0.5 * it->Radius() * it->Radius() * (it->AngleRadians() - sin(it->AngleRadians()));
            }
            break;
            }
        case CURVE_NURB_MAGIC:
        default:
            break;
        }
    }
    *area = fabs(*area) * 0.5 + full_circle_area;
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
