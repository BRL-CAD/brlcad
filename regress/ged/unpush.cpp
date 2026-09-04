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
#include "rt/calc.h"
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
    point_t eto_center_a = {0.0, -20.0, 5.0};
    vect_t eto_normal_a = {0.0, 0.0, 1.0};
    vect_t eto_major_a = {3.0, 0.0, 4.0};
    point_t eto_center_b = {50.0, 10.0, -5.0};
    vect_t eto_normal_b = {0.0, 1.0, 0.0};
    vect_t eto_major_b = {0.0, 8.0, 6.0};
    point_t tgc_base_a = VINIT_ZERO;
    vect_t tgc_height_a = {0.0, 0.0, 4.0};
    vect_t tgc_a_a = {2.0, 0.0, 0.0};
    vect_t tgc_b_a = {0.0, 3.0, 0.0};
    vect_t tgc_c_a = {1.0, 0.0, 0.0};
    vect_t tgc_d_a = {0.0, 1.5, 0.0};
    point_t tgc_base_b = {40.0, -5.0, 7.0};
    vect_t tgc_height_b = {2.0, 0.8, 6.0};
    vect_t tgc_a_b = {0.0, 6.0, 0.0};
    vect_t tgc_b_b = {-6.0, 0.0, 0.0};
    vect_t tgc_c_b = {0.0, 3.0, 0.0};
    vect_t tgc_d_b = {-3.0, 0.0, 0.0};
    point_t arb_a[8] = {
	{0.0, 0.0, 0.0}, {4.0, 0.0, 0.0},
	{4.0, 3.0, 0.0}, {0.0, 3.0, 0.0},
	{0.0, 0.0, 2.0}, {4.0, 0.0, 2.0},
	{4.0, 3.0, 2.0}, {0.0, 3.0, 2.0}
    };
    point_t arb_b[8];
    mat_t arb_transform;
    MAT_IDN(arb_transform);
    arb_transform[0] = 2.0;
    arb_transform[1] = 0.3;
    arb_transform[2] = 0.2;
    arb_transform[4] = 0.1;
    arb_transform[5] = 1.5;
    arb_transform[6] = 0.4;
    arb_transform[8] = 0.2;
    arb_transform[9] = 0.1;
    arb_transform[10] = 1.7;
    MAT_DELTAS(arb_transform, -20.0, 15.0, 8.0);
    for (size_t i = 0; i < 8; i++)
	MAT4X3PNT(arb_b[i], arb_transform, arb_a[i]);

    if (mk_sph(wdbp, "sphere_a.s", center_a, 2.0) ||
	mk_sph(wdbp, "sphere_b.s", center_b, 4.0) ||
	mk_sph(wdbp, "sphere_c.s", center_c, 8.0) ||
	mk_eto(wdbp, "eto_a.s", eto_center_a, eto_normal_a, eto_major_a, 8.0, 2.0) ||
	mk_eto(wdbp, "eto_b.s", eto_center_b, eto_normal_b, eto_major_b, 16.0, 4.0) ||
	mk_tgc(wdbp, "tgc_a.s", tgc_base_a, tgc_height_a,
	    tgc_a_a, tgc_b_a, tgc_c_a, tgc_d_a) ||
	mk_tgc(wdbp, "tgc_b.s", tgc_base_b, tgc_height_b,
	    tgc_a_b, tgc_b_b, tgc_c_b, tgc_d_b) ||
	mk_arb8(wdbp, "arb_a.s", &arb_a[0][X]) ||
	mk_arb8(wdbp, "arb_b.s", &arb_b[0][X])) {
	wdb_close(wdbp);
	bu_file_delete(path.c_str());
	return std::string();
    }

    struct wmember selected;
    BU_LIST_INIT(&selected.l);
    mk_addmember("sphere_a.s", &selected.l, nullptr, WMOP_UNION);
    mk_addmember("sphere_b.s", &selected.l, nullptr, WMOP_UNION);
    mk_addmember("sphere_c.s", &selected.l, nullptr, WMOP_UNION);
    mk_addmember("eto_a.s", &selected.l, nullptr, WMOP_UNION);
    mk_addmember("eto_b.s", &selected.l, nullptr, WMOP_UNION);
    mk_addmember("tgc_a.s", &selected.l, nullptr, WMOP_UNION);
    mk_addmember("tgc_b.s", &selected.l, nullptr, WMOP_UNION);
    mk_addmember("arb_a.s", &selected.l, nullptr, WMOP_UNION);
    mk_addmember("arb_b.s", &selected.l, nullptr, WMOP_UNION);
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

    const char *selected_object[] = {"selected.c"};
    point_t bounds_before_min;
    point_t bounds_before_max;
    bool bounds_before_ok = rt_obj_bounds(gedp->ged_result_str, gedp->dbip, 1,
	selected_object, 1, bounds_before_min, bounds_before_max) == BRLCAD_OK;

    const char *dry_run[] = {"unpush", "-D", "-L", "-v", "-v", "selected.c"};
    int result = ged_exec(gedp, 6, dry_run);
    const char *report = bu_vls_cstr(gedp->ged_result_str);
    bool report_ok = result == BRLCAD_OK &&
	contains(report, "verified groups: 4") &&
	contains(report, "grouped objects: 9") &&
	contains(report, "duplicate objects: 5") &&
	contains(report, "rewritable selected references: 9") &&
	contains(report, "externally exposed grouped objects: 1") &&
	contains(report, "selected.c/sphere_a.s replacement matrix") &&
	contains(report, "selected.c/eto_a.s replacement matrix");
    if (!report_ok)
	bu_log("unexpected unpush report:\n%s\n", report);

    const std::string after_dry_run = file_contents(database);
    bool dry_run_unchanged = before == after_dry_run;
    if (!dry_run_unchanged)
	bu_log("unpush dry-run changed the database\n");

    const char *write_request[] = {"unpush", "selected.c"};
    result = ged_exec(gedp, 2, write_request);
    const char *write_report = bu_vls_cstr(gedp->ged_result_str);
    bool write_ok = result == BRLCAD_OK &&
	contains(write_report, "canonical objects written: 2") &&
	contains(write_report, "parent combinations rewritten: 2") &&
	contains(write_report, "original objects removed: 5") &&
	contains(write_report, "original objects retained: 0") &&
	contains(write_report, "groups deferred: 0");
    if (!write_ok)
	bu_log("unexpected unpush write report:\n%s\n", write_report);

    const char *post_write_dry_run[] = {"unpush", "-D", "selected.c"};
    result = ged_exec(gedp, 3, post_write_dry_run);
    const char *post_write_report = bu_vls_cstr(gedp->ged_result_str);
    bool post_write_ok = result == BRLCAD_OK &&
	contains(post_write_report, "primitive objects: 6") &&
	contains(post_write_report, "canonicalized: 6") &&
	contains(post_write_report, "verified groups: 2");
    if (!post_write_ok)
	bu_log("unexpected post-write unpush report:\n%s\n", post_write_report);

    point_t bounds_after_min;
    point_t bounds_after_max;
    bool bounds_preserved = bounds_before_ok &&
	rt_obj_bounds(gedp->ged_result_str, gedp->dbip, 1, selected_object, 1,
	    bounds_after_min, bounds_after_max) == BRLCAD_OK &&
	VNEAR_EQUAL(bounds_before_min, bounds_after_min, SMALL_FASTF) &&
	VNEAR_EQUAL(bounds_before_max, bounds_after_max, SMALL_FASTF);
    if (!bounds_preserved)
	bu_log("unpush did not preserve selected.c bounds\n");

    const char *old_names[] = {
	"sphere_a.s", "sphere_b.s", "sphere_c.s", "eto_a.s", "eto_b.s"
    };
    bool directory_ok = true;
    for (const char *name : old_names) {
	if (db_lookup(gedp->dbip, name, LOOKUP_QUIET) != RT_DIR_NULL)
	    directory_ok = false;
    }
    for (size_t i = 1; i <= 2; i++) {
	std::string name = "unpush_" + std::to_string(i);
	if (db_lookup(gedp->dbip, name.c_str(), LOOKUP_QUIET) == RT_DIR_NULL)
	    directory_ok = false;
    }
    const char *deferred_names[] = {"tgc_a.s", "tgc_b.s", "arb_a.s", "arb_b.s"};
    for (const char *name : deferred_names) {
	if (db_lookup(gedp->dbip, name, LOOKUP_QUIET) == RT_DIR_NULL)
	    directory_ok = false;
    }
    if (!directory_ok)
	bu_log("unpush did not leave the expected canonical directory entries\n");

    ged_close(gedp);
    const std::string after = file_contents(database);
    bool write_changed_database = before != after;
    if (!write_changed_database)
	bu_log("unpush write did not change the database\n");
    bu_file_delete(database.c_str());

    std::string local_database = make_database();
    struct ged *local_gedp = local_database.empty() ? nullptr :
	ged_open("db", local_database.c_str(), 1);
    bool local_ok = local_gedp != nullptr;
    if (local_gedp) {
	const char *local_write[] = {"unpush", "-L", "selected.c"};
	result = ged_exec(local_gedp, 3, local_write);
	const char *local_report = bu_vls_cstr(local_gedp->ged_result_str);
	local_ok = result == BRLCAD_OK &&
	    contains(local_report, "canonical objects written: 1") &&
	    contains(local_report, "parent combinations rewritten: 1") &&
	    contains(local_report, "original objects removed: 2") &&
	    contains(local_report, "groups deferred: 1") &&
	    db_lookup(local_gedp->dbip, "sphere_a.s", LOOKUP_QUIET) != RT_DIR_NULL &&
	    db_lookup(local_gedp->dbip, "sphere_b.s", LOOKUP_QUIET) != RT_DIR_NULL &&
	    db_lookup(local_gedp->dbip, "sphere_c.s", LOOKUP_QUIET) != RT_DIR_NULL;
	if (!local_ok)
	    bu_log("unexpected local unpush result:\n%s\n", local_report);
	ged_close(local_gedp);
    }
    if (!local_database.empty())
	bu_file_delete(local_database.c_str());

    return report_ok && dry_run_unchanged && write_ok && post_write_ok &&
	directory_ok && bounds_preserved && write_changed_database && local_ok ? 0 : 1;
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
