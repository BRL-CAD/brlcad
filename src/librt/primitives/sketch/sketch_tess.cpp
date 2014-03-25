/*                        S K E T C H _ T E S S . C
 * BRL-CAD
 *
 * Copyright (c) 2012-2014 United States Government as represented by
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
 * An extension of sketch.c for functions using the openNURBS library. Includes
 * support for approximation of Bezier curves using circular arcs.
 */
/** @} */

/* NOTE: Current Bezier approximation routine is meant to handle cubic and lower
 * degree Bezier curves, and may not work properly for higher degrees.
 *
 * TODO:
 * rt_sketch_surf_area: add logic to determine if part of a sketch
 * "removes" area from the total area, e.g. a sketch of a circumscribed square
 * has area less than the sum of the areas of the square and circle.
 *
 * rt_sketch_tess: implement this.
 */

#include "common.h"

#include <vector>

#include "raytrace.h"
#include "rtgeom.h"
#include "vmath.h"
#include "brep.h"


/* returns the incenter of the inscribed circle inside the triangle defined by
 * points a, b, c
 */
HIDDEN ON_2dPoint
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
HIDDEN ON_Arc
make_biarc(const ON_BezierCurve& bezier)
{
    ON_2dPoint isect, arc_pt;
    ON_2dPoint p_start(bezier.PointAt(0)), p_end(bezier.PointAt(1.0));
    ON_2dVector t_start(bezier.TangentAt(0)), t_end(bezier.TangentAt(1.0));
    ON_Ray r_start(p_start, t_start), r_end(p_end, t_end);

    r_start.IntersectRay(r_end, isect);
    arc_pt = incenter(p_start, p_end, isect);

    return ON_Arc(p_start, arc_pt, p_end);
}


/* NOTE: MINSTEP and MAXSTEP were determined by experimentation. If MINSTEP is
 * much smaller (i.e. 0.00001), approx_bezier() slows significantly on curves with
 * high curvature over a large part of its domain. MAXSTEP represents a step
 * size of 1/10th the domain of a Bezier curve, and MINSTEP represents 1/10000th.
 */
#define MINSTEP 0.0001
#define MAXSTEP 0.1

#define GETSTEPSIZE(step) ((step) > MAXSTEP ? MAXSTEP : ((step) < MINSTEP ? MINSTEP : (step)))

#define POW3(x) ((x)*(x)*(x))
#define SIGN(x) ((x) >= 0 ? 1 : -1)
#define CURVATURE(d1, d2) (V2CROSS((d1), (d2)) / POW3((d1).Length()))

/* find a point of inflection on a bezier curve, if it exists, by finding the
 * value of parameter 't' where the signed curvature of the bezier changes
 * signs. Returns true if an inflection point is found.
 */
HIDDEN bool
bezier_inflection(const ON_BezierCurve& bezier, fastf_t& inflection_pt)
{
    int sign;
    fastf_t t, step, crv;
    ON_3dVector d1, d2; // first derivative, second derivative
    ON_3dPoint tmp;     // not used, but needed by Ev2Der

    // calculate curvature at t=0
    bezier.Ev2Der(0, tmp, d1, d2);
    crv = CURVATURE(d1, d2);
    // step size decreases as |crv| -> 0
    step = GETSTEPSIZE(fabs(crv));

    sign = SIGN(crv);

    for (t = step; t <= 1.0; t += step) {
	bezier.Ev2Der(t, tmp, d1, d2);
	crv = CURVATURE(d1, d2);
	// if sign changes, t is an inflection point
	if (sign != SIGN(crv)) {
	    inflection_pt = t;
	    return true;
	}
	step = GETSTEPSIZE(fabs(crv));
    }
    return false;
}


/* approximates a bezier curve with a set of circular arcs by dividing where
 * the bezier's deviation from its approximating biarc is at a maximum, then
 * recursively calling on the subsections until it is approximated to
 * tolerance by the biarc
 */
