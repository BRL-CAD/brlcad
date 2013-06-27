/*         T E S T _ C U R V E _ I N T E R S E C T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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
/** @file test_curve_intersect.cpp
 *
 * Do some test on CCI and CSI.
 *
 */

#include "common.h"

#include "vmath.h"
#include "bu.h"

#include "brep.h"

void
test_cci(ON_Curve *c1, ON_Curve *c2)
{
    ON_wString wstr;
    ON_TextLog textlog(wstr);
    ON_SimpleArray<ON_X_EVENT> x;
    // Use default tolerance
    ON_Intersect(c1, c2, x);

    if (x.Count() == 0) {
	bu_log("No intersection.\n");
    } else {
	for (int i = 0; i < x.Count(); i++)
	    x[i].Dump(textlog);
	ON_String str(wstr);
	bu_log(str.Array());
    }
    bu_log("\n\n");
}


int
main(int, char**)
{
    ON_3dPoint origin(0.0, 0.0, 0.0);
    ON_3dVector xdir(1.0, 0.0, 0.0);
    ON_3dVector ydir(0.0, 1.0, 1.0);
    ydir.Unitize();
    ON_Plane plane(origin, xdir, ydir);
    double radius = 10.0;

    // curve-curve intersection
    bu_log("*** Curve-curve intersection ***\n");

    bu_log("Test 1:\n");
    // circle A is a circle fixed at the origin
    ON_Circle circleA = ON_Circle(plane, origin, radius);
    ON_NurbsCurve *curveA = ON_NurbsCurve::New();
    circleA.GetNurbForm(*curveA);

    // We start circle B from somewhere that it doesn't intersect with
    // circleA, and move it closer and closer (along the x-axis). Then
    // it should first circumscribe with circleA (one intersection point),
    // then intersect (two points), coincident (overlap), intersect (two
    // points), circumscribe again, and finally depart from circleA.

    ON_3dPoint start(-2.5*radius, 0.0, 0.0);	// the starting center of circleB
    ON_3dPoint end(2.5*radius, 0.0, 0.0);	// the end center of circleB
    ON_3dVector move_dir = end - start;
    for (int i = 0; i <= 50.0; i++) {
	ON_3dPoint centerB = start + move_dir*((double)i/50.0);
	ON_Circle circleB(plane, centerB, radius);
	ON_NurbsCurve *curveB = ON_NurbsCurve::New();
	circleB.GetNurbForm(*curveB);
	bu_log("Center of circleB: (%lf,%lf,%lf):\n", centerB.x, centerB.y, centerB.z);
	test_cci(curveA, curveB);
	delete curveB;
    }
    delete curveA;

    bu_log("Test 2:\n");
    // Test the merge correctness of overlap events.
    ON_3dPointArray ptarrayA, ptarrayB;
    ptarrayA.Append(ON_3dPoint(0.0, -1.0, 0.0));
    ptarrayA.Append(ON_3dPoint(0.0, 1.0, 0.0));
    ptarrayA.Append(ON_3dPoint(-1.0, 0.0, 0.0));
    ptarrayA.Append(ON_3dPoint(1.0, 0.0, 0.0));
    ptarrayB.Append(ON_3dPoint(0.0, -2.0, 0.0));
    ptarrayB.Append(ON_3dPoint(0.0, 0.0, 0.0));
    ptarrayB.Append(ON_3dPoint(2.0, 0.0, 0.0));
    ON_PolylineCurve polyA(ptarrayA), polyB(ptarrayB);
    curveA = ON_NurbsCurve::New();
    ON_NurbsCurve *curveB = ON_NurbsCurve::New();
    polyA.GetNurbForm(*curveA);
    polyB.GetNurbForm(*curveB);
    test_cci(curveA, curveB);
    bu_log("%lf %lf %lf %lf\n", curveA->Domain().Min(), curveA->Domain().Max(), curveB->Domain().Min(), curveB->Domain().Max());
    delete curveA;
    delete curveB;

    // curve-surface intersections to be implemented.

    bu_log("All finished.\n");
    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
