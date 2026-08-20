/*                       T E S T _ D B . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
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


static int
test_db_callback(int argc, const char **argv, void *u1, void *u2)
{
    struct ged *gedp = (struct ged *)u1;
    int *calls = (int *)u2;

    if (!gedp || !calls || argc != 2 || !BU_STR_EQUAL(argv[0], "db") || !BU_STR_EQUAL(argv[1], "application_only"))
	return BRLCAD_ERROR;

    (*calls)++;
    bu_vls_strcpy(gedp->ged_result_str, "application callback");
    return BRLCAD_OK;
}


int
main(int argc, char **argv)
{
    if (argc != 2) {
	bu_log("Usage: %s file.g\n", argv[0]);
	return BRLCAD_ERROR;
    }

    bu_setprogname(argv[0]);
    struct ged *gedp = ged_open("db", argv[1], 1);
    if (!gedp) {
	bu_log("ged_open failed on %s\n", argv[1]);
	return BRLCAD_ERROR;
    }

    if (!ged_cmd_exists("find") || !ged_cmd_exists("version") ||
	!ged_cmd_exists("dbfind") || !ged_cmd_exists("dbversion") ||
	!ged_cmd_same("find", "dbfind") || !ged_cmd_same("version", "dbversion")) {
	bu_log("missing or inconsistent top-level find/version compatibility declarations\n");
	ged_close(gedp);
	return BRLCAD_ERROR;
    }

    const char *version_av[] = {"dbversion"};
    int direct_ret = ged_exec(gedp, 1, version_av);
    char *direct_result = bu_strdup(bu_vls_cstr(gedp->ged_result_str));

    const char *db_version_av[] = {"db", "version"};
    int db_ret = ged_exec(gedp, 2, db_version_av);
    if (db_ret != direct_ret || !BU_STR_EQUAL(direct_result, bu_vls_cstr(gedp->ged_result_str))) {
	bu_log("db version did not match dbversion: direct (%d) [%s], db (%d) [%s]\n",
		direct_ret, direct_result, db_ret, bu_vls_cstr(gedp->ged_result_str));
	bu_free(direct_result, "direct version result");
	ged_close(gedp);
	return BRLCAD_ERROR;
    }
    bu_free(direct_result, "direct version result");

    const char *find_av[] = {"dbfind", "sph.s"};
    direct_ret = ged_exec(gedp, 2, find_av);
    direct_result = bu_strdup(bu_vls_cstr(gedp->ged_result_str));

    const char *db_find_av[] = {"db", "find", "sph.s"};
    db_ret = ged_exec(gedp, 3, db_find_av);
    if (db_ret != direct_ret || !BU_STR_EQUAL(direct_result, bu_vls_cstr(gedp->ged_result_str))) {
	bu_log("db find did not match dbfind: direct (%d) [%s], db (%d) [%s]\n",
		direct_ret, direct_result, db_ret, bu_vls_cstr(gedp->ged_result_str));
	bu_free(direct_result, "direct find result");
	ged_close(gedp);
	return BRLCAD_ERROR;
    }
    bu_free(direct_result, "direct find result");

    const char *find_help_av[] = {"db", "-h", "find"};
    int ret = ged_exec(gedp, 3, find_help_av);
    if (ret != GED_HELP || !strstr(bu_vls_cstr(gedp->ged_result_str), "db find [-a] <objects>")) {
	bu_log("db find targeted help failed: ret %d, result [%s]\n", ret, bu_vls_cstr(gedp->ged_result_str));
	ged_close(gedp);
	return BRLCAD_ERROR;
    }

    const char *version_help_av[] = {"db", "help", "version"};
    ret = ged_exec(gedp, 3, version_help_av);
    if (ret != GED_HELP || !strstr(bu_vls_cstr(gedp->ged_result_str), "db version")) {
	bu_log("db version targeted help failed: ret %d, result [%s]\n", ret, bu_vls_cstr(gedp->ged_result_str));
	ged_close(gedp);
	return BRLCAD_ERROR;
    }

    const char *unknown_av[] = {"db", "application_only"};
    ret = ged_exec(gedp, 2, unknown_av);
    if (!(ret & GED_UNKNOWN)) {
	bu_log("db did not report an unknown unregistered subcommand\n");
	ged_close(gedp);
	return BRLCAD_ERROR;
    }

    int callback_calls = 0;
    if (ged_clbk_set(gedp, "db", BU_CLBK_DURING, test_db_callback, &callback_calls) != BRLCAD_OK) {
	bu_log("failed to register db during callback\n");
	ged_close(gedp);
	return BRLCAD_ERROR;
    }

    ret = ged_exec(gedp, 2, unknown_av);
    if (ret != BRLCAD_OK || callback_calls != 1 || !BU_STR_EQUAL(bu_vls_cstr(gedp->ged_result_str), "application callback")) {
	bu_log("db application callback failed: ret %d, calls %d, result [%s]\n",
		ret, callback_calls, bu_vls_cstr(gedp->ged_result_str));
	ged_close(gedp);
	return BRLCAD_ERROR;
    }

    ged_close(gedp);
    return BRLCAD_OK;
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