HIDDEN void
approx_bezier(const ON_BezierCurve& bezier, const ON_Arc& biarc, const struct bn_tol *tol, std::vector<ON_Arc>& approx)
{
    fastf_t t = 0.0, step = 0.0;
    fastf_t crv = 0.0, err = 0.0, max_t = 0.0, max_err = 0.0;
    ON_3dPoint test;
    ON_3dVector d1, d2;

    // walk the bezier curve at interval given by step
    for (t = 0; t <= 1.0; t += step) {
	bezier.Ev2Der(t, test, d1, d2);
	err = fabs((test - biarc.Center()).Length() - biarc.Radius());
	// find the maximum point of deviation
	if (err > max_err) {
	    max_t = t;
	    max_err = err;
	}
	crv = CURVATURE(d1, d2);
	// step size decreases as |crv| -> 1
	step = GETSTEPSIZE(1.0 - fabs(crv));
    }

    if (max_err + VDIVIDE_TOL < tol->dist) {
	// max deviation is less than the given tolerance, add the biarc approximation
	approx.push_back(biarc);
    } else {
	ON_BezierCurve head, tail;
	// split bezier at point of maximum deviation and recurse on the new subsections
	bezier.Split(max_t, head, tail);
	approx_bezier(head, make_biarc(head), tol, approx);
	approx_bezier(tail, make_biarc(tail), tol, approx);
    }
}


/* approximates a bezier curve with a set of circular arcs.
 * returns approximation in carcs
 */
HIDDEN void
bezier_to_carcs(const ON_BezierCurve& bezier, const struct bn_tol *tol, std::vector<ON_Arc>& carcs)
{
    bool skip_while = true, curvature_changed = false;
    fastf_t inflection_pt, biarc_angle;
    ON_Arc biarc;
    ON_BezierCurve current, next;
    std::vector<ON_BezierCurve> rest;

    // find inflection point, if it exists
    if (bezier_inflection(bezier, inflection_pt)) {
	curvature_changed = true;
	bezier.Split(inflection_pt, current, next);
	rest.push_back(next);
    } else {
	current = bezier;
    }

    while (skip_while || !rest.empty()) {
    if (skip_while) skip_while = false;
    biarc = make_biarc(current);
    if ((biarc_angle = biarc.AngleRadians()) <= M_PI_2) {
	// approximate the current bezier segment and add its biarc
	// approximation to carcs
	approx_bezier(current, biarc, tol, carcs);
    } else if (biarc_angle <= M_PI) {
	// divide the current bezier segment in half
	current.Split(0.5, current, next);
	// approximate first bezier segment
	approx_bezier(current, biarc, tol, carcs);
	// approximate second bezier segment
	approx_bezier(next, biarc, tol, carcs);
    } else {
	fastf_t t = 1.0;
	ON_Arc test_biarc;
	ON_BezierCurve test_bezier;
	// divide the current bezier such that the first curve segment would
	// have an approximating biarc segment <=90 degrees
	do {
	    t *= 0.5;
	    current.Split(t, test_bezier, next);
	    test_biarc = make_biarc(test_bezier);
	} while(test_biarc.AngleRadians() > M_PI_2);

	approx_bezier(test_bezier, test_biarc, tol, carcs);
	current = next;
	skip_while = true;
	continue;
    }

    if (curvature_changed) {
	curvature_changed = false;
	current = rest.back();
	rest.pop_back();
	// continue even if we just popped the last element
	skip_while = true;
    }
    }
}


#define SKETCH_PT(idx) sketch_ip->verts[(idx)]

#define DIST_PT2D_PT2D_SQ(_p1, _p2) \
    (((_p2)[X] - (_p1)[X]) * ((_p2)[X] - (_p1)[X]) + \
    ((_p2)[Y] - (_p1)[Y]) * ((_p2)[Y] - (_p1)[Y]))

#define DIST_PT2D_PT2D(_p1, _p2) sqrt(DIST_PT2D_PT2D_SQ(_p1, _p2))

