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

class PPX_Input {
public:
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

bool
PPX_Output::Equals(std::string &log, const PPX_Output &other)
{
    bool ret = true;
    std::stringstream out;

    if (status != other.status) {
	out << "return status: " << status << " vs " << other.status << "\n";
	ret = false;
    }

    if (events.Count() != other.events.Count()) {
	out << "event count: " << events.Count() << " vs " <<
	    other.events.Count() << "\n";
	ret = false;
    } else {
	for (int i = 0; i < events.Count(); ++i) {
	    if (ON_PX_EVENT::Compare(&other.events[i], &events[i]) != 0) {
		out << "event arrays don't match\n";
		ret = false;
		break;
	    }
	}
    }

    out >> log;

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
    if (expected_out.Equals(err_msg, out)) {
	bu_exit(1, "intersection output doesn't match expected:\n%s",
		err_msg.c_str());
    }
}

int
main(void)
{
    PPX_Input input;
    input.ptA = ON_3dPoint(0, 0, 0);
    input.ptB = ON_3dPoint(0, 0, 0);
    input.default_tol = true;

    PPX_Output expected_output;
    expected_output.status = true;

    test_intersection(input, expected_output);

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
