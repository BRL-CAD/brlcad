/*             R E G R E S S _ L I N T _ B R E P . C P P
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
/** @file regress_lint_brep.cpp
 *
 * Regression tests for the bounded B-Rep geometry lint checks.
 */

#include "common.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/vls.h"
#include "ged.h"
#include "raytrace.h"
#include "wdb.h"


static std::string
read_file(const char *path)
{
    std::ifstream f(path);
    if (!f.is_open())
	return std::string();
    return std::string((std::istreambuf_iterator<char>(f)),
	std::istreambuf_iterator<char>());
}


static std::string
temporary_path(const char *suffix)
{
    char path[MAXPATHLEN] = {0};
    FILE *fp = bu_temp_file(path, MAXPATHLEN);
    if (fp)
	fclose(fp);
    struct bu_vls result = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&result, "%s_%s", path, suffix);
    std::string ret(bu_vls_cstr(&result));
    bu_vls_free(&result);
    return ret;
}


static ON_Brep *
box_brep()
{
    ON_3dPoint corners[8] = {
	ON_3dPoint(0.0, 0.0, 0.0), ON_3dPoint(10.0, 0.0, 0.0),
	ON_3dPoint(10.0, 10.0, 0.0), ON_3dPoint(0.0, 10.0, 0.0),
	ON_3dPoint(0.0, 0.0, 10.0), ON_3dPoint(10.0, 0.0, 10.0),
	ON_3dPoint(10.0, 10.0, 10.0), ON_3dPoint(0.0, 10.0, 10.0)
    };
    return ON_BrepBox(corners);
}


static bool
make_database(const char *path)
{
    struct rt_wdb *wdbp = wdb_fopen(path);
    if (!wdbp)
	return false;

    ON_Brep *good = box_brep();
    ON_Brep *bad = box_brep();
    ON_Brep *shading_failure = box_brep();
    if (!good || !bad || !shading_failure) {
	delete good;
	delete bad;
	delete shading_failure;
	db_close(wdbp->dbip);
	return false;
    }

    ON_BrepLoop *failed_loop = shading_failure->m_F[0].OuterLoop();
    if (!failed_loop) {
	delete good;
	delete bad;
	delete shading_failure;
	db_close(wdbp->dbip);
	return false;
    }
    shading_failure->FlipLoop(*failed_loop);
    failed_loop->m_type = ON_BrepLoop::inner;

    ON_BrepEdge &edge = bad->m_E[0];
    ON_Curve *curve = (edge.m_c3i >= 0 && edge.m_c3i < bad->m_C3.Count()) ?
	bad->m_C3[edge.m_c3i] : NULL;
    const ON_Xform shift = ON_Xform::TranslationTransformation(
	ON_3dVector(0.0, 0.0, 1.0));
    if (!curve || !curve->Transform(shift)) {
	delete good;
	delete bad;
	db_close(wdbp->dbip);
	return false;
    }
    edge.m_tolerance = 5.0;
    for (int vi = 0; vi < 2; ++vi) {
	ON_BrepVertex *vertex = edge.Vertex(vi);
	if (vertex)
	    vertex->m_tolerance = 5.0;
    }
    for (int eti = 0; eti < edge.m_ti.Count(); ++eti) {
	const int ti = edge.m_ti[eti];
	if (ti >= 0 && ti < bad->m_T.Count()) {
	    bad->m_T[ti].m_tolerance[0] = 5.0;
	    bad->m_T[ti].m_tolerance[1] = 5.0;
	}
    }

    ON_TextLog text_log(stderr);
    const bool valid = good->IsValid(&text_log) && bad->IsValid(&text_log);
    const bool written = valid && mk_brep(wdbp, "good.s", good) == 0 &&
	mk_brep(wdbp, "masked_mismatch.s", bad) == 0 &&
	mk_brep(wdbp, "shading_failure.s", shading_failure) == 0;
    delete good;
    delete bad;
    delete shading_failure;
    db_close(wdbp->dbip);
    return written;
}


