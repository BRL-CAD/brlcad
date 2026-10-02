/*                   P I P E _ R O U T E . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file libged/tests/pipe/pipe_route.c
 *
 * Integration coverage for the PIPE route command.
 */

#include "common.h"

#include <stdio.h>

#include "bu/app.h"
#include "ged.h"
#include "raytrace.h"
#include "wdb.h"


static int
expect_object(struct ged *gedp, const char *name)
{
    if (db_lookup(gedp->dbip, name, LOOKUP_QUIET) != RT_DIR_NULL)
	return 0;

    bu_log("Expected object %s was not created\n", name);
    return 1;
}


static int
run_command(struct ged *gedp, int argc, const char *argv[])
{
    int status = ged_exec(gedp, argc, argv);

    if (status == BRLCAD_OK)
	return 0;

    bu_log("Command failed: %s\n%s\n", argv[0], bu_vls_cstr(gedp->ged_result_str));
    bu_vls_trunc(gedp->ged_result_str, 0);
    return 1;
}


static struct ged *
open_test_db(const char *path)
{
    struct rt_wdb *wdbp;
    point_t minimum = {80.0, -30.0, -50.0};
    point_t maximum = {120.0, 30.0, 50.0};

    wdbp = wdb_fopen(path);
    if (!wdbp)
	return NULL;
    if (mk_rpp(wdbp, "block.s", minimum, maximum)) {
	db_close(wdbp->dbip);
	return NULL;
    }
    db_close(wdbp->dbip);
    return ged_open("db", path, 1);
}


int
main(int argc, char *argv[])
{
    struct ged *gedp;
    int failures = 0;

    bu_setprogname(argv[0]);
    if (argc != 2) {
	fprintf(stderr, "Usage: %s test.g\n", argv[0]);
	return 1;
    }

    gedp = open_test_db(argv[1]);
    if (!gedp) {
	fprintf(stderr, "Unable to create test database %s\n", argv[1]);
	return 1;
    }

    {
	const char *command[] = {
	    "pipe", "route", "--diameter", "10", "--bend", "20",
	    "straight.s", "0 0 0", "200 0 0"
	};
	failures += run_command(gedp, (int)(sizeof(command) / sizeof(command[0])), command);
	failures += expect_object(gedp, "straight.s");
    }

    {
	const char *command[] = {
	    "pipe", "route", "--diameter", "10", "--bend", "20", "--grid", "80",
	    "--avoid", "block.s", "detour.s", "0 0 0", "200 0 0"
	};
	failures += run_command(gedp, (int)(sizeof(command) / sizeof(command[0])), command);
	failures += expect_object(gedp, "detour.s");
    }

    {
	const char *command[] = {
	    "pipe", "route", "--profile", "harness", "--diameter", "5", "--wires", "3",
	    "--spacing", "6", "--bend", "20", "harness.g", "0 0 0", "160 0 0", "160 80 0"
	};
	failures += run_command(gedp, (int)(sizeof(command) / sizeof(command[0])), command);
	failures += expect_object(gedp, "harness.g");
	failures += expect_object(gedp, "harness.g.wire01.s");
	failures += expect_object(gedp, "harness.g.wire02.s");
	failures += expect_object(gedp, "harness.g.wire03.s");
    }

    {
	const char *command[] = {
	    "pipe", "route", "--diameter", "10", "--bend", "20", "coolant.g",
	    "0 0 0", "200 0 0", "--tee", "100 0 0", "100 100 0"
	};
	failures += run_command(gedp, (int)(sizeof(command) / sizeof(command[0])), command);
	failures += expect_object(gedp, "coolant.g");
	failures += expect_object(gedp, "coolant.g.main.s");
	failures += expect_object(gedp, "coolant.g.tee01.s");
    }

    {
	const char *command[] = {
	    "pipe", "route", "--diameter", "10", "--bend", "20", "--avoid", "block.s",
	    "--max-nodes", "1", "--fallback", "subtract", "service.s", "0 0 0", "200 0 0"
	};
	failures += run_command(gedp, (int)(sizeof(command) / sizeof(command[0])), command);
	failures += expect_object(gedp, "service.s");
	failures += expect_object(gedp, "service.s.clearance.s");
	failures += expect_object(gedp, "service.s.cut01.c");
    }

    ged_close(gedp);
    return failures ? 1 : 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
