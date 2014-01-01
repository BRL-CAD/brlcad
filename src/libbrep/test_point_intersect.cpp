/*         T E S T _ P O I N T _ I N T E R S E C T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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
/** @file test_point_intersect.cpp
 *
 * Do some test on PPI, PCI and PSI.
 *
 */

#include "common.h"

#include "vmath.h"
#include "bu.h"

#include "brep.h"

void
test_ppi(ON_3dPoint &p1, ON_3dPoint &p2)
{
    ON_wString wstr;
    ON_TextLog textlog(wstr);
    ON_ClassArray<ON_PX_EVENT> x;
    // Use default tolerance
    ON_Intersect(p1, p2, x);
    bu_log("(%f,%f,%f) and (%f,%f,%f):\n", p1[0], p1[1], p1[2], p2[0], p2[1], p2[2]);
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

void
test_pci(ON_3dPoint &p, ON_Curve &c)
{
    ON_wString wstr;
    ON_TextLog textlog(wstr);
    ON_ClassArray<ON_PX_EVENT> x;
    // Use default tolerance
    ON_Intersect(p, c, x);

    ON_3dPoint start = c.PointAtStart();
    ON_3dPoint end = c.PointAtEnd();
    bu_log("(%f,%f,%f) and [(%f,%f,%f) to (%f,%f,%f)]:\n",
	   p[0], p[1], p[2], start[0], start[1], start[2], end[0], end[1], end[2]);
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

void
test_psi(ON_3dPoint &p, ON_Surface &s)
{
    ON_wString wstr;
    ON_TextLog textlog(wstr);
    ON_ClassArray<ON_PX_EVENT> x;
    // Use default tolerance
    ON_Intersect(p, s, x);

    // XXX: How to simply show a surface?
    bu_log("(%f,%f,%f) and a surface:\n", p[0], p[1], p[2]);
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

double
rand_f(double min, double max)
{
    double f = (double)rand() / RAND_MAX;
    return min + f * (max - min);
}

int
main(int, char**)
{
    srand(time(0));

    ON_3dPoint center(0.0, 0.0, 0.0);
    double radius = 10.0;
    ON_Sphere sphere(center, radius);
    ON_Brep *brep = ON_BrepSphere(sphere);

    ON_3dPoint p1(0.0, 0.0, 0.0);
    ON_3dPoint p2(0.0, 0.0, radius);

    // Point-point intersection
    bu_log("*** Point-point intersection ***\n");
    test_ppi(p1, p1);
    test_ppi(p1, p2);

    // Point-curve intersection
    bu_log("*** Point-curve intersection ***\n");
    // brep->m_C3[0] is an arc curve that starts from (0, 0, -R)
    // to (0, 0, R) through (R, 0, 0) which forms a semicircle.
    ON_Curve *curve = brep->m_C3[0];

    ON_3dPoint mid = curve->PointAt(curve->Domain().Mid());
    bu_log("debug: %f %f %f\n", mid[0], mid[1], mid[2]);

    bu_log("** Part 1 **\n");
    test_pci(p1, *curve);
    test_pci(p2, *curve);

    // Now we use some randomized points (should intersect)
    bu_log("** Part 2 **\n");
    for (int i = 0; i < 10; i++) {
	double x = rand_f(0.0, radius);
	double y = 0.0;
	double z = sqrt(radius*radius-x*x);
	if (rand() % 2) z = -z; // sometimes we have it negative
	ON_3dPoint test_pt(x, y, z);
	test_pci(test_pt, *curve);
    }

    // More randomize points (maybe no intersection)
    bu_log("** Part 3 **\n");
    for (int i = 0; i < 10; i++) {
	// We use test points randomly distributed inside a cube
	// from (-R, -R, -R) to (R, R, R)
	double x = rand_f(-radius, radius);
	double y = rand_f(-radius, radius);
	double z = rand_f(-radius, radius);
	ON_3dPoint test_pt(x, y, z);
	test_pci(test_pt, *curve);
    }

    // Point-surface intersection
    bu_log("*** Point-surface intersection ***\n");
    bu_log("** Part 1 **\n");
    ON_Surface *surf = brep->m_S[0];
    test_psi(p1, *surf);
    test_psi(p2, *surf);

    // Now we use some randomized points (should intersect)
    bu_log("** Part 2 **\n");
    for (int i = 0; i < 10; i++) {
	double x = rand_f(-radius, radius);
	double y_range = sqrt(radius*radius-x*x);
	double y = rand_f(-y_range, y_range);
	double z = sqrt(y_range*y_range-y*y);
	if (rand() % 2) z = -z; // sometimes we have it negative
	ON_3dPoint test_pt(x, y, z);
	test_psi(test_pt, *surf);
    }

    // More randomize points (maybe no intersection)
    bu_log("** Part 3 **\n");
    for (int i = 0; i < 10; i++) {
	// We use test points randomly distributed inside a cube
	// from (-R, -R, -R) to (R, R, R)
	double x = rand_f(-radius, radius);
	double y = rand_f(-radius, radius);
	double z = rand_f(-radius, radius);
	ON_3dPoint test_pt(x, y, z);
	test_psi(test_pt, *surf);
    }

    delete brep;
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
