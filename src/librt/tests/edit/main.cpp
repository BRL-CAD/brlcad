/*                         M A I N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file main.cpp
 *
 * Dispatch primitive edit tests by name.  CTest runs each case in its
 * own process so that fixture state and failures remain isolated.
 */

#include "common.h"

#include "bu/app.h"
#include "bu/defines.h"
#include "bu/log.h"
#include "bu/str.h"

struct edit_case {
    const char *name;
    int (*run)(void);
};

#include "cases.inc"

static void
usage(const char *progname)
{
    bu_log("Usage: %s <case|--list>\n", progname);
}

int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);

    if (argc != 2) {
	usage(argv[0]);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(argv[1], "--list")) {
	for (const struct edit_case *test = edit_cases; test->name; ++test)
	    bu_log("%s\n", test->name);
	return BRLCAD_OK;
    }

    for (const struct edit_case *test = edit_cases; test->name; ++test) {
	if (BU_STR_EQUAL(argv[1], test->name))
	    return test->run();
    }

    bu_log("Unknown edit test: %s\n", argv[1]);
    usage(argv[0]);
    return BRLCAD_ERROR;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