/**
 * calculate approximate surface area for a sketch primitive by iterating through
 * each curve segment in the sketch, calculating the area of the polygon
 * created by the start and end points of each curve segment, as well as the
 * additional areas for circular segments.
 *
 * line_seg: calculate the area for the polygon edge Start->End
 * carc_seg: if the segment is a full circle, calculate its area.
 *      else, calculate the area for the polygon edge Start->End, and the area
 *      of the circular segment
 * bezier_seg: approximate the bezier using the bezier_to_carcs() function. for
 *      each carc_seg, calculate the area for the polygon edges Start->End and
 *      the area of the circular segment
 */
extern "C" void
rt_sketch_surf_area(fastf_t *area, const struct rt_db_internal *ip)
{
    int j;
    size_t i;
    fastf_t poly_area = 0, carc_area = 0;

    struct bn_tol tol;
    struct rt_sketch_internal *sketch_ip = (struct rt_sketch_internal *)ip->idb_ptr;
    RT_SKETCH_CK_MAGIC(sketch_ip);
    struct rt_curve crv = sketch_ip->curve;

    // a sketch with no curves has no area
    if (UNLIKELY(crv.count == 0)) {
	return;
    }

    tol.magic = BN_TOL_MAGIC;
    tol.dist = RT_DOT_TOL;
    tol.dist_sq = RT_DOT_TOL * RT_DOT_TOL;

    // iterate through each curve segment in the sketch
    for (i = 0; i < crv.count; i++) {
	const struct line_seg *lsg;
	const struct carc_seg *csg;
	const struct bezier_seg *bsg;

	const uint32_t *lng = (uint32_t *)crv.segment[i];

	switch (*lng) {
	case CURVE_LSEG_MAGIC:
	    lsg = (struct line_seg *)lng;
	    // calculate area for polygon edge
	    poly_area += V2CROSS(SKETCH_PT(lsg->start), SKETCH_PT(lsg->end));
	    break;
	case CURVE_CARC_MAGIC:
	    csg = (struct carc_seg *)lng;
	    if (csg->radius < 0) {
		// calculate full circle area
		carc_area += M_PI * DIST_PT2D_PT2D_SQ(SKETCH_PT(csg->start), SKETCH_PT(csg->end));
	    } else {
		fastf_t theta, side_ratio;
		// calculate area for polygon edge
		poly_area += V2CROSS(SKETCH_PT(csg->start), SKETCH_PT(csg->end));
		// calculate area for circular segment
		side_ratio = DIST_PT2D_PT2D(SKETCH_PT(csg->start), SKETCH_PT(csg->end)) / (2.0 * csg->radius);
		theta = asin(side_ratio);
		carc_area += 0.5 * csg->radius * csg->radius * (theta - side_ratio);
	    }
	    break;
	case CURVE_BEZIER_MAGIC: {
	    bsg = (struct bezier_seg *)lng;
	    ON_2dPointArray bez_pts(bsg->degree + 1);
	    std::vector<ON_Arc> carcs;
	    // convert struct bezier_seg into ON_BezierCurve
	    for (j = 0; j < bsg->degree + 1; j++) {
		bez_pts.Append(SKETCH_PT(bsg->ctl_points[j]));
	    }
	    // approximate bezier curve by a set of circular arcs
	    bezier_to_carcs(ON_BezierCurve(bez_pts), &tol, carcs);
	    for (std::vector<ON_Arc>::iterator it = carcs.begin(); it != carcs.end(); ++it) {
		// calculate area for each polygon edge
		poly_area += V2CROSS(it->StartPoint(), it->EndPoint());
		// calculate area for each circular segment
		carc_area += 0.5 * it->Radius() * it->Radius() * (it->AngleRadians() - sin(it->AngleRadians()));
	    }
	    }
	    break;
	case CURVE_NURB_MAGIC:
	default:
	    break;
	}
    }
    *area = 0.5 * fabs(poly_area) + carc_area;
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
