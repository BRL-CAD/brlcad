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

#include "raytrace.h"
#include "rtgeom.h"
#include "brep.h"


fastf_t
CrossProduct2d(ON_2dVector u, ON_2dVector v)
{
    return u.x * v.y - u.y * v.x;
}


/* intersect two 2d rays, given by a point of origin and a vector. return the
 * point of intersection in isect.
 * -1: FAIL
 *  0: OKAY
 */
int
intersect_2dRay(ON_2dPoint p, ON_2dPoint q, ON_2dVector u, ON_2dVector v, ON_2dPoint *isect)
{
    fastf_t uxv, q_pxv;

    /* check for parallel and collinear cases */
    if (ZERO((uxv = CrossProduct2d(u, v)))) {
        return -1;
    } else if (ZERO((q_pxv = CrossProduct2d(q - p, v)))) {
        return -1;
    }

    *isect = p + (u * q_pxv / uxv);
    return 0;
}


/* returns the incenter of the inscribed circle inside the triangle formed by
 * points a, b, c */
ON_2dPoint
incenter(ON_2dPoint a, ON_2dPoint b, ON_2dPoint c)
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

int
biarc(ON_BezierCurve bezier, ON_Arc *biarc)
{
    ON_2dPoint p1, p2, p3, isect;
    ON_2dVector t1, t2;

    p1 = bezier.PointAt(0.0);
    p2 = bezier.PointAt(1.0);

    t1 = bezier.TangentAt(0.0);
    t2 = bezier.TangentAt(1.0);

    if (intersect_2dRay(p1, p2, t1, t2, &isect) < 0) {
        return -1;
    }

    p3 = incenter(p1, p2, isect);

    *biarc = ON_Arc(p1, p2, p3);
    return 0;
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
