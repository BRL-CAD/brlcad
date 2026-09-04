/*                R E G R E S S _ U N P U S H . C P P
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

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/vls.h"
#include "ged.h"
#include "wdb.h"


static std::string
file_contents(const std::string &path)
{
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream),
	std::istreambuf_iterator<char>());
}


static std::string
make_database(void)
{
    char temporary[MAXPATHLEN] = {0};
    FILE *file = bu_temp_file(temporary, MAXPATHLEN);
    if (file)
	std::fclose(file);

    struct bu_vls database_path = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&database_path, "%s_unpush.g", temporary);
    std::string path = bu_vls_cstr(&database_path);
    bu_vls_free(&database_path);

    struct rt_wdb *wdbp = wdb_fopen(path.c_str());
    if (!wdbp)
	return std::string();
    mk_id(wdbp, "unpush regression");

    point_t center_a = {10.0, 0.0, 0.0};
    point_t center_b = {0.0, 20.0, 0.0};
    point_t center_c = {0.0, 0.0, 30.0};
    if (mk_sph(wdbp, "sphere_a.s", center_a, 2.0) ||
	mk_sph(wdbp, "sphere_b.s", center_b, 4.0) ||
	mk_sph(wdbp, "sphere_c.s", center_c, 8.0)) {
	wdb_close(wdbp);
	bu_file_delete(path.c_str());
	return std::string();
    }

    struct wmember selected;
    BU_LIST_INIT(&selected.l);
    mk_addmember("sphere_a.s", &selected.l, nullptr, WMOP_UNION);
    mk_addmember("sphere_b.s", &selected.l, nullptr, WMOP_UNION);
    mk_addmember("sphere_c.s", &selected.l, nullptr, WMOP_UNION);
    if (mk_lcomb(wdbp, "selected.c", &selected, 0, nullptr, nullptr, nullptr, 0)) {
	wdb_close(wdbp);
	bu_file_delete(path.c_str());
	return std::string();
    }

    struct wmember external;
    BU_LIST_INIT(&external.l);
    mk_addmember("sphere_b.s", &external.l, nullptr, WMOP_UNION);
    if (mk_lcomb(wdbp, "external.c", &external, 0, nullptr, nullptr, nullptr, 0)) {
	wdb_close(wdbp);
	bu_file_delete(path.c_str());
	return std::string();
    }

    wdb_close(wdbp);
    return path;
}


static bool
contains(const char *text, const char *expected)
{
    return text && std::string(text).find(expected) != std::string::npos;
}


int
main(int UNUSED(argc), char *argv[])
{
    bu_setprogname(argv[0]);
    std::string database = make_database();
    if (database.empty()) {
	bu_log("unable to create unpush regression database\n");
	return 1;
    }

    const std::string before = file_contents(database);
    struct ged *gedp = ged_open("db", database.c_str(), 1);
    if (!gedp) {
	bu_file_delete(database.c_str());
	return 1;
    }

    const char *dry_run[] = {"unpush", "-D", "-L", "-v", "-v", "selected.c"};
    int result = ged_exec(gedp, 6, dry_run);
    const char *report = bu_vls_cstr(gedp->ged_result_str);
    bool report_ok = result == BRLCAD_OK &&
	contains(report, "verified groups: 1") &&
	contains(report, "grouped objects: 3") &&
	contains(report, "duplicate objects: 2") &&
	contains(report, "rewritable selected references: 3") &&
	contains(report, "externally exposed grouped objects: 1") &&
	contains(report, "selected.c/sphere_a.s replacement matrix");
    if (!report_ok)
	bu_log("unexpected unpush report:\n%s\n", report);

    const char *write_request[] = {"unpush", "selected.c"};
    result = ged_exec(gedp, 2, write_request);
    bool write_rejected = result == BRLCAD_ERROR &&
	contains(bu_vls_cstr(gedp->ged_result_str), "database rewriting is not enabled");
    if (!write_rejected)
	bu_log("unpush accepted a write request before rewrite support was enabled\n");

    ged_close(gedp);
    const std::string after = file_contents(database);
    bool unchanged = before == after;
    if (!unchanged)
	bu_log("unpush dry-run changed the database\n");

    bu_file_delete(database.c_str());
    return report_ok && write_rejected && unchanged ? 0 : 1;
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
