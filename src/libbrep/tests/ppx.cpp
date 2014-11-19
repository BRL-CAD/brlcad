/*                         P P X . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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
/** @file ppx.cpp
 *
 * Point-point intersection tests.
 *
 */
#include "common.h"
#include "bu.h"
#include "brep.h"
#include <sstream>
#include <string>
#include "../brep_defines.h"

class PPX_Input {
public:
    PPX_Input()
	: tol(0.0)
    {
    }
    ON_3dPoint ptA;
    ON_3dPoint ptB;
    ON_ClassArray<ON_PX_EVENT> events;
    bool default_tol;
    double tol;
};

class PPX_Output {
public:
    bool status;
    ON_ClassArray<ON_PX_EVENT> events;

    bool Equals(std::string &log, const PPX_Output &other);
};

#define EVENT_EQUAL_TOL INTERSECTION_TOL / 4.0

static bool
point_events_equal(ON_PX_EVENT a, ON_PX_EVENT b)
{
    if (a.m_type != b.m_type) {
	return false;
    }
    if (a.m_A.DistanceTo(b.m_A) >= EVENT_EQUAL_TOL) {
	return false;
    }
    if (a.m_B.DistanceTo(b.m_B) >= EVENT_EQUAL_TOL) {
	return false;
    }
    if (a.m_b.DistanceTo(b.m_b) >= EVENT_EQUAL_TOL) {
	return false;
    }
    if (a.m_Mid.DistanceTo(b.m_Mid) >= EVENT_EQUAL_TOL) {
	return false;
    }
    if (!ON_NearZero(a.m_radius - b.m_radius, EVENT_EQUAL_TOL)) {
	return false;
    }
    return true;
}

bool
PPX_Output::Equals(std::string &log, const PPX_Output &other)
{
    bool ret = true;
    std::stringstream out;

    if (status != other.status) {
	out << "return status: " << std::boolalpha << status << " vs " <<
	    other.status << "\n";
	ret = false;
    }

    if (events.Count() != other.events.Count()) {
	out << "event count: " << events.Count() << " vs " <<
	    other.events.Count() << "\n";
	ret = false;
    } else {
	for (int i = 0; i < events.Count(); ++i) {
	    if (!point_events_equal(other.events[i], events[i])) {
		out << "event arrays don't match\n";
		ret = false;
		break;
	    }
	}
    }

    log = out.str();

    return ret;
}

void
test_intersection(PPX_Input in, PPX_Output expected_out)
{
    PPX_Output out;

    out.events = in.events;

    if (in.default_tol) {
	out.status = ON_Intersect(in.ptA, in.ptB, out.events);
    } else {
	out.status = ON_Intersect(in.ptA, in.ptB, out.events, in.tol);
    }

    std::string err_msg;
    if (!expected_out.Equals(err_msg, out)) {
	bu_exit(1, "Unexpected intersection result. Expected vs actual:\n%s",
		err_msg.c_str());
    }
}

static ON_PX_EVENT
ppx_point_event(ON_3dPoint ptA, ON_3dPoint ptB)
{
    ON_PX_EVENT event;
    event.m_type = ON_PX_EVENT::ppx_point;
    event.m_A = ptA;
    event.m_B = ptB;
    event.m_b = ON_2dPoint(0.0, 0.0);
    event.m_Mid = (ptA + ptB) * 0.5;
    event.m_radius = ptA.DistanceTo(ptB) * 0.5;
    return event;
}

static void
test_equal_points(ON_3dPoint pt)
{
    ON_ClassArray<ON_PX_EVENT> events;
    events.Append(ppx_point_event(pt, pt));

    PPX_Output expected_output;
    expected_output.status = true;
    expected_output.events = events;

    PPX_Input input;
    input.ptA = input.ptB = pt;
    input.default_tol = true;

    test_intersection(input, expected_output);
}

static void
do_equal_point_tests(void)
{
    std::vector<ON_3dPoint> points;
    points.push_back(ON_3dPoint(0.0, 0.0, 0.0));

    for (size_t i = 0; i < points.size(); ++i) {
	test_equal_points(points[i]);
    }
}

int
main(void)
{
    do_equal_point_tests();

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
