/*                T E S T _ J O I N T _ L E G A C Y . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/file.h"
#include "ged.h"
#include "raytrace.h"
#include "wdb.h"

#define JOINT_FIXTURE_CAPACITY 4096

static const char *joint_fixture =
    "units cm;\n"
    "joint hinge {\n"
    "  arc = root.c/arm.c/child.s;\n"
    "  location = (0, 0, 0);\n"
    "  rotate { direction = (0, 0, 1); limits = -90, 90, 10; }\n"
    "  translate { direction = (1, 0, 0); limits = -2, 2, 1; }\n"
    "};\n";

static const char *partial_path_fixture =
    "units mm;\n"
    "joint partial {\n"
    "  path = root.c/arm.c-child.s;\n"
    "  location = (0, 0, 0);\n"
    "  rotate { direction = (0, 1, 0); limits = -45, 45, 0; }\n"
    "};\n";

static const char *malformed_fixture =
    "units mm;\n"
    "joint retained {\n"
    "  arc = root.c/arm.c/child.s;\n"
    "  location = (0, 0, 0);\n"
    "  translate { direction = (0, 0, 1); limits = -1, 1, 0; }\n"
    "};\n"
    "joint broken { arc = root.c/arm.c/child.s; };\n";

static int
write_fixture(char *path, size_t path_capacity, const char *contents)
{
    FILE *file = bu_temp_file(path, path_capacity);
    int write_failed;

    if (!file)
	return BRLCAD_ERROR;
    write_failed = fputs(contents, file) == EOF;
    if (fclose(file) != 0 || write_failed)
	return BRLCAD_ERROR;
    return BRLCAD_OK;
}

static int
read_fixture(const char *path, char *contents, size_t contents_capacity)
{
    FILE *file = fopen(path, "rb");
    size_t bytes_read;

    if (!file || contents_capacity < 2)
	return BRLCAD_ERROR;
    bytes_read = fread(contents, 1, contents_capacity - 1, file);
    contents[bytes_read] = '\0';
    if (ferror(file) || !feof(file)) {
	fclose(file);
	return BRLCAD_ERROR;
    }
    if (fclose(file) != 0)
	return BRLCAD_ERROR;
    return BRLCAD_OK;
}

static int
create_hierarchy(struct rt_wdb *wdbp)
{
    struct wmember members;
    point_t center = VINIT_ZERO;

    if (mk_sph(wdbp, "child.s", center, 1.0) < 0)
	return BRLCAD_ERROR;
    BU_LIST_INIT(&members.l);
    if (!mk_addmember("child.s", &members.l, NULL, WMOP_UNION) ||
	mk_comb(wdbp, "arm.c", &members.l, 0, NULL, NULL, NULL,
	    0, 0, 0, 0, 0, 0, 0) < 0)
	return BRLCAD_ERROR;
    BU_LIST_INIT(&members.l);
    if (!mk_addmember("arm.c", &members.l, NULL, WMOP_UNION) ||
	mk_comb(wdbp, "root.c", &members.l, 0, NULL, NULL, NULL,
	    0, 0, 0, 0, 0, 0, 0) < 0)
	return BRLCAD_ERROR;
    return BRLCAD_OK;
}

static int
run_joint(struct ged *gedp, int argc, const char *argv[])
{
    int result = ged_exec_joint(gedp, argc, argv);

    if (result != BRLCAD_OK)
	bu_log("joint command failed: %s\n", bu_vls_cstr(gedp->ged_result_str));
    return result;
}

static int
test_lifecycle(struct ged *gedp)
{
    char input_path[MAXPATHLEN] = {0};
    char output_path[MAXPATHLEN] = {0};
    char saved[JOINT_FIXTURE_CAPACITY] = {0};
    const char *load[] = {"joint", "legacy", "load", "-a", "-m", input_path};
    const char *list[] = {"joint", "legacy", "list"};
    const char *move[] = {"joint", "legacy", "move", "hinge", "30", "15"};
    const char *accept[] = {"joint", "legacy", "accept", "-m", "hinge"};
    const char *move_again[] = {"joint", "legacy", "move", "hinge", "-20", "-5"};
    const char *reject[] = {"joint", "legacy", "reject", "-m", "hinge"};
    const char *save[] = {"joint", "legacy", "save", output_path};
    const char *unload[] = {"joint", "legacy", "unload"};
    const char *step = "fixture creation";
    int result = BRLCAD_ERROR;

    if (write_fixture(input_path, sizeof(input_path), joint_fixture) != BRLCAD_OK ||
	write_fixture(output_path, sizeof(output_path), "") != BRLCAD_OK)
	goto cleanup;
    if (run_joint(gedp, 6, load) != BRLCAD_OK ||
	run_joint(gedp, 3, list) != BRLCAD_OK ||
	!strstr(bu_vls_cstr(gedp->ged_result_str), "hinge"))
	goto cleanup;
    step = "move and initial save";
    if (
	run_joint(gedp, 6, move) != BRLCAD_OK ||
	run_joint(gedp, 4, save) != BRLCAD_OK ||
	read_fixture(output_path, saved, sizeof(saved)) != BRLCAD_OK ||
	!strstr(saved, "3.000000000000000e+01") ||
	!strstr(saved, "1.500000000000000e+01"))
	goto cleanup;
    step = "accept and reject";
    if (
	run_joint(gedp, 5, accept) != BRLCAD_OK ||
	run_joint(gedp, 6, move_again) != BRLCAD_OK ||
	run_joint(gedp, 5, reject) != BRLCAD_OK ||
	run_joint(gedp, 4, save) != BRLCAD_OK ||
	read_fixture(output_path, saved, sizeof(saved)) != BRLCAD_OK ||
	!strstr(saved, "3.000000000000000e+01") ||
	!strstr(saved, "1.500000000000000e+01"))
	goto cleanup;
    step = "save and reload";
    if (
	run_joint(gedp, 3, unload) != BRLCAD_OK ||
	run_joint(gedp, 6, (const char *[]) {"joint", "legacy", "load", "-a", "-m", output_path}) != BRLCAD_OK ||
	run_joint(gedp, 3, list) != BRLCAD_OK ||
	!strstr(bu_vls_cstr(gedp->ged_result_str), "hinge"))
	goto cleanup;
    step = "unload";
    if (run_joint(gedp, 3, unload) != BRLCAD_OK ||
	run_joint(gedp, 3, list) != BRLCAD_OK ||
	bu_vls_strlen(gedp->ged_result_str) != 0)
	goto cleanup;
    result = BRLCAD_OK;

cleanup:
    if (result != BRLCAD_OK)
	bu_log("legacy lifecycle step '%s': result='%s' saved='%s'\n",
		step, bu_vls_cstr(gedp->ged_result_str), saved);
    bu_file_delete(output_path);
    bu_file_delete(input_path);
    return result;
}

static int
test_partial_path_grammar(struct ged *gedp)
{
    char input_path[MAXPATHLEN] = {0};
    const char *load[] = {"joint", "legacy", "load", "-a", "-m", input_path};
    const char *list[] = {"joint", "legacy", "list"};
    const char *unload[] = {"joint", "legacy", "unload"};
    int result = BRLCAD_ERROR;

    if (write_fixture(input_path, sizeof(input_path), partial_path_fixture) != BRLCAD_OK)
	return BRLCAD_ERROR;
    if (run_joint(gedp, 6, load) == BRLCAD_OK &&
	run_joint(gedp, 3, list) == BRLCAD_OK &&
	strstr(bu_vls_cstr(gedp->ged_result_str), "partial") &&
	run_joint(gedp, 3, unload) == BRLCAD_OK)
	result = BRLCAD_OK;
    if (result != BRLCAD_OK)
	bu_log("partial path result: %s\n", bu_vls_cstr(gedp->ged_result_str));
    bu_file_delete(input_path);
    return result;
}

static int
test_load_failure_behavior(struct ged *gedp)
{
    char input_path[MAXPATHLEN] = {0};
    char missing_path[MAXPATHLEN] = {0};
    const char *load[] = {"joint", "legacy", "load", "-a", "-m", input_path};
    const char *missing[] = {"joint", "legacy", "load", "-a", "-m", missing_path};
    const char *list[] = {"joint", "legacy", "list"};
    const char *unload[] = {"joint", "legacy", "unload"};
    int result = BRLCAD_ERROR;

    if (write_fixture(input_path, sizeof(input_path), malformed_fixture) != BRLCAD_OK ||
	write_fixture(missing_path, sizeof(missing_path), "") != BRLCAD_OK)
	goto cleanup;
    bu_file_delete(missing_path);

    /* These successful returns document current non-transactional behavior. */
    if (run_joint(gedp, 6, load) != BRLCAD_OK ||
	run_joint(gedp, 3, list) != BRLCAD_OK ||
	!strstr(bu_vls_cstr(gedp->ged_result_str), "retained") ||
	run_joint(gedp, 6, missing) != BRLCAD_OK ||
	run_joint(gedp, 3, unload) != BRLCAD_OK)
	goto cleanup;
    result = BRLCAD_OK;

cleanup:
    if (result != BRLCAD_OK)
	bu_log("load failure result: %s\n", bu_vls_cstr(gedp->ged_result_str));
    bu_file_delete(missing_path);
    bu_file_delete(input_path);
    return result;
}

int
main(int argc, char **argv)
{
    struct ged *gedp;
    struct rt_wdb *wdbp;
    int failures = 0;

    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;
    gedp = ged_open("inmem", "0", 0);
    if (!gedp)
	return 1;
    wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp || create_hierarchy(wdbp) != BRLCAD_OK) {
	ged_close(gedp);
	return 1;
    }
    if (test_lifecycle(gedp) != BRLCAD_OK) {
	bu_log("legacy joint lifecycle characterization failed\n");
	failures++;
    }
    if (test_partial_path_grammar(gedp) != BRLCAD_OK) {
	bu_log("legacy partial path characterization failed\n");
	failures++;
    }
    if (test_load_failure_behavior(gedp) != BRLCAD_OK) {
	bu_log("legacy load failure characterization failed\n");
	failures++;
    }
    ged_close(gedp);
    return failures;
}
