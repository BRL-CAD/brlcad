/*       T R I _ T R I _ I S E C T _ C O _ P L A N A R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2011-2022 United States Government as represented by
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

int
main(int argc, char *argv[])
{
    int expected_result = 0;
    int actual_result = 0;
    int area_flag = 0;
    point_t V0 = VINIT_ZERO;
    point_t V1 = VINIT_ZERO;
    point_t V2 = VINIT_ZERO;
    point_t U0 = VINIT_ZERO;
    point_t U1 = VINIT_ZERO;
    point_t U2 = VINIT_ZERO;

    bu_setprogname(argv[0]);

    if (argc != 9)
	bu_exit(1, "ERROR: input format is V0x,V0y,V0z V1x,V1y,V1z V2x,V2y,V2z U0x,U0y,U0z U1x,U1y,U1z U2x,U2y,U2z area_flag expected_result [%s]\n", argv[0]);

    read_point(&V0, argv[1]);
    read_point(&V1, argv[2]);
    read_point(&V2, argv[3]);
    read_point(&U0, argv[4]);
    read_point(&U1, argv[5]);
    read_point(&U2, argv[6]);

    sscanf(argv[7], "%d", &area_flag);
    sscanf(argv[8], "%d", &expected_result);

    actual_result = bg_tri_tri_isect_coplanar(V0, V1, V2, U0, U1, U2, area_flag);

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

