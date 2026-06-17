/*              T E S T _ C M D _ S C H E M A S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2024-2026 United States Government as represented by
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
/** @file libged/tests/test_cmd_schemas.cpp
 *
 * Test command option schema definitions for libged commands.
 *
 */

#include "common.h"

#include <stdio.h>

extern "C" {
#include "bu.h"
#include "bu/opt.h"
#include "ged.h"
}

static int
has_candidate(const char * const *completions, int cnt, const char *needle)
{
    if (!completions || !needle)
return 0;

    for (int i = 0; i < cnt; i++) {
if (completions[i] && BU_STR_EQUAL(completions[i], needle))
    return 1;
    }

    return 0;
}

static int
check(int cond, const char *msg)
{
    if (!cond) {
bu_log("FAIL: %s\n", msg);
return 1;
    }

    return 0;
}

int
main(int ac, char *av[])
{
    int ret = 0;
    struct ged *gedp = NULL;
    char *json = NULL;
    struct bu_opt_validate_result vr = BU_OPT_VALIDATE_RESULT_NULL;
    const char **completions = NULL;
    struct bu_vls prefix = BU_VLS_INIT_ZERO;
    int cnt = 0;

    bu_setprogname(av[0]);

    if (ac != 2) {
printf("Usage: %s file.g\n", av[0]);
return 1;
    }
    if (!bu_file_exists(av[1], NULL)) {
printf("ERROR: [%s] does not exist, expecting .g file\n", av[1]);
return 2;
    }

    gedp = ged_open("db", av[1], 1);
    if (!gedp) {
bu_log("ged_open failed on %s\n", av[1]);
return 2;
    }

    ret += check(ged_cmd_schema_exists("draw"), "draw schema advertised");
    ret += check(ged_cmd_schema_exists("open"), "open schema advertised");
    ret += check(ged_cmd_schema_exists("opendb"), "opendb schema advertised");
    ret += check(ged_cmd_schema_exists("erase"), "erase schema advertised");
    ret += check(ged_cmd_schema_exists("who"), "who schema advertised");
    ret += check(ged_cmd_schema_exists("ls"), "ls schema advertised");
    ret += check(ged_cmd_schema_exists("select"), "select schema advertised");
    ret += check(ged_cmd_schema_exists("view"), "view schema advertised");
    ret += check(ged_cmd_schema_exists("dm"), "dm schema advertised");
    ret += check(!ged_cmd_schema_exists("tops"), "tops remains schema-free");

    json = ged_cmd_schema_json("opendb");
    ret += check(json != NULL, "opendb JSON schema returned");
    ret += check(json && strstr(json, "\"name\":\"opendb\""), "opendb JSON names the requested alias");
    ret += check(json && strstr(json, "\"filename\""), "opendb JSON includes filename operand");
    if (json)
bu_free(json, "opendb schema json");

    json = ged_cmd_schema_json("view");
    ret += check(json != NULL, "view JSON schema returned");
    ret += check(json && strstr(json, "\"faceplate\""), "view JSON includes subcommand metadata");
    if (json)
bu_free(json, "view schema json");

    ret += check(ged_cmd_validate(gedp, "draw co", 7, &vr) == 0, "draw validation call succeeds");
    ret += check(vr.state == BU_OPT_VALIDATE_VALID, "draw partial operand is advisory-valid");
    ret += check(vr.completion_type == BU_OPT_VAL_DB_PATH || vr.completion_type == BU_OPT_VAL_DB_OBJECT,
	"draw requests geometry completion");
    ret += check(vr.completion_count > 0 && has_candidate(vr.completion_candidates, (int)vr.completion_count, "coil"),
"draw validation exposes geometry completions");
    bu_opt_validate_result_clear(&vr);

    ret += check(ged_cmd_validate(gedp, "view ", 5, &vr) == 0, "view validation call succeeds");
    ret += check(vr.state == BU_OPT_VALIDATE_VALID || vr.state == BU_OPT_VALIDATE_INCOMPLETE,
	"view exposes advisory syntax state at subcommand position");
    ret += check(vr.completion_count > 0 && has_candidate(vr.completion_candidates, (int)vr.completion_count, "faceplate"),
"view validation exposes subcommand completions");
    bu_opt_validate_result_clear(&vr);

    ret += check(ged_cmd_validate(gedp, "tops co", 7, &vr) == 0, "tops validation call succeeds");
    ret += check(vr.state == BU_OPT_VALIDATE_UNKNOWN, "commands without schemas report unknown syntax");
    ret += check(vr.completion_count > 0 && has_candidate(vr.completion_candidates, (int)vr.completion_count, "coil"),
"schema-free command validation falls back to geometry completions");
    bu_opt_validate_result_clear(&vr);

    cnt = ged_cmd_complete(&completions, &prefix, gedp, "tops co", 7);
    ret += check(cnt > 0 && has_candidate(completions, cnt, "coil"), "schema-free completion API falls back to geometry completion");
    ret += check(BU_STR_EQUAL(bu_vls_cstr(&prefix), "co"), "geometry completion prefix preserved");
    if (completions)
bu_argv_free(cnt, (char **)completions);
    bu_vls_trunc(&prefix, 0);
    completions = NULL;

    cnt = ged_cmd_complete(&completions, &prefix, gedp, "view ", 5);
    ret += check(cnt > 0 && has_candidate(completions, cnt, "faceplate"), "schema completion API returns subcommand candidates");
    ret += check(BU_STR_EQUAL(bu_vls_cstr(&prefix), ""), "subcommand completion prefix is empty at token start");
    if (completions)
bu_argv_free(cnt, (char **)completions);

    bu_vls_free(&prefix);
    ged_close(gedp);

    return ret ? 1 : 0;
}