static int
run_lint(const char *gfile, const char *object, const char *json_file,
	const char *techniques)
{
    struct ged *gedp = ged_open("db", gfile, 1);
    if (!gedp)
	return BRLCAD_ERROR;
    const char *argv[] = {
	"lint", "-I", techniques,
	"-j", json_file, object
    };
    const int ret = ged_exec(gedp, 6, argv);
    ged_close(gedp);
    return ret;
}


static bool
contains_problem(const std::string &json, const char *problem)
{
    struct bu_vls pattern = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&pattern, "\"problem_type\": \"%s\"", problem);
    const bool found = json.find(bu_vls_cstr(&pattern)) != std::string::npos;
    bu_vls_free(&pattern);
    return found;
}


int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc == 4) {
	const char *checks = "brep:edge_surface_mismatch brep:large_tolerance "
	    "brep:singular_boundary brep:trim_loop_crossing";
	return run_lint(argv[1], argv[2], argv[3], checks);
    }
    if (argc != 1)
	return 1;

    const std::string gfile = temporary_path("lint_brep.g");
    const std::string good_json = temporary_path("lint_brep_good.json");
    const std::string bad_json = temporary_path("lint_brep_bad.json");
    const std::string shading_json = temporary_path("lint_brep_shading.json");
    bool pass = make_database(gfile.c_str());

    const char *checks = "brep:edge_surface_mismatch brep:large_tolerance "
	"brep:singular_boundary brep:trim_loop_crossing";
    if (pass)
	pass = run_lint(gfile.c_str(), "good.s", good_json.c_str(), checks) ==
	    BRLCAD_OK;
    const std::string good_result = read_file(good_json.c_str());
    if (pass)
	pass = !contains_problem(good_result, "edge_surface_mismatch") &&
	    !contains_problem(good_result, "large_tolerance") &&
	    !contains_problem(good_result, "singular_boundary") &&
	    !contains_problem(good_result, "trim_loop_crossing");

    if (pass)
	pass = run_lint(gfile.c_str(), "masked_mismatch.s", bad_json.c_str(),
	    checks) == BRLCAD_OK;
    const std::string bad_result = read_file(bad_json.c_str());
    if (pass)
	pass = contains_problem(bad_result, "edge_surface_mismatch") &&
	    contains_problem(bad_result, "large_tolerance") &&
	    bad_result.find("\"declared_tolerance_masks\": true") !=
	    std::string::npos;

    if (pass)
	pass = run_lint(gfile.c_str(), "good.s", shading_json.c_str(),
	    "brep:fast_shading") == BRLCAD_OK;
    std::string shading_result = read_file(shading_json.c_str());
    if (pass)
	pass = !contains_problem(shading_result, "fast_shading_failure") &&
	    !contains_problem(shading_result, "fast_shading_incomplete");

    if (pass)
	pass = run_lint(gfile.c_str(), "shading_failure.s",
	    shading_json.c_str(), "brep:opennurbs") == BRLCAD_OK;
    shading_result = read_file(shading_json.c_str());
    if (pass)
	pass = contains_problem(shading_result, "opennurbs_invalid") &&
	    !contains_problem(shading_result, "fast_shading_failure") &&
	    !contains_problem(shading_result, "fast_shading_incomplete");

    if (pass)
	pass = run_lint(gfile.c_str(), "shading_failure.s",
	    shading_json.c_str(), "brep:fast_shading") == BRLCAD_OK;
    shading_result = read_file(shading_json.c_str());
    if (pass)
	pass = contains_problem(shading_result, "fast_shading_failure") &&
	    shading_result.find("\"face\": 0") != std::string::npos &&
	    shading_result.find("\"stage_name\": \"pslg_validation\"") !=
	    std::string::npos;

    bu_file_delete(gfile.c_str());
    bu_file_delete(good_json.c_str());
    bu_file_delete(bad_json.c_str());
    bu_file_delete(shading_json.c_str());
    if (!pass)
	bu_log("B-Rep lint regression failed\n");
    return pass ? 0 : 1;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
