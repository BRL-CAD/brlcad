/*               T E S T _ O P T I O N _ P R E F I X . C
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

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "bu.h"
#include "ged.h"
#include "wdb.h"


int
main(int argc, char **argv)
{
    enum { MAX_ARGS = 12 };
    const struct {
	const char *args[MAX_ARGS];
	const char *expected;
    } cases[] = {
	{{"view", "-V", "size", "size", "25", NULL}, ""},
	{{"view", "--view=size", "size", NULL}, "25"},
	{{"view", "-h", "size", NULL}, "view [options] size [vals]"},
	{{"view", "-V", "size", "-h", "size", NULL}, "view [options] size [vals]"},
	{{"select", "-S", "list", "-h", "add", NULL}, "select [options] add path1"},
	{{"select", "--set=list", "-h", "add", NULL}, "select [options] add path1"},
	{{"select", "-h", "add", NULL}, "select [options] add path1"},
	{{"bot", "-v", "-h", "stat", NULL}, "bot stat <pattern>"},
	{{"bot", "legacy_operand", "-v", "-h", "stat", NULL}, "bot stat <pattern>"},
	{{"bot", "-v", "stat", "*", NULL}, "Object Path"},
	{{"bot", "legacy_operand", "-v", "stat", "*", NULL}, "Object Path"},
	{{"brep", "-h", "info", NULL}, "brep [options] <objname> info"},
	{{"brep", "-h", "info", "C2", NULL}, "info C2"},
	{{"brep", "object.s", "-v", "-h", "info", NULL}, "brep [options] <objname> info"},
	{{"brep", "object.s", "-v", "-h", "info", "C2", NULL}, "info C2"},
	{{"brep", "info", "-v", "-h", "info", NULL}, "brep [options] <objname> info"},
	{{"brep", "-v", "info", "-h", "info", NULL}, "brep [options] <objname> info"},
	{{"view", "faceplate", "grid", "draw", "1", NULL}, ""},
	{{"view", "faceplate", "grid", "-v", "draw", "1", NULL}, ""},
	{{"view", "faceplate", "grid", "-h", "draw", NULL}, "view faceplate grid draw"},
	{{"view", "faceplate", "model_axes", "-h", "size", NULL}, "axes size [#]"},
	{{"view", "faceplate", "view_axes", "-h", "size", NULL}, "axes size [#]"},
	{{"view", "faceplate", "irect", "-h", "draw", NULL}, "view faceplate irect draw"},
	{{"view", "obj", "axes_marker", "axes", "-s", "create", "1", "2", "3", NULL}, ""},
	{{"view", "obj", "axes_marker", "axes", "pos", NULL}, "1.000000 2.000000 3.000000"},
	{{"view", "obj", "line_marker", "line", "-s", "create", "1", "2", "3", NULL}, ""},
	{{"view", "obj", "line_marker", "line", "append", "4", "5", "6", NULL}, ""},
	{{"view", "obj", "label_marker", "label", "-s", "-h", "create", NULL}, "view obj <objname> label create"}
    };
    char database_path[MAXPATHLEN] = {0};
    struct rt_wdb *wdbp;
    struct ged *gedp;
    FILE *fp;
    int failures = 0;

    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;

    fp = bu_temp_file(database_path, MAXPATHLEN);
    if (!fp)
	bu_exit(1, "Could not create temporary database path\n");
    fclose(fp);
    wdbp = wdb_fopen(database_path);
    if (!wdbp)
	bu_exit(1, "Could not create option test database\n");
    db_close(wdbp->dbip);
    gedp = ged_open("db", database_path, 1);
    if (!gedp)
	bu_exit(1, "Could not open option test database\n");
    gedp->new_cmd_forms = 1;
    bu_vls_strcpy(&gedp->ged_gvp->gv_name, "size");

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
	const char *args[MAX_ARGS];
	int ac = 0;
	int ret;
	const char *result;
	memcpy(args, cases[i].args, sizeof(args));
	while (args[ac])
	    ac++;
	ret = ged_exec(gedp, ac, args);
	result = bu_vls_cstr(gedp->ged_result_str);
	if (ret != BRLCAD_OK ||
	    (cases[i].expected[0] ? !strstr(result, cases[i].expected) : result[0] != '\0')) {
	    bu_log("FAIL option prefix case %zu: ret %d, result [%s]\n", i, ret, result);
	    failures++;
	}
    }

    if (!NEAR_EQUAL(gedp->ged_gvp->gv_size, 25.0, SMALL_FASTF) ||
	!gedp->ged_gvp->gv_s->gv_grid.draw) {
	bu_log("FAIL: view setters did not update the requested state\n");
	failures++;
    }

    {
	const char *polygon_help[] = {"view", "obj", "polygon_marker", "polygon", "-s", "-h", "create"};
	if (ged_exec(gedp, 7, polygon_help) != GED_HELP ||
	    !strstr(bu_vls_cstr(gedp->ged_result_str), "view obj <objname> polygon [options] [args]")) {
	    bu_log("FAIL: polygon help convention changed\n");
	    failures++;
	}
    }

    {
	const char *missing_names[] = {"bot_smooth", "-t", "45"};
	const char *missing_source[] = {"bot_smooth", "-t", "45", "new.s"};
	const char *bad_angle[] = {"bot_smooth", "-t", "not-an-angle", "new.s", "old.s"};
	if (ged_exec(gedp, 3, missing_names) != BRLCAD_ERROR ||
	    ged_exec(gedp, 4, missing_source) != BRLCAD_ERROR ||
	    ged_exec(gedp, 5, bad_angle) != BRLCAD_ERROR ||
	    !strstr(bu_vls_cstr(gedp->ged_result_str), "Usage:")) {
	    bu_log("FAIL: malformed bot_smooth options were not rejected\n");
	    failures++;
	}
    }

    ged_close(gedp);
    bu_file_delete(database_path);
    return failures ? 1 : 0;
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
