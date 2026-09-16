/*                 T R I _ R A Y _ I S E C T . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2026 United States Government as represented by
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

#include "common.h"

#include <string>
#include <sstream>
#include <iomanip>
#include <limits>

#include "bu.h"
#include "bg.h"

static const fastf_t test_point_tolerance = VUNITIZE_TOL;


static double
str_to_dbl(std::string s)
{
    double d;
    size_t prec = std::numeric_limits<double>::max_digits10;
    std::stringstream ss(s);
    ss >> std::setprecision(prec) >> std::fixed >> d;
    return d;
}

static void
read_point(point_t *p, const char *arg)
{
    std::string pstr(arg);
    size_t sp = pstr.find_first_not_of(" \"\t\n\v\f\r");
    size_t ep = pstr.find_last_not_of(" \"\t\n\v\f\r");
    pstr = pstr.substr(sp, ep-sp+1);
    int i = 0;
    while (i < 2) {
	size_t cp = pstr.find_first_of(",");
	if (cp == std::string::npos) {
	    bu_exit(1, "ERROR: failure while parsing point string \"%s\"", arg);
	}
	std::string dstr = pstr.substr(0, cp);
	pstr.erase(0, cp+1);
	(*p)[i] = str_to_dbl(dstr);
	i++;
    }
    (*p)[i] = str_to_dbl(pstr);
}


static int
check_intersection(const char *test_name, int result,
		   const point_t intersection, const point_t expected)
{
    if (!result) {
	bu_log("ERROR: %s: expected an intersection\n", test_name);
	return 1;
    }

    if (!VNEAR_EQUAL(intersection, expected, test_point_tolerance)) {
	bu_log("ERROR: %s: expected (%g, %g, %g), got (%g, %g, %g)\n",
	       test_name, V3ARGS(expected), V3ARGS(intersection));
	return 1;
    }

    return 0;
}


static int
check_miss(const char *test_name, int result)
{
    if (result) {
	bu_log("ERROR: %s: expected no intersection\n", test_name);
	return 1;
    }

    return 0;
}


static int
run_self_tests()
{
    const point_t tri_vert0 = {0.0, 0.0, 0.0};
    const point_t tri_vert1 = {1.0, 0.0, 0.0};
    const point_t tri_vert2 = {0.0, 1.0, 0.0};
    const point_t forward_origin = {0.25, 0.25, 1.0};
    const vect_t forward_direction = {0.0, 0.0, -2.0};
    const point_t forward_intersection = {0.25, 0.25, 0.0};
    const vect_t backward_direction = {0.0, 0.0, 1.0};
    const point_t precision_origin = {-1999999.75, 0.25, -1.0};
    const vect_t precision_direction = {1.0, 0.0, 0.0000005};
    const point_t edge_origin = {0.5, 0.0, 1.0};
    const point_t edge_intersection = {0.5, 0.0, 0.0};
    const point_t vertex_origin = {0.0, 0.0, 1.0};
    const point_t vertex_intersection = {0.0, 0.0, 0.0};
    const point_t coplanar_origin = {0.25, 0.25, 0.0};
    const vect_t coplanar_direction = {1.0, 0.0, 0.0};
    const point_t degenerate_vert2 = {2.0, 0.0, 0.0};
    const point_t degenerate_origin = {0.5, 0.0, 1.0};
    const point_t outside_origin = {1.0, 1.0, 1.0};
    const vect_t down_direction = {0.0, 0.0, -1.0};
    const point_t sentinel = {7.0, -3.0, 11.0};
    point_t intersection = VINIT_ZERO;
    int failures = 0;

    failures += check_intersection(
	"forward ray", bg_isect_tri_ray(forward_origin, forward_direction,
	tri_vert0, tri_vert1, tri_vert2, &intersection),
	intersection, forward_intersection);

    failures += check_intersection(
	"forward line", bg_isect_tri_line(&intersection, forward_origin, forward_direction,
	tri_vert0, tri_vert1, tri_vert2),
	intersection, forward_intersection);

    failures += check_miss(
	"backward ray", bg_isect_tri_ray(forward_origin, backward_direction,
	tri_vert0, tri_vert1, tri_vert2, &intersection));

    failures += check_intersection(
	"backward line", bg_isect_tri_line(&intersection, forward_origin, backward_direction,
	tri_vert0, tri_vert1, tri_vert2),
	intersection, forward_intersection);

    failures += check_intersection(
	"near-parallel ray", bg_isect_tri_ray(precision_origin, precision_direction,
	tri_vert0, tri_vert1, tri_vert2, &intersection),
	intersection, forward_intersection);

    failures += check_intersection(
	"edge ray", bg_isect_tri_ray(edge_origin, down_direction,
	tri_vert0, tri_vert1, tri_vert2, &intersection),
	intersection, edge_intersection);

    failures += check_intersection(
	"vertex ray", bg_isect_tri_ray(vertex_origin, down_direction,
	tri_vert0, tri_vert1, tri_vert2, &intersection),
	intersection, vertex_intersection);

    failures += check_intersection(
	"origin ray", bg_isect_tri_ray(forward_intersection, backward_direction,
	tri_vert0, tri_vert1, tri_vert2, &intersection),
	intersection, forward_intersection);

    failures += check_miss(
	"coplanar ray", bg_isect_tri_ray(coplanar_origin, coplanar_direction,
	tri_vert0, tri_vert1, tri_vert2, &intersection));

    failures += check_miss(
	"degenerate triangle ray", bg_isect_tri_ray(degenerate_origin, down_direction,
	tri_vert0, tri_vert1, degenerate_vert2, &intersection));

    VMOVE(intersection, sentinel);
    if (bg_isect_tri_ray(outside_origin, down_direction,
		 tri_vert0, tri_vert1, tri_vert2, &intersection) ||
	!VNEAR_EQUAL(intersection, sentinel, test_point_tolerance)) {
	bu_log("ERROR: ray miss altered the output point\n");
	failures++;
    }

    return failures;
}


int
main(int argc, char **argv)
{
    int expected_result = 0;
    int actual_result = 0;
    point_t V0 = VINIT_ZERO;
    point_t V1 = VINIT_ZERO;
    point_t V2 = VINIT_ZERO;
    point_t O = VINIT_ZERO;
    point_t D = VINIT_ZERO;

    bu_setprogname(argv[0]);

    if (argc == 1)
	return run_self_tests();

    if (argc != 7)
	bu_exit(1, "ERROR: input format is V0x,V0y,V0z V1x,V1y,V1z V2x,V2y,V2z Ox,Oy,Oz Dx,Dy,Dz expected_result\n");

    read_point(&V0, argv[1]);
    read_point(&V1, argv[2]);
    read_point(&V2, argv[3]);
    read_point(&O,  argv[4]);
    read_point(&D,  argv[5]);

    sscanf(argv[6], "%d", &expected_result);

    actual_result = bg_isect_tri_ray(O, D, V0, V1, V2, NULL);

    bu_log("result: %d\n", actual_result);

    if (expected_result == actual_result) {
	return 0;
    }

    return -1;
}


/** @} */
// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
