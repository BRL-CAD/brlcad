/*                  S U B P R O C E S S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2022 United States Government as represented by
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
/** @file bu_process_sub.c
 *
 * Program run by the process unit tests.
 *
 */

#include "common.h"

#include <chrono>
#include <thread>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "bu.h"

int
main(int argc, const char *argv[])
{
    if (argc > 2) {
	fprintf(stderr, "Usage: %s\n", argv[0]);
	return 1;
    }

    bu_setprogname(argv[0]);

    if (BU_STR_EQUAL(argv[1], "basic")) {
	// https://stackoverflow.com/a/11276503
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	fprintf(stdout, "Hello world!\n");
	return 0;
    }

    if (BU_STR_EQUAL(argv[1], "abort")) {
	// https://stackoverflow.com/a/11276503
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	fprintf(stderr, "Not aborted!\n");
	return 0;
    }


    return 1;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

